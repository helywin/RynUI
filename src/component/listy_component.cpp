#include "component/config_defaults.hpp"
#include "component/listy_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/empty.hpp>
#include <ryn/pagination.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>

namespace ryn::detail {
struct ListyRefState final {
    std::thread::id owner{std::this_thread::get_id()};
    std::function<bool(float)> scroll;
    std::function<bool(String, ListyScrollAlign, float)> reveal;

    void check() const {
        if (owner != std::this_thread::get_id()) {
            throw std::logic_error("ListyRef requires its owner thread");
        }
    }
};

namespace {
thread_local ListyComponentHost* active_listy{};
constexpr auto geometry_dirty = runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest;

struct RowState final {
    Signal<std::shared_ptr<const void>> value{std::shared_ptr<const void>{}};
    Signal<std::size_t> index{0};
};

struct Row final {
    ListyRecord record;
    runtime::ComponentId component;
    runtime::Size size;
    runtime::Rect frame;
    bool shown{};
};

struct State final {
    runtime::NodeId node;
    runtime::ComponentId header;
    runtime::ComponentId viewport;
    runtime::ComponentId empty;
    runtime::ComponentId pager;
    runtime::ComponentId footer;
    input::InteractionId interaction;
    ListyOptions options;
    ListyRenderer render;
    std::vector<ListyRecord> pending;
    std::vector<Row> rows;
    std::uint64_t revision{};
    std::uint64_t committed{};
    bool updating{};
    ListyDirection direction{ListyDirection::Vertical};
    std::size_t columns{1};
    bool rtl{};
    bool disabled{};
    bool visible{true};
    bool paginated{};
    std::optional<float> height;
    std::size_t requested{1};
    Signal<std::size_t> current{1};
    Signal<std::size_t> total{0};
    Signal<std::size_t> page_size{10};
    Signal<bool> pager_disabled{false};
    std::array<runtime::Size, 4> sizes;
    std::array<float, 4> gaps{};
    float extent{};
    float offset{};
    float maximum{};
    runtime::Rect clip;
    theme_runtime::Subscription theme;
};

void validate(const std::vector<ListyRecord>& records) {
    std::vector<String> keys;
    for (const auto& record : records) {
        if (record.key.empty() || !record.value || std::ranges::find(keys, record.key) != keys.end()) {
            throw std::invalid_argument("Listy requires unique nonempty keys and owned data");
        }
        keys.push_back(record.key);
    }
}

void positive(std::size_t value) {
    if (value == 0) {
        throw std::invalid_argument("Listy columns, current and pageSize must be positive");
    }
}

void height_valid(float value) {
    if (!std::isfinite(value) || value < 0) {
        throw std::invalid_argument("Listy height must be finite and nonnegative");
    }
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId ancestor) {
    for (auto at = std::optional{child}; at && services.components().contains(*at);
         at = services.components().parent(*at)) {
        if (*at == ancestor) {
            return true;
        }
    }
    return false;
}

std::optional<runtime::ComponentId> window_layer(WindowComponentServices& services, runtime::ComponentId id) {
    for (auto at = std::optional{id}; at && services.components().contains(*at);
         at = services.components().parent(*at)) {
        if (services.components().is_window_layer_root(*at)) {
            return at;
        }
    }
    return {};
}

void translate(WindowComponentServices& services, runtime::NodeId id, runtime::Point offset) {
    const auto& node = services.nodes().require(id);
    if (node.clip_boundary) {
        return;
    }
    runtime::NodePropertyWriter writer{services.nodes(), services.dirty()};
    writer.set_translation(id, offset);
    if (node.owns_child_translation) {
        return;
    }
    const auto children = node.children;
    for (auto child : children) {
        translate(services, child, offset);
    }
}

void invalidate(WindowComponentServices& services, runtime::ComponentId id) {
    if (services.components().contains(id)) {
        services.dirty().invalidate(services.components().root(id), geometry_dirty);
    }
}
} // namespace

ListyComponentHost::ListyComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

ListyComponentHost::~ListyComponentHost() {
    services_->detach(*this);
}

void* ListyComponentHost::begin_mount() noexcept {
    auto* old = active_listy;
    active_listy = this;
    return old;
}

void ListyComponentHost::end_mount(void* old) noexcept {
    active_listy = static_cast<ListyComponentHost*>(old);
}

void ListyComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

ListySnapshot ListyComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<State>(id);
    if (!s) {
        throw std::out_of_range("Listy is stale");
    }
    ListySnapshot result{{}, s->viewport, s->empty, s->pager, s->current.get(), s->offset, s->maximum};
    for (const auto& row : s->rows) {
        result.items.push_back({row.record.key, row.component, row.frame, row.shown});
    }
    return result;
}

void ListyComponentHost::refresh(runtime::ComponentId id) {
    auto& services = *services_;
    auto* s = services.components().state<State>(id);
    if (!s) {
        return;
    }
    const auto count = s->rows.size();
    const auto size = s->page_size.get();
    const auto last = std::max<std::size_t>(1, count / size + (count % size != 0));
    const auto page = s->paginated ? std::min(s->requested, last) : 1;
    const auto current = s->current;
    const auto total = s->total;
    const auto disabled = s->pager_disabled;
    current.set(page);
    if (!(s = services.components().state<State>(id))) {
        return;
    }
    total.set(count);
    if (!(s = services.components().state<State>(id))) {
        return;
    }
    disabled.set(s->disabled);
    if (!(s = services.components().state<State>(id))) {
        return;
    }
    const auto rows = s->rows;
    const auto first = s->paginated ? (page - 1) * size : 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        s = services.components().state<State>(id);
        if (!s) {
            return;
        }
        const bool shown = !s->paginated || (i >= first && i - first < size);
        s->rows[i].shown = shown;
        if (services.components().set_branch_active(rows[i].component, shown)) {
            services.mark_scene_structure_dirty();
        }
    }
    s = services.components().state<State>(id);
    if (!s) {
        return;
    }
    const auto blocked = s->disabled || !s->visible;
    const auto visible = s->visible;
    const std::array branches{std::pair{s->viewport, s->visible}, std::pair{s->empty, count == 0},
                              std::pair{s->pager, s->visible && s->paginated},
                              std::pair{s->header, s->visible && s->options.header.has_value()},
                              std::pair{s->footer, s->visible && s->options.footer.has_value()}};
    // Branch activation controls presentation; the input gate preserves disabled content.
    for (const auto& [component, shown] : branches) {
        if (!services.components().contains(id)) {
            return;
        }
        if (services.components().set_branch_active(component, shown)) {
            services.mark_scene_structure_dirty();
        }
    }
    invalidate(services, id);
    if (!services.components().contains(id)) {
        return;
    }
    if (services.components().set_branch_active(id, visible)) {
        services.mark_scene_structure_dirty();
    }
    services.set_input_blocked(id, blocked);
}

void ListyComponentHost::mount(ListyOptions options, Prop<std::vector<ListyRecord>> source, ListyRenderer renderer) {
    ConfigDefaults::apply(options);
    const auto records = read_prop(source);
    validate(records);
    if (!renderer && !records.empty()) {
        throw std::invalid_argument("Listy requires an itemRender for nonempty data");
    }
    positive(read_prop(options.columns));
    positive(read_prop(options.page_size));
    positive(options.current ? read_prop(*options.current) : options.default_current);
    if (options.height) {
        height_valid(read_prop(*options.height));
    }
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<State>();
    auto& s = build.state<State>(id);
    s.node = build.root(id);
    s.options = options;
    s.render = std::move(renderer);
    s.requested = options.default_current;
    runtime::connect_layout_style(build.scope(id), options.layout, s.node, services.nodes(), services.dirty());
    build.on_resource_cleanup(id, [&services, node = s.node, id] {
        services.interactions().set_input_blocked(id, false);
        services.layout().remove_layout(node);
    });
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            const auto region = [&](Content body) {
                const auto part = context.mount_component<int>(0);
                const auto node = context.root(part);
                services.layout().set_layout(node, layout::BoxLayout{{}, true});
                context.on_resource_cleanup(part, [&services, node] { services.layout().remove_layout(node); });
                context.mount_slot(part, body);
                return part;
            };
            s.header = region(Content{[header = options.header] {
                if (header) {
                    SlotContentAccess::function (*header)();
                }
            }});
            s.viewport = region(Content{[&] {
                auto& child = runtime::require_component_build_context();
                s.empty = child.mount_component<int>(0);
                const auto node = child.root(s.empty);
                services.layout().set_layout(node, layout::BoxLayout{{}, true});
                child.on_resource_cleanup(s.empty, [&services, node] { services.layout().remove_layout(node); });
                child.mount_slot(s.empty, Content{[empty = options.empty] {
                                     if (empty) {
                                         SlotContentAccess::function (*empty)();
                                     } else {
                                         Empty(EmptyProps{}.image(EmptyImage::Compact));
                                     }
                                 }});
            }});
            const auto viewport_node = context.root(s.viewport);
            services.nodes().require(viewport_node).clip_content = true;
            services.nodes().require(viewport_node).owns_child_translation = true;
            s.interaction =
                services.interactions().create({s.viewport, viewport_node, {}, true, false, {}, true, false});
            const auto hit =
                context.register_scene_fragment(s.viewport, runtime::SceneFragmentPlacement::before_children);
            services.scene_composer().set_fragment(hit, {}, s.interaction);
            context.on_resource_cleanup(s.viewport, [&services, interaction = s.interaction, hit] {
                services.pointer().cancel_interaction(interaction);
                services.interactions().remove(interaction);
                services.scene_composer().remove_fragment(hit);
            });
            s.pager = region(Content{
                [this, id, current = s.current, total = s.total, size = s.page_size, disabled = s.pager_disabled] {
                    Pagination(PaginationProps{}
                                   .current(current)
                                   .total(total)
                                   .pageSize(size)
                                   .disabled(disabled)
                                   .showSizeChanger(false)
                                   .onChange([this, id](std::size_t page, std::size_t count) {
                                       auto* state = services_->components().state<State>(id);
                                       if (!state || state->disabled || !state->visible ||
                                           !services_->interactions().component_active(id)) {
                                           return;
                                       }
                                       const auto callback = state->options.on_change;
                                       if (!state->options.current) {
                                           state->requested = page;
                                           state->offset = 0;
                                           refresh(id);
                                       }
                                       if (services_->components().contains(id) && callback) {
                                           callback(page, count);
                                       }
                                   }));
                }});
            s.footer = region(Content{[footer = options.footer] {
                if (footer) {
                    SlotContentAccess::function (*footer)();
                }
            }});
        }});
    services.layout().set_layout(
        services.components().root(s.viewport),
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                auto& s = *services_->components().state<State>(id);
                if (s.rows.empty()) {
                    auto size = engine.measure_child(services_->components().root(s.empty), limits);
                    if (s.height) {
                        size.height = *s.height;
                    }
                    return limits.constrain(size);
                }
                const auto pad = services_->components().theme_scope(id)->snapshot().map().size_small;
                const bool horizontal = s.direction == ListyDirection::Horizontal;
                const auto columns = horizontal ? std::max<std::size_t>(1, s.rows.size()) : s.columns;
                const float cell = std::isfinite(limits.max_width)
                                       ? limits.max_width / static_cast<float>(horizontal ? 1 : columns)
                                       : std::numeric_limits<float>::infinity();
                float width{};
                float height{};
                float row_height{};
                std::size_t index{};
                for (auto& row : s.rows) {
                    if (!row.shown) {
                        continue;
                    }
                    row.size = engine.measure_child(
                        services_->components().root(row.component),
                        {0, std::max(0.0F, cell - pad * 2), 0, std::numeric_limits<float>::infinity()});
                    row.size.width += pad * 2;
                    row.size.height += pad * 2;
                    if (horizontal) {
                        width += row.size.width;
                        height = std::max(height, row.size.height);
                    } else {
                        width = std::max(width, row.size.width * static_cast<float>(columns));
                        row_height = std::max(row_height, row.size.height);
                        if (++index % columns == 0) {
                            height += row_height;
                            row_height = 0;
                        }
                    }
                }
                if (!horizontal) {
                    height += row_height;
                }
                s.extent = horizontal ? width : height;
                return limits.constrain(
                    {std::isfinite(limits.max_width) ? limits.max_width : width, s.height.value_or(height)});
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                auto& s = *services_->components().state<State>(id);
                if (s.rows.empty()) {
                    s.extent = 0;
                    s.maximum = 0;
                    s.offset = 0;
                    engine.place_child(services_->components().root(s.empty), bounds);
                    return;
                }
                const bool horizontal = s.direction == ListyDirection::Horizontal;
                const auto pad = services_->components().theme_scope(id)->snapshot().map().size_small;
                const float cell = bounds.width / static_cast<float>(s.columns);
                float position{};
                std::vector<std::size_t> visible;
                for (std::size_t i = 0; i < s.rows.size(); ++i) {
                    if (s.rows[i].shown) {
                        visible.push_back(i);
                    }
                }
                for (std::size_t i = 0; i < visible.size();) {
                    const auto end = horizontal ? i + 1 : std::min(visible.size(), i + s.columns);
                    float height{};
                    for (auto j = i; j < end; ++j) {
                        height = std::max(height, s.rows[visible[j]].size.height);
                    }
                    for (auto j = i; j < end; ++j) {
                        auto& row = s.rows[visible[j]];
                        const auto column = static_cast<float>(j - i);
                        row.frame = {
                            bounds.x + (horizontal
                                            ? (s.rtl ? bounds.width - position - row.size.width : position)
                                            : (s.rtl ? static_cast<float>(s.columns - 1) - column : column) * cell),
                            bounds.y + (horizontal ? 0 : position), horizontal ? row.size.width : cell, height};
                        engine.place_child(services_->components().root(row.component),
                                           {row.frame.x + pad, row.frame.y + pad,
                                            std::max(0.0F, row.frame.width - pad * 2),
                                            std::max(0.0F, height - pad * 2)});
                    }
                    position += horizontal ? s.rows[visible[i]].size.width : height;
                    i = end;
                }
                s.extent = position;
                s.maximum = std::max(0.0F, position - (horizontal ? bounds.width : bounds.height));
                s.offset = std::clamp(s.offset, 0.0F, s.maximum);
            }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        auto& s = *services_->components().state<State>(id);
                        float height{};
                        float width{};
                        bool occupied{};
                        const auto gap = services_->components().theme_scope(id)->snapshot().map().size_small;
                        const std::array parts{s.header, s.viewport, s.pager, s.footer};
                        for (std::size_t i = 0; i < parts.size(); ++i) {
                            s.gaps[i] = 0;
                            s.sizes[i] = services_->components().branch_active(parts[i])
                                             ? engine.measure_child(
                                                   services_->components().root(parts[i]),
                                                   {0, limits.max_width, 0,
                                                    std::max(0.0F, limits.max_height - height - (occupied ? gap : 0))})
                                             : runtime::Size{};
                            if (s.sizes[i].height > 0) {
                                s.gaps[i] = occupied ? gap : 0;
                                occupied = true;
                            }
                            height += s.gaps[i];
                            height += s.sizes[i].height;
                            width = std::max(width, s.sizes[i].width);
                        }
                        return limits.constrain({std::isfinite(limits.max_width) ? limits.max_width : width, height});
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                        auto& s = *services_->components().state<State>(id);
                        float y = bounds.y;
                        const std::array parts{s.header, s.viewport, s.pager, s.footer};
                        for (std::size_t i = 0; i < parts.size(); ++i) {
                            y += s.gaps[i];
                            if (services_->components().branch_active(parts[i])) {
                                engine.place_child(services_->components().root(parts[i]),
                                                   {bounds.x, y, bounds.width, s.sizes[i].height});
                            }
                            y += s.sizes[i].height;
                        }
                    }});
    auto& scope = build.scope(id);
    const auto connect = [&](const auto& prop, auto setter) {
        connect_prop(scope, prop, [this, id, setter](auto value) {
            if (auto* state = services_->components().state<State>(id)) {
                setter(*state, value);
                refresh(id);
            }
        });
    };
    connect(options.direction, [](State& s, ListyDirection value) {
        if (value != ListyDirection::Vertical && value != ListyDirection::Horizontal) {
            throw std::invalid_argument("Listy direction is invalid");
        }
        s.direction = value;
        s.offset = 0;
    });
    connect(options.columns, [](State& s, auto value) {
        positive(value);
        s.columns = value;
    });
    connect(options.rtl, [](State& s, bool value) { s.rtl = value; });
    connect(options.disabled, [](State& s, bool value) { s.disabled = value; });
    connect(options.visible, [](State& s, bool value) { s.visible = value; });
    connect(options.pagination, [](State& s, bool value) {
        s.paginated = value;
        s.offset = 0;
    });
    connect(options.page_size, [](State& s, auto value) {
        positive(value);
        s.page_size.set(value);
        s.offset = 0;
    });
    if (options.current) {
        connect(*options.current, [](State& s, auto value) {
            positive(value);
            s.requested = value;
            s.offset = 0;
        });
    }
    if (options.height) {
        connect(*options.height, [](State& s, float value) {
            height_valid(value);
            s.height = value;
        });
    }
    connect_prop(scope, source, [this, id](const auto& value) {
        validate(value);
        if (auto* state = services_->components().state<State>(id)) {
            if (!state->render && !value.empty()) {
                throw std::invalid_argument("Listy requires an itemRender for nonempty data");
            }
            state->pending = value;
            ++state->revision;
            invalidate(*services_, id);
        }
    });
    const auto theme = build.theme_scope();
    s.theme = theme->capture([this, id](auto) { invalidate(*services_, id); }, [theme] { (void)theme->map(); });
    if (options.ref) {
        const auto ref = options.ref->state_;
        ref->check();
        if (ref->scroll) {
            throw std::invalid_argument("ListyRef is already bound");
        }
        ref->scroll = [this, id](float offset) {
            return scroll(id, offset);
        };
        ref->reveal = [this, id](String key, auto align, float offset) {
            return reveal(id, std::move(key), align, offset);
        };
        build.on_resource_cleanup(id, [ref] {
            ref->scroll = {};
            ref->reveal = {};
        });
    }
    mounted_.push_back(id);
    // Dynamic append cannot reenter ComponentHost mount. The first prepare phase mounts rows.
    refresh(id);
}

void ListyComponentHost::reconcile(runtime::ComponentId id) {
    auto& services = *services_;
    auto* s = services.components().state<State>(id);
    if (!s || s->updating || s->revision == s->committed) {
        return;
    }
    s->updating = true;
    const auto records = s->pending;
    const auto revision = s->revision;
    const auto previous = s->rows;
    const auto viewport = s->viewport;
    const auto render = s->render;
    std::vector<Row> next;
    try {
        for (std::size_t i = 0; i < records.size(); ++i) {
            const auto& record = records[i];
            const auto found =
                std::ranges::find_if(previous, [&](const Row& row) { return row.record.key == record.key; });
            if (found != previous.end()) {
                auto row = *found;
                row.record = record;
                auto* state = services.components().state<RowState>(row.component);
                if (!state) {
                    throw std::logic_error("Listy row was retired during update");
                }
                const auto data = state->value;
                const auto index = state->index;
                data.set(record.value);
                if (!services.components().contains(id)) {
                    return;
                }
                index.set(i);
                if (!services.components().contains(id)) {
                    return;
                }
                if (!services.components().contains(row.component)) {
                    throw std::logic_error("Listy row was retired during notification");
                }
                next.push_back(std::move(row));
            } else {
                services.append_slot(viewport, Content{[&] {
                                         auto& build = runtime::require_component_build_context();
                                         const auto child = build.mount_component<RowState>();
                                         auto& state = build.state<RowState>(child);
                                         state.value.set(record.value);
                                         state.index.set(i);
                                         const auto node = build.root(child);
                                         services.layout().set_layout(node, layout::BoxLayout{{}, true});
                                         build.on_resource_cleanup(
                                             child, [&services, node] { services.layout().remove_layout(node); });
                                         const auto data = state.value;
                                         const auto index = state.index;
                                         build.mount_slot(child, Content{[&] {
                                                              render(record.key, bind([data] { return data.get(); }),
                                                                     bind([index] { return index.get(); }));
                                                          }});
                                         next.push_back({record, child});
                                     }});
                if (!services.components().contains(id)) {
                    return;
                }
            }
        }
        for (const auto& row : previous) {
            if (std::ranges::none_of(next, [&](const Row& item) { return item.component == row.component; })) {
                services.destroy(row.component);
                if (!services.components().contains(id)) {
                    return;
                }
            }
        }
        s = services.components().state<State>(id);
        if (!s) {
            return;
        }
        s->rows = std::move(next);
        std::vector<runtime::ComponentId> order{s->empty};
        for (const auto& row : s->rows) {
            order.push_back(row.component);
        }
        services.components().reorder_children(s->viewport, order);
        s->committed = revision;
        s->updating = false;
        refresh(id);
    } catch (...) {
        // A renderer/observer can mutate arbitrary descendants. Retire this exact list generation.
        services.retire_failed_component(id);
        throw;
    }
}

void ListyComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (auto id : ids) {
        reconcile(id);
    }
}

bool ListyComponentHost::scroll(runtime::ComponentId id, float value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("Listy scroll offset must be finite");
    }
    auto* s = services_->components().state<State>(id);
    if (!s || !s->visible || s->disabled || !services_->interactions().component_active(id)) {
        return false;
    }
    const float next = std::clamp(value, 0.0F, s->maximum);
    if (next == s->offset) {
        return false;
    }
    s->offset = next;
    const auto callback = s->options.on_scroll;
    invalidate(*services_, id);
    if (callback) {
        callback(next);
    }
    return true;
}

bool ListyComponentHost::reveal(runtime::ComponentId id, String key, ListyScrollAlign align, float offset) {
    if (!std::isfinite(offset) || align < ListyScrollAlign::Start || align > ListyScrollAlign::Auto) {
        throw std::invalid_argument("Listy scroll alignment is invalid");
    }
    const auto* s = services_->components().state<State>(id);
    if (!s) {
        return false;
    }
    const auto found =
        std::ranges::find_if(s->rows, [&](const Row& row) { return row.record.key == key && row.shown; });
    if (found == s->rows.end()) {
        return false;
    }
    const bool horizontal = s->direction == ListyDirection::Horizontal;
    const auto box = services_->nodes().require(services_->components().root(s->viewport)).bounds;
    const float start =
        horizontal ? (s->rtl ? box.x + box.width - found->frame.x - found->frame.width : found->frame.x - box.x)
                   : found->frame.y - box.y;
    const float size = horizontal ? found->frame.width : found->frame.height;
    const float viewport = horizontal ? box.width : box.height;
    const auto next = align == ListyScrollAlign::Start      ? start
                      : align == ListyScrollAlign::End      ? start + size - viewport
                      : start < s->offset                   ? start
                      : start + size > s->offset + viewport ? start + size - viewport
                                                            : s->offset;
    return scroll(id, next + offset);
}

void ListyComponentHost::synchronize_retained_translations() {
    for (auto id : mounted_) {
        services_->enqueue_retained_translation(id, [this, id] {
            const auto* s = services_->components().state<State>(id);
            if (!s) {
                return;
            }
            auto offset = services_->nodes().require(s->node).translation;
            if (s->direction == ListyDirection::Horizontal) {
                offset.x += s->rtl ? s->offset : -s->offset;
            } else {
                offset.y -= s->offset;
            }
            for (const auto& row : s->rows) {
                translate(*services_, services_->components().root(row.component), offset);
            }
        });
    }
}

void ListyComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    for (auto id : mounted_) {
        if (auto* s = services_->components().state<State>(id)) {
            s->clip = services_->component_clip(id, viewport, clip);
        }
    }
}

bool ListyComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    if (!target) {
        return false;
    }
    // The topmost hit's nearest list owns the wheel, including lists in popup/window layers.
    for (auto at = std::optional{target->component}; at && services_->components().contains(*at);
         at = services_->components().parent(*at)) {
        auto* s = services_->components().state<State>(*at);
        if (!s) {
            continue;
        }
        if (window_layer(*services_, target->component) != window_layer(*services_, *at)) {
            return false;
        }
        if (!descendant(*services_, target->component, s->viewport)) {
            return false;
        }
        const auto& node = services_->nodes().require(services_->components().root(s->viewport));
        const auto box =
            graphics::intersect_effect_bounds({node.bounds.x + node.translation.x, node.bounds.y + node.translation.y,
                                               node.bounds.width, node.bounds.height},
                                              s->clip);
        if (event.x < box.x || event.y < box.y || event.x >= box.x + box.width || event.y >= box.y + box.height) {
            return false;
        }
        const float delta =
            s->direction == ListyDirection::Horizontal && event.delta_x != 0 ? event.delta_x : -event.delta_y;
        const float step = services_->components().theme_scope(*at)->snapshot().map().font_size * 3;
        return scroll(*at, s->offset + delta * step);
    }
    return false;
}

void mount_listy(ListyOptions options, Prop<std::vector<ListyRecord>> items, ListyRenderer renderer) {
    if (!active_listy) {
        throw std::logic_error("Listy requires window component services");
    }
    active_listy->mount(std::move(options), std::move(items), std::move(renderer));
}
} // namespace ryn::detail

namespace ryn {
ListyRef::ListyRef() : state_(std::make_shared<detail::ListyRefState>()) {}

bool ListyRef::bound() const {
    state_->check();
    return bool(state_->scroll);
}

bool ListyRef::scrollTo(float offset) const {
    state_->check();
    return state_->scroll && state_->scroll(offset);
}

bool ListyRef::scrollTo(String key, ListyScrollAlign align, float offset) const {
    state_->check();
    return state_->reveal && state_->reveal(std::move(key), align, offset);
}
} // namespace ryn
