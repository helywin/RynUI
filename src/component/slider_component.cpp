#include "component/slider_component.hpp"
#include "component/slider_value.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
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

struct SliderState final {
    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId rail;
    component::RetainedSurfaceId surface;
    std::array<runtime::ComponentId, 2> children;
    std::array<runtime::NodeId, 2> nodes;
    std::array<input::InteractionId, 2> thumbs;
    std::array<component::RetainedSurfaceId, 2> surfaces;
    std::array<input::FocusPresentation, 2> focus;
    std::array<bool, 2> hover{};
    std::array<runtime::Point, 2> centers;
    SliderRange value;
    SliderRange candidate;
    SliderRange raw_value;
    SliderLimits limits;
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
    services_->layout().remove_layout(s.node);
}

void SliderComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto* s = find(id);
    if (!s) {
        return;
    }
    const auto& token = services_->components().theme_scope(id)->snapshot().slider();
    const bool vertical = s->orientation == SliderOrientation::Vertical;
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
        s->centers[i] = vertical ? runtime::Point{bounds.x + bounds.width / 2, bounds.y + offset}
                                 : runtime::Point{bounds.x + offset, bounds.y + bounds.height / 2};
        const float hit =
            std::min(vertical ? bounds.width : bounds.height, std::max(24.0F, handle_extent(token.metrics)));
        engine.place_child(s->nodes[i], {s->centers[i].x - hit / 2, s->centers[i].y - hit / 2, hit, hit});
    }
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
    const auto bounds = node.bounds;
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
    const runtime::Rect track =
        vertical ? runtime::Rect{rail.x, a, rail.width, b - a} : runtime::Rect{a, rail.y, b - a, rail.height};
    const bool hover = !s->disabled && (s->rail_hover || s->dragging || s->hover[0] || s->hover[1]);
    const std::array visuals{quad(rail, hover ? c.rail_hover : c.rail, m.rail_size / 2, node.translation),
                             quad(track,
                                  s->disabled ? c.track_disabled
                                  : hover     ? c.track_hover
                                              : c.track,
                                  m.rail_size / 2, node.translation)};
    std::size_t changes = s->surface.valid() ? services_->surfaces().update_surface(s->surface, visuals) : 0;
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
    value = normalize_slider_value(value, s->limits);
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
        double direction = event.key == Key::right || event.key == Key::up || event.key == Key::page_up ? 1 : -1;
        if (s->reverse && event.key != Key::page_up && event.key != Key::page_down) {
            direction = -direction;
        }
        const double steps = event.key == Key::page_up || event.key == Key::page_down ? 10 : 1;
        value = std::clamp(value + direction * steps * s->limits.step, s->limits.minimum, s->limits.maximum);
        // At an extra maximum endpoint, one decrement returns to the last grid point.
        if (direction < 0 && before == s->limits.maximum) {
            value = std::fma(std::ceil((before - s->limits.minimum) / s->limits.step) - steps, s->limits.step,
                             s->limits.minimum);
        }
    }
    value = normalize_slider_value(std::clamp(value, s->limits.minimum, s->limits.maximum), s->limits);
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
        const auto raw = props.value_ ? read_prop(*props.value_) : props.default_value_.value_or(Value{});
        const auto as_range = [](Value value) {
            if constexpr (std::is_same_v<Value, SliderRange>) {
                return value;
            } else {
                return SliderRange{value, value};
            }
        };
        const auto initial = normalize_slider_range(as_range(raw), limits);
        auto& build = runtime::require_component_build_context();
        const auto id = build.mount_component<SliderState>();
        auto& s = build.state<SliderState>(id);
        s.component = id;
        s.node = build.root(id);
        s.range = range;
        s.controlled = props.value_.has_value();
        s.limits = limits;
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
        build.mount_slot(id, Content{[&] {
                             auto& nested = runtime::require_component_build_context();
                             for (std::size_t i = 0; i < s.count(); ++i) {
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
                                 s.surfaces[i] = services.surfaces().create_surface(
                                     s.children[i], s.nodes[i], thumb_fragment, empty, {}, s.thumbs[i]);
                             }
                         }});
        services.layout().set_layout(
            s.node,
            layout::ComponentLayout{
                [&host, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                    auto& state = *host.find(id);
                    const auto& metrics = host.services_->components().theme_scope(id)->snapshot().slider().metrics;
                    for (std::size_t i = 0; i < state.count(); ++i) {
                        static_cast<void>(engine.measure_child(state.nodes[i], layout::Constraints::fixed(24, 24)));
                    }
                    const float cross = std::max(32.0F, handle_extent(metrics));
                    return limits.constrain(state.orientation == SliderOrientation::Vertical
                                                ? runtime::Size{cross, 160}
                                                : runtime::Size{160, cross});
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
                const auto next = normalize_slider_range(as_range(value), state->limits);
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
            const auto next = normalize_slider_range(state->raw_value, limits);
            host.cancel(id);
            state = host.find(id);
            if (!state) {
                return;
            }
            state->limits = limits;
            state->value = state->candidate = next;
            host.update(id, true);
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
