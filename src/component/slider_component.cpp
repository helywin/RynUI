#include "component/slider_component.hpp"
#include "component/slider_value.hpp"
#include "component/tooltip_component.hpp"
#include "input/pressable_behavior.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <charconv>
#include <memory>
#include <numeric>
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

struct SliderThumb final {
    runtime::ComponentId component;
    runtime::ComponentId tooltip;
    runtime::NodeId node;
    runtime::NodeId wrapper;
    input::InteractionId interaction;
    component::RetainedSurfaceId surface;
    Signal<String> hint_title{String{}};
    Signal<bool> hint_open{false};
    Signal<TooltipPlacement> hint_placement{TooltipPlacement::Top};
    bool hint_dismissed{};
    std::optional<double> hinted_value;
    input::FocusPresentation focus;
    bool hover{};
    bool disabled{};
    runtime::Point center;
};

struct SliderState final {
    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId rail;
    component::RetainedSurfaceId surface;
    std::vector<std::unique_ptr<SliderThumb>> handles;
    bool formatting_hint{};
    std::vector<SliderLabel> labels;
    runtime::Rect rail_bounds;
    float label_extent{};
    bool has_labels{};
    component::RetainedSurfaceId dot_surface;
    SliderValues value;
    SliderValues candidate;
    SliderValues raw_value;
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
    bool multiple{};
    bool reconciling{};
    SliderRangeOptions options;
    bool controlled{};
    bool disabled{};
    SliderDisabledHandles disabled_handles;
    bool keyboard_enabled{true};
    bool reverse{};
    bool rail_hover{};
    bool dragging{};
    bool dragging_track{};
    SliderValues drag_origin;
    float drag_start{};
    bool delete_preview{};
    bool editing_command{};
    bool gesture{};
    bool disposing{};
    SliderOrientation orientation{SliderOrientation::Horizontal};
    std::size_t active{};
    input::PointerIdentity pointer;
    input::InteractionId capture;
    float pointer_offset{};
    std::optional<input::Key> key;
    std::function<void(SliderValues)> on_change;
    std::function<void(SliderValues)> on_complete;
    theme_runtime::Subscription colors;
    theme_runtime::Subscription metrics;
    theme_runtime::Subscription fonts;

    std::size_t count() const noexcept {
        return handles.size();
    }

    bool handle_disabled(std::size_t index) const noexcept {
        return disabled || (index < disabled_handles.size() && disabled_handles[index]);
    }

    bool any_disabled() const noexcept {
        for (std::size_t i = 0; i < count(); ++i) {
            if (handle_disabled(i)) {
                return true;
            }
        }
        return false;
    }

    bool editable() const noexcept {
        return multiple && options.editable && !disabled && !any_disabled();
    }

    std::optional<std::size_t> nearest_enabled(double target) const {
        std::optional<std::size_t> nearest;
        if (active < count() && !handle_disabled(active)) {
            nearest = active;
        }
        for (std::size_t i = 0; i < count(); ++i) {
            if (!handle_disabled(i) && (!nearest || std::abs(target - value[i]) < std::abs(target - value[*nearest]))) {
                nearest = i;
            }
        }
        return nearest;
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
    SliderSnapshot result;
    result.value = s->value.empty() ? SliderRange{} : SliderRange{s->value.front(), s->value.back()};
    result.limits = s->limits;
    result.range = s->range;
    result.disabled = s->disabled;
    result.dragging = s->dragging;
    result.reverse = s->reverse;
    result.orientation = s->orientation;
    result.values = s->value;
    result.delete_preview = s->delete_preview;
    for (const auto& thumb : s->handles) {
        result.centers.push_back(thumb->center);
        result.focus.push_back(thumb->focus);
    }
    return result;
}

void SliderComponentHost::cancel(runtime::ComponentId id) {
    auto* s = find(id);
    if (!s) {
        return;
    }
    const auto capture = s->capture;
    s->dragging = s->gesture = false;
    s->dragging_track = false;
    s->delete_preview = false;
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
        for (std::size_t i = 0; i < mounted_.size();) {
            const auto id = mounted_[i].component;
            cancel(id);
            if (i < mounted_.size() && mounted_[i].component == id) {
                ++i;
            }
        }
    }
}

void SliderComponentHost::release(SliderState& s) {
    s.disposing = true;
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

std::optional<std::size_t> SliderComponentHost::thumb_index(runtime::ComponentId id, runtime::ComponentId thumb) const {
    const auto* state = services_->components().state<SliderState>(id);
    if (!state) {
        return {};
    }
    for (std::size_t i = 0; i < state->handles.size(); ++i) {
        if (state->handles[i]->component == thumb) {
            return i;
        }
    }
    return {};
}

void SliderComponentHost::mount_thumb(runtime::ComponentId id, SliderThumb& thumb, runtime::ComponentBuildContext&) {
    Tooltip(TooltipProps{}
                .title(thumb.hint_title)
                .open(thumb.hint_open)
                .placement(thumb.hint_placement)
                .trigger(TooltipTriggerMode::Manual)
                .onOpenChange([this, id, &thumb](bool open) {
                    if (auto* state = find(id); state && !open && thumb_index(id, thumb.component)) {
                        thumb.hint_dismissed = true;
                        thumb.hint_open.set(false);
                    }
                }),
            TooltipTrigger{[this, id, &thumb] {
                auto& build = runtime::require_component_build_context();
                const auto* state = find(id);
                thumb.component = build.mount_component<int>(0);
                thumb.node = build.root(thumb.component);
                build.on_resource_cleanup(thumb.component, [this, &thumb] {
                    if (thumb.interaction.valid()) {
                        services_->pointer().cancel_interaction(thumb.interaction);
                        services_->focus().cancel_interaction(thumb.interaction);
                        services_->interactions().remove(thumb.interaction);
                    }
                    if (thumb.surface.valid()) {
                        services_->surfaces().destroy(thumb.surface);
                    }
                    services_->layout().remove_layout(thumb.node);
                });
                services_->layout().set_layout(thumb.node, layout::LeafLayout{{24, 24}});
                thumb.interaction = services_->interactions().create(
                    {thumb.component, thumb.node, state->rail, !state->disabled && !thumb.disabled, true, {}, false});
                const auto component = thumb.component;
                input::InteractionHandlers handlers;
                handlers.target = [this, id, component](input::PointerDispatchContext& event) {
                    if (const auto index = thumb_index(id, component)) {
                        pointer(id, *index, event);
                    }
                };
                services_->interactions().set_handlers(thumb.interaction, std::move(handlers));
                input::FocusHandlers focus;
                focus.state_changed = [this, id, component](input::FocusPresentation value) {
                    auto index = thumb_index(id, component);
                    auto* state = find(id);
                    if (!state || !index || state->reconciling || !services_->components().scope(id).active()) {
                        return;
                    }
                    if (state->handles[*index]->focus.focused && !value.focused && state->key) {
                        cancel(id);
                    }
                    state = find(id);
                    index = thumb_index(id, component);
                    if (!state || !index) {
                        return;
                    }
                    state->handles[*index]->focus = value;
                    if (value.focused) {
                        state->active = *index;
                    }
                    update(id, false);
                };
                focus.text_edit = [this, id, component](const input::KeyboardInputEvent& event) {
                    const auto index = thumb_index(id, component);
                    return index && keyboard(id, *index, event);
                };
                services_->interactions().set_focus_handlers(thumb.interaction, std::move(focus));
                const auto fragment =
                    build.register_scene_fragment(thumb.component, runtime::SceneFragmentPlacement::before_children);
                const std::array<graphics::QuadInstance, 2> empty{};
                thumb.surface = services_->surfaces().create_surface(thumb.component, thumb.node, fragment, empty, {},
                                                                     thumb.interaction);
            }});
    thumb.tooltip = services_->tooltip().mounted().back();
    thumb.wrapper = services_->components().root(thumb.tooltip);
}

void SliderComponentHost::set_values(runtime::ComponentId id, SliderValues values) {
    auto* state = find(id);
    if (!state) {
        return;
    }
    if (values.size() == state->count()) {
        state->value = std::move(values);
        return;
    }
    // Match unchanged sorted values, including duplicate occurrences, without
    // replacing their retained component, focus or Tooltip state.
    std::array<std::array<std::uint8_t, 65>, 65> common{};
    for (std::size_t a = state->value.size(); a-- > 0;) {
        for (std::size_t b = values.size(); b-- > 0;) {
            common[a][b] = state->value[a] == values[b] ? static_cast<std::uint8_t>(1 + common[a + 1][b + 1])
                                                        : std::max(common[a + 1][b], common[a][b + 1]);
        }
    }
    std::vector<std::size_t> matched(values.size(), 64);
    std::size_t a{};
    std::size_t b{};
    while (a < state->value.size() && b < values.size()) {
        if (state->value[a] == values[b]) {
            matched[b++] = a++;
        } else if (common[a + 1][b] >= common[a][b + 1]) {
            ++a;
        } else {
            ++b;
        }
    }
    std::vector<std::unique_ptr<SliderThumb>> replacement(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (matched[i] == 64) {
            replacement[i] = std::make_unique<SliderThumb>();
            replacement[i]->disabled = state->handle_disabled(i);
        }
    }
    const auto active =
        state->active < state->count() ? state->handles[state->active]->component : runtime::ComponentId{};
    state->reconciling = true;
    try {
        if (std::ranges::any_of(replacement, [](const auto& thumb) { return bool(thumb); })) {
            services_->append_slot(id, Content{[this, id, &replacement] {
                                       auto& build = runtime::require_component_build_context();
                                       for (const auto& thumb : replacement) {
                                           if (thumb) {
                                               mount_thumb(id, *thumb, build);
                                           }
                                       }
                                   }});
        }
    } catch (...) {
        if (auto* current = find(id)) {
            current->reconciling = false;
        }
        throw;
    }
    auto removed = std::move(state->handles);
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (matched[i] != 64) {
            replacement[i] = std::move(removed[matched[i]]);
        }
    }
    state->handles = std::move(replacement);
    state->value = std::move(values);
    state->active = thumb_index(id, active).value_or(0);
    for (const auto& thumb : removed) {
        if (thumb) {
            services_->destroy(thumb->tooltip);
        }
    }
    state = find(id);
    if (!state) {
        return;
    }
    state->reconciling = false;
    synchronize_disabled(id);
    state = find(id);
    if (!state) {
        return;
    }
    for (auto& item : mounted_) {
        if (item.component == id) {
            item.thumbs.clear();
            for (const auto& thumb : state->handles) {
                item.thumbs.push_back(thumb->interaction);
            }
            services_->interactions().reorder_after(state->rail, item.thumbs);
            break;
        }
    }
    services_->dirty().invalidate(state->node, runtime::DirtyFlags::Measure);
    services_->mark_scene_structure_dirty();
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
            s->gesture = true;
            s->candidate = s->value;
            select_value(id, value);
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
    if (!s || s->formatting_hint || s->reconciling || !services_->components().scope(id).active()) {
        return;
    }
    const auto count = s->count();
    for (std::size_t i = 0; i < count; ++i) {
        s = find(id);
        if (!s || i >= s->count()) {
            return;
        }
        const double value = s->value[i];
        const auto component = s->handles[i]->component;
        if (s->handles[i]->hinted_value != value) {
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
            if (i >= s->count() || s->handles[i]->component != component || s->value[i] != value) {
                update_hints(id);
                return;
            }
            s->handles[i]->hinted_value = value;
            s->handles[i]->hint_title.set(std::move(text));
        }
        const bool active =
            !s->handle_disabled(i) && !(s->delete_preview && s->active == i) &&
            (s->hint.mode == SliderHintMode::Always ||
             (s->hint.mode == SliderHintMode::Auto && (s->handles[i]->hover || s->handles[i]->focus.focus_visible ||
                                                       (s->dragging && (s->dragging_track || s->active == i)))));
        if (!active) {
            s->handles[i]->hint_dismissed = false;
        }
        s->handles[i]->hint_placement.set(s->hint.placement);
        s->handles[i]->hint_open.set(active && !s->handles[i]->hint_dismissed);
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
        const double value = s->value[i];
        double ratio = (value - s->limits.minimum) / (s->limits.maximum - s->limits.minimum);
        if (slider_inverted(s->orientation, s->reverse)) {
            ratio = 1 - ratio;
        }
        const float offset = inset + travel * static_cast<float>(ratio);
        s->handles[i]->center = vertical ? runtime::Point{rail_bounds.x + rail_bounds.width / 2, bounds.y + offset}
                                         : runtime::Point{bounds.x + offset, rail_bounds.y + rail_bounds.height / 2};
        const float hit =
            std::min(vertical ? rail_bounds.width : rail_bounds.height, std::max(24.0F, handle_extent(token.metrics)));
        engine.place_child(s->handles[i]->wrapper,
                           {s->handles[i]->center.x - hit / 2, s->handles[i]->center.y - hit / 2, hit, hit});
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
    if (!s || s->disposing || s->reconciling || !services_->components().scope(id).active()) {
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
    float a = s->range && !s->handles.empty() ? axis(s->handles.front()->center)
                                              : (slider_inverted(s->orientation, s->reverse) ? start + travel : start);
    float b = s->handles.empty() ? a : axis(s->handles.back()->center);
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
    const bool hover =
        !s->disabled && (s->rail_hover || s->dragging ||
                         std::ranges::any_of(s->handles, [](const auto& thumb) { return thumb->hover; }));
    const std::array visuals{quad(rail, hover ? c.rail_hover : c.rail, m.rail_size / 2, node.translation),
                             quad(track,
                                  s->disabled ? c.track_disabled
                                  : hover     ? c.track_hover
                                              : c.track,
                                  m.rail_size / 2, node.translation)};
    std::size_t changes = s->surface.valid() ? services_->surfaces().update_surface(s->surface, visuals) : 0;
    const auto selected = [s](double value) {
        if (s->value.empty()) {
            return false;
        }
        if (!s->included) {
            return std::ranges::find(s->value, value) != s->value.end();
        }
        return value >= (s->range ? s->value.front() : s->limits.minimum) && value <= s->value.back();
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
        services_->nodes().require(s->handles[i]->node).translation = node.translation;
        const bool disabled = s->handle_disabled(i) || (s->delete_preview && s->active == i);
        const bool active =
            !disabled && (s->handles[i]->hover || s->handles[i]->focus.focused || (s->dragging && s->active == i));
        const float size = active ? m.handle_size_hover : m.handle_size;
        const float line = active ? m.handle_line_width_hover : m.handle_line_width;
        const auto center = s->handles[i]->center;
        const float outer = size + 2 * line;
        const std::array thumb{quad({center.x - outer / 2, center.y - outer / 2, outer, outer},
                                    disabled ? c.handle_disabled
                                    : active ? c.handle_active
                                             : c.handle,
                                    outer / 2, node.translation),
                               quad({center.x - size / 2, center.y - size / 2, size, size}, c.handle_background,
                                    size / 2, node.translation)};
        if (!s->handles[i]->surface.valid()) {
            continue;
        }
        changes += services_->surfaces().update_surface(s->handles[i]->surface, thumb);
        component::RetainedSurfaceEffects effects;
        effects.shape = {{center.x - outer / 2, center.y - outer / 2, outer, outer}, outer / 2};
        effects.translation = node.translation;
        effects.focus_color = c.handle_outline;
        effects.focus_width = 6;
        effects.focus_offset = 0;
        effects.focus_opacity = active ? 1.0F : 0.0F;
        changes += services_->surfaces().update_effects(s->handles[i]->surface, effects);
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
    for (std::size_t i = 0; i < mounted_.size();) {
        const auto id = mounted_[i].component;
        update(id, false);
        if (i < mounted_.size() && mounted_[i].component == id) {
            ++i;
        }
    }
}

void SliderComponentHost::synchronize_disabled(runtime::ComponentId id) {
    auto* state = find(id);
    for (std::size_t i = 0; state && i < state->count(); ++i) {
        auto& thumb = *state->handles[i];
        thumb.disabled = state->handle_disabled(i);
        services_->interactions().set_eligible(thumb.interaction, !thumb.disabled);
        if (thumb.disabled) {
            services_->focus().cancel_interaction(thumb.interaction);
        }
        state = find(id);
    }
}

std::optional<std::size_t> SliderComponentHost::select_value(runtime::ComponentId id, double value) {
    auto* s = find(id);
    if (!s || s->disabled) {
        return {};
    }
    value = normalize_slider_value(value, s->limits, s->marks, s->marks_only);
    auto next = s->gesture ? s->candidate : s->value;
    if (s->editable() && next.size() < s->options.max_count && std::ranges::find(next, value) == next.end()) {
        const auto position = std::ranges::lower_bound(next, value);
        const auto index = static_cast<std::size_t>(position - next.begin());
        next.insert(position, value);
        s->active = index;
        change_values(id, std::move(next));
        if (auto* current = find(id)) {
            current->active = index;
            if (current->value == current->candidate && index < current->count()) {
                services_->focus().request_focus(current->handles[index]->interaction, input::FocusModality::pointer);
            }
        }
        return index;
    }
    const auto nearest = s->nearest_enabled(value);
    if (nearest) {
        s->active = *nearest;
        services_->focus().request_focus(s->handles[*nearest]->interaction, input::FocusModality::pointer);
        change(id, *nearest, value);
    }
    return nearest;
}

void SliderComponentHost::delete_handle(runtime::ComponentId id, std::size_t index) {
    auto* s = find(id);
    if (!s || !s->editable()) {
        return;
    }
    auto next = s->gesture ? s->candidate : s->value;
    if (index >= next.size() || next.size() <= s->options.min_count) {
        return;
    }
    const auto previous_count = s->count();
    s->gesture = true;
    s->candidate = next;
    s->editing_command = true;
    s->key.reset();
    s->capture = {};
    next.erase(next.begin() + index);
    try {
        change_values(id, std::move(next));
    } catch (...) {
        if (auto* current = find(id)) {
            current->editing_command = false;
        }
        throw;
    }
    s = find(id);
    if (!s) {
        return;
    }
    s->editing_command = false;
    if (s->count() != previous_count && !s->handles.empty()) {
        const auto next_index = std::min(index, s->count() - 1);
        s->active = next_index;
        services_->focus().defer_focus(s->handles[next_index]->interaction, input::FocusModality::keyboard);
    }
    complete(id);
}

void SliderComponentHost::change(runtime::ComponentId id, std::size_t thumb, double value) {
    auto* s = find(id);
    if (!s || s->disabled) {
        return;
    }
    auto next = s->gesture ? s->candidate : s->value;
    value = normalize_slider_value(value, s->limits, s->marks, s->marks_only);
    if (thumb >= next.size()) {
        return;
    }
    const double lower = thumb == 0 ? s->limits.minimum : next[thumb - 1];
    const double upper = thumb + 1 == next.size() ? s->limits.maximum : next[thumb + 1];
    next[thumb] = std::clamp(value, lower, upper);
    change_values(id, std::move(next));
}

void SliderComponentHost::change_values(runtime::ComponentId id, SliderValues next) {
    auto* s = find(id);
    if (!s || s->disabled) {
        return;
    }
    validate_slider_range_options(s->options, s->marks_only, next.size());
    if (next == (s->gesture ? s->candidate : s->value)) {
        return;
    }
    s->candidate = next;
    auto callback = s->on_change;
    if (!s->controlled) {
        s->raw_value = next;
        set_values(id, next);
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
    s->dragging_track = false;
    s->delete_preview = false;
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
            s->handles[*thumb]->hover = hover;
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
    if (s->disabled || (thumb && s->handle_disabled(*thumb)) ||
        (s->handles.empty() && !s->editable() && !s->dragging)) {
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
        const double midpoint = std::midpoint(s->limits.minimum, s->limits.maximum);
        if ((!thumb && !s->editable() && !s->nearest_enabled(midpoint)) || s->dragging ||
            e.button != input::PointerButton::primary || !event.capture_pointer()) {
            return;
        }
        const bool overlap = thumb && s->active < s->count() && !s->handle_disabled(s->active) &&
                             s->handles[*thumb]->center == s->handles[s->active]->center;
        if (!overlap) {
            s->active = thumb.value_or(s->active);
        }
        if (!thumb && !s->handles.empty()) {
            if (s->active >= s->count() || s->handle_disabled(s->active)) {
                s->active = *s->nearest_enabled(midpoint);
            }
            for (std::size_t i = 0; i < s->count(); ++i) {
                if (s->handle_disabled(i)) {
                    continue;
                }
                const float candidate = vertical ? s->handles[i]->center.y : s->handles[i]->center.x;
                const float active = vertical ? s->handles[s->active]->center.y : s->handles[s->active]->center.x;
                if (std::abs(pos - candidate) < std::abs(pos - active)) {
                    s->active = i;
                }
            }
        }
        s->key.reset();
        s->dragging = s->gesture = true;
        const float cross = vertical ? e.x - n.translation.x : e.y - n.translation.y;
        const float rail_cross =
            vertical ? s->rail_bounds.x + s->rail_bounds.width / 2 : s->rail_bounds.y + s->rail_bounds.height / 2;
        s->dragging_track = !thumb && s->included && !s->any_disabled() && s->options.draggable_track &&
                            s->count() > 1 && std::abs(cross - rail_cross) <= 4 &&
                            pos > std::min(vertical ? s->handles.front()->center.y : s->handles.front()->center.x,
                                           vertical ? s->handles.back()->center.y : s->handles.back()->center.x) &&
                            pos < std::max(vertical ? s->handles.front()->center.y : s->handles.front()->center.x,
                                           vertical ? s->handles.back()->center.y : s->handles.back()->center.x);
        s->drag_origin = s->value;
        s->drag_start = pos;
        s->candidate = s->value;
        s->pointer = e.pointer;
        s->capture = event.current_target();
        s->pointer_offset = thumb ? pos - (vertical ? s->handles[*thumb]->center.y : s->handles[*thumb]->center.x) : 0;
        if (s->active < s->count()) {
            static_cast<void>(
                services_->focus().request_focus(s->handles[s->active]->interaction, input::FocusModality::pointer));
        }
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
        if (s->dragging_track) {
            double delta = (pos - s->drag_start) / travel * (s->limits.maximum - s->limits.minimum);
            if (slider_inverted(s->orientation, s->reverse)) {
                delta = -delta;
            }
            const double first = normalize_slider_value(s->drag_origin.front() + delta, s->limits, s->marks, false);
            delta = std::clamp(first - s->drag_origin.front(), s->limits.minimum - s->drag_origin.front(),
                               s->limits.maximum - s->drag_origin.back());
            auto next = s->drag_origin;
            for (auto& value : next) {
                value = normalize_slider_value(value + delta, s->limits, s->marks, false);
            }
            change_values(id, std::move(next));
        } else {
            const float cross = vertical ? e.x - n.translation.x : e.y - n.translation.y;
            const float rail_cross =
                vertical ? s->rail_bounds.x + s->rail_bounds.width / 2 : s->rail_bounds.y + s->rail_bounds.height / 2;
            s->delete_preview =
                s->editable() && s->candidate.size() > s->options.min_count && std::abs(cross - rail_cross) > 130;
            double ratio = std::clamp((pos - s->pointer_offset - (vertical ? n.bounds.y : n.bounds.x) - inset) / travel,
                                      0.0F, 1.0F);
            if (slider_inverted(s->orientation, s->reverse)) {
                ratio = 1 - ratio;
            }
            if (!s->delete_preview) {
                const double value = std::lerp(s->limits.minimum, s->limits.maximum, ratio);
                if (event.kind() == input::PointerEventKind::down && !thumb) {
                    select_value(id, value);
                } else {
                    change(id, s->active, value);
                }
            }
        }
    }
    s = find(id);
    if (!s) {
        return;
    }
    if (event.kind() == input::PointerEventKind::up && e.button == input::PointerButton::primary) {
        static_cast<void>(event.release_pointer_capture());
        if (s->delete_preview) {
            delete_handle(id, s->active);
            return;
        }
        complete(id);
    } else {
        update(id, false);
    }
}

bool SliderComponentHost::keyboard(runtime::ComponentId id, std::size_t thumb, const input::KeyboardInputEvent& event) {
    using input::Key;
    using input::KeyAction;
    if (event.key == Key::delete_forward || event.key == Key::backspace) {
        if (event.action == KeyAction::down && !event.repeat && event.modifiers == input::KeyModifier::none) {
            if (auto* s = find(id);
                s && s->keyboard_enabled && !s->dragging && s->editable() && !s->handle_disabled(thumb)) {
                delete_handle(id, thumb);
            }
        }
        return true;
    }
    const bool command = event.key == Key::left || event.key == Key::right || event.key == Key::up ||
                         event.key == Key::down || event.key == Key::home || event.key == Key::end ||
                         event.key == Key::page_up || event.key == Key::page_down;
    if (!command) {
        return false;
    }
    auto* s = find(id);
    if (!s || s->handle_disabled(thumb) || !s->keyboard_enabled || s->dragging || thumb >= s->count()) {
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
    double value = current[thumb];
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
    value = std::clamp(value, thumb == 0 ? s->limits.minimum : current[thumb - 1],
                       thumb + 1 == current.size() ? s->limits.maximum : current[thumb + 1]);
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
        constexpr bool multiple = std::is_same_v<Value, SliderValues>;
        constexpr bool range = !std::is_same_v<Value, double>;
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
        Value raw = props.value_ ? read_prop(*props.value_) : props.default_value_.value_or(Value{});
        if constexpr (multiple) {
            if (!props.value_ && !props.default_value_) {
                raw = {limits.minimum, limits.minimum};
            }
        }
        const auto as_range = [](Value value) -> SliderValues {
            if constexpr (std::is_same_v<Value, SliderRange>) {
                return {value.lower, value.upper};
            } else if constexpr (multiple) {
                return value;
            } else {
                return {value};
            }
        };
        const auto initial = normalize_slider_values(as_range(raw), limits, marks, marks_only);
        auto options = multiple ? read_prop(props.range_options_) : SliderRangeOptions{};
        if constexpr (!multiple && range) {
            options.draggable_track = read_prop(props.draggable_track_);
        }
        validate_slider_range_options(options, marks_only, initial.size());
        auto& build = runtime::require_component_build_context();
        const auto id = build.mount_component<SliderState>();
        auto& s = build.state<SliderState>(id);
        s.component = id;
        s.node = build.root(id);
        s.range = range;
        s.multiple = multiple;
        s.options = options;
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
        s.disabled_handles = read_prop(props.handle_disabled_);
        if (s.disabled_handles.size() > 64) {
            throw std::invalid_argument("Slider disabled handle list exceeds 64");
        }
        s.keyboard_enabled = read_prop(props.keyboard_);
        const auto callback = [](const std::function<void(Value)>& fn) -> std::function<void(SliderValues)> {
            if (!fn) {
                return {};
            }
            return [fn](SliderValues value) {
                if constexpr (std::is_same_v<Value, SliderRange>) {
                    fn({value.front(), value.back()});
                } else if constexpr (multiple) {
                    fn(std::move(value));
                } else {
                    fn(value.front());
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
                             for (std::size_t i = 0; i < s.value.size(); ++i) {
                                 s.handles.push_back(std::make_unique<SliderThumb>());
                                 s.handles.back()->disabled = s.handle_disabled(i);
                                 host.mount_thumb(id, *s.handles.back(), nested);
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
                        static_cast<void>(
                            engine.measure_child(state.handles[i]->wrapper, layout::Constraints::fixed(24, 24)));
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
        if constexpr (multiple) {
            connect_prop(scope, props.range_options_, [&host, id](SliderRangeOptions options) {
                auto* state = host.find(id);
                if (!state) {
                    return;
                }
                validate_slider_range_options(options, state->marks_only, state->value.size());
                if (state->options == options) {
                    return;
                }
                host.cancel(id);
                if (auto* current = host.find(id)) {
                    current->options = options;
                }
            });
        } else if constexpr (range) {
            connect_prop(scope, props.draggable_track_, [&host, id](bool value) {
                auto* state = host.find(id);
                if (!state) {
                    return;
                }
                auto options = state->options;
                options.draggable_track = value;
                validate_slider_range_options(options, state->marks_only, state->value.size());
                if (state->options == options) {
                    return;
                }
                host.cancel(id);
                if (auto* current = host.find(id)) {
                    current->options = options;
                }
            });
        }
        if (props.value_) {
            connect_prop(scope, *props.value_, [&host, id, as_range](Value value) {
                auto* state = host.find(id);
                if (!state) {
                    return;
                }
                const auto next =
                    normalize_slider_values(as_range(value), state->limits, state->marks, state->marks_only);
                validate_slider_range_options(state->options, state->marks_only, next.size());
                if (next.size() != state->value.size() &&
                    !(state->gesture && next == state->candidate &&
                      (state->capture == state->rail || state->editing_command))) {
                    host.cancel(id);
                    state = host.find(id);
                    if (!state) {
                        return;
                    }
                }
                state->raw_value = as_range(value);
                host.set_values(id, next);
                state = host.find(id);
                if (!state) {
                    return;
                }
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
            const auto next = normalize_slider_values(state->raw_value, limits, state->marks, state->marks_only);
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
            const auto next = normalize_slider_values(state->raw_value, state->limits, marks, state->marks_only);
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
            validate_slider_range_options(state->options, value, state->value.size());
            auto points = slider_visual_points(state->limits, state->marks, value, state->dots);
            const auto next = normalize_slider_values(state->raw_value, state->limits, state->marks, value);
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
            host.synchronize_disabled(id);
            state = host.find(id);
            if (!state) {
                return;
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
        connect_prop(scope, props.handle_disabled_, [&host, id](SliderDisabledHandles value) {
            if (value.size() > 64) {
                throw std::invalid_argument("Slider disabled handle list exceeds 64");
            }
            auto* state = host.find(id);
            if (!state || state->disabled_handles == value) {
                return;
            }
            host.cancel(id);
            if (auto* current = host.find(id)) {
                current->disabled_handles = std::move(value);
                host.synchronize_disabled(id);
                host.update(id, false);
            }
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
        std::vector<input::InteractionId> thumbs;
        for (const auto& thumb : s.handles) {
            thumbs.push_back(thumb->interaction);
        }
        host.mounted_.push_back({id, s.node, std::move(thumbs), s.surface, range});
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

void MultiSlider(MultiSliderProps props) {
    detail::SliderPropsAccess::mount(props);
}
} // namespace ryn
