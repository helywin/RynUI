#include "component/input_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
namespace {
thread_local InputComponentHost* active_input_host{};
constexpr auto text_dirty = runtime::DirtyFlags::Text | runtime::DirtyFlags::Measure
    | runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry;
struct InputState {
    MountedInputComponent mounted;
    bool controlled{}, disabled{}, read_only{}, focused{};
    ControlSize size{ControlSize::Middle};
    InputStatus status{InputStatus::Default};
    String placeholder;
    std::function<void(String)> on_change, on_submit;
    runtime::NodeId viewport;
    TextSceneId text_scene;
    layout::InputContentLayout layout;
    runtime::SemanticTypography typography;
    Signal<runtime::SemanticTypography> slot_typography{runtime::SemanticTypography{}};
    Signal<runtime::SemanticForeground> slot_foreground{runtime::SemanticForeground{0, 0, 0, 1}};
    theme_runtime::Subscription theme_subscription;
    InputLayoutSnapshot geometry;
    std::optional<animation::AnimationTime> caret_deadline;
    InputDisplayState display;
    text::TextCaretMap carets;
    std::uint64_t measured_value_revision{};
};
struct InputSlotState {};
void check(input::TextEditResult result) {
    if(!result) throw std::runtime_error("Input editor update failed");
}
void validate(ControlSize value) {
    if(value != ControlSize::Small && value != ControlSize::Middle && value != ControlSize::Large)
        throw std::invalid_argument("Invalid Input size");
}
void validate(InputStatus value) {
    if(value != InputStatus::Default && value != InputStatus::Warning && value != InputStatus::Error)
        throw std::invalid_argument("Invalid Input status");
}
}

struct InputPropsAccess {
    static void mount(InputComponentHost& owner, const InputProps& props,
        const std::optional<InputPrefix>& prefix, const std::optional<InputSuffix>& suffix) {
        // Resolve and validate before allocating component, editor or interaction identities.
        if(props.value_ && props.default_value_)
            throw std::invalid_argument("Input value and defaultValue are mutually exclusive");
        const auto initial = props.value_ ? read_prop(*props.value_)
            : props.default_value_.value_or(String{});
        const auto size = read_prop(props.size_);
        const auto status = read_prop(props.status_);
        validate(size); validate(status);
        const auto disabled = read_prop(props.disabled_);
        const auto read_only = read_prop(props.read_only_);
        const input::TextEditorLimits limits{props.max_length_ ? read_prop(*props.max_length_)
            : std::numeric_limits<std::size_t>::max()};
        auto& host = *owner.host_;
        auto& build = runtime::require_component_build_context();
        const auto component = build.mount_component<InputState>();
        auto& state = build.state<InputState>(component);
        state.mounted.component = component;
        state.mounted.node = build.root(component);
        state.controlled = props.value_.has_value();
        state.size = size; state.status = status;
        state.disabled = disabled; state.read_only = read_only;
        state.on_change = props.on_change_; state.on_submit = props.on_submit_;
        // Install cleanup before subsequent resource acquisition can fail.
        build.on_resource_cleanup(component, [&owner, component] {
            auto& host = *owner.host_;
            if(auto* current = host.components().state<InputState>(component)) {
                const auto mounted = current->mounted;
                host.focus().cancel_interaction(mounted.interaction);
                host.pointer().cancel_interaction(mounted.interaction);
                static_cast<void>(host.interactions().remove(mounted.interaction));
                static_cast<void>(owner.editors_.destroy(mounted.editor));
                static_cast<void>(host.text().scene_service().destroy(current->text_scene));
                static_cast<void>(host.layout().remove_intrinsic_measure(current->viewport));
                static_cast<void>(host.layout().remove_layout(mounted.node));
                std::erase_if(owner.mounted_, [component](const auto& item) { return item.component == component; });
            }
        });
        state.mounted.editor = owner.editors_.create(initial.bytes(), limits);
        owner.editors_.require(state.mounted.editor).set_eligibility(disabled, read_only);
        std::optional<input::InteractionId> parent;
        for(auto ancestor = host.components().parent(component); ancestor && !parent;
            ancestor = host.components().parent(*ancestor)) {
            for(const auto interaction : host.interactions().declaration_order()) {
                if(const auto* record = host.interactions().find(interaction);
                    record && record->component == *ancestor) { parent = interaction; break; }
            }
        }
        state.mounted.interaction = host.interactions().create(
            {component, state.mounted.node, parent, !disabled, true, {}});
        input::FocusHandlers handlers;
        handlers.state_changed = [&owner, component](input::FocusPresentation focus) {
            if(auto* current = owner.host_->components().state<InputState>(component)) {
                const bool was_focused = current->focused;
                current->focused = focus.focused;
                if(!focus.focused) current->caret_deadline.reset();
                if(focus.focused) static_cast<void>(owner.sessions_.focus(current->mounted.editor));
                else if(was_focused)
                    static_cast<void>(owner.sessions_.blur());
                owner.invalidate(component, runtime::DirtyFlags::Material);
            }
        };
        // Enter submission is routed separately from Button's Space/Enter activation.
        handlers.activation_allowed = [] { return false; };
        host.interactions().set_focus_handlers(state.mounted.interaction, std::move(handlers));
        state.layout.prefix = prefix.has_value(); state.layout.suffix = suffix.has_value();
        build.mount_slot(component, Content{[&] {
            auto& slots = runtime::require_component_build_context();
            const auto make_slot = [&] {
                const auto slot = slots.mount_component<InputSlotState>();
                const auto node = slots.root(slot);
                host.layout().set_layout(node, layout::BoxLayout{});
                slots.on_resource_cleanup(slot, [layout = &host.layout(), node] { static_cast<void>(layout->remove_layout(node)); });
                return slot;
            };
            const auto prefix_component = make_slot();
            if(prefix) slots.mount_slot_with_semantic_text_style(prefix_component, *prefix,
                Prop<runtime::SemanticForeground>{state.slot_foreground},
                Prop<runtime::SemanticTypography>{state.slot_typography});
            const auto editable = make_slot();
            state.viewport = slots.root(editable);
            host.layout().set_layout(state.viewport, layout::LeafLayout{});
            const auto suffix_component = make_slot();
            if(suffix) slots.mount_slot_with_semantic_text_style(suffix_component, *suffix,
                Prop<runtime::SemanticForeground>{state.slot_foreground},
                Prop<runtime::SemanticTypography>{state.slot_typography});
        }});
        owner.update_theme(component);
        owner.update_text(component);
        host.layout().set_intrinsic_measure(state.viewport, 1,
            [&owner, component](layout::Constraints) {
                auto* current = owner.host_->components().state<InputState>(component);
                if(!current) return runtime::Size{};
                auto& scene = owner.host_->text().scene_service();
                if(!scene.synchronize_measurement(current->text_scene, std::numeric_limits<float>::infinity()))
                    throw std::runtime_error("Input intrinsic text measurement failed");
                const auto& measurement = scene.text_state(current->text_scene).measurement();
                return runtime::Size{measurement.width, measurement.height};
            });
        auto& scope = build.scope(component);
        runtime::connect_layout_style(scope, props.layout_, state.mounted.node, host.nodes(), host.dirty());
        const auto connect = [&]<typename T, typename Apply>(const Prop<T>& prop, Apply apply) {
            static_cast<void>(connect_prop(scope, prop, [&owner, component, apply](T value) {
                if(auto* current = owner.host_->components().state<InputState>(component))
                    apply(owner, *current, std::move(value));
            }));
        };
        if(props.value_) connect(*props.value_, [](auto& owner, auto& current, String value) {
            const auto result = owner.editors_.require(current.mounted.editor).reconcile(value.bytes());
            check(result.edit);
            if(result.edit.value_changed) owner.update_text(current.mounted.component);
        });
        connect(props.placeholder_, [](auto& owner, auto& current, String value) {
            if(current.placeholder == value) return;
            current.placeholder = std::move(value);
            if(owner.editors_.require(current.mounted.editor).value().empty())
                owner.update_text(current.mounted.component);
        });
        connect(props.size_, [](auto& owner, auto& current, ControlSize value) {
            validate(value);
            if(current.size == value) return;
            current.size = value;
            owner.update_theme(current.mounted.component);
        });
        connect(props.status_, [](auto& owner, auto& current, InputStatus value) {
            validate(value);
            if(current.status == value) return;
            current.status = value;
            owner.invalidate(current.mounted.component, runtime::DirtyFlags::Material);
        });
        const auto eligibility = [](auto& owner, auto& current) {
            if(current.disabled || current.read_only) current.caret_deadline.reset();
            owner.editors_.require(current.mounted.editor).set_eligibility(current.disabled, current.read_only);
            owner.host_->interactions().set_eligible(current.mounted.interaction, !current.disabled);
            if(current.disabled) owner.host_->pointer().cancel_interaction(current.mounted.interaction);
            owner.host_->focus().synchronize();
            owner.invalidate(current.mounted.component, runtime::DirtyFlags::Material | runtime::DirtyFlags::HitTest);
        };
        connect(props.disabled_, [eligibility](auto& owner, auto& current, bool value) {
            if(current.disabled == value) return;
            current.disabled = value; eligibility(owner, current);
        });
        connect(props.read_only_, [eligibility](auto& owner, auto& current, bool value) {
            if(current.read_only == value) return;
            current.read_only = value; eligibility(owner, current);
        });
        if(props.max_length_) connect(*props.max_length_, [](auto& owner, auto& current, std::size_t value) {
            auto& editor = owner.editors_.require(current.mounted.editor);
            if(editor.limits().max_scalars == value) return;
            auto limits = editor.limits(); limits.max_scalars = value;
            const auto result = editor.set_limits(limits); check(result);
            if(result.value_changed) owner.update_text(current.mounted.component);
        });
        const auto theme = build.theme_scope();
        state.theme_subscription = theme->capture(
            [&owner, component](theme_runtime::DirtyPhase) { owner.update_theme(component); },
            [theme] { static_cast<void>(theme->map()); static_cast<void>(theme->text_font_family());
                static_cast<void>(theme->text_font_weight()); static_cast<void>(theme->text_font_size());
                static_cast<void>(theme->text_line_height()); static_cast<void>(theme->text_color());
                static_cast<void>(theme->line_width()); });
        owner.mounted_.push_back(state.mounted);
        owner.invalidate(component, text_dirty | runtime::DirtyFlags::HitTest);
    }
};

InputComponentHost::InputComponentHost(ButtonComponentHost& host, input::TextInputPlatform& platform,
    input::TextClipboard& clipboard)
    : host_(&host), sessions_(editors_, platform), clipboard_(editors_, clipboard) { host_->attach_auxiliary(*this); }
InputComponentHost::~InputComponentHost() { dispose(); host_->detach_auxiliary(*this); }
void InputComponentHost::mount(const Content& content) {
    struct Restore { InputComponentHost* previous; ~Restore() { active_input_host = previous; } } restore{active_input_host};
    active_input_host = this;
    host_->mount(content);
}
void InputComponentHost::dispose() noexcept {
    while(!mounted_.empty()) {
        const auto id = mounted_.back().component;
        if(!host_->destroy(id)) mounted_.pop_back();
    }
}
void InputComponentHost::set_window_active(bool active) {
    if(!active) for(const auto& mounted : mounted_) {
        if(auto* state = host_->components().state<InputState>(mounted.component)) state->caret_deadline.reset();
    }
    static_cast<void>(sessions_.set_window_active(active));
    host_->set_window_active(active);
}
void InputComponentHost::invalidate(runtime::ComponentId component, runtime::DirtyFlags flags) {
    if(auto* current = host_->components().state<InputState>(component))
        host_->dirty().invalidate(current->mounted.node, flags);
}
void InputComponentHost::update_theme(runtime::ComponentId component) {
    auto* state = host_->components().state<InputState>(component);
    if(!state) return;
    const auto& theme = host_->components().theme_scope(component)->snapshot();
    const auto& map = theme.map();
    auto model = state->layout;
    model.control_height = state->size == ControlSize::Small ? map.control_height_small
        : state->size == ControlSize::Large ? map.control_height_large : map.control_height;
    model.border_width = theme.seed().line_width;
    model.padding_inline = std::max(0.0F,
        (state->size == ControlSize::Small ? map.size_xs * 0.5F : map.size_small) - model.border_width);
    model.gap = map.size_xs * 0.5F;
    if(!state->text_scene.valid() || model != state->layout) {
        state->layout = model;
        host_->layout().set_layout(state->mounted.node, model);
        invalidate(component, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout
            | runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    }
    const bool large = state->size == ControlSize::Large;
    runtime::SemanticTypography typography{theme.text().font_family, theme.text().font_weight,
        large ? theme.text().font_size * map.font_size_large / map.font_size : theme.text().font_size,
        large ? theme.text().line_height * map.line_height_large / map.line_height : theme.text().line_height};
    auto& scene = host_->text().scene_service();
    if(!state->text_scene.valid()) {
        state->text_scene = scene.create(state->viewport, String{}, host_->text().resolve_fonts(typography),
            static_cast<std::uint32_t>(std::lround(typography.font_size)),
            {typography.line_height, std::numeric_limits<float>::infinity()});
        state->typography = typography;
    } else if(state->typography != typography) {
        scene.set_font_chain(state->text_scene, host_->text().resolve_fonts(typography));
        scene.set_pixel_size(state->text_scene, static_cast<std::uint32_t>(std::lround(typography.font_size)));
        scene.set_line_height(state->text_scene, typography.line_height);
        state->typography = typography;
        const auto revisions = scene.revisions(state->text_scene);
        host_->layout().set_intrinsic_revision(state->viewport, revisions.content + revisions.layout);
        invalidate(component, text_dirty);
    }
    state->slot_typography.set(typography);
    const auto color = theme.text().color;
    state->slot_foreground.set({color.red(), color.green(), color.blue(), color.alpha()});
}
void InputComponentHost::update_text(runtime::ComponentId component, bool measure_layout) {
    auto* state = host_->components().state<InputState>(component);
    if(!state || !state->text_scene.valid()) return;
    const auto& editor = editors_.require(state->mounted.editor);
    const auto changed = state->display.update(editor, state->placeholder.view());
    auto& scene = host_->text().scene_service();
    if(changed.text_changed && scene.text_state(state->text_scene).content().bytes() != state->display.snapshot().text) {
        scene.set_content(state->text_scene, String::from_utf8(state->display.snapshot().text).value());
        const auto revisions = scene.revisions(state->text_scene);
        host_->layout().set_intrinsic_revision(state->viewport, revisions.content + revisions.layout);
        invalidate(component, measure_layout ? text_dirty : runtime::DirtyFlags::Geometry);
    }
    if(changed.geometry_changed) invalidate(component, runtime::DirtyFlags::Geometry);
    if(measure_layout && state->measured_value_revision != editor.revision()) {
        state->measured_value_revision = editor.revision();
        invalidate(component, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry);
    }
}
InputLayoutSnapshot InputComponentHost::layout_snapshot(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if(!state) throw std::out_of_range("Input component is stale");
    return state->geometry;
}
TextSceneId InputComponentHost::text_scene(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if(!state) throw std::out_of_range("Input component is stale");
    return state->text_scene;
}
InputDisplaySnapshot InputComponentHost::display_snapshot(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if(!state) throw std::out_of_range("Input component is stale");
    return state->display.snapshot();
}
const text::TextCaretMap& InputComponentHost::caret_map(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if(!state) throw std::out_of_range("Input component is stale");
    return state->carets;
}
bool InputComponentHost::set_caret_deadline(runtime::ComponentId component,
    std::optional<animation::AnimationTime> deadline) {
    auto* state = host_->components().state<InputState>(component);
    if(!state || (deadline && (!state->focused || state->disabled || state->read_only
        || !host_->focus().state().window_active))) return false;
    if(state->caret_deadline == deadline) return false;
    state->caret_deadline = deadline;
    return true;
}
std::optional<animation::AnimationTime> InputComponentHost::next_caret_deadline() const {
    std::optional<animation::AnimationTime> next;
    for(const auto& mounted : mounted_) {
        const auto* state = host_->components().state<InputState>(mounted.component);
        if(state && state->caret_deadline && (!next || *state->caret_deadline < *next)) next = state->caret_deadline;
    }
    return next;
}
void InputComponentHost::set_horizontal_scroll(runtime::ComponentId component, float offset) {
    if(!std::isfinite(offset)) throw std::invalid_argument("Input scroll must be finite");
    auto* state = host_->components().state<InputState>(component);
    if(!state) return;
    const auto next = std::clamp(offset, 0.0F, std::max(0.0F, state->geometry.text_width - state->geometry.viewport.width));
    if(next == state->geometry.scroll_offset) return;
    state->geometry.scroll_offset = next;
    invalidate(component, runtime::DirtyFlags::Geometry);
}
void InputComponentHost::synchronize_auxiliary_geometry(runtime::Size, runtime::Rect clip) {
    for(const auto& mounted : mounted_) {
        auto* state = host_->components().state<InputState>(mounted.component);
        if(!state) continue;
        update_text(mounted.component, false);
        auto& text_scene = host_->text().scene_service();
        if(state->carets.revision() != text_scene.text_state(state->text_scene).revision()
            && !text_scene.synchronize_caret_map(state->text_scene, state->carets))
                throw std::runtime_error("Input caret mapping failed");
        const auto& node = host_->nodes().require(state->viewport);
        auto viewport = node.bounds;
        // Include parent translations; bounds themselves are absolute layout coordinates.
        for(auto ancestor = std::optional{state->viewport}; ancestor;
            ancestor = host_->nodes().require(*ancestor).parent) {
            const auto translation = host_->nodes().require(*ancestor).translation;
            viewport.x += translation.x; viewport.y += translation.y;
        }
        const auto right = std::min(viewport.x + viewport.width, clip.x + clip.width);
        const auto bottom = std::min(viewport.y + viewport.height, clip.y + clip.height);
        const auto left = std::max(viewport.x, clip.x), top = std::max(viewport.y, clip.y);
        const auto& measurement = host_->text().scene_service().text_state(state->text_scene).measurement();
        state->geometry.viewport = viewport;
        state->geometry.clip = {left, top, std::max(0.0F, right - left), std::max(0.0F, bottom - top)};
        state->geometry.baseline = viewport.y + measurement.first_baseline;
        state->geometry.text_width = measurement.width;
        state->geometry.scroll_offset = state->display.scroll_for_caret(state->carets,
            state->carets.revision(), viewport.width, state->geometry.scroll_offset).value();
        const auto display = state->display.snapshot();
        const auto x = [&](std::size_t byte) { return viewport.x + state->carets.at(byte, state->carets.revision()).value().x
            - state->geometry.scroll_offset; };
        state->geometry.caret_x = x(display.caret);
        state->geometry.selection_start = x(display.selection.begin());
        state->geometry.selection_end = x(display.selection.end());
        state->geometry.composition_start = x(display.composition.begin());
        state->geometry.composition_end = x(display.composition.end());
    }
}
void InputComponentHost::notify_change(input::TextInputOwnerId editor_id) {
    const auto found = std::find_if(mounted_.begin(), mounted_.end(), [editor_id](const auto& value) { return value.editor == editor_id; });
    if(found == mounted_.end()) return;
    const auto component = found->component;
    auto* state = host_->components().state<InputState>(component);
    auto* editor = editors_.find(editor_id);
    if(!state || !editor) return;
    update_text(component);
    // Own both callback and value across synchronous Signal echoes or self-unmount.
    auto callback = state->on_change;
    if(!callback) return;
    auto value = String::from_utf8(editor->value()).value();
    check(editor->note_emitted_value());
    callback(std::move(value));
}
input::TextEditResult InputComponentHost::dispatch(const input::TextCommitted& event) {
    auto result = sessions_.dispatch(event);
    if(result.value_changed) notify_change(event.session.owner);
    else if(result) for(const auto& mounted : mounted_)
        if(mounted.editor == event.session.owner) update_text(mounted.component, false);
    return result;
}
input::TextEditResult InputComponentHost::dispatch(const input::CompositionChanged& event) {
    auto result = sessions_.dispatch(event);
    if(result) for(const auto& mounted : mounted_) if(mounted.editor == event.session.owner) update_text(mounted.component, false);
    return result;
}
input::TextEditResult InputComponentHost::dispatch(const input::CandidatesChanged& event) { return sessions_.dispatch(event); }
void InputComponentHost::submit(runtime::ComponentId component) {
    const auto* state = host_->components().state<InputState>(component);
    if(!state || state->disabled || state->read_only) return;
    auto& editor = editors_.require(state->mounted.editor);
    if(editor.composition().active) return;
    editor.break_history_merge();
    auto callback = state->on_submit;
    if(callback) { auto value = String::from_utf8(editor.value()).value(); callback(std::move(value)); }
}
} // namespace ryn::detail

namespace ryn {
void Input(InputProps props, std::optional<InputPrefix> prefix, std::optional<InputSuffix> suffix) {
    if(!detail::active_input_host) throw std::logic_error("Input requires an active InputComponentHost");
    detail::InputPropsAccess::mount(*detail::active_input_host, props, prefix, suffix);
}
}
