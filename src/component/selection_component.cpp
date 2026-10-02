#include "component/selection_component.hpp"

#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <ryn/text.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ryn::detail {

struct SwitchRefState final {
    std::thread::id owner{std::this_thread::get_id()};
    bool binding{};
    std::function<bool()> focus;
    std::function<bool()> blur;

    void ensure_owner() const {
        if (owner != std::this_thread::get_id()) {
            throw std::logic_error("SwitchRef requires its owner thread");
        }
    }
};

struct CheckboxRefState final {
    std::thread::id owner{std::this_thread::get_id()};
    bool binding{};
    std::function<bool()> focus;
    std::function<bool()> blur;

    void ensure_owner() const {
        if (owner != std::this_thread::get_id()) {
            throw std::logic_error("CheckboxRef requires its owner thread");
        }
    }
};

namespace {
thread_local SelectionComponentHost* active_selection_host{};
constexpr std::size_t switch_loading_segments = 8;
constexpr std::size_t switch_layer_count = 2 + switch_loading_segments;
constexpr std::size_t checkbox_check_segments = 12;
constexpr std::size_t checkbox_indeterminate_layer = 2 + checkbox_check_segments;
constexpr std::size_t checkbox_layer_count = checkbox_indeterminate_layer + 1;
constexpr std::size_t radio_layer_count = 3;

std::array<float, 4> channels(Color color) noexcept {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

void set_geometry(graphics::QuadInstance& quad, runtime::Rect rect, runtime::Size viewport, float radius,
                  runtime::Point translation) {
    if (viewport.width <= 0.0F || viewport.height <= 0.0F) {
        return;
    }
    quad.bounds = {
        rect.x,
        rect.y,
        rect.width,
        rect.height,
    };
    quad.corner_radius = radius;
    quad.translation = {
        translation.x,
        translation.y,
    };
}

void set_material(graphics::QuadInstance& quad, Color color, float opacity = 1.0F) {
    quad.color = channels(color);
    quad.opacity = opacity;
}

void validate(SwitchSize size) {
    if (size != SwitchSize::Middle && size != SwitchSize::Small) {
        throw std::invalid_argument("Switch size must be Middle or Small");
    }
}

void validate(SwitchDirection direction) {
    if (direction != SwitchDirection::LeftToRight && direction != SwitchDirection::RightToLeft) {
        throw std::invalid_argument("Switch direction is invalid");
    }
}

SwitchDirection selection_direction(CheckboxDirection direction) {
    if (direction != CheckboxDirection::LeftToRight && direction != CheckboxDirection::RightToLeft) {
        throw std::invalid_argument("Checkbox direction is invalid");
    }
    return direction == CheckboxDirection::RightToLeft ? SwitchDirection::RightToLeft : SwitchDirection::LeftToRight;
}

void translate_content(runtime::NodeStore& nodes, runtime::NodeId id, runtime::Point delta) {
    auto& node = nodes.require(id);
    node.translation.x += delta.x;
    node.translation.y += delta.y;
    for (const auto child : node.children) {
        translate_content(nodes, child, delta);
    }
}

struct SelectionTokens {
    float switch_height{};
    float switch_width{};
    float handle_size{};
    float track_padding{};
    float inner_min_margin{};
    float inner_max_margin{};
    float checkbox_size{};
    float indicator_size{};
    float radio_size{};
    float radio_dot_size{};
    float line_width{};
    float label_gap{};
    Color on;
    Color on_hover;
    Color on_active;
    Color off;
    Color off_hover;
    Color off_active;
    Color handle;
    Color checkmark;
    Color box_background;
    Color box_border;
    Color disabled_background;
    Color disabled_foreground;
    Color focus;
    Color radio_dot;
};

SelectionTokens resolve_tokens(const ThemeSnapshot& theme, SwitchSize size) {
    const auto& map = theme.map();
    const auto& alias = theme.alias();
    const auto& switch_token = theme.switch_token();
    return {
        size == SwitchSize::Small ? switch_token.track_height_small : switch_token.track_height,
        size == SwitchSize::Small ? switch_token.track_min_width_small : switch_token.track_min_width,
        size == SwitchSize::Small ? switch_token.handle_size_small : switch_token.handle_size,
        switch_token.track_padding,
        size == SwitchSize::Small ? switch_token.inner_min_margin_small : switch_token.inner_min_margin,
        size == SwitchSize::Small ? switch_token.inner_max_margin_small : switch_token.inner_max_margin,
        map.control_height / 2.0F,
        map.font_size_large / 2.0F,
        map.font_size_large,
        std::max(0.0F, map.font_size_large - 2.0F * (4.0F + theme.seed().line_width)),
        theme.seed().line_width,
        map.size_xs,
        map.color_primary,
        map.color_primary_hover,
        map.color_primary_active,
        // Ant 6.6.5 Switch track uses colorTextQuaternary/Tertiary.
        Color(map.color_text_base.red(), map.color_text_base.green(), map.color_text_base.blue(), 0.25F),
        Color(map.color_text_base.red(), map.color_text_base.green(), map.color_text_base.blue(), 0.45F),
        Color(map.color_text_base.red(), map.color_text_base.green(), map.color_text_base.blue(), 0.45F),
        switch_token.handle_background,
        Color::rgba8(255, 255, 255),
        alias.color_background_container,
        alias.color_border,
        alias.color_background_container_disabled,
        alias.color_text_disabled,
        alias.color_focus_outline,
        Color::rgba8(255, 255, 255),
    };
}
} // namespace

struct SelectionState final {
    runtime::ComponentId component;
    runtime::NodeId node;
    runtime::NodeId spacer;
    input::InteractionId interaction;
    runtime::SceneFragmentId fragment;
    component::RetainedSurfaceId surface;
    std::vector<graphics::QuadInstance> visuals;
    component::RetainedSurfaceEffects effects;
    bool checkbox{};
    bool radio{};
    bool own_disabled{};
    runtime::ComponentId group;
    std::optional<String> value;
    std::optional<CheckboxValue> checkbox_value;
    bool controlled{};
    bool checked{};
    bool indeterminate{};
    bool disabled{};
    bool loading{};
    bool hovered{};
    SwitchSize size{SwitchSize::Middle};
    float presented_checked{};
    animation::AnimationScopeId animation_scope;
    animation::AnimationTargetId handle_target;
    animation::AnimationId handle_animation;
    animation::AnimationTargetId spinner_target;
    animation::AnimationId spinner_animation;
    float spinner_phase{};
    float layout_width{-1.0F};
    float layout_height{-1.0F};
    float layout_gap{-1.0F};
    float layout_inner_min{-1};
    float layout_inner_max{-1};
    runtime::ComponentId checked_content;
    runtime::ComponentId unchecked_content;
    SwitchDirection direction{SwitchDirection::LeftToRight};
    std::shared_ptr<SwitchRefState> ref;
    std::shared_ptr<CheckboxRefState> checkbox_ref;
    bool own_direction{};
    bool wave{true};
    bool wave_active{};
    float wave_progress{1};
    Color wave_color;
    animation::AnimationTargetId wave_target;
    animation::AnimationId wave_animation;
    runtime::SceneFragmentId wave_fragment;
    component::RetainedSurfaceId wave_range;
    input::PressableBehavior press;
    input::FocusPresentation focus;
    std::function<void(bool)> on_change;
    std::function<void(bool)> on_click;
    Signal<runtime::SemanticForeground> label_foreground{{0.0F, 0.0F, 0.0F, 1.0F}};
    Signal<runtime::SemanticTypography> label_typography{runtime::SemanticTypography{}};
    theme_runtime::Subscription theme_subscription;
};

struct RadioGroupState final {
    runtime::ComponentId component;
    runtime::NodeId node;
    std::vector<runtime::ComponentId> options;
    std::optional<String> value;
    bool controlled{};
    bool disabled{};
    RadioGroupOrientation orientation{RadioGroupOrientation::Horizontal};
    float layout_gap{-1.0F};
    RadioGroupOrientation layout_orientation{RadioGroupOrientation::Vertical};
    std::function<void(const String&)> on_change;
    theme_runtime::Subscription theme_subscription;
};

struct GeneratedCheckboxOption final {
    CheckboxValue value;
    Signal<String> label;
    Signal<bool> disabled;
    runtime::ComponentId component;

    explicit GeneratedCheckboxOption(const CheckboxOption& option)
        : value(option.value), label(option.label), disabled(option.disabled) {}
};

struct CheckboxGroupState final {
    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId anchor;
    runtime::SceneFragmentId fragment;
    std::vector<runtime::ComponentId> options;
    std::vector<std::unique_ptr<GeneratedCheckboxOption>> generated;
    std::vector<CheckboxOption> source_options;
    CheckboxValues value;
    bool controlled{};
    bool disabled{};
    bool reconciling{};
    CheckboxGroupOrientation orientation{CheckboxGroupOrientation::Horizontal};
    CheckboxDirection direction{CheckboxDirection::LeftToRight};
    std::optional<CheckboxGroupOrientation> layout_orientation;
    float layout_gap{-1};
    std::function<void(const CheckboxValues&)> on_change;
    theme_runtime::Subscription theme_subscription;
};

namespace {
void validate_checkbox_value(const CheckboxValue& value) {
    if (const auto* number = std::get_if<double>(&value); number && !std::isfinite(*number)) {
        throw std::invalid_argument("Checkbox numeric value must be finite");
    }
}

void validate_checkbox_values(const CheckboxValues& values) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        validate_checkbox_value(values[i]);
        if (std::find(values.begin(), values.begin() + i, values[i]) != values.begin() + i) {
            throw std::invalid_argument("Checkbox selected values must be unique");
        }
    }
}

void validate_checkbox_options(const std::vector<CheckboxOption>& options) {
    if (options.size() > 1024) {
        throw std::invalid_argument("CheckboxGroup supports at most 1024 options");
    }
    CheckboxValues values;
    for (const auto& option : options) {
        values.push_back(option.value);
    }
    validate_checkbox_values(values);
}
} // namespace

template <class Label>
void mount_selection_label(runtime::ComponentBuildContext& build, WindowComponentServices& services,
                           SelectionState& state, runtime::ComponentId component, const std::optional<Label>& label,
                           float size) {
    build.mount_slot(component, Content{[&] {
                         auto& nested = runtime::require_component_build_context();
                         const auto spacer = nested.mount_component<int>(0);
                         const auto node = nested.root(spacer);
                         state.spacer = node;
                         services.layout().set_layout(node, layout::LeafLayout{{size, size}});
                         nested.on_resource_cleanup(spacer, [layout = &services.layout(), node] {
                             static_cast<void>(layout->remove_layout(node));
                         });
                         if (label) {
                             nested.mount_slot_with_semantic_text_style(
                                 component, *label, Prop<runtime::SemanticForeground>{state.label_foreground},
                                 Prop<runtime::SemanticTypography>{state.label_typography});
                         }
                     }});
}

SelectionComponentHost::SelectionComponentHost(WindowComponentServices& services) : services_(&services) {
    services_->attach(*this);
}

SelectionComponentHost::~SelectionComponentHost() {
    while (!checkbox_groups_.empty()) {
        if (!services_->destroy(checkbox_groups_.back().component)) {
            checkbox_groups_.pop_back();
        }
    }
    while (!groups_.empty()) {
        const auto id = groups_.back();
        if (!services_->destroy(id)) {
            groups_.pop_back();
        }
    }
    while (!mounted_.empty()) {
        const auto id = mounted_.back().component;
        if (!services_->destroy(id)) {
            mounted_.pop_back();
        }
    }
    services_->detach(*this);
}

void SelectionComponentHost::mount(const Content& content) {
    services_->mount(content);
}

void* SelectionComponentHost::begin_mount() noexcept {
    return std::exchange(active_selection_host, this);
}

void SelectionComponentHost::end_mount(void* previous) noexcept {
    active_selection_host = static_cast<SelectionComponentHost*>(previous);
}

void SelectionComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](const auto& item) { return !services_->components().contains(item.component); });
    std::erase_if(groups_, [this](const auto id) { return !services_->components().contains(id); });
    std::erase_if(checkbox_groups_,
                  [this](const auto& item) { return !services_->components().contains(item.component); });
}

void SelectionComponentHost::on_dispose() noexcept {
    mounted_.clear();
    groups_.clear();
    checkbox_groups_.clear();
}

void SelectionComponentHost::synchronize_auxiliary_motion() {
    for (const auto& item : mounted_) {
        auto* state = find(item.component);
        if (!state || state->radio) {
            continue;
        }
        if (!state->checkbox) {
            synchronize_spinner(*state);
        }
        if (state->wave_active &&
            !animation::resolve_motion_policy(services_->components().theme_scope(item.component)->snapshot(),
                                              services_->motion_preference())
                 .enabled()) {
            stop_wave(*state);
        }
        const auto& theme = services_->components().theme_scope(item.component)->snapshot();
        if (state->handle_animation.valid() &&
            !animation::resolve_motion_policy(theme, services_->motion_preference()).enabled()) {
            static_cast<void>(services_->animations().finish(state->handle_animation));
            state->handle_animation = {};
            state->presented_checked = state->checked ? 1.0F : 0.0F;
            if (viewport_.width > 0.0F && viewport_.height > 0.0F) {
                update_geometry(*state, viewport_);
            }
        }
    }
}

SelectionState* SelectionComponentHost::find(runtime::ComponentId id) noexcept {
    return services_->components().state<SelectionState>(id);
}

const SelectionState* SelectionComponentHost::find(runtime::ComponentId id) const noexcept {
    return services_->components().state<SelectionState>(id);
}

SelectionSnapshot SelectionComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* state = find(id);
    if (!state) {
        throw std::out_of_range("Selection component is stale or invalid");
    }
    return {state->checkbox,    state->checked,         state->indeterminate, state->disabled, state->loading,
            state->hovered,     state->press.pressed(), state->focus,         state->size,     state->radio,
            state->wave_active, state->wave_progress,   state->wave_range};
}

std::optional<input::InteractionId> SelectionComponentHost::parent_interaction(runtime::ComponentId component) const {
    for (auto ancestor = services_->components().parent(component); ancestor;
         ancestor = services_->components().parent(*ancestor)) {
        for (const auto interaction : services_->interactions().declaration_order()) {
            const auto* record = services_->interactions().find(interaction);
            if (record && record->component == *ancestor) {
                return interaction;
            }
        }
    }
    return {};
}

void SelectionComponentHost::attach_interaction(SelectionState& state) {
    const auto id = state.component;
    state.interaction =
        services_->interactions().create({id, state.node, parent_interaction(id), !state.disabled, true, {}});
    input::InteractionHandlers pointer_handlers;
    pointer_handlers.target = [this, id](input::PointerDispatchContext& event) {
        handle_pointer(id, event);
    };
    static_cast<void>(services_->interactions().set_handlers(state.interaction, std::move(pointer_handlers)));
    input::FocusHandlers focus_handlers;
    focus_handlers.state_changed = [this, id](input::FocusPresentation focus) {
        apply_focus(id, focus);
    };
    focus_handlers.activation_allowed = [this, id] {
        return activation_allowed(id);
    };
    focus_handlers.activate = [this, id] {
        activate(id);
    };
    // Selection controls use Space only; Enter is a Button action.
    focus_handlers.text_edit = [](const input::KeyboardInputEvent& event) {
        return event.key == input::Key::enter;
    };
    static_cast<void>(services_->interactions().set_focus_handlers(state.interaction, std::move(focus_handlers)));
}

void SelectionComponentHost::release_selection(SelectionState& state) {
    if (state.checkbox && state.group.valid() && state.checkbox_value) {
        if (auto* group = services_->components().state<CheckboxGroupState>(state.group);
            group && !group->reconciling) {
            std::erase(group->options, state.component);
            if (!group->controlled) {
                std::erase(group->value, *state.checkbox_value);
            }
        }
    }
    if (state.ref) {
        state.ref->binding = false;
        state.ref->focus = {};
        state.ref->blur = {};
    }
    if (state.checkbox_ref) {
        state.checkbox_ref->binding = false;
        state.checkbox_ref->focus = {};
        state.checkbox_ref->blur = {};
    }
    services_->pointer().cancel_interaction(state.interaction);
    if (state.animation_scope.valid()) {
        static_cast<void>(services_->animations().dispose_scope(state.animation_scope));
    }
    if (state.wave_range.valid()) {
        static_cast<void>(services_->surfaces().destroy_content_range(state.wave_range));
    }
    services_->focus().cancel_interaction(state.interaction);
    static_cast<void>(services_->interactions().remove(state.interaction));
    static_cast<void>(services_->surfaces().destroy(state.surface));
    static_cast<void>(services_->layout().remove_layout(state.node));
}

bool SelectionComponentHost::activation_allowed(runtime::ComponentId id) const noexcept {
    const auto* state = find(id);
    return state && !state->disabled && !state->loading;
}

void SelectionComponentHost::activate(runtime::ComponentId id) {
    auto* state = find(id);
    if (!state || state->disabled || state->loading) {
        return;
    }
    if (state->radio && state->checked) {
        return;
    }
    if (state->radio && state->group.valid() && state->value) {
        const String value = *state->value;
        select_group_option(state->group, value);
        return;
    }
    if (state->checkbox && state->group.valid() && state->checkbox_value) {
        const auto group_id = state->group;
        const auto* group = services_->components().state<CheckboxGroupState>(group_id);
        if (!group || group->disabled) {
            return;
        }
        const bool next = !state->checked;
        auto selected = group->value;
        if (next) {
            selected.push_back(*state->checkbox_value);
        } else {
            std::erase(selected, *state->checkbox_value);
        }
        CheckboxValues candidate;
        for (const auto option : group->options) {
            const auto* item = find(option);
            if (item && item->checkbox_value && std::ranges::find(selected, *item->checkbox_value) != selected.end()) {
                candidate.push_back(*item->checkbox_value);
            }
        }
        const auto own_callback = state->on_change;
        const auto group_callback = group->on_change;
        const auto click = state->on_click;
        start_wave(*state);
        if (!group->controlled) {
            apply_checkbox_group_value(group_id, candidate);
        }
        if (own_callback) {
            own_callback(next);
        }
        if (group_callback) {
            group_callback(candidate);
        }
        if (click) {
            click(next);
        }
        return;
    }
    const bool next = state->radio ? true : !state->checked;
    auto callback = state->on_change;
    auto click = state->on_click;
    if (!state->controlled) {
        state->checked = next;
        if (!state->checkbox && !state->radio) {
            retarget_handle(*state);
        }
        update_visuals(*state);
    }
    if (!state->radio) {
        start_wave(*state);
    }
    if (callback) {
        callback(next);
    }
    if (click) {
        click(next);
    }
}

void SelectionComponentHost::handle_pointer(runtime::ComponentId id, input::PointerDispatchContext& event) {
    auto* state = find(id);
    if (!state) {
        return;
    }
    if (event.kind() == input::PointerEventKind::enter) {
        if (!state->disabled && !state->loading && !state->hovered) {
            state->hovered = true;
            update_visuals(*state);
        }
        return;
    }
    if (event.kind() == input::PointerEventKind::leave) {
        if (state->hovered) {
            state->hovered = false;
            update_visuals(*state);
        }
        return;
    }
    const auto result = state->press.dispatch(event, state->interaction, activation_allowed(id));
    state = find(id);
    if (!state) {
        return;
    }
    if (result.pressed_changed) {
        update_visuals(*state);
    }
    if (result.activate) {
        activate(id);
    }
}

void SelectionComponentHost::apply_checked(runtime::ComponentId id, bool value) {
    if (auto* state = find(id); state && state->checked != value) {
        state->checked = value;
        if (!state->checkbox && !state->radio) {
            retarget_handle(*state);
        }
        update_visuals(*state);
    }
}

void SelectionComponentHost::apply_radio_own_disabled(runtime::ComponentId id, bool value) {
    auto* state = find(id);
    if (!state || !state->radio) {
        return;
    }
    state->own_disabled = value;
    const auto* group = state->group.valid() ? services_->components().state<RadioGroupState>(state->group) : nullptr;
    apply_disabled(id, value || (group && group->disabled));
}

runtime::ComponentId SelectionComponentHost::parent_checkbox_group(runtime::ComponentId id) const {
    for (auto parent = services_->components().parent(id); parent; parent = services_->components().parent(*parent)) {
        if (services_->components().state<CheckboxGroupState>(*parent)) {
            return *parent;
        }
    }
    return {};
}

CheckboxValues SelectionComponentHost::checkbox_group_value(runtime::ComponentId id) const {
    const auto* group = services_->components().state<CheckboxGroupState>(id);
    if (!group) {
        throw std::out_of_range("CheckboxGroup is stale or invalid");
    }
    return group->value;
}

void SelectionComponentHost::apply_checkbox_own_disabled(runtime::ComponentId id, bool value) {
    auto* state = find(id);
    if (!state || !state->checkbox) {
        return;
    }
    state->own_disabled = value;
    const auto* group = services_->components().state<CheckboxGroupState>(state->group);
    apply_disabled(id, value || (group && group->disabled));
}

void SelectionComponentHost::apply_checkbox_group_value(runtime::ComponentId id, CheckboxValues value) {
    validate_checkbox_values(value);
    auto* group = services_->components().state<CheckboxGroupState>(id);
    if (!group || group->value == value) {
        return;
    }
    group->value = std::move(value);
    const auto options = group->options;
    for (const auto option : options) {
        const auto* state = find(option);
        if (state && state->checkbox_value) {
            apply_checked(option, std::ranges::find(group->value, *state->checkbox_value) != group->value.end());
        }
    }
}

void SelectionComponentHost::apply_checkbox_group_disabled(runtime::ComponentId id, bool value) {
    auto* group = services_->components().state<CheckboxGroupState>(id);
    if (!group || group->disabled == value) {
        return;
    }
    group->disabled = value;
    const auto options = group->options;
    for (const auto option : options) {
        if (const auto* state = find(option)) {
            apply_disabled(option, state->own_disabled || value);
        }
    }
}

void SelectionComponentHost::apply_checkbox_group_orientation(runtime::ComponentId id, CheckboxGroupOrientation value) {
    if (value != CheckboxGroupOrientation::Horizontal && value != CheckboxGroupOrientation::Vertical) {
        throw std::invalid_argument("CheckboxGroup orientation is invalid");
    }
    if (auto* group = services_->components().state<CheckboxGroupState>(id); group && group->orientation != value) {
        group->orientation = value;
        update_checkbox_group_layout(*group);
    }
}

void SelectionComponentHost::update_checkbox_group_layout(CheckboxGroupState& group) {
    const float gap = services_->components().theme_scope(group.component)->snapshot().map().size_xs;
    if (group.layout_gap == gap && group.layout_orientation == group.orientation) {
        return;
    }
    layout::FlexLayout model;
    model.direction = group.orientation == CheckboxGroupOrientation::Vertical ? layout::FlexDirection::vertical
                                                                              : layout::FlexDirection::horizontal;
    model.main_gap = gap;
    model.cross_gap = gap;
    model.wrap =
        group.orientation == CheckboxGroupOrientation::Horizontal ? layout::FlexWrap::wrap : layout::FlexWrap::no_wrap;
    model.align = layout::FlexAlign::center;
    services_->layout().set_layout(group.node, model);
    group.layout_gap = gap;
    group.layout_orientation = group.orientation;
    services_->dirty().invalidate(group.node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                  runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
}

void SelectionComponentHost::apply_checkbox_group_direction(runtime::ComponentId id, CheckboxDirection value) {
    const auto direction = selection_direction(value);
    if (auto* group = services_->components().state<CheckboxGroupState>(id); group && group->direction != value) {
        group->direction = value;
        for (const auto option : group->options) {
            if (const auto* state = find(option); state && !state->own_direction) {
                apply_direction(option, direction);
            }
        }
    }
}

void SelectionComponentHost::apply_group_value(runtime::ComponentId id, std::optional<String> value) {
    auto* group = services_->components().state<RadioGroupState>(id);
    if (!group || group->value == value) {
        return;
    }
    group->value = std::move(value);
    const auto options = group->options;
    for (const auto option : options) {
        const auto* state = find(option);
        if (state && state->value) {
            apply_checked(option, group->value && *state->value == *group->value);
        }
    }
}

void SelectionComponentHost::apply_group_disabled(runtime::ComponentId id, bool value) {
    auto* group = services_->components().state<RadioGroupState>(id);
    if (!group || group->disabled == value) {
        return;
    }
    group->disabled = value;
    const auto options = group->options;
    for (const auto option : options) {
        if (const auto* state = find(option)) {
            apply_disabled(option, state->own_disabled || value);
        }
    }
}

void SelectionComponentHost::apply_group_orientation(runtime::ComponentId id, RadioGroupOrientation value) {
    if (value != RadioGroupOrientation::Horizontal && value != RadioGroupOrientation::Vertical) {
        throw std::invalid_argument("RadioGroup orientation is invalid");
    }
    auto* group = services_->components().state<RadioGroupState>(id);
    if (!group || group->orientation == value) {
        return;
    }
    group->orientation = value;
    update_group_layout(*group);
}

void SelectionComponentHost::select_group_option(runtime::ComponentId id, const String& value) {
    auto* group = services_->components().state<RadioGroupState>(id);
    if (!group || group->disabled || group->value == value) {
        return;
    }
    const auto callback = group->on_change;
    const bool controlled = group->controlled;
    if (!controlled) {
        apply_group_value(id, value);
    }
    if (callback) {
        callback(value);
    }
}

void SelectionComponentHost::update_group_layout(RadioGroupState& group) {
    const auto& theme = services_->components().theme_scope(group.component)->snapshot();
    const float gap = theme.map().size_xs;
    if (group.layout_gap == gap && group.layout_orientation == group.orientation) {
        return;
    }
    layout::FlexLayout model;
    model.direction = group.orientation == RadioGroupOrientation::Vertical ? layout::FlexDirection::vertical
                                                                           : layout::FlexDirection::horizontal;
    model.main_gap = gap;
    model.align = layout::FlexAlign::center;
    services_->layout().set_layout(group.node, model);
    group.layout_gap = gap;
    group.layout_orientation = group.orientation;
    services_->dirty().invalidate(group.node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                  runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
}

void SelectionComponentHost::retarget_handle(SelectionState& state) {
    const auto& theme = services_->components().theme_scope(state.component)->snapshot();
    const auto policy = animation::resolve_motion_policy(theme, services_->motion_preference());
    const float target = state.checked ? 1.0F : 0.0F;
    if (!policy.enabled() || !state.handle_target.valid()) {
        if (state.handle_animation.valid()) {
            static_cast<void>(services_->animations().finish(state.handle_animation));
            state.handle_animation = {};
        }
        state.presented_checked = target;
        return;
    }
    const auto spec = policy.transition(animation::MotionDurationToken::mid, animation::MotionEasingToken::ease_in_out);
    if (state.handle_animation.valid() && services_->animations().contains(state.handle_animation) &&
        services_->animations().retarget(state.handle_animation, target, spec, services_->animation_time())) {
        return;
    }
    state.handle_animation = services_->animations().play(state.handle_target, state.presented_checked, target, spec,
                                                          services_->animation_time());
}

void SelectionComponentHost::synchronize_spinner(SelectionState& state) {
    const auto& theme = services_->components().theme_scope(state.component)->snapshot();
    const bool animate =
        state.loading && animation::resolve_motion_policy(theme, services_->motion_preference()).enabled();
    if (!animate) {
        if (services_->animations().contains(state.spinner_animation)) {
            static_cast<void>(services_->animations().cancel(state.spinner_animation, services_->animation_time()));
        }
        state.spinner_animation = {};
        state.spinner_phase = 0.0F;
        return;
    }
    if (services_->animations().contains(state.spinner_animation)) {
        return;
    }
    state.spinner_phase -= std::floor(state.spinner_phase);
    state.spinner_animation = services_->animations().play(
        state.spinner_target, state.spinner_phase, state.spinner_phase + 1.0F,
        {{}, animation::AnimationDuration::microseconds(800'000), animation::Easing::linear()},
        services_->animation_time());
}

void SelectionComponentHost::apply(animation::AnimationId, animation::AnimationTargetId target,
                                   const animation::AnimationValue& value, animation::AnimationDirtyDomain) {
    for (const auto& item : mounted_) {
        auto* state = find(item.component);
        if (!state) {
            continue;
        }
        if (state->spinner_target == target) {
            state->spinner_phase = std::get<float>(value);
            update_visuals(*state);
            return;
        }
        if (state->wave_target == target) {
            state->wave_progress = std::clamp(std::get<float>(value), 0.0F, 1.0F);
            publish_wave(*state);
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Animation);
            return;
        }
        if (state->handle_target != target) {
            continue;
        }
        state->presented_checked = std::get<float>(value);
        if (viewport_.width > 0.0F && viewport_.height > 0.0F) {
            update_geometry(*state, viewport_);
        }
        services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        return;
    }
}

void SelectionComponentHost::completed(animation::AnimationId animation, animation::AnimationTargetId target) {
    for (const auto& item : mounted_) {
        auto* state = find(item.component);
        if (!state) {
            continue;
        }
        if (state->spinner_target == target && state->spinner_animation == animation) {
            state->spinner_animation = {};
            synchronize_spinner(*state);
            return;
        }
        if (state->wave_target == target && state->wave_animation == animation) {
            state->wave_animation = {};
            state->wave_active = false;
            state->wave_progress = 1;
            publish_wave(*state);
            return;
        }
        if (state->handle_target == target && state->handle_animation == animation) {
            state->handle_animation = {};
            return;
        }
    }
}

void SelectionComponentHost::apply_disabled(runtime::ComponentId id, bool value) {
    auto* state = find(id);
    if (!state || state->disabled == value) {
        return;
    }
    state->disabled = value;
    if (value) {
        state->hovered = false;
        static_cast<void>(state->press.reset());
        services_->pointer().cancel_interaction(state->interaction);
        state = find(id);
        if (!state) {
            return;
        }
    }
    static_cast<void>(services_->interactions().set_eligible(state->interaction, !value));
    services_->focus().synchronize();
    update_visuals(*state);
}

void SelectionComponentHost::apply_loading(runtime::ComponentId id, bool value) {
    auto* state = find(id);
    if (!state || state->loading == value) {
        return;
    }
    state->loading = value;
    if (value) {
        state->hovered = false;
        static_cast<void>(state->press.reset());
        services_->pointer().cancel_pointer_interaction(state->interaction);
        state = find(id);
        if (!state) {
            return;
        }
    }
    services_->focus().synchronize();
    state = find(id);
    if (!state) {
        return;
    }
    synchronize_spinner(*state);
    update_visuals(*state);
}

void SelectionComponentHost::apply_indeterminate(runtime::ComponentId id, bool value) {
    if (auto* state = find(id); state && state->indeterminate != value) {
        state->indeterminate = value;
        update_visuals(*state);
    }
}

void SelectionComponentHost::apply_size(runtime::ComponentId id, SwitchSize value) {
    validate(value);
    if (auto* state = find(id); state && state->size != value) {
        state->size = value;
        update_layout(*state);
        update_visuals(*state);
    }
}

void SelectionComponentHost::apply_direction(runtime::ComponentId id, SwitchDirection value) {
    validate(value);
    if (auto* state = find(id); state && state->direction != value) {
        state->direction = value;
        if (state->checkbox) {
            if (state->spacer.valid()) {
                services_->nodes().require(state->spacer).external_layout.order =
                    value == SwitchDirection::RightToLeft ? 1 : 0;
            }
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry |
                                                           runtime::DirtyFlags::HitTest);
        } else {
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    }
}

void SelectionComponentHost::apply_wave(runtime::ComponentId id, bool value) {
    if (auto* state = find(id); state && state->wave != value) {
        state->wave = value;
        if (!value) {
            stop_wave(*state);
        }
    }
}

void SelectionComponentHost::start_wave(SelectionState& state) {
    const auto& theme = services_->components().theme_scope(state.component)->snapshot();
    const float width = state.checkbox ? theme.checkbox().wave_width : theme.switch_token().wave_width;
    const float opacity = state.checkbox ? theme.checkbox().wave_opacity : theme.switch_token().wave_opacity;
    const auto policy = animation::resolve_motion_policy(theme, services_->motion_preference());
    const auto spec = policy.transition(animation::MotionDurationToken::slow, animation::MotionEasingToken::ease_out);
    if (!state.wave || state.disabled || state.loading || !services_->focus().state().window_active ||
        !policy.enabled() || width <= 0 || opacity <= 0 || spec.duration.count_microseconds() == 0) {
        return;
    }
    stop_wave(state);
    state.wave_color = state.checkbox  ? theme.checkbox().primary
                       : state.checked ? theme.switch_token().checked_background
                                       : theme.switch_token().unchecked_background;
    state.wave_active = true;
    state.wave_progress = 0;
    state.wave_animation =
        services_->animations().play(state.wave_target, 0.0F, 1.0F, spec, services_->animation_time());
    publish_wave(state);
    services_->dirty().invalidate(state.node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Animation);
}

void SelectionComponentHost::stop_wave(SelectionState& state) {
    if (services_->animations().contains(state.wave_animation)) {
        static_cast<void>(services_->animations().cancel(state.wave_animation, services_->animation_time()));
    }
    state.wave_animation = {};
    const bool active = state.wave_active;
    state.wave_active = false;
    state.wave_progress = 1;
    publish_wave(state);
    if (active) {
        services_->dirty().invalidate(state.node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Animation);
    }
}

void SelectionComponentHost::publish_wave(SelectionState& state) {
    if (!state.wave_active || state.wave_progress >= 1) {
        if (state.wave_range.valid()) {
            services_->surfaces().update_content_effects(state.wave_range, {});
        }
        return;
    }
    if (!state.wave_range.valid()) {
        state.wave_fragment = services_->components().register_scene_fragment(
            state.component, runtime::SceneFragmentPlacement::before_children);
        state.wave_range = services_->surfaces().create_content_range(state.wave_fragment, {});
    }
    const auto& node = services_->nodes().require(state.node);
    const auto& theme = services_->components().theme_scope(state.component)->snapshot();
    const auto& checkbox = theme.checkbox();
    const auto& token = theme.switch_token();
    const auto shape =
        state.checkbox ? state.effects.shape
                       : graphics::LogicalRoundedRect{node.bounds, std::min(node.bounds.width, node.bounds.height) / 2};
    const auto effect = graphics::make_outline_effect(
        shape, state.checkbox ? checkbox.wave_width : token.wave_width,
        (state.checkbox ? checkbox.wave_spread : token.wave_spread) * state.wave_progress, state.wave_color,
        (state.checkbox ? checkbox.wave_opacity : token.wave_opacity) * (1 - state.wave_progress), node.translation,
        window_clip_ ? std::optional{graphics::EffectClip{1, *window_clip_}} : std::nullopt);
    services_->surfaces().update_content_effects(state.wave_range, std::span{&effect, 1});
}

void SelectionComponentHost::on_window_active(bool active) {
    if (!active) {
        for (const auto& item : mounted_) {
            if (auto* state = find(item.component); state && !state->radio) {
                stop_wave(*state);
            }
        }
    }
}

void SelectionComponentHost::synchronize_switch_content(SelectionState& state) {
    if (state.checkbox || state.radio) {
        return;
    }
    bool changed = false;
    for (const auto branch : {state.checked_content, state.unchecked_content}) {
        if (branch.valid()) {
            const auto& bounds = services_->nodes().require(services_->components().root(branch)).bounds;
            const bool selected = branch == (state.checked ? state.checked_content : state.unchecked_content);
            changed |=
                services_->components().set_branch_active(branch, selected && bounds.width > 0 && bounds.height > 0);
        }
    }
    if (changed) {
        services_->mark_scene_structure_dirty();
    }
}

void SelectionComponentHost::place_switch_content(SelectionState& state, layout::LayoutEngine& engine,
                                                  runtime::Rect bounds) {
    const auto token = resolve_tokens(services_->components().theme_scope(state.component)->snapshot(), state.size);
    const float near = token.inner_min_margin;
    const float far = token.inner_max_margin;
    for (const auto branch : {state.checked_content, state.unchecked_content}) {
        if (!branch.valid()) {
            continue;
        }
        const bool checked = branch == state.checked_content;
        const bool rtl = state.direction == SwitchDirection::RightToLeft;
        const float left = std::min(bounds.width, checked != rtl ? near : far);
        const float width = std::max(0.0F, bounds.width - near - far);
        const bool active =
            !state.disabled && !state.loading && (state.press.pressed() || state.focus.keyboard_pressed);
        const float offset =
            active ? (state.size == SwitchSize::Small ? token.track_padding : 2 * token.track_padding) : 0;
        const float shift = (checked != rtl ? -1.0F : 1.0F) * std::min(offset, width);
        const runtime::Rect content{bounds.x + left + shift, bounds.y, width, bounds.height};
        const auto node = services_->components().root(branch);
        const auto& retained = services_->nodes().require(node);
        if (retained.bounds != content || retained.translation != services_->nodes().require(state.node).translation) {
            if (retained.measure_generation == engine.generation()) {
                engine.place_child(node, content, true, true);
            } else {
                engine.place_retained_child(node, content);
            }
            const auto translation = services_->nodes().require(state.node).translation;
            const auto current = services_->nodes().require(node).translation;
            translate_content(services_->nodes(), node, {translation.x - current.x, translation.y - current.y});
        }
    }
    synchronize_switch_content(state);
}

void SelectionComponentHost::position_window_layers(runtime::Size, runtime::Rect) {
    for (const auto& item : mounted_) {
        if (auto* state = find(item.component); state && !state->checkbox && !state->radio) {
            place_switch_content(*state, services_->layout(), services_->nodes().require(state->node).bounds);
        }
    }
}

void SelectionComponentHost::apply_focus(runtime::ComponentId id, input::FocusPresentation value) {
    auto* state = find(id);
    if (!state || state->focus == value) {
        return;
    }
    state->focus = value;
    if (!value.focused && state->press.reset()) {
        services_->pointer().cancel_pointer_interaction(state->interaction);
        state = find(id);
        if (!state) {
            return;
        }
    }
    update_visuals(*state);
}

void SelectionComponentHost::update_layout(SelectionState& state) {
    const auto& theme = services_->components().theme_scope(state.component)->snapshot();
    const auto token = resolve_tokens(theme, state.size);
    if (state.checkbox || state.radio) {
        const float size = state.radio ? token.radio_size : theme.checkbox().size;
        const float gap = state.checkbox ? theme.checkbox().label_gap : token.label_gap;
        if (state.layout_height == size && state.layout_gap == gap) {
            return;
        }
        layout::FlexLayout model;
        model.direction = layout::FlexDirection::horizontal;
        model.main_gap = gap;
        model.align = layout::FlexAlign::center;
        services_->layout().set_layout(state.node, model);
        if (state.spacer.valid()) {
            services_->layout().set_layout(state.spacer, layout::LeafLayout{{size, size}});
        }
        state.layout_height = size;
        state.layout_gap = gap;
    } else {
        if (state.layout_width == token.switch_width && state.layout_height == token.switch_height &&
            state.layout_inner_min == token.inner_min_margin && state.layout_inner_max == token.inner_max_margin) {
            return;
        }
        const auto id = state.component;
        services_->layout().set_layout(
            state.node,
            layout::ComponentLayout{
                [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                    const auto& state = *find(id);
                    const auto token = resolve_tokens(services_->components().theme_scope(id)->snapshot(), state.size);
                    const float margins = token.inner_min_margin + token.inner_max_margin;
                    const layout::Constraints content_limits{0, std::max(0.0F, limits.max_width - margins), 0,
                                                             token.switch_height};
                    float width = 0;
                    for (const auto branch : {state.checked_content, state.unchecked_content}) {
                        if (branch.valid()) {
                            width = std::max(
                                width,
                                engine.measure_child(services_->components().root(branch), content_limits).width);
                        }
                    }
                    return limits.constrain({std::max(token.switch_width, width + margins), token.switch_height});
                },
                [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                    place_switch_content(*find(id), engine, bounds);
                }});
        state.layout_width = token.switch_width;
        state.layout_height = token.switch_height;
        state.layout_inner_min = token.inner_min_margin;
        state.layout_inner_max = token.inner_max_margin;
    }
    services_->dirty().invalidate(state.node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                  runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
}

void SelectionComponentHost::update_visuals(SelectionState& state) {
    const auto& theme = services_->components().theme_scope(state.component)->snapshot();
    const auto token = resolve_tokens(theme, state.size);
    const bool faded = state.disabled || state.loading;
    const float opacity = faded ? theme.switch_token().loading_opacity : 1.0F;
    if (!state.checkbox && !state.radio) {
        const auto& switch_token = theme.switch_token();
        if (state.wave_active &&
            (!state.wave || state.disabled || state.loading || !services_->focus().state().window_active ||
             !animation::resolve_motion_policy(theme, services_->motion_preference()).enabled() ||
             switch_token.wave_width <= 0 || switch_token.wave_opacity <= 0)) {
            stop_wave(state);
        }
        state.effects.shadows = switch_token.handle_shadow;
        state.effects.shadow_fill_offset = 1;
        state.effects.shadow_opacity = faded ? 0.0F : 1.0F;
        const Color track =
            state.checked
                ? (state.hovered ? switch_token.checked_hover_background : switch_token.checked_background)
                : (state.hovered ? switch_token.unchecked_hover_background : switch_token.unchecked_background);
        set_material(state.visuals[0], track, opacity);
        set_material(state.visuals[1], token.handle, opacity);
        for (std::size_t segment = 0; segment < switch_loading_segments; ++segment) {
            const float angle = 2.0F * std::numbers::pi_v<float> *
                                (static_cast<float>(segment) / static_cast<float>(switch_loading_segments) -
                                 (state.spinner_phase - std::floor(state.spinner_phase)));
            const float wave = 0.5F + 0.5F * std::cos(angle);
            set_material(state.visuals[2 + segment],
                         state.checked ? switch_token.checked_background : Color(0, 0, 0, switch_token.loading_opacity),
                         state.loading ? opacity * (0.18F + 0.82F * wave * wave) : 0.0F);
        }
        for (const auto branch : {state.checked_content, state.unchecked_content}) {
            if (branch.valid()) {
                services_->nodes().require(services_->components().root(branch)).content_opacity = opacity;
            }
        }
    } else if (state.radio) {
        const Color border = state.disabled                   ? token.box_border
                             : state.checked || state.hovered ? token.on
                                                              : token.box_border;
        const Color fill = state.disabled  ? token.disabled_background
                           : state.checked ? (state.hovered ? token.on_hover : token.on)
                                           : token.box_background;
        set_material(state.visuals[0], border);
        set_material(state.visuals[1], fill);
        set_material(state.visuals[2], state.disabled ? token.disabled_foreground : token.radio_dot,
                     state.checked ? 1.0F : 0.0F);
    } else {
        const auto& checkbox = theme.checkbox();
        if (state.wave_active && (!state.wave || state.disabled || !services_->focus().state().window_active ||
                                  !animation::resolve_motion_policy(theme, services_->motion_preference()).enabled() ||
                                  checkbox.wave_width <= 0 || checkbox.wave_opacity <= 0)) {
            stop_wave(state);
        }
        const bool mixed = state.indeterminate;
        const bool pressed = state.press.pressed() || state.focus.keyboard_pressed;
        const Color active = pressed         ? checkbox.primary_active
                             : state.hovered ? checkbox.primary_hover
                                             : checkbox.primary;
        const Color border = state.disabled             ? checkbox.border
                             : state.checked && !mixed  ? active
                             : state.hovered || pressed ? checkbox.primary
                                                        : checkbox.border;
        const Color fill = state.disabled            ? checkbox.disabled_background
                           : state.checked && !mixed ? active
                                                     : checkbox.background;
        set_material(state.visuals[0], border);
        set_material(state.visuals[1], fill);
        for (std::size_t segment = 0; segment < checkbox_check_segments; ++segment) {
            set_material(state.visuals[2 + segment], state.disabled ? checkbox.disabled_foreground : checkbox.checkmark,
                         state.checked && !mixed ? 1.0F : 0.0F);
        }
        set_material(state.visuals[checkbox_indeterminate_layer],
                     state.disabled ? checkbox.disabled_foreground : checkbox.primary, mixed ? 1.0F : 0.0F);
    }
    const bool is_switch = !state.checkbox && !state.radio;
    state.effects.focus_color = is_switch        ? theme.switch_token().focus_color
                                : state.checkbox ? theme.checkbox().focus
                                                 : token.focus;
    state.effects.focus_opacity = state.focus.focus_visible && !state.disabled ? (is_switch ? opacity : 1.0F) : 0.0F;
    state.effects.focus_width = is_switch        ? theme.switch_token().focus_width
                                : state.checkbox ? theme.checkbox().focus_width
                                                 : 2.0F;
    state.effects.focus_enabled = state.effects.focus_width > 0;
    state.effects.focus_offset = is_switch        ? theme.switch_token().focus_offset
                                 : state.checkbox ? theme.checkbox().focus_offset
                                                  : 0.0F;
    if (state.surface.valid()) {
        const auto visual_changes = services_->surfaces().update_surface(state.surface, state.visuals);
        const auto effect_changes = services_->surfaces().update_effects(state.surface, state.effects);
        if (visual_changes || effect_changes) {
            services_->dirty().invalidate(state.node, runtime::DirtyFlags::Material);
        }
    }
    const auto label_color = state.checkbox
                                 ? (state.disabled ? theme.checkbox().disabled_foreground : theme.checkbox().label)
                             : !state.radio   ? Color::rgba8(255, 255, 255)
                             : state.disabled ? token.disabled_foreground
                                              : theme.alias().color_text;
    static_cast<void>(state.label_foreground.set(channels(label_color)));
    static_cast<void>(state.label_typography.set({theme.text().font_family, theme.text().font_weight, false,
                                                  is_switch        ? theme.switch_token().content_font_size
                                                  : state.checkbox ? theme.checkbox().font_size
                                                                   : theme.text().font_size,
                                                  is_switch        ? token.switch_height
                                                  : state.checkbox ? theme.checkbox().line_height
                                                                   : theme.text().line_height}));
    synchronize_switch_content(state);
}

void SelectionComponentHost::update_geometry(SelectionState& state, runtime::Size viewport) {
    const auto& node = services_->nodes().require(state.node);
    const auto& theme = services_->components().theme_scope(state.component)->snapshot();
    const auto token = resolve_tokens(theme, state.size);
    const auto rect = node.bounds;
    auto next = state.visuals;
    if (!state.checkbox && !state.radio) {
        set_geometry(next[0], rect, viewport, std::min(rect.width, rect.height) / 2.0F, node.translation);
        const float checked = std::clamp(state.presented_checked, 0.0F, 1.0F);
        const float position = state.direction == SwitchDirection::RightToLeft ? 1 - checked : checked;
        const bool active =
            !state.disabled && !state.loading && (state.press.pressed() || state.focus.keyboard_pressed);
        const float extra = active ? token.handle_size * 0.3F : 0;
        const float handle_size =
            std::min(token.handle_size + extra, std::max(0.0F, rect.width - 2 * token.track_padding));
        const float x = rect.x + std::min(token.track_padding, rect.width / 2) +
                        position * std::max(0.0F, rect.width - handle_size - 2 * token.track_padding);
        const runtime::Rect handle{x, rect.y + (rect.height - token.handle_size) / 2.0F, handle_size,
                                   token.handle_size};
        set_geometry(next[1], handle, viewport, std::min(handle.width, handle.height) / 2.0F, node.translation);
        const float dot = std::max(1.0F, token.handle_size * 0.13F);
        const float orbit = token.handle_size * 0.27F;
        const float center_x = x + token.handle_size / 2.0F;
        const float center_y = rect.y + rect.height / 2.0F;
        for (std::size_t segment = 0; segment < switch_loading_segments; ++segment) {
            const float angle = 2.0F * std::numbers::pi_v<float> * static_cast<float>(segment) /
                                static_cast<float>(switch_loading_segments);
            set_geometry(next[2 + segment],
                         {center_x + std::sin(angle) * orbit - dot / 2.0F,
                          center_y - std::cos(angle) * orbit - dot / 2.0F, dot, dot},
                         viewport, dot / 2.0F, node.translation);
        }
    } else if (state.radio) {
        const runtime::Rect ring{rect.x, rect.y + (rect.height - token.radio_size) / 2.0F, token.radio_size,
                                 token.radio_size};
        set_geometry(next[0], ring, viewport, ring.width / 2.0F, node.translation);
        const float stroke = token.line_width;
        const runtime::Rect inner{ring.x + stroke, ring.y + stroke, std::max(0.0F, ring.width - 2.0F * stroke),
                                  std::max(0.0F, ring.height - 2.0F * stroke)};
        set_geometry(next[1], inner, viewport, inner.width / 2.0F, node.translation);
        const float dot = token.radio_dot_size;
        set_geometry(next[2], {ring.x + (ring.width - dot) / 2.0F, ring.y + (ring.height - dot) / 2.0F, dot, dot},
                     viewport, dot / 2.0F, node.translation);
    } else {
        const auto& checkbox = theme.checkbox();
        const auto spacer = services_->nodes().require(state.spacer).bounds;
        const runtime::Rect box{spacer.x, rect.y + (rect.height - checkbox.size) / 2.0F, checkbox.size, checkbox.size};
        set_geometry(next[0], box, viewport, checkbox.border_radius, node.translation);
        const float inset = checkbox.line_width;
        set_geometry(next[1],
                     {box.x + inset, box.y + inset, std::max(0.0F, box.width - 2 * inset),
                      std::max(0.0F, box.height - 2 * inset)},
                     viewport, std::max(0.0F, checkbox.border_radius - inset), node.translation);
        const float stroke = checkbox.check_width;
        for (std::size_t segment = 0; segment < checkbox_check_segments; ++segment) {
            const float progress = static_cast<float>(segment) / static_cast<float>(checkbox_check_segments - 1);
            const float x =
                progress < 0.4F ? 0.22F + 0.20F * (progress / 0.4F) : 0.42F + 0.37F * ((progress - 0.4F) / 0.6F);
            const float y =
                progress < 0.4F ? 0.52F + 0.18F * (progress / 0.4F) : 0.70F - 0.40F * ((progress - 0.4F) / 0.6F);
            set_geometry(
                next[2 + segment],
                {box.x + box.width * x - stroke / 2.0F, box.y + box.height * y - stroke / 2.0F, stroke, stroke},
                viewport, stroke / 2.0F, node.translation);
        }
        const float side = checkbox.indeterminate_size;
        set_geometry(next[checkbox_indeterminate_layer],
                     {box.x + (box.width - side) / 2.0F, box.y + (box.height - side) / 2.0F, side, side}, viewport,
                     0.0F, node.translation);
    }
    if (next != state.visuals) {
        state.visuals = next;
        static_cast<void>(services_->surfaces().update_surface(state.surface, state.visuals));
    }
    auto effects = state.effects;
    if (!state.checkbox && !state.radio) {
        effects.shadow_shape = graphics::LogicalRoundedRect{{state.visuals[1].bounds[0], state.visuals[1].bounds[1],
                                                             state.visuals[1].bounds[2], state.visuals[1].bounds[3]},
                                                            state.visuals[1].corner_radius};
        effects.ancestor_clip = window_clip_ ? std::optional{graphics::EffectClip{1, *window_clip_}} : std::nullopt;
    }
    const float indicator_size = state.radio ? token.radio_size : theme.checkbox().size;
    const float indicator_x = state.checkbox ? services_->nodes().require(state.spacer).bounds.x : rect.x;
    effects.shape = {
        state.checkbox || state.radio
            ? runtime::Rect{indicator_x, rect.y + (rect.height - indicator_size) / 2.0F, indicator_size, indicator_size}
            : rect,
        state.checkbox ? theme.checkbox().border_radius
        : state.radio  ? indicator_size / 2.0F
                       : std::min(rect.width, rect.height) / 2.0F};
    effects.translation = node.translation;
    if (effects != state.effects) {
        state.effects = effects;
        static_cast<void>(services_->surfaces().update_effects(state.surface, state.effects));
    }
    if (state.wave_active) {
        publish_wave(state);
    }
}

void SelectionComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    viewport_ = viewport;
    window_clip_ = clip;
    for (const auto& item : mounted_) {
        if (auto* state = find(item.component)) {
            update_geometry(*state, viewport);
        }
    }
}

struct SwitchPropsAccess {
    static void mount(const SwitchProps& props, const SwitchSlots& slots) {
        if (!active_selection_host) {
            throw std::logic_error("Switch requires an active SelectionComponentHost");
        }
        auto& host = *active_selection_host;
        if (props.checked_ && props.default_checked_) {
            throw std::invalid_argument("Switch checked and defaultChecked are mutually exclusive");
        }
        const auto size = read_prop(props.size_);
        validate(size);
        const auto direction = read_prop(props.direction_);
        validate(direction);
        const auto reference = props.ref_ ? props.ref_->state_ : nullptr;
        if (reference) {
            reference->ensure_owner();
            if (reference->binding) {
                throw std::invalid_argument("SwitchRef is already bound");
            }
        }
        auto& build = runtime::require_component_build_context();
        const auto component = build.mount_component<SelectionState>();
        auto& state = build.state<SelectionState>(component);
        state.component = component;
        state.node = build.root(component);
        host.services_->nodes().require(state.node).clip_content = true;
        state.controlled = props.checked_.has_value();
        state.checked = props.checked_ ? read_prop(*props.checked_) : props.default_checked_.value_or(false);
        state.presented_checked = state.checked ? 1.0F : 0.0F;
        state.disabled = read_prop(props.disabled_);
        state.loading = read_prop(props.loading_);
        state.size = size;
        state.direction = direction;
        state.wave = read_prop(props.wave_);
        state.ref = reference;
        state.visuals.resize(switch_layer_count);
        state.on_change = props.on_change_;
        state.on_click = props.on_click_;
        build.on_resource_cleanup(component, [&host, component] {
            if (auto* current = host.find(component)) {
                host.release_selection(*current);
            }
        });
        if (reference) {
            reference->binding = true;
        }
        host.update_layout(state);
        runtime::connect_layout_style(build.scope(component), props.layout_, state.node, host.services_->nodes(),
                                      host.services_->dirty());
        state.fragment = build.register_scene_fragment(component, runtime::SceneFragmentPlacement::before_children);
        host.attach_interaction(state);
        host.update_visuals(state);
        auto mount_branch = [&](const auto& slot, runtime::ComponentId& branch) {
            if (!slot) {
                return;
            }
            build.mount_slot(component, Content{[&] {
                                 auto& nested = runtime::require_component_build_context();
                                 branch = nested.mount_component<int>(0);
                                 const auto node = nested.root(branch);
                                 auto model = layout::FlexLayout{};
                                 model.justify = layout::FlexJustify::center;
                                 model.align = layout::FlexAlign::center;
                                 host.services_->layout().set_layout(node, model);
                                 host.services_->nodes().require(node).clip_content = true;
                                 nested.on_resource_cleanup(branch, [layout = &host.services_->layout(), node] {
                                     static_cast<void>(layout->remove_layout(node));
                                 });
                                 nested.mount_slot_with_semantic_text_style(
                                     branch, *slot, Prop<runtime::SemanticForeground>{state.label_foreground},
                                     Prop<runtime::SemanticTypography>{state.label_typography});
                             }});
        };
        mount_branch(slots.checked, state.checked_content);
        mount_branch(slots.unchecked, state.unchecked_content);
        for (const auto interaction : host.services_->interactions().declaration_order()) {
            const auto* record = host.services_->interactions().find(interaction);
            if (!record || record->component == component) {
                continue;
            }
            for (auto parent = host.services_->components().parent(record->component); parent;
                 parent = host.services_->components().parent(*parent)) {
                if (*parent == component) {
                    throw std::invalid_argument("Switch content must be passive");
                }
            }
        }
        state.surface = host.services_->surfaces().create_surface(component, state.node, state.fragment, state.visuals,
                                                                  state.effects, state.interaction);
        state.animation_scope = host.services_->animations().create_scope();
        state.handle_target = host.services_->animations().register_target(state.animation_scope, host,
                                                                           animation::AnimationValueKind::scalar,
                                                                           animation::AnimationDirtyDomain::geometry);
        state.spinner_target = host.services_->animations().register_target(
            state.animation_scope, host, animation::AnimationValueKind::scalar,
            animation::AnimationDirtyDomain::material | animation::AnimationDirtyDomain::animation);
        state.wave_target = host.services_->animations().register_target(
            state.animation_scope, host, animation::AnimationValueKind::scalar,
            animation::AnimationDirtyDomain::geometry | animation::AnimationDirtyDomain::animation);
        host.synchronize_spinner(state);
        auto& scope = build.scope(component);
        if (props.checked_) {
            static_cast<void>(connect_prop(scope, *props.checked_,
                                           [&host, component](bool value) { host.apply_checked(component, value); }));
        }
        static_cast<void>(connect_prop(scope, props.disabled_,
                                       [&host, component](bool value) { host.apply_disabled(component, value); }));
        static_cast<void>(connect_prop(scope, props.loading_,
                                       [&host, component](bool value) { host.apply_loading(component, value); }));
        static_cast<void>(connect_prop(scope, props.size_,
                                       [&host, component](SwitchSize value) { host.apply_size(component, value); }));
        static_cast<void>(connect_prop(scope, props.direction_, [&host, component](SwitchDirection value) {
            host.apply_direction(component, value);
        }));
        static_cast<void>(
            connect_prop(scope, props.wave_, [&host, component](bool value) { host.apply_wave(component, value); }));
        const auto theme = host.services_->components().theme_scope(component);
        state.theme_subscription = theme->capture(
            [&host, component](theme_runtime::DirtyPhase) {
                if (auto* current = host.find(component)) {
                    host.update_layout(*current);
                    host.update_visuals(*current);
                    host.synchronize_spinner(*current);
                    if (!animation::resolve_motion_policy(
                             host.services_->components().theme_scope(component)->snapshot(),
                             host.services_->motion_preference())
                             .enabled()) {
                        host.retarget_handle(*current);
                    }
                }
            },
            [theme] {
                static_cast<void>(theme->map());
                static_cast<void>(theme->alias());
                static_cast<void>(theme->switch_geometry());
                static_cast<void>(theme->switch_colors());
                static_cast<void>(theme->switch_effects());
                static_cast<void>(theme->text());
            });
        host.mounted_.push_back({component, state.node, state.interaction, state.surface, false});
        const auto focus = [&host, component] {
            const auto* state = host.find(component);
            if (!state || state->disabled || !host.services_->focus().state().window_active) {
                return false;
            }
            host.services_->focus().defer_focus(state->interaction, input::FocusModality::keyboard);
            return true;
        };
        if (reference) {
            reference->focus = focus;
            reference->blur = [&host, component] {
                const auto* state = host.find(component);
                if (!state || host.services_->focus().state().focused != state->interaction) {
                    return false;
                }
                host.services_->focus().defer_focus({}, input::FocusModality::keyboard);
                return true;
            };
        }
        if (props.auto_focus_) {
            static_cast<void>(focus());
        }
    }
};

struct CheckboxPropsAccess {
    static runtime::ComponentId mount(const CheckboxProps& props, const std::optional<CheckboxLabel>& label) {
        if (!active_selection_host) {
            throw std::logic_error("Checkbox requires an active SelectionComponentHost");
        }
        auto& host = *active_selection_host;
        if (props.checked_ && props.default_checked_) {
            throw std::invalid_argument("Checkbox checked and defaultChecked are mutually exclusive");
        }
        const auto reference = props.ref_ ? props.ref_->state_ : nullptr;
        if (reference) {
            reference->ensure_owner();
            if (reference->binding) {
                throw std::invalid_argument("CheckboxRef is already bound");
            }
        }
        if (props.direction_) {
            static_cast<void>(selection_direction(read_prop(*props.direction_)));
        }
        auto& build = runtime::require_component_build_context();
        const auto component = build.mount_component<SelectionState>();
        auto& state = build.state<SelectionState>(component);
        state.component = component;
        state.node = build.root(component);
        state.checkbox = true;
        state.group = props.skip_group_ ? runtime::ComponentId{} : host.parent_checkbox_group(component);
        auto* group = host.services_->components().state<CheckboxGroupState>(state.group);
        if (props.value_) {
            validate_checkbox_value(*props.value_);
        }
        if (group) {
            if (props.checked_ || props.default_checked_ || !props.value_) {
                throw std::invalid_argument("Grouped Checkbox requires value without checked props");
            }
            if (!group->reconciling && group->options.size() >= 1024) {
                throw std::invalid_argument("CheckboxGroup supports at most 1024 options");
            }
            for (const auto option : group->options) {
                const auto* existing = host.find(option);
                if (existing && existing->checkbox_value == props.value_) {
                    throw std::invalid_argument("CheckboxGroup option values must be unique");
                }
            }
        }
        state.checkbox_value = props.value_;
        state.visuals.resize(checkbox_layer_count);
        state.controlled = group || props.checked_.has_value();
        state.checked = group            ? std::ranges::find(group->value, *props.value_) != group->value.end()
                        : props.checked_ ? read_prop(*props.checked_)
                                         : props.default_checked_.value_or(false);
        state.indeterminate = read_prop(props.indeterminate_);
        state.own_disabled = read_prop(props.disabled_);
        state.disabled = state.own_disabled || (group && group->disabled);
        state.on_change = props.on_change_;
        state.on_click = props.on_click_;
        state.checkbox_ref = reference;
        state.own_direction = props.direction_.has_value();
        const auto direction = props.direction_ ? read_prop(*props.direction_)
                               : group          ? group->direction
                                                : CheckboxDirection::LeftToRight;
        state.direction = selection_direction(direction);
        state.wave = read_prop(props.wave_);
        build.on_resource_cleanup(component, [&host, component] {
            if (auto* current = host.find(component)) {
                host.release_selection(*current);
            }
        });
        if (reference) {
            reference->binding = true;
        }
        host.update_layout(state);
        runtime::connect_layout_style(build.scope(component), props.layout_, state.node, host.services_->nodes(),
                                      host.services_->dirty());
        state.fragment = build.register_scene_fragment(component, runtime::SceneFragmentPlacement::before_children);
        host.attach_interaction(state);
        host.update_visuals(state);
        state.surface = host.services_->surfaces().create_surface(component, state.node, state.fragment, state.visuals,
                                                                  state.effects, state.interaction);
        state.animation_scope = host.services_->animations().create_scope();
        state.wave_target = host.services_->animations().register_target(
            state.animation_scope, host, animation::AnimationValueKind::scalar,
            animation::AnimationDirtyDomain::geometry | animation::AnimationDirtyDomain::animation);
        auto& scope = build.scope(component);
        if (props.checked_) {
            static_cast<void>(connect_prop(scope, *props.checked_,
                                           [&host, component](bool value) { host.apply_checked(component, value); }));
        }
        static_cast<void>(connect_prop(scope, props.disabled_, [&host, component](bool value) {
            host.apply_checkbox_own_disabled(component, value);
        }));
        static_cast<void>(connect_prop(scope, props.indeterminate_,
                                       [&host, component](bool value) { host.apply_indeterminate(component, value); }));
        static_cast<void>(
            connect_prop(scope, props.wave_, [&host, component](bool value) { host.apply_wave(component, value); }));
        if (props.direction_) {
            static_cast<void>(connect_prop(scope, *props.direction_, [&host, component](CheckboxDirection value) {
                host.apply_direction(component, selection_direction(value));
            }));
        }
        const auto theme = host.services_->components().theme_scope(component);
        state.theme_subscription = theme->capture(
            [&host, component](theme_runtime::DirtyPhase) {
                if (auto* current = host.find(component)) {
                    host.update_layout(*current);
                    host.update_visuals(*current);
                }
            },
            [theme] {
                static_cast<void>(theme->checkbox_metrics());
                static_cast<void>(theme->checkbox_colors());
                static_cast<void>(theme->checkbox_effects());
                static_cast<void>(theme->motion_enabled());
                static_cast<void>(theme->text());
            });
        mount_selection_label(build, *host.services_, state, component, label,
                              host.services_->components().theme_scope(component)->snapshot().checkbox().size);
        host.services_->nodes().require(state.spacer).external_layout.order =
            state.direction == SwitchDirection::RightToLeft ? 1 : 0;
        for (const auto interaction : host.services_->interactions().declaration_order()) {
            const auto* record = host.services_->interactions().find(interaction);
            if (!record || record->component == component) {
                continue;
            }
            for (auto parent = host.services_->components().parent(record->component); parent;
                 parent = host.services_->components().parent(*parent)) {
                if (*parent == component) {
                    throw std::invalid_argument("CheckboxLabel accepts passive content only");
                }
            }
        }
        host.mounted_.push_back({component, state.node, state.interaction, state.surface, true});
        if (group) {
            group->options.push_back(component);
        }
        const auto focus = [&host, component] {
            const auto* state = host.find(component);
            if (!state || state->disabled || !host.services_->focus().state().window_active) {
                return false;
            }
            host.services_->focus().defer_focus(state->interaction, input::FocusModality::keyboard);
            return true;
        };
        if (reference) {
            reference->focus = focus;
            reference->blur = [&host, component] {
                const auto* state = host.find(component);
                if (!state || host.services_->focus().state().focused != state->interaction) {
                    return false;
                }
                host.services_->focus().defer_focus({}, input::FocusModality::keyboard);
                return true;
            };
        }
        if (props.auto_focus_) {
            static_cast<void>(focus());
        }
        return component;
    }
};

struct CheckboxGroupPropsAccess {
    static void mount(const CheckboxGroupProps& props, const std::optional<CheckboxGroupContent>& content) {
        if (!active_selection_host) {
            throw std::logic_error("CheckboxGroup requires an active SelectionComponentHost");
        }
        auto& host = *active_selection_host;
        if ((props.value_ && props.default_value_) || (props.options_ && content)) {
            throw std::invalid_argument("CheckboxGroup value/defaultValue and options/content are exclusive");
        }
        const auto options = props.options_ ? read_prop(*props.options_) : std::vector<CheckboxOption>{};
        validate_checkbox_options(options);
        const auto initial_value =
            props.value_ ? read_prop(*props.value_) : props.default_value_.value_or(CheckboxValues{});
        validate_checkbox_values(initial_value);
        const auto orientation = read_prop(props.orientation_);
        if (orientation != CheckboxGroupOrientation::Horizontal && orientation != CheckboxGroupOrientation::Vertical) {
            throw std::invalid_argument("CheckboxGroup orientation is invalid");
        }
        const auto direction = read_prop(props.direction_);
        static_cast<void>(selection_direction(direction));
        auto& build = runtime::require_component_build_context();
        const auto component = build.mount_component<CheckboxGroupState>();
        auto& state = build.state<CheckboxGroupState>(component);
        state.component = component;
        state.node = build.root(component);
        state.controlled = props.value_.has_value();
        state.value = initial_value;
        state.disabled = read_prop(props.disabled_);
        state.orientation = orientation;
        state.direction = direction;
        state.on_change = props.on_change_;
        build.on_resource_cleanup(component, [&host, component] {
            if (auto* current = host.services_->components().state<CheckboxGroupState>(component)) {
                host.services_->pointer().cancel_interaction(current->anchor);
                host.services_->focus().cancel_interaction(current->anchor);
                static_cast<void>(host.services_->interactions().remove(current->anchor));
                static_cast<void>(host.services_->layout().remove_layout(current->node));
            }
        });
        host.update_checkbox_group_layout(state);
        runtime::connect_layout_style(build.scope(component), props.layout_, state.node, host.services_->nodes(),
                                      host.services_->dirty());
        state.anchor = host.services_->interactions().create(
            {component, state.node, host.parent_interaction(component), true, false, {}});
        state.fragment = build.register_scene_fragment(component, runtime::SceneFragmentPlacement::before_children);
        host.services_->scene_composer().set_fragment(state.fragment, {}, state.anchor);
        if (content) {
            build.mount_slot(component, *content);
        } else {
            build.mount_slot(component, Content{[&] {
                                 for (const auto& option : options) {
                                     auto generated = std::make_unique<GeneratedCheckboxOption>(option);
                                     CheckboxProps child;
                                     child.value(option.value).disabled(generated->disabled);
                                     generated->component =
                                         CheckboxPropsAccess::mount(child, CheckboxLabel{[caption = generated->label] {
                                                                        Text(TextProps{}.content(caption));
                                                                    }});
                                     state.generated.push_back(std::move(generated));
                                 }
                             }});
        }
        state.source_options = options;
        auto& scope = build.scope(component);
        if (props.value_) {
            static_cast<void>(connect_prop(scope, *props.value_, [&host, component](const CheckboxValues& value) {
                host.apply_checkbox_group_value(component, value);
            }));
        }
        static_cast<void>(connect_prop(scope, props.disabled_, [&host, component](bool value) {
            host.apply_checkbox_group_disabled(component, value);
        }));
        static_cast<void>(connect_prop(scope, props.orientation_, [&host, component](CheckboxGroupOrientation value) {
            host.apply_checkbox_group_orientation(component, value);
        }));
        static_cast<void>(connect_prop(scope, props.direction_, [&host, component](CheckboxDirection value) {
            host.apply_checkbox_group_direction(component, value);
        }));
        if (props.options_) {
            static_cast<void>(
                connect_prop(scope, *props.options_, [&host, component](const std::vector<CheckboxOption>& value) {
                    host.apply_checkbox_options(component, value);
                }));
        }
        const auto theme = host.services_->components().theme_scope(component);
        state.theme_subscription = theme->capture(
            [&host, component](theme_runtime::DirtyPhase) {
                if (auto* group = host.services_->components().state<CheckboxGroupState>(component)) {
                    host.update_checkbox_group_layout(*group);
                }
            },
            [theme] { static_cast<void>(theme->map()); });
        host.checkbox_groups_.push_back({component, state.node, state.anchor});
    }
};

void SelectionComponentHost::apply_checkbox_options(runtime::ComponentId id, std::vector<CheckboxOption> options) {
    validate_checkbox_options(options);
    auto* group = services_->components().state<CheckboxGroupState>(id);
    if (!group || group->source_options == options) {
        return;
    }
    std::vector<std::size_t> matched(options.size(), group->generated.size());
    std::vector<std::unique_ptr<GeneratedCheckboxOption>> replacement(options.size());
    for (std::size_t i = 0; i < options.size(); ++i) {
        for (std::size_t previous = 0; previous < group->generated.size(); ++previous) {
            if (group->generated[previous]->value == options[i].value) {
                matched[i] = previous;
                break;
            }
        }
        if (matched[i] == group->generated.size()) {
            replacement[i] = std::make_unique<GeneratedCheckboxOption>(options[i]);
        }
    }
    const auto original_options = group->options;
    group->reconciling = true;
    try {
        if (std::ranges::any_of(replacement, [](const auto& option) { return bool(option); })) {
            services_->append_slot(id, Content{[&] {
                                       for (auto& option : replacement) {
                                           if (option) {
                                               CheckboxProps child;
                                               child.value(option->value).disabled(option->disabled);
                                               option->component = CheckboxPropsAccess::mount(
                                                   child, CheckboxLabel{[caption = option->label] {
                                                       Text(TextProps{}.content(caption));
                                                   }});
                                           }
                                       }
                                   }});
        }
    } catch (...) {
        if (auto* live = services_->components().state<CheckboxGroupState>(id)) {
            live->options = original_options;
            live->reconciling = false;
        }
        throw;
    }
    auto removed = std::move(group->generated);
    for (std::size_t i = 0; i < replacement.size(); ++i) {
        if (matched[i] < removed.size()) {
            replacement[i] = std::move(removed[matched[i]]);
        }
    }
    group->generated = std::move(replacement);
    group->source_options = options;
    group->options.clear();
    std::vector<input::InteractionId> order;
    CheckboxValues retained_value;
    for (std::size_t i = 0; i < group->generated.size(); ++i) {
        const auto& option = group->generated[i];
        group->options.push_back(option->component);
        auto* state = find(option->component);
        if (!state) {
            throw std::logic_error("CheckboxGroup retained option is stale");
        }
        services_->nodes().require(state->node).external_layout.order = static_cast<int>(i);
        order.push_back(state->interaction);
        option->label.set(options[i].label);
        option->disabled.set(options[i].disabled);
        if (std::ranges::find(group->value, option->value) != group->value.end()) {
            retained_value.push_back(option->value);
        }
    }
    for (const auto& option : removed) {
        if (option) {
            services_->destroy(option->component);
        }
    }
    group = services_->components().state<CheckboxGroupState>(id);
    if (!group) {
        return;
    }
    group->reconciling = false;
    if (!group->controlled) {
        apply_checkbox_group_value(id, std::move(retained_value));
    }
    services_->interactions().reorder_after(group->anchor, order);
    services_->dirty().invalidate(group->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                   runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    services_->mark_scene_structure_dirty();
}

struct RadioPropsAccess {
    static void mount(const RadioProps& props, const std::optional<RadioLabel>& label,
                      runtime::ComponentId group_id = {}) {
        if (!active_selection_host) {
            throw std::logic_error("Radio requires an active SelectionComponentHost");
        }
        auto& host = *active_selection_host;
        if (props.checked_ && props.default_checked_) {
            throw std::invalid_argument("Radio checked and defaultChecked are mutually exclusive");
        }
        if (group_id.valid() && (props.checked_ || props.default_checked_ || !props.value_)) {
            throw std::invalid_argument("Grouped Radio requires value without checked props");
        }
        auto& build = runtime::require_component_build_context();
        auto* group = group_id.valid() ? host.services_->components().state<RadioGroupState>(group_id) : nullptr;
        if (group_id.valid() && !group) {
            throw std::logic_error("RadioGroup is stale");
        }
        const auto component = build.mount_component<SelectionState>();
        auto& state = build.state<SelectionState>(component);
        state.component = component;
        state.node = build.root(component);
        state.radio = true;
        state.group = group_id;
        state.value = props.value_;
        state.visuals.resize(radio_layer_count);
        state.controlled = group_id.valid() || props.checked_.has_value();
        state.checked = group            ? group->value && props.value_ && *group->value == *props.value_
                        : props.checked_ ? read_prop(*props.checked_)
                                         : props.default_checked_.value_or(false);
        state.own_disabled = read_prop(props.disabled_);
        state.disabled = state.own_disabled || (group && group->disabled);
        state.on_change = props.on_change_;
        build.on_resource_cleanup(component, [&host, component] {
            if (auto* current = host.find(component)) {
                host.release_selection(*current);
            }
        });
        host.update_layout(state);
        runtime::connect_layout_style(build.scope(component), props.layout_, state.node, host.services_->nodes(),
                                      host.services_->dirty());
        state.fragment = build.register_scene_fragment(component, runtime::SceneFragmentPlacement::before_children);
        host.attach_interaction(state);
        host.update_visuals(state);
        state.surface = host.services_->surfaces().create_surface(component, state.node, state.fragment, state.visuals,
                                                                  state.effects, state.interaction);
        auto& scope = build.scope(component);
        if (props.checked_) {
            static_cast<void>(connect_prop(scope, *props.checked_,
                                           [&host, component](bool value) { host.apply_checked(component, value); }));
        }
        static_cast<void>(connect_prop(scope, props.disabled_, [&host, component](bool value) {
            host.apply_radio_own_disabled(component, value);
        }));
        const auto theme = host.services_->components().theme_scope(component);
        state.theme_subscription = theme->capture(
            [&host, component](theme_runtime::DirtyPhase) {
                if (auto* current = host.find(component)) {
                    host.update_layout(*current);
                    host.update_visuals(*current);
                }
            },
            [theme] {
                static_cast<void>(theme->map());
                static_cast<void>(theme->alias());
                static_cast<void>(theme->text());
                static_cast<void>(theme->line_width());
            });
        mount_selection_label(build, *host.services_, state, component, label,
                              host.services_->components().theme_scope(component)->snapshot().map().font_size_large);
        host.mounted_.push_back({component, state.node, state.interaction, state.surface, false, true});
        if (group) {
            group->options.push_back(component);
        }
    }
};

struct RadioGroupPropsAccess {
    static void mount(const RadioGroupProps& props) {
        if (!active_selection_host) {
            throw std::logic_error("RadioGroup requires an active SelectionComponentHost");
        }
        auto& host = *active_selection_host;
        if (props.value_ && props.default_value_) {
            throw std::invalid_argument("RadioGroup value and defaultValue are mutually exclusive");
        }
        const auto orientation = read_prop(props.orientation_);
        if (orientation != RadioGroupOrientation::Horizontal && orientation != RadioGroupOrientation::Vertical) {
            throw std::invalid_argument("RadioGroup orientation is invalid");
        }
        for (std::size_t left = 0; left < props.options_.size(); ++left) {
            for (std::size_t right = left + 1; right < props.options_.size(); ++right) {
                if (props.options_[left].value == props.options_[right].value) {
                    throw std::invalid_argument("RadioGroup option values must be unique");
                }
            }
        }
        auto& build = runtime::require_component_build_context();
        const auto component = build.mount_component<RadioGroupState>();
        auto& state = build.state<RadioGroupState>(component);
        state.component = component;
        state.node = build.root(component);
        state.controlled = props.value_.has_value();
        state.value = props.value_ ? read_prop(*props.value_) : props.default_value_;
        state.disabled = read_prop(props.disabled_);
        state.orientation = orientation;
        state.on_change = props.on_change_;
        build.on_resource_cleanup(component, [&host, component] {
            if (auto* current = host.services_->components().state<RadioGroupState>(component)) {
                static_cast<void>(host.services_->layout().remove_layout(current->node));
            }
        });
        host.update_group_layout(state);
        runtime::connect_layout_style(build.scope(component), props.layout_, state.node, host.services_->nodes(),
                                      host.services_->dirty());
        auto& scope = build.scope(component);
        if (props.value_) {
            static_cast<void>(
                connect_prop(scope, *props.value_, [&host, component](const std::optional<String>& value) {
                    host.apply_group_value(component, value);
                }));
        }
        static_cast<void>(connect_prop(
            scope, props.disabled_, [&host, component](bool value) { host.apply_group_disabled(component, value); }));
        static_cast<void>(connect_prop(scope, props.orientation_, [&host, component](RadioGroupOrientation value) {
            host.apply_group_orientation(component, value);
        }));
        const auto theme = host.services_->components().theme_scope(component);
        state.theme_subscription = theme->capture(
            [&host, component](theme_runtime::DirtyPhase) {
                if (auto* current = host.services_->components().state<RadioGroupState>(component)) {
                    host.update_group_layout(*current);
                }
            },
            [theme] { static_cast<void>(theme->map()); });
        build.mount_slot(component, Content{[&] {
                             for (const auto& option : props.options_) {
                                 RadioProps radio;
                                 radio.value(option.value).disabled(option.disabled);
                                 RadioPropsAccess::mount(radio, RadioLabel{[label = option.label] { Text(label); }},
                                                         component);
                             }
                         }});
        host.groups_.push_back(component);
    }
};

} // namespace ryn::detail

namespace ryn {
void Switch(SwitchProps props) {
    detail::SwitchPropsAccess::mount(props, {});
}

void Switch(SwitchProps props, SwitchSlots slots) {
    detail::SwitchPropsAccess::mount(props, slots);
}

SwitchRef::SwitchRef() : state_(std::make_shared<detail::SwitchRefState>()) {}

bool SwitchRef::bound() const {
    if (!state_) {
        return false;
    }
    state_->ensure_owner();
    return bool(state_->focus);
}

bool SwitchRef::focus() const {
    if (!state_) {
        return false;
    }
    state_->ensure_owner();
    const auto callback = state_->focus;
    return callback && callback();
}

bool SwitchRef::blur() const {
    if (!state_) {
        return false;
    }
    state_->ensure_owner();
    const auto callback = state_->blur;
    return callback && callback();
}

void Checkbox(CheckboxProps props, std::optional<CheckboxLabel> label) {
    static_cast<void>(detail::CheckboxPropsAccess::mount(props, label));
}

CheckboxRef::CheckboxRef() : state_(std::make_shared<detail::CheckboxRefState>()) {}

bool CheckboxRef::bound() const {
    if (!state_) {
        return false;
    }
    state_->ensure_owner();
    return bool(state_->focus);
}

bool CheckboxRef::focus() const {
    if (!state_) {
        return false;
    }
    state_->ensure_owner();
    const auto callback = state_->focus;
    return callback && callback();
}

bool CheckboxRef::blur() const {
    if (!state_) {
        return false;
    }
    state_->ensure_owner();
    const auto callback = state_->blur;
    return callback && callback();
}

void CheckboxGroup(CheckboxGroupProps props, std::optional<CheckboxGroupContent> content) {
    detail::CheckboxGroupPropsAccess::mount(props, content);
}

void Radio(RadioProps props, std::optional<RadioLabel> label) {
    detail::RadioPropsAccess::mount(props, label);
}

void RadioGroup(RadioGroupProps props) {
    detail::RadioGroupPropsAccess::mount(props);
}
} // namespace ryn
