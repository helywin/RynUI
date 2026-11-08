#include "component/config_defaults.hpp"
#include "component/modal_component.hpp"
#include "component/localized_prop.hpp"
#include "component/confirm_action.hpp"
#include "component/modal_model.hpp"
#include "component/drawer_model.hpp"
#include "component/data_display_retained.hpp"
#include <ryn/skeleton.hpp>
#include "component/button_component.hpp"
#include "component/input_component.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/icon.hpp>
#include <algorithm>
#include <thread>
#include <utility>
#include <cmath>
#include <limits>
#include <exception>

namespace ryn::detail {
namespace {
thread_local ModalComponentHost* active_modals{};

struct DrawerPresentation final {
    float progress{};
    float from{};
    bool target{};
    bool running{};
    std::uint64_t revision{};
    animation::AnimationTime start;
    animation::AnimationSpec spec;
    std::optional<animation::AnimationTime> deadline;
    std::optional<std::uint64_t> completion;
    std::optional<std::uint64_t> geometry;
    std::uint64_t geometry_sync{};

    bool painted() const {
        return target || running;
    }
};

struct ModalState final {
    explicit ModalState(ModalOptions options) : model(options) {}

    ModalModel model;
    std::optional<DrawerGeometry> drawer;
    DrawerPresentation presentation;
    theme_runtime::Subscription motion_subscription;
    Content drawer_content{[] {}};
    bool drawer_lazy{};
    bool drawer_destroy{};
    bool drawer_rtl{};
    bool drawer_extra{};
    DrawerResize drawer_resize{false};
    DrawerPush drawer_push;
    input::InteractionId resize_hit;
    component::RetainedSurfaceId resize_surface;
    float resize_origin{};
    float resize_extent{};
    float resize_requested{};
    float drawer_push_offset{};
    runtime::Point drawer_family_offset;
    bool drawer_syncing{};
    std::uint64_t refresh_revision{};
    bool keyboard_resizing{};
    runtime::Size extra_size;
    std::function<void(float)> on_resize;
    std::function<void(float)> on_resize_end;
    ModalSnapshot snapshot;
    runtime::NodeId node;
    runtime::ComponentId default_footer;
    runtime::ComponentId custom_footer;
    input::InteractionId shield_hit;
    input::InteractionId dialog_hit;
    component::RetainedSurfaceId shield_surface;
    runtime::SceneFragmentId shield_fragment;
    component::RetainedSurfaceId dialog_surface;
    Signal<String> title{String{}};
    Signal<String> ok_text{String{u8"OK"}};
    Signal<String> cancel_text{String{u8"Cancel"}};
    std::uint64_t label_revision{};
    Signal<bool> loading{false};
    bool user_loading{};
    bool action_starting{};
    ConfirmAction action_source;
    std::function<bool()> action_live;
    std::shared_ptr<ConfirmActionSession> action;
    std::function<void(ConfirmActionError)> action_error;
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
    bool show_cancel{true};
    bool restore{true};
    bool focus_trap{true};
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
    std::optional<WindowOverlayTicket> overlay_ticket;
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
    if (services.nodes().require(node).owns_child_translation) {
        return;
    }
    for (auto child : services.nodes().require(node).children) {
        translate(child, offset, services);
    }
}

bool inside(runtime::Rect rect, float x, float y) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
}

runtime::Point drawer_exit(const DrawerGeometry& geometry, runtime::Rect rect, float progress) {
    switch (geometry.placement) {
    case DrawerPlacement::Left:
        return {-(1 - progress) * rect.width, 0};
    case DrawerPlacement::Right:
        return {(1 - progress) * rect.width, 0};
    case DrawerPlacement::Top:
        return {0, -(1 - progress) * rect.height};
    case DrawerPlacement::Bottom:
        return {0, (1 - progress) * rect.height};
    }
    return {};
}

bool paint_visible(const ModalState& state) {
    return state.drawer ? state.presentation.painted() : state.model.visible();
}
} // namespace

ModalComponentHost::ModalComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

ModalComponentHost::~ModalComponentHost() {
    lifetime_.reset();
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
    result.topmost = services_->overlay_stack().is_top(id);
    result.paint_visible = paint_visible(*state);
    result.progress = state->drawer ? state->presentation.progress : (result.visible ? 1 : 0);
    result.transitioning = state->drawer && state->presentation.running;
    result.motion_revision = state->presentation.revision;
    return result;
}

void ModalComponentHost::on_dispose() noexcept {
    lifetime_.reset();
    mounted_.clear();
    transitions_.clear();
    drawer_clock_.reset();
}

void ModalComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
    std::erase_if(transitions_, [this](const auto& event) { return !services_->components().contains(event.owner); });
}

void ModalComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    const auto retained = services_->components().retain_state<ModalState>(id);
    auto* s = retained.get();
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto life = std::weak_ptr{lifetime_};
    const auto revision = ++s->refresh_revision;
    if (s->drawer && s->presentation.completion) {
        s->presentation.geometry.reset();
    }
    const auto current = [&] {
        return !life.expired() && services_->components().contains(id) && services_->components().scope(id).active() &&
               s->refresh_revision == revision;
    };
    const auto token = panel_theme(services_->components().theme_scope(id)->snapshot(), s->drawer.has_value());
    const auto& fonts = token.typography;
    s->text_color.set(foreground(token.colors.text));
    if (!current()) {
        return;
    }
    s->title_color.set(foreground(token.colors.title));
    if (!current()) {
        return;
    }
    s->body_font.set({fonts.font_family, fonts.font_weight, false, fonts.font_size, fonts.line_height});
    if (!current()) {
        return;
    }
    s->title_font.set(
        {fonts.font_family, fonts.title_font_weight, false, fonts.title_font_size, fonts.title_line_height});
    if (!current()) {
        return;
    }
    s->loading.set(s->model.options().confirm_loading || s->action_starting || (s->action && s->action->pending()));
    if (!current()) {
        return;
    }
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
    if (!current()) {
        return;
    }
    const bool header = s->title_slot || !s->title.get().empty() || s->model.options().closable || s->drawer_extra;
    const bool footer = s->footer_slot || s->model.options().footer == ModalFooterMode::Default;
    for (const auto& [component, shown] :
         {std::pair{s->snapshot.title, header}, std::pair{s->snapshot.close, s->model.options().closable},
          std::pair{s->snapshot.footer, footer}, std::pair{s->default_footer, footer && !s->footer_slot},
          std::pair{s->custom_footer, s->footer_slot}}) {
        if (services_->components().set_branch_active(component, shown)) {
            services_->mark_scene_structure_dirty();
        }
    }
    if (geometry) {
        s->needs_layout = true;
        s->reveal = true;
    }
    if (s->drawer && s->ready) {
        reconcile_drawer_content(id);
        if (!current()) {
            return;
        }
        s = services_->components().state<ModalState>(id);
        if (!s) {
            return;
        }
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
    if (s->ready && !mounting_) {
        reconcile();
    }
}

void ModalComponentHost::update_overlay(runtime::ComponentId id) {
    const auto* state = services_->components().state<ModalState>(id);
    if (!state || !state->overlay_ticket) {
        return;
    }
    WindowOverlayState overlay;
    overlay.input_visible = state->ready && state->model.visible();
    overlay.paint_visible = state->ready && paint_visible(*state);
    if (state->drawer) {
        services_->interactions().set_input_blocked(state->snapshot.overlay,
                                                    !overlay.input_visible || !state->model.options().open);
    }
    overlay.focus_trap = state->focus_trap;
    overlay.restore_focus = state->restore;
    overlay.layer_priority =
        panel_theme(services_->components().theme_scope(id)->snapshot(), state->drawer.has_value()).z_index_popup;
    overlay.body = state->snapshot.body;
    overlay.primary_action = state->snapshot.ok;
    overlay.fallback_focus = state->dialog_hit;
    services_->overlay_stack().update(*state->overlay_ticket, overlay);
}

bool ModalComponentHost::visible() const noexcept {
    return std::ranges::any_of(services_->overlay_stack().ordered(),
                               [](const auto& entry) { return entry.kind != WindowOverlayKind::FullscreenSpin; });
}

void ModalComponentHost::sample_drawers(animation::AnimationTime time) {
    const auto now = drawer_clock_.observe(time).effective;
    for (const auto id : mounted_) {
        auto* state = services_->components().state<ModalState>(id);
        if (!state || !state->drawer || !state->presentation.running) {
            continue;
        }
        auto& motion = state->presentation;
        const auto policy = animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                                             services_->motion_preference());
        const auto interval = animation::sample_animation_interval(now, motion.start, {}, motion.spec.duration);
        const bool finished = !policy.enabled() || !services_->focus().state().window_active ||
                              interval.phase == animation::AnimationIntervalPhase::completed;
        const auto progress = finished ? (motion.target ? 1.0F : 0.0F)
                                       : std::lerp(motion.from, motion.target ? 1.0F : 0.0F,
                                                   motion.spec.easing.sample(interval.progress));
        const bool changed = motion.progress != progress || finished;
        motion.progress = progress;
        motion.running = !finished;
        motion.deadline = finished ? std::nullopt
                                   : std::optional{std::min(now + animation::AnimationDuration::milliseconds(16),
                                                            motion.start + motion.spec.duration)};
        if (finished) {
            motion.completion = motion.revision;
        }
        if (changed) {
            state->needs_layout = true;
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest |
                                                           runtime::DirtyFlags::Material);
            services_->request_frame();
        }
    }
}

void ModalComponentHost::retarget_drawer(runtime::ComponentId id, bool open, bool suppress) {
    auto* state = services_->components().state<ModalState>(id);
    auto& motion = state->presentation;
    const auto now = drawer_clock_.observe(services_->animation_time()).effective;
    ++motion.revision;
    motion.completion.reset();
    motion.geometry.reset();
    motion.from = motion.progress;
    motion.target = open;
    motion.start = now;
    const auto policy = animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                                         services_->motion_preference());
    motion.spec = policy.transition(animation::MotionDurationToken::slow, animation::MotionEasingToken::ease_in_out);
    const auto distance = std::abs((open ? 1.0F : 0.0F) - motion.from);
    const auto duration = static_cast<animation::AnimationDuration::rep>(
        std::ceil(static_cast<double>(motion.spec.duration.count_microseconds()) * distance));
    motion.spec.duration = animation::AnimationDuration::microseconds(duration);
    motion.running = !suppress && duration > 0 && services_->focus().state().window_active &&
                     services_->overlay_stack().logical_branch_active(id);
    if (motion.running) {
        motion.deadline = std::min(now + animation::AnimationDuration::milliseconds(16), now + motion.spec.duration);
    } else {
        motion.progress = open ? 1.0F : 0.0F;
        motion.deadline.reset();
        if (!suppress) {
            motion.completion = motion.revision;
        }
    }
    // Publish the entire subtree block before the stack can revoke its last
    // Restrict lease. This low-level setter cannot invoke application code.
    services_->interactions().set_input_blocked(state->snapshot.overlay, !open);
    if (!open) {
        state->snapshot.resizing = false;
        state->keyboard_resizing = false;
    }
    state->needs_layout = true;
    services_->mark_scene_structure_dirty();
    services_->request_frame();
}

void ModalComponentHost::reconcile() {
    if (mounting_ || !lifetime_) {
        return;
    }
    if (reconciling_) {
        reconcile_again_ = true;
        return;
    }
    const auto life = std::weak_ptr{lifetime_};
    reconciling_ = true;
    try {
        unsigned rounds{};
        do {
            if (++rounds > 32) {
                throw std::logic_error("Drawer visibility callbacks did not converge");
            }
            reconcile_again_ = false;
            sample_drawers(services_->animation_time());
            std::optional<std::vector<runtime::ComponentId>> snapshot;
            if (std::ranges::any_of(mounted_, [this](auto id) {
                    const auto* state = services_->components().state<ModalState>(id);
                    return state && (state->drawer || (state->action && state->action->pending() &&
                                                       (!state->model.options().open ||
                                                        !services_->overlay_stack().logical_branch_active(id))));
                })) {
                snapshot = mounted_;
            }
            const auto ids = snapshot ? std::span<const runtime::ComponentId>{*snapshot}
                                      : std::span<const runtime::ComponentId>{mounted_};
            for (auto id : ids) {
                auto* s = services_->components().state<ModalState>(id);
                if (!s || !s->ready || !services_->components().scope(id).active()) {
                    continue;
                }
                if (const auto change =
                        s->model.settle_visibility(services_->overlay_stack().logical_branch_active(id))) {
                    if (!*change && s->action && s->action->pending()) {
                        auto cancel = s->action->detach_cancel();
                        s->loading.set(s->user_loading);
                        ConfirmActionSession::run_cancel(std::move(cancel));
                        if (life.expired()) {
                            return;
                        }
                        s = services_->components().state<ModalState>(id);
                        if (!s || !services_->components().scope(id).active()) {
                            reconcile_again_ = true;
                            break;
                        }
                    }
                    s->needs_layout = true;
                    s->reveal = true;
                    s->revealed_focus.reset();
                    s->revealed_text_revision.reset();
                    s->revealed_caret_byte.reset();
                    if (s->drawer) {
                        bool family_hidden{};
                        for (auto parent = services_->components().parent(id); parent;
                             parent = services_->components().parent(*parent)) {
                            const auto* ancestor = services_->components().state<ModalState>(*parent);
                            if (ancestor && ancestor->drawer && !paint_visible(*ancestor)) {
                                family_hidden = true;
                                break;
                            }
                        }
                        retarget_drawer(id, *change, !*change && family_hidden && s->model.options().open);
                    } else {
                        transitions_.push_back({id, *change});
                    }
                    if (notifying_) {
                        services_->request_frame();
                    }
                }
                if (s->drawer && s->presentation.running && !services_->overlay_stack().logical_branch_active(id)) {
                    s->presentation.running = false;
                    s->presentation.progress = 0;
                    s->presentation.deadline.reset();
                    s->presentation.completion = s->presentation.revision;
                    s->needs_layout = true;
                    services_->request_frame();
                }
                update_overlay(id);
                if (s->drawer) {
                    reconcile_drawer_content(id);
                    if (life.expired()) {
                        return;
                    }
                }
            }
            static_cast<void>(services_->overlay_stack().reconcile(false));
            if (life.expired()) {
                return;
            }
            on_input_boundary_changed();
        } while (reconcile_again_);
        reconciling_ = false;
    } catch (...) {
        if (!life.expired()) {
            reconciling_ = false;
        }
        throw;
    }
}

std::optional<runtime::ComponentId> ModalComponentHost::effective_input_domain() const noexcept {
    return services_->overlay_stack().input_domain();
}

ModalInputCompanion ModalComponentHost::register_tour_controls(input::InputRootRef owner,
                                                               std::vector<input::InputRootRef> controls, bool active) {
    return services_->overlay_stack().register_tour_controls(owner, std::move(controls), active);
}

void ModalComponentHost::on_input_boundary_changed() {
    const auto life = std::weak_ptr{lifetime_};
    if (!reconciling_ && !mounting_) {
        reconcile();
    }
    if (life.expired()) {
        return;
    }
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
}

void ModalComponentHost::request_ok(runtime::ComponentId id) {
    const auto* s = services_->components().state<ModalState>(id);
    if (!s || !services_->overlay_stack().is_top(id) || !services_->components().scope(id).active() ||
        !s->model.request_ok() || s->action_starting || (s->action && s->action->pending()) ||
        (s->action_live && !s->action_live())) {
        return;
    }
    const auto callback = s->on_ok;
    if (s->action_source) {
        const auto action = s->action;
        const auto source = s->action_source;
        const auto loading = s->loading;
        services_->components().state<ModalState>(id)->action_starting = true;
        const auto life = std::weak_ptr{lifetime_};
        loading.set(true);
        if (life.expired()) {
            return;
        }
        s = services_->components().state<ModalState>(id);
        if (s && services_->components().scope(id).active()) {
            services_->components().state<ModalState>(id)->action_starting = false;
            if (s->model.visible()) {
                services_->request_frame();
                action->start(source, services_->animation_time());
            } else {
                loading.set(s->user_loading);
            }
        }
        return;
    }
    if (callback) {
        callback();
    }
}

void ModalComponentHost::request_cancel(runtime::ComponentId id, ModalCancelReason reason) {
    const auto* s = services_->components().state<ModalState>(id);
    if (!s || !services_->overlay_stack().is_top(id) || !services_->components().scope(id).active() ||
        !s->model.request_cancel(reason) || (reason == ModalCancelReason::CancelButton && !s->show_cancel) ||
        s->action_starting || (s->action && s->action->pending())) {
        return;
    }
    const auto callback = s->on_cancel;
    if (callback) {
        callback(reason);
    }
}

std::function<void()> ModalComponentHost::detach_action(runtime::ComponentId id) noexcept {
    auto* state = services_->components().state<ModalState>(id);
    return state && state->action ? state->action->detach_cancel() : ConfirmAction::Cancel{};
}

void ModalComponentHost::arm_service_drawer_close(runtime::ComponentId id) {
    const auto* state = services_->components().state<ModalState>(id);
    if (!state || !state->drawer || !services_->components().scope(id).active()) {
        throw std::logic_error("App Drawer close owner is retired");
    }
    // A service may be closed while its external owner is already hidden. No
    // logical visible edge then exists, but this accepted close still needs the
    // same finite terminal-sync completion boundary, never an inline future.
    if (!state->presentation.target && !state->presentation.running && !state->presentation.completion) {
        retarget_drawer(id, false);
        update_overlay(id);
    }
}

void ModalComponentHost::append_service_body(runtime::ComponentId id, runtime::ComponentId parent,
                                             const Content& content, const runtime::CapturedComponentContext& context) {
    const auto state = services_->components().retain_state<ModalState>(id);
    if (!state || !services_->components().scope(id).active() || state->snapshot.body != parent) {
        throw std::logic_error("App panel body is retired");
    }
    services_->append_slot(parent, Content{[state, parent, content] {
                               auto& build = runtime::require_component_build_context();
                               build.mount_slot_with_semantic_text_style(
                                   parent, content, Prop<runtime::SemanticForeground>{state->text_color},
                                   Prop<runtime::SemanticTypography>{state->body_font});
                           }},
                           context);
}

std::size_t ModalComponentHost::tick_auxiliary(animation::AnimationTime time) {
    if (!next_auxiliary_deadline()) {
        return 0;
    }
    const auto ids = mounted_;
    const auto life = std::weak_ptr{lifetime_};
    std::size_t count{};
    sample_drawers(time);
    for (const auto id : ids) {
        auto* state = services_->components().state<ModalState>(id);
        const auto action = state ? state->action : nullptr;
        if (action && action->tick(time)) {
            ++count;
        }
        if (life.expired()) {
            break;
        }
        state = services_->components().state<ModalState>(id);
        if (state && state->drawer && state->presentation.running) {
            ++count;
        }
    }
    return count;
}

std::optional<animation::AnimationTime> ModalComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> result;
    for (const auto id : mounted_) {
        const auto* state = services_->components().state<ModalState>(id);
        const auto deadline = state && state->action ? state->action->deadline() : std::nullopt;
        if (deadline && (!result || *deadline < *result)) {
            result = deadline;
        }
        if (state && state->drawer && state->presentation.deadline &&
            (!result || *state->presentation.deadline < *result)) {
            result = state->presentation.deadline;
        }
    }
    return result;
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
    mount_panel(panel, ModalContent{SlotContentAccess::function(content)}, regions, &props, &slots);
}

void ModalComponentHost::mount_panel(const ModalProps& props, const ModalContent& content, const ModalSlots& slots,
                                     const DrawerProps* drawer, const DrawerSlots* drawer_slots) {
    ModalOptions options{read_prop(props.open_), read_prop(props.closable_),        read_prop(props.keyboard_),
                         read_prop(props.mask_), read_prop(props.confirm_loading_), read_prop(props.footer_),
                         read_prop(props.width_)};
    ModalModel validate{options};
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto locale =
        localized_prop(props.locale_, build.config_scope(), [](const LocaleData& data) { return data.common; });
    const auto ok_text = props.ok_text_ ? *props.ok_text_ : bind([locale] { return read_prop(locale).confirm; });
    const auto cancel_text =
        props.cancel_text_ ? *props.cancel_text_ : bind([locale] { return read_prop(locale).cancel; });
    const auto id = build.mount_component<ModalState>(options);
    const auto retained = build.host().retain_state<ModalState>(id);
    auto& s = *retained;
    if (drawer) {
        s.drawer = DrawerGeometry{read_prop(drawer->placement_), read_prop(drawer->width_), read_prop(drawer->height_)};
        s.snapshot.drawer = true;
        s.drawer_content = Content{SlotContentAccess::function(content)};
        s.drawer_lazy = read_prop(drawer->lazy_);
        s.drawer_destroy = read_prop(drawer->destroy_hidden_);
        s.snapshot.loading = read_prop(drawer->loading_);
        s.drawer_rtl = read_prop(drawer->rtl_);
        s.drawer_extra = drawer_slots && drawer_slots->extra.has_value();
        s.on_resize = drawer->on_resize_;
        s.on_resize_end = drawer->on_resize_end_;
    }
    s.node = build.root(id);
    s.title_slot = slots.title.has_value();
    s.footer_slot = slots.footer.has_value();
    s.centered = read_prop(props.centered_);
    s.restore = read_prop(props.restore_focus_);
    s.focus_trap = read_prop(props.focus_trap_);
    s.title.set(read_prop(props.title_));
    s.ok_text.set(read_prop(ok_text));
    s.cancel_text.set(read_prop(cancel_text));
    s.on_ok = props.on_ok_;
    s.action_source = props.action_;
    s.action_live = props.action_live_;
    s.action_error = props.action_error_;
    s.action = std::make_shared<ConfirmActionSession>([this, life = std::weak_ptr{lifetime_},
                                                       id](ConfirmActionResult result) {
        if (life.expired()) {
            return;
        }
        auto* state = services_->components().state<ModalState>(id);
        if (!state || !services_->components().scope(id).active() || (state->action_live && !state->action_live())) {
            return;
        }
        const auto loading = state->loading;
        const auto error = state->action_error;
        const auto accepted = state->on_ok;
        const auto generation = state->action->generation();
        loading.set(state->user_loading);
        if (life.expired()) {
            return;
        }
        state = services_->components().state<ModalState>(id);
        if (!state || !services_->components().scope(id).active() || state->action->generation() != generation ||
            (state->action_live && !state->action_live())) {
            return;
        }
        if (result.error) {
            if (error) {
                error(std::move(*result.error));
            }
        } else if (result.decision == ConfirmActionDecision::Close && accepted) {
            accepted();
        }
    });
    build.scope(id).on_cleanup([action = s.action] { action->cancel(); });
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
            s.overlay_ticket = services.overlay_stack().add(
                services.interactions().input_root(id), services.interactions().input_root(s.snapshot.overlay),
                drawer ? WindowOverlayKind::Drawer : WindowOverlayKind::Modal);
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
            s.shield_fragment = shield_fragment;
            s.shield_surface = services.surfaces().create(s.snapshot.overlay, overlay_node, shield_fragment,
                                                          s.shield_hit, std::span{&initial, 1}, no_effect);
            context.on_resource_cleanup(s.snapshot.overlay, [&services, root = s.snapshot.overlay, node = overlay_node,
                                                             hit = s.shield_hit, surface = s.shield_surface] {
                // The block belongs to this exact overlay generation.
                services.interactions().set_input_blocked(root, false);
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
                            dialog.mount_slot(
                                s.snapshot.body_viewport, Content{[&] {
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
                                    viewport.on_resource_cleanup(
                                        s.snapshot.body, [&services, node, body = s.snapshot.body] {
                                            services.interactions().set_input_blocked(body, false);
                                            services.layout().remove_layout(node);
                                        });
                                    if (!drawer || options.open || (!s.drawer_lazy && !s.drawer_destroy)) {
                                        viewport.mount_slot_with_semantic_text_style(
                                            s.snapshot.body, Content{SlotContentAccess::function(content)},
                                            Prop<runtime::SemanticForeground>{s.text_color},
                                            Prop<runtime::SemanticTypography>{s.body_font});
                                        s.snapshot.body_mounted = true;
                                    }
                                }});
                            if (drawer) {
                                s.snapshot.placeholder = region(layout::BoxLayout{{}, true}, true);
                                dialog.mount_slot(
                                    s.snapshot.placeholder, Content{[] {
                                        Skeleton(SkeletonProps{}.paragraph(SkeletonParagraph{.rows = std::size_t{4}}));
                                    }});
                                s.snapshot.extra = region(layout::BoxLayout{});
                                dialog.mount_slot_with_semantic_text_style(
                                    s.snapshot.extra, Content{[slot = drawer_slots->extra] {
                                        if (slot) {
                                            SlotContentAccess::function (*slot)();
                                        }
                                    }},
                                    Prop<runtime::SemanticForeground>{s.text_color},
                                    Prop<runtime::SemanticTypography>{s.body_font});
                                s.snapshot.resize_handle = region(layout::LeafLayout{});
                                const auto handle_node = dialog.root(s.snapshot.resize_handle);
                                s.resize_hit = services.interactions().create(
                                    {s.snapshot.resize_handle, handle_node, {}, true, true, {}, true, true});
                                const auto fragment = dialog.register_scene_fragment(
                                    s.snapshot.resize_handle, runtime::SceneFragmentPlacement::before_children);
                                s.resize_surface =
                                    services.surfaces().create(s.snapshot.resize_handle, handle_node, fragment,
                                                               s.resize_hit, std::span{&initial, 1}, no_effect);
                                dialog.on_resource_cleanup(s.snapshot.resize_handle,
                                                           [&services, hit = s.resize_hit, surface = s.resize_surface] {
                                                               services.pointer().cancel_interaction(hit);
                                                               services.focus().cancel_interaction(hit);
                                                               services.interactions().remove(hit);
                                                               services.surfaces().destroy(surface);
                                                           });
                                input::FocusHandlers resize_focus;
                                resize_focus.state_changed = [this, id](input::FocusPresentation) {
                                    if (const auto* state = services_->components().state<ModalState>(id)) {
                                        services_->dirty().invalidate(state->node, runtime::DirtyFlags::Material);
                                    }
                                };
                                services.interactions().set_focus_handlers(s.resize_hit, std::move(resize_focus));
                                input::InteractionHandlers handlers;
                                handlers.target = [this, id](input::PointerDispatchContext& event) {
                                    auto* state = services_->components().state<ModalState>(id);
                                    if (!state || !state->drawer_resize.enabled || !state->model.visible()) {
                                        return;
                                    }
                                    const bool horizontal = state->drawer->placement == DrawerPlacement::Left ||
                                                            state->drawer->placement == DrawerPlacement::Right;
                                    const float coordinate = horizontal ? event.event().x : event.event().y;
                                    if (event.kind() == input::PointerEventKind::down &&
                                        event.event().button == input::PointerButton::primary) {
                                        state->resize_origin = coordinate;
                                        state->resize_extent =
                                            horizontal ? state->drawer->width : state->drawer->height;
                                        state->resize_requested = state->resize_extent;
                                        state->snapshot.resizing = event.capture_pointer();
                                        event.stop_propagation();
                                    } else if (event.kind() == input::PointerEventKind::move &&
                                               state->snapshot.resizing) {
                                        const bool reverse = state->drawer->placement == DrawerPlacement::Right ||
                                                             state->drawer->placement == DrawerPlacement::Bottom;
                                        const float value = state->resize_extent +
                                                            (coordinate - state->resize_origin) * (reverse ? -1 : 1);
                                        event.stop_propagation();
                                        resize_drawer(id, value, false);
                                    } else if (event.kind() == input::PointerEventKind::up &&
                                               event.event().button == input::PointerButton::primary &&
                                               state->snapshot.resizing) {
                                        const float value = state->resize_requested;
                                        state->snapshot.resizing = false;
                                        static_cast<void>(event.release_pointer_capture());
                                        event.stop_propagation();
                                        resize_drawer(id, value, true);
                                    } else if (event.kind() == input::PointerEventKind::cancel) {
                                        state->snapshot.resizing = false;
                                    }
                                };
                                services.interactions().set_handlers(s.resize_hit, std::move(handlers));
                            }
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
                                            Button(ButtonProps{}.disabled(false).onClick([this, id] {
                                                request_cancel(id, ModalCancelReason::CancelButton);
                                            }),
                                                   ButtonContent{
                                                       [text = s.cancel_text] { Text(TextProps{}.content(text)); }});
                                            s.snapshot.cancel = services.components().children(s.default_footer).back();
                                            Button(
                                                ButtonProps{}
                                                    .disabled(false)
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
    if (!drawer) {
        connect_prop(scope, config_rtl(Prop<bool>{false}, false), [this, id](bool rtl) {
            const auto* state = services_->components().state<ModalState>(id);
            if (!state || !state->default_footer.valid()) {
                return;
            }
            layout::FlexLayout row;
            row.main_gap = 8;
            row.fill_width = true;
            row.wrap = layout::FlexWrap::no_wrap;
            row.justify = layout::FlexJustify::end;
            row.align = layout::FlexAlign::center;
            row.item_policy = layout::FlexItemPolicy::sequential;
            row.right_to_left = rtl;
            services_->layout().set_layout(services_->components().root(state->default_footer), row);
        });
    }
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
    connect_prop(scope, props.open_, [this, id](bool value) {
        if (auto* state = services_->components().state<ModalState>(id)) {
            auto options = state->model.options();
            options.open = value;
            state->model.reconcile(options);
            const auto life = std::weak_ptr{lifetime_};
            if (state->drawer && !value) {
                // Also applies during an enclosing input/visibility callback:
                // reconcile may defer its walk, but admission stops right now.
                services_->interactions().set_input_blocked(state->snapshot.overlay, true);
                reconcile();
            }
            if (!life.expired() && services_->components().contains(id)) {
                refresh(id, false);
            }
        }
    });
    option(props.closable_, [](auto& options, bool value) { options.closable = value; }, true);
    option(props.keyboard_, [](auto& options, bool value) { options.keyboard = value; });
    option(props.mask_, [](auto& options, ModalMask value) { options.mask = value; });
    connect_prop(scope, props.confirm_loading_, [this, id](bool value) {
        if (auto* state = services_->components().state<ModalState>(id)) {
            state->user_loading = value;
            auto options = state->model.options();
            options.confirm_loading = value;
            state->model.reconcile(options);
            state->loading.set(value || state->action->pending());
        }
    });
    option(props.footer_, [](auto& options, ModalFooterMode value) { options.footer = value; }, true);
    option(props.width_, [](auto& options, float value) { options.width = value; }, true);
    connect_prop(scope, props.title_, [this, id](String value) {
        if (auto* s = services_->components().state<ModalState>(id)) {
            s->title.set(std::move(value));
            refresh(id, true);
        }
    });
    if (!drawer && !slots.footer) {
        connect_prop(scope, ok_text, [this, id](String value) {
            if (auto* s = services_->components().state<ModalState>(id)) {
                const auto revision = ++s->label_revision;
                const auto target = s->ok_text;
                target.set(std::move(value));
                s = services_->components().state<ModalState>(id);
                if (!s || s->label_revision != revision) {
                    return;
                }
                refresh(id, true);
            }
        });
        connect_prop(scope, cancel_text, [this, id](String value) {
            if (auto* s = services_->components().state<ModalState>(id)) {
                const auto revision = ++s->label_revision;
                const auto target = s->cancel_text;
                target.set(std::move(value));
                s = services_->components().state<ModalState>(id);
                if (!s || s->label_revision != revision) {
                    return;
                }
                refresh(id, true);
            }
        });
    }
    connect_prop(scope, props.centered_, [this, id](bool value) {
        if (auto* s = services_->components().state<ModalState>(id)) {
            s->centered = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.show_cancel_, [this, id](bool value) {
        if (auto* state = services_->components().state<ModalState>(id)) {
            state->show_cancel = value;
            if (state->snapshot.cancel.valid()) {
                services_->components().set_branch_active(state->snapshot.cancel, value);
                services_->nodes().require(services_->components().root(state->snapshot.cancel)).out_of_flow = !value;
                refresh(id, true);
            }
        }
    });
    connect_prop(scope, props.focus_trap_, [this, id](bool value) {
        if (auto* state = services_->components().state<ModalState>(id)) {
            if (state->focus_trap != value) {
                state->focus_trap = value;
                refresh(id, false);
            }
        }
    });
    connect_prop(scope, props.restore_focus_, [this, id](bool value) {
        if (auto* s = services_->components().state<ModalState>(id)) {
            s->restore = value;
            update_overlay(id);
            static_cast<void>(services_->overlay_stack().reconcile(false));
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
                        if (next.placement != state->drawer->placement) {
                            const auto revision = state->presentation.revision;
                            const auto hit = state->resize_hit;
                            state->snapshot.resizing = false;
                            state->keyboard_resizing = false;
                            state->drawer = next;
                            const auto life = std::weak_ptr{lifetime_};
                            services_->pointer().cancel_interaction(hit);
                            if (life.expired() || !services_->components().contains(id)) {
                                return;
                            }
                            state = services_->components().state<ModalState>(id);
                            if (state->presentation.revision != revision || *state->drawer != next) {
                                return;
                            }
                            services_->focus().cancel_interaction(hit);
                            if (life.expired() || !services_->components().contains(id)) {
                                return;
                            }
                            state = services_->components().state<ModalState>(id);
                            if (state->presentation.revision != revision || *state->drawer != next) {
                                return;
                            }
                        }
                        state = services_->components().state<ModalState>(id);
                        state->drawer = next;
                        refresh(id, true);
                    }
                }
            });
        };
        geometry(drawer->placement_, [](auto& next, DrawerPlacement value) { next.placement = value; });
        geometry(drawer->width_, [](auto& next, float value) { next.width = value; });
        geometry(drawer->height_, [](auto& next, float value) { next.height = value; });
        const auto property = [&]<typename T>(const Prop<T>& source, auto update) {
            connect_prop(scope, source, [this, id, update](T value) {
                if (auto* state = services_->components().state<ModalState>(id)) {
                    update(*state, value);
                    refresh(id, true);
                }
            });
        };
        property(drawer->loading_, [](auto& state, bool value) { state.snapshot.loading = value; });
        property(drawer->lazy_, [](auto& state, bool value) { state.drawer_lazy = value; });
        property(drawer->destroy_hidden_, [](auto& state, bool value) { state.drawer_destroy = value; });
        property(drawer->rtl_, [](auto& state, bool value) { state.drawer_rtl = value; });
        property(drawer->resize_, [](auto& state, DrawerResize value) {
            if (!std::isfinite(value.minimum) || !std::isfinite(value.maximum) || value.minimum <= 0 ||
                value.maximum < value.minimum || !std::isfinite(value.keyboard_step) || value.keyboard_step <= 0) {
                throw std::invalid_argument("Invalid Drawer resize limits");
            }
            state.drawer_resize = value;
        });
        property(drawer->push_, [](auto& state, DrawerPush value) {
            if (!std::isfinite(value.distance) || value.distance < 0) {
                throw std::invalid_argument("Invalid Drawer push distance");
            }
            state.drawer_push = value;
        });
        subscribe(std::array{&theme_runtime::ThemeScope::drawer_colors, &theme_runtime::ThemeScope::drawer_metrics,
                             &theme_runtime::ThemeScope::drawer_typography, &theme_runtime::ThemeScope::drawer_effects,
                             &theme_runtime::ThemeScope::drawer_order});
        s.motion_subscription = theme->capture([this](theme_runtime::DirtyPhase) { synchronize_auxiliary_motion(); },
                                               [theme] { static_cast<void>(theme->motion_enabled()); });
    } else {
        subscribe(std::array{&theme_runtime::ThemeScope::modal_colors, &theme_runtime::ThemeScope::modal_metrics,
                             &theme_runtime::ThemeScope::modal_typography, &theme_runtime::ThemeScope::modal_effects,
                             &theme_runtime::ThemeScope::modal_order});
    }
    s.ready = true;
    refresh(id, true);
}

void ModalComponentHost::reconcile_drawer_content(runtime::ComponentId id) {
    auto* s = services_->components().state<ModalState>(id);
    if (!s || !s->drawer || s->drawer_syncing) {
        return;
    }
    s->drawer_syncing = true;
    const auto life = std::weak_ptr{lifetime_};
    try {
        bool settled{};
        for (unsigned attempt = 0; attempt < 32; ++attempt) {
            s = services_->components().state<ModalState>(id);
            if (!s) {
                return;
            }
            const bool open = s->model.options().open;
            const bool render = open || s->presentation.painted() || s->presentation.completion ||
                                (!s->drawer_lazy && !s->drawer_destroy);
            if (render && !s->snapshot.body_mounted) {
                s->snapshot.body_mounted = true;
                const auto content = s->drawer_content;
                const auto color = s->text_color;
                const auto font = s->body_font;
                display_append_styled(*services_, id, s->snapshot.body, content, color, font, false);
                if (life.expired()) {
                    return;
                }
            } else if (!render && s->drawer_destroy && s->snapshot.body_mounted) {
                s->snapshot.body_mounted = false;
                const auto revision = s->presentation.revision;
                const auto children = services_->components().children(s->snapshot.body);
                for (auto child : children) {
                    services_->destroy(child);
                    if (life.expired()) {
                        return;
                    }
                    s = services_->components().state<ModalState>(id);
                    if (!s) {
                        return;
                    }
                }
                if (s->presentation.revision != revision) {
                    continue; // Re-read the new target before publishing any gates.
                }
            } else {
                settled = true;
                break;
            }
        }
        if (!settled) {
            services_->retire_failed_component(id);
            throw std::logic_error("Drawer content callbacks did not converge");
        }
        const bool shown = paint_visible(*s);
        const bool input = s->model.visible();
        for (const auto& [component, active] :
             {std::pair{s->snapshot.body, shown && !s->snapshot.loading},
              std::pair{s->snapshot.placeholder, shown && s->snapshot.loading},
              std::pair{s->snapshot.extra, s->drawer_extra},
              std::pair{s->snapshot.resize_handle, shown && s->drawer_resize.enabled}}) {
            if (services_->components().set_branch_active(component, active)) {
                services_->mark_scene_structure_dirty();
            }
        }
        services_->interactions().set_eligible(s->resize_hit, input && s->drawer_resize.enabled);
        const auto handle = s->resize_hit;
        const bool cancel = !input || !s->drawer_resize.enabled;
        const auto body = s->snapshot.body;
        const bool blocked = s->snapshot.loading;
        const auto revision = s->presentation.revision;
        const auto refresh_revision = s->refresh_revision;
        const auto current = [&] {
            if (life.expired()) {
                return false;
            }
            const auto* state = services_->components().state<ModalState>(id);
            return state && state->presentation.revision == revision && state->refresh_revision == refresh_revision;
        };
        s->drawer_syncing = false;
        if (cancel) {
            services_->pointer().cancel_interaction(handle);
            if (!current()) {
                return;
            }
            services_->focus().cancel_interaction(handle);
        }
        if (current()) {
            services_->set_input_blocked(body, blocked);
        }
    } catch (...) {
        if (!life.expired()) {
            if (auto* state = services_->components().state<ModalState>(id)) {
                state->drawer_syncing = false;
            }
        }
        throw;
    }
}

void ModalComponentHost::resize_drawer(runtime::ComponentId id, float value, bool finished) {
    auto* s = services_->components().state<ModalState>(id);
    if (!s || !s->drawer || !s->model.visible() || !s->drawer_resize.enabled ||
        !services_->interactions().component_active(s->snapshot.resize_handle)) {
        return;
    }
    const float requested = std::clamp(value, s->drawer_resize.minimum, s->drawer_resize.maximum);
    s->resize_requested = requested;
    const auto callback = finished ? s->on_resize_end : s->on_resize;
    if (callback) {
        callback(requested);
    }
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
    s.extra_size = s.drawer_extra
                       ? engine.measure_child(node(s.snapshot.extra),
                                              {0, std::max(0.0F, width - header_x * 2 - reserve), 0, infinity})
                       : runtime::Size{};
    const float extra_reserve = s.drawer_extra ? s.extra_size.width + m.close_gap : 0;
    float title_height{};
    if (services_->components().branch_active(s.snapshot.title)) {
        title_height =
            engine
                .measure_child(node(s.snapshot.title),
                               {0, std::max(0.0F, width - header_x * 2 - reserve - extra_reserve), 0, infinity})
                .height;
        title_height = std::max(title_height, s.extra_size.height);
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
    const float content_height = engine
                                     .measure_child(node(s.snapshot.loading ? s.snapshot.placeholder : s.snapshot.body),
                                                    {inner_width, inner_width, 0, infinity})
                                     .height;
    s.reveal = s.reveal || s.content_height != content_height;
    s.content_height = content_height;
    s.body_height = std::max(0.0F, remaining - s.padding_y * 2);
    static_cast<void>(
        engine.measure_child(node(s.snapshot.body_viewport), layout::Constraints::fixed(inner_width, s.body_height)));
    if (s.drawer_resize.enabled) {
        const float handle = std::min(6.0F, std::min(width, height));
        const auto edge = s.drawer->placement;
        const bool horizontal = edge == DrawerPlacement::Left || edge == DrawerPlacement::Right;
        static_cast<void>(engine.measure_child(
            node(s.snapshot.resize_handle),
            layout::Constraints::fixed(horizontal ? handle : width, horizontal ? height : handle)));
    }
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
    const float extra_reserve = s.drawer_extra ? s.extra_size.width + m.close_gap : 0;
    if (services_->components().branch_active(s.snapshot.title)) {
        const float start = std::min(rect.width - header_x, header_x + reserve);
        engine.place_child(node(s.snapshot.title),
                           {rect.x + (s.drawer_rtl ? header_x + extra_reserve : start), rect.y + header_y,
                            std::max(0.0F, rect.width - start - header_x - extra_reserve), title_height});
    }
    if (s.model.options().closable) {
        const float size = std::min(m.close_size, std::min(rect.width, s.header_height));
        const float top =
            std::clamp(header_y + (title_height - size) / 2, 0.0F, std::max(0.0F, s.header_height - size));
        engine.place_child(node(s.snapshot.close),
                           {rect.x + (s.drawer_rtl ? std::max(0.0F, rect.width - header_x - size)
                                                   : std::min(header_x, rect.width - size)),
                            rect.y + top, size, size});
    }
    if (s.drawer_extra) {
        engine.place_child(node(s.snapshot.extra),
                           {s.drawer_rtl ? rect.x + header_x : rect.x + rect.width - header_x - s.extra_size.width,
                            rect.y + header_y + std::max(0.0F, (title_height - s.extra_size.height) / 2),
                            s.extra_size.width, std::min(s.extra_size.height, title_height)});
    }
    const runtime::Rect body_bounds{rect.x + s.padding_x, rect.y + s.header_height + s.padding_y,
                                    std::max(0.0F, rect.width - s.padding_x * 2), s.body_height};
    s.reveal = s.reveal || s.snapshot.body_bounds != body_bounds;
    s.snapshot.body_bounds = body_bounds;
    engine.place_child(node(s.snapshot.body_viewport), s.snapshot.body_bounds);
    if (s.snapshot.loading) {
        engine.place_child(node(s.snapshot.placeholder), s.snapshot.body_bounds);
    } else {
        engine.place_child(node(s.snapshot.body), {s.snapshot.body_bounds.x, s.snapshot.body_bounds.y,
                                                   s.snapshot.body_bounds.width, s.content_height});
    }
    if (s.drawer_resize.enabled) {
        const float handle = std::min(6.0F, std::min(rect.width, rect.height));
        const auto edge = s.drawer->placement;
        const bool horizontal = edge == DrawerPlacement::Left || edge == DrawerPlacement::Right;
        engine.place_child(node(s.snapshot.resize_handle),
                           {edge == DrawerPlacement::Left ? rect.x + rect.width - handle : rect.x,
                            edge == DrawerPlacement::Top ? rect.y + rect.height - handle : rect.y,
                            horizontal ? handle : rect.width, horizontal ? rect.height : handle});
    }
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

void ModalComponentHost::complete_drawers() {
    if (std::ranges::none_of(mounted_, [this](auto id) {
            const auto* state = services_->components().state<ModalState>(id);
            return state && state->drawer && state->presentation.completion && state->presentation.geometry;
        })) {
        return;
    }
    const auto ids = mounted_;
    const auto life = std::weak_ptr{lifetime_};
    std::exception_ptr error;
    const auto call = [&](auto&& callback) {
        try {
            callback();
        } catch (...) {
            if (!error) {
                error = std::current_exception();
            }
        }
    };
    for (auto id : ids) {
        auto* s = services_->components().state<ModalState>(id);
        if (!s || !s->drawer || !s->presentation.completion || s->presentation.geometry != s->presentation.completion ||
            s->presentation.geometry_sync == services_->successful_sync_revision() ||
            !services_->components().scope(id).active()) {
            continue;
        }
        const auto revision = s->presentation.revision;
        const bool open = s->presentation.target;
        const auto current = [&] {
            if (life.expired()) {
                return false;
            }
            const auto* state = services_->components().state<ModalState>(id);
            return state && services_->components().scope(id).active() && state->presentation.revision == revision &&
                   state->presentation.target == open;
        };
        const auto changed = s->after_open_change;
        const auto closed = s->after_close;
        // Consume before cleanup or user code. A throw cannot deliver this ticket twice.
        s->presentation.completion.reset();
        s->presentation.geometry.reset();
        if (!open && s->drawer_destroy && s->snapshot.body_mounted) {
            s->snapshot.body_mounted = false;
            const auto children = services_->components().children(s->snapshot.body);
            // Cleanup callbacks may reopen the Drawer while old InputRef bindings
            // are still alive. Drain those generations before mounting new content.
            s->drawer_syncing = true;
            for (auto child : ids) {
                auto* nested = services_->components().state<ModalState>(child);
                if (child != id && nested && nested->drawer && descendant(*services_, child, s->snapshot.body)) {
                    ++nested->presentation.revision;
                    nested->presentation.completion.reset();
                    nested->presentation.geometry.reset();
                    nested->presentation.running = false;
                    nested->presentation.deadline.reset();
                }
            }
            // Only destroy the snapshotted old generations. Reentrant open may
            // append new content under the same body; it is never in this list.
            for (auto child : children) {
                call([&] { services_->destroy(child); });
                if (life.expired()) {
                    return;
                }
                if (!services_->components().contains(id)) {
                    break;
                }
            }
            if (auto* state = services_->components().state<ModalState>(id)) {
                state->drawer_syncing = false;
                call([&] { reconcile_drawer_content(id); });
            }
        }
        if (current() && changed) {
            call([&] { changed(open); });
        }
        if (life.expired()) {
            return;
        }
        if (!open && current() && closed) {
            call(closed);
        }
        if (life.expired()) {
            return;
        }
    }
    if (error) {
        std::rethrow_exception(error);
    }
}

void ModalComponentHost::synchronize_auxiliary_motion() {
    reconcile();
}

void ModalComponentHost::prepare_auxiliary_layout() {
    const auto life = std::weak_ptr{lifetime_};
    reconcile();
    if (life.expired()) {
        return;
    }
    on_input_boundary_changed();
    complete_drawers();
    if (life.expired()) {
        return;
    }
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
            if (life.expired()) {
                return;
            }
            if (!event.visible && closed && services_->components().contains(event.owner) &&
                services_->components().scope(event.owner).active()) {
                closed();
                if (life.expired()) {
                    return;
                }
            }
        }
        notifying_ = false;
    } catch (...) {
        if (!life.expired()) {
            notifying_ = false;
        }
        throw;
    }
}

void ModalComponentHost::position_window_layers(runtime::Size, runtime::Rect clip) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<ModalState>(id);
        if (!s || !paint_visible(*s)) {
            continue;
        }
        const auto token = panel_theme(services_->components().theme_scope(id)->snapshot(), s->drawer.has_value());
        const auto overlay = services_->components().root(s->snapshot.overlay);
        const auto dialog = services_->components().root(s->snapshot.dialog);
        auto& engine = services_->layout();
        if (s->drawer) {
            float push{};
            const auto overlays = services_->overlay_stack().ordered();
            for (const auto& entry : overlays) {
                const auto* child = services_->components().state<ModalState>(entry.owner);
                if (child && child->drawer && paint_visible(*child) && child->drawer_push.enabled &&
                    services_->overlay_stack().drawer_parent(entry.owner) == id) {
                    push = std::lerp(push, child->drawer_push.distance, child->presentation.progress);
                }
            }
            push *= s->presentation.progress;
            if (s->drawer_push_offset != push) {
                s->drawer_push_offset = push;
                s->needs_layout = true;
            }
            runtime::Point family;
            for (auto parent = services_->components().parent(id); parent;
                 parent = services_->components().parent(*parent)) {
                const auto* ancestor = services_->components().state<ModalState>(*parent);
                if (ancestor && ancestor->drawer && ancestor->presentation.running) {
                    const auto offset = drawer_exit(*ancestor->drawer, drawer_bounds(*ancestor->drawer, clip),
                                                    ancestor->presentation.progress);
                    family.x += offset.x;
                    family.y += offset.y;
                }
            }
            // Window layers escape ordinary Node translations. Project family
            // exit motion explicitly, without inheriting ancestor push/scroll.
            if (s->drawer_family_offset != family) {
                s->drawer_family_offset = family;
                s->needs_layout = true;
            }
        }
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
                const auto offset = drawer_exit(*s->drawer, s->snapshot.bounds, s->presentation.progress);
                s->snapshot.bounds.x += offset.x + s->drawer_family_offset.x;
                s->snapshot.bounds.y += offset.y + s->drawer_family_offset.y;
                switch (s->drawer->placement) {
                case DrawerPlacement::Left:
                    s->snapshot.bounds.x += s->drawer_push_offset;
                    break;
                case DrawerPlacement::Right:
                    s->snapshot.bounds.x -= s->drawer_push_offset;
                    break;
                case DrawerPlacement::Top:
                    s->snapshot.bounds.y += s->drawer_push_offset;
                    break;
                case DrawerPlacement::Bottom:
                    s->snapshot.bounds.y -= s->drawer_push_offset;
                    break;
                }
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
        if (!s) {
            continue;
        }
        const auto& snapshot = services_->components().theme_scope(id)->snapshot();
        const auto token = panel_theme(snapshot, s->drawer.has_value());
        const auto mask = s->model.options().mask;
        const bool blur = !s->drawer && s->model.visible() && mask.enabled && mask.blur;
        std::optional<graphics::SceneCompositionRequest> backdrop;
        if (blur) {
            const auto overlay = services_->components().root(s->snapshot.overlay);
            backdrop = graphics::SceneCompositionRequest{overlay,
                                                         {window, 0},
                                                         window,
                                                         true,
                                                         snapshot.modal().effects.mask_blur_sigma,
                                                         {token.colors.mask.red(), token.colors.mask.green(),
                                                          token.colors.mask.blue(), token.colors.mask.alpha()},
                                                         services_->nodes().paint_opacity(overlay)};
        }
        // Clear the request on close even when its paint subtree is inactive.
        services_->scene_composer().set_fragment_composition(s->shield_fragment, false,
                                                             graphics::DrawCoverageRole::Normal, backdrop);
        if (!paint_visible(*s)) {
            continue;
        }
        auto shield = quad(window, mask.enabled && !blur ? token.colors.mask : Color::rgba8(0, 0, 0, 0));
        if (s->drawer) {
            shield.color[3] *= s->presentation.progress;
        }
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
            auto handle_bounds = bounds(*services_, s->snapshot.resize_handle);
            const bool horizontal =
                s->drawer->placement == DrawerPlacement::Left || s->drawer->placement == DrawerPlacement::Right;
            if (horizontal) {
                handle_bounds.y += handle_bounds.height / 3;
                handle_bounds.height /= 3;
                handle_bounds.x += handle_bounds.width / 3;
                handle_bounds.width /= 3;
            } else {
                handle_bounds.x += handle_bounds.width / 3;
                handle_bounds.width /= 3;
                handle_bounds.y += handle_bounds.height / 3;
                handle_bounds.height /= 3;
            }
            const auto handle_quad =
                quad(handle_bounds,
                     services_->focus().state().focused == s->resize_hit ? token.colors.focus_outline : drawer.border);
            static_cast<void>(services_->surfaces().update_surface(s->resize_surface, std::span{&handle_quad, 1}));
        } else {
            static_cast<void>(services_->surfaces().update_surface(s->dialog_surface, visuals));
        }
        component::RetainedSurfaceEffects effects;
        effects.shape = {rect, std::min(token.effects.focus_radius, std::min(rect.width, rect.height) / 2)};
        effects.shadow_shape = graphics::LogicalRoundedRect{rect, radius};
        effects.shadows = token.effects.shadow;
        if (s->drawer) {
            std::array<ShadowLayer, ShadowList::capacity> directional;
            for (std::size_t index = 0; index < token.effects.shadow.size(); ++index) {
                auto layer = token.effects.shadow[index];
                const auto offset = layer.offset;
                switch (s->drawer->placement) {
                case DrawerPlacement::Top:
                    break;
                case DrawerPlacement::Bottom:
                    layer.offset = {-offset.x, -offset.y};
                    break;
                case DrawerPlacement::Left:
                    layer.offset = {offset.y, -offset.x};
                    break;
                case DrawerPlacement::Right:
                    layer.offset = {-offset.y, offset.x};
                    break;
                }
                directional[index] = layer;
            }
            effects.shadows = ShadowList{std::span{directional}.first(token.effects.shadow.size())};
        }
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
    const auto top = services_->overlay_stack().top();
    if (top && top->kind != WindowOverlayKind::FullscreenSpin && reveal_focus(top->owner, true)) {
        services_->request_frame();
    }
    for (auto id : mounted_) {
        auto* state = services_->components().state<ModalState>(id);
        if (state && state->drawer && state->presentation.completion && !state->presentation.running &&
            state->presentation.geometry != state->presentation.completion) {
            state->presentation.geometry = state->presentation.completion;
            state->presentation.geometry_sync = services_->successful_sync_revision();
            services_->request_frame();
        }
    }
    return false;
}

bool ModalComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    const auto top = services_->overlay_stack().top();
    if (!top || top->kind == WindowOverlayKind::FullscreenSpin || !services_->focus().state().window_active) {
        return false;
    }
    const auto* drawer = services_->components().state<ModalState>(top->owner);
    if (drawer && drawer->drawer && drawer->drawer_resize.enabled &&
        services_->focus().state().focused == drawer->resize_hit &&
        (event.key == input::Key::left || event.key == input::Key::right || event.key == input::Key::up ||
         event.key == input::Key::down)) {
        const auto edge = drawer->drawer->placement;
        const bool horizontal = edge == DrawerPlacement::Left || edge == DrawerPlacement::Right;
        if ((event.key == input::Key::left || event.key == input::Key::right) != horizontal) {
            return false;
        }
        if (event.action == input::KeyAction::down) {
            const bool positive = event.key == input::Key::right || event.key == input::Key::down;
            const bool reverse = edge == DrawerPlacement::Right || edge == DrawerPlacement::Bottom;
            const auto id = top->owner;
            services_->components().state<ModalState>(id)->keyboard_resizing = true;
            resize_drawer(id,
                          (horizontal ? drawer->drawer->width : drawer->drawer->height) +
                              drawer->drawer_resize.keyboard_step * (positive != reverse ? 1 : -1),
                          false);
        } else if (event.action == input::KeyAction::up && drawer->keyboard_resizing) {
            const auto id = top->owner;
            const auto value = drawer->resize_requested;
            services_->components().state<ModalState>(id)->keyboard_resizing = false;
            resize_drawer(id, value, true);
        }
        return true;
    }
    if (event.key == input::Key::escape) {
        if (event.action == input::KeyAction::down && !event.repeat) {
            request_cancel(top->owner, ModalCancelReason::Escape);
        }
        return true;
    }
    return false;
}

bool ModalComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    const auto top = services_->overlay_stack().top();
    if (!top || top->kind == WindowOverlayKind::FullscreenSpin) {
        return false;
    }
    const auto id = top->owner;
    const auto* s = services_->components().state<ModalState>(id);
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    auto layer = target ? std::optional{target->component} : std::nullopt;
    while (layer && !services_->components().is_window_layer_root(*layer)) {
        layer = services_->components().parent(*layer);
    }
    if (!s || !layer || *layer != s->snapshot.overlay ||
        !services_->interactions().component_active(s->snapshot.overlay)) {
        return false;
    }
    if (inside(s->snapshot.body_bounds, event.x, event.y)) {
        static_cast<void>(scroll_to(id, s->snapshot.scroll - event.delta_y * 48));
    }
    return true;
}

void ModalComponentHost::on_window_active(bool) {
    synchronize_auxiliary_motion();
}
} // namespace ryn::detail

namespace ryn {
void Drawer(DrawerProps props, DrawerContent content, DrawerSlots slots) {
    detail::ConfigDefaults::apply(props);
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
