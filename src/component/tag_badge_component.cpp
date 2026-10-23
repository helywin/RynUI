#include "component/tag_badge_component.hpp"
#include "input/pressable_behavior.hpp"
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
    TagSnapshot snapshot;
    bool checkable{};
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
    std::array<runtime::Size, 3> sizes;
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

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
    auto* s = services_->components().state<TagState>(id);
    if (!s) {
        return;
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
    set_active(*services_, id, s->snapshot.visible);
    set_active(*services_, s->snapshot.close, s->snapshot.closable && !s->checkable);
    bool eligibility_changed = false;
    if (s->snapshot.interaction.valid()) {
        const bool eligible = s->snapshot.visible && !s->snapshot.disabled && (s->checkable || s->snapshot.closable);
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
    if (eligibility_changed) {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::HitTest);
        services_->focus().synchronize();
    }
}

void TagBadgeComponentHost::activate(runtime::ComponentId id) {
    auto* s = services_->components().state<TagState>(id);
    if (!s || s->snapshot.disabled || !s->snapshot.visible) {
        return;
    }
    if (s->checkable) {
        const auto callback = s->on_change;
        const auto next = !s->snapshot.checked;
        if (callback) {
            callback(next);
        }
    } else if (s->snapshot.closable) {
        const auto callback = s->on_close;
        if (callback) {
            callback();
        }
    }
}

runtime::ComponentId TagBadgeComponentHost::mount_tag(const TagProps& props, const TagContent& content,
                                                      const TagSlots& slots, bool checkable, bool checked,
                                                      std::function<void(bool)> change) {
    validate(read_prop(props.color_));
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<TagState>();
    auto& s = build.state<TagState>(id);
    s.node = build.root(id);
    s.checkable = checkable;
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
    s.snapshot.icon = region(services, id, Content{[slot = slots.icon] {
                                 if (slot) {
                                     SlotContentAccess::function (*slot)();
                                 }
                             }},
                             s.foreground, s.font);
    s.snapshot.content =
        region(services, id, Content{[content] { SlotContentAccess::function(content)(); }}, s.foreground, s.font);
    s.snapshot.close = region(services, id, Content{[] { Icon(IconProps{}.name(IconName::CloseOutlined)); }},
                              s.close_foreground, s.close_font);
    const auto action = checkable ? id : s.snapshot.close;
    const auto action_node = services.components().root(action);
    s.snapshot.interaction = services.interactions().create({action, action_node, {}, true, true, {}, true, true});
    // A static Tag retains an ineligible close action so closable can change without remounting.
    s.surface = surface(services, id, checkable ? std::optional{s.snapshot.interaction} : std::nullopt);
    if (!checkable) {
        const auto fragment = build.register_scene_fragment(action, runtime::SceneFragmentPlacement::before_children);
        services.scene_composer().set_fragment(fragment, {}, s.snapshot.interaction);
    }
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.border = services.surfaces().create_content_range(fragment, {});
    build.on_resource_cleanup(id, [&services, node = s.node, hit = s.snapshot.interaction, border = s.border] {
        services.pointer().cancel_interaction(hit);
        services.focus().cancel_interaction(hit);
        services.interactions().remove(hit);
        services.surfaces().destroy_content_range(border);
        services.layout().remove_layout(node);
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
        const auto result =
            s->press.dispatch(context, s->snapshot.interaction, !s->snapshot.disabled && s->snapshot.visible);
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
        return s && !s->snapshot.disabled && s->snapshot.visible && (s->checkable || s->snapshot.closable);
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
                return limits.constrain(
                    {edge * 2 + s.sizes[0].width + gap + s.sizes[1].width + close + (close > 0 ? m.close_gap : 0),
                     std::max({m.line_height, s.sizes[0].height, s.sizes[1].height, close}) + m.border_width * 2});
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                auto& s = *services_->components().state<TagState>(id);
                const auto& m = services_->components().theme_scope(id)->snapshot().tag().metrics;
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
                                       {x, bounds.y + (bounds.height - size.height) / 2, size.width, size.height});
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
            s->snapshot.visible = v;
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
    tag.disabled(props.disabled_).layout(props.layout_);
    const auto id = mount_tag(tag, content, slots, true, read_prop(props.checked_), props.on_change_);
    connect_prop(services_->components().scope(id), props.checked_, [this, id](bool value) {
        if (auto* s = services_->components().state<TagState>(id)) {
            s->snapshot.checked = value;
            refresh_tag(id, false);
        }
    });
}

void TagBadgeComponentHost::refresh_badge(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<BadgeState>(id);
    if (!s) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().badge();
    const bool zero = s->count && *s->count == 0;
    const bool count = s->count && (!zero || s->show_zero);
    s->snapshot.status = !count && s->status != Status::None;
    s->snapshot.dot = s->snapshot.status || (s->dot && (!zero || s->show_zero));
    s->snapshot.shown = s->snapshot.status || s->snapshot.dot || count;
    s->snapshot.display_count = {};
    if (s->snapshot.shown && !s->snapshot.dot && s->count) {
        const auto value = *s->count > s->overflow ? std::to_string(s->overflow) + "+" : std::to_string(*s->count);
        s->snapshot.display_count = String::from_utf8(value).value();
    }
    s->snapshot.color =
        s->snapshot.status ? t.colors.statuses[static_cast<std::size_t>(s->status)] : t.colors.background;
    s->count_text.set(s->snapshot.display_count);
    s->count_color.set(foreground(t.colors.count_text));
    s->text_color.set(foreground(t.colors.text));
    const auto& m = t.metrics;
    const float height = s->size == BadgeSize::Small ? m.indicator_height_small : m.indicator_height;
    s->count_font.set({m.font_family, m.font_weight, false, m.font_size, height});
    s->text_font.set({m.font_family, 400, false, m.text_font_size, m.text_line_height});
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
    auto& s = build.state<BadgeState>(id);
    s.node = build.root(id);
    s.wrapped = content.has_value();
    if (props.count_) {
        s.count = read_prop(*props.count_);
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
                         if (content) {
                             context.mount_slot(s.snapshot.content,
                                                Content{[content] { SlotContentAccess::function (*content)(); }});
                         }
                     }});
    s.snapshot.indicator = region(services, id, Content{[value = s.count_text] { Text(TextProps{}.content(value)); }},
                                  s.count_color, s.count_font);
    s.surface = surface(services, s.snapshot.indicator);
    s.snapshot.text = region(services, id, Content{[value = s.label] { Text(TextProps{}.content(value)); }},
                             s.text_color, s.text_font);
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                auto& s = *services_->components().state<BadgeState>(id);
                const auto& m = services_->components().theme_scope(id)->snapshot().badge().metrics;
                const float height = s.snapshot.status            ? m.status_size
                                     : s.snapshot.dot             ? m.dot_size
                                     : s.size == BadgeSize::Small ? m.indicator_height_small
                                                                  : m.indicator_height;
                const auto text = engine.measure_child(
                    services_->components().root(s.snapshot.indicator),
                    {0, std::numeric_limits<float>::infinity(), 0, std::numeric_limits<float>::infinity()});
                const float width =
                    s.snapshot.dot
                        ? height
                        : std::max(height,
                                   text.width + (s.snapshot.display_count.size_bytes() > 1 ? m.padding_inline * 2 : 0));
                s.sizes[1] = s.snapshot.shown ? runtime::Size{width, height} : runtime::Size{};
                s.sizes[0] = s.wrapped ? engine.measure_child(services_->components().root(s.snapshot.content),
                                                              {0, limits.max_width, 0, limits.max_height})
                                       : runtime::Size{};
                const float used = s.wrapped ? s.sizes[0].width : s.sizes[1].width;
                const bool label = s.snapshot.shown && !s.label.get().empty();
                s.sizes[2] = label ? engine.measure_child(services_->components().root(s.snapshot.text),
                                                          {0, std::max(0.0F, limits.max_width - used - m.text_gap), 0,
                                                           limits.max_height})
                                   : runtime::Size{};
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
                if (s.wrapped) {
                    engine.place_child(services_->components().root(s.snapshot.content),
                                       {bounds.x, bounds.y, child.width, child.height});
                }
                const float x = s.wrapped ? bounds.x + child.width - indicator.width / 2 : bounds.x;
                const float y =
                    s.wrapped ? bounds.y - indicator.height / 2 : bounds.y + (bounds.height - indicator.height) / 2;
                s.snapshot.indicator_bounds = {x + s.offset.x, y + s.offset.y, indicator.width, indicator.height};
                if (s.snapshot.shown) {
                    engine.place_child(services_->components().root(s.snapshot.indicator), s.snapshot.indicator_bounds);
                }
                if (s.snapshot.shown && !s.label.get().empty()) {
                    engine.place_child(services_->components().root(s.snapshot.text),
                                       {bounds.x + (s.wrapped ? child.width : indicator.width) + m.text_gap,
                                        bounds.y + (bounds.height - label.height) / 2, label.width, label.height});
                }
            }});
    // Center the retained count inside the indicator's externally selected pill geometry.
    const auto indicator_node = services.components().root(s.snapshot.indicator);
    services.layout().set_layout(indicator_node, layout::FlexLayout{layout::FlexDirection::horizontal,
                                                                    0,
                                                                    {},
                                                                    false,
                                                                    false,
                                                                    layout::FlexWrap::no_wrap,
                                                                    layout::FlexJustify::center,
                                                                    layout::FlexAlign::center});
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    badges_.push_back(id);
    refresh_badge(id, true);
    auto& scope = build.scope(id);
    if (props.count_) {
        connect_prop(scope, *props.count_, [this, id](int v) {
            validate_count(v);
            if (auto* s = services_->components().state<BadgeState>(id)) {
                s->count = v;
                refresh_badge(id, true);
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
    const auto theme = build.theme_scope();
    s.subscriptions[0] = theme->capture([this, id](auto) { refresh_badge(id, false); },
                                        [theme] { static_cast<void>(theme->badge_colors()); });
    s.subscriptions[1] = theme->capture([this, id](auto) { refresh_badge(id, true); },
                                        [theme] { static_cast<void>(theme->badge_metrics()); });
}

void TagBadgeComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : tags_) {
        auto* s = services_->components().state<TagState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& node = services_->nodes().require(s->node);
        const auto bounds = node.bounds;
        const auto& token = services_->components().theme_scope(id)->snapshot().tag();
        const auto& m = token.metrics;
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
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
        if (!s || !services_->components().branch_active(id) || !s->snapshot.shown) {
            continue;
        }
        const auto& node = services_->nodes().require(services_->components().root(s->snapshot.indicator));
        const auto& t = services_->components().theme_scope(id)->snapshot().badge();
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
        paint(*services_, s->surface, node, node.bounds, s->snapshot.color, node.bounds.height / 2, clip,
              s->snapshot.status ? 0 : t.metrics.outline_width, 0, t.colors.outline, node.opacity);
    }
}
} // namespace ryn::detail

namespace ryn {
void Tag(TagProps props, TagContent content, TagSlots slots) {
    if (!detail::active_status_components) {
        throw std::logic_error("Tag requires window services");
    }
    detail::active_status_components->mount(props, content, slots);
}

void CheckableTag(CheckableTagProps props, TagContent content, TagSlots slots) {
    if (!detail::active_status_components) {
        throw std::logic_error("CheckableTag requires window services");
    }
    detail::active_status_components->mount(props, content, slots);
}

void Badge(BadgeProps props, std::optional<BadgeContent> content) {
    if (!detail::active_status_components) {
        throw std::logic_error("Badge requires window services");
    }
    detail::active_status_components->mount(props, content);
}
} // namespace ryn
