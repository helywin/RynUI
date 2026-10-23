#include "component/collapse_component.hpp"
#include "input/pressable_behavior.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>

namespace ryn::detail {
namespace {
thread_local CollapseComponentHost* active_collapse{};

struct PanelState final {
    String key;
    CollapsePanelSnapshot snapshot;
    runtime::ComponentId extra_viewport;
    input::InteractionId interaction;
    input::PressableBehavior press;
    component::RetainedSurfaceId header_effects;
    component::RetainedSurfaceId body_effects;
    Signal<IconName> arrow{IconName::RightOutlined};
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    runtime::Size title_size;
    runtime::Size extra_size;
    runtime::Size header_size;
    runtime::Size body_size;
    bool has_extra{};
};

struct CollapseState final {
    runtime::NodeId node;
    std::vector<PanelState> panels;
    CollapseKeys keys;
    bool controlled{};
    bool accordion{};
    ControlSize size{ControlSize::Middle};
    Signal<runtime::SemanticForeground> body_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> header_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> body_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> arrow_font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 4> subscriptions;
    component::RetainedSurfaceId background;
    component::RetainedSurfaceId border;
    std::function<void(CollapseKeys)> on_change;
};

void validate(ControlSize size) {
    if (size != ControlSize::Small && size != ControlSize::Middle && size != ControlSize::Large) {
        throw std::invalid_argument("Collapse size is invalid");
    }
}

float sized(ControlSize size, float small, float middle, float large) {
    return size == ControlSize::Small ? small : size == ControlSize::Large ? large : middle;
}

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

CollapseKeys normalized(const CollapseState& s) {
    CollapseKeys result;
    for (const auto& key : s.keys) {
        if (std::ranges::any_of(s.panels, [&](const auto& p) { return p.key == key; }) &&
            std::ranges::find(result, key) == result.end()) {
            result.push_back(key);
            if (s.accordion) {
                break;
            }
        }
    }
    return result;
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId ancestor) {
    for (auto current = std::optional{child}; current; current = services.components().parent(*current)) {
        if (*current == ancestor) {
            return true;
        }
    }
    return false;
}

auto fill(runtime::Rect rect, float radius, std::array<bool, 4> corners, Color color, float opacity,
          runtime::Point translation, graphics::EffectClip clip) {
    radius = std::min(radius, std::min(rect.width, rect.height) / 2);
    return graphics::make_corner_fill_effects({rect, radius}, corners, color, opacity, translation, clip);
}
} // namespace

CollapseComponentHost::CollapseComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

CollapseComponentHost::~CollapseComponentHost() {
    services_->detach(*this);
}

void* CollapseComponentHost::begin_mount() noexcept {
    auto* previous = active_collapse;
    active_collapse = this;
    return previous;
}

void CollapseComponentHost::end_mount(void* previous) noexcept {
    active_collapse = static_cast<CollapseComponentHost*>(previous);
}

void CollapseComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

CollapseSnapshot CollapseComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<CollapseState>(id);
    if (!s) {
        throw std::out_of_range("Collapse component is stale");
    }
    CollapseSnapshot result{normalized(*s), {}};
    for (const auto& row : s->panels) {
        result.panels.push_back(row.snapshot);
    }
    return result;
}

void CollapseComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<CollapseState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().collapse();
    const auto& f = t.typography;
    s->header_font.set({f.font_family, f.font_weight, false,
                        s->size == ControlSize::Large ? f.header_font_size_large : f.font_size,
                        s->size == ControlSize::Large ? f.header_line_height_large : f.line_height});
    s->body_font.set({f.font_family, f.font_weight, false, f.font_size, f.line_height});
    s->arrow_font.set({f.font_family, f.font_weight, false, t.metrics.arrow_size, t.metrics.arrow_size});
    s->body_color.set(foreground(t.colors.body_text));
    const auto active = normalized(*s);
    std::vector<runtime::ComponentId> hidden;
    std::vector<input::InteractionId> disabled;
    for (auto& row : s->panels) {
        row.foreground.set(foreground(row.snapshot.disabled ? t.colors.disabled_text : t.colors.header_text));
        if (services_->interactions().set_eligible(row.interaction, !row.snapshot.disabled)) {
            services_->dirty().invalidate(services_->components().root(row.snapshot.header),
                                          runtime::DirtyFlags::HitTest);
        }
        if (row.snapshot.disabled) {
            static_cast<void>(row.press.reset());
            disabled.push_back(row.interaction);
        }
        row.snapshot.expanded = std::ranges::find(active, row.key) != active.end();
        row.arrow.set(row.snapshot.expanded ? IconName::DownOutlined : IconName::RightOutlined);
        if (services_->components().set_branch_active(row.snapshot.body, row.snapshot.expanded)) {
            services_->mark_scene_structure_dirty();
            geometry = true;
            if (!row.snapshot.expanded) {
                hidden.push_back(row.snapshot.body);
            }
        }
        if (geometry) {
            const float padding =
                sized(s->size, t.metrics.body_padding_small, t.metrics.body_padding, t.metrics.body_padding_large);
            services_->layout().set_layout(services_->components().root(row.snapshot.body),
                                           layout::BoxLayout{{padding, padding, padding, padding}, true});
        }
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
    // Cancellation may invoke callbacks that dispose this Collapse. Keep only value snapshots beyond this point.
    const auto order = services_->interactions().declaration_order();
    const std::vector<input::InteractionId> interactions{order.begin(), order.end()};
    for (auto hit : interactions) {
        const auto* record = services_->interactions().find(hit);
        if (record &&
            (std::ranges::find(disabled, hit) != disabled.end() || std::ranges::any_of(hidden, [&](auto body) {
                 return services_->components().contains(body) && descendant(*services_, record->component, body);
             }))) {
            services_->pointer().cancel_pointer_interaction(hit);
        }
    }
}

void CollapseComponentHost::activate(runtime::ComponentId id, std::size_t index) {
    auto* s = services_->components().state<CollapseState>(id);
    if (!s || index >= s->panels.size() || s->panels[index].snapshot.disabled ||
        !services_->interactions().component_active(s->panels[index].snapshot.header)) {
        return;
    }
    auto next = normalized(*s);
    const auto key = s->panels[index].key;
    if (std::ranges::find(next, key) != next.end()) {
        std::erase(next, key);
    } else {
        if (s->accordion) {
            next.clear();
        }
        next.push_back(key);
    }
    const auto callback = s->on_change;
    if (!s->controlled) {
        s->keys = next;
    }
    refresh(id, false);
    if (callback && services_->components().contains(id) && services_->components().scope(id).active()) {
        callback(std::move(next));
    }
}

void CollapseComponentHost::mount(const CollapseProps& props, const std::vector<CollapsePanel>& panels) {
    validate(read_prop(props.size_));
    if (props.active_keys_ && props.default_keys_) {
        throw std::invalid_argument("Collapse activeKeys and defaultActiveKeys conflict");
    }
    CollapseKeys keys;
    for (const auto& panel : panels) {
        if (panel.key_.empty() || std::ranges::find(keys, panel.key_) != keys.end()) {
            throw std::invalid_argument("Collapse panel keys must be non-empty and unique");
        }
        keys.push_back(panel.key_);
    }
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<CollapseState>();
    auto& s = build.state<CollapseState>(id);
    s.node = build.root(id);
    services.nodes().require(s.node).clip_content = true;
    s.controlled = props.active_keys_.has_value();
    s.keys = props.active_keys_ ? read_prop(*props.active_keys_) : props.default_keys_.value_or(CollapseKeys{});
    s.accordion = read_prop(props.accordion_);
    s.size = read_prop(props.size_);
    s.on_change = props.on_change_;
    s.panels.reserve(panels.size());
    CollapseKeys initially_active;
    for (const auto& key : s.keys) {
        if (std::ranges::find(keys, key) != keys.end() &&
            std::ranges::find(initially_active, key) == initially_active.end()) {
            initially_active.push_back(key);
            if (s.accordion) {
                break;
            }
        }
    }
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    const auto background = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    const auto border = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::after_children);
    s.background = services.surfaces().create_content_range(background, {});
    s.border = services.surfaces().create_content_range(border, {});
    build.on_resource_cleanup(id, [&services, node = s.node, bg = s.background, border = s.border] {
        services.surfaces().destroy_content_range(bg);
        services.surfaces().destroy_content_range(border);
        services.layout().remove_layout(node);
    });
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            const auto region = [&](runtime::ComponentId parent, const Content& content,
                                    const Signal<runtime::SemanticForeground>& color,
                                    const Signal<runtime::SemanticTypography>& font, bool shown = true) {
                runtime::ComponentId result;
                context.mount_slot(parent, Content{[&] {
                                       auto& children = runtime::require_component_build_context();
                                       result = children.mount_component<int>(0);
                                       services.components().set_branch_active(result, shown);
                                       const auto node = children.root(result);
                                       services.layout().set_layout(node, layout::BoxLayout{});
                                       children.on_resource_cleanup(
                                           result, [&services, node] { services.layout().remove_layout(node); });
                                       children.mount_slot_with_semantic_text_style(
                                           result, content, Prop<runtime::SemanticForeground>{color},
                                           Prop<runtime::SemanticTypography>{font});
                                   }});
                return result;
            };
            for (std::size_t index = 0; index < panels.size(); ++index) {
                const auto& panel = panels[index];
                s.panels.emplace_back();
                auto& row = s.panels.back();
                row.key = panel.key_;
                row.snapshot.disabled = read_prop(panel.disabled_);
                row.has_extra = panel.extra_.has_value();
                row.snapshot.header = context.mount_component<int>(0);
                const auto header = row.snapshot.header;
                const auto node = context.root(header);
                services.nodes().require(node).clip_content = true;
                services.layout().set_layout(node, layout::LeafLayout{});
                row.interaction =
                    services.interactions().create({header, node, {}, !row.snapshot.disabled, true, {}, true, true});
                const auto hit =
                    context.register_scene_fragment(header, runtime::SceneFragmentPlacement::before_children);
                services.scene_composer().set_fragment(hit, {}, row.interaction);
                const auto decoration =
                    context.register_scene_fragment(header, runtime::SceneFragmentPlacement::before_children);
                row.header_effects = services.surfaces().create_content_range(decoration, {});
                context.on_resource_cleanup(
                    header, [&services, node, hit, interaction = row.interaction, effects = row.header_effects] {
                        services.pointer().cancel_interaction(interaction);
                        services.interactions().remove(interaction);
                        services.scene_composer().remove_fragment(hit);
                        services.surfaces().destroy_content_range(effects);
                        services.layout().remove_layout(node);
                    });
                input::InteractionHandlers handlers;
                handlers.target = [this, id, index](input::PointerDispatchContext& event) {
                    auto* s = services_->components().state<CollapseState>(id);
                    if (!s) {
                        return;
                    }
                    auto& row = s->panels[index];
                    const bool allowed =
                        !row.snapshot.disabled && services_->interactions().component_active(row.snapshot.header);
                    const auto result = row.press.dispatch(event, row.interaction, allowed);
                    if (event.kind() == input::PointerEventKind::down || event.kind() == input::PointerEventKind::up) {
                        event.stop_propagation();
                    }
                    if (result.activate) {
                        activate(id, index);
                    }
                };
                services.interactions().set_handlers(row.interaction, std::move(handlers));
                input::FocusHandlers focus;
                focus.state_changed = [this, id](input::FocusPresentation) {
                    if (const auto* s = services_->components().state<CollapseState>(id)) {
                        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
                    }
                };
                services.interactions().set_focus_handlers(row.interaction, std::move(focus));
                context.mount_slot_with_semantic_text_style(header, Content{[&] { Icon(IconProps{}.name(row.arrow)); }},
                                                            Prop<runtime::SemanticForeground>{row.foreground},
                                                            Prop<runtime::SemanticTypography>{s.arrow_font});
                row.snapshot.arrow = services.nodes().require(node).children.back();
                row.snapshot.title =
                    region(header, Content{[title = panel.title_] { SlotContentAccess::function(title)(); }},
                           row.foreground, s.header_font);
                // Keep native extra controls outside the disabled header interaction ancestry.
                auto extra_parent = id;
                if (row.has_extra) {
                    row.extra_viewport = context.mount_component<int>(0);
                    extra_parent = row.extra_viewport;
                    const auto viewport_node = context.root(row.extra_viewport);
                    services.nodes().require(viewport_node).clip_content = true;
                    services.layout().set_layout(viewport_node, layout::LeafLayout{});
                    context.on_resource_cleanup(row.extra_viewport, [&services, viewport_node] {
                        services.layout().remove_layout(viewport_node);
                    });
                }
                row.snapshot.extra = region(extra_parent, Content{[extra = panel.extra_] {
                                                if (extra) {
                                                    SlotContentAccess::function (*extra)();
                                                }
                                            }},
                                            row.foreground, s.header_font);
                row.snapshot.body =
                    region(id, Content{[body = panel.body_] { SlotContentAccess::function(body)(); }}, s.body_color,
                           s.body_font, std::ranges::find(initially_active, row.key) != initially_active.end());
                const auto body_effects = context.register_scene_fragment(
                    row.snapshot.body, runtime::SceneFragmentPlacement::before_children);
                row.body_effects = services.surfaces().create_content_range(body_effects, {});
                context.on_resource_cleanup(row.snapshot.body, [&services, effects = row.body_effects] {
                    services.surfaces().destroy_content_range(effects);
                });
            }
        }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{[this, id](layout::LayoutEngine& engine, runtime::NodeId,
                                                   layout::Constraints limits) { return measure(id, engine, limits); },
                                        [this, id](layout::LayoutEngine& engine, runtime::NodeId,
                                                   runtime::Rect bounds) { place(id, engine, bounds); }});
    mounted_.push_back(id);
    refresh(id, true);
    auto& scope = build.scope(id);
    if (props.active_keys_) {
        connect_prop(scope, *props.active_keys_, [this, id](CollapseKeys keys) {
            if (auto* s = services_->components().state<CollapseState>(id)) {
                s->keys = std::move(keys);
                refresh(id, false);
            }
        });
    }
    connect_prop(scope, props.accordion_, [this, id](bool value) {
        if (auto* s = services_->components().state<CollapseState>(id)) {
            s->accordion = value;
            if (!s->controlled) {
                s->keys = normalized(*s);
            }
            refresh(id, false);
        }
    });
    connect_prop(scope, props.size_, [this, id](ControlSize value) {
        validate(value);
        if (auto* s = services_->components().state<CollapseState>(id)) {
            s->size = value;
            refresh(id, true);
        }
    });
    for (std::size_t index = 0; index < panels.size(); ++index) {
        connect_prop(scope, panels[index].disabled_, [this, id, index](bool value) {
            if (auto* s = services_->components().state<CollapseState>(id)) {
                s->panels[index].snapshot.disabled = value;
                refresh(id, false);
            }
        });
    }
    const auto theme = build.theme_scope();
    const std::array accessors{
        &theme_runtime::ThemeScope::collapse_colors, &theme_runtime::ThemeScope::collapse_metrics,
        &theme_runtime::ThemeScope::collapse_typography, &theme_runtime::ThemeScope::collapse_effects};
    for (std::size_t index = 0; index < accessors.size(); ++index) {
        s.subscriptions[index] = theme->capture(
            [this, id, geometry = index == 1 || index == 2](theme_runtime::DirtyPhase) { refresh(id, geometry); },
            [theme, accessor = accessors[index]] { static_cast<void>((theme.get()->*accessor)()); });
    }
}

runtime::Size CollapseComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                             layout::Constraints limits) {
    auto& s = *services_->components().state<CollapseState>(id);
    const auto& t = services_->components().theme_scope(id)->snapshot().collapse();
    const auto& m = t.metrics;
    const float border = std::min(m.border_width, limits.max_width / 2);
    const float width = std::max(0.0F, limits.max_width - border * 2);
    const float start = std::min(sized(s.size, m.header_start_small, m.header_start, m.header_start_large), width);
    const float end = std::min(sized(s.size, m.header_end_small, m.header_end, m.header_end_large), width - start);
    const float block = sized(s.size, m.header_block_small, m.header_block, m.header_block_large);
    const float available = std::max(0.0F, width - start - end);
    float height = border * 2;
    float natural_width = 0;
    for (auto& row : s.panels) {
        const float arrow = std::min(m.arrow_size, available);
        const float arrow_gap = std::min(m.header_gap, std::max(0.0F, available - arrow));
        const float content_width = std::max(0.0F, available - arrow - arrow_gap);
        row.extra_size = row.has_extra ? engine.measure_child(services_->components().root(row.snapshot.extra),
                                                              {0, content_width, 0, limits.max_height})
                                       : runtime::Size{};
        const float extra_gap =
            row.has_extra ? std::min(m.header_gap, std::max(0.0F, content_width - row.extra_size.width)) : 0;
        row.title_size = engine.measure_child(
            services_->components().root(row.snapshot.title),
            {0, std::max(0.0F, content_width - row.extra_size.width - extra_gap), 0, limits.max_height});
        const float line_height =
            s.size == ControlSize::Large ? t.typography.header_line_height_large : t.typography.line_height;
        const float header_width = std::isfinite(width) ? width
                                                        : start + end + arrow + arrow_gap + row.title_size.width +
                                                              row.extra_size.width + extra_gap;
        row.header_size = {header_width,
                           std::max({line_height, row.title_size.height, row.extra_size.height, arrow}) + block * 2};
        static_cast<void>(
            engine.measure_child(services_->components().root(row.snapshot.header),
                                 layout::Constraints::fixed(row.header_size.width, row.header_size.height)));
        if (row.has_extra) {
            static_cast<void>(
                engine.measure_child(services_->components().root(row.extra_viewport),
                                     layout::Constraints::fixed(row.header_size.width, row.header_size.height)));
        }
        static_cast<void>(engine.measure_child(row.snapshot.arrow, layout::Constraints::fixed(arrow, arrow)));
        row.body_size = row.snapshot.expanded
                            ? engine.measure_child(
                                  services_->components().root(row.snapshot.body),
                                  {0, width, 0, std::max(0.0F, limits.max_height - height - row.header_size.height)})
                            : runtime::Size{};
        natural_width = std::max(natural_width, std::max(start + end + arrow + arrow_gap + row.title_size.width +
                                                             row.extra_size.width + extra_gap,
                                                         row.body_size.width));
        height += row.header_size.height + row.body_size.height;
    }
    return limits.constrain({std::isfinite(limits.max_width) ? limits.max_width : natural_width + border * 2, height});
}

void CollapseComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto& s = *services_->components().state<CollapseState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().collapse().metrics;
    const float border = std::min(m.border_width, std::min(bounds.width, bounds.height) / 2);
    const float width = std::max(0.0F, bounds.width - border * 2);
    const float start = std::min(sized(s.size, m.header_start_small, m.header_start, m.header_start_large), width);
    const float end = std::min(sized(s.size, m.header_end_small, m.header_end, m.header_end_large), width - start);
    const float available = std::max(0.0F, width - start - end);
    const float arrow = std::min(m.arrow_size, available);
    const float arrow_gap = std::min(m.header_gap, std::max(0.0F, available - arrow));
    float y = bounds.y + border;
    const float bottom = bounds.y + bounds.height - border;
    for (auto& row : s.panels) {
        const float height = std::min(row.header_size.height, std::max(0.0F, bottom - y));
        const float arrow_size = std::min(arrow, height);
        const float title_height = std::min(height, row.title_size.height);
        const float extra_height = std::min(height, row.extra_size.height);
        const float x = bounds.x + border;
        engine.place_child(services_->components().root(row.snapshot.header), {x, y, width, height});
        engine.place_child(row.snapshot.arrow, {x + start + (arrow - arrow_size) / 2, y + (height - arrow_size) / 2,
                                                arrow_size, arrow_size});
        const float content = std::max(0.0F, available - arrow - arrow_gap);
        const float extra = std::min(content, row.extra_size.width);
        const float extra_gap = row.has_extra ? std::min(m.header_gap, content - extra) : 0;
        engine.place_child(services_->components().root(row.snapshot.title),
                           {x + start + arrow + arrow_gap, y + (height - title_height) / 2,
                            std::max(0.0F, content - extra - extra_gap), title_height});
        if (row.has_extra) {
            // Clip to the available header while retaining space around native focus outlines.
            engine.place_child(services_->components().root(row.extra_viewport), {x, y, width, height});
            engine.place_child(services_->components().root(row.snapshot.extra),
                               {x + width - end - extra, y + (height - extra_height) / 2, extra, extra_height});
        }
        y += height;
        if (row.snapshot.expanded) {
            const float body_height = std::min(row.body_size.height, std::max(0.0F, bottom - y));
            engine.place_child(services_->components().root(row.snapshot.body), {x, y, width, body_height});
            y += body_height;
        }
    }
}

bool CollapseComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<CollapseState>(id);
        if (!s || !services_->interactions().component_active(id)) {
            continue;
        }
        const auto focused = services_->focus().state().focused;
        for (std::size_t index = 0; index < s->panels.size(); ++index) {
            const auto& row = s->panels[index];
            if (focused != row.interaction || row.snapshot.disabled) {
                continue;
            }
            const bool activation = event.key == input::Key::enter || event.key == input::Key::space;
            const bool navigation = event.key == input::Key::up || event.key == input::Key::down ||
                                    event.key == input::Key::home || event.key == input::Key::end;
            if (!activation && !navigation) {
                return false;
            }
            services_->focus().defer_focus(row.interaction, input::FocusModality::keyboard);
            if (event.action != input::KeyAction::down || event.repeat) {
                return true;
            }
            if (activation) {
                activate(id, index);
                return true;
            }
            const int step = event.key == input::Key::up || event.key == input::Key::end ? -1 : 1;
            const auto count = s->panels.size();
            auto next = event.key == input::Key::home ? count - 1 : event.key == input::Key::end ? 0 : index;
            for (std::size_t n = 0; n < count; ++n) {
                next = step < 0 ? (next + count - 1) % count : (next + 1) % count;
                if (!s->panels[next].snapshot.disabled) {
                    services_->focus().defer_focus(s->panels[next].interaction, input::FocusModality::keyboard);
                    break;
                }
            }
            return true;
        }
    }
    return false;
}

void CollapseComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<CollapseState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().collapse();
        const auto& node = services_->nodes().require(s->node);
        const auto clip = graphics::EffectClip{
            1, services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip))};
        const float opacity = s->panels.empty() ? 0 : services_->nodes().content_opacity(s->node);
        const auto bg = fill(node.bounds, t.metrics.border_radius, {true, true, true, true}, t.colors.header_background,
                             opacity, node.translation, clip);
        static_cast<void>(services_->surfaces().update_content_effects(s->background, bg));
        const float border = std::min(t.metrics.border_width, std::min(node.bounds.width, node.bounds.height) / 2);
        const runtime::Rect inner{node.bounds.x + border, node.bounds.y + border,
                                  std::max(0.0F, node.bounds.width - border * 2),
                                  std::max(0.0F, node.bounds.height - border * 2)};
        const float radius =
            std::min(std::max(0.0F, t.metrics.border_radius - border), std::min(inner.width, inner.height) / 2);
        const auto outline =
            graphics::make_corner_outline_effects({inner, radius}, {true, true, true, true}, border, 0, t.colors.border,
                                                  border > 0 ? opacity : 0, node.translation, clip);
        static_cast<void>(services_->surfaces().update_content_effects(s->border, outline));
        for (std::size_t index = 0; index < s->panels.size(); ++index) {
            const auto& row = s->panels[index];
            const auto& header = services_->nodes().require(services_->components().root(row.snapshot.header));
            std::array<graphics::RoundedEffectInstance, 5> effects;
            const auto separator =
                fill({header.bounds.x, header.bounds.y, header.bounds.width, border}, 0, {false, false, false, false},
                     t.colors.border, index > 0 ? opacity : 0, header.translation, clip);
            std::copy(separator.begin(), separator.end(), effects.begin());
            const auto focus = services_->focus().state();
            const float inset = std::min(t.effects.focus_width + t.effects.focus_offset,
                                         std::min(header.bounds.width, header.bounds.height) / 2);
            const runtime::Rect focus_bounds{header.bounds.x + inset, header.bounds.y + inset,
                                             std::max(0.0F, header.bounds.width - inset * 2),
                                             std::max(0.0F, header.bounds.height - inset * 2)};
            effects[4] = graphics::make_outline_effect(
                {focus_bounds, 0}, std::max(1.0F, t.effects.focus_width), t.effects.focus_offset,
                t.colors.focus_outline,
                focus.focus_visible && focus.focused == row.interaction && t.effects.focus_width > 0 ? opacity : 0,
                header.translation, clip);
            static_cast<void>(services_->surfaces().update_content_effects(row.header_effects, effects));
            if (row.snapshot.expanded) {
                const auto& body = services_->nodes().require(services_->components().root(row.snapshot.body));
                std::array<graphics::RoundedEffectInstance, 8> body_effects;
                const bool last = index + 1 == s->panels.size();
                const auto background = fill(body.bounds, radius, {false, false, last, last}, t.colors.body_background,
                                             opacity, body.translation, clip);
                std::copy(background.begin(), background.end(), body_effects.begin());
                const auto line = fill({body.bounds.x, body.bounds.y, body.bounds.width, border}, 0,
                                       {false, false, false, false}, t.colors.border, opacity, body.translation, clip);
                std::copy(line.begin(), line.end(), body_effects.begin() + 4);
                static_cast<void>(services_->surfaces().update_content_effects(row.body_effects, body_effects));
            }
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Collapse(CollapseProps props, std::vector<CollapsePanel> panels) {
    if (!detail::active_collapse) {
        throw std::logic_error("Collapse requires window component services");
    }
    detail::active_collapse->mount(props, panels);
}
} // namespace ryn
