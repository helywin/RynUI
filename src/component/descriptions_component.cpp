#include "component/descriptions_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local DescriptionsComponentHost* active_descriptions{};

struct ItemState final {
    DescriptionsItemSnapshot snapshot;
    runtime::Rect frame;
    runtime::Rect label;
    runtime::Rect content;
    runtime::Rect colon;
    component::RetainedSurfaceId background;
    component::RetainedSurfaceId lines;
};

struct DescriptionsState final {
    runtime::NodeId node;
    DescriptionsSnapshot snapshot;
    std::vector<DescriptionsModelItem> model;
    std::vector<ItemState> items;
    int columns{3};
    bool bordered{};
    bool colon{true};
    bool has_title{};
    bool has_extra{};
    DescriptionsSize size{DescriptionsSize::Default};
    DescriptionsLabelPlacement placement{DescriptionsLabelPlacement::Horizontal};
    Signal<runtime::SemanticForeground> label_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> content_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> title_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> extra_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> title_font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 2> subscriptions;
    component::RetainedSurfaceId border;
    runtime::Rect view;
    runtime::Rect header;
    runtime::Rect title;
    runtime::Rect extra;
};

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

float sized(DescriptionsSize size, float normal, float middle, float small) {
    return size == DescriptionsSize::Small ? small : size == DescriptionsSize::Middle ? middle : normal;
}

void validate(DescriptionsSize size, DescriptionsLabelPlacement placement) {
    if ((size != DescriptionsSize::Default && size != DescriptionsSize::Middle && size != DescriptionsSize::Small) ||
        (placement != DescriptionsLabelPlacement::Horizontal && placement != DescriptionsLabelPlacement::Vertical)) {
        throw std::invalid_argument("Descriptions size or labelPlacement is invalid");
    }
}

runtime::Rect inset(runtime::Rect r, float x, float y) {
    x = std::min(x, r.width / 2);
    y = std::min(y, r.height / 2);
    return {r.x + x, r.y + y, r.width - 2 * x, r.height - 2 * y};
}

runtime::Rect offset(runtime::Rect r, runtime::Point p) {
    return {r.x + p.x, r.y + p.y, r.width, r.height};
}

runtime::Rect intersect(runtime::Rect a, runtime::Rect b) {
    const float x = std::clamp(a.x, b.x, b.x + b.width);
    const float y = std::clamp(a.y, b.y, b.y + b.height);
    return {x, y, std::max(0.0F, std::min(a.x + a.width, b.x + b.width) - x),
            std::max(0.0F, std::min(a.y + a.height, b.y + b.height) - y)};
}
} // namespace

DescriptionsComponentHost::DescriptionsComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

DescriptionsComponentHost::~DescriptionsComponentHost() {
    services_->detach(*this);
}

void* DescriptionsComponentHost::begin_mount() noexcept {
    auto* previous = active_descriptions;
    active_descriptions = this;
    return previous;
}

void DescriptionsComponentHost::end_mount(void* previous) noexcept {
    active_descriptions = static_cast<DescriptionsComponentHost*>(previous);
}

void DescriptionsComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void DescriptionsComponentHost::on_dispose() noexcept {
    mounted_.clear();
}

DescriptionsSnapshot DescriptionsComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* state = services_->components().state<DescriptionsState>(id);
    if (!state) {
        throw std::out_of_range("Descriptions component is stale");
    }
    auto result = state->snapshot;
    for (const auto& item : state->items) {
        result.items.push_back(item.snapshot);
    }
    return result;
}

void DescriptionsComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<DescriptionsState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().descriptions();
    const auto& m = t.metrics;
    s->label_color.set(foreground(s->bordered ? t.colors.bordered_label : t.colors.label));
    s->content_color.set(foreground(t.colors.content));
    s->title_color.set(foreground(t.colors.title));
    s->extra_color.set(foreground(t.colors.extra));
    s->font.set({m.font_family, 400, false, m.font_size, m.line_height});
    s->title_font.set({m.font_family, m.title_font_weight, false, m.title_font_size, m.title_line_height});
    std::vector<runtime::ComponentId> boundaries;
    for (std::size_t i = 0; i < s->items.size(); ++i) {
        auto& item = s->items[i];
        const bool visible = s->model[i].visible;
        item.snapshot.visible = visible;
        if (services_->components().set_branch_active(item.snapshot.viewport, visible)) {
            geometry = true;
            services_->mark_scene_structure_dirty();
        }
        services_->nodes().require(services_->components().root(item.snapshot.viewport)).out_of_flow = !visible;
        const bool colon = !s->bordered && s->colon;
        if (services_->components().set_branch_active(item.snapshot.colon, colon)) {
            geometry = true;
            services_->mark_scene_structure_dirty();
        }
        boundaries.push_back(item.snapshot.viewport);
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
    // Boundary reconciliation delivers user blur callbacks. Never keep component references across it.
    for (const auto branch : boundaries) {
        const auto* current = services_->components().state<DescriptionsState>(id);
        if (!current) {
            return;
        }
        const auto found = std::ranges::find_if(
            current->items, [branch](const auto& item) { return item.snapshot.viewport == branch; });
        if (found != current->items.end()) {
            services_->set_input_blocked(branch, !found->snapshot.visible);
        }
    }
}

void DescriptionsComponentHost::mount(const DescriptionsProps& props, const std::vector<DescriptionsItem>& items) {
    const auto size = read_prop(props.size_);
    const auto placement = read_prop(props.placement_);
    validate(size, placement);
    std::vector<DescriptionsModelItem> model;
    for (const auto& item : items) {
        model.push_back({item.key_, read_prop(item.span_), read_prop(item.visible_)});
    }
    const int columns = read_prop(props.column_);
    auto rows = pack_descriptions(columns, model);
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<DescriptionsState>();
    auto& s = build.state<DescriptionsState>(id);
    s.node = build.root(id);
    s.columns = columns;
    s.size = size;
    s.placement = placement;
    s.bordered = read_prop(props.bordered_);
    s.colon = read_prop(props.colon_);
    s.has_title = props.title_.has_value();
    s.has_extra = props.extra_.has_value();
    s.model = std::move(model);
    s.snapshot.rows = std::move(rows);
    s.items.reserve(items.size());
    services.nodes().require(s.node).clip_content = true;
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    const auto border_fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::after_children);
    s.border = services.surfaces().create_content_range(border_fragment, {});
    build.on_resource_cleanup(id, [&services, node = s.node, border = s.border] {
        services.surfaces().destroy_content_range(border);
        services.layout().remove_layout(node);
    });
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            const auto container = [&](runtime::ComponentId parent, bool clip, bool active = true) {
                runtime::ComponentId child;
                context.mount_slot(parent, Content{[&] {
                                       auto& children = runtime::require_component_build_context();
                                       child = children.mount_component<int>(0);
                                       const auto node = context.root(child);
                                       services.nodes().require(node).clip_content = clip;
                                       services.components().set_branch_active(child, active);
                                       services.layout().set_layout(node, layout::LeafLayout{});
                                       context.on_resource_cleanup(child, [&services, node, child] {
                                           services.interactions().set_input_blocked(child, false);
                                           services.layout().remove_layout(node);
                                       });
                                   }});
                return child;
            };
            const auto region = [&](runtime::ComponentId parent, Content body,
                                    const Signal<runtime::SemanticForeground>& color,
                                    const Signal<runtime::SemanticTypography>& font) {
                const auto child = container(parent, false);
                services.layout().set_layout(context.root(child), layout::BoxLayout{});
                context.mount_slot_with_semantic_text_style(child, body, Prop<runtime::SemanticForeground>{color},
                                                            Prop<runtime::SemanticTypography>{font});
                return child;
            };
            s.snapshot.header = container(id, true, s.has_title || s.has_extra);
            s.snapshot.title = region(s.snapshot.header, Content{[slot = props.title_] {
                                          if (slot) {
                                              SlotContentAccess::function (*slot)();
                                          }
                                      }},
                                      s.title_color, s.title_font);
            s.snapshot.extra = region(s.snapshot.header, Content{[slot = props.extra_] {
                                          if (slot) {
                                              SlotContentAccess::function (*slot)();
                                          }
                                      }},
                                      s.extra_color, s.font);
            for (std::size_t i = 0; i < items.size(); ++i) {
                s.items.emplace_back();
                auto& item = s.items.back();
                item.snapshot.viewport = container(id, true, s.model[i].visible);
                item.snapshot.label = region(item.snapshot.viewport,
                                             Content{[slot = items[i].label_] { SlotContentAccess::function(slot)(); }},
                                             s.label_color, s.font);
                item.snapshot.content =
                    region(item.snapshot.viewport,
                           Content{[slot = items[i].content_] { SlotContentAccess::function(slot)(); }},
                           s.content_color, s.font);
                item.snapshot.colon =
                    region(item.snapshot.viewport, Content{[] { Text(u8":"); }}, s.label_color, s.font);
                const auto fragment = context.register_scene_fragment(item.snapshot.viewport,
                                                                      runtime::SceneFragmentPlacement::before_children);
                item.background = services.surfaces().create_content_range(fragment, {});
                const auto lines = context.register_scene_fragment(item.snapshot.viewport,
                                                                   runtime::SceneFragmentPlacement::after_children);
                item.lines = services.surfaces().create_content_range(lines, {});
                context.on_resource_cleanup(item.snapshot.viewport,
                                            [&services, bg = item.background, lines = item.lines] {
                                                services.surfaces().destroy_content_range(bg);
                                                services.surfaces().destroy_content_range(lines);
                                            });
            }
        }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{[this, id](layout::LayoutEngine& engine, runtime::NodeId,
                                                   layout::Constraints limits) { return measure(id, engine, limits); },
                                        [this, id](layout::LayoutEngine& engine, runtime::NodeId,
                                                   runtime::Rect bounds) { place(id, engine, bounds); }});
    mounted_.push_back(id);
    auto& scope = build.scope(id);
    connect_prop(scope, props.column_, [this, id](int value) {
        if (auto* s = services_->components().state<DescriptionsState>(id)) {
            auto rows = pack_descriptions(value, s->model);
            s->columns = value;
            s->snapshot.rows = std::move(rows);
            refresh(id, true);
        }
    });
    connect_prop(scope, props.size_, [this, id](DescriptionsSize value) {
        if (auto* s = services_->components().state<DescriptionsState>(id)) {
            validate(value, s->placement);
            s->size = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.placement_, [this, id](DescriptionsLabelPlacement value) {
        if (auto* s = services_->components().state<DescriptionsState>(id)) {
            validate(s->size, value);
            s->placement = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.bordered_, [this, id](bool value) {
        if (auto* s = services_->components().state<DescriptionsState>(id)) {
            s->bordered = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.colon_, [this, id](bool value) {
        if (auto* s = services_->components().state<DescriptionsState>(id)) {
            s->colon = value;
            refresh(id, true);
        }
    });
    for (std::size_t i = 0; i < items.size(); ++i) {
        connect_prop(scope, items[i].span_, [this, id, i](int value) {
            if (auto* s = services_->components().state<DescriptionsState>(id)) {
                if (value <= 0) {
                    throw std::invalid_argument("Descriptions span must be positive");
                }
                s->model[i].span = value;
                s->snapshot.rows = pack_descriptions(s->columns, s->model);
                refresh(id, true);
            }
        });
        connect_prop(scope, items[i].visible_, [this, id, i](bool value) {
            if (auto* s = services_->components().state<DescriptionsState>(id)) {
                s->model[i].visible = value;
                s->snapshot.rows = pack_descriptions(s->columns, s->model);
                refresh(id, true);
            }
        });
    }
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::descriptions_colors,
                               &theme_runtime::ThemeScope::descriptions_metrics};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { refresh(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    refresh(id, true);
}

runtime::Size DescriptionsComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                                 layout::Constraints limits) {
    auto& s = *services_->components().state<DescriptionsState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().descriptions().metrics;
    const auto node = [&](runtime::ComponentId child) {
        return services_->components().root(child);
    };
    const auto measure = [&](runtime::ComponentId child, float width) {
        return engine.measure_child(node(child), {0, std::max(0.0F, width), 0, std::numeric_limits<float>::infinity()});
    };
    const float px = s.bordered ? (s.size == DescriptionsSize::Small ? m.padding_inline_small : m.padding_inline) : 0;
    const float py = s.bordered ? sized(s.size, m.padding_block, m.padding_block_middle, m.padding_block_small) : 0;
    float width = limits.max_width;
    if (!std::isfinite(width)) {
        float unit = 0;
        for (const auto& row : s.snapshot.rows) {
            for (const auto& cell : row) {
                const auto& item = s.items[cell.item];
                const auto label = measure(item.snapshot.label, width);
                const auto content = measure(item.snapshot.content, width);
                const float natural = s.placement == DescriptionsLabelPlacement::Vertical
                                          ? std::max(label.width, content.width) + px * 2
                                          : label.width + content.width + px * 4 + m.colon_margin_left +
                                                m.colon_margin_right + m.font_size;
                unit = std::max(unit, natural / static_cast<float>(cell.span));
            }
        }
        width = std::max(unit * static_cast<float>(s.columns),
                         measure(s.snapshot.title, width).width + measure(s.snapshot.extra, width).width);
        if (!std::isfinite(width)) {
            throw std::invalid_argument("Descriptions intrinsic extent overflow");
        }
        width = std::max(width, limits.min_width);
    }
    const float border = s.bordered ? std::min(m.border_width, width / 2) : 0;
    const float inner = std::max(0.0F, width - border * 2);
    float y = 0;
    if (s.has_title || s.has_extra) {
        const auto extra = measure(s.snapshot.extra, width);
        const float gap =
            s.has_extra && s.has_title ? std::min(m.item_padding_end, std::max(0.0F, width - extra.width)) : 0;
        const auto title = measure(s.snapshot.title, std::max(0.0F, width - extra.width - gap));
        const float height = std::max(title.height, extra.height);
        s.header = {0, 0, width, height};
        s.title = {0, (height - title.height) / 2, std::max(0.0F, width - extra.width - gap), title.height};
        s.extra = {width - extra.width, (height - extra.height) / 2, extra.width, extra.height};
        static_cast<void>(engine.measure_child(node(s.snapshot.header), layout::Constraints::fixed(width, height)));
        y = height + m.title_margin_bottom;
    } else {
        s.header = {};
        s.title = {};
        s.extra = {};
    }
    const float view_y = y;
    y += s.snapshot.rows.empty() ? 0 : border;
    for (std::size_t r = 0; r < s.snapshot.rows.size(); ++r) {
        const auto& row = s.snapshot.rows[r];
        float label_height = 0;
        float content_height = 0;
        for (std::size_t c = 0; c < row.size(); ++c) {
            const auto& cell = row[c];
            auto& item = s.items[cell.item];
            const float start = inner * (static_cast<float>(cell.column) / s.columns);
            const float end = inner * (static_cast<float>(cell.column + cell.span) / s.columns);
            const float cell_width = std::max(0.0F, end - start);
            const float gap = !s.bordered && c + 1 < row.size() ? std::min(m.item_padding_end, cell_width) : 0;
            const float available = cell_width - gap;
            item.frame = {border + start, y, cell_width, 0};
            static_cast<void>(measure(item.snapshot.colon, 0));
            if (s.placement == DescriptionsLabelPlacement::Vertical) {
                const float content_width = std::max(0.0F, available - px * 2);
                auto label = measure(item.snapshot.label, content_width);
                const auto colon = !s.bordered && s.colon && label.width > 0
                                       ? measure(item.snapshot.colon, content_width)
                                       : runtime::Size{};
                const float colon_space =
                    colon.width > 0 ? std::min(content_width, colon.width + m.colon_margin_left + m.colon_margin_right)
                                    : 0;
                label = measure(item.snapshot.label, std::max(0.0F, content_width - colon_space));
                const auto content = measure(item.snapshot.content, content_width);
                item.label = {px, py, content_width - colon_space, label.height};
                item.colon = {px + label.width + std::min(m.colon_margin_left, colon_space), py,
                              std::min(colon.width, colon_space), colon.height};
                item.content = {px, 0, content_width, content.height};
                label_height = std::max({label_height, label.height + py * 2, colon.height + py * 2});
                content_height = std::max(content_height, content.height + py * 2);
                item.snapshot.label_cell = {0, 0, available, 0};
                item.snapshot.content_cell = {0, 0, available, 0};
            } else {
                const float label_cell_width =
                    s.bordered ? std::min(available, inner / (static_cast<float>(s.columns) * 2)) : available / 2;
                const float max_label = std::max(0.0F, label_cell_width - px * 2);
                const auto initial_label = measure(item.snapshot.label, max_label);
                const auto colon = !s.bordered && s.colon && initial_label.width > 0
                                       ? measure(item.snapshot.colon, max_label)
                                       : runtime::Size{};
                const float colon_space =
                    colon.width > 0 ? std::min(max_label, colon.width + m.colon_margin_left + m.colon_margin_right) : 0;
                const auto label = measure(item.snapshot.label, std::max(0.0F, max_label - colon_space));
                const float split = s.bordered ? label_cell_width : std::min(available, label.width + colon_space);
                const float content_width = std::max(0.0F, available - split - px * 2);
                const auto content = measure(item.snapshot.content, content_width);
                item.label = {px, py, std::max(0.0F, max_label - colon_space), label.height};
                item.colon = {label.width + std::min(m.colon_margin_left, colon_space), py,
                              std::min(colon.width, colon_space), colon.height};
                item.content = {split + std::min(px, available - split), py, content_width, content.height};
                label_height =
                    std::max({label_height, label.height + py * 2, content.height + py * 2, colon.height + py * 2});
                item.snapshot.label_cell = {0, 0, split, 0};
                item.snapshot.content_cell = {split, 0, available - split, 0};
            }
        }
        const bool vertical = s.placement == DescriptionsLabelPlacement::Vertical;
        const float row_height = label_height + (vertical ? content_height + border : 0);
        for (const auto& cell : row) {
            auto& item = s.items[cell.item];
            item.frame.height = row_height;
            item.snapshot.label_cell.height = label_height;
            if (vertical) {
                item.content.y = label_height + border + py;
                item.snapshot.content_cell.y = label_height + border;
                item.snapshot.content_cell.height = content_height;
            } else {
                item.snapshot.content_cell.height = label_height;
            }
            static_cast<void>(engine.measure_child(node(item.snapshot.viewport),
                                                   layout::Constraints::fixed(item.frame.width, item.frame.height)));
        }
        y += row_height;
        if (r + 1 < s.snapshot.rows.size()) {
            y += s.bordered
                     ? border
                     : sized(s.size, m.item_padding_bottom, m.item_padding_bottom_middle, m.item_padding_bottom_small);
        }
    }
    if (!s.snapshot.rows.empty()) {
        y += border;
    }
    s.view = {0, view_y, width, y - view_y};
    return limits.constrain({width, y});
}

void DescriptionsComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto& s = *services_->components().state<DescriptionsState>(id);
    const runtime::Point origin{bounds.x, bounds.y};
    const auto place = [&](runtime::ComponentId child, runtime::Rect r, runtime::Rect clip) {
        engine.place_child(services_->components().root(child), intersect(offset(r, origin), clip));
    };
    const auto header = intersect(offset(s.header, origin), bounds);
    if (s.has_title || s.has_extra) {
        place(s.snapshot.header, s.header, bounds);
        place(s.snapshot.title, s.title, header);
        place(s.snapshot.extra, s.extra, header);
    }
    for (auto& item : s.items) {
        if (!item.snapshot.visible) {
            continue;
        }
        const auto frame = intersect(offset(item.frame, origin), bounds);
        place(item.snapshot.viewport, item.frame, bounds);
        const runtime::Point cell_origin{bounds.x + item.frame.x, bounds.y + item.frame.y};
        engine.place_child(services_->components().root(item.snapshot.label),
                           intersect(offset(item.label, cell_origin), frame));
        engine.place_child(services_->components().root(item.snapshot.content),
                           intersect(offset(item.content, cell_origin), frame));
        engine.place_child(services_->components().root(item.snapshot.colon),
                           intersect(offset(item.colon, cell_origin), frame));
    }
}

void DescriptionsComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<DescriptionsState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().descriptions();
        const auto& root = services_->nodes().require(s->node);
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
        const float opacity = services_->nodes().content_opacity(s->node);
        const runtime::Point origin{root.bounds.x, root.bounds.y};
        const auto view = offset(s->view, origin);
        const float border = s->bordered ? std::min(t.metrics.border_width, std::min(view.width, view.height) / 2) : 0;
        const auto shape = inset(view, border / 2, border / 2);
        const auto outline = graphics::make_corner_outline_effects(
            {shape, std::min(t.metrics.border_radius, std::min(shape.width, shape.height) / 2)},
            {true, true, true, true}, border, 0, t.colors.border, border > 0 && !s->snapshot.rows.empty() ? opacity : 0,
            root.translation, graphics::EffectClip{1, clip});
        static_cast<void>(services_->surfaces().update_content_effects(s->border, outline));
        for (const auto& row : s->snapshot.rows) {
            for (std::size_t c = 0; c < row.size(); ++c) {
                const auto& item = s->items[row[c].item];
                const auto& node = services_->nodes().require(services_->components().root(item.snapshot.viewport));
                const auto label = offset(item.snapshot.label_cell, {node.bounds.x, node.bounds.y});
                const bool top = item.frame.y == s->view.y + border;
                const bool bottom = &row == &s->snapshot.rows.back();
                const auto bg = graphics::make_corner_fill_effects(
                    {label, std::min(t.metrics.border_radius, std::min(label.width, label.height) / 2)},
                    {top && c == 0, top && c + 1 == row.size() && s->placement == DescriptionsLabelPlacement::Vertical,
                     false, bottom && c == 0 && s->placement == DescriptionsLabelPlacement::Horizontal},
                    t.colors.label_background, s->bordered ? opacity : 0, node.translation,
                    graphics::EffectClip{1, clip});
                static_cast<void>(services_->surfaces().update_content_effects(item.background, bg));
                std::vector<graphics::QuadInstance> lines;
                const auto line = [&](runtime::Rect rect) {
                    rect = intersect(offset(rect, node.translation), clip);
                    if (s->bordered && rect.width > 0 && rect.height > 0) {
                        lines.push_back({{rect.x, rect.y, rect.width, rect.height},
                                         {t.colors.border.red(), t.colors.border.green(), t.colors.border.blue(),
                                          t.colors.border.alpha()},
                                         opacity});
                    }
                };
                if (s->placement == DescriptionsLabelPlacement::Horizontal) {
                    line({label.x + label.width, node.bounds.y, border, node.bounds.height});
                } else {
                    line({node.bounds.x, label.y + label.height, node.bounds.width, border});
                }
                if (c + 1 < row.size()) {
                    line({node.bounds.x + node.bounds.width - border, node.bounds.y, border, node.bounds.height});
                }
                if (!bottom) {
                    line({node.bounds.x, node.bounds.y + node.bounds.height, node.bounds.width, border});
                }
                static_cast<void>(services_->surfaces().update_content_range(item.lines, lines));
            }
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Descriptions(DescriptionsProps props, std::vector<DescriptionsItem> items) {
    if (!detail::active_descriptions) {
        throw std::logic_error("Descriptions requires window component services");
    }
    detail::active_descriptions->mount(props, items);
}
} // namespace ryn
