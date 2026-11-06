#include "component/collapse_component.hpp"
#include "input/pressable_behavior.hpp"
#include "component/data_display_retained.hpp"
#include "animation/material_transition_channels.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local CollapseComponentHost* active_collapse{};

class PanelMotion final : private animation::AnimationTargetSink {
public:
    PanelMotion(WindowComponentServices& services, runtime::ComponentId owner, float initial)
        : value(initial), services_(&services), owner_(owner),
          targets_(services.animations(), *this, {animation::AnimationValueKind::scalar},
                   animation::AnimationDirtyDomain::geometry | animation::AnimationDirtyDomain::animation) {}

    void to(float destination, bool enabled) {
        target = destination;
        const auto policy = animation::resolve_motion_policy(services_->components().theme_scope(owner_)->snapshot(),
                                                             services_->motion_preference());
        if (!enabled || !policy.enabled() || !services_->focus().state().window_active ||
            !services_->components().branch_active(owner_)) {
            if (services_->animations().contains(active_)) {
                static_cast<void>(services_->animations().cancel(active_, services_->animation_time()));
            }
            value = target;
            return;
        }
        animation::retarget_material_channel(
            services_->animations(), active_, targets_.target(0), value, target,
            policy.transition(animation::MotionDurationToken::mid, animation::MotionEasingToken::ease_in_out),
            services_->animation_time());
    }

    bool running() const {
        return services_->animations().contains(active_);
    }

    float value{};
    float target{};

private:
    void apply(animation::AnimationId, animation::AnimationTargetId, const animation::AnimationValue& v,
               animation::AnimationDirtyDomain) override {
        value = std::get<float>(v);
        if (services_->components().contains(owner_)) {
            services_->dirty().invalidate(services_->components().root(owner_),
                                          runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                              runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
        }
    }

    WindowComponentServices* services_;
    runtime::ComponentId owner_;
    animation::MaterialTransitionTargets<1> targets_;
    animation::AnimationId active_;
};

struct PanelState final {
    String key;
    std::optional<CollapsePanel> definition;
    std::shared_ptr<Scope> bindings;
    runtime::ComponentId body_content;
    std::shared_ptr<PanelMotion> motion;
    Signal<float> rotation{0};
    Signal<bool> active{false};
    CollapseCollapsible collapsible{CollapseCollapsible::Header};
    bool show_arrow{true};
    bool rendered{};
    float presented{-1};
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
    input::InteractionId anchor;
    std::vector<PanelState> panels;
    CollapseKeys keys;
    CollapseKeys topology;
    bool controlled{};
    bool bordered{true};
    bool ghost{};
    bool rtl{};
    bool visible{true};
    bool lazy{};
    bool destroy_hidden{};
    bool animated{true};
    bool updating{};
    bool refresh_pending{};
    bool pending_geometry{};
    std::optional<std::vector<CollapsePanel>> pending;
    CollapseCollapsible collapsible{CollapseCollapsible::Header};
    CollapseIconPlacement icon_placement{CollapseIconPlacement::Start};
    CollapseExpandIcon expand_icon;
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

using CollapseOwner = std::shared_ptr<CollapseState>;

CollapseOwner collapse_state(WindowComponentServices& services, runtime::ComponentId id) {
    const auto* slot = services.components().state<CollapseOwner>(id);
    return slot ? *slot : nullptr;
}

std::size_t panel_index(const CollapseState& s, const String& key) {
    const auto found = std::ranges::find(s.panels, key, &PanelState::key);
    return static_cast<std::size_t>(found - s.panels.begin());
}

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
        if (std::ranges::find(s.topology, key) != s.topology.end() && std::ranges::find(result, key) == result.end()) {
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
    const auto s = collapse_state(*services_, id);
    if (!s) {
        throw std::out_of_range("Collapse component is stale");
    }
    CollapseSnapshot result{normalized(*s), {}};
    for (const auto& row : s->panels) {
        auto panel = row.snapshot;
        panel.motion = row.motion ? row.motion->value : (row.snapshot.expanded ? 1.0F : 0.0F);
        panel.animating = row.motion && row.motion->running();
        result.panels.push_back(panel);
    }
    return result;
}

void CollapseComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    run_untracked([&] {
        auto s = collapse_state(*services_, id);
        if (!s || !services_->components().scope(id).active()) {
            return;
        }
        if (s->updating) {
            s->refresh_pending = true;
            s->pending_geometry |= geometry;
            return;
        }
        s->refresh_pending = false;
        s->pending_geometry = false;
        s->updating = true;
        struct ResetFlag final {
            bool& value;
            ~ResetFlag() {
                value = false;
            }
        };
        ResetFlag reset{s->updating};
        const auto live = [&] {
            return services_->components().contains(id) && services_->components().scope(id).active();
        };
        try {
            const auto t = services_->components().theme_scope(id)->snapshot().collapse();
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
            services_->components().set_branch_active(id, s->visible);
            services_->nodes().require(s->node).out_of_flow = !s->visible;
            for (auto& row : s->panels) {
                row.collapsible =
                    row.definition->collapsible_ ? read_prop(*row.definition->collapsible_) : s->collapsible;
                if (!live()) {
                    return;
                }
                row.show_arrow = read_prop(row.definition->show_arrow_);
                if (!live()) {
                    return;
                }
                row.foreground.set(foreground(row.snapshot.disabled ? t.colors.disabled_text : t.colors.header_text));
                if (services_->interactions().set_eligible(
                        row.interaction, !row.snapshot.disabled && row.collapsible != CollapseCollapsible::Disabled &&
                                             (row.collapsible != CollapseCollapsible::Icon || row.show_arrow))) {
                    services_->dirty().invalidate(services_->components().root(row.snapshot.header),
                                                  runtime::DirtyFlags::HitTest);
                }
                if (row.snapshot.disabled) {
                    static_cast<void>(row.press.reset());
                    disabled.push_back(row.interaction);
                }
                const bool expanded = std::ranges::find(active, row.key) != active.end();
                if (row.snapshot.expanded != expanded) {
                    row.snapshot.expanded = expanded;
                    row.motion->to(expanded ? 1.0F : 0.0F, s->animated && s->visible);
                    geometry = true;
                }
                services_->set_input_blocked(row.snapshot.body, !s->visible || !expanded);
                if (!live()) {
                    return;
                }
                row.presented = row.motion->value;
                row.active.set(expanded);
                if (!live()) {
                    return;
                }
                row.arrow.set(expanded ? IconName::DownOutlined : IconName::RightOutlined);
                row.rotation.set((s->rtl ? 180.0F - row.motion->value * 90.0F : row.motion->value * 90.0F) -
                                 (expanded ? 90.0F : 0.0F));
                services_->nodes().require(row.snapshot.arrow).opacity = row.show_arrow ? 1.0F : 0.0F;
                const bool painting = expanded || row.motion->value > 0;
                if (services_->components().set_branch_active(row.snapshot.body, painting)) {
                    services_->mark_scene_structure_dirty();
                    geometry = true;
                }
                if (!expanded) {
                    hidden.push_back(row.snapshot.body);
                }
                if (geometry) {
                    const float padding = sized(s->size, t.metrics.body_padding_small, t.metrics.body_padding,
                                                t.metrics.body_padding_large);
                    services_->layout().set_layout(services_->components().root(row.body_content),
                                                   layout::BoxLayout{{padding, padding, padding, padding}, true});
                }
            }
            services_->dirty().invalidate(s->node,
                                          geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                         runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                   : runtime::DirtyFlags::Material);
            // Cancellation may invoke callbacks that dispose this Collapse. Keep only value snapshots beyond this
            // point.
            const auto order = services_->interactions().declaration_order();
            const std::vector<input::InteractionId> interactions{order.begin(), order.end()};
            for (auto hit : interactions) {
                const auto* record = services_->interactions().find(hit);
                if (record &&
                    (std::ranges::find(disabled, hit) != disabled.end() || std::ranges::any_of(hidden, [&](auto body) {
                         return services_->components().contains(body) &&
                                descendant(*services_, record->component, body);
                     }))) {
                    services_->pointer().cancel_pointer_interaction(hit);
                }
            }
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
        s->updating = false;
        if (live() && s->pending) {
            auto pending = std::move(*s->pending);
            s->pending.reset();
            update(id, std::move(pending));
        } else if (live() && s->refresh_pending) {
            const bool pending_geometry = s->pending_geometry;
            s->refresh_pending = false;
            s->pending_geometry = false;
            refresh(id, pending_geometry);
        }
    });
}

void CollapseComponentHost::activate(runtime::ComponentId id, std::size_t index) {
    auto s = collapse_state(*services_, id);
    if (!s || index >= s->panels.size() || s->panels[index].snapshot.disabled ||
        s->panels[index].collapsible == CollapseCollapsible::Disabled ||
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

void CollapseComponentHost::mount_panel(runtime::ComponentId id, std::size_t index, bool initial) {
    auto owner = collapse_state(*services_, id);
    if (!owner) {
        return;
    }
    auto& s = *owner;
    auto& services = *services_;
    const auto content = Content{[&] {
        auto& context = runtime::require_component_build_context();
        const auto region = [&](runtime::ComponentId parent, const Content& content,
                                const Signal<runtime::SemanticForeground>& color,
                                const Signal<runtime::SemanticTypography>& font, bool shown = true) {
            runtime::ComponentId result;
            context.mount_slot(
                parent, Content{[&] {
                    auto& children = runtime::require_component_build_context();
                    result = children.mount_component<int>(0);
                    services.components().set_branch_active(result, shown);
                    const auto node = children.root(result);
                    services.layout().set_layout(node, layout::BoxLayout{});
                    children.on_resource_cleanup(result, [&services, node] { services.layout().remove_layout(node); });
                    children.mount_slot_with_semantic_text_style(result, content,
                                                                 Prop<runtime::SemanticForeground>{color},
                                                                 Prop<runtime::SemanticTypography>{font});
                }});
            return result;
        };
        const auto panel = *s.panels[index].definition;
        auto& row = s.panels[index];
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
        const auto hit = context.register_scene_fragment(header, runtime::SceneFragmentPlacement::before_children);
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
        handlers.target = [this, id, key = panel.key_](input::PointerDispatchContext& event) {
            auto s = collapse_state(*services_, id);
            if (!s) {
                return;
            }
            const auto index = panel_index(*s, key);
            if (index >= s->panels.size()) {
                return;
            }
            auto& row = s->panels[index];
            const bool allowed = !row.snapshot.disabled && row.collapsible != CollapseCollapsible::Disabled &&
                                 services_->interactions().component_active(row.snapshot.header);
            const auto& arrow_node = services_->nodes().require(row.snapshot.arrow);
            const auto r = arrow_node.bounds;
            const bool icon_hit = event.event().x >= r.x + arrow_node.translation.x &&
                                  event.event().x < r.x + arrow_node.translation.x + r.width &&
                                  event.event().y >= r.y + arrow_node.translation.y &&
                                  event.event().y < r.y + arrow_node.translation.y + r.height;
            const auto result = row.press.dispatch(
                event, row.interaction,
                allowed && (row.collapsible != CollapseCollapsible::Icon || (row.show_arrow && icon_hit)));
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
            if (const auto s = collapse_state(*services_, id)) {
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
            }
        };
        services.interactions().set_focus_handlers(row.interaction, std::move(focus));
        if (s.expand_icon) {
            const auto icon_region = region(header, Content{[&] {
                                                SlotContentAccess::function(s.expand_icon(
                                                    {row.key, bind([active = row.active] { return active.get(); })}))();
                                            }},
                                            row.foreground, s.arrow_font);
            row.snapshot.arrow = services.components().root(icon_region);
        } else {
            context.mount_slot_with_semantic_text_style(
                header, Content{[&] { Icon(IconProps{}.name(row.arrow).rotate(row.rotation)); }},
                Prop<runtime::SemanticForeground>{row.foreground}, Prop<runtime::SemanticTypography>{s.arrow_font});
            row.snapshot.arrow = services.nodes().require(node).children.back();
        }
        row.snapshot.title = region(header, Content{[title = panel.title_] { SlotContentAccess::function(title)(); }},
                                    row.foreground, s.header_font);
        // Keep native extra controls outside the disabled header interaction ancestry.
        auto extra_parent = id;
        {
            row.extra_viewport = context.mount_component<int>(0);
            extra_parent = row.extra_viewport;
            const auto viewport_node = context.root(row.extra_viewport);
            services.nodes().require(viewport_node).clip_content = true;
            services.layout().set_layout(viewport_node, layout::LeafLayout{});
            context.on_resource_cleanup(row.extra_viewport,
                                        [&services, viewport_node] { services.layout().remove_layout(viewport_node); });
        }
        row.snapshot.extra = region(extra_parent, Content{[extra = panel.extra_] {
                                        if (extra) {
                                            SlotContentAccess::function (*extra)();
                                        }
                                    }},
                                    row.foreground, s.header_font);
        row.snapshot.body = region(id, Content{[] {}}, s.body_color, s.body_font, true);
        const auto body = row.snapshot.body;
        context.on_resource_cleanup(body,
                                    [&services, body] { services.interactions().set_input_blocked(body, false); });
        services.layout().set_layout(context.root(row.snapshot.body), layout::LeafLayout{});
        row.body_content = region(row.snapshot.body, Content{[] {}}, s.body_color, s.body_font);
        const auto active_keys = normalized(s);
        const bool expanded = std::ranges::find(active_keys, row.key) != active_keys.end();
        row.snapshot.expanded = expanded;
        row.motion = std::make_shared<PanelMotion>(services, id, expanded ? 1.0F : 0.0F);
        const bool render =
            expanded || panel.force_render_ || (!s.lazy && !panel.destroy_hidden_.value_or(s.destroy_hidden));
        services.components().set_branch_active(row.snapshot.body, expanded);
        if (render) {
            context.mount_slot_with_semantic_text_style(
                row.body_content, Content{[body = panel.body_] { SlotContentAccess::function(body)(); }},
                Prop<runtime::SemanticForeground>{s.body_color}, Prop<runtime::SemanticTypography>{s.body_font});
            if (!services.components().contains(id)) {
                throw std::logic_error("Collapse destroyed in body factory");
            }
            row.rendered = true;
        }
        const auto body_effects =
            context.register_scene_fragment(row.snapshot.body, runtime::SceneFragmentPlacement::before_children);
        row.body_effects = services.surfaces().create_content_range(body_effects, {});
        context.on_resource_cleanup(row.snapshot.body, [&services, effects = row.body_effects] {
            services.surfaces().destroy_content_range(effects);
        });
    }};
    display_append(services, id, id, content, initial);
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
    const auto id = build.mount_component<CollapseOwner>(std::make_shared<CollapseState>());
    auto owner = build.state<CollapseOwner>(id);
    auto& s = *owner;
    s.node = build.root(id);
    s.anchor = panels.empty() ? input::InteractionId{} : display_anchor(services, id);
    services.nodes().require(s.node).clip_content = true;
    s.topology = keys;
    s.controlled = props.active_keys_.has_value();
    s.keys = props.active_keys_ ? read_prop(*props.active_keys_) : props.default_keys_.value_or(CollapseKeys{});
    s.accordion = read_prop(props.accordion_);
    s.size = read_prop(props.size_);
    s.on_change = props.on_change_;
    s.bordered = read_prop(props.bordered_);
    s.ghost = read_prop(props.ghost_);
    s.rtl = read_prop(props.rtl_);
    s.visible = read_prop(props.visible_);
    s.lazy = read_prop(props.lazy_);
    s.destroy_hidden = read_prop(props.destroy_hidden_);
    s.animated = read_prop(props.animated_);
    s.collapsible = read_prop(props.collapsible_);
    s.icon_placement = read_prop(props.icon_placement_);
    s.expand_icon = props.expand_icon_;
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
    build.on_resource_cleanup(id, [weak = std::weak_ptr<CollapseState>{owner}] {
        if (auto s = weak.lock()) {
            for (auto& row : s->panels) {
                row.bindings.reset();
                row.motion.reset();
            }
        }
    });
    s.updating = true;
    for (const auto& panel : panels) {
        s.panels.emplace_back();
        s.panels.back().key = panel.key_;
        s.panels.back().definition = panel;
        mount_panel(id, s.panels.size() - 1, true);
    }
    s.updating = false;
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
            if (auto s = collapse_state(*services_, id)) {
                s->keys = std::move(keys);
                refresh(id, false);
            }
        });
    }
    connect_prop(scope, props.accordion_, [this, id](bool value) {
        if (auto s = collapse_state(*services_, id)) {
            s->accordion = value;
            if (!s->controlled) {
                s->keys = normalized(*s);
            }
            refresh(id, false);
        }
    });
    connect_prop(scope, props.size_, [this, id](ControlSize value) {
        validate(value);
        if (auto s = collapse_state(*services_, id)) {
            s->size = value;
            refresh(id, true);
        }
    });
    for (std::size_t index = 0; index < panels.size(); ++index) {
        bind_panel(id, index);
    }
    connect_prop(scope, props.bordered_, [this, id](bool value) {
        if (auto s = collapse_state(*services_, id)) {
            s->bordered = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.ghost_, [this, id](bool value) {
        if (auto s = collapse_state(*services_, id)) {
            s->ghost = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.rtl_, [this, id](bool value) {
        if (auto s = collapse_state(*services_, id)) {
            s->rtl = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.visible_, [this, id](bool value) {
        if (auto s = collapse_state(*services_, id)) {
            s->visible = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.lazy_, [this, id](bool value) {
        if (auto s = collapse_state(*services_, id)) {
            s->lazy = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.destroy_hidden_, [this, id](bool value) {
        if (auto s = collapse_state(*services_, id)) {
            s->destroy_hidden = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.animated_, [this, id](bool value) {
        if (auto s = collapse_state(*services_, id)) {
            s->animated = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.collapsible_, [this, id](CollapseCollapsible value) {
        if (auto s = collapse_state(*services_, id)) {
            if (value < CollapseCollapsible::Header || value > CollapseCollapsible::Disabled) {
                throw std::invalid_argument("Invalid Collapse collapsible");
            }
            s->collapsible = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.icon_placement_, [this, id](CollapseIconPlacement value) {
        if (auto s = collapse_state(*services_, id)) {
            if (value < CollapseIconPlacement::Start || value > CollapseIconPlacement::End) {
                throw std::invalid_argument("Invalid Collapse icon placement");
            }
            s->icon_placement = value;
            refresh(id, true);
        }
    });
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

void CollapseComponentHost::bind_panel(runtime::ComponentId id, std::size_t index) {
    auto s = collapse_state(*services_, id);
    if (!s) {
        return;
    }
    auto& row = s->panels[index];
    row.bindings = std::make_shared<Scope>();
    const auto bindings = row.bindings;
    const auto panel = *row.definition;
    connect_prop(*bindings, panel.disabled_, [this, id, key = panel.key_](bool value) {
        if (auto s = collapse_state(*services_, id)) {
            const auto i = panel_index(*s, key);
            if (i < s->panels.size()) {
                s->panels[i].snapshot.disabled = value;
                refresh(id, false);
            }
        }
    });
    connect_prop(*bindings, panel.show_arrow_, [this, id](bool) { refresh(id, true); });
    if (panel.collapsible_) {
        connect_prop(*bindings, *panel.collapsible_, [this, id](CollapseCollapsible value) {
            if (value < CollapseCollapsible::Header || value > CollapseCollapsible::Disabled) {
                throw std::invalid_argument("Invalid Collapse collapsible");
            }
            refresh(id, false);
        });
    }
}

void CollapseComponentHost::update(runtime::ComponentId id, std::vector<CollapsePanel> panels) {
    run_untracked([&] {
        CollapseKeys keys;
        for (const auto& panel : panels) {
            if (panel.key_.empty() || std::ranges::find(keys, panel.key_) != keys.end()) {
                throw std::invalid_argument("Collapse panel keys must be non-empty and unique");
            }
            keys.push_back(panel.key_);
            static_cast<void>(read_prop(panel.disabled_));
            static_cast<void>(read_prop(panel.show_arrow_));
            if (panel.collapsible_) {
                const auto c = read_prop(*panel.collapsible_);
                if (c < CollapseCollapsible::Header || c > CollapseCollapsible::Disabled) {
                    throw std::invalid_argument("Invalid Collapse collapsible");
                }
            }
        }
        auto s = collapse_state(*services_, id);
        if (!s) {
            return;
        }
        if (s->updating) {
            s->pending = std::move(panels);
            return;
        }
        s->updating = true;
        try {
            if (!s->anchor.valid() && !panels.empty()) {
                display_append(*services_, id, id, Content{[&] { s->anchor = display_anchor(*services_, id); }}, false);
            }
            s->topology = keys;
            auto old = std::move(s->panels);
            s->panels.clear();
            s->panels.reserve(panels.size());
            for (const auto& panel : panels) {
                const auto found = std::ranges::find(old, panel.key_, &PanelState::key);
                if (found != old.end()) {
                    s->panels.push_back(std::move(*found));
                    auto& row = s->panels.back();
                    row.bindings.reset();
                    const auto previous = *row.definition;
                    row.definition = panel;
                    const auto replace = [&](runtime::ComponentId region, const Content& content) {
                        display_clear(*services_, id, region);
                        if (services_->components().contains(id)) {
                            display_append_styled(*services_, id, region, content, row.foreground, s->header_font,
                                                  false);
                        }
                    };
                    if (previous.title_revision_ != panel.title_revision_) {
                        replace(row.snapshot.title,
                                Content{[slot = panel.title_] { SlotContentAccess::function(slot)(); }});
                    }
                    if (!services_->components().contains(id)) {
                        return;
                    }
                    if (previous.extra_revision_ != panel.extra_revision_ ||
                        previous.extra_.has_value() != panel.extra_.has_value()) {
                        replace(row.snapshot.extra, Content{[slot = panel.extra_] {
                                    if (slot) {
                                        SlotContentAccess::function (*slot)();
                                    }
                                }});
                    }
                    if (!services_->components().contains(id)) {
                        return;
                    }
                    row.has_extra = panel.extra_.has_value();
                    if (previous.body_revision_ != panel.body_revision_) {
                        display_clear(*services_, id, row.body_content);
                        row.rendered = false;
                    }
                } else {
                    s->panels.emplace_back();
                    s->panels.back().key = panel.key_;
                    s->panels.back().definition = panel;
                    mount_panel(id, s->panels.size() - 1, false);
                }
                if (!services_->components().contains(id)) {
                    return;
                }
            }
            for (auto& row : old) {
                if (std::ranges::any_of(
                        s->panels, [&](const auto& kept) { return kept.snapshot.header == row.snapshot.header; })) {
                    continue;
                }
                row.bindings.reset();
                row.motion.reset();
                for (auto component : {row.snapshot.header, row.extra_viewport, row.snapshot.body}) {
                    if (services_->components().contains(component)) {
                        services_->destroy(component);
                    }
                    if (!services_->components().contains(id)) {
                        return;
                    }
                }
            }
            for (std::size_t i = 0; i < s->panels.size(); ++i) {
                bind_panel(id, i);
            }
            std::vector<runtime::ComponentId> regions;
            for (const auto& row : s->panels) {
                regions.insert(regions.end(), {row.snapshot.header, row.extra_viewport, row.snapshot.body});
            }
            display_reorder(*services_, s->anchor, regions);
            s->updating = false;
            refresh(id, true);
            prepare_auxiliary_layout();
            if (s->pending && services_->components().contains(id)) {
                auto pending = std::move(*s->pending);
                s->pending.reset();
                update(id, std::move(pending));
            }
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
    });
}

void CollapseComponentHost::mount(const CollapseProps& props, const Prop<std::vector<CollapsePanel>>& panels) {
    mount(props, read_prop(panels));
    const auto id = mounted_.back();
    connect_prop(
        services_->components().scope(id), panels,
        [this, id](std::vector<CollapsePanel> value) { update(id, std::move(value)); },
        [](const auto&, const auto&) { return false; });
}

void CollapseComponentHost::prepare_auxiliary_layout() {
    const auto mounted = mounted_;
    for (auto id : mounted) {
        auto s = collapse_state(*services_, id);
        if (!s || s->updating) {
            continue;
        }
        s->updating = true;
        bool changed = false;
        try {
            for (auto& row : s->panels) {
                if (!services_->components().contains(id)) {
                    break;
                }
                if (!s->visible || !services_->components().branch_active(id) ||
                    !services_->focus().state().window_active ||
                    !animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                                      services_->motion_preference())
                         .enabled() ||
                    !s->animated) {
                    row.motion->to(row.snapshot.expanded ? 1.0F : 0.0F, false);
                }
                changed |= row.presented != row.motion->value;
                const bool render = row.snapshot.expanded || row.definition->force_render_ ||
                                    (!s->lazy && !row.definition->destroy_hidden_.value_or(s->destroy_hidden));
                if (render && !row.rendered) {
                    const auto body = row.definition->body_;
                    display_append_styled(*services_, id, row.body_content,
                                          Content{[body] { SlotContentAccess::function(body)(); }}, s->body_color,
                                          s->body_font, false);
                    row.rendered = true;
                    changed = true;
                }
                if (!row.snapshot.expanded && !row.motion->running() &&
                    row.definition->destroy_hidden_.value_or(s->destroy_hidden) && !row.definition->force_render_ &&
                    row.rendered) {
                    display_clear(*services_, id, row.body_content);
                    row.rendered = false;
                    changed = true;
                }
            }
            if (!services_->components().contains(id)) {
                continue;
            }
            s->updating = false;
            if (changed) {
                refresh(id, true);
            }
            if (s->pending) {
                auto pending = std::move(*s->pending);
                s->pending.reset();
                update(id, std::move(pending));
            }
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
    }
}

void CollapseComponentHost::synchronize_auxiliary_motion() {
    prepare_auxiliary_layout();
}

void CollapseComponentHost::on_window_active(bool) {
    prepare_auxiliary_layout();
}

void CollapseComponentHost::on_input_boundary_changed() {
    prepare_auxiliary_layout();
}

runtime::Size CollapseComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                             layout::Constraints limits) {
    auto& s = *collapse_state(*services_, id);
    const auto& t = services_->components().theme_scope(id)->snapshot().collapse();
    const auto& m = t.metrics;
    const float border = (s.bordered && !s.ghost ? std::min(m.border_width, limits.max_width / 2) : 0);
    const float width = std::max(0.0F, limits.max_width - border * 2);
    const float start = std::min(sized(s.size, m.header_start_small, m.header_start, m.header_start_large), width);
    const float end = std::min(sized(s.size, m.header_end_small, m.header_end, m.header_end_large), width - start);
    const float block = sized(s.size, m.header_block_small, m.header_block, m.header_block_large);
    const float available = std::max(0.0F, width - start - end);
    float height = border * 2;
    float natural_width = 0;
    for (auto& row : s.panels) {
        const float arrow = row.show_arrow ? std::min(m.arrow_size, available) : 0;
        const float arrow_gap = arrow > 0 ? std::min(m.header_gap, std::max(0.0F, available - arrow)) : 0;
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
        row.body_size = (row.snapshot.expanded || row.motion->value > 0) && row.rendered
                            ? engine.measure_child(services_->components().root(row.body_content),
                                                   {0, width, 0, std::numeric_limits<float>::infinity()})
                            : runtime::Size{};
        if (row.snapshot.expanded || row.motion->value > 0) {
            static_cast<void>(
                engine.measure_child(services_->components().root(row.snapshot.body),
                                     layout::Constraints::fixed(width, row.body_size.height * row.motion->value)));
        }
        natural_width = std::max(natural_width, std::max(start + end + arrow + arrow_gap + row.title_size.width +
                                                             row.extra_size.width + extra_gap,
                                                         row.body_size.width));
        height += row.header_size.height + row.body_size.height * row.motion->value;
    }
    return limits.constrain({std::isfinite(limits.max_width) ? limits.max_width : natural_width + border * 2, height});
}

void CollapseComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto& s = *collapse_state(*services_, id);
    const auto& m = services_->components().theme_scope(id)->snapshot().collapse().metrics;
    const float border =
        (s.bordered && !s.ghost ? std::min(m.border_width, std::min(bounds.width, bounds.height) / 2) : 0);
    const float width = std::max(0.0F, bounds.width - border * 2);
    const float start = std::min(sized(s.size, m.header_start_small, m.header_start, m.header_start_large), width);
    const float end = std::min(sized(s.size, m.header_end_small, m.header_end, m.header_end_large), width - start);
    const float available = std::max(0.0F, width - start - end);
    float y = bounds.y + border;
    const float bottom = bounds.y + bounds.height - border;
    for (auto& row : s.panels) {
        const float arrow = row.show_arrow ? std::min(m.arrow_size, available) : 0;
        const float arrow_gap = arrow > 0 ? std::min(m.header_gap, std::max(0.0F, available - arrow)) : 0;
        const float height = std::min(row.header_size.height, std::max(0.0F, bottom - y));
        const float arrow_size = std::min(arrow, height);
        const float title_height = std::min(height, row.title_size.height);
        const float extra_height = std::min(height, row.extra_size.height);
        const float x = bounds.x + border;
        const auto mirror = [&](float local, float extent) {
            return s.rtl ? x + width - local - extent : x + local;
        };
        const bool icon_end = s.icon_placement == CollapseIconPlacement::End;
        engine.place_child(services_->components().root(row.snapshot.header), {x, y, width, height});
        engine.place_child(row.snapshot.arrow, {mirror(icon_end ? width - end - arrow + (arrow - arrow_size) / 2
                                                                : start + (arrow - arrow_size) / 2,
                                                       arrow_size),
                                                y + (height - arrow_size) / 2, arrow_size, arrow_size});
        const float content = std::max(0.0F, available - arrow - arrow_gap);
        const float extra = std::min(content, row.extra_size.width);
        const float extra_gap = row.has_extra ? std::min(m.header_gap, content - extra) : 0;
        engine.place_child(
            services_->components().root(row.snapshot.title),
            {mirror(start + (icon_end ? 0 : arrow + arrow_gap), std::max(0.0F, content - extra - extra_gap)),
             y + (height - title_height) / 2, std::max(0.0F, content - extra - extra_gap), title_height});
        if (row.has_extra) {
            // Clip to the available header while retaining space around native focus outlines.
            engine.place_child(services_->components().root(row.extra_viewport), {x, y, width, height});
            engine.place_child(services_->components().root(row.snapshot.extra),
                               {mirror(width - end - extra - (icon_end ? arrow + arrow_gap : 0), extra),
                                y + (height - extra_height) / 2, extra, extra_height});
        }
        y += height;
        if (row.snapshot.expanded || row.motion->value > 0) {
            const float body_height = std::min(row.body_size.height * row.motion->value, std::max(0.0F, bottom - y));
            engine.place_child(services_->components().root(row.snapshot.body), {x, y, width, body_height});
            engine.place_child(services_->components().root(row.body_content), {x, y, width, row.body_size.height});
            y += body_height;
        }
    }
}

bool CollapseComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    for (auto id : mounted_) {
        const auto s = collapse_state(*services_, id);
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
        const auto s = collapse_state(*services_, id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().collapse();
        const auto& node = services_->nodes().require(s->node);
        const auto clip = graphics::EffectClip{
            1, services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip))};
        const float opacity = s->panels.empty() ? 0 : services_->nodes().content_opacity(s->node);
        const auto bg = fill(node.bounds, t.metrics.border_radius, {true, true, true, true}, t.colors.header_background,
                             s->ghost ? 0 : opacity, node.translation, clip);
        static_cast<void>(services_->surfaces().update_content_effects(s->background, bg));
        const float border = s->bordered && !s->ghost
                                 ? std::min(t.metrics.border_width, std::min(node.bounds.width, node.bounds.height) / 2)
                                 : 0;
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
            if (row.snapshot.expanded || row.motion->value > 0) {
                const auto& body = services_->nodes().require(services_->components().root(row.snapshot.body));
                std::array<graphics::RoundedEffectInstance, 8> body_effects;
                const bool last = index + 1 == s->panels.size();
                const auto background = fill(body.bounds, radius, {false, false, last, last}, t.colors.body_background,
                                             s->ghost ? 0 : opacity, body.translation, clip);
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
void Collapse(CollapseProps props, Prop<std::vector<CollapsePanel>> panels) {
    if (!detail::active_collapse) {
        throw std::logic_error("Collapse requires window component services");
    }
    detail::active_collapse->mount(props, panels);
}

void Collapse(CollapseProps props, std::vector<CollapsePanel> panels) {
    if (!detail::active_collapse) {
        throw std::logic_error("Collapse requires window component services");
    }
    detail::active_collapse->mount(props, panels);
}
} // namespace ryn
