#include "component/masonry_component.hpp"

#include "component/responsive_layout.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

namespace ryn::detail {
namespace {

thread_local MasonryComponentHost* active_masonry{};
constexpr auto masonry_dirty = runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                               runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest;

struct ItemState final {
    Signal<MasonryValue> value{MasonryValue{}};
    Signal<std::size_t> index{0};
    Signal<int> column{0};
};

struct Item final {
    MasonryRecord record;
    runtime::ComponentId component;
    int column{};
    runtime::Rect frame;
};

struct MasonryState final {
    runtime::NodeId node;
    Responsive<int> columns{3};
    MasonryGutter gutter;
    Breakpoints breakpoints;
    float window_width{};
    bool rtl{};
    bool fresh{};
    bool reconciling{};
    bool deferred_layout{};
    std::uint64_t pending_revision{};
    std::uint64_t committed_revision{};
    std::vector<MasonryRecord> pending;
    std::vector<Item> items;
    MasonryRenderer render;
    std::function<void(std::vector<MasonryPlacement>)> on_layout;
    std::optional<std::vector<MasonryPlacement>> delivered;
    std::vector<MasonryPlacement> assignments;
    std::uint64_t measure_generation{};
};

void validate_columns(const Responsive<int>& value) {
    const auto validate = [](int count) {
        if (count <= 0) {
            throw std::invalid_argument("Masonry columns must be positive");
        }
    };
    validate(value.fallback());
    for (const auto& count : value.overrides()) {
        if (count) {
            validate(*count);
        }
    }
}

void validate_gutter(const MasonryGutter& value) {
    for (const auto* axis : {&value.horizontal, &value.vertical}) {
        const auto validate = [](LogicalLength length) {
            if (length.is_auto() || !std::isfinite(length.value()) || length.value() < 0) {
                throw std::invalid_argument("Masonry gutter must be a finite non-negative logical length");
            }
        };
        validate(axis->fallback());
        for (const auto& length : axis->overrides()) {
            if (length) {
                validate(*length);
            }
        }
    }
}

void validate_records(const std::vector<MasonryRecord>& records) {
    std::vector<MasonryKey> keys;
    for (const auto& record : records) {
        if (!record.key.valid() || std::ranges::find(keys, record.key) != keys.end()) {
            throw std::invalid_argument(
                "Masonry keys must be non-empty and unique, with distinct string/integer types");
        }
        if (record.column && *record.column < 0) {
            throw std::invalid_argument("Masonry item column must be non-negative");
        }
        if (record.height &&
            (record.height->is_auto() || !std::isfinite(record.height->value()) || record.height->value() < 0)) {
            throw std::invalid_argument("Masonry item height must be a finite non-negative logical length");
        }
        keys.push_back(record.key);
    }
}

void invalidate(WindowComponentServices& services, runtime::ComponentId id) {
    if (services.components().contains(id)) {
        services.dirty().invalidate(services.components().root(id), masonry_dirty);
    }
}

Item mount_item(WindowComponentServices& services, runtime::ComponentBuildContext& build, const MasonryRecord& record,
                const MasonryRenderer& render, std::size_t index) {
    const auto id = build.mount_component<ItemState>();
    auto& state = build.state<ItemState>(id);
    state.value.set(record.value);
    state.index.set(index);
    const auto node = build.root(id);
    services.nodes().require(node).clip_content = true;
    services.nodes().require(node).clip_decorations = false;
    services.layout().set_layout(node, layout::BoxLayout{{}, true});
    build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
    build.mount_slot(
        id, Content{[record, render, value = state.value, item_index = state.index, column = state.column] {
            if (record.children) {
                SlotContentAccess::function (*record.children)();
            } else if (render) {
                render(record.key, bind([value] { return value.get(); }),
                       bind([item_index] { return item_index.get(); }), bind([column] { return column.get(); }));
            }
        }});
    return {record, id, 0, {}};
}

float finite_geometry(double value) {
    if (!std::isfinite(value) || value < 0 || value > std::numeric_limits<float>::max()) {
        throw std::invalid_argument("Masonry geometry exceeds finite logical dimensions");
    }
    return static_cast<float>(value);
}

} // namespace

MasonryComponentHost::MasonryComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

MasonryComponentHost::~MasonryComponentHost() {
    services_->detach(*this);
}

void* MasonryComponentHost::begin_mount() noexcept {
    auto* previous = active_masonry;
    active_masonry = this;
    return previous;
}

void MasonryComponentHost::end_mount(void* previous) noexcept {
    active_masonry = static_cast<MasonryComponentHost*>(previous);
}

void MasonryComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void MasonryComponentHost::mount(MasonryOptions options, Prop<std::vector<MasonryRecord>> source,
                                 MasonryRenderer render) {
    auto& services = *services_;
    validate_columns(read_prop(options.columns));
    validate_gutter(read_prop(options.gutter));
    const auto records = read_prop(source);
    validate_records(records);
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<MasonryState>();
    auto& state = build.state<MasonryState>(id);
    state.node = build.root(id);
    state.render = std::move(render);
    state.on_layout = std::move(options.on_layout);
    const auto node = state.node;
    services.layout().set_layout(
        node,
        layout::ComponentLayout{[this, id](auto& engine, auto, auto limits) { return measure(id, engine, limits); },
                                [this, id](auto& engine, auto, auto bounds) { place(id, engine, bounds); }});
    build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
    runtime::connect_layout_style(build.scope(id), options.layout, node, services.nodes(), services.dirty());
    auto& scope = build.scope(id);
    connect_prop(scope, options.columns, [this, id](const auto& value) {
        validate_columns(value);
        services_->components().state<MasonryState>(id)->columns = value;
        invalidate(*services_, id);
    });
    connect_prop(scope, options.gutter, [this, id](const auto& value) {
        validate_gutter(value);
        services_->components().state<MasonryState>(id)->gutter = value;
        invalidate(*services_, id);
    });
    connect_prop(scope, options.breakpoints, [this, id](const auto& value) {
        services_->components().state<MasonryState>(id)->breakpoints = value;
        invalidate(*services_, id);
    });
    connect_prop(scope, options.direction, [this, id](FlexDirection value) {
        if (value != FlexDirection::LeftToRight && value != FlexDirection::RightToLeft) {
            throw std::invalid_argument("Masonry direction is invalid");
        }
        services_->components().state<MasonryState>(id)->rtl = value == FlexDirection::RightToLeft;
        invalidate(*services_, id);
    });
    connect_prop(scope, options.fresh, [this, id](bool value) {
        services_->components().state<MasonryState>(id)->fresh = value;
        invalidate(*services_, id);
    });
    connect_prop(scope, options.refresh, [this, id](auto) { invalidate(*services_, id); });
    connect_prop(scope, Prop<float>{services.responsive_layout().window_width()}, [this, id](float value) {
        services_->components().state<MasonryState>(id)->window_width = value;
        invalidate(*services_, id);
    });
    connect_prop(scope, source, [this, id](const auto& value) {
        auto& current = *services_->components().state<MasonryState>(id);
        validate_records(value);
        current.pending = value;
        ++current.pending_revision;
        invalidate(*services_, id);
    });
    const auto initial = state.pending;
    const auto initial_revision = state.pending_revision;
    build.mount_slot(id, Content{[&] {
                         auto& context = runtime::require_component_build_context();
                         for (std::size_t index = 0; index < initial.size(); ++index) {
                             state.items.push_back(mount_item(services, context, initial[index], state.render, index));
                         }
                     }});
    state.committed_revision = initial_revision;
    mounted_.push_back(id);
    invalidate(services, id);
}

void MasonryComponentHost::reconcile(runtime::ComponentId id) {
    auto& services = *services_;
    auto* state = services.components().state<MasonryState>(id);
    if (!state || state->reconciling || state->pending_revision == state->committed_revision) {
        return;
    }
    state->reconciling = true;
    const auto records = state->pending;
    const auto revision = state->pending_revision;
    const auto old = state->items;
    const auto render = state->render;
    std::vector<Item> next;
    std::vector<runtime::ComponentId> added;
    try {
        for (std::size_t index = 0; index < records.size(); ++index) {
            const auto& record = records[index];
            const auto found =
                std::ranges::find_if(old, [&](const auto& item) { return item.record.key == record.key; });
            if (found != old.end() && services.components().contains(found->component)) {
                auto retained = *found;
                retained.record = record;
                if (found->record.children != record.children) {
                    const auto children = services.components().children(retained.component);
                    for (const auto child : children) {
                        services.destroy(child);
                    }
                    if (!services.components().contains(id)) {
                        return;
                    }
                    auto* replacement = services.components().state<ItemState>(retained.component);
                    if (!replacement) {
                        throw std::logic_error("Masonry item retired during content replacement");
                    }
                    const auto replacement_value = replacement->value;
                    replacement_value.set(record.value);
                    if (!services.components().contains(id)) {
                        return;
                    }
                    if (!services.components().contains(retained.component)) {
                        throw std::logic_error("Masonry item retired during content replacement");
                    }
                    services.append_slot(retained.component, Content{[&, record] {
                                             if (record.children) {
                                                 SlotContentAccess::function (*record.children)();
                                             } else if (render) {
                                                 auto& item =
                                                     *services.components().state<ItemState>(retained.component);
                                                 render(record.key, bind([value = item.value] { return value.get(); }),
                                                        bind([value = item.index] { return value.get(); }),
                                                        bind([value = item.column] { return value.get(); }));
                                             }
                                         }});
                }
                auto* item = services.components().state<ItemState>(retained.component);
                if (!services.components().contains(id)) {
                    return;
                }
                if (!item) {
                    throw std::logic_error("Masonry item retired during data update");
                }
                const auto value = item->value;
                const auto item_index = item->index;
                value.set(record.value);
                if (!services.components().contains(id)) {
                    return;
                }
                if (!services.components().contains(retained.component)) {
                    throw std::logic_error("Masonry item retired during data update");
                }
                item_index.set(index);
                if (!services.components().contains(id)) {
                    return;
                }
                if (!services.components().contains(retained.component)) {
                    throw std::logic_error("Masonry item retired during index update");
                }
                next.push_back(std::move(retained));
            } else {
                services.append_slot(id, Content{[&] {
                                         auto& context = runtime::require_component_build_context();
                                         next.push_back(mount_item(services, context, record, render, index));
                                         added.push_back(next.back().component);
                                     }});
                if (!services.components().contains(id)) {
                    return;
                }
            }
        }
        for (const auto& item : old) {
            if (std::ranges::none_of(next,
                                     [&](const auto& candidate) { return candidate.component == item.component; })) {
                services.destroy(item.component);
                if (!services.components().contains(id)) {
                    return;
                }
            }
        }
        state = services.components().state<MasonryState>(id);
        state->items = std::move(next);
        state->committed_revision = revision;
        state->reconciling = false;
        invalidate(services, id);
    } catch (...) {
        for (const auto child : added) {
            services.retire_failed_component(child);
        }
        if (auto* current = services.components().state<MasonryState>(id)) {
            current->reconciling = false;
        }
        throw;
    }
}

void MasonryComponentHost::prepare_auxiliary_layout() {
    const auto mounted = mounted_;
    for (const auto id : mounted) {
        reconcile(id);
        if (auto* state = services_->components().state<MasonryState>(id);
            state && (state->fresh || state->deferred_layout)) {
            state->deferred_layout = false;
            services_->dirty().invalidate_in_frame(state->node, masonry_dirty);
        }
    }
}

runtime::Size MasonryComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                            layout::Constraints limits) {
    auto& state = *services_->components().state<MasonryState>(id);
    const int count = state.columns.resolve(state.window_width, state.breakpoints);
    const float requested_gap = state.gutter.horizontal.resolve(state.window_width, state.breakpoints).value();
    const float vertical = state.gutter.vertical.resolve(state.window_width, state.breakpoints).value();
    float width = limits.max_width;
    if (!std::isfinite(width)) {
        float natural{};
        for (const auto& item : state.items) {
            const auto size = engine.measure_child(services_->components().root(item.component),
                                                   {0, width, 0, std::numeric_limits<float>::infinity()});
            natural = std::max(natural, size.width);
        }
        width =
            finite_geometry(static_cast<double>(natural) * count + static_cast<double>(requested_gap) * (count - 1));
    }
    const float gap = count == 1 ? 0 : std::min(requested_gap, width / static_cast<float>(count - 1));
    const float item_width =
        finite_geometry(std::max(0.0, (static_cast<double>(width) - static_cast<double>(gap) * (count - 1)) / count));
    std::map<int, float> heights;
    auto next = state.items;
    std::vector<MasonryPlacement> assignments;
    float height{};
    for (auto& item : next) {
        int column{};
        if (item.record.column) {
            column = std::min(*item.record.column, count - 1);
        } else {
            // Sparse storage keeps even a very large valid column count bounded by item count.
            while (heights.contains(column) && column < count - 1) {
                ++column;
            }
            float lowest = heights.contains(column) ? heights.at(column) : 0;
            for (const auto& [candidate, used] : heights) {
                if (used < lowest || (used == lowest && candidate < column)) {
                    column = candidate;
                    lowest = used;
                }
            }
        }
        const auto node = services_->components().root(item.component);
        const auto specified = item.record.height ? std::optional{item.record.height->value()} : std::nullopt;
        const auto size = engine.measure_child(node, {0, item_width, 0, std::numeric_limits<float>::infinity()},
                                               item_width, specified);
        const float top = heights.contains(column) ? heights.at(column) : 0;
        const float bottom = finite_geometry(static_cast<double>(top) + size.height);
        item.column = column;
        float x = finite_geometry((static_cast<double>(item_width) + gap) * column);
        if (state.rtl) {
            x = std::max(0.0F, width - x - item_width);
        }
        item.frame = {x, top, item_width, size.height};
        heights[column] = finite_geometry(static_cast<double>(bottom) + vertical);
        height = std::max(height, bottom);
        assignments.push_back({item.record.key, column});
    }
    state.items = std::move(next);
    state.assignments = std::move(assignments);
    state.measure_generation = engine.generation();
    return limits.constrain({width, height});
}

void MasonryComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto& state = *services_->components().state<MasonryState>(id);
    if (state.measure_generation != engine.generation()) {
        throw std::logic_error("Masonry placement requires the matching measurement generation");
    }
    for (const auto& item : state.items) {
        auto frame = item.frame;
        frame.x += bounds.x;
        frame.y += bounds.y;
        engine.place_child(services_->components().root(item.component), frame, true, true);
    }
}

void MasonryComponentHost::synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) {
    const auto mounted = mounted_;
    for (const auto id : mounted) {
        auto* state = services_->components().state<MasonryState>(id);
        if (!state || state->measure_generation == 0) {
            continue;
        }
        const auto request_revision = services_->frame_request_revision();
        const auto items = state->items;
        for (const auto& item : items) {
            if (auto* inner = services_->components().state<ItemState>(item.component)) {
                const auto column = inner->column;
                column.set(item.column);
            }
            if (!services_->components().contains(id)) {
                break;
            }
        }
        state = services_->components().state<MasonryState>(id);
        if (!state) {
            continue;
        }
        if (!state->delivered || *state->delivered != state->assignments) {
            const auto callback = state->on_layout;
            const auto assignments = state->assignments;
            state->delivered = assignments;
            // No component references survive the user callback, including self-destruction.
            if (callback) {
                callback(assignments);
            }
        }
        state = services_->components().state<MasonryState>(id);
        if (state && services_->frame_request_revision() != request_revision) {
            // The window consumes this frame's dirty queues after auxiliary geometry.
            // Preserve remeasurement requested by column bindings or layout callbacks.
            state->deferred_layout = true;
        }
    }
}

std::vector<MasonryItemSnapshot> MasonryComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* state = services_->components().state<MasonryState>(id);
    if (!state) {
        throw std::out_of_range("Masonry component is stale");
    }
    std::vector<MasonryItemSnapshot> result;
    for (const auto& item : state->items) {
        const auto bounds = services_->nodes().require(services_->components().root(item.component)).bounds;
        result.push_back({item.record.key, item.component, item.column, bounds});
    }
    return result;
}

void mount_masonry(MasonryOptions options, Prop<std::vector<MasonryRecord>> items, MasonryRenderer render) {
    if (!active_masonry) {
        throw std::logic_error("Masonry requires an active window component host");
    }
    active_masonry->mount(std::move(options), std::move(items), std::move(render));
}

} // namespace ryn::detail
