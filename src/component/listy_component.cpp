#include "component/config_defaults.hpp"
#include "component/listy_component.hpp"
#include "component/table_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "component/retained_slot_mount.hpp"
#include "component/data_display_retained.hpp"
#include "reactive/untracked.hpp"
#include <ryn/empty.hpp>
#include <ryn/pagination.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <thread>

namespace ryn::detail {
struct ListyRefState final {
    std::thread::id owner{std::this_thread::get_id()};
    std::function<bool(float)> scroll;
    std::function<bool(String, ListyScrollAlign, float)> reveal;
    std::function<bool(String, ListyScrollAlign, float)> reveal_group;

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
    Signal<std::vector<std::shared_ptr<const void>>> group_values{std::vector<std::shared_ptr<const void>>{}};
    component::RetainedSurfaceId surface;
};

struct Row final {
    ListyRecord record;
    runtime::ComponentId component;
    runtime::Size size;
    runtime::Rect frame;
    bool shown{};
    bool page{};
    bool measured{};
};

struct RevealRequest final {
    String key;
    ListyScrollAlign align;
    float offset{};
    bool group{};
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
    std::vector<Row> groups;
    bool virtualized{};
    bool sticky{};
    float viewport_extent{};
    std::vector<runtime::ComponentId> input_order;
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
    std::uint64_t scroll_revision{};
    std::optional<RevealRequest> reveal_request;
    std::optional<float> offset_request;
    float maximum{};
    runtime::Rect clip;
    theme_runtime::Subscription theme;
};

void validate(const std::vector<ListyRecord>& records) {
    std::set<std::string, std::less<>> keys;
    for (const auto& record : records) {
        if (record.key.empty() || !record.value || !keys.emplace(record.key.bytes()).second) {
            throw std::invalid_argument("Listy requires unique nonempty keys and owned data");
        }
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

std::vector<std::pair<bool, std::size_t>> entries(const State& s) {
    std::vector<std::pair<bool, std::size_t>> result;
    if (s.options.group_render) {
        std::map<std::string, std::vector<std::size_t>, std::less<>> members;
        for (std::size_t i = 0; i < s.rows.size(); ++i) {
            if (s.rows[i].page) {
                members[std::string{s.rows[i].record.group.bytes()}].push_back(i);
            }
        }
        for (std::size_t g = 0; g < s.groups.size(); ++g) {
            const auto found = members.find(s.groups[g].record.key.bytes());
            if (found == members.end()) {
                continue;
            }
            result.emplace_back(true, g);
            for (auto i : found->second) {
                result.emplace_back(false, i);
            }
        }
    } else {
        for (std::size_t i = 0; i < s.rows.size(); ++i) {
            if (s.rows[i].page) {
                result.emplace_back(false, i);
            }
        }
    }
    return result;
}

// Unknown rows use an estimate until their first actual measure. Visited keyed
// subtrees stay cached; no editor is recycled into another business identity.
void project(State& s, runtime::Rect bounds) {
    const bool horizontal = s.direction == ListyDirection::Horizontal;
    const auto cell = bounds.width / static_cast<float>(s.columns);
    const auto order = entries(s);
    float position{};
    for (std::size_t i = 0; i < order.size();) {
        const auto [group, index] = order[i];
        auto& row = group ? s.groups[index] : s.rows[index];
        if (group || horizontal) {
            const auto estimate = group ? s.options.virtualization.estimated_group_height
                                        : s.options.virtualization.estimated_item_height;
            const auto extent = row.measured ? (horizontal ? row.size.width : row.size.height) : estimate;
            row.frame = {bounds.x + (horizontal ? (s.rtl ? bounds.width - position - extent : position) : 0),
                         bounds.y + (horizontal ? 0 : position), horizontal ? extent : bounds.width,
                         horizontal ? std::max(bounds.height, row.size.height) : extent};
            position += extent;
            ++i;
            continue;
        }
        auto end = i;
        float height{};
        while (end < order.size() && !order[end].first && end - i < s.columns) {
            const auto& item = s.rows[order[end].second];
            height =
                std::max(height, item.measured ? item.size.height : s.options.virtualization.estimated_item_height);
            ++end;
        }
        for (auto j = i; j < end; ++j) {
            const auto column = static_cast<float>(j - i);
            s.rows[order[j].second].frame = {bounds.x +
                                                 (s.rtl ? static_cast<float>(s.columns - 1) - column : column) * cell,
                                             bounds.y + position, cell, height};
        }
        position += height;
        i = end;
    }
    s.extent = position;
    s.viewport_extent = horizontal ? bounds.width : bounds.height;
    s.maximum = std::max(0.0F, s.extent - s.viewport_extent);
    s.offset = std::clamp(s.offset, 0.0F, s.maximum);
}

bool in_view(const State& s, const Row& row, bool group) {
    if (!row.page) {
        return false;
    }
    if (!s.virtualized) {
        return true;
    }
    const bool horizontal = s.direction == ListyDirection::Horizontal;
    const auto start =
        horizontal ? (s.rtl ? s.viewport_extent - row.frame.x - row.frame.width : row.frame.x) : row.frame.y;
    const auto extent = horizontal ? row.frame.width : row.frame.height;
    const auto margin =
        static_cast<float>(s.options.virtualization.overscan) * s.options.virtualization.estimated_item_height;
    const bool sticky =
        group && s.sticky && start <= s.offset && std::ranges::none_of(s.groups, [&](const auto& next) {
            const auto at = horizontal ? (s.rtl ? s.viewport_extent - next.frame.x - next.frame.width : next.frame.x)
                                       : next.frame.y;
            return next.page && at > start && at <= s.offset;
        });
    return sticky || (start + extent >= s.offset - margin && start <= s.offset + s.viewport_extent + margin);
}

float reveal_offset(const State& s, const Row& row, ListyScrollAlign align, float offset) {
    const bool horizontal = s.direction == ListyDirection::Horizontal;
    const auto start =
        horizontal ? (s.rtl ? s.viewport_extent - row.frame.x - row.frame.width : row.frame.x) : row.frame.y;
    const auto extent = horizontal ? row.frame.width : row.frame.height;
    const auto next = align == ListyScrollAlign::Start                ? start
                      : align == ListyScrollAlign::End                ? start + extent - s.viewport_extent
                      : start < s.offset                              ? start
                      : start + extent > s.offset + s.viewport_extent ? start + extent - s.viewport_extent
                                                                      : s.offset;
    return next + offset;
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
    ListySnapshot result{{}, s->viewport, s->empty, s->pager, s->current.get(), s->offset, s->maximum, {}, 0};
    for (const auto& row : s->rows) {
        result.items.push_back({row.record.key, row.component, row.frame, row.shown});
        result.mounted_items += row.component.valid();
    }
    for (const auto& row : s->groups) {
        result.groups.push_back({row.record.key, row.component, row.frame, row.shown});
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
        s->rows[i].page = shown;
        s->rows[i].shown = shown && (!s->virtualized || rows[i].shown);
        if (rows[i].component.valid() && services.components().set_branch_active(rows[i].component, s->rows[i].shown)) {
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
    const auto v = options.virtualization;
    if (!std::isfinite(v.estimated_item_height) || v.estimated_item_height <= 0 ||
        !std::isfinite(v.estimated_group_height) || v.estimated_group_height <= 0 || v.overscan > 4096) {
        throw std::invalid_argument("Listy virtualization requires positive finite estimates and bounded overscan");
    }
    if (read_prop(options.virtualized) && !options.height) {
        throw std::invalid_argument("Listy virtualized requires height");
    }
    if (options.group_render && std::ranges::any_of(records, [](const auto& row) { return row.group.empty(); })) {
        throw std::invalid_argument("Listy group keys must be nonempty");
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
                float width{};
                for (const auto [group, index] : entries(s)) {
                    auto& row = group ? s.groups[index] : s.rows[index];
                    if (!row.component.valid() || !row.shown) {
                        continue;
                    }
                    const auto columns = horizontal || group ? 1 : s.columns;
                    const auto cell = std::isfinite(limits.max_width) ? limits.max_width / static_cast<float>(columns)
                                                                      : std::numeric_limits<float>::infinity();
                    const auto node = services_->components().root(row.component);
                    if (group) {
                        services_->layout().set_layout(node, layout::BoxLayout{{pad, pad, pad, pad}, true});
                    }
                    row.size = engine.measure_child(node, {0, std::max(0.0F, cell - (group ? 0 : pad * 2)), 0,
                                                           std::numeric_limits<float>::infinity()});
                    if (!group) {
                        row.size.width += pad * 2;
                        row.size.height += pad * 2;
                    }
                    row.measured = true;
                    width = std::max(width, row.size.width * static_cast<float>(columns));
                }
                width = std::isfinite(limits.max_width) ? limits.max_width : width;
                project(s, {0, 0, width, s.height.value_or(s.extent)});
                float height = s.extent;
                if (horizontal) {
                    height = 0;
                    for (const auto& row : s.rows) {
                        if (row.page) {
                            height = std::max(height, row.size.height);
                        }
                    }
                }
                return limits.constrain({width, s.height.value_or(height)});
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                auto& s = *services_->components().state<State>(id);
                if (s.rows.empty()) {
                    s.extent = s.maximum = s.offset = 0;
                    engine.place_child(services_->components().root(s.empty), bounds);
                    return;
                }
                const auto pad = services_->components().theme_scope(id)->snapshot().map().size_small;
                project(s, {0, 0, bounds.width, bounds.height});
                for (const auto [group, index] : entries(s)) {
                    auto& row = group ? s.groups[index] : s.rows[index];
                    if (!row.component.valid() || !row.shown) {
                        continue;
                    }
                    auto frame = row.frame;
                    frame.x += bounds.x;
                    frame.y += bounds.y;
                    const auto padding = group ? 0 : pad;
                    engine.place_child(services_->components().root(row.component),
                                       {frame.x + padding, frame.y + padding, std::max(0.0F, frame.width - padding * 2),
                                        std::max(0.0F, frame.height - padding * 2)});
                }
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
    connect(options.virtualized, [](State& s, bool value) {
        if (value && !s.options.height) {
            throw std::invalid_argument("Listy virtualized requires height");
        }
        s.virtualized = value;
    });
    connect(options.sticky, [](State& s, bool value) { s.sticky = value; });
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
            if (state->options.group_render &&
                std::ranges::any_of(value, [](const auto& row) { return row.group.empty(); })) {
                throw std::invalid_argument("Listy group keys must be nonempty");
            }
            state->pending = value;
            ++state->revision;
            invalidate(*services_, id);
        }
    });
    const auto theme = build.theme_scope();
    s.theme = theme->capture(
        [this, id](theme_runtime::DirtyPhase phase) {
            if (theme_runtime::has_any(phase, theme_runtime::DirtyPhase::geometry | theme_runtime::DirtyPhase::text |
                                                  theme_runtime::DirtyPhase::measure_layout |
                                                  theme_runtime::DirtyPhase::hit_test)) {
                invalidate(*services_, id);
            } else if (services_->components().contains(id)) {
                services_->dirty().invalidate(services_->components().root(id), runtime::DirtyFlags::Material);
            }
        },
        [theme] {
            (void)theme->map();
            (void)theme->alias();
        });
    if (options.ref) {
        const auto ref = options.ref->state_;
        ref->check();
        if (ref->scroll) {
            throw std::invalid_argument("ListyRef is already bound");
        }
        ref->scroll = [this, id](float offset) {
            const auto* before = services_->components().state<State>(id);
            const auto revision = before ? before->scroll_revision : 0;
            const auto changed = scroll(id, offset);
            if (auto* state = services_->components().state<State>(id);
                state && state->scroll_revision == revision + 1 && state->visible && !state->disabled &&
                services_->interactions().component_active(id)) {
                state->offset_request = offset;
            }
            return changed;
        };
        ref->reveal = [this, id](String key, auto align, float offset) {
            return reveal(id, std::move(key), align, offset);
        };
        ref->reveal_group = [this, id](String key, auto align, float offset) {
            return reveal(id, std::move(key), align, offset, true);
        };
        build.on_resource_cleanup(id, [ref] {
            ref->scroll = {};
            ref->reveal = {};
            ref->reveal_group = {};
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
    const auto old_groups = s->groups;
    std::vector<Row> next;
    std::vector<Row> groups;
    try {
        for (std::size_t i = 0; i < records.size(); ++i) {
            const auto& record = records[i];
            const auto found =
                std::ranges::find_if(previous, [&](const Row& row) { return row.record.key == record.key; });
            auto row = found == previous.end() ? Row{} : *found;
            row.record = record;
            if (row.component.valid()) {
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
            }
            next.push_back(std::move(row));
            if (!record.group.empty() &&
                std::ranges::none_of(groups, [&](const auto& g) { return g.record.key == record.group; })) {
                const auto old =
                    std::ranges::find_if(old_groups, [&](const auto& g) { return g.record.key == record.group; });
                auto group = old == old_groups.end() ? Row{} : *old;
                group.record.key = record.group;
                groups.push_back(std::move(group));
            }
        }
        for (const auto& group : groups) {
            if (auto* state = services.components().state<RowState>(group.component)) {
                std::vector<std::shared_ptr<const void>> values;
                for (const auto& record : records) {
                    if (record.group == group.record.key) {
                        values.push_back(record.value);
                    }
                }
                const auto signal = state->group_values;
                signal.set(std::move(values));
                if (!services.components().contains(id)) {
                    return;
                }
                if (!services.components().contains(group.component)) {
                    throw std::logic_error("Listy group was retired during notification");
                }
            }
        }
        for (const auto& old : {previous, old_groups}) {
            for (const auto& row : old) {
                // Item and group keys are separate identity domains.
                const bool retained =
                    std::ranges::any_of(next, [&](const auto& r) { return r.component == row.component; }) ||
                    std::ranges::any_of(groups, [&](const auto& r) { return r.component == row.component; });
                if (row.component.valid() && !retained) {
                    services.destroy(row.component);
                    if (!services.components().contains(id)) {
                        return;
                    }
                }
            }
        }
        s = services.components().state<State>(id);
        s->rows = std::move(next);
        s->groups = std::move(groups);
        s->committed = revision;
        s->updating = false;
        refresh(id);
    } catch (...) {
        services.retire_failed_component(id);
        throw;
    }
}

bool ListyComponentHost::materialize(runtime::ComponentId id) {
    auto& services = *services_;
    auto* s = services.components().state<State>(id);
    if (!s || !s->visible) {
        return false;
    }
    const auto viewport = s->viewport;
    auto bounds = services.nodes().require(services.components().root(viewport)).bounds;
    if (bounds.height <= 0) {
        bounds.height = s->height.value_or(bounds.height);
    }
    project(*s, {0, 0, bounds.width, bounds.height});
    for (auto& group : s->groups) {
        group.page = std::ranges::any_of(
            s->rows, [&](const auto& row) { return row.page && row.record.group == group.record.key; });
        if (!group.page && group.component.valid()) {
            group.shown = false;
            if (services.components().set_branch_active(group.component, false)) {
                services.mark_scene_structure_dirty();
            }
        }
    }
    const auto order = entries(*s);
    bool changed{};
    try {
        for (const auto [group, index] : order) {
            s = services.components().state<State>(id);
            if (!s) {
                return false;
            }
            const auto row = group ? s->groups[index] : s->rows[index];
            const auto focused = services.focus().state().focused;
            const auto* target = focused ? services.interactions().find(*focused) : nullptr;
            const bool pinned =
                target && row.component.valid() && descendant(services, target->component, row.component);
            const bool shown = in_view(*s, row, group) || pinned;
            auto child = row.component;
            if (shown && !child.valid()) {
                const auto render = s->render;
                const auto group_render = s->options.group_render;
                std::vector<std::shared_ptr<const void>> values;
                if (group) {
                    for (const auto& item : s->rows) {
                        if (item.record.group == row.record.key) {
                            values.push_back(item.record.value);
                        }
                    }
                }
                run_untracked([&] {
                    mount_retained_slot(
                        services, viewport, Content{[&] {
                            auto& build = runtime::require_component_build_context();
                            child = build.mount_component<RowState>();
                            auto& state = build.state<RowState>(child);
                            state.value.set(row.record.value);
                            state.index.set(index);
                            state.group_values.set(values);
                            const auto node = build.root(child);
                            services.layout().set_layout(node, layout::BoxLayout{{}, true});
                            services.nodes().require(node).clip_content = true;
                            if (group) {
                                const auto hit_fragment = build.register_scene_fragment(
                                    child, runtime::SceneFragmentPlacement::before_children);
                                const auto hit =
                                    services.interactions().create({child, node, {}, true, false, {}, true, false});
                                services.scene_composer().set_fragment(hit_fragment, {}, hit);
                                const auto surface_fragment = build.register_scene_fragment(
                                    child, runtime::SceneFragmentPlacement::before_children);
                                state.surface = services.surfaces().create_content_range(surface_fragment, {});
                                build.on_resource_cleanup(child,
                                                          [&services, surface = state.surface, hit, hit_fragment] {
                                                              services.pointer().cancel_interaction(hit);
                                                              services.interactions().remove(hit);
                                                              services.scene_composer().remove_fragment(hit_fragment);
                                                              services.surfaces().destroy_content_range(surface);
                                                          });
                            }
                            build.on_resource_cleanup(child,
                                                      [&services, node] { services.layout().remove_layout(node); });
                            const auto data = state.value;
                            const auto position = state.index;
                            const auto grouped = state.group_values;
                            build.mount_slot(child, Content{[&] {
                                                 if (group) {
                                                     group_render(row.record.key,
                                                                  bind([grouped] { return grouped.get(); }));
                                                 } else {
                                                     render(row.record.key, bind([data] { return data.get(); }),
                                                            bind([position] { return position.get(); }));
                                                 }
                                             }});
                        }});
                });
                if (!(s = services.components().state<State>(id))) {
                    return false;
                }
                if (!services.components().contains(child)) {
                    throw std::logic_error("Listy factory retired its retained region");
                }
                (group ? s->groups[index] : s->rows[index]).component = child;
                changed = true;
            }
            auto& updated = group ? s->groups[index] : s->rows[index];
            changed |= updated.shown != shown;
            updated.shown = shown;
            if (child.valid() && services.components().set_branch_active(child, shown)) {
                services.mark_scene_structure_dirty();
            }
        }
        std::vector<runtime::ComponentId> children{s->empty};
        for (const auto& row : s->rows) {
            if (row.component.valid()) {
                children.push_back(row.component);
            }
        }
        // Headers paint above scrolling rows, without an escaping window layer.
        for (const auto& group : s->groups) {
            if (group.component.valid()) {
                children.push_back(group.component);
            }
        }
        if (services.components().children(viewport) != children) {
            services.components().reorder_children(viewport, children);
            changed = true;
        }
        std::vector<runtime::ComponentId> input_order;
        for (const auto [group, index] : order) {
            const auto component = (group ? s->groups[index] : s->rows[index]).component;
            if (component.valid()) {
                input_order.push_back(component);
            }
        }
        if (input_order != s->input_order) {
            display_reorder(services, s->interaction, input_order);
            s->input_order = std::move(input_order);
        }
        if (changed) {
            invalidate(services, id);
        }
        return changed;
    } catch (...) {
        services.retire_failed_component(id);
        throw;
    }
}

void ListyComponentHost::prepare_auxiliary_layout() {
    auto ids = mounted_;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const auto id = ids[i];
        reconcile(id);
        materialize(id);
        for (auto added : mounted_) {
            if (std::ranges::find(ids, added) == ids.end()) {
                ids.push_back(added);
            }
        }
    }
}

void ListyComponentHost::position_window_layers(runtime::Size, runtime::Rect) {
    auto ids = mounted_;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const auto id = ids[i];
        reconcile(id);
        bool corrected{};
        for (;;) {
            bool changed = materialize(id);
            auto* s = services_->components().state<State>(id);
            if (!s) {
                break;
            }
            auto target = s->offset_request;
            if (s->reveal_request) {
                const auto& request = *s->reveal_request;
                const auto& rows = request.group ? s->groups : s->rows;
                const auto found = std::ranges::find_if(
                    rows, [&](const auto& row) { return row.page && row.record.key == request.key; });
                if (found != rows.end()) {
                    target = reveal_offset(*s, *found, request.align, request.offset);
                }
            }
            if (target) {
                const auto next = std::clamp(*target, 0.0F, s->maximum);
                if (s->offset != next) {
                    s->offset = next;
                    corrected = changed = true;
                    invalidate(*services_, id);
                }
            }
            if (!changed) {
                s->reveal_request.reset();
                s->offset_request.reset();
                if (corrected && s->options.on_scroll) {
                    const auto callback = s->options.on_scroll;
                    const auto offset = s->offset;
                    callback(offset);
                }
                break;
            }
            // Each pass settles new measured keys/visibility before the shared P
            // worklist; no extra frame or layout_and_synchronize call is needed.
            const auto node = s->node;
            const auto bounds = services_->nodes().require(node).bounds;
            static_cast<void>(
                services_->layout().measure_child(node, {bounds.width, bounds.width, bounds.height, bounds.height}));
            services_->layout().place_child(node, bounds);
        }
        for (auto added : mounted_) {
            if (std::ranges::find(ids, added) == ids.end()) {
                ids.push_back(added);
            }
        }
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
    ++s->scroll_revision;
    s->reveal_request.reset();
    s->offset_request.reset();
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

bool ListyComponentHost::reveal(runtime::ComponentId id, String key, ListyScrollAlign align, float offset, bool group) {
    if (!std::isfinite(offset) || align < ListyScrollAlign::Start || align > ListyScrollAlign::Auto) {
        throw std::invalid_argument("Listy scroll alignment is invalid");
    }
    const auto* s = services_->components().state<State>(id);
    if (!s) {
        return false;
    }
    const auto& rows = group ? s->groups : s->rows;
    const auto found = std::ranges::find_if(rows, [&](const Row& row) { return row.record.key == key && row.page; });
    if (found == rows.end()) {
        return false;
    }
    const auto next = reveal_offset(*s, *found, align, offset);
    if (align == ListyScrollAlign::Auto) {
        const bool horizontal = s->direction == ListyDirection::Horizontal;
        const auto start = horizontal
                               ? (s->rtl ? s->viewport_extent - found->frame.x - found->frame.width : found->frame.x)
                               : found->frame.y;
        const auto extent = horizontal ? found->frame.width : found->frame.height;
        if (start < s->offset) {
            align = ListyScrollAlign::Start;
        } else if (start + extent > s->offset + s->viewport_extent) {
            align = ListyScrollAlign::End;
        }
    }
    const auto revision = s->scroll_revision;
    const auto changed = scroll(id, next);
    if (auto* state = services_->components().state<State>(id); state && state->scroll_revision == revision + 1 &&
                                                                state->visible && !state->disabled &&
                                                                services_->interactions().component_active(id)) {
        if (align == ListyScrollAlign::Auto) {
            state->offset_request = next;
        } else {
            state->reveal_request = RevealRequest{std::move(key), align, offset, group};
        }
    }
    return changed;
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
                if (row.component.valid() && row.shown) {
                    translate(*services_, services_->components().root(row.component), offset);
                }
            }
            for (std::size_t i = 0; i < s->groups.size(); ++i) {
                const auto& group = s->groups[i];
                if (!group.component.valid() || !group.shown) {
                    continue;
                }
                auto shift = offset;
                if (s->sticky && group.page) {
                    const auto next = std::find_if(s->groups.begin() + i + 1, s->groups.end(),
                                                   [](const auto& row) { return row.page; });
                    if (s->direction == ListyDirection::Vertical) {
                        const auto limit = next == s->groups.end() ? s->extent : next->frame.y;
                        shift.y += std::max(
                            0.0F, std::min(s->offset - group.frame.y, limit - group.frame.y - group.frame.height));
                    } else {
                        const auto start =
                            s->rtl ? s->viewport_extent - group.frame.x - group.frame.width : group.frame.x;
                        const auto limit =
                            next == s->groups.end()
                                ? s->extent
                                : (s->rtl ? s->viewport_extent - next->frame.x - next->frame.width : next->frame.x);
                        const auto pinned =
                            std::max(0.0F, std::min(s->offset - start, limit - start - group.frame.width));
                        shift.x += s->rtl ? -pinned : pinned;
                    }
                }
                translate(*services_, services_->components().root(group.component), shift);
            }
        });
    }
}

void ListyComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    for (auto id : mounted_) {
        if (auto* s = services_->components().state<State>(id)) {
            s->clip = services_->component_clip(id, viewport, clip);
            const auto color = services_->components().theme_scope(id)->snapshot().alias().color_background_container;
            const auto viewport_clip =
                services_->nodes().content_clip(services_->components().root(s->viewport), s->clip);
            for (const auto& group : s->groups) {
                if (!group.component.valid()) {
                    continue;
                }
                const auto* row = services_->components().state<RowState>(group.component);
                if (!row) {
                    services_->retire_failed_component(id);
                    break;
                }
                const auto& node = services_->nodes().require(services_->components().root(group.component));
                const auto effects = graphics::make_corner_fill_effects(
                    {node.bounds, 0}, {false, false, false, false}, color,
                    services_->nodes().content_opacity(services_->components().root(group.component)), node.translation,
                    graphics::EffectClip{1, viewport_clip});
                static_cast<void>(services_->surfaces().update_content_effects(row->surface, effects));
            }
        }
    }
}

bool ListyComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    if (!target) {
        return false;
    }
    // Only take over mixed chains that contain a Listy in this hit's actual
    // layer. Each scroll owner gets one chance, from the target outwards.
    bool mixed{};
    for (auto at = std::optional{target->component}; at; at = services_->components().parent(*at)) {
        if (services_->components().state<State>(*at)) {
            mixed = true;
            break;
        }
        if (services_->components().is_window_layer_root(*at)) {
            return false;
        }
    }
    if (!mixed) {
        return false;
    }
    const auto tables = services_->table().mounted();
    for (auto at = std::optional{target->component}; at && services_->components().contains(*at);
         at = services_->components().parent(*at)) {
        if (std::ranges::find(tables, *at) != tables.end() && services_->table().scroll_owner_input(*at, event)) {
            return true;
        }
        if (auto* s = services_->components().state<State>(*at)) {
            if (window_layer(*services_, target->component) != window_layer(*services_, *at)) {
                return false;
            }
            if (descendant(*services_, target->component, s->viewport)) {
                const auto& node = services_->nodes().require(services_->components().root(s->viewport));
                const auto box = graphics::intersect_effect_bounds({node.bounds.x + node.translation.x,
                                                                    node.bounds.y + node.translation.y,
                                                                    node.bounds.width, node.bounds.height},
                                                                   s->clip);
                if (event.x >= box.x && event.y >= box.y && event.x < box.x + box.width &&
                    event.y < box.y + box.height) {
                    const float delta = s->direction == ListyDirection::Horizontal && event.delta_x != 0
                                            ? event.delta_x
                                            : -event.delta_y;
                    const float step = services_->components().theme_scope(*at)->snapshot().map().font_size * 3;
                    if (scroll(*at, s->offset + delta * step)) {
                        return true;
                    }
                }
            }
        }
        if (services_->components().is_window_layer_root(*at)) {
            return false;
        }
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

bool ListyRef::scrollToGroup(String key, ListyScrollAlign align, float offset) const {
    state_->check();
    return state_->reveal_group && state_->reveal_group(std::move(key), align, offset);
}

bool ListyRef::scrollTo(String key, ListyScrollAlign align, float offset) const {
    state_->check();
    return state_->reveal && state_->reveal(std::move(key), align, offset);
}
} // namespace ryn
