#include "component/config_defaults.hpp"
#include "component/tag_badge_component.hpp"
#include "input/pressable_behavior.hpp"
#include "component/retained_slot.hpp"
#include "component/form_control_binding.hpp"
#include <set>
#include <charconv>
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/icon.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local TagBadgeComponentHost* active_status_components{};

struct TagState final {
    runtime::NodeId node;
    runtime::ComponentId group;
    TagSnapshot snapshot;
    runtime::ComponentId viewport;
    bool checkable{};
    bool controlled_checked{true};
    bool controlled_visible{true};
    bool closing_callback{};
    bool destroy_on_close{};
    bool rtl{};
    bool animated{true};
    TagVariant variant{TagVariant::Outlined};
    std::uint64_t revision{};
    std::function<void(TagCloseEvent&)> on_close_event;
    std::optional<animation::AnimationTime> motion_start;
    std::int64_t motion_duration{200000};
    bool bordered{true};
    bool has_icon{};
    bool hovered{};
    TagColor color{PresetColor::Default};
    input::PressableBehavior press;
    input::FocusPresentation focus;
    component::RetainedSurfaceId surface;
    component::RetainedSurfaceId border;
    std::function<void()> on_close;
    std::function<void(bool)> on_change;
    Signal<runtime::SemanticForeground> foreground{runtime::SemanticForeground{}};
    Signal<runtime::SemanticForeground> close_foreground{runtime::SemanticForeground{}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> close_font{runtime::SemanticTypography{}};
    std::array<runtime::Size, 3> sizes;
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

struct BadgeState final {
    runtime::NodeId node;
    BadgeSnapshot snapshot;
    bool wrapped{};
    bool visible{true};
    bool rtl{};
    bool animated{true};
    bool ribbon{};
    RibbonPlacement placement{RibbonPlacement::End};
    bool processing_visible{};
    bool processing_started{};
    std::optional<Color> custom_color;
    BadgeCountValue value;
    std::shared_ptr<const BadgeRenderer> content_renderer;
    std::shared_ptr<const BadgeRenderer> count_renderer;
    runtime::ComponentId custom_content;
    runtime::ComponentId base_content;
    runtime::ComponentId custom_count;
    runtime::ComponentId current_count;
    runtime::ComponentId fold;
    Signal<IconSource> fold_icon{IconSource{}};
    Signal<runtime::SemanticForeground> fold_color{runtime::SemanticForeground{}};
    Signal<runtime::SemanticTypography> fold_font{runtime::SemanticTypography{}};
    std::uint64_t revision{};
    Signal<String> old_count{String{}};
    int direction{1};
    std::optional<animation::AnimationTime> number_start;
    animation::AnimationTime phase_time;
    std::int64_t duration{200000};
    std::optional<int> count;
    int overflow{99};
    bool show_zero{};
    bool dot{};
    Status status{Status::None};
    BadgeSize size{BadgeSize::Default};
    LogicalOffset offset;
    Signal<String> count_text{String{}};
    Signal<String> label{String{}};
    Signal<runtime::SemanticForeground> count_color{runtime::SemanticForeground{}};
    Signal<runtime::SemanticForeground> text_color{runtime::SemanticForeground{}};
    Signal<runtime::SemanticTypography> count_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> text_font{runtime::SemanticTypography{}};
    component::RetainedSurfaceId surface;
    component::RetainedSurfaceId pulse;
    std::array<runtime::Size, 3> sizes;
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

struct TagRow final {
    TagGroupItem item;
    runtime::ComponentId owner;
    runtime::ComponentId tag;
    runtime::ComponentId content;
    input::InteractionId drag;
    Signal<String> text{String{}};
    Signal<TagColor> color{TagColor{PresetColor::Default}};
    Signal<bool> disabled{false};
    Signal<bool> closable{false};
    Signal<bool> checked{false};
    Signal<bool> rtl{false};
    bool handle_focused{};
    Signal<runtime::SemanticForeground> handle_color{runtime::SemanticForeground{}};
    Signal<runtime::SemanticTypography> handle_font{runtime::SemanticTypography{}};
};

struct TagGroupState final {
    runtime::NodeId node;
    std::vector<TagRow> rows;
    TagValues selected;
    bool controlled{};
    bool checkable{};
    bool multiple{};
    bool draggable{};
    bool disabled{};
    bool visible{true};
    bool rtl{};
    std::uint64_t revision{};
    bool reconciling{};
    std::optional<std::vector<TagGroupItem>> pending_items;
    std::optional<input::PointerIdentity> pointer;
    String dragging;
    runtime::Point origin;
    runtime::Point drag_delta;
    runtime::Rect clip;
    bool moved{};
    std::optional<input::InteractionId> capture;
    std::function<void(TagValues)> on_reorder;
    std::function<void(TagValues)> on_change;
    std::function<void(String, TagCloseEvent&)> on_close;
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

bool motion_allowed(WindowComponentServices& services, runtime::ComponentId id) {
    return services.focus().state().window_active && services.components().branch_active(id) &&
           animation::resolve_motion_policy(services.components().theme_scope(id)->snapshot(),
                                            services.motion_preference())
               .enabled();
}

std::int64_t duration(WindowComponentServices& services, runtime::ComponentId id) {
    return animation::resolve_motion_tokens(services.components().theme_scope(id)->snapshot())
        .fast.count_microseconds();
}

float progress(animation::AnimationTime now, animation::AnimationTime start, std::int64_t duration) {
    return duration <= 0
               ? 1
               : std::clamp(float(std::max<std::int64_t>(0, now.count_microseconds() - start.count_microseconds())) /
                                float(duration),
                            0.0F, 1.0F);
}

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

void validate(Status value) {
    if (value < Status::None || value > Status::Warning) {
        throw std::invalid_argument("invalid status");
    }
}

void validate(const TagColor& value) {
    if (const auto* preset = std::get_if<PresetColor>(&value);
        preset && (*preset < PresetColor::Default || *preset > PresetColor::Pink)) {
        throw std::invalid_argument("invalid Tag preset");
    }
    if (const auto* status = std::get_if<Status>(&value)) {
        validate(*status);
    }
}

void validate_count(int value) {
    if (value < 0) {
        throw std::invalid_argument("Badge count/overflowCount must be non-negative");
    }
}

void validate(BadgeSize value) {
    if (value != BadgeSize::Default && value != BadgeSize::Small) {
        throw std::invalid_argument("invalid Badge size");
    }
}

void validate(LogicalOffset value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) {
        throw std::invalid_argument("invalid Badge offset");
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

void set_active(WindowComponentServices& services, runtime::ComponentId id, bool shown) {
    if (services.components().set_branch_active(id, shown)) {
        services.mark_scene_structure_dirty();
    }
}

component::RetainedSurfaceId surface(WindowComponentServices& services, runtime::ComponentId id,
                                     std::optional<input::InteractionId> hit = {}) {
    auto& build = runtime::require_component_build_context();
    const auto node = build.root(id);
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    graphics::QuadInstance quad;
    quad.opacity = 0;
    const auto result = services.surfaces().create(id, node, fragment, hit, std::span{&quad, 1});
    build.on_resource_cleanup(id, [&services, result] { services.surfaces().destroy(result); });
    return result;
}

void paint(WindowComponentServices& services, component::RetainedSurfaceId surface, const runtime::Node& node,
           runtime::Rect rect, Color color, float radius, runtime::Rect clip, float focus_width = 0,
           float focus_offset = 0, Color focus_color = {}, float focus_opacity = 0) {
    graphics::QuadInstance quad;
    quad.bounds = {rect.x, rect.y, rect.width, rect.height};
    quad.color = {color.red(), color.green(), color.blue(), color.alpha()};
    quad.opacity = node.opacity;
    quad.corner_radius = std::min(radius, std::min(rect.width, rect.height) / 2);
    quad.translation = {node.translation.x, node.translation.y};
    static_cast<void>(services.surfaces().update(surface, std::span{&quad, 1}));
    component::RetainedSurfaceEffects effects;
    effects.shape = {rect, quad.corner_radius};
    effects.translation = node.translation;
    effects.focus_enabled = focus_width > 0;
    effects.focus_width = focus_width;
    effects.focus_offset = focus_offset;
    effects.focus_color = focus_color;
    effects.focus_opacity = focus_opacity;
    effects.ancestor_clip = graphics::EffectClip{1, clip};
    static_cast<void>(services.surfaces().update_effects(surface, effects));
}
} // namespace

TagBadgeComponentHost::TagBadgeComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

TagBadgeComponentHost::~TagBadgeComponentHost() {
    services_->detach(*this);
}

void* TagBadgeComponentHost::begin_mount() noexcept {
    auto* previous = active_status_components;
    active_status_components = this;
    return previous;
}

void TagBadgeComponentHost::end_mount(void* previous) noexcept {
    active_status_components = static_cast<TagBadgeComponentHost*>(previous);
}

void TagBadgeComponentHost::on_destroy() noexcept {
    std::erase_if(tags_, [this](auto id) { return !services_->components().contains(id); });
    std::erase_if(badges_, [this](auto id) { return !services_->components().contains(id); });
    std::erase_if(groups_, [this](auto id) { return !services_->components().contains(id); });
}

TagSnapshot TagBadgeComponentHost::tag_snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<TagState>(id);
    if (!s) {
        throw std::out_of_range("stale Tag");
    }
    return s->snapshot;
}

BadgeSnapshot TagBadgeComponentHost::badge_snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<BadgeState>(id);
    if (!s) {
        throw std::out_of_range("stale Badge");
    }
    return s->snapshot;
}

void TagBadgeComponentHost::refresh_tag(runtime::ComponentId id, bool geometry) {
    const auto storage = services_->components().retain_state<TagState>(id);
    auto* s = services_->components().state<TagState>(id);
    if (!s) {
        return;
    }
    if (s->snapshot.closing && s->group.valid()) {
        const auto focused = services_->focus().state().focused;
        const auto* hit = focused ? services_->interactions().find(*focused) : nullptr;
        bool inside = false;
        for (auto owner = hit ? std::optional{hit->component} : std::nullopt; owner;
             owner = services_->components().parent(*owner)) {
            if (*owner == id) {
                inside = true;
                break;
            }
        }
        const auto* group = services_->components().state<TagGroupState>(s->group);
        if (inside && group) {
            const auto found =
                std::find_if(group->rows.begin(), group->rows.end(), [&](const auto& row) { return row.tag == id; });
            const auto index = std::size_t(found - group->rows.begin());
            for (std::size_t offset = 1; offset < group->rows.size(); ++offset) {
                const auto& row = group->rows[(index + offset) % group->rows.size()];
                const auto view = tag_snapshot(row.tag);
                if (!row.item.disabled && view.visible && !view.closing) {
                    services_->focus().defer_focus(group->draggable ? row.drag : view.interaction,
                                                   input::FocusModality::keyboard);
                    break;
                }
            }
            s = services_->components().state<TagState>(id);
            if (!s) {
                return;
            }
        }
    }
    if (s->motion_start && (!s->animated || !motion_allowed(*services_, id) || s->motion_duration == 0)) {
        s->motion_start.reset();
        s->snapshot.motion = 1;
        if (s->snapshot.closing) {
            s->snapshot.closing = false;
            s->snapshot.visible = false;
            if (s->destroy_on_close) {
                services_->destroy(id);
                return;
            }
        }
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().tag();
    auto p = t.colors.normal;
    if (const auto* preset = std::get_if<PresetColor>(&s->color)) {
        p = t.colors.presets[static_cast<std::size_t>(*preset)];
    } else if (const auto* status = std::get_if<Status>(&s->color)) {
        p = t.colors.statuses[static_cast<std::size_t>(*status)];
    } else {
        const auto c = std::get<Color>(s->color);
        p = {c, c, Color::rgba8(255, 255, 255)};
    }
    if (!s->checkable && s->variant == TagVariant::Solid) {
        p = {p.text, p.text, t.colors.checked_text};
    }
    if (!s->checkable && s->variant == TagVariant::Filled) {
        p.border = Color{};
    }
    if (s->checkable) {
        const bool pressed = s->press.pressed() || s->focus.keyboard_pressed;
        p = {pressed               ? t.colors.primary_active
             : s->snapshot.checked ? (s->hovered ? t.colors.primary_hover : t.colors.primary)
             : s->hovered          ? t.colors.hover_background
                                   : Color{},
             Color{},
             pressed || s->snapshot.checked ? t.colors.checked_text
             : s->hovered                   ? t.colors.primary
                                            : t.colors.normal.text};
    }
    if (s->snapshot.disabled) {
        p = {s->checkable && !s->snapshot.checked ? Color{} : t.colors.disabled_background, t.colors.normal.border,
             t.colors.disabled_text};
    }
    s->snapshot.palette = p;
    s->foreground.set(foreground(p.text));
    s->close_foreground.set(foreground(s->snapshot.disabled                   ? t.colors.disabled_text
                                       : s->hovered || s->focus.focus_visible ? t.colors.close_hover
                                                                              : t.colors.close));
    const auto& m = t.metrics;
    s->font.set({m.font_family, 400, false, m.font_size, m.line_height});
    s->close_font.set({m.font_family, 400, false, std::max(1.0F, m.close_size), std::max(1.0F, m.close_size)});
    services_->nodes().require(s->node).out_of_flow = !s->snapshot.visible && !s->snapshot.closing;
    set_active(*services_, id, s->snapshot.visible || s->snapshot.closing);
    set_active(*services_, s->snapshot.close, s->snapshot.closable && !s->checkable);
    bool eligibility_changed = false;
    if (s->snapshot.interaction.valid()) {
        const bool eligible = s->snapshot.visible && !s->snapshot.closing && !s->snapshot.disabled &&
                              (s->checkable || s->snapshot.closable);
        eligibility_changed = services_->interactions().set_eligible(s->snapshot.interaction, eligible);
        if (!eligible) {
            s->hovered = false;
            static_cast<void>(s->press.reset());
            services_->pointer().cancel_interaction(s->snapshot.interaction);
        }
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
    const bool blocked = !s->snapshot.visible || s->snapshot.closing || s->snapshot.disabled;
    services_->set_input_blocked(id, blocked);
    if (!services_->components().contains(id)) {
        return;
    }
    if (eligibility_changed) {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::HitTest);
        services_->focus().synchronize();
    }
}

void TagBadgeComponentHost::activate(runtime::ComponentId id) {
    auto* s = services_->components().state<TagState>(id);
    if (!s || !active_ || s->snapshot.disabled || !s->snapshot.visible || s->snapshot.closing || s->closing_callback) {
        return;
    }
    const auto revision = s->revision;
    if (s->checkable) {
        const auto callback = s->on_change;
        const bool next = !s->snapshot.checked;
        if (!s->controlled_checked) {
            s->snapshot.checked = next;
            refresh_tag(id, false);
        }
        try {
            if (callback) {
                callback(next);
            }
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
        return;
    }
    if (!s->snapshot.closable) {
        return;
    }
    const auto legacy = s->on_close;
    const auto callback = s->on_close_event;
    s->closing_callback = true;
    TagCloseEvent event;
    try {
        if (callback) {
            callback(event);
        }
        if (!event.prevented && services_->components().contains(id) && legacy) {
            legacy();
        }
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
    s = services_->components().state<TagState>(id);
    if (!s) {
        return;
    }
    s->closing_callback = false;
    if (event.prevented || s->revision != revision || s->controlled_visible) {
        return;
    }
    s->snapshot.closing = true;
    s->snapshot.motion = 1;
    s->motion_duration = duration(*services_, id);
    s->motion_start = services_->animation_time();
    const auto group = s->group;
    refresh_tag(id, true);
    if (group.valid()) {
        refresh_group(group);
    }
    synchronize_auxiliary_motion();
    services_->request_frame();
}

runtime::ComponentId TagBadgeComponentHost::mount_tag(const TagProps& props, const TagContent& content,
                                                      const TagSlots& slots, bool checkable, bool checked,
                                                      std::function<void(bool)> change) {
    validate(read_prop(props.color_));
    if (read_prop(props.variant_) < TagVariant::Outlined || read_prop(props.variant_) > TagVariant::Solid) {
        throw std::invalid_argument("Invalid Tag variant");
    }
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<TagState>();
    const auto storage = build.host().retain_state<TagState>(id);
    auto& s = *storage;
    s.node = build.root(id);
    s.checkable = checkable;
    s.variant = read_prop(props.variant_);
    s.rtl = read_prop(props.rtl_);
    s.animated = read_prop(props.animated_);
    s.controlled_visible = props.controlled_visible_;
    s.destroy_on_close = props.destroy_on_close_;
    s.on_close_event = props.on_close_event_;
    s.motion_start = services.animation_time();
    s.motion_duration = duration(services, id);
    s.has_icon = slots.icon.has_value();
    s.color = read_prop(props.color_);
    s.bordered = read_prop(props.bordered_);
    s.snapshot.checked = checked;
    s.snapshot.visible = read_prop(props.visible_);
    s.snapshot.disabled = read_prop(props.disabled_);
    s.snapshot.closable = read_prop(props.closable_);
    s.on_close = props.on_close_;
    s.on_change = std::move(change);
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    s.viewport = region(services, id, Content{[] {}}, s.foreground, s.font);
    services.nodes().require(services.components().root(s.viewport)).clip_content = true;
    services.nodes().require(services.components().root(s.viewport)).clip_decorations = true;
    s.snapshot.icon = region(services, s.viewport, Content{[slot = slots.icon] {
                                 if (slot) {
                                     SlotContentAccess::function (*slot)();
                                 }
                             }},
                             s.foreground, s.font);
    if (!services.components().contains(id)) {
        return id;
    }
    s.snapshot.content = region(services, s.viewport, Content{[content] {
                                    FormPrivateEditorScope private_scope;
                                    SlotContentAccess::function(content)();
                                }},
                                s.foreground, s.font);
    if (!services.components().contains(id)) {
        return id;
    }
    s.snapshot.close = region(services, s.viewport, Content{[close = slots.close] {
                                  if (close) {
                                      SlotContentAccess::function (*close)();
                                  } else {
                                      Icon(IconProps{}.name(IconName::CloseOutlined));
                                  }
                              }},
                              s.close_foreground, s.close_font);
    if (!services.components().contains(id)) {
        return id;
    }
    services.layout().set_layout(services.components().root(s.viewport),
                                 layout::ComponentLayout{[](layout::LayoutEngine&, runtime::NodeId,
                                                            layout::Constraints c) { return c.constrain({}); },
                                                         [](layout::LayoutEngine&, runtime::NodeId, runtime::Rect) {}});
    const auto action = checkable ? s.viewport : s.snapshot.close;
    const auto action_node = services.components().root(action);
    s.snapshot.interaction = services.interactions().create({action, action_node, {}, true, true, {}, true, true});
    // A static Tag retains an ineligible close action so closable can change without remounting.
    s.surface = surface(services, id);
    {
        const auto fragment = build.register_scene_fragment(action, runtime::SceneFragmentPlacement::before_children);
        services.scene_composer().set_fragment(fragment, {}, s.snapshot.interaction);
    }
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.border = services.surfaces().create_content_range(fragment, {});
    build.on_resource_cleanup(id, [&services, id, node = s.node, hit = s.snapshot.interaction, border = s.border] {
        services.pointer().cancel_interaction(hit);
        services.focus().cancel_interaction(hit);
        services.interactions().remove(hit);
        services.surfaces().destroy_content_range(border);
        services.layout().remove_layout(node);
        services.interactions().set_input_blocked(id, false);
    });
    input::InteractionHandlers pointer;
    pointer.target = [this, id](input::PointerDispatchContext& context) {
        auto* s = services_->components().state<TagState>(id);
        if (!s) {
            return;
        }
        if (context.kind() == input::PointerEventKind::enter) {
            s->hovered = true;
        }
        if (context.kind() == input::PointerEventKind::leave || context.kind() == input::PointerEventKind::cancel) {
            s->hovered = false;
        }
        const auto result = s->press.dispatch(context, s->snapshot.interaction,
                                              !s->snapshot.disabled && s->snapshot.visible && !s->snapshot.closing);
        refresh_tag(id, false);
        context.stop_propagation();
        if (result.activate) {
            activate(id);
        }
    };
    services.interactions().set_handlers(s.snapshot.interaction, std::move(pointer));
    input::FocusHandlers focus;
    focus.state_changed = [this, id](input::FocusPresentation state) {
        if (auto* s = services_->components().state<TagState>(id)) {
            s->focus = state;
            refresh_tag(id, false);
        }
    };
    focus.activation_allowed = [this, id] {
        const auto* s = services_->components().state<TagState>(id);
        return s && !s->snapshot.disabled && !s->snapshot.closing && s->snapshot.visible &&
               (s->checkable || s->snapshot.closable);
    };
    focus.activate = [this, id] {
        activate(id);
    };
    services.interactions().set_focus_handlers(s.snapshot.interaction, std::move(focus));
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                auto& s = *services_->components().state<TagState>(id);
                if (!s.snapshot.visible && !s.snapshot.closing) {
                    return runtime::Size{};
                }
                const auto& m = services_->components().theme_scope(id)->snapshot().tag().metrics;
                const float edge = m.padding_inline + m.border_width;
                float available = std::max(0.0F, limits.max_width - edge * 2);
                const float close = s.snapshot.closable && !s.checkable ? std::min(m.close_size, available) : 0;
                s.sizes[2] = {close, close};
                available = std::max(0.0F, available - close - (close > 0 ? m.close_gap : 0));
                s.sizes[0] = s.has_icon ? engine.measure_child(services_->components().root(s.snapshot.icon),
                                                               {0, available, 0, limits.max_height})
                                        : runtime::Size{};
                const float gap = s.has_icon ? std::min(m.icon_gap, std::max(0.0F, available - s.sizes[0].width)) : 0;
                s.sizes[1] =
                    engine.measure_child(services_->components().root(s.snapshot.content),
                                         {0, std::max(0.0F, available - s.sizes[0].width - gap), 0, limits.max_height});
                // Measure the retained close icon even when its box is externally positioned.
                static_cast<void>(
                    engine.measure_child(services_->components().root(s.snapshot.close), {0, close, 0, close}));
                const auto natural = limits.constrain(
                    {edge * 2 + s.sizes[0].width + gap + s.sizes[1].width + close + (close > 0 ? m.close_gap : 0),
                     std::max({m.line_height, s.sizes[0].height, s.sizes[1].height, close}) + m.border_width * 2});
                static_cast<void>(engine.measure_child(services_->components().root(s.viewport),
                                                       layout::Constraints::fixed(natural.width, natural.height)));
                return natural;
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                auto& s = *services_->components().state<TagState>(id);
                if (!s.snapshot.visible && !s.snapshot.closing) {
                    return;
                }
                const auto& m = services_->components().theme_scope(id)->snapshot().tag().metrics;
                const float fraction = s.snapshot.closing ? s.snapshot.motion : .8F + .2F * s.snapshot.motion;
                const runtime::Rect visual{bounds.x, bounds.y + bounds.height * (1 - fraction) / 2, bounds.width,
                                           bounds.height * fraction};
                s.snapshot.visual_bounds = visual;
                engine.place_child(services_->components().root(s.viewport), visual, true, true);
                float x = bounds.x + std::min(m.padding_inline + m.border_width, bounds.width / 2);
                const std::array ids{s.snapshot.icon, s.snapshot.content, s.snapshot.close};
                for (std::size_t i = 0; i < ids.size(); ++i) {
                    if ((i == 0 && !s.has_icon) || (i == 2 && (!s.snapshot.closable || s.checkable))) {
                        continue;
                    }
                    auto size = s.sizes[i];
                    size.width = std::min(size.width, std::max(0.0F, bounds.x + bounds.width - x));
                    size.height = std::min(size.height, bounds.height);
                    engine.place_child(services_->components().root(ids[i]),
                                       {s.rtl ? bounds.x + bounds.width - (x - bounds.x) - size.width : x,
                                        bounds.y + (bounds.height - size.height) / 2, size.width, size.height});
                    x += size.width;
                    if (i == 0 && s.has_icon) {
                        x += m.icon_gap;
                    }
                    if (i == 1 && s.snapshot.closable && !s.checkable) {
                        x += m.close_gap;
                    }
                }
            }});
    tags_.push_back(id);
    refresh_tag(id, true);
    auto& scope = build.scope(id);
    connect_prop(scope, props.rtl_, [this, id](bool v) {
        if (auto* s = services_->components().state<TagState>(id)) {
            s->rtl = v;
            refresh_tag(id, true);
        }
    });
    connect_prop(scope, props.animated_, [this, id](bool v) {
        if (auto* s = services_->components().state<TagState>(id)) {
            s->animated = v;
            synchronize_auxiliary_motion();
        }
    });
    connect_prop(scope, props.variant_, [this, id](TagVariant v) {
        if (v < TagVariant::Outlined || v > TagVariant::Solid) {
            throw std::invalid_argument("Invalid Tag variant");
        }
        if (auto* s = services_->components().state<TagState>(id)) {
            s->variant = v;
            refresh_tag(id, false);
        }
    });
    connect_prop(scope, props.color_, [this, id](TagColor v) {
        validate(v);
        if (auto* s = services_->components().state<TagState>(id)) {
            s->color = v;
            refresh_tag(id, false);
        }
    });
    connect_prop(scope, props.bordered_, [this, id](bool v) {
        if (auto* s = services_->components().state<TagState>(id)) {
            s->bordered = v;
            refresh_tag(id, false);
        }
    });
    connect_prop(scope, props.closable_, [this, id](bool v) {
        if (auto* s = services_->components().state<TagState>(id)) {
            s->snapshot.closable = v;
            refresh_tag(id, true);
        }
    });
    connect_prop(scope, props.visible_, [this, id](bool v) {
        if (auto* s = services_->components().state<TagState>(id)) {
            ++s->revision;
            if (s->snapshot.visible != v) {
                if (v) {
                    s->snapshot.visible = true;
                    s->snapshot.closing = false;
                    s->snapshot.motion = 0;
                } else if (s->snapshot.visible) {
                    s->snapshot.visible = false;
                    s->snapshot.closing = true;
                    s->snapshot.motion = 1;
                }
                s->motion_start = services_->animation_time();
                s->motion_duration = duration(*services_, id);
            }
            refresh_tag(id, true);
        }
    });
    connect_prop(scope, props.disabled_, [this, id](bool v) {
        if (auto* s = services_->components().state<TagState>(id)) {
            s->snapshot.disabled = v;
            refresh_tag(id, false);
        }
    });
    const auto theme = build.theme_scope();
    s.subscriptions[0] = theme->capture([this, id](auto) { refresh_tag(id, false); },
                                        [theme] { static_cast<void>(theme->tag_colors()); });
    s.subscriptions[1] = theme->capture([this, id](auto) { refresh_tag(id, true); },
                                        [theme] { static_cast<void>(theme->tag_metrics()); });
    return id;
}

void TagBadgeComponentHost::mount(const TagProps& props, const TagContent& content, const TagSlots& slots) {
    static_cast<void>(mount_tag(props, content, slots, false, false, {}));
}

void TagBadgeComponentHost::mount(const CheckableTagProps& props, const TagContent& content, const TagSlots& slots) {
    TagProps tag;
    tag.disabled(props.disabled_).rtl(props.rtl_).animated(props.animated_).layout(props.layout_);
    const auto id =
        mount_tag(tag, content, slots, true,
                  props.controlled_checked_ ? read_prop(props.checked_) : props.default_checked_, props.on_change_);
    services_->components().state<TagState>(id)->controlled_checked = props.controlled_checked_;
    if (!props.controlled_checked_) {
        return;
    }
    connect_prop(services_->components().scope(id), props.checked_, [this, id](bool value) {
        if (auto* s = services_->components().state<TagState>(id)) {
            s->snapshot.checked = value;
            refresh_tag(id, false);
        }
    });
}

void TagBadgeComponentHost::replace_badge_slot(runtime::ComponentId id, bool count,
                                               std::shared_ptr<const BadgeRenderer> renderer) {
    auto* s = services_->components().state<BadgeState>(id);
    if (!s) {
        return;
    }
    auto& current = count ? s->count_renderer : s->content_renderer;
    if (current == renderer) {
        return;
    }
    const auto revision = ++s->revision;
    const auto old = count ? s->custom_count : s->custom_content;
    runtime::ComponentId replacement;
    const auto parent = count ? s->snapshot.indicator : s->snapshot.content;
    try {
        if (renderer) {
            const auto color = s->count_color;
            const auto font = s->count_font;
            append_retained_slot(*services_, parent, Content{[&] {
                replacement = region(*services_, parent, Content{[renderer, count] {
                    if (count) {
                        FormPrivateEditorScope private_scope;
                        if (renderer->render) {
                            renderer->render();
                        }
                    } else if (renderer->render) {
                        renderer->render();
                    }
                }},
                                     color, font);
            }});
        }
        s = services_->components().state<BadgeState>(id);
        if (!s || s->revision != revision) {
            if (replacement.valid()) {
                services_->destroy(replacement);
            }
            return;
        }
        if (count) {
            s->count_renderer = renderer;
            s->custom_count = replacement;
        } else {
            s->content_renderer = renderer;
            s->custom_content = replacement;
            set_active(*services_, s->base_content, !renderer);
            services_->nodes().require(services_->components().root(s->base_content)).out_of_flow = bool(renderer);
        }
        if (old.valid()) {
            services_->destroy(old);
        }
        if (services_->components().contains(id)) {
            refresh_badge(id, true);
        }
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

void TagBadgeComponentHost::update_count(runtime::ComponentId id, BadgeCountValue value) {
    if (const auto* n = std::get_if<int>(&value)) {
        validate_count(*n);
    }
    if (const auto* n = std::get_if<double>(&value); n && !std::isfinite(*n)) {
        throw std::invalid_argument("Badge numeric value must be finite");
    }
    auto* s = services_->components().state<BadgeState>(id);
    if (!s) {
        return;
    }
    s->value = value;
    s->count = std::holds_alternative<int>(value) ? std::optional{std::get<int>(value)} : std::nullopt;
    replace_badge_slot(id, true,
                       std::holds_alternative<std::shared_ptr<const BadgeRenderer>>(value)
                           ? std::get<std::shared_ptr<const BadgeRenderer>>(value)
                           : nullptr);
    refresh_badge(id, true);
}

void TagBadgeComponentHost::refresh_badge(runtime::ComponentId id, bool geometry) {
    const auto storage = services_->components().retain_state<BadgeState>(id);
    auto* s = services_->components().state<BadgeState>(id);
    if (!s) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().badge();
    const auto previous = s->snapshot.display_count;
    const auto* string = std::get_if<String>(&s->value);
    const auto* number = std::get_if<double>(&s->value);
    const bool zero = (number && *number == 0) || (s->count && *s->count == 0) || (string && *string == String{u8"0"});
    const bool count =
        (s->count || number || (string && !string->empty()) || s->custom_count.valid()) && (!zero || s->show_zero);
    s->snapshot.status = !count && (s->status != Status::None || s->custom_color.has_value());
    s->snapshot.dot = s->snapshot.status || (s->dot && (!zero || s->show_zero));
    s->snapshot.shown = s->ribbon || s->snapshot.status || s->snapshot.dot || count;
    s->snapshot.ribbon = s->ribbon;
    s->snapshot.display_count = {};
    if (s->snapshot.shown && !s->snapshot.dot && s->count) {
        const auto value = *s->count > s->overflow ? std::to_string(s->overflow) + "+" : std::to_string(*s->count);
        s->snapshot.display_count = String::from_utf8(value).value();
    }
    if (string && count && !s->snapshot.dot) {
        s->snapshot.display_count = *string;
        const auto bytes = string->view().bytes();
        double numeric{};
        const auto parsed = std::from_chars(bytes.data(), bytes.data() + bytes.size(), numeric);
        if (parsed.ec == std::errc{} && parsed.ptr == bytes.data() + bytes.size() && std::isfinite(numeric) &&
            numeric > s->overflow) {
            s->snapshot.display_count = String::from_utf8(std::to_string(s->overflow) + "+").value();
        }
        auto digits = string->view().bytes();
        if (!digits.empty() && std::ranges::all_of(digits, [](char c) { return c >= '0' && c <= '9'; })) {
            const auto first = digits.find_first_not_of('0');
            digits = first == std::string_view::npos ? std::string_view{"0"} : digits.substr(first);
            const auto limit = std::to_string(s->overflow);
            if (digits.size() > limit.size() || (digits.size() == limit.size() && digits > limit)) {
                s->snapshot.display_count = String::from_utf8(limit + "+").value();
            }
        }
    }
    if (number && count && !s->snapshot.dot) {
        if (*number > s->overflow) {
            s->snapshot.display_count = String::from_utf8(std::to_string(s->overflow) + "+").value();
        } else {
            std::array<char, 64> buffer;
            const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), *number);
            if (result.ec != std::errc{}) {
                throw std::invalid_argument("Badge numeric formatting failed");
            }
            s->snapshot.display_count =
                String::from_utf8(std::string_view{buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())})
                    .value();
        }
    }
    s->snapshot.color = s->custom_color.value_or(
        s->snapshot.status ? t.colors.statuses[static_cast<std::size_t>(s->status)] : t.colors.background);
    if (previous != s->snapshot.display_count) {
        s->snapshot.number_motion = 1;
        s->number_start.reset();
        const auto numeric = [](const String& value) {
            double number{};
            const auto bytes = value.view().bytes();
            const auto result = std::from_chars(bytes.data(), bytes.data() + bytes.size(), number);
            return result.ec == std::errc{} && result.ptr == bytes.data() + bytes.size() && std::isfinite(number);
        };
        if (s->animated && !s->ribbon && numeric(previous) && numeric(s->snapshot.display_count) &&
            motion_allowed(*services_, id) && duration(*services_, id) > 0) {
            s->old_count.set(previous);
            double from{};
            double to{};
            const auto old_bytes = previous.view().bytes();
            const auto new_bytes = s->snapshot.display_count.view().bytes();
            static_cast<void>(std::from_chars(old_bytes.data(), old_bytes.data() + old_bytes.size(), from));
            static_cast<void>(std::from_chars(new_bytes.data(), new_bytes.data() + new_bytes.size(), to));
            s->direction = to >= from ? 1 : -1;
            s->number_start = services_->animation_time();
            s->duration = duration(*services_, id);
            s->snapshot.number_motion = 0;
            services_->request_frame();
        }
    }
    set_active(*services_, s->current_count, !s->custom_count.valid() && !s->snapshot.dot);
    set_active(*services_, s->snapshot.previous_count, s->number_start.has_value() && !s->snapshot.dot);
    services_->components().set_branch_active(id, s->visible);
    services_->nodes().require(s->node).out_of_flow = !s->visible;
    s->count_text.set(s->snapshot.display_count);
    s->count_color.set(foreground(t.colors.count_text));
    s->text_color.set(foreground(t.colors.text));
    const auto& m = t.metrics;
    const float height = s->size == BadgeSize::Small ? m.indicator_height_small : m.indicator_height;
    s->count_font.set({m.font_family, m.font_weight, false, m.font_size, height});
    s->text_font.set({m.font_family, 400, false, m.text_font_size, m.text_line_height});
    if (s->ribbon) {
        s->snapshot.color = s->custom_color.value_or(t.colors.statuses[static_cast<std::size_t>(Status::Processing)]);
        const auto color = s->snapshot.color;
        s->fold_color.set({color.red() * .7F, color.green() * .7F, color.blue() * .7F, color.alpha()});
        s->fold_font.set({m.font_family, 400, false, m.text_gap, m.text_gap});
        const bool right = (s->placement == RibbonPlacement::End) != s->rtl;
        const IconVector triangle{{0, 0, 1, 1},
                                  {{IconColorRole::Primary,
                                    {IconMove{{0, 0}}, IconLine{{1, 0}}, IconLine{{right ? 0.F : 1.F, 1}}, IconClose{}},
                                    1}}};
        s->fold_icon.set(IconSource{triangle});
    }
    set_active(*services_, s->fold, s->ribbon);
    set_active(*services_, s->snapshot.indicator, s->snapshot.shown);
    set_active(*services_, s->snapshot.text, s->snapshot.shown && !s->label.get().empty());
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry
                                                    : runtime::DirtyFlags::Material);
}

void TagBadgeComponentHost::mount(const BadgeProps& props, const std::optional<BadgeContent>& content) {
    if (props.count_) {
        validate_count(read_prop(*props.count_));
    }
    validate_count(read_prop(props.overflow_count_));
    validate(read_prop(props.status_));
    validate(read_prop(props.size_));
    validate(read_prop(props.offset_));
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<BadgeState>();
    const auto storage = build.host().retain_state<BadgeState>(id);
    auto& s = *storage;
    s.node = build.root(id);
    s.wrapped = content.has_value() || read_prop(props.content_) != nullptr;
    s.visible = read_prop(props.visible_);
    s.rtl = read_prop(props.rtl_);
    s.animated = read_prop(props.animated_);
    s.custom_color = read_prop(props.color_);
    if (props.count_) {
        s.count = read_prop(*props.count_);
        s.value = *s.count;
    }
    s.overflow = read_prop(props.overflow_count_);
    s.show_zero = read_prop(props.show_zero_);
    s.dot = read_prop(props.dot_);
    s.status = read_prop(props.status_);
    s.size = read_prop(props.size_);
    s.offset = read_prop(props.offset_);
    s.label.set(read_prop(props.text_));
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    // Preserve the child's inherited semantic style; Badge only styles its own indicator and label.
    build.mount_slot(id, Content{[&] {
                         auto& context = runtime::require_component_build_context();
                         s.snapshot.content = context.mount_component<int>(0);
                         const auto node = context.root(s.snapshot.content);
                         services.layout().set_layout(node, layout::BoxLayout{});
                         context.on_resource_cleanup(s.snapshot.content,
                                                     [&services, node] { services.layout().remove_layout(node); });
                         context.mount_slot(s.snapshot.content, Content{[&] {
                                                auto& inner = runtime::require_component_build_context();
                                                s.base_content = inner.mount_component<int>(0);
                                                const auto node = inner.root(s.base_content);
                                                services.layout().set_layout(node, layout::BoxLayout{});
                                                inner.on_resource_cleanup(s.base_content, [&services, node] {
                                                    services.layout().remove_layout(node);
                                                });
                                                if (content) {
                                                    inner.mount_slot(s.base_content, Content{[content] {
                                                                         SlotContentAccess::function (*content)();
                                                                     }});
                                                }
                                            }});
                     }});
    if (!services.components().contains(id)) {
        return;
    }
    s.snapshot.indicator = region(services, id, Content{[] {}}, s.count_color, s.count_font);
    services.nodes().require(services.components().root(s.snapshot.indicator)).clip_content = true;
    s.current_count =
        region(services, s.snapshot.indicator, Content{[value = s.count_text] { Text(TextProps{}.content(value)); }},
               s.count_color, s.count_font);
    s.snapshot.previous_count =
        region(services, s.snapshot.indicator, Content{[value = s.old_count] { Text(TextProps{}.content(value)); }},
               s.count_color, s.count_font);
    s.fold = region(services, id, Content{[icon = s.fold_icon] { Icon(IconProps{}.source(icon)); }}, s.fold_color,
                    s.fold_font);
    s.surface = surface(services, s.snapshot.indicator);
    const auto pulse_fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::after_children);
    s.pulse = services.surfaces().create_content_range(pulse_fragment, {});
    build.on_resource_cleanup(id, [&services, pulse = s.pulse] { services.surfaces().destroy_content_range(pulse); });
    s.snapshot.text = region(services, id, Content{[value = s.label] { Text(TextProps{}.content(value)); }},
                             s.text_color, s.text_font);
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                auto& s = *services_->components().state<BadgeState>(id);
                const auto& m = services_->components().theme_scope(id)->snapshot().badge().metrics;
                const float height = s.ribbon                     ? m.text_line_height
                                     : s.snapshot.status          ? m.status_size
                                     : s.snapshot.dot             ? m.dot_size
                                     : s.size == BadgeSize::Small ? m.indicator_height_small
                                                                  : m.indicator_height;
                const auto text = engine.measure_child(
                    services_->components().root(s.snapshot.indicator),
                    {0, std::numeric_limits<float>::infinity(), 0, std::numeric_limits<float>::infinity()});
                const float width =
                    s.custom_count.valid() && !s.ribbon && !s.snapshot.dot ? text.width
                    : s.snapshot.dot
                        ? height
                        : std::max(height,
                                   text.width + (s.snapshot.display_count.size_bytes() > 1 ? m.padding_inline * 2 : 0));
                s.sizes[1] = s.snapshot.shown
                                 ? runtime::Size{width, s.custom_count.valid() && !s.snapshot.dot
                                                            ? std::max(s.ribbon ? height : 0.F, text.height)
                                                            : height}
                                 : runtime::Size{};
                s.sizes[0] = s.wrapped ? engine.measure_child(services_->components().root(s.snapshot.content),
                                                              {0, limits.max_width, 0, limits.max_height})
                                       : runtime::Size{};
                const float used = s.wrapped ? s.sizes[0].width : s.sizes[1].width;
                const bool label = s.snapshot.shown && !s.label.get().empty();
                s.sizes[2] = label ? engine.measure_child(services_->components().root(s.snapshot.text),
                                                          {0, std::max(0.0F, limits.max_width - used - m.text_gap), 0,
                                                           limits.max_height})
                                   : runtime::Size{};
                if (s.ribbon) {
                    static_cast<void>(engine.measure_child(services_->components().root(s.fold),
                                                           layout::Constraints::fixed(m.text_gap, m.text_gap)));
                }
                return limits.constrain(
                    {used + (label ? m.text_gap + s.sizes[2].width : 0),
                     std::max(s.wrapped ? s.sizes[0].height : s.sizes[1].height, s.sizes[2].height)});
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                auto& s = *services_->components().state<BadgeState>(id);
                const auto& m = services_->components().theme_scope(id)->snapshot().badge().metrics;
                const auto child = s.sizes[0];
                const auto indicator = s.sizes[1];
                const auto label = s.sizes[2];
                const float content_x = s.rtl && label.width > 0 ? bounds.x + label.width + m.text_gap : bounds.x;
                if (s.wrapped) {
                    engine.place_child(services_->components().root(s.snapshot.content),
                                       {content_x, bounds.y, child.width, child.height});
                }
                const float x =
                    s.ribbon ? (s.placement == RibbonPlacement::End) != s.rtl ? bounds.x + child.width - indicator.width
                                                                              : bounds.x
                    : s.wrapped
                        ? (s.rtl ? content_x - indicator.width / 2 : content_x + child.width - indicator.width / 2)
                    : s.rtl && label.width > 0 ? bounds.x + label.width + m.text_gap
                                               : bounds.x;
                const float y = s.ribbon    ? bounds.y + m.text_gap
                                : s.wrapped ? bounds.y - indicator.height / 2
                                            : bounds.y + (bounds.height - indicator.height) / 2;
                s.snapshot.indicator_bounds = {x + (s.rtl ? -s.offset.x : s.offset.x), y + s.offset.y, indicator.width,
                                               indicator.height};
                if (s.snapshot.shown) {
                    engine.place_child(services_->components().root(s.snapshot.indicator), s.snapshot.indicator_bounds,
                                       true, true);
                }
                if (s.ribbon) {
                    const bool right = (s.placement == RibbonPlacement::End) != s.rtl;
                    engine.place_child(services_->components().root(s.fold),
                                       {right ? s.snapshot.indicator_bounds.x + indicator.width - m.text_gap
                                              : s.snapshot.indicator_bounds.x,
                                        s.snapshot.indicator_bounds.y + indicator.height, m.text_gap, m.text_gap},
                                       true, true);
                }
                if (s.snapshot.shown && !s.label.get().empty()) {
                    engine.place_child(
                        services_->components().root(s.snapshot.text),
                        {s.rtl ? bounds.x : bounds.x + (s.wrapped ? child.width : indicator.width) + m.text_gap,
                         bounds.y + (bounds.height - label.height) / 2, label.width, label.height});
                }
            }});
    // Center the retained count inside the indicator's externally selected pill geometry.
    const auto indicator_node = services.components().root(s.snapshot.indicator);
    services.layout().set_layout(
        indicator_node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints c) {
                auto& s = *services_->components().state<BadgeState>(id);
                runtime::Size size;
                for (auto child : {s.current_count, s.snapshot.previous_count, s.custom_count}) {
                    if (child.valid() && services_->components().branch_active(child)) {
                        const auto v = engine.measure_child(services_->components().root(child), c);
                        size.width = std::max(size.width, v.width);
                        size.height = std::max(size.height, v.height);
                    }
                }
                return c.constrain(size);
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect r) {
                auto& s = *services_->components().state<BadgeState>(id);
                for (auto child : {s.current_count, s.snapshot.previous_count, s.custom_count}) {
                    if (!child.valid() || !services_->components().branch_active(child)) {
                        continue;
                    }
                    const auto size = services_->nodes().require(services_->components().root(child)).layout_size;
                    float y = r.y + (r.height - size.height) / 2;
                    if (s.number_start) {
                        y += s.direction * r.height *
                             (child == s.snapshot.previous_count ? -s.snapshot.number_motion
                                                                 : 1 - s.snapshot.number_motion);
                    }
                    engine.place_child(services_->components().root(child),
                                       {r.x + (r.width - size.width) / 2, y, size.width, size.height}, true, true);
                }
            }});
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    badges_.push_back(id);
    refresh_badge(id, true);
    auto& scope = build.scope(id);
    if (props.count_) {
        connect_prop(scope, *props.count_, [this, id](int v) {
            validate_count(v);
            if (services_->components().state<BadgeState>(id)) {
                update_count(id, BadgeCountValue{v});
            }
        });
    }
    connect_prop(scope, props.overflow_count_, [this, id](int v) {
        validate_count(v);
        if (auto* s = services_->components().state<BadgeState>(id)) {
            s->overflow = v;
            refresh_badge(id, true);
        }
    });
    connect_prop(scope, props.show_zero_, [this, id](bool v) {
        if (auto* s = services_->components().state<BadgeState>(id)) {
            s->show_zero = v;
            refresh_badge(id, true);
        }
    });
    connect_prop(scope, props.dot_, [this, id](bool v) {
        if (auto* s = services_->components().state<BadgeState>(id)) {
            s->dot = v;
            refresh_badge(id, true);
        }
    });
    connect_prop(scope, props.status_, [this, id](Status v) {
        validate(v);
        if (auto* s = services_->components().state<BadgeState>(id)) {
            const bool geometry = (s->status == Status::None) != (v == Status::None);
            s->status = v;
            refresh_badge(id, geometry);
        }
    });
    connect_prop(scope, props.text_, [this, id](String v) {
        if (auto* s = services_->components().state<BadgeState>(id)) {
            s->label.set(std::move(v));
            refresh_badge(id, true);
        }
    });
    connect_prop(scope, props.size_, [this, id](BadgeSize v) {
        validate(v);
        if (auto* s = services_->components().state<BadgeState>(id)) {
            s->size = v;
            refresh_badge(id, true);
        }
    });
    connect_prop(scope, props.offset_, [this, id](LogicalOffset v) {
        validate(v);
        if (auto* s = services_->components().state<BadgeState>(id)) {
            s->offset = v;
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry);
        }
    });
    if (props.string_count_) {
        connect_prop(scope, *props.string_count_,
                     [this, id](String v) { update_count(id, BadgeCountValue{std::move(v)}); });
    }
    if (props.value_count_) {
        connect_prop(scope, *props.value_count_, [this, id](BadgeCountValue v) { update_count(id, std::move(v)); });
    }
    connect_prop(scope, props.content_,
                 [this, id](std::shared_ptr<const BadgeRenderer> v) { replace_badge_slot(id, false, std::move(v)); });
    connect_prop(scope, props.rtl_, [this, id](bool v) {
        if (auto* s = services_->components().state<BadgeState>(id)) {
            s->rtl = v;
            refresh_badge(id, true);
        }
    });
    connect_prop(scope, props.visible_, [this, id](bool v) {
        if (auto* s = services_->components().state<BadgeState>(id)) {
            s->visible = v;
            refresh_badge(id, true);
            synchronize_auxiliary_motion();
        }
    });
    connect_prop(scope, props.animated_, [this, id](bool v) {
        if (auto* s = services_->components().state<BadgeState>(id)) {
            s->animated = v;
            synchronize_auxiliary_motion();
        }
    });
    connect_prop(scope, props.color_, [this, id](std::optional<Color> v) {
        if (auto* s = services_->components().state<BadgeState>(id)) {
            s->custom_color = v;
            refresh_badge(id, false);
        }
    });
    const auto theme = build.theme_scope();
    s.subscriptions[0] = theme->capture([this, id](auto) { refresh_badge(id, false); },
                                        [theme] { static_cast<void>(theme->badge_colors()); });
    s.subscriptions[1] = theme->capture([this, id](auto) { refresh_badge(id, true); },
                                        [theme] { static_cast<void>(theme->badge_metrics()); });
}

void TagBadgeComponentHost::mount(const RibbonProps& p, const BadgeContent& content) {
    if (read_prop(p.placement_) < RibbonPlacement::Start || read_prop(p.placement_) > RibbonPlacement::End) {
        throw std::invalid_argument("Invalid Ribbon placement");
    }
    BadgeProps badge;
    badge.count(p.text_).rtl(p.rtl_).visible(p.visible_).color(p.color_).contentRender(p.content_).layout(p.layout_);
    mount(badge, content);
    const auto id = badges_.back();
    auto* s = services_->components().state<BadgeState>(id);
    s->ribbon = true;
    s->placement = read_prop(p.placement_);
    auto& scope = services_->components().scope(id);
    connect_prop(scope, p.placement_, [this, id](RibbonPlacement v) {
        if (v < RibbonPlacement::Start || v > RibbonPlacement::End) {
            throw std::invalid_argument("Invalid Ribbon placement");
        }
        if (auto* s = services_->components().state<BadgeState>(id)) {
            s->placement = v;
            refresh_badge(id, true);
        }
    });
    connect_prop(scope, p.text_render_,
                 [this, id](std::shared_ptr<const BadgeRenderer> v) { replace_badge_slot(id, true, std::move(v)); });
    refresh_badge(id, true);
}

void TagBadgeComponentHost::on_window_active(bool value) {
    active_ = value;
    if (!value) {
        for (auto id : groups_) {
            auto* s = services_->components().state<TagGroupState>(id);
            if (!s) {
                continue;
            }
            const auto capture = s->capture;
            s->capture.reset();
            s->pointer.reset();
            s->dragging = {};
            s->moved = false;
            if (capture) {
                services_->pointer().cancel_pointer_interaction(*capture);
            }
        }
    }
    synchronize_auxiliary_motion();
}

void TagBadgeComponentHost::on_input_boundary_changed() {
    const auto ids = groups_;
    for (auto id : ids) {
        refresh_group(id);
    }
    synchronize_auxiliary_motion();
}

void TagBadgeComponentHost::synchronize_auxiliary_motion() {
    const auto ids = tags_;
    for (auto id : ids) {
        auto* s = services_->components().state<TagState>(id);
        if (!s || !s->motion_start) {
            continue;
        }
        if (!s->animated || !motion_allowed(*services_, id) || s->motion_duration == 0) {
            s->motion_start.reset();
            s->snapshot.motion = 1;
            const bool retire = s->snapshot.closing && s->destroy_on_close;
            if (s->snapshot.closing) {
                s->snapshot.closing = false;
                s->snapshot.visible = false;
            }
            if (retire) {
                services_->destroy(id);
            } else {
                refresh_tag(id, true);
            }
        }
    }
    for (auto id : badges_) {
        auto* s = services_->components().state<BadgeState>(id);
        if (!s) {
            continue;
        }
        if (!s->animated || !s->visible || !motion_allowed(*services_, id) || duration(*services_, id) == 0) {
            if (s->number_start || s->snapshot.number_motion != 1) {
                s->number_start.reset();
                s->snapshot.number_motion = 1;
                set_active(*services_, s->snapshot.previous_count, false);
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry);
            }
            if (s->processing_visible) {
                s->processing_visible = false;
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
            }
        }
    }
}

std::size_t TagBadgeComponentHost::tick_auxiliary(animation::AnimationTime now) {
    now = std::max(now, observed_time_);
    observed_time_ = now;
    std::size_t changed{};
    const auto tags = tags_;
    for (auto id : tags) {
        auto* s = services_->components().state<TagState>(id);
        if (!s || !s->motion_start) {
            continue;
        }
        const float value = progress(now, *s->motion_start, s->motion_duration);
        s->snapshot.motion = s->snapshot.closing ? 1 - value : value;
        if (value >= 1) {
            s->motion_start.reset();
            s->snapshot.motion = 1;
            if (s->snapshot.closing) {
                s->snapshot.closing = false;
                s->snapshot.visible = false;
                if (s->destroy_on_close) {
                    services_->destroy(id);
                    ++changed;
                    continue;
                }
            }
        }
        const auto group = s->group;
        refresh_tag(id, true);
        if (group.valid()) {
            refresh_group(group);
        }
        ++changed;
    }
    for (auto id : badges_) {
        auto* s = services_->components().state<BadgeState>(id);
        if (!s) {
            continue;
        }
        if (s->number_start) {
            s->snapshot.number_motion = progress(now, *s->number_start, s->duration);
            if (s->snapshot.number_motion >= 1) {
                s->number_start.reset();
                set_active(*services_, s->snapshot.previous_count, false);
            }
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry);
            ++changed;
        }
        if (s->processing_visible) {
            if (s->processing_started) {
                s->phase_time = now;
                s->processing_started = false;
            }
            const auto step = std::max<std::int64_t>(0, now.count_microseconds() - s->phase_time.count_microseconds());
            s->snapshot.processing_phase = std::fmod(s->snapshot.processing_phase + float(step) / 1000000.0F, 1.0F);
            s->phase_time = now;
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
            ++changed;
        } else {
            s->phase_time = now;
        }
    }
    return changed;
}

std::optional<animation::AnimationTime> TagBadgeComponentHost::next_auxiliary_deadline() const {
    for (auto id : tags_) {
        const auto* s = services_->components().state<TagState>(id);
        if (s && s->motion_start && motion_allowed(*services_, id)) {
            return std::max(observed_time_, services_->animation_time()) +
                   animation::AnimationDuration::microseconds(16667);
        }
    }
    for (auto id : badges_) {
        const auto* s = services_->components().state<BadgeState>(id);
        if (s && (s->number_start || s->processing_visible) && s->visible && motion_allowed(*services_, id)) {
            return std::max(observed_time_, services_->animation_time()) +
                   animation::AnimationDuration::microseconds(16667);
        }
    }
    return {};
}

TagBadgeComponentHost::GroupSnapshot TagBadgeComponentHost::group_snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<TagGroupState>(id);
    if (!s) {
        throw std::out_of_range("Stale TagGroup");
    }
    GroupSnapshot result;
    for (const auto& row : s->rows) {
        result.items.emplace_back(row.item.key, row.tag);
    }
    result.value = s->selected;
    result.dragging = s->moved;
    return result;
}

void TagBadgeComponentHost::refresh_group(runtime::ComponentId id) {
    try {

        const auto storage = services_->components().retain_state<TagGroupState>(id);
        auto* s = services_->components().state<TagGroupState>(id);
        if (!s) {
            return;
        }
        const auto revision = s->revision;
        const bool blocked = s->disabled || !s->visible || !active_ || !services_->interactions().component_active(id);
        const auto capture = s->capture;
        if (blocked || !s->draggable) {
            s->capture.reset();
            s->pointer.reset();
            s->dragging = {};
            s->moved = false;
            if (capture) {
                services_->pointer().cancel_pointer_interaction(*capture);
            }
        }
        s = services_->components().state<TagGroupState>(id);
        if (!s || s->revision != revision) {
            return;
        }
        services_->components().set_branch_active(id, s->visible);
        services_->nodes().require(s->node).out_of_flow = !s->visible;
        // Copy signals: disabling a rich child can synchronously run its blur callback.
        const auto rows = s->rows;
        const auto focused_handle = services_->focus().state().focused;
        const bool has_focused_handle = std::ranges::any_of(
            rows, [&](const auto& row) { return focused_handle == row.drag && !row.item.disabled; });
        bool first = true;
        for (const auto& row : rows) {
            s = services_->components().state<TagGroupState>(id);
            if (!s || s->revision != revision) {
                return;
            }
            const auto tag = tag_snapshot(row.tag);
            const bool disabled = s->disabled || row.item.disabled;
            services_->nodes().require(services_->components().root(row.owner)).out_of_flow =
                !tag.visible && !tag.closing;
            if ((tag.closing || !tag.visible) && services_->focus().state().focused == tag.interaction) {
                const auto current =
                    std::find_if(rows.begin(), rows.end(), [&](const auto& r) { return r.item.key == row.item.key; });
                const auto index = std::size_t(current - rows.begin());
                for (std::size_t n = 1; n < rows.size(); ++n) {
                    const auto& candidate = rows[(index + n) % rows.size()];
                    const auto view = tag_snapshot(candidate.tag);
                    if (!candidate.item.disabled && view.visible && !view.closing) {
                        services_->focus().defer_focus(s->draggable ? candidate.drag : view.interaction,
                                                       input::FocusModality::keyboard);
                        break;
                    }
                }
            }
            if (row.rtl.get() != s->rtl) {
                layout::FlexLayout model;
                model.align = layout::FlexAlign::center;
                model.right_to_left = s->rtl;
                services_->layout().set_layout(services_->components().root(row.owner), model);
            }
            const auto& token = services_->components().theme_scope(id)->snapshot().tag();
            row.handle_color.set(foreground(row.handle_focused ? token.colors.focus : token.colors.normal.text));
            row.handle_font.set(
                {token.metrics.font_family, 400, false, token.metrics.font_size, token.metrics.line_height});
            row.rtl.set(s->rtl);
            row.checked.set(std::find(s->selected.begin(), s->selected.end(), row.item.key) != s->selected.end());
            row.disabled.set(disabled);
            s = services_->components().state<TagGroupState>(id);
            if (!s || s->revision != revision) {
                return;
            }
            const bool eligible = s->draggable && !disabled && s->visible && active_ && tag.visible && !tag.closing;
            const auto handle = services_->interactions().require(row.drag).component;
            set_active(*services_, handle, s->draggable && tag.visible);
            services_->nodes().require(services_->components().root(handle)).out_of_flow =
                !s->draggable || !tag.visible;
            services_->interactions().set_eligible(row.drag, eligible);
            services_->interactions().set_focusable(row.drag, eligible);
            services_->interactions().set_tab_stop(
                row.drag, eligible && (has_focused_handle ? focused_handle == row.drag : first));
            if (eligible) {
                first = false;
            }
        }
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                   runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
        services_->set_input_blocked(id, !s->visible || s->disabled);

    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

void TagBadgeComponentHost::request_reorder(runtime::ComponentId id, String key, std::size_t target) {
    auto* s = services_->components().state<TagGroupState>(id);
    if (!s || s->disabled || !s->draggable || !s->visible || !active_ ||
        !services_->interactions().component_active(id)) {
        return;
    }
    const auto found =
        std::find_if(s->rows.begin(), s->rows.end(), [&](const auto& row) { return row.item.key == key; });
    if (found == s->rows.end() || found->item.disabled || target >= s->rows.size()) {
        return;
    }
    const auto from = std::size_t(found - s->rows.begin());
    if (from == target || s->rows[target].item.disabled) {
        return;
    }
    TagValues order;
    for (const auto& row : s->rows) {
        order.push_back(row.item.key);
    }
    const auto moved = order[from];
    order.erase(order.begin() + from);
    order.insert(order.begin() + target, moved);
    const auto callback = s->on_reorder;
    if (!callback) {
        auto row = std::move(s->rows[from]);
        s->rows.erase(s->rows.begin() + from);
        s->rows.insert(s->rows.begin() + target, std::move(row));
        std::vector<runtime::ComponentId> children;
        for (const auto& value : s->rows) {
            children.push_back(value.owner);
        }
        services_->components().reorder_children(id, children);
        ++s->revision;
        refresh_group(id);
    }
    try {
        if (callback) {
            callback(std::move(order));
        }
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

void TagBadgeComponentHost::group_pointer(runtime::ComponentId id, String key, input::PointerDispatchContext& context) {
    auto* s = services_->components().state<TagGroupState>(id);
    if (!s) {
        return;
    }
    if (context.kind() == input::PointerEventKind::cancel) {
        std::vector<runtime::ComponentId> order;
        for (const auto& row : s->rows) {
            order.push_back(row.owner);
        }
        services_->components().reorder_children(id, order);
        services_->mark_scene_structure_dirty();
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry |
                                                   runtime::DirtyFlags::HitTest);
        s->pointer.reset();
        s->capture.reset();
        s->dragging = {};
        s->moved = false;
        return;
    }
    if (!s->draggable || s->disabled || !active_ || !s->visible) {
        return;
    }
    auto found = std::find_if(s->rows.begin(), s->rows.end(), [&](const auto& row) { return row.item.key == key; });
    if (found == s->rows.end() || found->item.disabled) {
        return;
    }
    const auto& event = context.event();
    if (context.kind() == input::PointerEventKind::down && event.button == input::PointerButton::primary) {
        s->pointer = event.pointer;
        s->origin = {event.x, event.y};
        s->drag_delta = {};
        s->dragging = key;
        s->moved = false;
        s->capture = context.current_target();
        if (!context.capture_pointer()) {
            s->pointer.reset();
            s->capture.reset();
            s->dragging = {};
        }
        context.stop_propagation();
        return;
    }
    if (!s->pointer || *s->pointer != event.pointer || s->dragging != key) {
        return;
    }
    if (context.kind() == input::PointerEventKind::move) {
        const bool was_moved = s->moved;
        s->moved |= std::hypot(event.x - s->origin.x, event.y - s->origin.y) >= 5;
        if (s->moved) {
            s->drag_delta = {event.x - s->origin.x, event.y - s->origin.y};
            if (!was_moved) {
                std::vector<runtime::ComponentId> order;
                for (const auto& row : s->rows) {
                    if (row.item.key != key) {
                        order.push_back(row.owner);
                    }
                }
                order.push_back(found->owner);
                services_->components().reorder_children(id, order);
                services_->mark_scene_structure_dirty();
            }
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry |
                                                       runtime::DirtyFlags::HitTest);
        }
        context.stop_propagation();
        return;
    }
    if (context.kind() == input::PointerEventKind::up) {
        const bool moved = s->moved;
        std::vector<runtime::ComponentId> order;
        for (const auto& row : s->rows) {
            order.push_back(row.owner);
        }
        services_->components().reorder_children(id, order);
        services_->mark_scene_structure_dirty();
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry |
                                                   runtime::DirtyFlags::HitTest);
        s->pointer.reset();
        s->capture.reset();
        s->moved = false;
        s->dragging = {};
        static_cast<void>(context.release_pointer_capture());
        context.stop_propagation();
        if (!moved) {
            return;
        }
        std::optional<std::size_t> target;
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            if (s->rows[i].item.key == key) {
                continue;
            }
            const auto node = services_->components().root(s->rows[i].owner);
            const auto& geometry = services_->nodes().require(node);
            const runtime::Rect bounds{geometry.bounds.x + geometry.translation.x,
                                       geometry.bounds.y + geometry.translation.y, geometry.bounds.width,
                                       geometry.bounds.height};
            const auto clip = services_->nodes().content_clip(node, s->clip);
            if (event.x >= bounds.x && event.x < bounds.x + bounds.width && event.y >= bounds.y &&
                event.y < bounds.y + bounds.height && event.x >= clip.x && event.x < clip.x + clip.width &&
                event.y >= clip.y && event.y < clip.y + clip.height) {
                target = i;
                break;
            }
        }
        if (target) {
            request_reorder(id, key, *target);
        }
    }
}

bool TagBadgeComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.action != input::KeyAction::down || !active_) {
        return false;
    }
    const auto focused = services_->focus().state().focused;
    const auto* hit = focused ? services_->interactions().find(*focused) : nullptr;
    if (!hit) {
        return false;
    }
    for (auto id : groups_) {
        auto* s = services_->components().state<TagGroupState>(id);
        if (!s || s->disabled || !s->visible || !s->draggable || !services_->interactions().component_active(id)) {
            continue;
        }
        const auto found =
            std::find_if(s->rows.begin(), s->rows.end(), [&](const auto& row) { return row.drag == *focused; });
        if (found == s->rows.end()) {
            continue;
        }
        const auto i = std::size_t(found - s->rows.begin());
        if (event.key == input::Key::escape && s->capture) {
            const auto capture = *s->capture;
            s->pointer.reset();
            s->capture.reset();
            s->moved = false;
            s->dragging = {};
            services_->pointer().cancel_pointer_interaction(capture);
            return true;
        }
        if (event.key != input::Key::left && event.key != input::Key::right) {
            return false;
        }
        const bool next = (event.key == input::Key::right) != s->rtl;
        const auto target = next ? std::min(i + 1, s->rows.size() - 1) : i == 0 ? 0 : i - 1;
        request_reorder(id, found->item.key, target);
        return true;
    }
    return false;
}

void TagBadgeComponentHost::reconcile_group(runtime::ComponentId id, const std::vector<TagGroupItem>& items) {
    const auto storage = services_->components().retain_state<TagGroupState>(id);
    if (items.size() > 4096) {
        throw std::length_error("TagGroup item limit exceeded");
    }
    std::set<std::string> keys;
    for (const auto& item : items) {
        if (item.key.empty() || !keys.insert(std::string(item.key.view().bytes())).second) {
            throw std::invalid_argument("TagGroup keys must be nonempty and unique");
        }
        validate(item.color);
    }
    auto* s = services_->components().state<TagGroupState>(id);
    if (!s) {
        return;
    }
    if (s->reconciling) {
        s->pending_items = items;
        return;
    }
    const auto revision = ++s->revision;
    s->reconciling = true;
    const auto focused = services_->focus().state().focused;
    std::optional<std::size_t> focused_index;
    if (focused) {
        const auto* record = services_->interactions().find(*focused);
        for (std::size_t i = 0; record && i < s->rows.size(); ++i) {
            for (auto current = std::optional{record->component}; current;
                 current = services_->components().parent(*current)) {
                if (*current == s->rows[i].owner) {
                    focused_index = i;
                    break;
                }
            }
        }
    }
    auto previous = std::move(s->rows);
    std::vector<TagRow> next;
    try {
        for (const auto& item : items) {
            auto found = std::find_if(previous.begin(), previous.end(),
                                      [&](const auto& row) { return row.item.key == item.key; });
            TagRow row;
            if (found != previous.end() && found->item.content == item.content && found->item.icon == item.icon) {
                row = std::move(*found);
                found->item.key = {};
            } else if (found != previous.end()) {
                services_->destroy(found->owner);
                found->item.key = {};
            }
            s = services_->components().state<TagGroupState>(id);
            if (!s || s->revision != revision) {
                services_->retire_failed_component(id);
                return;
            }
            row.item = item;
            row.text.set(item.text);
            row.color.set(item.color);
            row.closable.set(item.closable);
            row.disabled.set(s->disabled || item.disabled);
            row.rtl.set(s->rtl);
            row.checked.set(std::find(s->selected.begin(), s->selected.end(), item.key) != s->selected.end());
            if (!row.owner.valid()) {
                append_retained_slot(*services_, id, Content{[&] {
                    auto& build = runtime::require_component_build_context();
                    row.owner = build.mount_component<int>(0);
                    const auto node = build.root(row.owner);
                    services_->nodes().require(node).clip_content = true;
                    layout::FlexLayout row_layout;
                    row_layout.align = layout::FlexAlign::center;
                    row_layout.right_to_left = s->rtl;
                    services_->layout().set_layout(node, row_layout);
                    build.on_resource_cleanup(row.owner, [this, node] { services_->layout().remove_layout(node); });
                    build.mount_slot(
                        row.owner, Content{[&] {
                            const auto handle = runtime::require_component_build_context().mount_component<int>(0);
                            const auto handle_node = build.root(handle);
                            services_->layout().set_layout(
                                handle_node,
                                layout::ComponentLayout{
                                    [this, id](layout::LayoutEngine& engine, runtime::NodeId node,
                                               layout::Constraints c) {
                                        const auto& m =
                                            services_->components().theme_scope(id)->snapshot().tag().metrics;
                                        for (auto child : services_->nodes().require(node).children) {
                                            static_cast<void>(
                                                engine.measure_child(child, {0, c.max_width, 0, c.max_height}));
                                        }
                                        return c.constrain(
                                            {m.close_size + m.icon_gap, m.line_height + 2 * m.border_width});
                                    },
                                    [this](layout::LayoutEngine& engine, runtime::NodeId node, runtime::Rect r) {
                                        for (auto child : services_->nodes().require(node).children) {
                                            const auto natural = services_->nodes().require(child).layout_size;
                                            const runtime::Size size{std::min(natural.width, r.width),
                                                                     std::min(natural.height, r.height)};
                                            engine.place_child(child,
                                                               {r.x + (r.width - size.width) / 2,
                                                                r.y + (r.height - size.height) / 2, size.width,
                                                                size.height},
                                                               true, true);
                                        }
                                    }});
                            row.drag =
                                services_->interactions().create({handle, handle_node, {}, true, true, {}, true, true});
                            const auto fragment =
                                build.register_scene_fragment(handle, runtime::SceneFragmentPlacement::before_children);
                            services_->scene_composer().set_fragment(fragment, {}, row.drag);
                            build.on_resource_cleanup(handle, [this, hit = row.drag, handle_node] {
                                services_->pointer().cancel_interaction(hit);
                                services_->focus().cancel_interaction(hit);
                                services_->interactions().remove(hit);
                                services_->layout().remove_layout(handle_node);
                            });
                            input::InteractionHandlers handlers;
                            handlers.target = [this, id, key = item.key](input::PointerDispatchContext& c) {
                                group_pointer(id, key, c);
                            };
                            services_->interactions().set_handlers(row.drag, std::move(handlers));
                            // A retained, separate handle keeps editor selection and independent child actions intact.
                            build.mount_slot_with_semantic_text_style(
                                handle, Content{[] { Icon(IconProps{}.name(IconName::HolderOutlined)); }},
                                row.handle_color, row.handle_font);
                            input::FocusHandlers focus;
                            focus.state_changed = [this, id, key = item.key](input::FocusPresentation presentation) {
                                auto* state = services_->components().state<TagGroupState>(id);
                                if (!state) {
                                    return;
                                }
                                auto row = std::find_if(state->rows.begin(), state->rows.end(),
                                                        [&](const auto& row) { return row.item.key == key; });
                                if (row == state->rows.end()) {
                                    return;
                                }
                                row->handle_focused = presentation.focus_visible;
                                const auto& colors = services_->components().theme_scope(id)->snapshot().tag().colors;
                                row->handle_color.set(
                                    foreground(presentation.focus_visible ? colors.focus : colors.normal.text));
                            };
                            services_->interactions().set_focus_handlers(row.drag, std::move(focus));
                            TagSlots slots;
                            if (item.icon) {
                                slots.icon = TagIcon{[renderer = item.icon] {
                                    FormPrivateEditorScope private_scope;
                                    if (renderer->render) {
                                        renderer->render();
                                    }
                                }};
                            }
                            const TagContent content{[&, renderer = item.content, text = row.text] {
                                FormPrivateEditorScope private_scope;
                                if (renderer && renderer->render) {
                                    renderer->render();
                                } else {
                                    Text(TextProps{}.content(text));
                                }
                            }};
                            const auto before = tags_.size();
                            if (s->checkable) {
                                CheckableTagProps props;
                                props.checked(row.checked)
                                    .disabled(row.disabled)
                                    .rtl(row.rtl)
                                    .onChange([this, id, key = item.key](bool checked) {
                                        auto* state = services_->components().state<TagGroupState>(id);
                                        if (!state) {
                                            return;
                                        }
                                        TagValues next = state->multiple ? state->selected : TagValues{};
                                        std::erase(next, key);
                                        if (checked) {
                                            next.push_back(key);
                                        }
                                        const auto callback = state->on_change;
                                        if (!state->controlled) {
                                            state->selected = next;
                                            refresh_group(id);
                                        }
                                        try {
                                            if (callback) {
                                                callback(next);
                                            }
                                        } catch (...) {
                                            services_->retire_failed_component(id);
                                            throw;
                                        }
                                    });
                                mount(props, content, slots);
                            } else {
                                TagProps props;
                                props.color(row.color)
                                    .closable(row.closable)
                                    .disabled(row.disabled)
                                    .rtl(row.rtl)
                                    .onClose(std::function<void(TagCloseEvent&)>{
                                        [this, id, key = item.key](TagCloseEvent& event) {
                                            auto* state = services_->components().state<TagGroupState>(id);
                                            if (!state) {
                                                return;
                                            }
                                            const auto callback = state->on_close;
                                            try {
                                                if (callback) {
                                                    callback(key, event);
                                                }
                                            } catch (...) {
                                                services_->retire_failed_component(id);
                                                throw;
                                            }
                                        }});
                                mount(props, content, slots);
                            }
                            if (tags_.size() <= before) {
                                services_->retire_failed_component(id);
                                return;
                            }
                            row.tag = tags_[before];
                            if (auto* state = services_->components().state<TagState>(row.tag)) {
                                state->group = id;
                            }
                        }});
                }});
            }
            next.push_back(std::move(row));
        }
        s = services_->components().state<TagGroupState>(id);
        if (!s || s->revision != revision) {
            services_->retire_failed_component(id);
            return;
        }
        s->rows = std::move(next);
        bool removed_focus = false;
        for (std::size_t i = 0; i < previous.size(); ++i) {
            if (!previous[i].item.key.empty()) {
                removed_focus |= focused_index && *focused_index == i;
                services_->destroy(previous[i].owner);
                s = services_->components().state<TagGroupState>(id);
                if (!s || s->revision != revision) {
                    return;
                }
            }
        }
        if (!s->controlled) {
            std::erase_if(s->selected,
                          [&](const auto& key) { return !keys.contains(std::string(key.view().bytes())); });
        }
        std::vector<runtime::ComponentId> order;
        for (const auto& row : s->rows) {
            order.push_back(row.owner);
        }
        services_->components().reorder_children(id, order);
        refresh_group(id);
        s = services_->components().state<TagGroupState>(id);
        if (s && removed_focus && !s->rows.empty()) {
            const auto index = std::min(focused_index.value_or(0), s->rows.size() - 1);
            for (std::size_t offset = 0; offset < s->rows.size(); ++offset) {
                const auto& row = s->rows[(index + offset) % s->rows.size()];
                if (!row.item.disabled) {
                    services_->focus().defer_focus(s->draggable ? row.drag : tag_snapshot(row.tag).interaction,
                                                   input::FocusModality::keyboard);
                    break;
                }
            }
        }
        s = services_->components().state<TagGroupState>(id);
        if (s) {
            s->reconciling = false;
            auto pending = std::move(s->pending_items);
            s->pending_items.reset();
            if (pending) {
                reconcile_group(id, *pending);
            }
        }
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

void TagBadgeComponentHost::mount(const TagGroupProps& p) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<TagGroupState>();
    const auto storage = build.host().retain_state<TagGroupState>(id);
    auto& s = *storage;
    s.node = build.root(id);
    s.controlled = p.value_.has_value();
    s.selected = p.value_ ? read_prop(*p.value_) : p.default_value_;
    s.checkable = p.checkable_;
    s.multiple = p.multiple_;
    s.draggable = read_prop(p.draggable_);
    s.disabled = read_prop(p.disabled_);
    s.visible = read_prop(p.visible_);
    s.rtl = read_prop(p.rtl_);
    std::set<std::string> selected_keys;
    for (const auto& key : s.selected) {
        if (key.empty() || !selected_keys.insert(std::string(key.view().bytes())).second) {
            throw std::invalid_argument("Invalid CheckableTagGroup default value");
        }
    }
    if (!s.multiple && s.selected.size() > 1) {
        throw std::invalid_argument("Single CheckableTagGroup accepts at most one value");
    }
    s.on_change = p.on_change_;
    s.on_reorder = p.on_reorder_;
    s.on_close = p.on_close_;
    runtime::connect_layout_style(build.scope(id), p.layout_, s.node, services_->nodes(), services_->dirty());
    services_->nodes().require(s.node).clip_content = true;
    services_->layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) {
                auto& s = *services_->components().state<TagGroupState>(id);
                float width = 0;
                float height = 0;
                float line_width = 0;
                float line_height = 0;
                const auto gap = services_->components().theme_scope(id)->snapshot().tag().metrics.icon_gap;
                for (const auto& row : s.rows) {
                    if (services_->nodes().require(services_->components().root(row.owner)).out_of_flow) {
                        continue;
                    }
                    const auto size =
                        e.measure_child(services_->components().root(row.owner), {0, c.max_width, 0, c.max_height});
                    if (line_width > 0 && line_width + gap + size.width > c.max_width) {
                        width = std::max(width, line_width);
                        height += line_height + gap;
                        line_width = 0;
                        line_height = 0;
                    }
                    line_width += (line_width > 0 ? gap : 0) + size.width;
                    line_height = std::max(line_height, size.height);
                }
                return c.constrain({std::max(width, line_width), height + line_height});
            },
            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) {
                auto& s = *services_->components().state<TagGroupState>(id);
                float x = 0;
                float y = 0;
                float line_height = 0;
                const auto gap = services_->components().theme_scope(id)->snapshot().tag().metrics.icon_gap;
                for (const auto& row : s.rows) {
                    const auto node = services_->components().root(row.owner);
                    if (services_->nodes().require(node).out_of_flow) {
                        continue;
                    }
                    const auto size = services_->nodes().require(node).layout_size;
                    if (x > 0 && x + gap + size.width > r.width) {
                        y += line_height + gap;
                        x = 0;
                        line_height = 0;
                    }
                    if (x > 0) {
                        x += gap;
                    }
                    const auto delta = s.moved && s.dragging == row.item.key ? s.drag_delta : runtime::Point{};
                    e.place_child(node,
                                  {(s.rtl ? r.x + r.width - x - size.width : r.x + x) + delta.x, r.y + y + delta.y,
                                   size.width, std::min(size.height, std::max(0.F, r.height - y))},
                                  true, true);
                    x += size.width;
                    line_height = std::max(line_height, size.height);
                }
            }});
    build.on_resource_cleanup(id, [this, id, node = s.node] {
        services_->interactions().set_input_blocked(id, false);
        services_->layout().remove_layout(node);
    });
    groups_.push_back(id);
    auto& scope = build.scope(id);
    connect_prop(
        scope, p.items_, [this, id](std::vector<TagGroupItem> v) { reconcile_group(id, v); },
        [](const auto&, const auto&) { return false; });
    if (p.value_) {
        connect_prop(scope, *p.value_, [this, id](TagValues v) {
            auto* s = services_->components().state<TagGroupState>(id);
            if (!s) {
                return;
            }
            std::set<std::string> keys;
            for (const auto& key : v) {
                if (key.empty() || !keys.insert(std::string(key.view().bytes())).second) {
                    throw std::invalid_argument("Invalid CheckableTagGroup value");
                }
            }
            if (!s->multiple && v.size() > 1) {
                throw std::invalid_argument("Single CheckableTagGroup accepts at most one value");
            }
            s->selected = std::move(v);
            refresh_group(id);
        });
    }
    connect_prop(scope, p.disabled_, [this, id](bool v) {
        if (auto* s = services_->components().state<TagGroupState>(id)) {
            s->disabled = v;
            refresh_group(id);
        }
    });
    connect_prop(scope, p.draggable_, [this, id](bool v) {
        if (auto* s = services_->components().state<TagGroupState>(id)) {
            s->draggable = v;
            refresh_group(id);
        }
    });
    connect_prop(scope, p.visible_, [this, id](bool v) {
        if (auto* s = services_->components().state<TagGroupState>(id)) {
            s->visible = v;
            refresh_group(id);
        }
    });
    connect_prop(scope, p.rtl_, [this, id](bool v) {
        if (auto* s = services_->components().state<TagGroupState>(id)) {
            s->rtl = v;
            refresh_group(id);
        }
    });
    const auto theme = build.theme_scope();
    s.subscriptions[0] = theme->capture(
        [this, id](auto) {
            if (const auto* state = services_->components().state<TagGroupState>(id)) {
                const auto rows = state->rows;
                const auto& colors = services_->components().theme_scope(id)->snapshot().tag().colors;
                for (const auto& row : rows) {
                    row.handle_color.set(foreground(row.handle_focused ? colors.focus : colors.normal.text));
                }
            }
        },
        [theme] { static_cast<void>(theme->tag_colors()); });
    s.subscriptions[1] =
        theme->capture([this, id](auto) { refresh_group(id); }, [theme] { static_cast<void>(theme->tag_metrics()); });
    refresh_group(id);
}

void TagBadgeComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    synchronize_auxiliary_motion();
    for (auto id : groups_) {
        if (auto* s = services_->components().state<TagGroupState>(id)) {
            s->clip = services_->component_clip(id, viewport, document_clip);
        }
    }
    for (auto id : tags_) {
        auto* s = services_->components().state<TagState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& node = services_->nodes().require(s->node);
        const auto bounds = node.bounds;
        const auto& token = services_->components().theme_scope(id)->snapshot().tag();
        const auto& m = token.metrics;
        const auto clip = services_->nodes().content_clip(services_->components().root(s->viewport),
                                                          services_->component_clip(id, viewport, document_clip));
        paint(*services_, s->surface, node, bounds, s->snapshot.palette.background, m.border_radius, clip);
        std::array<graphics::RoundedEffectInstance, 2> effects;
        const float border = std::min(m.border_width, std::min(bounds.width, bounds.height) / 2);
        const runtime::Rect inner{bounds.x + border, bounds.y + border, std::max(0.0F, bounds.width - border * 2),
                                  std::max(0.0F, bounds.height - border * 2)};
        const float inner_radius =
            std::min(std::max(0.0F, m.border_radius - border), std::min(inner.width, inner.height) / 2);
        effects[0] =
            graphics::make_outline_effect({inner, inner_radius}, border > 0 ? border : 1, 0, s->snapshot.palette.border,
                                          !s->checkable && s->bordered && border > 0 ? node.opacity : 0,
                                          node.translation, graphics::EffectClip{1, clip});
        const auto focus_bounds =
            s->checkable ? bounds : services_->nodes().require(services_->components().root(s->snapshot.close)).bounds;
        const float focus_radius =
            std::min(s->checkable ? m.border_radius : 2, std::min(focus_bounds.width, focus_bounds.height) / 2);
        effects[1] = graphics::make_outline_effect({focus_bounds, focus_radius}, std::max(m.focus_width, 0.001F),
                                                   m.focus_offset, token.colors.focus,
                                                   s->focus.focus_visible && m.focus_width > 0 ? node.opacity : 0,
                                                   node.translation, graphics::EffectClip{1, clip});
        static_cast<void>(services_->surfaces().update_content_effects(s->border, effects));
    }
    for (auto id : badges_) {
        auto* s = services_->components().state<BadgeState>(id);
        const bool was_processing = s && s->processing_visible;
        if (s) {
            s->processing_visible = false;
        }
        if (!s || !services_->components().branch_active(id) || !s->snapshot.shown) {
            continue;
        }
        const auto& node = services_->nodes().require(services_->components().root(s->snapshot.indicator));
        const auto& t = services_->components().theme_scope(id)->snapshot().badge();
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
        paint(*services_, s->surface, node, node.bounds,
              s->custom_count.valid() && !s->ribbon && !s->snapshot.dot ? Color{} : s->snapshot.color,
              s->ribbon ? 0 : node.bounds.height / 2, clip,
              s->snapshot.status || (s->custom_count.valid() && !s->ribbon) ? 0 : t.metrics.outline_width, 0,
              t.colors.outline, node.opacity);
        s->processing_visible = active_ && s->animated && s->status == Status::Processing && s->snapshot.status &&
                                motion_allowed(*services_, id) && services_->nodes().content_opacity(s->node) > 0 &&
                                node.bounds.width > 0 && node.bounds.height > 0 &&
                                node.bounds.x < clip.x + clip.width && node.bounds.x + node.bounds.width > clip.x &&
                                node.bounds.y < clip.y + clip.height && node.bounds.y + node.bounds.height > clip.y;
        if (s->processing_visible && !was_processing) {
            s->processing_started = true;
        }
        std::vector<graphics::RoundedEffectInstance> pulse;
        if (s->processing_visible) {
            const float spread = t.metrics.status_size * s->snapshot.processing_phase;
            const runtime::Rect r{node.bounds.x - spread, node.bounds.y - spread, node.bounds.width + 2 * spread,
                                  node.bounds.height + 2 * spread};
            pulse.push_back(graphics::make_outline_effect(
                {r, r.height / 2}, std::max(.5F, t.metrics.outline_width), 0, s->snapshot.color,
                (1 - s->snapshot.processing_phase) * services_->nodes().content_opacity(s->node), node.translation,
                graphics::EffectClip{1, clip}));
        }
        static_cast<void>(services_->surfaces().update_content_effects(s->pulse, pulse));
    }
}
} // namespace ryn::detail

namespace ryn {
void Tag(TagProps props, TagContent content, TagSlots slots) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_status_components) {
        throw std::logic_error("Tag requires window services");
    }
    detail::active_status_components->mount(props, content, slots);
}

void CheckableTag(CheckableTagProps props, TagContent content, TagSlots slots) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_status_components) {
        throw std::logic_error("CheckableTag requires window services");
    }
    detail::active_status_components->mount(props, content, slots);
}

void TagGroup(TagGroupProps props) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_status_components) {
        throw std::logic_error("TagGroup requires window services");
    }
    detail::active_status_components->mount(props);
}

void CheckableTagGroup(TagGroupProps props) {
    TagGroup(props.checkable());
}

void BadgeRibbon(RibbonProps props, BadgeContent content) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_status_components) {
        throw std::logic_error("BadgeRibbon requires window services");
    }
    detail::active_status_components->mount(props, content);
}

void Badge(BadgeProps props, std::optional<BadgeContent> content) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_status_components) {
        throw std::logic_error("Badge requires window services");
    }
    detail::active_status_components->mount(props, content);
}
} // namespace ryn
