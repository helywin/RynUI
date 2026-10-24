#include "component/breadcrumb_component.hpp"
#include "component/navigation_model.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <limits>

namespace ryn::detail {
namespace {
thread_local BreadcrumbComponentHost* active_breadcrumb{};

struct BreadcrumbRow final {
    runtime::ComponentId component;
    runtime::ComponentId separator;
    runtime::NodeId node;
    runtime::NodeId title_node;
    std::optional<runtime::NodeId> icon_node;
    input::InteractionId interaction;
    component::RetainedSurfaceId surface;
    Signal<String> title{String{}};
    Signal<runtime::SemanticForeground> color{{0, 0, 0, 1}};
    runtime::Size title_size;
    runtime::Size icon_size;
    runtime::Size natural;
    runtime::Size separator_size;
};

struct BreadcrumbState final {
    runtime::NodeId node;
    BreadcrumbModel model;
    std::vector<BreadcrumbRow> rows;
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> icon_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticForeground> separator_color{{0, 0, 0, 1}};
    std::array<theme_runtime::Subscription, 3> subscriptions;
    std::optional<std::size_t> hovered;
    std::optional<std::size_t> pressed;
    runtime::Rect clip;
    std::function<void(BreadcrumbActivation)> callback;
};

runtime::SemanticForeground fg(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

runtime::Rect translated(const runtime::Node& n) {
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

bool inside(runtime::Rect b, float x, float y) {
    return x >= b.x && y >= b.y && x < b.x + b.width && y < b.y + b.height;
}

runtime::NodeId slot(WindowComponentServices& services, runtime::ComponentBuildContext& build, const Content& content,
                     const Signal<runtime::SemanticForeground>* color = nullptr,
                     const Signal<runtime::SemanticTypography>* font = nullptr) {
    const auto component = build.mount_component<int>(0);
    const auto node = build.root(component);
    services.nodes().require(node).clip_content = true;
    services.layout().set_layout(node, layout::BoxLayout{});
    build.on_resource_cleanup(component, [&services, node] { services.layout().remove_layout(node); });
    if (color && font) {
        build.mount_slot_with_semantic_text_style(component, content, Prop<runtime::SemanticForeground>{*color},
                                                  Prop<runtime::SemanticTypography>{*font});
    } else {
        build.mount_slot(component, content);
    }
    return node;
}
} // namespace

BreadcrumbComponentHost::BreadcrumbComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

BreadcrumbComponentHost::~BreadcrumbComponentHost() {
    services_->detach(*this);
}

void* BreadcrumbComponentHost::begin_mount() noexcept {
    auto* old = active_breadcrumb;
    active_breadcrumb = this;
    return old;
}

void BreadcrumbComponentHost::end_mount(void* old) noexcept {
    active_breadcrumb = static_cast<BreadcrumbComponentHost*>(old);
}

void BreadcrumbComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

BreadcrumbSnapshot BreadcrumbComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<BreadcrumbState>(id);
    if (!s) {
        throw std::out_of_range("Breadcrumb component is stale");
    }
    BreadcrumbSnapshot result{s->model.current(), {}, {}};
    for (const auto& row : s->rows) {
        result.items.push_back(row.component);
        result.separators.push_back(row.separator);
    }
    return result;
}

void BreadcrumbComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<BreadcrumbState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().breadcrumb();
    const auto& m = t.metrics;
    s->font.set({m.font_family, 400, false, m.font_size, m.line_height});
    s->icon_font.set({m.font_family, 400, false, m.icon_size, m.line_height});
    s->separator_color.set(fg(t.colors.separator));
    const auto current = s->model.current();
    const bool shown = current.has_value();
    auto& root = services_->nodes().require(s->node);
    if (root.out_of_flow == shown) {
        root.out_of_flow = !shown;
        geometry = true;
    }
    if (services_->components().set_branch_active(id, shown)) {
        services_->mark_scene_structure_dirty();
        geometry = true;
    }
    std::vector<std::pair<runtime::ComponentId, bool>> branches;
    for (std::size_t i = 0; i < s->rows.size(); ++i) {
        auto& row = s->rows[i];
        const bool visible = s->model.items[i].visible;
        for (auto [component, active] :
             {std::pair{row.component, visible}, std::pair{row.separator, visible && current != i}}) {
            if (services_->components().set_branch_active(component, active)) {
                services_->mark_scene_structure_dirty();
                geometry = true;
            }
            services_->nodes().require(services_->components().root(component)).out_of_flow = !active;
        }
        const bool eligible = s->callback && s->model.eligible(i);
        if (services_->interactions().set_eligible(row.interaction, eligible)) {
            services_->dirty().invalidate(row.node, runtime::DirtyFlags::HitTest);
        }
        row.color.set(fg(s->model.items[i].disabled    ? t.colors.disabled
                         : current == i                ? t.colors.current
                         : s->hovered == i && eligible ? t.colors.hover
                         : s->callback                 ? t.colors.link
                                                       : t.colors.item));
        branches.emplace_back(row.component, !visible);
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
    // Boundary changes can invoke user cleanup. Keep value snapshots after publication.
    for (auto [component, blocked] : branches) {
        if (services_->components().contains(component)) {
            services_->set_input_blocked(component, blocked);
        }
    }
    reconcile(id);
}

void BreadcrumbComponentHost::reconcile(runtime::ComponentId id) {
    const auto* s = services_->components().state<BreadcrumbState>(id);
    if (!s) {
        return;
    }
    std::vector<input::InteractionId> cancelled;
    for (std::size_t i = 0; i < s->rows.size(); ++i) {
        if (!s->model.eligible(i) || !services_->interactions().branch_active(s->rows[i].interaction)) {
            cancelled.push_back(s->rows[i].interaction);
        }
    }
    for (auto hit : cancelled) {
        services_->pointer().cancel_interaction(hit);
        services_->focus().cancel_interaction(hit);
    }
}

void BreadcrumbComponentHost::activate(runtime::ComponentId id, std::size_t i) {
    const auto* s = services_->components().state<BreadcrumbState>(id);
    if (!s || !s->callback || !s->model.eligible(i) ||
        !services_->interactions().branch_active(s->rows[i].interaction)) {
        return;
    }
    const auto callback = s->callback;
    const BreadcrumbActivation value{s->model.items[i].key, i};
    callback(value);
}

void BreadcrumbComponentHost::mount(const BreadcrumbProps& props, const std::vector<BreadcrumbItem>& items) {
    BreadcrumbModel model;
    for (const auto& item : items) {
        model.items.push_back({item.key_, read_prop(item.disabled_), read_prop(item.visible_), {}});
    }
    validate_navigation_items(model.items);
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<BreadcrumbState>();
    auto& s = build.state<BreadcrumbState>(id);
    s.node = build.root(id);
    s.model = std::move(model);
    s.callback = props.on_activate_;
    s.rows.resize(items.size());
    services.nodes().require(s.node).clip_content = true;
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    build.mount_slot(
        id, Content{[&] {
            auto& children = runtime::require_component_build_context();
            for (std::size_t i = 0; i < items.size(); ++i) {
                auto& row = s.rows[i];
                const auto& item = items[i];
                row.component = children.mount_component<int>(0);
                row.node = children.root(row.component);
                row.title.set(read_prop(item.title_));
                row.interaction = services.interactions().create({row.component, row.node, {}, false, true});
                const auto hit =
                    children.register_scene_fragment(row.component, runtime::SceneFragmentPlacement::before_children);
                services.scene_composer().set_fragment(hit, {}, row.interaction);
                const auto fragment =
                    children.register_scene_fragment(row.component, runtime::SceneFragmentPlacement::before_children);
                row.surface = services.surfaces().create_content_range(fragment, {});
                children.on_resource_cleanup(row.component, [&services, node = row.node, hit,
                                                             interaction = row.interaction, surface = row.surface] {
                    services.pointer().cancel_interaction(interaction);
                    services.focus().cancel_interaction(interaction);
                    services.interactions().remove(interaction);
                    services.scene_composer().remove_fragment(hit);
                    services.surfaces().destroy_content_range(surface);
                    services.layout().remove_layout(node);
                });
                services.layout().set_layout(
                    row.node,
                    layout::ComponentLayout{[this, id, i](layout::LayoutEngine& e, runtime::NodeId,
                                                          layout::Constraints c) { return measure_item(id, i, e, c); },
                                            [this, id, i](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect b) {
                                                place_item(id, i, e, b);
                                            }});
                input::InteractionHandlers handlers;
                handlers.target = [this, id, i](input::PointerDispatchContext& event) {
                    auto* current = services_->components().state<BreadcrumbState>(id);
                    if (!current) {
                        return;
                    }
                    if (event.kind() == input::PointerEventKind::cancel) {
                        current->pressed.reset();
                        refresh(id, false);
                        return;
                    }
                    if (!current->callback || !current->model.eligible(i) ||
                        !services_->interactions().branch_active(current->rows[i].interaction) ||
                        event.event().button != input::PointerButton::primary) {
                        return;
                    }
                    if (event.kind() == input::PointerEventKind::down) {
                        current->pressed = i;
                        static_cast<void>(event.capture_pointer());
                        event.stop_propagation();
                        refresh(id, false);
                    } else if (event.kind() == input::PointerEventKind::up) {
                        const auto& row = current->rows[i];
                        const auto& node = services_->nodes().require(row.node);
                        const bool hit =
                            current->pressed == i && event.press_origin() == row.interaction &&
                            inside(graphics::intersect_effect_bounds(
                                       translated(node), services_->nodes().content_clip(row.node, current->clip)),
                                   event.event().x, event.event().y);
                        current->pressed.reset();
                        static_cast<void>(event.release_pointer_capture());
                        event.stop_propagation();
                        refresh(id, false);
                        if (hit) {
                            activate(id, i);
                        }
                    }
                };
                services.interactions().set_handlers(row.interaction, std::move(handlers));
                input::FocusHandlers focus;
                focus.state_changed = [this, id](input::FocusPresentation) {
                    refresh(id, false);
                };
                services.interactions().set_focus_handlers(row.interaction, std::move(focus));
                children.mount_slot_with_semantic_text_style(
                    row.component, Content{[&] {
                        auto& inner = runtime::require_component_build_context();
                        if (item.icon_) {
                            row.icon_node = slot(services, inner,
                                                 Content{[icon = *item.icon_] { SlotContentAccess::function(icon)(); }},
                                                 &row.color, &s.icon_font);
                        }
                        row.title_node =
                            slot(services, inner,
                                 item.rich_title_
                                     ? Content{[title = *item.rich_title_] { SlotContentAccess::function(title)(); }}
                                     : Content{[title = row.title] { Text(TextProps{}.content(title)); }});
                    }},
                    Prop<runtime::SemanticForeground>{row.color}, Prop<runtime::SemanticTypography>{s.font});
                row.separator = children.mount_component<int>(0);
                const auto sep_node = children.root(row.separator);
                services.nodes().require(sep_node).clip_content = true;
                services.layout().set_layout(sep_node, layout::BoxLayout{});
                children.on_resource_cleanup(row.separator,
                                             [&services, sep_node] { services.layout().remove_layout(sep_node); });
                children.mount_slot_with_semantic_text_style(
                    row.separator,
                    props.separator_
                        ? Content{[separator = *props.separator_] { SlotContentAccess::function(separator)(); }}
                        : Content{[] { Text(u8"/"); }},
                    Prop<runtime::SemanticForeground>{s.separator_color}, Prop<runtime::SemanticTypography>{s.font});
            }
        }});
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) { return measure(id, e, c); },
            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect b) { place(id, e, b); }});
    mounted_.push_back(id);
    for (std::size_t i = 0; i < items.size(); ++i) {
        connect_prop(build.scope(id), items[i].title_, [this, id, i](String value) {
            if (auto* s = services_->components().state<BreadcrumbState>(id)) {
                s->rows[i].title.set(std::move(value));
            }
        });
        connect_prop(build.scope(id), items[i].visible_, [this, id, i](bool value) {
            if (auto* s = services_->components().state<BreadcrumbState>(id)) {
                s->model.items[i].visible = value;
                refresh(id, true);
            }
        });
        connect_prop(build.scope(id), items[i].disabled_, [this, id, i](bool value) {
            if (auto* s = services_->components().state<BreadcrumbState>(id)) {
                s->model.items[i].disabled = value;
                refresh(id, false);
            }
        });
    }
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::breadcrumb_colors,
                               &theme_runtime::ThemeScope::breadcrumb_metrics,
                               &theme_runtime::ThemeScope::breadcrumb_effects};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { refresh(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    refresh(id, true);
}

runtime::Size BreadcrumbComponentHost::measure_item(runtime::ComponentId id, std::size_t i,
                                                    layout::LayoutEngine& engine, layout::Constraints c) {
    auto& s = *services_->components().state<BreadcrumbState>(id);
    auto& row = s.rows[i];
    const auto& m = services_->components().theme_scope(id)->snapshot().breadcrumb().metrics;
    const float available = std::max(0.0F, c.max_width - m.padding * 2);
    row.icon_size =
        row.icon_node ? engine.measure_child(*row.icon_node, {0, available, 0, c.max_height}) : runtime::Size{};
    const float gap = row.icon_node ? std::min(m.icon_gap, std::max(0.0F, available - row.icon_size.width)) : 0;
    row.title_size = engine.measure_child(row.title_node,
                                          {0, std::max(0.0F, available - row.icon_size.width - gap), 0, c.max_height});
    return c.constrain({m.padding * 2 + row.icon_size.width + gap + row.title_size.width,
                        std::max(m.line_height, std::max(row.icon_size.height, row.title_size.height))});
}

void BreadcrumbComponentHost::place_item(runtime::ComponentId id, std::size_t i, layout::LayoutEngine& engine,
                                         runtime::Rect b) {
    auto& row = services_->components().state<BreadcrumbState>(id)->rows[i];
    const auto& m = services_->components().theme_scope(id)->snapshot().breadcrumb().metrics;
    const float padding = std::min(m.padding, b.width / 2);
    float x = b.x + padding;
    float remaining = std::max(0.0F, b.width - padding * 2);
    if (row.icon_node) {
        const float w = std::min(remaining, row.icon_size.width);
        const float h = std::min(b.height, row.icon_size.height);
        engine.place_child(*row.icon_node, {x, b.y + (b.height - h) / 2, w, h});
        const float gap = std::min(m.icon_gap, remaining - w);
        x += w + gap;
        remaining -= w + gap;
    }
    const float h = std::min(b.height, row.title_size.height);
    engine.place_child(row.title_node, {x, b.y + (b.height - h) / 2, std::min(remaining, row.title_size.width), h});
}

runtime::Size BreadcrumbComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                               layout::Constraints c) {
    auto& s = *services_->components().state<BreadcrumbState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().breadcrumb().metrics;
    float width = 0;
    float height = 0;
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (!s.model.items[i].visible) {
            continue;
        }
        auto& row = s.rows[i];
        row.natural = engine.measure_child(row.node, {0, std::numeric_limits<float>::infinity(), 0, c.max_height});
        width += row.natural.width;
        height = std::max(height, row.natural.height);
        if (s.model.current() != i) {
            row.separator_size = engine.measure_child(services_->components().root(row.separator),
                                                      {0, std::numeric_limits<float>::infinity(), 0, c.max_height});
            width += row.separator_size.width + m.separator_margin * 2;
            height = std::max(height, row.separator_size.height);
        }
    }
    return c.constrain({width, height});
}

void BreadcrumbComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect b) {
    auto& s = *services_->components().state<BreadcrumbState>(id);
    const auto margin = services_->components().theme_scope(id)->snapshot().breadcrumb().metrics.separator_margin;
    float total = 0;
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (s.model.items[i].visible) {
            total +=
                s.rows[i].natural.width + (s.model.current() == i ? 0 : s.rows[i].separator_size.width + margin * 2);
        }
    }
    const float scale = total > b.width && total > 0 ? b.width / total : 1;
    float x = b.x;
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (!s.model.items[i].visible) {
            continue;
        }
        auto& row = s.rows[i];
        const float w = std::min(std::max(0.0F, b.x + b.width - x), row.natural.width * scale);
        static_cast<void>(engine.measure_child(row.node, layout::Constraints::fixed(w, b.height)));
        engine.place_child(row.node, {x, b.y, w, b.height}, true, true);
        x += w;
        if (s.model.current() != i) {
            const float w = row.separator_size.width * scale;
            const float h = std::min(b.height, row.separator_size.height);
            x += margin * scale;
            engine.place_child(services_->components().root(row.separator), {x, b.y + (b.height - h) / 2, w, h});
            x += w + margin * scale;
        }
    }
}

bool BreadcrumbComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    const auto focused = services_->focus().state().focused;
    for (auto id : mounted_) {
        const auto* s = services_->components().state<BreadcrumbState>(id);
        if (!s) {
            continue;
        }
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            if (focused != s->rows[i].interaction || !s->callback || !s->model.eligible(i) ||
                !services_->interactions().branch_active(s->rows[i].interaction)) {
                continue;
            }
            if (event.key != input::Key::enter && event.key != input::Key::space) {
                return false;
            }
            if (event.action == input::KeyAction::down && !event.repeat) {
                services_->focus().defer_focus(s->rows[i].interaction, input::FocusModality::keyboard);
                activate(id, i);
            }
            return true;
        }
    }
    return false;
}

void BreadcrumbComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                               std::optional<input::InteractionId> hit,
                                               std::optional<input::InteractionId>) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<BreadcrumbState>(id);
        if (!s) {
            continue;
        }
        std::optional<std::size_t> hovered;
        for (std::size_t i = 0; event.action != input::PointerAction::cancel && i < s->rows.size(); ++i) {
            if (hit == s->rows[i].interaction) {
                hovered = i;
            }
        }
        if (s->hovered != hovered) {
            s->hovered = hovered;
            refresh(id, false);
        }
    }
}

void BreadcrumbComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        reconcile(id);
    }
}

void BreadcrumbComponentHost::on_window_active(bool active) {
    if (!active) {
        const auto ids = mounted_;
        for (auto id : ids) {
            if (auto* s = services_->components().state<BreadcrumbState>(id)) {
                s->pressed.reset();
                s->hovered.reset();
                refresh(id, false);
            }
        }
    }
}

void BreadcrumbComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<BreadcrumbState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        s->clip = services_->component_clip(id, viewport, document_clip);
        const auto& t = services_->components().theme_scope(id)->snapshot().breadcrumb();
        const auto focus = services_->focus().state();
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            const auto& row = s->rows[i];
            const auto& node = services_->nodes().require(row.node);
            const graphics::EffectClip clip{1, services_->nodes().content_clip(row.node, s->clip)};
            const float opacity = services_->nodes().content_opacity(row.node);
            const float radius = std::min(t.metrics.radius, std::min(node.bounds.width, node.bounds.height) / 2);
            const bool eligible =
                s->callback && s->model.eligible(i) && services_->interactions().branch_active(row.interaction);
            const auto bg = graphics::make_corner_fill_effects(
                {node.bounds, radius}, {true, true, true, true}, t.colors.hover_background,
                eligible && (s->hovered == i || s->pressed == i) ? opacity : 0, node.translation, clip);
            std::vector<graphics::RoundedEffectInstance> effects{bg.begin(), bg.end()};
            effects.push_back(graphics::make_outline_effect(
                {node.bounds, radius}, t.effects.focus_width > 0 ? t.effects.focus_width : 1, t.effects.focus_offset,
                t.colors.focus,
                eligible && focus.focused == row.interaction && focus.focus_visible && t.effects.focus_width > 0
                    ? opacity
                    : 0,
                node.translation, clip));
            static_cast<void>(services_->surfaces().update_content_effects(row.surface, effects));
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Breadcrumb(BreadcrumbProps props, std::vector<BreadcrumbItem> items) {
    if (!detail::active_breadcrumb) {
        throw std::logic_error("Breadcrumb requires window component services");
    }
    detail::active_breadcrumb->mount(props, items);
}
} // namespace ryn
