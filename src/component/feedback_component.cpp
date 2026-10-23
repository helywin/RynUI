#include "component/feedback_component.hpp"
#include "input/pressable_behavior.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/icon.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace ryn::detail {
namespace {
thread_local FeedbackComponentHost* active_feedback{};
constexpr std::size_t arc_points = 192;
constexpr std::size_t progress_layers = arc_points + 2;

struct AlertState final {
    runtime::NodeId node;
    AlertSnapshot snapshot;
    bool show_icon{};
    bool description_slot{};
    bool action_slot{};
    bool hovered{};
    input::PressableBehavior press;
    input::FocusPresentation focus;
    std::function<void()> on_close;
    Signal<String> message{String{}};
    Signal<String> description{String{}};
    Signal<IconName> icon{IconName::InfoCircleFilled};
    Signal<runtime::SemanticForeground> message_color{runtime::SemanticForeground{}};
    Signal<runtime::SemanticForeground> description_color{runtime::SemanticForeground{}};
    Signal<runtime::SemanticForeground> icon_color{runtime::SemanticForeground{}};
    Signal<runtime::SemanticForeground> close_color{runtime::SemanticForeground{}};
    Signal<runtime::SemanticTypography> message_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> body_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> icon_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> close_font{runtime::SemanticTypography{}};
    component::RetainedSurfaceId surface;
    component::RetainedSurfaceId border;
    std::array<runtime::Size, 4> sizes;
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

struct SpinState final {
    runtime::NodeId node;
    SpinSnapshot snapshot;
    ControlSize size{ControlSize::Middle};
    Signal<String> description{String{}};
    Signal<bool> animate{false};
    Signal<runtime::SemanticForeground> color{runtime::SemanticForeground{}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> icon_font{runtime::SemanticTypography{}};
    component::RetainedSurfaceId mask;
    std::array<runtime::Size, 3> sizes;
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

struct ProgressState final {
    runtime::NodeId node;
    ProgressSnapshot snapshot;
    ProgressStatus requested_status{ProgressStatus::Normal};
    ControlSize size{ControlSize::Middle};
    bool show_info{true};
    Signal<String> label{String{}};
    Signal<runtime::SemanticForeground> color{runtime::SemanticForeground{}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    component::RetainedSurfaceId surface;
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

void validate(ControlSize size) {
    if (size != ControlSize::Small && size != ControlSize::Middle && size != ControlSize::Large) {
        throw std::invalid_argument("Invalid feedback size");
    }
}

void validate(Status status) {
    if (status < Status::None || status > Status::Warning) {
        throw std::invalid_argument("Invalid Alert status");
    }
}

void validate(ProgressType type) {
    if (type < ProgressType::Line || type > ProgressType::Dashboard) {
        throw std::invalid_argument("Invalid Progress type");
    }
}

void validate(ProgressStatus status) {
    if (status < ProgressStatus::Normal || status > ProgressStatus::Exception) {
        throw std::invalid_argument("Invalid Progress status");
    }
}

float percent(float value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("Progress percent must be finite");
    }
    return std::clamp(value, 0.0F, 100.0F);
}

float sized(ControlSize size, float small, float middle, float large) {
    return size == ControlSize::Small ? small : size == ControlSize::Large ? large : middle;
}

void invalidate(WindowComponentServices& services, runtime::NodeId node, bool geometry) {
    services.dirty().invalidate(node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                     runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                               : runtime::DirtyFlags::Material);
}

void active(WindowComponentServices& services, runtime::ComponentId id, bool visible) {
    if (services.components().set_branch_active(id, visible)) {
        services.mark_scene_structure_dirty();
    }
}

runtime::ComponentId region(WindowComponentServices& services, runtime::ComponentId parent, const Content& content,
                            const Signal<runtime::SemanticForeground>& color,
                            const Signal<runtime::SemanticTypography>& font) {
    auto& build = runtime::require_component_build_context();
    runtime::ComponentId id;
    build.mount_slot(parent, Content{[&] {
                         auto& context = runtime::require_component_build_context();
                         id = context.mount_component<int>(0);
                         const auto node = context.root(id);
                         services.layout().set_layout(node, layout::BoxLayout{});
                         context.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
                         context.mount_slot_with_semantic_text_style(id, content,
                                                                     Prop<runtime::SemanticForeground>{color},
                                                                     Prop<runtime::SemanticTypography>{font});
                     }});
    return id;
}

component::RetainedSurfaceId surface(WindowComponentServices& services, runtime::ComponentId id,
                                     std::optional<input::InteractionId> hit = {}, std::size_t count = 1) {
    auto& build = runtime::require_component_build_context();
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    std::vector<graphics::QuadInstance> quads(count);
    for (auto& q : quads) {
        q.opacity = 0;
    }
    const auto result = services.surfaces().create(id, build.root(id), fragment, hit, quads);
    build.on_resource_cleanup(id, [&services, result] { services.surfaces().destroy(result); });
    return result;
}

graphics::QuadInstance quad(const runtime::Node& node, runtime::Rect bounds, Color color, float radius,
                            float opacity = 1) {
    graphics::QuadInstance q;
    q.bounds = {bounds.x, bounds.y, std::max(0.0F, bounds.width), std::max(0.0F, bounds.height)};
    q.color = {color.red(), color.green(), color.blue(), color.alpha()};
    q.corner_radius = std::min(radius, std::min(q.bounds[2], q.bounds[3]) / 2);
    q.opacity = node.opacity * opacity;
    q.translation = {node.translation.x, node.translation.y};
    return q;
}

runtime::Rect bounds(WindowComponentServices& services, runtime::ComponentId id) {
    return services.nodes().require(services.components().root(id)).bounds;
}

bool intersects(runtime::Rect a, runtime::Rect b) {
    return a.width > 0 && a.height > 0 && b.width > 0 && b.height > 0 && a.x < b.x + b.width && a.x + a.width > b.x &&
           a.y < b.y + b.height && a.y + a.height > b.y;
}
} // namespace

FeedbackComponentHost::FeedbackComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

FeedbackComponentHost::~FeedbackComponentHost() {
    services_->detach(*this);
}

void* FeedbackComponentHost::begin_mount() noexcept {
    auto* previous = active_feedback;
    active_feedback = this;
    return previous;
}

void FeedbackComponentHost::end_mount(void* previous) noexcept {
    active_feedback = static_cast<FeedbackComponentHost*>(previous);
}

void FeedbackComponentHost::on_destroy() noexcept {
    for (auto* list : {&alerts_, &spins_, &progress_}) {
        std::erase_if(*list, [this](auto id) { return !services_->components().contains(id); });
    }
}

void FeedbackComponentHost::on_dispose() noexcept {
    alerts_.clear();
    spins_.clear();
    progress_.clear();
}

AlertSnapshot FeedbackComponentHost::alert_snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<AlertState>(id);
    if (!s) {
        throw std::out_of_range("Stale Alert");
    }
    return s->snapshot;
}

SpinSnapshot FeedbackComponentHost::spin_snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<SpinState>(id);
    if (!s) {
        throw std::out_of_range("Stale Spin");
    }
    return s->snapshot;
}

ProgressSnapshot FeedbackComponentHost::progress_snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<ProgressState>(id);
    if (!s) {
        throw std::out_of_range("Stale Progress");
    }
    return s->snapshot;
}

void FeedbackComponentHost::close_alert(runtime::ComponentId id) {
    auto* s = services_->components().state<AlertState>(id);
    if (!s || !s->snapshot.visible || !s->snapshot.closable ||
        !services_->interactions().component_active(s->snapshot.close)) {
        return;
    }
    const auto callback = s->on_close;
    if (callback) {
        callback();
    }
}

void FeedbackComponentHost::refresh_alert(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<AlertState>(id);
    if (!s) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().alert();
    const auto& m = t.metrics;
    s->snapshot.has_description = s->description_slot || !s->description.get().empty();
    s->snapshot.palette = t.colors.statuses[static_cast<std::size_t>(s->snapshot.status)];
    const auto status = s->snapshot.status;
    s->icon.set(status == Status::Success   ? IconName::CheckCircleFilled
                : status == Status::Error   ? IconName::CloseCircleFilled
                : status == Status::Warning ? IconName::ExclamationCircleFilled
                                            : IconName::InfoCircleFilled);
    s->message_color.set(foreground(t.colors.message));
    s->description_color.set(foreground(t.colors.description));
    s->icon_color.set(foreground(s->snapshot.palette.icon));
    s->close_color.set(foreground(s->hovered || s->focus.focus_visible ? t.colors.close_hover : t.colors.close));
    const bool description = s->snapshot.has_description;
    s->message_font.set({m.font_family, 400, false, description ? m.message_font_size : m.font_size,
                         description ? m.message_line_height : m.line_height});
    s->body_font.set({m.font_family, 400, false, m.font_size, m.line_height});
    const auto icon_size = description ? m.description_icon_size : m.icon_size;
    s->icon_font.set({m.font_family, 400, false, icon_size, icon_size});
    s->close_font.set({m.font_family, 400, false, m.close_size, m.close_size});
    services_->nodes().require(s->node).out_of_flow = !s->snapshot.visible;
    active(*services_, id, s->snapshot.visible);
    active(*services_, s->snapshot.icon, s->show_icon);
    active(*services_, s->snapshot.description, description);
    active(*services_, s->snapshot.close, s->snapshot.closable);
    const auto hit = s->snapshot.close_interaction;
    if (services_->interactions().set_eligible(hit, s->snapshot.visible && s->snapshot.closable)) {
        services_->mark_scene_structure_dirty();
    }
    if (!s->snapshot.visible || !s->snapshot.closable) {
        services_->pointer().cancel_interaction(hit);
        services_->focus().cancel_interaction(hit);
        s->hovered = false;
        static_cast<void>(s->press.reset());
    }
    if (geometry) {
        services_->layout().set_layout(
            services_->components().root(s->snapshot.body),
            layout::FlexLayout{layout::FlexDirection::vertical, description ? m.description_gap : 0});
    }
    invalidate(*services_, s->node, geometry);
}

void FeedbackComponentHost::mount(const AlertProps& props, const AlertSlots& slots) {
    validate(read_prop(props.status_));
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<AlertState>();
    auto& s = build.state<AlertState>(id);
    s.node = build.root(id);
    s.snapshot.status = read_prop(props.status_);
    s.snapshot.visible = read_prop(props.visible_);
    s.snapshot.closable = read_prop(props.closable_);
    s.message.set(read_prop(props.message_));
    s.description.set(read_prop(props.description_));
    s.show_icon = read_prop(props.show_icon_);
    s.description_slot = slots.description.has_value();
    s.action_slot = slots.action.has_value();
    s.on_close = props.on_close_;
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    s.surface = surface(services, id);
    const auto border_fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.border = services.surfaces().create_content_range(border_fragment, {});
    build.on_resource_cleanup(id, [&services, border = s.border, node = s.node] {
        services.surfaces().destroy_content_range(border);
        services.layout().remove_layout(node);
    });
    s.snapshot.icon = region(services, id, Content{[slot = slots.icon, icon = s.icon] {
                                 if (slot) {
                                     SlotContentAccess::function (*slot)();
                                 } else {
                                     Icon(IconProps{}.name(icon));
                                 }
                             }},
                             s.icon_color, s.icon_font);
    s.snapshot.body = region(services, id, Content{[] {}}, s.message_color, s.body_font);
    s.snapshot.message = region(services, s.snapshot.body, Content{[slot = slots.message, text = s.message] {
                                    if (slot) {
                                        SlotContentAccess::function (*slot)();
                                    } else {
                                        Text(TextProps{}.content(text));
                                    }
                                }},
                                s.message_color, s.message_font);
    s.snapshot.description =
        region(services, s.snapshot.body, Content{[slot = slots.description, text = s.description] {
                   if (slot) {
                       SlotContentAccess::function (*slot)();
                   } else {
                       Text(TextProps{}.content(text));
                   }
               }},
               s.description_color, s.body_font);
    s.snapshot.action = region(services, id, Content{[slot = slots.action] {
                                   if (slot) {
                                       SlotContentAccess::function (*slot)();
                                   }
                               }},
                               s.description_color, s.body_font);
    s.snapshot.close = region(services, id, Content{[slot = slots.close] {
                                  if (slot) {
                                      SlotContentAccess::function (*slot)();
                                  } else {
                                      Icon(IconProps{}.name(IconName::CloseOutlined));
                                  }
                              }},
                              s.close_color, s.close_font);
    const auto close_node = services.components().root(s.snapshot.close);
    s.snapshot.close_interaction =
        services.interactions().create({s.snapshot.close, close_node, {}, true, true, {}, true, true});
    const auto hit = s.snapshot.close_interaction;
    const auto hit_fragment =
        build.register_scene_fragment(s.snapshot.close, runtime::SceneFragmentPlacement::before_children);
    services.scene_composer().set_fragment(hit_fragment, {}, hit);
    build.on_resource_cleanup(id, [&services, hit] {
        services.pointer().cancel_interaction(hit);
        services.focus().cancel_interaction(hit);
        services.interactions().remove(hit);
    });
    input::InteractionHandlers pointer;
    pointer.target = [this, id](input::PointerDispatchContext& context) {
        auto* s = services_->components().state<AlertState>(id);
        if (!s) {
            return;
        }
        if (context.kind() == input::PointerEventKind::enter) {
            s->hovered = true;
        }
        if (context.kind() == input::PointerEventKind::leave || context.kind() == input::PointerEventKind::cancel) {
            s->hovered = false;
        }
        const auto result =
            s->press.dispatch(context, s->snapshot.close_interaction, s->snapshot.visible && s->snapshot.closable);
        refresh_alert(id, false);
        context.stop_propagation();
        if (result.activate) {
            close_alert(id);
        }
    };
    services.interactions().set_handlers(hit, std::move(pointer));
    input::FocusHandlers focus;
    focus.state_changed = [this, id](input::FocusPresentation value) {
        if (auto* s = services_->components().state<AlertState>(id)) {
            s->focus = value;
            refresh_alert(id, false);
        }
    };
    focus.activation_allowed = [this, id] {
        const auto* s = services_->components().state<AlertState>(id);
        return s && s->snapshot.visible && s->snapshot.closable;
    };
    focus.activate = [this, id] {
        close_alert(id);
    };
    services.interactions().set_focus_handlers(hit, std::move(focus));
    services.layout().set_layout(services.components().root(s.snapshot.body),
                                 layout::FlexLayout{layout::FlexDirection::vertical, 0});
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                auto& s = *services_->components().state<AlertState>(id);
                if (!s.snapshot.visible) {
                    return runtime::Size{};
                }
                const auto& m = services_->components().theme_scope(id)->snapshot().alert().metrics;
                const auto px = std::min(
                    (s.snapshot.has_description ? m.description_padding_inline : m.padding_inline) + m.border_width,
                    limits.max_width / 2);
                const auto py =
                    (s.snapshot.has_description ? m.description_padding_block : m.padding_block) + m.border_width;
                float available = std::max(0.0F, limits.max_width - 2 * px);
                const auto height = std::max(0.0F, limits.max_height - 2 * py);
                const std::array ids{s.snapshot.close, s.snapshot.action, s.snapshot.icon};
                const std::array shown{s.snapshot.closable, s.action_slot, s.show_icon};
                float reserved = 0;
                for (std::size_t i = 0; i < ids.size(); ++i) {
                    s.sizes[i] =
                        shown[i] ? engine.measure_child(services_->components().root(ids[i]), {0, available, 0, height})
                                 : runtime::Size{};
                    const auto gap = shown[i] ? std::min(m.gap, std::max(0.0F, available - s.sizes[i].width)) : 0;
                    reserved += s.sizes[i].width + gap;
                    available = std::max(0.0F, available - s.sizes[i].width - gap);
                }
                s.sizes[3] =
                    engine.measure_child(services_->components().root(s.snapshot.body), {0, available, 0, height});
                return limits.constrain(
                    {std::isfinite(limits.max_width) ? limits.max_width : 2 * px + reserved + s.sizes[3].width,
                     2 * py + std::max({s.sizes[0].height, s.sizes[1].height, s.sizes[2].height, s.sizes[3].height})});
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect r) {
                auto& s = *services_->components().state<AlertState>(id);
                if (!s.snapshot.visible) {
                    return;
                }
                const auto& m = services_->components().theme_scope(id)->snapshot().alert().metrics;
                const auto px = std::min(
                    (s.snapshot.has_description ? m.description_padding_inline : m.padding_inline) + m.border_width,
                    r.width / 2);
                const auto py = std::min((s.snapshot.has_description ? m.description_padding_block : m.padding_block) +
                                             m.border_width,
                                         r.height / 2);
                float left = r.x + px;
                float right = r.x + r.width - px;
                const auto place = [&](runtime::ComponentId child, runtime::Size size, float x) {
                    const auto h = std::min(size.height, std::max(0.0F, r.height - py * 2));
                    engine.place_child(
                        services_->components().root(child),
                        {x, s.snapshot.has_description ? r.y + py : r.y + (r.height - h) / 2, size.width, h});
                };
                for (std::size_t i = 0; i < 2; ++i) {
                    if (i == 0 ? !s.snapshot.closable : !s.action_slot) {
                        continue;
                    }
                    const auto child = i == 0 ? s.snapshot.close : s.snapshot.action;
                    auto size = s.sizes[i];
                    size.width = std::min(size.width, std::max(0.0F, right - left));
                    right -= size.width;
                    place(child, size, right);
                    right -= std::min(m.gap, std::max(0.0F, right - left));
                }
                if (s.show_icon) {
                    auto size = s.sizes[2];
                    size.width = std::min(size.width, std::max(0.0F, right - left));
                    place(s.snapshot.icon, size, left);
                    left += size.width + std::min(m.gap, std::max(0.0F, right - left - size.width));
                }
                place(s.snapshot.body, {std::max(0.0F, right - left), s.sizes[3].height}, left);
            }});
    alerts_.push_back(id);
    refresh_alert(id, true);
    auto& scope = build.scope(id);
    connect_prop(scope, props.status_, [this, id](Status value) {
        validate(value);
        if (auto* s = services_->components().state<AlertState>(id)) {
            s->snapshot.status = value;
            refresh_alert(id, false);
        }
    });
    connect_prop(scope, props.message_, [this, id](String value) {
        if (auto* s = services_->components().state<AlertState>(id)) {
            s->message.set(std::move(value));
        }
    });
    connect_prop(scope, props.description_, [this, id](String value) {
        if (auto* s = services_->components().state<AlertState>(id)) {
            s->description.set(std::move(value));
            refresh_alert(id, true);
        }
    });
    connect_prop(scope, props.show_icon_, [this, id](bool value) {
        if (auto* s = services_->components().state<AlertState>(id)) {
            s->show_icon = value;
            refresh_alert(id, true);
        }
    });
    connect_prop(scope, props.closable_, [this, id](bool value) {
        if (auto* s = services_->components().state<AlertState>(id)) {
            s->snapshot.closable = value;
            refresh_alert(id, true);
        }
    });
    connect_prop(scope, props.visible_, [this, id](bool value) {
        if (auto* s = services_->components().state<AlertState>(id)) {
            s->snapshot.visible = value;
            refresh_alert(id, true);
        }
    });
    const auto theme = build.theme_scope();
    s.subscriptions[0] = theme->capture([this, id](auto) { refresh_alert(id, false); },
                                        [theme] { static_cast<void>(theme->alert_colors()); });
    s.subscriptions[1] = theme->capture([this, id](auto) { refresh_alert(id, true); },
                                        [theme] { static_cast<void>(theme->alert_metrics()); });
}

void FeedbackComponentHost::refresh_spin(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<SpinState>(id);
    if (!s) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().spin();
    const auto& m = t.metrics;
    const auto dot = sized(s->size, m.dot_size_small, m.dot_size, m.dot_size_large);
    s->color.set(foreground(t.colors.indicator));
    s->font.set({m.font_family, 400, false, m.font_size, m.line_height});
    s->icon_font.set({m.font_family, 400, false, dot, dot});
    active(*services_, s->snapshot.overlay, s->snapshot.spinning);
    active(*services_, s->snapshot.description, !s->description.get().empty());
    s->animate.set(s->snapshot.spinning && s->snapshot.in_view);
    if (services_->interactions().set_eligible(s->snapshot.shield, s->snapshot.spinning && s->snapshot.wrapped)) {
        services_->mark_scene_structure_dirty();
    }
    invalidate(*services_, s->node, geometry);
    // Reconciliation can deliver blur callbacks that destroy this component.
    const auto content = s->snapshot.content;
    const auto blocked = s->snapshot.spinning && s->snapshot.wrapped;
    services_->set_input_blocked(content, blocked);
}

void FeedbackComponentHost::mount(const SpinProps& props, const std::optional<SpinContent>& content,
                                  const SpinSlots& slots) {
    validate(read_prop(props.size_));
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<SpinState>();
    auto& s = build.state<SpinState>(id);
    s.node = build.root(id);
    s.size = read_prop(props.size_);
    s.snapshot.spinning = read_prop(props.spinning_);
    s.snapshot.wrapped = content.has_value();
    s.description.set(read_prop(props.description_));
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.mount_slot(id, Content{[&] {
                         auto& context = runtime::require_component_build_context();
                         s.snapshot.content = context.mount_component<int>(0);
                         const auto node = context.root(s.snapshot.content);
                         services.layout().set_layout(node, layout::BoxLayout{});
                         context.on_resource_cleanup(s.snapshot.content,
                                                     [&services, node] { services.layout().remove_layout(node); });
                         if (content) {
                             context.mount_slot(s.snapshot.content, *content);
                         }
                     }});
    s.snapshot.overlay = region(services, id, Content{[] {}}, s.color, s.font);
    const auto overlay_node = services.components().root(s.snapshot.overlay);
    s.snapshot.shield =
        services.interactions().create({s.snapshot.overlay, overlay_node, {}, true, false, {}, false, false});
    build.on_resource_cleanup(id, [&services, node = s.node, hit = s.snapshot.shield, content = s.snapshot.content] {
        services.interactions().set_input_blocked(content, false);
        services.pointer().cancel_interaction(hit);
        services.interactions().remove(hit);
        services.layout().remove_layout(node);
    });
    input::InteractionHandlers shield;
    shield.target = [](input::PointerDispatchContext& context) {
        context.stop_propagation();
    };
    services.interactions().set_handlers(s.snapshot.shield, std::move(shield));
    s.mask = surface(services, s.snapshot.overlay, s.snapshot.shield);
    s.snapshot.indicator = region(services, s.snapshot.overlay, Content{[slot = slots.indicator, animate = s.animate] {
                                      if (slot) {
                                          SlotContentAccess::function (*slot)();
                                      } else {
                                          Icon(IconProps{}.name(IconName::LoadingOutlined).spin(animate));
                                      }
                                  }},
                                  s.color, s.icon_font);
    s.snapshot.description =
        region(services, s.snapshot.overlay, Content{[text = s.description] { Text(TextProps{}.content(text)); }},
               s.color, s.font);
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                auto& s = *services_->components().state<SpinState>(id);
                const auto& m = services_->components().theme_scope(id)->snapshot().spin().metrics;
                const auto available = layout::Constraints{0, limits.max_width, 0, limits.max_height};
                s.sizes[0] = engine.measure_child(services_->components().root(s.snapshot.content), available);
                const auto icon = engine.measure_child(services_->components().root(s.snapshot.indicator), available);
                s.sizes[2] = icon;
                const auto description =
                    s.description.get().empty()
                        ? runtime::Size{}
                        : engine.measure_child(services_->components().root(s.snapshot.description), available);
                s.sizes[1] = {std::max(icon.width, description.width),
                              icon.height + (description.height > 0 ? m.description_gap + description.height : 0)};
                if (s.snapshot.spinning) {
                    static_cast<void>(
                        engine.measure_child(services_->components().root(s.snapshot.overlay), available));
                }
                return limits.constrain(s.snapshot.wrapped    ? s.sizes[0]
                                        : s.snapshot.spinning ? s.sizes[1]
                                                              : runtime::Size{});
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect r) {
                auto& s = *services_->components().state<SpinState>(id);
                engine.place_child(services_->components().root(s.snapshot.content), r);
                if (s.snapshot.spinning) {
                    engine.place_child(services_->components().root(s.snapshot.overlay), r, true, true);
                }
            }});
    services.layout().set_layout(
        overlay_node,
        layout::ComponentLayout{
            [](layout::LayoutEngine&, runtime::NodeId, layout::Constraints limits) { return limits.constrain({}); },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect r) {
                auto& s = *services_->components().state<SpinState>(id);
                const auto& m = services_->components().theme_scope(id)->snapshot().spin().metrics;
                const auto icon =
                    runtime::Size{std::min(s.sizes[2].width, r.width), std::min(s.sizes[2].height, r.height)};
                const auto description =
                    s.description.get().empty()
                        ? runtime::Size{}
                        : engine.measure_child(
                              services_->components().root(s.snapshot.description),
                              {0, r.width, 0, std::max(0.0F, r.height - icon.height - m.description_gap)});
                const auto h = icon.height + (description.height > 0 ? m.description_gap + description.height : 0);
                const auto y = r.y + std::max(0.0F, (r.height - h) / 2);
                engine.place_child(services_->components().root(s.snapshot.indicator),
                                   {r.x + (r.width - icon.width) / 2, y, icon.width, icon.height});
                if (description.height > 0) {
                    engine.place_child(services_->components().root(s.snapshot.description),
                                       {r.x + (r.width - description.width) / 2, y + icon.height + m.description_gap,
                                        description.width, description.height});
                }
            }});
    spins_.push_back(id);
    refresh_spin(id, true);
    auto& scope = build.scope(id);
    connect_prop(scope, props.spinning_, [this, id](bool value) {
        if (auto* s = services_->components().state<SpinState>(id)) {
            s->snapshot.spinning = value;
            refresh_spin(id, true);
        }
    });
    connect_prop(scope, props.size_, [this, id](ControlSize value) {
        validate(value);
        if (auto* s = services_->components().state<SpinState>(id)) {
            s->size = value;
            refresh_spin(id, true);
        }
    });
    connect_prop(scope, props.description_, [this, id](String value) {
        if (auto* s = services_->components().state<SpinState>(id)) {
            s->description.set(std::move(value));
            refresh_spin(id, true);
        }
    });
    const auto theme = build.theme_scope();
    s.subscriptions[0] = theme->capture([this, id](auto) { refresh_spin(id, false); },
                                        [theme] { static_cast<void>(theme->spin_colors()); });
    s.subscriptions[1] = theme->capture([this, id](auto) { refresh_spin(id, true); },
                                        [theme] { static_cast<void>(theme->spin_metrics()); });
}

void FeedbackComponentHost::refresh_progress(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<ProgressState>(id);
    if (!s) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().progress();
    const auto& m = t.metrics;
    s->snapshot.status = s->requested_status == ProgressStatus::Exception ? ProgressStatus::Exception
                         : s->requested_status == ProgressStatus::Success || s->snapshot.percent >= 100
                             ? ProgressStatus::Success
                             : ProgressStatus::Normal;
    s->snapshot.color = s->snapshot.status == ProgressStatus::Exception ? t.colors.exception
                        : s->snapshot.status == ProgressStatus::Success ? t.colors.success
                                                                        : t.colors.normal;
    s->color.set(foreground(s->snapshot.status == ProgressStatus::Normal ? t.colors.text : s->snapshot.color));
    const auto display = static_cast<int>(std::round(s->snapshot.percent));
    s->label.set(s->snapshot.status == ProgressStatus::Success ? String{u8"✓"}
                 : s->snapshot.status == ProgressStatus::Exception
                     ? String{u8"!"}
                     : String::from_utf8(std::to_string(display) + "%").value());
    s->font.set({m.font_family, 400, false, m.font_size, m.line_height});
    active(*services_, s->snapshot.info, s->show_info);
    invalidate(*services_, s->node, geometry);
}

void FeedbackComponentHost::mount(const ProgressProps& props) {
    validate(read_prop(props.type_));
    validate(read_prop(props.status_));
    validate(read_prop(props.size_));
    const auto value = percent(read_prop(props.percent_));
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<ProgressState>();
    auto& s = build.state<ProgressState>(id);
    s.node = build.root(id);
    s.snapshot.type = read_prop(props.type_);
    s.snapshot.percent = value;
    s.requested_status = read_prop(props.status_);
    s.size = read_prop(props.size_);
    s.show_info = read_prop(props.show_info_);
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.surface = services.surfaces().create_content_range(fragment, {});
    build.on_resource_cleanup(id,
                              [&services, surface = s.surface] { services.surfaces().destroy_content_range(surface); });
    s.snapshot.info =
        region(services, id, Content{[text = s.label] { Text(TextProps{}.content(text)); }}, s.color, s.font);
    services.layout().set_layout(services.components().root(s.snapshot.info),
                                 layout::FlexLayout{layout::FlexDirection::horizontal,
                                                    0,
                                                    {},
                                                    false,
                                                    false,
                                                    layout::FlexWrap::no_wrap,
                                                    layout::FlexJustify::center,
                                                    layout::FlexAlign::center});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        auto& s = *services_->components().state<ProgressState>(id);
                        const auto& m = services_->components().theme_scope(id)->snapshot().progress().metrics;
                        const bool line = s.snapshot.type == ProgressType::Line;
                        const auto diameter = sized(s.size, m.circle_size_small, m.circle_size, m.circle_size_large);
                        const auto stroke = sized(s.size, m.line_width_small, m.line_width, m.line_width_large);
                        if (s.show_info) {
                            static_cast<void>(engine.measure_child(
                                services_->components().root(s.snapshot.info),
                                {0, line ? m.info_width : diameter, 0, line ? m.line_height : diameter}));
                        }
                        return limits.constrain(
                            line ? runtime::Size{std::isfinite(limits.max_width) ? limits.max_width : m.line_length,
                                                 std::max(stroke, s.show_info ? m.line_height : 0)}
                                 : runtime::Size{diameter, diameter});
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect r) {
                        auto& s = *services_->components().state<ProgressState>(id);
                        const auto& m = services_->components().theme_scope(id)->snapshot().progress().metrics;
                        if (!s.show_info) {
                            return;
                        }
                        const bool line = s.snapshot.type == ProgressType::Line;
                        const auto width = std::min(line ? m.info_width : r.width, r.width);
                        const auto h = std::min(m.line_height, r.height);
                        // Center the info region's children for circle/dashboard without changing their identity.
                        engine.place_child(services_->components().root(s.snapshot.info),
                                           {line ? r.x + r.width - width : r.x + (r.width - width) / 2,
                                            r.y + (r.height - h) / 2, width, h},
                                           true, true);
                    }});
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    progress_.push_back(id);
    refresh_progress(id, true);
    auto& scope = build.scope(id);
    connect_prop(scope, props.percent_, [this, id](float value) {
        value = percent(value);
        if (auto* s = services_->components().state<ProgressState>(id)) {
            s->snapshot.percent = value;
            refresh_progress(id, false);
        }
    });
    connect_prop(scope, props.status_, [this, id](ProgressStatus value) {
        validate(value);
        if (auto* s = services_->components().state<ProgressState>(id)) {
            s->requested_status = value;
            refresh_progress(id, false);
        }
    });
    connect_prop(scope, props.type_, [this, id](ProgressType value) {
        validate(value);
        if (auto* s = services_->components().state<ProgressState>(id)) {
            s->snapshot.type = value;
            refresh_progress(id, true);
        }
    });
    connect_prop(scope, props.size_, [this, id](ControlSize value) {
        validate(value);
        if (auto* s = services_->components().state<ProgressState>(id)) {
            s->size = value;
            refresh_progress(id, true);
        }
    });
    connect_prop(scope, props.show_info_, [this, id](bool value) {
        if (auto* s = services_->components().state<ProgressState>(id)) {
            s->show_info = value;
            refresh_progress(id, true);
        }
    });
    const auto theme = build.theme_scope();
    s.subscriptions[0] = theme->capture([this, id](auto) { refresh_progress(id, false); },
                                        [theme] { static_cast<void>(theme->progress_colors()); });
    s.subscriptions[1] = theme->capture([this, id](auto) { refresh_progress(id, true); },
                                        [theme] { static_cast<void>(theme->progress_metrics()); });
}

bool FeedbackComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    return std::any_of(spins_.begin(), spins_.end(), [&](auto id) {
        const auto* s = services_->components().state<SpinState>(id);
        return s && s->snapshot.spinning && s->snapshot.wrapped && hit == s->snapshot.shield;
    });
}

void FeedbackComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : alerts_) {
        auto* s = services_->components().state<AlertState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& node = services_->nodes().require(s->node);
        const auto& t = services_->components().theme_scope(id)->snapshot().alert();
        const auto& m = t.metrics;
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
        const auto p = s->snapshot.palette;
        const auto q = quad(node, node.bounds, p.background, m.border_radius);
        static_cast<void>(services_->surfaces().update(s->surface, std::span{&q, 1}));
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.ancestor_clip = graphics::EffectClip{1, clip};
        static_cast<void>(services_->surfaces().update_effects(s->surface, effects));
        const auto r = node.bounds;
        const auto width = std::min(m.border_width, std::min(r.width, r.height) / 2);
        const auto radius = std::min(m.border_radius, std::min(r.width, r.height) / 2);
        const runtime::Rect inner{r.x + width, r.y + width, std::max(0.0F, r.width - 2 * width),
                                  std::max(0.0F, r.height - 2 * width)};
        const auto close = bounds(*services_, s->snapshot.close);
        const std::array borders{
            graphics::make_outline_effect({inner, std::max(0.0F, radius - width)}, width > 0 ? width : 1, 0, p.border,
                                          width > 0 ? node.opacity : 0, node.translation,
                                          graphics::EffectClip{1, clip}),
            graphics::make_outline_effect(
                {close, 0}, m.focus_width > 0 ? m.focus_width : 1, m.focus_offset, t.colors.focus,
                m.focus_width > 0 && s->snapshot.closable && s->focus.focus_visible ? node.opacity : 0,
                node.translation, graphics::EffectClip{1, clip})};
        static_cast<void>(services_->surfaces().update_content_effects(s->border, borders));
    }
    for (auto id : spins_) {
        auto* s = services_->components().state<SpinState>(id);
        if (!s) {
            continue;
        }
        const auto& node = services_->nodes().require(s->node);
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
        const auto icon = bounds(*services_, s->snapshot.indicator);
        const runtime::Rect translated{icon.x + node.translation.x, icon.y + node.translation.y, icon.width,
                                       icon.height};
        s->snapshot.in_view = services_->components().branch_active(id) && node.opacity > 0 &&
                              intersects(translated, clip) &&
                              intersects(translated, {0, 0, viewport.width, viewport.height});
        s->animate.set(s->snapshot.spinning && s->snapshot.in_view);
        const auto& t = services_->components().theme_scope(id)->snapshot().spin();
        const auto q =
            quad(node, node.bounds, t.colors.mask, 0, s->snapshot.spinning && s->snapshot.wrapped ? 1.0F : 0.0F);
        static_cast<void>(services_->surfaces().update(s->mask, std::span{&q, 1}));
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.ancestor_clip = graphics::EffectClip{1, clip};
        static_cast<void>(services_->surfaces().update_effects(s->mask, effects));
    }
    for (auto id : progress_) {
        auto* s = services_->components().state<ProgressState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& node = services_->nodes().require(s->node);
        const auto& t = services_->components().theme_scope(id)->snapshot().progress();
        const auto& m = t.metrics;
        const auto r = node.bounds;
        std::array<graphics::QuadInstance, progress_layers> quads;
        for (auto& q : quads) {
            q.opacity = 0;
        }
        if (s->snapshot.type == ProgressType::Line) {
            const auto info = s->show_info ? std::min(r.width, m.info_width + m.info_gap) : 0;
            const auto stroke =
                std::min(r.height, sized(s->size, m.line_width_small, m.line_width, m.line_width_large));
            const runtime::Rect track{r.x, r.y + (r.height - stroke) / 2, std::max(0.0F, r.width - info), stroke};
            s->snapshot.track = track;
            s->snapshot.sweep_degrees = 0;
            quads[0] = quad(node, track, t.colors.remaining, m.line_radius);
            quads[1] = quad(node, {track.x, track.y, track.width * s->snapshot.percent / 100, track.height},
                            s->snapshot.color, m.line_radius);
        } else {
            const auto diameter = std::min(r.width, r.height);
            const auto stroke = diameter * m.circle_stroke_ratio;
            const auto radius = std::max(0.0F, (diameter - stroke) / 2);
            const auto cx = r.x + r.width / 2;
            const auto cy = r.y + r.height / 2;
            const bool dash = s->snapshot.type == ProgressType::Dashboard;
            const auto sweep = dash ? 285.0F : 360.0F;
            const auto start = dash ? 127.5F : -90.0F;
            const auto fraction = s->snapshot.percent / 100;
            s->snapshot.track = {cx - diameter / 2, cy - diameter / 2, diameter, diameter};
            s->snapshot.sweep_degrees = sweep * fraction;
            for (std::size_t i = 0; i < arc_points; ++i) {
                const auto point = [&](float turn, Color color, float opacity) {
                    const auto angle = (start + sweep * turn) * std::numbers::pi_v<float> / 180;
                    return quad(node,
                                {cx + radius * std::cos(angle) - stroke / 2, cy + radius * std::sin(angle) - stroke / 2,
                                 stroke, stroke},
                                color, stroke / 2, opacity);
                };
                const auto turn = static_cast<float>(i) / static_cast<float>(arc_points - 1);
                const auto previous = static_cast<float>(i == 0 ? 0 : i - 1) / static_cast<float>(arc_points - 1);
                quads[2 + i] = point(std::min(turn, fraction), s->snapshot.color,
                                     fraction > 0 && previous <= fraction ? 1.0F : 0.0F);
            }
        }
        const auto clip = graphics::EffectClip{
            1, services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip))};
        std::array<graphics::RoundedEffectInstance, progress_layers> fills;
        for (std::size_t i = 0; i < quads.size(); ++i) {
            const auto& q = quads[i];
            // Zero-blur rounded fill uses the existing effect clip contract, including window layers.
            fills[i] = graphics::make_shadow_effect(
                {{q.bounds[0], q.bounds[1], q.bounds[2], q.bounds[3]}, q.corner_radius},
                ShadowLayer{ShadowKind::outer, {}, 0, 0, Color{q.color[0], q.color[1], q.color[2], q.color[3]}},
                node.translation, clip);
            fills[i].material.opacity = q.opacity;
        }
        if (s->snapshot.type != ProgressType::Line) {
            const auto diameter = std::min(r.width, r.height);
            const auto stroke = diameter * m.circle_stroke_ratio;
            const auto inner_radius = std::max(0.0F, diameter / 2 - stroke);
            const auto cx = r.x + r.width / 2;
            const auto cy = r.y + r.height / 2;
            auto rail_clip = clip;
            if (s->snapshot.type == ProgressType::Dashboard) {
                const auto cutoff = cy + (diameter - stroke) / 2 * std::cos(37.5F * std::numbers::pi_v<float> / 180);
                rail_clip.bounds = graphics::intersect_effect_bounds(
                    rail_clip.bounds,
                    {r.x + node.translation.x, r.y + node.translation.y, r.width, std::max(0.0F, cutoff - r.y)});
            }
            // A single rail avoids alpha accumulation between overlapping arc samples.
            if (inner_radius > 0 && stroke > 0) {
                fills[0] = graphics::make_outline_effect(
                    {{cx - inner_radius, cy - inner_radius, inner_radius * 2, inner_radius * 2}, inner_radius}, stroke,
                    0, t.colors.remaining, node.opacity, node.translation, rail_clip);
            } else {
                fills[0] = graphics::make_shadow_effect(
                    {{cx - diameter / 2, cy - diameter / 2, diameter, diameter}, diameter / 2},
                    ShadowLayer{ShadowKind::outer, {}, 0, 0, t.colors.remaining}, node.translation, rail_clip);
                fills[0].material.opacity = node.opacity;
            }
        }
        static_cast<void>(services_->surfaces().update_content_effects(s->surface, fills));
    }
}

} // namespace ryn::detail

namespace ryn {
void Alert(AlertProps props, AlertSlots slots) {
    if (!detail::active_feedback) {
        throw std::logic_error("Alert requires window component services");
    }
    detail::active_feedback->mount(props, slots);
}

void Spin(SpinProps props) {
    if (!detail::active_feedback) {
        throw std::logic_error("Spin requires window component services");
    }
    detail::active_feedback->mount(props, std::nullopt, {});
}

void Spin(SpinProps props, SpinContent content, SpinSlots slots) {
    if (!detail::active_feedback) {
        throw std::logic_error("Spin requires window component services");
    }
    detail::active_feedback->mount(props, content, slots);
}

void Progress(ProgressProps props) {
    if (!detail::active_feedback) {
        throw std::logic_error("Progress requires window component services");
    }
    detail::active_feedback->mount(props);
}
} // namespace ryn
