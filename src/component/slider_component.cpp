#include "component/slider_component.hpp"
#include "component/slider_value.hpp"
#include "component/tooltip_component.hpp"
#include "input/pressable_behavior.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <charconv>
#include <stdexcept>

namespace ryn::detail {
namespace {
thread_local SliderComponentHost* active_slider{};

float handle_extent(const SliderMetricToken& token) noexcept {
    return std::max(token.handle_size + 2 * token.handle_line_width,
                    token.handle_size_hover + 2 * token.handle_line_width_hover);
}

void validate_orientation(SliderOrientation value) {
    if (value != SliderOrientation::Horizontal && value != SliderOrientation::Vertical) {
        throw std::invalid_argument("Slider orientation is invalid");
    }
}

graphics::QuadInstance quad(runtime::Rect rect, Color color, float radius, runtime::Point translation) {
    graphics::QuadInstance result;
    result.bounds = {rect.x, rect.y, rect.width, rect.height};
    result.color = {color.red(), color.green(), color.blue(), color.alpha()};
    result.corner_radius = radius;
    result.translation = {translation.x, translation.y};
    return result;
}
} // namespace

struct SliderLabel final {
    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId interaction;
    input::PressableBehavior pressable;
    Signal<String> text{String{}};
    Signal<runtime::SemanticForeground> foreground{runtime::SemanticForeground{}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
};

struct SliderState final {
    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId rail;
    component::RetainedSurfaceId surface;
    std::array<runtime::ComponentId, 2> children;
    std::array<runtime::NodeId, 2> nodes;
    std::array<runtime::NodeId, 2> wrappers;
    std::array<Signal<String>, 2> hint_titles{Signal<String>{String{}}, Signal<String>{String{}}};
    std::array<Signal<bool>, 2> hint_open{Signal<bool>{false}, Signal<bool>{false}};
    std::array<Signal<TooltipPlacement>, 2> hint_placements{Signal<TooltipPlacement>{TooltipPlacement::Top},
                                                            Signal<TooltipPlacement>{TooltipPlacement::Top}};
    std::array<bool, 2> hint_dismissed{};
    std::array<bool, 2> hint_active{};
    std::array<std::optional<double>, 2> hinted_values;
    bool formatting_hint{};
    std::vector<SliderLabel> labels;
    runtime::Rect rail_bounds;
    float label_extent{};
    bool has_labels{};
    component::RetainedSurfaceId dot_surface;
    std::array<input::InteractionId, 2> thumbs;
    std::array<component::RetainedSurfaceId, 2> surfaces;
    std::array<input::FocusPresentation, 2> focus;
    std::array<bool, 2> hover{};
    std::array<runtime::Point, 2> centers;
    SliderRange value;
    SliderRange candidate;
    SliderRange raw_value;
    SliderLimits limits;
    SliderMarks marks;
    std::vector<double> points;
    std::vector<graphics::QuadInstance> dot_quads;
    bool marks_only{};
    bool dots{};
    bool included{true};
    SliderHintOptions hint;
    std::function<String(double)> hint_formatter;
    bool range{};
    bool controlled{};
    bool disabled{};
    bool keyboard_enabled{true};
    bool reverse{};
    bool rail_hover{};
    bool dragging{};
    bool gesture{};
    bool disposing{};
    SliderOrientation orientation{SliderOrientation::Horizontal};
    std::size_t active{};
    input::PointerIdentity pointer;
    input::InteractionId capture;
    float pointer_offset{};
    std::optional<input::Key> key;
    std::function<void(SliderRange)> on_change;
    std::function<void(SliderRange)> on_complete;
    theme_runtime::Subscription colors;
    theme_runtime::Subscription metrics;
    theme_runtime::Subscription fonts;

    std::size_t count() const noexcept {
        return range ? 2 : 1;
    }
};

SliderComponentHost::SliderComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

SliderComponentHost::~SliderComponentHost() {
    services_->detach(*this);
}

void* SliderComponentHost::begin_mount() noexcept {
    return std::exchange(active_slider, this);
}

void SliderComponentHost::end_mount(void* previous) noexcept {
    active_slider = static_cast<SliderComponentHost*>(previous);
}

void SliderComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](const auto& m) { return !services_->components().contains(m.component); });
}

SliderState* SliderComponentHost::find(runtime::ComponentId id) noexcept {
    return services_->components().state<SliderState>(id);
}

SliderSnapshot SliderComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<SliderState>(id);
    if (!s) {
        throw std::out_of_range("Slider component is stale");
    }
    return {s->value, s->limits, s->range, s->disabled, s->dragging, s->reverse, s->orientation, s->centers, s->focus};
}

void SliderComponentHost::cancel(runtime::ComponentId id) {
    auto* s = find(id);
    if (!s) {
        return;
    }
    const auto capture = s->capture;
    s->dragging = s->gesture = false;
    s->key.reset();
    s->capture = {};
    s->candidate = s->value;
    if (capture.valid()) {
        services_->pointer().cancel_pointer_interaction(capture);
    }
    update(id, false);
}

void SliderComponentHost::on_window_active(bool active) {
    if (!active) {
        for (const auto& m : mounted_) {
            cancel(m.component);
        }
    }
}

void SliderComponentHost::release(SliderState& s) {
    s.dragging = s.gesture = false;
    s.key.reset();
    services_->pointer().cancel_interaction(s.rail);
    services_->interactions().remove(s.rail);
    services_->surfaces().destroy(s.surface);
    if (s.dot_surface.valid()) {
        services_->surfaces().destroy_content_range(s.dot_surface);
    }
    services_->layout().remove_layout(s.node);
}

void SliderComponentHost::mount_labels(runtime::ComponentId id, runtime::ComponentBuildContext& build) {
    auto* s = find(id);
    for (std::size_t i = s->labels.size(); i < s->marks.size(); ++i) {
        s->labels.emplace_back();
        auto& label = s->labels.back();
        label.text.set(s->marks[i].label);
        label.component = build.mount_component<int>(0);
        label.node = build.root(label.component);
        services_->layout().set_layout(label.node, layout::BoxLayout{});
        label.interaction =
            services_->interactions().create({label.component, label.node, s->rail, !s->disabled, false, {}, false});
        const auto node = label.node;
        const auto interaction = label.interaction;
        build.on_resource_cleanup(label.component, [this, node, interaction] {
            services_->pointer().cancel_interaction(interaction);
            services_->interactions().remove(interaction);
            services_->layout().remove_layout(node);
        });
        input::InteractionHandlers handlers;
        handlers.target = [this, id, i](input::PointerDispatchContext& event) {
            auto* s = find(id);
            if (!s || i >= s->labels.size()) {
                return;
            }
            auto& label = s->labels[i];
            const auto result = label.pressable.dispatch(event, label.interaction, !s->disabled);
            if (!result.activate) {
                return;
            }
            const double value = s->marks[i].value;
            if (s->range) {
                if (std::abs(value - s->value.lower) < std::abs(value - s->value.upper)) {
                    s->active = 0;
                } else if (std::abs(value - s->value.lower) > std::abs(value - s->value.upper)) {
                    s->active = 1;
                }
            }
            s->gesture = true;
            s->candidate = s->value;
            const auto active = s->active;
            static_cast<void>(services_->focus().request_focus(s->thumbs[active], input::FocusModality::pointer));
            change(id, active, value);
            complete(id);
        };
        services_->interactions().set_handlers(interaction, std::move(handlers));
        const auto fragment =
            build.register_scene_fragment(label.component, runtime::SceneFragmentPlacement::before_children);
        services_->scene_composer().set_fragment(fragment, {}, interaction);
        const auto content = label.text;
        build.mount_slot_with_semantic_text_style(label.component,
                                                  Content{[content] { Text(TextProps{}.content(content)); }},
                                                  label.foreground, label.typography);
        services_->components().set_branch_active(label.component, !s->marks[i].label.empty());
    }
}

void SliderComponentHost::synchronize_labels(runtime::ComponentId id) {
    auto* s = find(id);
    std::vector<runtime::ComponentId> removed;
    for (std::size_t i = s->marks.size(); i < s->labels.size(); ++i) {
        removed.push_back(s->labels[i].component);
    }
    if (!removed.empty()) {
        s->labels.resize(s->marks.size());
    }
    for (auto component : removed) {
        services_->destroy(component);
        s = find(id);
        if (!s) {
            return;
        }
    }
    if (s->labels.size() < s->marks.size()) {
        const auto previous = s->labels.size();
        try {
            services_->append_slot(
                id, Content{[this, id] { mount_labels(id, runtime::require_component_build_context()); }});
        } catch (...) {
            if (auto* current = find(id)) {
                current->labels.resize(previous);
            }
            throw;
        }
    }
    s = find(id);
    for (std::size_t i = 0; s && i < s->labels.size(); ++i) {
        s->labels[i].text.set(s->marks[i].label);
        if (services_->components().set_branch_active(s->labels[i].component, !s->marks[i].label.empty())) {
            services_->mark_scene_structure_dirty();
        }
    }
}

void SliderComponentHost::update_hints(runtime::ComponentId id) {
    auto* s = find(id);
    if (!s || s->formatting_hint) {
        return;
    }
    const auto count = s->count();
    for (std::size_t i = 0; i < count; ++i) {
        s = find(id);
        if (!s) {
            return;
        }
        const double value = i == 0 ? s->value.lower : s->value.upper;
        if (s->hinted_values[i] != value) {
            const auto formatter = s->hint_formatter;
            String text;
            s->formatting_hint = true;
            try {
                if (formatter) {
                    text = formatter(value);
                } else {
                    std::array<char, 64> buffer;
                    const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
                    if (converted.ec != std::errc{}) {
                        throw std::runtime_error("Slider value formatting failed");
                    }
                    text = String::from_utf8(
                               std::string_view{buffer.data(), static_cast<std::size_t>(converted.ptr - buffer.data())})
                               .value();
                }
            } catch (...) {
                if (auto* current = find(id)) {
                    current->formatting_hint = false;
                }
                throw;
            }
            s = find(id);
            if (!s) {
                return;
            }
            s->formatting_hint = false;
            s->hinted_values[i] = value;
            s->hint_titles[i].set(std::move(text));
        }
        const bool active =
            !s->disabled && (s->hint.mode == SliderHintMode::Always ||
                             (s->hint.mode == SliderHintMode::Auto &&
                              (s->hover[i] || s->focus[i].focus_visible || (s->dragging && s->active == i))));
        if (!active) {
            s->hint_dismissed[i] = false;
        }
        s->hint_active[i] = active;
        s->hint_placements[i].set(s->hint.placement);
        s->hint_open[i].set(active && !s->hint_dismissed[i]);
    }
}

void SliderComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto* s = find(id);
    if (!s) {
        return;
    }
    const auto& token = services_->components().theme_scope(id)->snapshot().slider();
    const bool vertical = s->orientation == SliderOrientation::Vertical;
    const float reserve = s->has_labels ? s->label_extent + token.metrics.mark_gap : 0;
    s->rail_bounds = bounds;
    if (vertical) {
        s->rail_bounds.width = std::max(0.0F, bounds.width - reserve);
    } else {
        s->rail_bounds.height = std::max(0.0F, bounds.height - reserve);
    }
    const auto rail_bounds = s->rail_bounds;
    const float length = vertical ? bounds.height : bounds.width;
    const float inset = std::min(length / 2, handle_extent(token.metrics) / 2);
    const float travel = std::max(0.0F, length - 2 * inset);
    for (std::size_t i = 0; i < s->count(); ++i) {
        const double value = i == 0 ? s->value.lower : s->value.upper;
        double ratio = (value - s->limits.minimum) / (s->limits.maximum - s->limits.minimum);
        if (slider_inverted(s->orientation, s->reverse)) {
            ratio = 1 - ratio;
        }
        const float offset = inset + travel * static_cast<float>(ratio);
        s->centers[i] = vertical ? runtime::Point{rail_bounds.x + rail_bounds.width / 2, bounds.y + offset}
                                 : runtime::Point{bounds.x + offset, rail_bounds.y + rail_bounds.height / 2};
        const float hit =
            std::min(vertical ? rail_bounds.width : rail_bounds.height, std::max(24.0F, handle_extent(token.metrics)));
        engine.place_child(s->wrappers[i], {s->centers[i].x - hit / 2, s->centers[i].y - hit / 2, hit, hit});
    }
    for (std::size_t i = 0; i < s->labels.size(); ++i) {
        const auto node = s->labels[i].node;
        const auto size = services_->nodes().require(node).measured_size;
        double ratio = (s->marks[i].value - s->limits.minimum) / (s->limits.maximum - s->limits.minimum);
        if (slider_inverted(s->orientation, s->reverse)) {
            ratio = 1 - ratio;
        }
        const float offset = inset + travel * static_cast<float>(ratio);
        const auto rect = vertical ? runtime::Rect{rail_bounds.x + rail_bounds.width + token.metrics.mark_gap,
                                                   std::clamp(bounds.y + offset - size.height / 2, bounds.y,
                                                              bounds.y + std::max(0.0F, bounds.height - size.height)),
                                                   size.width, size.height}
                                   : runtime::Rect{std::clamp(bounds.x + offset - size.width / 2, bounds.x,
                                                              bounds.x + std::max(0.0F, bounds.width - size.width)),
                                                   rail_bounds.y + rail_bounds.height + token.metrics.mark_gap,
                                                   size.width, size.height};
        engine.place_child(node, rect);
    }
    update_hints(id);
}

void SliderComponentHost::update(runtime::ComponentId id, bool geometry) {
    auto* s = find(id);
    if (!s || s->disposing) {
        return;
    }
    const auto& node = services_->nodes().require(s->node);
    const auto& token = services_->components().theme_scope(id)->snapshot().slider();
    const auto& m = token.metrics;
    const auto& c = token.colors;
    const bool vertical = s->orientation == SliderOrientation::Vertical;
    const auto bounds = s->rail_bounds;
    const float length = vertical ? bounds.height : bounds.width;
    const float inset = std::min(length / 2, handle_extent(m) / 2);
    const float travel = std::max(0.0F, length - 2 * inset);
    const auto axis = [vertical](runtime::Point p) {
        return vertical ? p.y : p.x;
    };
    const float start = (vertical ? bounds.y : bounds.x) + inset;
    float a = s->range ? axis(s->centers[0]) : (slider_inverted(s->orientation, s->reverse) ? start + travel : start);
    float b = axis(s->centers[s->range ? 1 : 0]);
    if (a > b) {
        std::swap(a, b);
    }
    const runtime::Rect rail =
        vertical ? runtime::Rect{bounds.x + (bounds.width - m.rail_size) / 2, start, m.rail_size, travel}
                 : runtime::Rect{start, bounds.y + (bounds.height - m.rail_size) / 2, travel, m.rail_size};
    runtime::Rect track =
        vertical ? runtime::Rect{rail.x, a, rail.width, b - a} : runtime::Rect{a, rail.y, b - a, rail.height};
    if (!s->included) {
        track.width = track.height = 0;
    }
    const bool hover = !s->disabled && (s->rail_hover || s->dragging || s->hover[0] || s->hover[1]);
    const std::array visuals{quad(rail, hover ? c.rail_hover : c.rail, m.rail_size / 2, node.translation),
                             quad(track,
                                  s->disabled ? c.track_disabled
                                  : hover     ? c.track_hover
                                              : c.track,
                                  m.rail_size / 2, node.translation)};
    std::size_t changes = s->surface.valid() ? services_->surfaces().update_surface(s->surface, visuals) : 0;
    const auto selected = [s](double value) {
        if (!s->included) {
            return value == s->value.lower || (s->range && value == s->value.upper);
        }
        return value >= (s->range ? s->value.lower : s->limits.minimum) && value <= s->value.upper;
    };
    auto& dots = s->dot_quads;
    dots.resize(s->points.size() * 2);
    const float dot_line = std::min(m.dot_border_width, m.dot_size / 2);
    const float dot_inner = m.dot_size - 2 * dot_line;
    std::size_t dot_index{};
    for (double value : s->points) {
        double ratio = (value - s->limits.minimum) / (s->limits.maximum - s->limits.minimum);
        if (slider_inverted(s->orientation, s->reverse)) {
            ratio = 1 - ratio;
        }
        const float position = start + travel * static_cast<float>(ratio);
        const auto center = vertical ? runtime::Point{bounds.x + bounds.width / 2, position}
                                     : runtime::Point{position, bounds.y + bounds.height / 2};
        dots[dot_index++] = quad({center.x - m.dot_size / 2, center.y - m.dot_size / 2, m.dot_size, m.dot_size},
                                 s->disabled       ? c.handle_disabled
                                 : selected(value) ? c.dot_active_border
                                                   : c.dot_border,
                                 m.dot_size / 2, node.translation);
        dots[dot_index++] = quad({center.x - dot_inner / 2, center.y - dot_inner / 2, dot_inner, dot_inner},
                                 c.dot_background, dot_inner / 2, node.translation);
    }
    if (s->dot_surface.valid()) {
        const auto previous = services_->surfaces().visual_range(s->dot_surface).count;
        changes += services_->surfaces().update_content_range(s->dot_surface, dots);
        if (previous != dots.size()) {
            services_->mark_scene_structure_dirty();
        }
    }
    const auto& theme = services_->components().theme_scope(id)->snapshot();
    for (std::size_t i = 0; i < s->labels.size(); ++i) {
        const auto color = s->disabled                   ? c.mark_disabled_text
                           : selected(s->marks[i].value) ? c.mark_active_text
                                                         : c.mark_text;
        s->labels[i].foreground.set({color.red(), color.green(), color.blue(), color.alpha()});
        s->labels[i].typography.set({theme.typography().font_family, theme.typography().font_weight, false,
                                     m.mark_font_size, m.mark_line_height});
        services_->nodes().require(s->labels[i].node).translation = node.translation;
    }
    for (std::size_t i = 0; i < s->count(); ++i) {
        services_->nodes().require(s->nodes[i]).translation = node.translation;
        const bool active = !s->disabled && (s->hover[i] || s->focus[i].focused || (s->dragging && s->active == i));
        const float size = active ? m.handle_size_hover : m.handle_size;
        const float line = active ? m.handle_line_width_hover : m.handle_line_width;
        const auto center = s->centers[i];
        const float outer = size + 2 * line;
        const std::array thumb{quad({center.x - outer / 2, center.y - outer / 2, outer, outer},
                                    s->disabled ? c.handle_disabled
                                    : active    ? c.handle_active
                                                : c.handle,
                                    outer / 2, node.translation),
                               quad({center.x - size / 2, center.y - size / 2, size, size}, c.handle_background,
                                    size / 2, node.translation)};
        if (!s->surfaces[i].valid()) {
            continue;
        }
        changes += services_->surfaces().update_surface(s->surfaces[i], thumb);
        component::RetainedSurfaceEffects effects;
        effects.shape = {{center.x - outer / 2, center.y - outer / 2, outer, outer}, outer / 2};
        effects.translation = node.translation;
        effects.focus_color = c.handle_outline;
        effects.focus_width = 6;
        effects.focus_offset = 0;
        effects.focus_opacity = active ? 1.0F : 0.0F;
        changes += services_->surfaces().update_effects(s->surfaces[i], effects);
    }
    if (changes) {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
    }
    if (geometry) {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry);
    }
    update_hints(id);
}

void SliderComponentHost::synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) {
    for (const auto& m : mounted_) {
        update(m.component, false);
    }
}

void SliderComponentHost::change(runtime::ComponentId id, std::size_t thumb, double value) {
    auto* s = find(id);
    if (!s || s->disabled) {
        return;
    }
    auto next = s->gesture ? s->candidate : s->value;
    value = normalize_slider_value(value, s->limits, s->marks, s->marks_only);
    if (s->range) {
        if (thumb == 0) {
            next.lower = std::min(value, next.upper);
        } else {
            next.upper = std::max(value, next.lower);
        }
    } else {
        next.lower = next.upper = value;
    }
    if (next == (s->gesture ? s->candidate : s->value)) {
        return;
    }
    s->candidate = next;
    auto callback = s->on_change;
    if (!s->controlled) {
        s->raw_value = s->value = next;
        update(id, true);
    }
    if (callback) {
        callback(next);
    }
}

void SliderComponentHost::complete(runtime::ComponentId id) {
    auto* s = find(id);
    if (!s || !s->gesture) {
        return;
    }
    const auto value = s->candidate;
    auto callback = s->on_complete;
    s->gesture = s->dragging = false;
    s->key.reset();
    s->capture = {};
    update(id, false);
    if (callback) {
        callback(value);
    }
}

void SliderComponentHost::pointer(runtime::ComponentId id, std::optional<std::size_t> thumb,
                                  input::PointerDispatchContext& event) {
    auto* s = find(id);
    if (!s) {
        return;
    }
    if (event.kind() == input::PointerEventKind::enter || event.kind() == input::PointerEventKind::leave) {
        const bool hover = event.kind() == input::PointerEventKind::enter;
        if (thumb) {
            s->hover[*thumb] = hover;
        } else {
            s->rail_hover = hover;
        }
        update(id, false);
        return;
    }
    if (event.kind() == input::PointerEventKind::cancel) {
        if (s->dragging && s->pointer == event.event().pointer) {
            cancel(id);
        }
        return;
    }
    if (s->disabled) {
        return;
    }
    const auto& e = event.event();
    if (event.kind() == input::PointerEventKind::up && e.button != input::PointerButton::primary) {
        return;
    }
    const bool vertical = s->orientation == SliderOrientation::Vertical;
    const auto& n = services_->nodes().require(s->node);
    const float pos = vertical ? e.y - n.translation.y : e.x - n.translation.x;
    if (event.kind() == input::PointerEventKind::down) {
        if (s->dragging || e.button != input::PointerButton::primary || !event.capture_pointer()) {
            return;
        }
        const bool overlap = s->range && s->centers[0] == s->centers[1];
        if (!overlap) {
            s->active = thumb.value_or(s->active);
        }
        if (!thumb && s->range) {
            const float lower = vertical ? s->centers[0].y : s->centers[0].x;
            const float upper = vertical ? s->centers[1].y : s->centers[1].x;
            if (std::abs(pos - lower) < std::abs(pos - upper)) {
                s->active = 0;
            } else if (std::abs(pos - lower) > std::abs(pos - upper)) {
                s->active = 1;
            }
        }
        s->key.reset();
        s->dragging = s->gesture = true;
        s->candidate = s->value;
        s->pointer = e.pointer;
        s->capture = event.current_target();
        s->pointer_offset = thumb ? pos - (vertical ? s->centers[*thumb].y : s->centers[*thumb].x) : 0;
        static_cast<void>(services_->focus().request_focus(s->thumbs[s->active], input::FocusModality::pointer));
        s = find(id);
        if (!s || !s->dragging) {
            return;
        }
    } else if (!s->dragging || s->pointer != e.pointer) {
        return;
    }
    if (event.kind() != input::PointerEventKind::down && event.kind() != input::PointerEventKind::move &&
        event.kind() != input::PointerEventKind::up) {
        return;
    }
    const auto& token = services_->components().theme_scope(id)->snapshot().slider();
    const float length = vertical ? n.bounds.height : n.bounds.width;
    const float inset = std::min(length / 2, handle_extent(token.metrics) / 2);
    const float travel = length - 2 * inset;
    if (travel > 0) {
        double ratio =
            std::clamp((pos - s->pointer_offset - (vertical ? n.bounds.y : n.bounds.x) - inset) / travel, 0.0F, 1.0F);
        if (slider_inverted(s->orientation, s->reverse)) {
            ratio = 1 - ratio;
        }
        change(id, s->active, std::lerp(s->limits.minimum, s->limits.maximum, ratio));
    }
    s = find(id);
    if (!s) {
        return;
    }
    if (event.kind() == input::PointerEventKind::up && e.button == input::PointerButton::primary) {
        static_cast<void>(event.release_pointer_capture());
        complete(id);
    } else {
        update(id, false);
    }
}

bool SliderComponentHost::keyboard(runtime::ComponentId id, std::size_t thumb, const input::KeyboardInputEvent& event) {
    using input::Key;
    using input::KeyAction;
    const bool command = event.key == Key::left || event.key == Key::right || event.key == Key::up ||
                         event.key == Key::down || event.key == Key::home || event.key == Key::end ||
                         event.key == Key::page_up || event.key == Key::page_down;
    if (!command) {
        return false;
    }
    auto* s = find(id);
    if (!s || s->disabled || !s->keyboard_enabled || s->dragging) {
        return true;
    }
    if (event.action == KeyAction::up) {
        if (s->key == event.key) {
            complete(id);
        }
        return true;
    }
    if (event.modifiers != input::KeyModifier::none) {
        return true;
    }
    s->active = thumb;
    const auto current = s->gesture ? s->candidate : s->value;
    double value = thumb == 0 ? current.lower : current.upper;
    const double before = value;
    if (event.key == Key::home) {
        value = s->limits.minimum;
    } else if (event.key == Key::end) {
        value = s->limits.maximum;
    } else {
        int direction = event.key == Key::right || event.key == Key::up || event.key == Key::page_up ? 1 : -1;
        if (s->reverse && event.key != Key::page_up && event.key != Key::page_down) {
            direction = -direction;
        }
        const int steps = event.key == Key::page_up || event.key == Key::page_down ? 10 : 1;
        value = advance_slider_value(value, s->limits, s->marks, s->marks_only, direction, steps);
    }
    value = normalize_slider_value(std::clamp(value, s->limits.minimum, s->limits.maximum), s->limits, s->marks,
                                   s->marks_only);
    if (s->range) {
        value = thumb == 0 ? std::min(value, current.upper) : std::max(value, current.lower);
    }
    if (value == before) {
        return true;
    }
    if (!s->gesture) {
        s->gesture = true;
        s->candidate = s->value;
    }
    s->key = event.key;
    change(id, thumb, value);
    return true;
}

struct SliderPropsAccess {
    template <class Value, class Derived> static void mount(const SliderPropsBase<Value, Derived>& props) {
        if (!active_slider) {
            throw std::logic_error("Slider requires WindowComponentServices");
        }
        auto& host = *active_slider;
        auto& services = *host.services_;
        const bool range = std::is_same_v<Value, SliderRange>;
        if (props.value_ && props.default_value_) {
            throw std::invalid_argument("Slider value and defaultValue are mutually exclusive");
        }
        const auto limits = read_prop(props.limits_);
        validate_slider_limits(limits);
        const auto orientation = read_prop(props.orientation_);
        validate_orientation(orientation);
        auto marks = sorted_slider_marks(read_prop(props.marks_), limits);
        const bool marks_only = read_prop(props.marks_only_);
        const bool dots = read_prop(props.dots_);
        auto points = slider_visual_points(limits, marks, marks_only, dots);
        const auto hint = read_prop(props.hint_);
        validate_slider_hint(hint);
        const auto raw = props.value_ ? read_prop(*props.value_) : props.default_value_.value_or(Value{});
        const auto as_range = [](Value value) {
            if constexpr (std::is_same_v<Value, SliderRange>) {
                return value;
            } else {
                return SliderRange{value, value};
            }
        };
        const auto initial = normalize_slider_range(as_range(raw), limits, marks, marks_only);
        auto& build = runtime::require_component_build_context();
        const auto id = build.mount_component<SliderState>();
        auto& s = build.state<SliderState>(id);
        s.component = id;
        s.node = build.root(id);
        s.range = range;
        s.controlled = props.value_.has_value();
        s.limits = limits;
        s.marks = std::move(marks);
        s.marks_only = marks_only;
        s.dots = dots;
        s.points = std::move(points);
        s.included = read_prop(props.included_);
        s.hint = hint;
        s.hint_formatter = props.hint_formatter_;
        s.raw_value = as_range(raw);
        s.value = s.candidate = initial;
        s.orientation = orientation;
        s.reverse = read_prop(props.reverse_);
        s.disabled = read_prop(props.disabled_);
        s.keyboard_enabled = read_prop(props.keyboard_);
        const auto callback = [](const std::function<void(Value)>& fn) -> std::function<void(SliderRange)> {
            if (!fn) {
                return {};
            }
            return [fn](SliderRange value) {
                if constexpr (std::is_same_v<Value, SliderRange>) {
                    fn(value);
                } else {
                    fn(value.lower);
                }
            };
        };
        s.on_change = callback(props.on_change_);
        s.on_complete = callback(props.on_complete_);
        build.on_resource_cleanup(id, [&host, id] {
            if (auto* state = host.find(id)) {
                host.release(*state);
            }
        });
        runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
        std::optional<input::InteractionId> parent;
        for (auto ancestor = services.components().parent(id); ancestor && !parent;
             ancestor = services.components().parent(*ancestor)) {
            for (auto interaction : services.interactions().declaration_order()) {
                if (services.interactions().require(interaction).component == *ancestor) {
                    parent = interaction;
                    break;
                }
            }
        }
        s.rail = services.interactions().create({id, s.node, parent, !s.disabled, false, {}, false});
        input::InteractionHandlers rail_handlers;
        rail_handlers.target = [&host, id](input::PointerDispatchContext& event) {
            host.pointer(id, {}, event);
        };
        services.interactions().set_handlers(s.rail, std::move(rail_handlers));
        const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
        const std::array<graphics::QuadInstance, 2> empty{};
        s.surface = services.surfaces().create_surface(id, s.node, fragment, empty, {}, s.rail);
        const auto dot_fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
        s.dot_surface = services.surfaces().create_content_range(dot_fragment, {});
        build.mount_slot(id, Content{[&] {
                             auto& nested = runtime::require_component_build_context();
                             for (std::size_t i = 0; i < s.count(); ++i) {
                                 Tooltip(TooltipProps{}
                                             .title(s.hint_titles[i])
                                             .open(s.hint_open[i])
                                             .placement(s.hint_placements[i])
                                             .trigger(TooltipTriggerMode::Manual)
                                             .onOpenChange([&host, id, i](bool open) {
                                                 if (auto* state = host.find(id); state && !open) {
                                                     state->hint_dismissed[i] = true;
                                                     state->hint_open[i].set(false);
                                                 }
                                             }),
                                         TooltipTrigger{[&host, id, i] {
                                             auto& services = *host.services_;
                                             auto& nested = runtime::require_component_build_context();
                                             auto& s = *host.find(id);
                                             s.children[i] = nested.mount_component<int>(0);
                                             s.nodes[i] = nested.root(s.children[i]);
                                             nested.on_resource_cleanup(s.children[i], [&host, id, i] {
                                                 auto* state = host.find(id);
                                                 if (!state) {
                                                     return;
                                                 }
                                                 state->disposing = true;
                                                 auto& services = *host.services_;
                                                 const auto interaction = state->thumbs[i];
                                                 if (interaction.valid()) {
                                                     services.pointer().cancel_interaction(interaction);
                                                     services.focus().cancel_interaction(interaction);
                                                     services.interactions().remove(interaction);
                                                 }
                                                 if (state->surfaces[i].valid()) {
                                                     services.surfaces().destroy(state->surfaces[i]);
                                                 }
                                                 services.layout().remove_layout(state->nodes[i]);
                                             });
                                             services.layout().set_layout(s.nodes[i], layout::LeafLayout{{24, 24}});
                                             // The host selects the active thumb before assigning pointer focus,
                                             // including when overlapping hit regions would prefer paint order.
                                             s.thumbs[i] = services.interactions().create(
                                                 {s.children[i], s.nodes[i], s.rail, !s.disabled, true, {}, false});
                                             input::InteractionHandlers handlers;
                                             handlers.target = [&host, id, i](input::PointerDispatchContext& event) {
                                                 host.pointer(id, i, event);
                                             };
                                             services.interactions().set_handlers(s.thumbs[i], std::move(handlers));
                                             input::FocusHandlers focus;
                                             focus.state_changed = [&host, id, i](input::FocusPresentation value) {
                                                 if (auto* state = host.find(id)) {
                                                     if (state->focus[i].focused && !value.focused && state->key) {
                                                         host.cancel(id);
                                                     }
                                                     state = host.find(id);
                                                     if (!state) {
                                                         return;
                                                     }
                                                     state->focus[i] = value;
                                                     if (value.focused) {
                                                         state->active = i;
                                                     }
                                                     host.update(id, false);
                                                 }
                                             };
                                             focus.text_edit = [&host, id, i](const input::KeyboardInputEvent& e) {
                                                 return host.keyboard(id, i, e);
                                             };
                                             services.interactions().set_focus_handlers(s.thumbs[i], std::move(focus));
                                             const auto thumb_fragment = nested.register_scene_fragment(
                                                 s.children[i], runtime::SceneFragmentPlacement::before_children);
                                             const std::array<graphics::QuadInstance, 2> thumb_empty{};
                                             s.surfaces[i] = services.surfaces().create_surface(
                                                 s.children[i], s.nodes[i], thumb_fragment, thumb_empty, {},
                                                 s.thumbs[i]);
                                         }});
                                 const auto wrapper = services.tooltip().mounted().back();
                                 s.wrappers[i] = services.components().root(wrapper);
                             }
                             host.mount_labels(id, nested);
                         }});
        services.layout().set_layout(
            s.node,
            layout::ComponentLayout{
                [&host, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                    auto& state = *host.find(id);
                    const auto& metrics = host.services_->components().theme_scope(id)->snapshot().slider().metrics;
                    for (std::size_t i = 0; i < state.count(); ++i) {
                        static_cast<void>(engine.measure_child(state.wrappers[i], layout::Constraints::fixed(24, 24)));
                    }
                    const float cross = std::max(32.0F, handle_extent(metrics));
                    state.label_extent = 0;
                    state.has_labels = false;
                    for (std::size_t i = 0; i < state.labels.size(); ++i) {
                        const bool visible = !state.marks[i].label.empty();
                        const auto size = engine.measure_child(
                            state.labels[i].node, visible
                                                      ? layout::Constraints{0, limits.max_width, 0, limits.max_height}
                                                      : layout::Constraints::fixed(0, 0));
                        if (visible) {
                            state.has_labels = true;
                            state.label_extent =
                                std::max(state.label_extent,
                                         state.orientation == SliderOrientation::Vertical ? size.width : size.height);
                        }
                    }
                    const float extent = cross + (state.has_labels ? metrics.mark_gap + state.label_extent : 0);
                    return limits.constrain(state.orientation == SliderOrientation::Vertical
                                                ? runtime::Size{extent, 160}
                                                : runtime::Size{160, extent});
                },
                [&host, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                    host.place(id, engine, rect);
                }});
        auto& scope = build.scope(id);
        if (props.value_) {
            connect_prop(scope, *props.value_, [&host, id, as_range](Value value) {
                auto* state = host.find(id);
                if (!state) {
                    return;
                }
                const auto next =
                    normalize_slider_range(as_range(value), state->limits, state->marks, state->marks_only);
                state->raw_value = as_range(value);
                state->value = next;
                if (!state->gesture) {
                    state->candidate = next;
                }
                host.update(id, true);
            });
        }
        connect_prop(scope, props.limits_, [&host, id](SliderLimits limits) {
            validate_slider_limits(limits);
            auto* state = host.find(id);
            if (!state) {
                return;
            }
            validate_slider_marks(state->marks, limits);
            auto points = slider_visual_points(limits, state->marks, state->marks_only, state->dots);
            const auto next = normalize_slider_range(state->raw_value, limits, state->marks, state->marks_only);
            host.cancel(id);
            state = host.find(id);
            if (!state) {
                return;
            }
            state->limits = limits;
            state->points = std::move(points);
            state->value = state->candidate = next;
            host.update(id, true);
        });
        connect_prop(scope, props.marks_, [&host, id](SliderMarks marks) {
            auto* state = host.find(id);
            if (!state) {
                return;
            }
            marks = sorted_slider_marks(std::move(marks), state->limits);
            if (marks == state->marks) {
                return;
            }
            auto points = slider_visual_points(state->limits, marks, state->marks_only, state->dots);
            const auto next = normalize_slider_range(state->raw_value, state->limits, marks, state->marks_only);
            host.cancel(id);
            state = host.find(id);
            if (!state) {
                return;
            }
            state->marks = std::move(marks);
            state->points = std::move(points);
            state->value = state->candidate = next;
            host.synchronize_labels(id);
            host.services_->dirty().invalidate(state->node, runtime::DirtyFlags::Measure);
            host.update(id, true);
        });
        connect_prop(scope, props.marks_only_, [&host, id](bool value) {
            auto* state = host.find(id);
            if (!state) {
                return;
            }
            auto points = slider_visual_points(state->limits, state->marks, value, state->dots);
            const auto next = normalize_slider_range(state->raw_value, state->limits, state->marks, value);
            host.cancel(id);
            state = host.find(id);
            if (!state) {
                return;
            }
            state->marks_only = value;
            state->points = std::move(points);
            state->value = state->candidate = next;
            host.update(id, true);
        });
        connect_prop(scope, props.dots_, [&host, id](bool value) {
            if (auto* state = host.find(id)) {
                auto points = slider_visual_points(state->limits, state->marks, state->marks_only, value);
                state->dots = value;
                state->points = std::move(points);
                host.update(id, true);
            }
        });
        connect_prop(scope, props.included_, [&host, id](bool value) {
            if (auto* state = host.find(id)) {
                state->included = value;
                host.update(id, false);
            }
        });
        connect_prop(scope, props.hint_, [&host, id](SliderHintOptions value) {
            validate_slider_hint(value);
            if (auto* state = host.find(id)) {
                state->hint = std::move(value);
                host.update(id, false);
            }
        });
        connect_prop(scope, props.disabled_, [&host, id](bool disabled) {
            auto* state = host.find(id);
            if (!state || state->disabled == disabled) {
                return;
            }
            host.cancel(id);
            state = host.find(id);
            if (!state) {
                return;
            }
            state->disabled = disabled;
            host.services_->interactions().set_eligible(state->rail, !disabled);
            for (std::size_t i = 0; i < state->count(); ++i) {
                host.services_->interactions().set_eligible(state->thumbs[i], !disabled);
                if (disabled) {
                    host.services_->focus().cancel_interaction(state->thumbs[i]);
                }
            }
            for (auto& label : state->labels) {
                host.services_->interactions().set_eligible(label.interaction, !disabled);
                if (disabled) {
                    host.services_->pointer().cancel_interaction(label.interaction);
                    static_cast<void>(label.pressable.reset());
                }
            }
            host.update(id, false);
        });
        connect_prop(scope, props.keyboard_, [&host, id](bool value) {
            host.cancel(id);
            if (auto* state = host.find(id)) {
                state->keyboard_enabled = value;
            }
        });
        connect_prop(scope, props.orientation_, [&host, id](SliderOrientation value) {
            validate_orientation(value);
            host.cancel(id);
            if (auto* state = host.find(id)) {
                state->orientation = value;
            }
            if (auto* state = host.find(id)) {
                host.services_->dirty().invalidate(state->node, runtime::DirtyFlags::Measure);
            }
            host.update(id, true);
        });
        connect_prop(scope, props.reverse_, [&host, id](bool value) {
            host.cancel(id);
            if (auto* state = host.find(id)) {
                state->reverse = value;
            }
            host.update(id, true);
        });
        const auto theme = services.components().theme_scope(id);
        s.colors = theme->capture([&host, id](theme_runtime::DirtyPhase) { host.update(id, false); },
                                  [theme] { static_cast<void>(theme->slider_colors()); });
        s.metrics = theme->capture(
            [&host, id](theme_runtime::DirtyPhase) {
                if (auto* state = host.find(id)) {
                    host.services_->dirty().invalidate(state->node, runtime::DirtyFlags::Measure);
                }
                host.update(id, true);
            },
            [theme] { static_cast<void>(theme->slider_metrics()); });
        s.fonts = theme->capture([&host, id](theme_runtime::DirtyPhase) { host.update(id, false); },
                                 [theme] { static_cast<void>(theme->typography_fonts()); });
        host.mounted_.push_back({id, s.node, s.thumbs, s.surface, range});
        host.update(id, true);
    }
};
} // namespace ryn::detail

namespace ryn {
void Slider(SliderProps props) {
    detail::SliderPropsAccess::mount(props);
}

void RangeSlider(RangeSliderProps props) {
    detail::SliderPropsAccess::mount(props);
}
} // namespace ryn
