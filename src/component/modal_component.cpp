#include "component/modal_component.hpp"
#include "component/modal_model.hpp"
#include "component/drawer_model.hpp"
#include "component/button_component.hpp"
#include "component/input_component.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/icon.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local ModalComponentHost* active_modals{};

struct ModalState final {
    explicit ModalState(ModalOptions options) : model(options) {}

    ModalModel model;
    std::optional<DrawerGeometry> drawer;
    ModalSnapshot snapshot;
    runtime::NodeId node;
    runtime::ComponentId default_footer;
    runtime::ComponentId custom_footer;
    input::InteractionId shield_hit;
    input::InteractionId dialog_hit;
    component::RetainedSurfaceId shield_surface;
    component::RetainedSurfaceId dialog_surface;
    Signal<String> title{String{}};
    Signal<String> ok_text{String{u8"OK"}};
    Signal<String> cancel_text{String{u8"Cancel"}};
    Signal<bool> loading{false};
    Signal<runtime::SemanticForeground> text_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> title_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> body_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> title_font{runtime::SemanticTypography{}};
    Signal<ThemeConfig> close_theme{ThemeConfig{}};
    std::array<theme_runtime::Subscription, 5> subscriptions;
    std::function<void()> on_ok;
    std::function<void(ModalCancelReason)> on_cancel;
    std::function<void()> after_close;
    std::function<void(bool)> after_open_change;
    bool title_slot{};
    bool footer_slot{};
    bool centered{};
    bool restore{true};
    bool ready{};
    bool needs_layout{true};
    bool reveal{true};
    float scale{1};
    float padding_x{};
    float padding_y{};
    float header_height{};
    float footer_height{};
    float header_gap{};
    float footer_gap{};
    float body_height{};
    float content_height{};
    runtime::Rect header_bounds;
    runtime::Rect footer_bounds;
    runtime::Rect layout_clip;
    std::optional<input::InteractionId> revealed_focus;
    std::optional<std::uint64_t> revealed_text_revision;
    std::optional<std::size_t> revealed_caret_byte;
    std::optional<input::InteractionId> trigger;
};

struct PanelThemeView final {
    const ModalColorToken& colors;
    ModalMetricToken metrics;
    const ModalTypographyToken& typography;
    const ModalEffectToken& effects;
    std::int32_t z_index_popup;
};

PanelThemeView panel_theme(const ThemeSnapshot& theme, bool drawer) {
    if (drawer) {
        const auto& token = theme.drawer();
        return {token.colors,
                {token.metrics.padding_inline, token.metrics.padding_block, 0, 0, 0, 0, token.metrics.close_size,
                 token.metrics.close_icon_size, 0, 0, 0},
                token.typography,
                token.effects,
                token.z_index_popup};
    }
    const auto& token = theme.modal();
    return {token.colors, token.metrics, token.typography, token.effects, token.z_index_popup};
}

runtime::SemanticForeground foreground(Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

graphics::QuadInstance quad(runtime::Rect bounds, Color color, float radius = 0) {
    graphics::QuadInstance result;
    result.bounds = {bounds.x, bounds.y, bounds.width, bounds.height};
    result.color = {color.red(), color.green(), color.blue(), color.alpha()};
    result.corner_radius = radius;
    return result;
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto current = std::optional{child}; current && services.components().contains(*current);
         current = services.components().parent(*current)) {
        if (*current == parent) {
            return true;
        }
    }
    return false;
}

bool can_focus(WindowComponentServices& services, input::InteractionId id) {
    const auto* record = services.interactions().find(id);
    return record && record->eligible && record->focusable && services.interactions().branch_active(id);
}

bool restorable(WindowComponentServices& services, input::InteractionId id,
                std::optional<runtime::ComponentId> boundary) {
    const auto* record = services.interactions().find(id);
    return record && record->eligible && record->focusable && services.components().branch_active(record->component) &&
           (!boundary || descendant(services, record->component, *boundary));
}

void settle_scroll_metrics(WindowComponentServices& services, ModalState& state) {
    for (const auto& text : services.text().mounted_texts()) {
        if (descendant(services, text.component, state.snapshot.body)) {
            const auto scale =
                services.text().scene_service().text_state(text.scene).shaped().default_metrics.display_scale;
            state.scale = std::isfinite(scale) && scale > 0 ? scale : 1;
            break;
        }
    }
    state.snapshot.maximum_scroll =
        std::ceil(std::max(0.0F, state.content_height - state.body_height) * state.scale) / state.scale;
    state.snapshot.scroll = std::clamp(state.snapshot.scroll, 0.0F, state.snapshot.maximum_scroll);
}

runtime::Rect bounds(WindowComponentServices& services, runtime::ComponentId id) {
    const auto& node = services.nodes().require(services.components().root(id));
    return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
            node.bounds.height};
}

void translate(runtime::NodeId node, runtime::Point offset, WindowComponentServices& services) {
    if (services.nodes().require(node).clip_boundary) {
        return;
    }
    runtime::NodePropertyWriter writer{services.nodes(), services.dirty()};
    writer.set_translation(node, offset);
    for (auto child : services.nodes().require(node).children) {
        translate(child, offset, services);
    }
}

bool inside(runtime::Rect rect, float x, float y) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
}
} // namespace

ModalComponentHost::ModalComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

ModalComponentHost::~ModalComponentHost() {
    services_->detach(*this);
}

void* ModalComponentHost::begin_mount() noexcept {
    auto* previous = active_modals;
    active_modals = this;
    mounting_ = true;
    return previous;
}

void ModalComponentHost::end_mount(void* previous) noexcept {
    active_modals = static_cast<ModalComponentHost*>(previous);
    mounting_ = active_modals == this;
}

ModalSnapshot ModalComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* state = services_->components().state<ModalState>(id);
    if (!state) {
        throw std::out_of_range("Modal component is stale");
    }
    auto result = state->snapshot;
    result.visible = state->model.visible();
    result.topmost = !stack_.empty() && stack_.back().owner == id;
    return result;
}

void ModalComponentHost::on_dispose() noexcept {
    mounted_.clear();
    stack_.clear();
    transitions_.clear();
    focus_pending_ = false;
    restore_target_.reset();
}

void ModalComponentHost::on_destroy() noexcept {
    if (!stack_.empty() && !services_->components().contains(stack_.back().owner)) {
        focus_pending_ = true;
        restore_target_.reset();
        for (auto entry = stack_.rbegin(); entry != stack_.rend(); ++entry) {
            if (!services_->components().contains(entry->owner) && entry->restore && entry->trigger &&
                restorable(*services_, *entry->trigger, {})) {
                restore_target_ = entry->trigger;
                break;
            }
        }
    }
    std::erase_if(stack_, [this](const auto& entry) { return !services_->components().contains(entry.owner); });
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
    std::erase_if(transitions_, [this](const auto& event) { return !services_->components().contains(event.owner); });
    if (focus_pending_) {
        // No user callbacks in this noexcept cleanup. Window services reconcile
        // pointer/focus state after every host has retired its resources.
        services_->interactions().set_input_boundary(stack_.empty() ? std::optional<runtime::ComponentId>{}
                                                                    : stack_.back().overlay);
    }
}

void ModalComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<ModalState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto token = panel_theme(services_->components().theme_scope(id)->snapshot(), s->drawer.has_value());
    const auto& fonts = token.typography;
    s->text_color.set(foreground(token.colors.text));
    s->title_color.set(foreground(token.colors.title));
    s->body_font.set({fonts.font_family, fonts.font_weight, false, fonts.font_size, fonts.line_height});
    s->title_font.set(
        {fonts.font_family, fonts.title_font_weight, false, fonts.title_font_size, fonts.title_line_height});
    s->loading.set(s->model.options().confirm_loading);
    ThemeConfig close_theme;
    close_theme.button.seed.control_height = dp(token.metrics.close_size);
    close_theme.button.seed.font_size = dp(token.metrics.close_icon_size);
    close_theme.button.tokens.padding_inline = dp(0);
    close_theme.button.tokens.icon_gap = dp(0);
    auto& palette = close_theme.button.tokens.colors[static_cast<std::size_t>(ButtonColor::Primary)];
    palette.base = token.colors.close;
    palette.hover = token.colors.close_hover;
    palette.active = token.colors.close_hover;
    palette.light = token.colors.close_hover_background;
    palette.light_active = token.colors.close_hover_background;
    s->close_theme.set(std::move(close_theme));
    const bool header = s->title_slot || !s->title.get().empty() || s->model.options().closable;
    const bool footer = s->footer_slot || s->model.options().footer == ModalFooterMode::Default;
    for (const auto& [component, shown] :
         {std::pair{s->snapshot.title, header}, std::pair{s->snapshot.close, s->model.options().closable},
          std::pair{s->snapshot.footer, footer}, std::pair{s->default_footer, footer && !s->footer_slot},
          std::pair{s->custom_footer, s->footer_slot}}) {
        if (services_->components().set_branch_active(component, shown)) {
            services_->mark_scene_structure_dirty();
        }
    }
    if (services_->components().set_window_layer(s->snapshot.overlay, token.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    if (geometry) {
        s->needs_layout = true;
        s->reveal = true;
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
    if (s->ready && !mounting_) {
        reconcile();
    }
}

void ModalComponentHost::reconcile() {
    if (mounting_) {
        return;
    }
    if (mounted_.empty() && stack_.empty() && !focus_pending_) {
        return;
    }
    if (reconciling_) {
        reconcile_again_ = true;
        return;
    }
    reconciling_ = true;
    try {
        do {
            reconcile_again_ = false;
            const auto old_top = stack_.empty() ? std::optional<Entry>{} : stack_.back();
            std::optional<input::InteractionId> closed_restore;
            for (auto id : mounted_) {
                auto* s = services_->components().state<ModalState>(id);
                if (!s || !s->ready || !services_->components().scope(id).active()) {
                    continue;
                }
                const auto change = s->model.settle_visibility(services_->components().branch_active(id));
                if (!change) {
                    continue;
                }
                services_->components().set_branch_active(s->snapshot.overlay, *change);
                services_->mark_scene_structure_dirty();
                s->needs_layout = true;
                s->reveal = true;
                s->revealed_focus.reset();
                s->revealed_text_revision.reset();
                s->revealed_caret_byte.reset();
                if (*change) {
                    s->trigger = services_->focus().state().focused;
                    stack_.push_back({id, s->snapshot.overlay, s->trigger, s->restore});
                    services_->components().set_window_layer_stack(s->snapshot.overlay, ++next_order_);
                } else {
                    if (s->restore && s->trigger && restorable(*services_, *s->trigger, {})) {
                        closed_restore = s->trigger;
                    }
                    std::erase_if(stack_, [id](const auto& entry) { return entry.owner == id; });
                }
                transitions_.push_back({id, *change});
                if (notifying_) {
                    services_->request_frame();
                }
            }
            const auto top = stack_.empty() ? std::optional<Entry>{} : stack_.back();
            const bool top_changed = (!old_top && top) || (old_top && (!top || old_top->owner != top->owner));
            if (top_changed) {
                focus_pending_ = true;
                restore_target_.reset();
                const bool old_closed = old_top && std::ranges::none_of(stack_, [&](const auto& entry) {
                                            return entry.owner == old_top->owner;
                                        });
                if (old_closed && old_top->restore) {
                    restore_target_ = old_top->trigger;
                    if ((!restore_target_ ||
                         !restorable(*services_, *restore_target_,
                                     top ? std::optional{top->overlay} : std::optional<runtime::ComponentId>{})) &&
                        closed_restore) {
                        restore_target_ = closed_restore;
                    }
                }
            }
            if (top_changed) {
                services_->set_input_boundary(top ? std::optional{top->overlay}
                                                  : std::optional<runtime::ComponentId>{});
            }
            on_input_boundary_changed();
        } while (reconcile_again_);
        reconciling_ = false;
    } catch (...) {
        reconciling_ = false;
        throw;
    }
}

void ModalComponentHost::focus_top() {
    auto& services = *services_;
    if (!services.focus().state().window_active) {
        return;
    }
    const auto top = stack_.empty() ? std::optional<Entry>{} : stack_.back();
    if (services.interactions().input_boundary() !=
        (top ? std::optional{top->overlay} : std::optional<runtime::ComponentId>{})) {
        return;
    }
    if (restore_target_ && can_focus(services, *restore_target_)) {
        const auto target = *restore_target_;
        restore_target_.reset();
        focus_pending_ = false;
        services.focus().defer_focus(target, input::FocusModality::keyboard);
        return;
    }
    restore_target_.reset();
    focus_pending_ = false;
    if (!top) {
        return;
    }
    const auto* s = services.components().state<ModalState>(top->owner);
    if (!s) {
        return;
    }
    std::optional<input::InteractionId> body;
    std::optional<input::InteractionId> ok;
    std::optional<input::InteractionId> other;
    for (auto id : services.interactions().declaration_order()) {
        const auto* record = services.interactions().find(id);
        if (!record || id == s->dialog_hit || !record->tab_stop || !can_focus(services, id)) {
            continue;
        }
        if (!other) {
            other = id;
        }
        if (!body && descendant(services, record->component, s->snapshot.body)) {
            body = id;
        }
        if (record->component == s->snapshot.ok) {
            ok = id;
        }
    }
    services.focus().defer_focus(body    ? body
                                 : ok    ? ok
                                 : other ? other
                                         : std::optional{s->dialog_hit},
                                 input::FocusModality::keyboard);
}

void ModalComponentHost::on_input_boundary_changed() {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<ModalState>(id);
        if (!s || !s->ready || !s->model.visible()) {
            continue;
        }
        bool child_entry = false;
        for (auto hit : services_->interactions().declaration_order()) {
            const auto* record = services_->interactions().find(hit);
            if (record && hit != s->dialog_hit && record->tab_stop && can_focus(*services_, hit) &&
                descendant(*services_, record->component, s->snapshot.dialog)) {
                child_entry = true;
                break;
            }
        }
        services_->interactions().set_tab_stop(s->dialog_hit, !child_entry);
    }
    if (focus_pending_) {
        focus_top();
    } else if (!stack_.empty() && services_->focus().state().window_active && !services_->focus().state().focused) {
        focus_pending_ = true;
        focus_top();
    }
}

void ModalComponentHost::request_ok(runtime::ComponentId id) {
    const auto* s = services_->components().state<ModalState>(id);
    if (!s || stack_.empty() || stack_.back().owner != id || !services_->components().scope(id).active() ||
        !s->model.request_ok()) {
        return;
    }
    const auto callback = s->on_ok;
    if (callback) {
        callback();
    }
}

void ModalComponentHost::request_cancel(runtime::ComponentId id, ModalCancelReason reason) {
    const auto* s = services_->components().state<ModalState>(id);
    if (!s || stack_.empty() || stack_.back().owner != id || !services_->components().scope(id).active() ||
        !s->model.request_cancel(reason)) {
        return;
    }
    const auto callback = s->on_cancel;
    if (callback) {
        callback(reason);
    }
}

void ModalComponentHost::mount(const ModalProps& props, const ModalContent& content, const ModalSlots& slots) {
    mount_panel(props, content, slots, nullptr);
}

void ModalComponentHost::mount(const DrawerProps& props, const DrawerContent& content, const DrawerSlots& slots) {
    validate_drawer_geometry({read_prop(props.placement_), read_prop(props.width_), read_prop(props.height_)});
    ModalProps panel;
    panel.open(props.open_)
        .title(props.title_)
        .closable(props.closable_)
        .keyboard(props.keyboard_)
        .footer(ModalFooterMode::None)
        .restoreFocusAfterClose(props.restore_focus_)
        .mask(bind([mask = props.mask_] {
            const auto value = read_prop(mask);
            return ModalMask{value.enabled, value.closable};
        }))
        .afterClose(props.after_close_)
        .afterOpenChange(props.after_open_change_)
        .onCancel([callback = props.on_cancel_](ModalCancelReason reason) {
            if (!callback) {
                return;
            }
            switch (reason) {
            case ModalCancelReason::CloseButton:
                callback(DrawerCancelReason::CloseButton);
                break;
            case ModalCancelReason::Mask:
                callback(DrawerCancelReason::Mask);
                break;
            case ModalCancelReason::Escape:
                callback(DrawerCancelReason::Escape);
                break;
            case ModalCancelReason::CancelButton:
                break;
            }
        });
    ModalSlots regions;
    if (slots.title) {
        regions.title = ModalTitle{SlotContentAccess::function(*slots.title)};
    }
    if (slots.footer) {
        regions.footer = ModalFooter{SlotContentAccess::function(*slots.footer)};
    }
    mount_panel(panel, ModalContent{SlotContentAccess::function(content)}, regions, &props);
}

void ModalComponentHost::mount_panel(const ModalProps& props, const ModalContent& content, const ModalSlots& slots,
                                     const DrawerProps* drawer) {
    ModalOptions options{read_prop(props.open_), read_prop(props.closable_),        read_prop(props.keyboard_),
                         read_prop(props.mask_), read_prop(props.confirm_loading_), read_prop(props.footer_),
                         read_prop(props.width_)};
    ModalModel validate{options};
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<ModalState>(options);
    auto& s = build.state<ModalState>(id);
    if (drawer) {
        s.drawer = DrawerGeometry{read_prop(drawer->placement_), read_prop(drawer->width_), read_prop(drawer->height_)};
        s.snapshot.drawer = true;
    }
    s.node = build.root(id);
    s.title_slot = slots.title.has_value();
    s.footer_slot = slots.footer.has_value();
    s.centered = read_prop(props.centered_);
    s.restore = read_prop(props.restore_focus_);
    s.title.set(read_prop(props.title_));
    s.ok_text.set(read_prop(props.ok_text_));
    s.cancel_text.set(read_prop(props.cancel_text_));
    s.on_ok = props.on_ok_;
    s.on_cancel = props.on_cancel_;
    s.after_close = props.after_close_;
    s.after_open_change = props.after_open_change_;
    services.layout().set_layout(s.node, layout::LeafLayout{});
    services.nodes().require(s.node).out_of_flow = true;
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    mounted_.push_back(id); // Parent before nested dialogs when settling branches.
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            s.snapshot.overlay = context.mount_component<int>(0);
            const auto overlay_node = context.root(s.snapshot.overlay);
            services.layout().set_layout(overlay_node, layout::LeafLayout{});
            services.components().set_window_layer(s.snapshot.overlay, 1000);
            services.components().set_branch_active(s.snapshot.overlay, false);
            s.shield_hit =
                services.interactions().create({s.snapshot.overlay, overlay_node, {}, true, false, {}, false, false});
            input::InteractionHandlers shield_handlers;
            shield_handlers.target = [this, id](input::PointerDispatchContext& event) {
                const auto* s = services_->components().state<ModalState>(id);
                if (!s) {
                    return;
                }
                const auto shield = s->shield_hit;
                if (event.kind() == input::PointerEventKind::down &&
                    event.event().button == input::PointerButton::primary) {
                    static_cast<void>(event.capture_pointer());
                    event.stop_propagation();
                } else if (event.kind() == input::PointerEventKind::up &&
                           event.event().button == input::PointerButton::primary) {
                    const bool activate = event.press_origin() == shield && event.actual_hit_target() == shield;
                    static_cast<void>(event.release_pointer_capture());
                    event.stop_propagation();
                    if (activate) {
                        request_cancel(id, ModalCancelReason::Mask);
                    }
                }
            };
            services.interactions().set_handlers(s.shield_hit, std::move(shield_handlers));
            auto initial = quad({}, Color::rgba8(0, 0, 0, 0));
            component::RetainedSurfaceEffects no_effect;
            no_effect.focus_enabled = false;
            const auto shield_fragment =
                context.register_scene_fragment(s.snapshot.overlay, runtime::SceneFragmentPlacement::before_children);
            s.shield_surface = services.surfaces().create(s.snapshot.overlay, overlay_node, shield_fragment,
                                                          s.shield_hit, std::span{&initial, 1}, no_effect);
            context.on_resource_cleanup(
                s.snapshot.overlay, [&services, node = overlay_node, hit = s.shield_hit, surface = s.shield_surface] {
                    services.pointer().cancel_interaction(hit);
                    services.interactions().remove(hit);
                    services.surfaces().destroy(surface);
                    services.layout().remove_layout(node);
                });
            context.mount_slot(
                s.snapshot.overlay, Content{[&] {
                    auto& overlay = runtime::require_component_build_context();
                    s.snapshot.dialog = overlay.mount_component<int>(0);
                    const auto dialog_node = overlay.root(s.snapshot.dialog);
                    services.nodes().require(dialog_node).clip_content = true;
                    s.dialog_hit = services.interactions().create(
                        {s.snapshot.dialog, dialog_node, {}, true, true, {}, true, false});
                    input::FocusHandlers focus;
                    focus.state_changed = [this, id](input::FocusPresentation) {
                        if (const auto* s = services_->components().state<ModalState>(id)) {
                            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
                        }
                    };
                    services.interactions().set_focus_handlers(s.dialog_hit, std::move(focus));
                    const auto dialog_fragment = overlay.register_scene_fragment(
                        s.snapshot.dialog, runtime::SceneFragmentPlacement::before_children);
                    const std::array<graphics::QuadInstance, 5> visuals{initial, initial, initial, initial, initial};
                    s.dialog_surface =
                        services.surfaces().create(s.snapshot.dialog, dialog_node, dialog_fragment, s.dialog_hit,
                                                   std::span{visuals}.first(s.drawer ? 5 : 3), no_effect);
                    overlay.on_resource_cleanup(s.snapshot.dialog, [&services, node = dialog_node, hit = s.dialog_hit,
                                                                    surface = s.dialog_surface] {
                        services.pointer().cancel_interaction(hit);
                        services.interactions().remove(hit);
                        services.surfaces().destroy(surface);
                        services.layout().remove_layout(node);
                    });
                    overlay.mount_slot(
                        s.snapshot.dialog, Content{[&] {
                            auto& dialog = runtime::require_component_build_context();
                            const auto region = [&](layout::LayoutModel layout, bool clip = false) {
                                const auto component = dialog.mount_component<int>(0);
                                const auto node = dialog.root(component);
                                services.layout().set_layout(node, std::move(layout));
                                services.nodes().require(node).clip_content = clip;
                                dialog.on_resource_cleanup(
                                    component, [&services, node] { services.layout().remove_layout(node); });
                                return component;
                            };
                            s.snapshot.title = region(layout::BoxLayout{{}, true}, true);
                            dialog.mount_slot_with_semantic_text_style(s.snapshot.title,
                                                                       Content{[title = slots.title, value = s.title] {
                                                                           if (title) {
                                                                               SlotContentAccess::function (*title)();
                                                                           } else {
                                                                               Text(TextProps{}.content(value));
                                                                           }
                                                                       }},
                                                                       Prop<runtime::SemanticForeground>{s.title_color},
                                                                       Prop<runtime::SemanticTypography>{s.title_font});
                            s.snapshot.body_viewport = region(layout::LeafLayout{}, true);
                            dialog.mount_slot(s.snapshot.body_viewport, Content{[&] {
                                                  auto& viewport = runtime::require_component_build_context();
                                                  s.snapshot.body = viewport.mount_component<int>(0);
                                                  const auto node = viewport.root(s.snapshot.body);
                                                  services.layout().set_layout(
                                                      node, layout::FlexLayout{layout::FlexDirection::vertical,
                                                                               0,
                                                                               {},
                                                                               true,
                                                                               false,
                                                                               layout::FlexWrap::no_wrap,
                                                                               layout::FlexJustify::start,
                                                                               layout::FlexAlign::stretch,
                                                                               0,
                                                                               layout::FlexItemPolicy::sequential});
                                                  viewport.on_resource_cleanup(s.snapshot.body, [&services, node] {
                                                      services.layout().remove_layout(node);
                                                  });
                                                  viewport.mount_slot_with_semantic_text_style(
                                                      s.snapshot.body, Content{SlotContentAccess::function(content)},
                                                      Prop<runtime::SemanticForeground>{s.text_color},
                                                      Prop<runtime::SemanticTypography>{s.body_font});
                                              }});
                            s.snapshot.footer = region(layout::BoxLayout{{}, true});
                            dialog.mount_slot(
                                s.snapshot.footer, Content{[&] {
                                    auto& footer = runtime::require_component_build_context();
                                    s.default_footer = footer.mount_component<int>(0);
                                    const auto default_node = footer.root(s.default_footer);
                                    services.layout().set_layout(
                                        default_node, layout::FlexLayout{layout::FlexDirection::horizontal,
                                                                         8,
                                                                         {},
                                                                         true,
                                                                         false,
                                                                         layout::FlexWrap::no_wrap,
                                                                         layout::FlexJustify::end,
                                                                         layout::FlexAlign::center,
                                                                         0,
                                                                         layout::FlexItemPolicy::sequential});
                                    footer.on_resource_cleanup(s.default_footer, [&services, default_node] {
                                        services.layout().remove_layout(default_node);
                                    });
                                    footer.mount_slot(
                                        s.default_footer, Content{[&] {
                                            if (s.drawer) {
                                                return;
                                            }
                                            Button(ButtonProps{}.onClick([this, id] {
                                                request_cancel(id, ModalCancelReason::CancelButton);
                                            }),
                                                   ButtonContent{
                                                       [text = s.cancel_text] { Text(TextProps{}.content(text)); }});
                                            s.snapshot.cancel = services.components().children(s.default_footer).back();
                                            Button(
                                                ButtonProps{}
                                                    .type(ButtonType::Primary)
                                                    .loading(s.loading)
                                                    .onClick([this, id] { request_ok(id); }),
                                                ButtonContent{[text = s.ok_text] { Text(TextProps{}.content(text)); }});
                                            s.snapshot.ok = services.components().children(s.default_footer).back();
                                        }});
                                    s.custom_footer = footer.mount_component<int>(0);
                                    const auto custom_node = footer.root(s.custom_footer);
                                    services.layout().set_layout(custom_node, layout::BoxLayout{{}, true});
                                    footer.on_resource_cleanup(s.custom_footer, [&services, custom_node] {
                                        services.layout().remove_layout(custom_node);
                                    });
                                    footer.mount_slot_with_semantic_text_style(
                                        s.custom_footer, Content{[slot = slots.footer] {
                                            if (slot) {
                                                SlotContentAccess::function (*slot)();
                                            }
                                        }},
                                        Prop<runtime::SemanticForeground>{s.text_color},
                                        Prop<runtime::SemanticTypography>{s.body_font});
                                }});
                            services.layout().set_layout(
                                dialog.root(s.snapshot.footer),
                                layout::ComponentLayout{
                                    [this, id](layout::LayoutEngine& engine, runtime::NodeId,
                                               layout::Constraints limits) {
                                        const auto& state = *services_->components().state<ModalState>(id);
                                        return engine.measure_child(
                                            services_->components().root(state.footer_slot ? state.custom_footer
                                                                                           : state.default_footer),
                                            limits);
                                    },
                                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                                        const auto& state = *services_->components().state<ModalState>(id);
                                        engine.place_child(services_->components().root(state.footer_slot
                                                                                            ? state.custom_footer
                                                                                            : state.default_footer),
                                                           rect);
                                    }});
                            Theme(ThemeProps{}.config(s.close_theme), ThemeContent{[&] {
                                      Button(ButtonProps{}
                                                 .color(ButtonColor::Primary)
                                                 .variant(ButtonVariant::Text)
                                                 .wave(false)
                                                 .onClick([this, id] {
                                                     request_cancel(id, ModalCancelReason::CloseButton);
                                                 }),
                                             ButtonSlots{.icon = ButtonIcon{
                                                             [] { Icon(IconProps{}.name(IconName::CloseOutlined)); }}});
                                      s.snapshot.close = services.components().children(s.snapshot.dialog).back();
                                  }});
                        }});
                    services.layout().set_layout(
                        dialog_node, layout::ComponentLayout{
                                         [this, id](layout::LayoutEngine& engine, runtime::NodeId,
                                                    layout::Constraints limits) { return measure(id, engine, limits); },
                                         [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                                             place(id, engine, rect);
                                         }});
                }});
        }});
    auto& scope = build.scope(id);
    const auto option = [&]<typename T>(const Prop<T>& prop, auto update, bool geometry = false) {
        connect_prop(scope, prop, [this, id, update, geometry](T value) {
            if (auto* s = services_->components().state<ModalState>(id)) {
                auto options = s->model.options();
                update(options, value);
                s->model.reconcile(options);
                refresh(id, geometry);
            }
        });
    };
    option(props.open_, [](auto& options, bool value) { options.open = value; });
    option(props.closable_, [](auto& options, bool value) { options.closable = value; }, true);
    option(props.keyboard_, [](auto& options, bool value) { options.keyboard = value; });
    option(props.mask_, [](auto& options, ModalMask value) { options.mask = value; });
    option(props.confirm_loading_, [](auto& options, bool value) { options.confirm_loading = value; });
    option(props.footer_, [](auto& options, ModalFooterMode value) { options.footer = value; }, true);
    option(props.width_, [](auto& options, float value) { options.width = value; }, true);
    connect_prop(scope, props.title_, [this, id](String value) {
        if (auto* s = services_->components().state<ModalState>(id)) {
            s->title.set(std::move(value));
            refresh(id, true);
        }
    });
    connect_prop(scope, props.ok_text_, [this, id](String value) {
        if (auto* s = services_->components().state<ModalState>(id)) {
            s->ok_text.set(std::move(value));
            refresh(id, true);
        }
    });
    connect_prop(scope, props.cancel_text_, [this, id](String value) {
        if (auto* s = services_->components().state<ModalState>(id)) {
            s->cancel_text.set(std::move(value));
            refresh(id, true);
        }
    });
    connect_prop(scope, props.centered_, [this, id](bool value) {
        if (auto* s = services_->components().state<ModalState>(id)) {
            s->centered = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.restore_focus_, [this, id](bool value) {
        if (auto* s = services_->components().state<ModalState>(id)) {
            s->restore = value;
            for (auto& entry : stack_) {
                if (entry.owner == id) {
                    entry.restore = value;
                }
            }
        }
    });
    const auto theme = build.theme_scope();
    const auto subscribe = [&](const auto& accessors) {
        for (std::size_t index = 0; index < accessors.size(); ++index) {
            s.subscriptions[index] = theme->capture(
                [this, id, geometry = index == 1 || index == 2](theme_runtime::DirtyPhase) { refresh(id, geometry); },
                [theme, accessor = accessors[index]] { static_cast<void>((theme.get()->*accessor)()); });
        }
    };
    if (drawer) {
        const auto geometry = [&]<typename T>(const Prop<T>& prop, auto update) {
            connect_prop(scope, prop, [this, id, update](T value) {
                if (auto* state = services_->components().state<ModalState>(id)) {
                    auto next = *state->drawer;
                    update(next, value);
                    validate_drawer_geometry(next);
                    if (next != *state->drawer) {
                        state->drawer = next;
                        refresh(id, true);
                    }
                }
            });
        };
        geometry(drawer->placement_, [](auto& next, DrawerPlacement value) { next.placement = value; });
        geometry(drawer->width_, [](auto& next, float value) { next.width = value; });
        geometry(drawer->height_, [](auto& next, float value) { next.height = value; });
        subscribe(std::array{&theme_runtime::ThemeScope::drawer_colors, &theme_runtime::ThemeScope::drawer_metrics,
                             &theme_runtime::ThemeScope::drawer_typography, &theme_runtime::ThemeScope::drawer_effects,
                             &theme_runtime::ThemeScope::drawer_order});
    } else {
        subscribe(std::array{&theme_runtime::ThemeScope::modal_colors, &theme_runtime::ThemeScope::modal_metrics,
                             &theme_runtime::ThemeScope::modal_typography, &theme_runtime::ThemeScope::modal_effects,
                             &theme_runtime::ThemeScope::modal_order});
    }
    s.ready = true;
    refresh(id, true);
}

runtime::Size ModalComponentHost::measure_drawer(runtime::ComponentId id, layout::LayoutEngine& engine,
                                                 layout::Constraints limits) {
    auto& s = *services_->components().state<ModalState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().drawer().metrics;
    const float width = limits.max_width;
    const float height = limits.max_height;
    const float infinity = std::numeric_limits<float>::infinity();
    const auto node = [&](runtime::ComponentId component) {
        return services_->components().root(component);
    };
    const float header_x = std::min(m.header_padding_inline, width / 2);
    const float reserve = s.model.options().closable ? m.close_size + m.close_gap : 0;
    float title_height{};
    if (services_->components().branch_active(s.snapshot.title)) {
        title_height =
            engine
                .measure_child(node(s.snapshot.title), {0, std::max(0.0F, width - header_x * 2 - reserve), 0, infinity})
                .height;
        if (s.model.options().closable) {
            const float size = std::min(m.close_size, std::min(width, height));
            static_cast<void>(engine.measure_child(node(s.snapshot.close), layout::Constraints::fixed(size, size)));
            title_height = std::max(title_height, size);
        }
        s.header_height = std::min(height, title_height + m.header_padding_block * 2 + m.border_width);
    } else {
        s.header_height = 0;
    }
    s.footer_height = 0;
    if (s.footer_slot) {
        const float inner_width = std::max(0.0F, width - m.footer_padding_inline * 2);
        const float natural =
            engine.measure_child(node(s.snapshot.footer), {inner_width, inner_width, 0, infinity}).height;
        s.footer_height =
            std::min(std::max(0.0F, height - s.header_height), natural + m.footer_padding_block * 2 + m.border_width);
    }
    s.padding_x = std::min(m.padding_inline, width / 2);
    const float remaining = std::max(0.0F, height - s.header_height - s.footer_height);
    s.padding_y = std::min(m.padding_block, remaining / 2);
    const float inner_width = std::max(0.0F, width - s.padding_x * 2);
    const float content_height =
        engine.measure_child(node(s.snapshot.body), {inner_width, inner_width, 0, infinity}).height;
    s.reveal = s.reveal || s.content_height != content_height;
    s.content_height = content_height;
    s.body_height = std::max(0.0F, remaining - s.padding_y * 2);
    static_cast<void>(
        engine.measure_child(node(s.snapshot.body_viewport), layout::Constraints::fixed(inner_width, s.body_height)));
    settle_scroll_metrics(*services_, s);
    return limits.constrain({width, height});
}

void ModalComponentHost::place_drawer(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect rect) {
    auto& s = *services_->components().state<ModalState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().drawer().metrics;
    const auto node = [&](runtime::ComponentId component) {
        return services_->components().root(component);
    };
    s.header_bounds = {rect.x, rect.y, rect.width, s.header_height};
    s.footer_bounds = {rect.x, rect.y + rect.height - s.footer_height, rect.width, s.footer_height};
    const float header_x = std::min(m.header_padding_inline, rect.width / 2);
    const float header_y = std::min(m.header_padding_block, std::max(0.0F, s.header_height - m.border_width) / 2);
    const float title_height = std::max(0.0F, s.header_height - m.border_width - header_y * 2);
    const float reserve = s.model.options().closable ? m.close_size + m.close_gap : 0;
    if (services_->components().branch_active(s.snapshot.title)) {
        const float start = std::min(rect.width - header_x, header_x + reserve);
        engine.place_child(node(s.snapshot.title), {rect.x + start, rect.y + header_y,
                                                    std::max(0.0F, rect.width - start - header_x), title_height});
    }
    if (s.model.options().closable) {
        const float size = std::min(m.close_size, std::min(rect.width, s.header_height));
        const float top =
            std::clamp(header_y + (title_height - size) / 2, 0.0F, std::max(0.0F, s.header_height - size));
        engine.place_child(node(s.snapshot.close),
                           {rect.x + std::min(header_x, rect.width - size), rect.y + top, size, size});
    }
    s.snapshot.body_bounds = {rect.x + s.padding_x, rect.y + s.header_height + s.padding_y,
                              std::max(0.0F, rect.width - s.padding_x * 2), s.body_height};
    engine.place_child(node(s.snapshot.body_viewport), s.snapshot.body_bounds);
    engine.place_child(node(s.snapshot.body), {s.snapshot.body_bounds.x, s.snapshot.body_bounds.y,
                                               s.snapshot.body_bounds.width, s.content_height});
    if (s.footer_slot) {
        const float padding_x = std::min(m.footer_padding_inline, rect.width / 2);
        const float border = std::min(m.border_width, s.footer_height);
        const float padding_y = std::min(m.footer_padding_block, (s.footer_height - border) / 2);
        engine.place_child(node(s.snapshot.footer), {rect.x + padding_x, s.footer_bounds.y + border + padding_y,
                                                     std::max(0.0F, rect.width - padding_x * 2),
                                                     std::max(0.0F, s.footer_height - border - padding_y * 2)});
    }
}

runtime::Size ModalComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                          layout::Constraints limits) {
    auto& s = *services_->components().state<ModalState>(id);
    if (s.drawer) {
        return measure_drawer(id, engine, limits);
    }
    const auto& metrics = services_->components().theme_scope(id)->snapshot().modal().metrics;
    const float width = limits.max_width;
    s.padding_x = std::min(metrics.padding_inline, width / 2);
    s.padding_y = std::min(metrics.padding_block, limits.max_height / 2);
    const float inner_width = std::max(0.0F, width - s.padding_x * 2);
    const float inner_height = std::max(0.0F, limits.max_height - s.padding_y * 2);
    const auto node = [&](runtime::ComponentId component) {
        return services_->components().root(component);
    };
    const float infinity = std::numeric_limits<float>::infinity();
    float title_height = 0;
    if (services_->components().branch_active(s.snapshot.title)) {
        const float reserve =
            s.model.options().closable
                ? std::max(0.0F, metrics.close_inset + metrics.close_size + metrics.header_gap - s.padding_x)
                : 0;
        title_height =
            engine.measure_child(node(s.snapshot.title), {0, std::max(0.0F, inner_width - reserve), 0, infinity})
                .height;
    }
    if (s.model.options().closable) {
        const float size = std::min(metrics.close_size, std::min(width, limits.max_height));
        static_cast<void>(engine.measure_child(node(s.snapshot.close), layout::Constraints::fixed(size, size)));
        title_height = std::max(title_height, std::max(0.0F, metrics.close_inset + size - s.padding_y));
    }
    float footer_height = 0;
    if (services_->components().branch_active(s.snapshot.footer)) {
        services_->layout().set_layout(node(s.default_footer), layout::FlexLayout{layout::FlexDirection::horizontal,
                                                                                  metrics.button_gap,
                                                                                  {},
                                                                                  true,
                                                                                  false,
                                                                                  layout::FlexWrap::no_wrap,
                                                                                  layout::FlexJustify::end,
                                                                                  layout::FlexAlign::center,
                                                                                  0,
                                                                                  layout::FlexItemPolicy::sequential});
        footer_height = engine.measure_child(node(s.snapshot.footer), {inner_width, inner_width, 0, infinity}).height;
    }
    s.footer_height = std::min(footer_height, inner_height);
    float remaining = std::max(0.0F, inner_height - s.footer_height);
    s.footer_gap = s.footer_height > 0 ? std::min(metrics.footer_gap, remaining) : 0;
    remaining -= s.footer_gap;
    s.header_height = std::min(title_height, remaining);
    remaining -= s.header_height;
    s.header_gap = s.header_height > 0 ? std::min(metrics.header_gap, remaining) : 0;
    remaining -= s.header_gap;
    const auto content_height =
        engine.measure_child(node(s.snapshot.body), {inner_width, inner_width, 0, infinity}).height;
    s.reveal = s.reveal || s.content_height != content_height;
    s.content_height = content_height;
    s.body_height = std::min(s.content_height, remaining);
    static_cast<void>(
        engine.measure_child(node(s.snapshot.body_viewport), layout::Constraints::fixed(inner_width, s.body_height)));
    settle_scroll_metrics(*services_, s);
    return limits.constrain(
        {width, s.padding_y * 2 + s.header_height + s.header_gap + s.body_height + s.footer_gap + s.footer_height});
}

void ModalComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect rect) {
    auto& s = *services_->components().state<ModalState>(id);
    if (s.drawer) {
        place_drawer(id, engine, rect);
        return;
    }
    const auto& metrics = services_->components().theme_scope(id)->snapshot().modal().metrics;
    const auto node = [&](runtime::ComponentId component) {
        return services_->components().root(component);
    };
    const float width = std::max(0.0F, rect.width - s.padding_x * 2);
    float y = rect.y + s.padding_y;
    const float reserve =
        s.model.options().closable
            ? std::max(0.0F, metrics.close_inset + metrics.close_size + metrics.header_gap - s.padding_x)
            : 0;
    s.header_bounds = {rect.x + s.padding_x, y, width, s.header_height};
    if (services_->components().branch_active(s.snapshot.title)) {
        engine.place_child(node(s.snapshot.title),
                           {s.header_bounds.x, y, std::max(0.0F, width - reserve), s.header_height});
    }
    y += s.header_height + s.header_gap;
    const runtime::Rect body_bounds{rect.x + s.padding_x, y, width, s.body_height};
    s.reveal = s.reveal || s.snapshot.body_bounds != body_bounds;
    s.snapshot.body_bounds = body_bounds;
    engine.place_child(node(s.snapshot.body_viewport), s.snapshot.body_bounds);
    engine.place_child(node(s.snapshot.body), {s.snapshot.body_bounds.x, y, width, s.content_height});
    y += s.body_height + s.footer_gap;
    s.footer_bounds = {rect.x + s.padding_x, y, width, s.footer_height};
    if (services_->components().branch_active(s.snapshot.footer)) {
        engine.place_child(node(s.snapshot.footer), s.footer_bounds);
    }
    if (s.model.options().closable) {
        const float size = std::min(metrics.close_size, std::min(rect.width, rect.height));
        const float inset_x = std::min(metrics.close_inset, std::max(0.0F, rect.width - size));
        const float inset_y = std::min(metrics.close_inset, std::max(0.0F, rect.height - size));
        engine.place_child(node(s.snapshot.close),
                           {rect.x + rect.width - inset_x - size, rect.y + inset_y, size, size});
    }
}

bool ModalComponentHost::scroll_to(runtime::ComponentId id, float value) {
    auto* s = services_->components().state<ModalState>(id);
    if (!s) {
        return false;
    }
    const float next = std::clamp(std::round(value * s->scale) / s->scale, 0.0F, s->snapshot.maximum_scroll);
    if (s->snapshot.scroll == next) {
        return false;
    }
    s->snapshot.scroll = next;
    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    return true;
}

bool ModalComponentHost::reveal_focus(runtime::ComponentId id, bool caret_ready) {
    auto* s = services_->components().state<ModalState>(id);
    const auto focus = services_->focus().state().focused;
    const auto* hit = focus ? services_->interactions().find(*focus) : nullptr;
    if (!s || !hit || !services_->focus().state().window_active ||
        !descendant(*services_, hit->component, s->snapshot.body)) {
        return false;
    }
    for (auto current = std::optional{hit->component}; current && *current != s->snapshot.body;
         current = services_->components().parent(*current)) {
        if (services_->components().is_window_layer_root(*current)) {
            return false;
        }
    }
    auto target = bounds(*services_, hit->component);
    bool reveal = s->reveal || s->revealed_focus != focus;
    if (caret_ready && services_->input_runtime()) {
        for (const auto& input : services_->input_runtime()->mounted_inputs()) {
            if (input.interaction != *focus) {
                continue;
            }
            const auto caret = services_->input_runtime()->layout_snapshot(input.component).caret;
            const auto scene = services_->input_runtime()->text_scene(input.component);
            const auto revision = services_->text().scene_service().text_state(scene).revision();
            const auto byte = services_->input_runtime()->display_snapshot(input.component).caret;
            reveal = reveal || s->revealed_text_revision != revision || s->revealed_caret_byte != byte;
            s->revealed_text_revision = revision;
            s->revealed_caret_byte = byte;
            if (target.height > s->snapshot.body_bounds.height) {
                target = caret;
            }
            break;
        }
    }
    if (!reveal) {
        return false;
    }
    s->revealed_focus = focus;
    s->reveal = false;
    const auto viewport = s->snapshot.body_bounds;
    float next = s->snapshot.scroll;
    if (target.y < viewport.y) {
        next += std::floor((target.y - viewport.y) * s->scale) / s->scale;
    } else if (target.y + target.height > viewport.y + viewport.height) {
        next +=
            std::ceil((target.y + std::min(target.height, viewport.height) - viewport.y - viewport.height) * s->scale) /
            s->scale;
    }
    return scroll_to(id, next);
}

void ModalComponentHost::prepare_auxiliary_layout() {
    reconcile();
    on_input_boundary_changed();
    // MSVC Debug's vector move constructs a container proxy even for an empty
    // vector. Stable frames must not allocate just to find no notifications.
    if (transitions_.empty()) {
        return;
    }
    auto events = std::move(transitions_);
    transitions_.clear();
    notifying_ = true;
    try {
        for (const auto& event : events) {
            const auto* s = services_->components().state<ModalState>(event.owner);
            if (!s || !services_->components().scope(event.owner).active()) {
                continue;
            }
            const auto changed = s->after_open_change;
            const auto closed = s->after_close;
            if (changed) {
                changed(event.visible);
            }
            if (!event.visible && closed && services_->components().contains(event.owner) &&
                services_->components().scope(event.owner).active()) {
                closed();
            }
        }
        notifying_ = false;
    } catch (...) {
        notifying_ = false;
        throw;
    }
}

void ModalComponentHost::position_window_layers(runtime::Size, runtime::Rect clip) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<ModalState>(id);
        if (!s || !s->model.visible()) {
            continue;
        }
        const auto token = panel_theme(services_->components().theme_scope(id)->snapshot(), s->drawer.has_value());
        const auto overlay = services_->components().root(s->snapshot.overlay);
        const auto dialog = services_->components().root(s->snapshot.dialog);
        auto& engine = services_->layout();
        const bool layout = s->needs_layout || s->layout_clip != clip ||
                            services_->nodes().require(dialog).measure_generation != engine.generation();
        if (layout) {
            const float margin_x = std::min(token.metrics.viewport_margin, clip.width / 2);
            const float margin_y = std::min(token.metrics.viewport_margin, clip.height / 2);
            const float width = std::min(s->model.options().width, std::max(0.0F, clip.width - margin_x * 2));
            const float maximum_height = std::max(0.0F, clip.height - margin_y * 2);
            static_cast<void>(engine.measure_child(overlay, layout::Constraints::fixed(clip.width, clip.height)));
            engine.place_child(overlay, clip);
            if (s->drawer) {
                s->snapshot.bounds = drawer_bounds(*s->drawer, clip);
                static_cast<void>(engine.measure_child(
                    dialog, layout::Constraints::fixed(s->snapshot.bounds.width, s->snapshot.bounds.height)));
            } else {
                const auto measured = engine.measure_child(dialog, {width, width, 0, maximum_height});
                const float top = s->centered
                                      ? (clip.height - measured.height) / 2
                                      : std::clamp(token.metrics.top_offset, margin_y,
                                                   std::max(margin_y, clip.height - margin_y - measured.height));
                s->snapshot.bounds = {clip.x + (clip.width - measured.width) / 2, clip.y + top, measured.width,
                                      measured.height};
            }
            engine.place_child(dialog, s->snapshot.bounds);
            s->needs_layout = false;
            s->reveal = s->reveal || s->layout_clip != clip;
            s->layout_clip = clip;
        }
        static_cast<void>(reveal_focus(id, false));
        translate(services_->components().root(s->snapshot.body), {0, -s->snapshot.scroll}, *services_);
    }
}

void ModalComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect) {
    const runtime::Rect window{0, 0, viewport.width, viewport.height};
    for (auto id : mounted_) {
        const auto* s = services_->components().state<ModalState>(id);
        if (!s || !s->model.visible()) {
            continue;
        }
        const auto token = panel_theme(services_->components().theme_scope(id)->snapshot(), s->drawer.has_value());
        const auto shield =
            quad(window, s->model.options().mask.enabled ? token.colors.mask : Color::rgba8(0, 0, 0, 0));
        static_cast<void>(services_->surfaces().update_surface(s->shield_surface, std::span{&shield, 1}));
        const auto rect = s->snapshot.bounds;
        const float radius = std::min(token.metrics.border_radius, std::min(rect.width, rect.height) / 2);
        const std::array visuals{quad(rect, token.colors.background, radius),
                                 quad(s->header_bounds, token.colors.header_background),
                                 quad(s->footer_bounds, token.colors.footer_background)};
        if (s->drawer) {
            const auto& drawer = services_->components().theme_scope(id)->snapshot().drawer();
            const float border = std::min(drawer.metrics.border_width, rect.height);
            const std::array drawer_visuals{
                visuals[0], visuals[1], visuals[2],
                quad({rect.x, s->header_bounds.y + s->header_bounds.height - border, rect.width,
                      s->header_height > 0 ? border : 0},
                     drawer.border),
                quad({rect.x, s->footer_bounds.y, rect.width, s->footer_height > 0 ? border : 0}, drawer.border)};
            static_cast<void>(services_->surfaces().update_surface(s->dialog_surface, drawer_visuals));
        } else {
            static_cast<void>(services_->surfaces().update_surface(s->dialog_surface, visuals));
        }
        component::RetainedSurfaceEffects effects;
        effects.shape = {rect, std::min(token.effects.focus_radius, std::min(rect.width, rect.height) / 2)};
        effects.shadow_shape = graphics::LogicalRoundedRect{rect, radius};
        effects.shadows = token.effects.shadow;
        effects.focus_color = token.colors.focus_outline;
        effects.focus_width = std::max(1.0F, token.effects.focus_width);
        effects.focus_offset = token.effects.focus_offset;
        const auto focus = services_->focus().state();
        effects.focus_opacity =
            focus.focused == s->dialog_hit && focus.focus_visible && token.effects.focus_width > 0 ? 1.0F : 0.0F;
        effects.ancestor_clip = graphics::EffectClip{1, window};
        static_cast<void>(services_->surfaces().update_effects(s->dialog_surface, effects));
    }
}

bool ModalComponentHost::synchronize_auxiliary_fragments() {
    if (!stack_.empty() && reveal_focus(stack_.back().owner, true)) {
        // Input publishes its fresh caret after layer positioning. A transfer
        // beyond the body clip needs one follow-up translation, never a timer.
        services_->request_frame();
    }
    return false;
}

bool ModalComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (stack_.empty() || !services_->focus().state().window_active) {
        return false;
    }
    if (event.key == input::Key::escape) {
        if (event.action == input::KeyAction::down && !event.repeat) {
            request_cancel(stack_.back().owner, ModalCancelReason::Escape);
        }
        return true;
    }
    return false;
}

bool ModalComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    if (stack_.empty()) {
        return false;
    }
    const auto id = stack_.back().owner;
    const auto* s = services_->components().state<ModalState>(id);
    if (s && inside(s->snapshot.body_bounds, event.x, event.y)) {
        static_cast<void>(scroll_to(id, s->snapshot.scroll - event.delta_y * 48));
    }
    return true;
}

void ModalComponentHost::on_window_active(bool active) {
    if (active && (focus_pending_ || (!stack_.empty() && !services_->focus().state().focused))) {
        focus_pending_ = true;
        focus_top();
    }
}
} // namespace ryn::detail

namespace ryn {
void Drawer(DrawerProps props, DrawerContent content, DrawerSlots slots) {
    if (!detail::active_modals) {
        throw std::logic_error("Drawer requires window component services");
    }
    detail::active_modals->mount(props, content, slots);
}

void Modal(ModalProps props, ModalContent content, ModalSlots slots) {
    if (!detail::active_modals) {
        throw std::logic_error("Modal requires window component services");
    }
    detail::active_modals->mount(props, content, slots);
}
} // namespace ryn
