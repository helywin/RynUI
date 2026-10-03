#include "component/input_component.hpp"
#include "component/input_affix_action.hpp"
#include "component/input_material_transition.hpp"
#include "component/input_caret_blink.hpp"
#include "component/space_compact.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "theme/input_tokens.hpp"
#include "component/otp_input_cell.hpp"
#include <ryn/password.hpp>
#include <ryn/text.hpp>
#include <ryn/text_area.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ryn::detail {
struct InputRefState final {
    std::thread::id owner{std::this_thread::get_id()};
    std::optional<runtime::ComponentId> binding;
    std::function<bool(InputFocusOptions)> focus;
    std::function<bool()> blur;
    std::function<bool(std::size_t, std::size_t)> select;

    void ensure_owner() const {
        if (owner != std::this_thread::get_id()) {
            throw std::logic_error("InputRef requires its owner thread");
        }
    }
};

namespace {
thread_local InputComponentHost* active_input_host{};
constexpr auto text_dirty = runtime::DirtyFlags::Text | runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                            runtime::DirtyFlags::Geometry;

struct InputContainerPresentation {
    runtime::Rect bounds;
    runtime::Rect clip;
    float radius{};
    float border_width{};
    Color background;
    Color border;
    ShadowList shadows;
    std::array<Color, input_shadow_layer_capacity> shadow_colors;
    float shadow_opacity{};
    std::array<bool, 4> corners{true, true, true, true};
    InputVariant variant{InputVariant::Outlined};
    Color focus_color;
    float focus_width{};
    bool focus_visible{};
    friend bool operator==(const InputContainerPresentation&, const InputContainerPresentation&) = default;
};

struct TextAreaState {
    std::size_t rows{4};
    TextAreaAutoSize auto_size;
    bool wrap{true};
    TextAreaResize resize{TextAreaResize::Vertical};
    std::function<void(TextAreaSize)> on_resize;
    runtime::Size clear_size;
    runtime::Size count_size;
    float footer_height{};
    float vertical_scroll{};
    std::optional<float> resized_width;
    std::optional<float> resized_height;
    std::optional<float> preferred_x;
    bool reveal_caret{true};
    std::optional<input::PointerIdentity> resizing_pointer;
    runtime::Point resize_start;
    TextAreaSize resize_initial;
    std::optional<TextAreaSize> published_size;
};

struct InputState {
    MountedInputComponent mounted;
    bool controlled{};
    bool disabled{};
    bool read_only{};
    bool focused{};
    bool focus_visible{};
    bool active{true};
    std::optional<runtime::SemanticTypography> inherited_typography;
    std::function<void(String)> on_internal_commit;
    std::function<void(String)> on_internal_blur;
    std::function<void()> on_internal_cancel;
    bool password{};
    bool visible{};
    std::optional<input::TextInputProperties> session_properties;
    InputPurpose purpose{InputPurpose::Text};
    InputCapitalization capitalization{InputCapitalization::None};
    bool autocorrect{true};
    std::shared_ptr<InputRefState> reference;
    bool allow_clear{};
    bool clear_disabled{};
    std::function<void()> on_clear;
    bool custom_suffix{};
    Signal<bool> clear_visible{false};
    bool show_count{};
    InputCountOptions count_options;
    std::optional<std::size_t> hard_max;
    std::function<std::size_t(StringView)> count_strategy;
    std::function<String(InputCountInfo)> count_formatter;
    std::function<String(String, std::size_t)> exceed_formatter;
    Signal<String> count_text{String{}};
    runtime::NodeId count_node;
    std::size_t count_value{};
    bool count_exceeded{};
    std::optional<std::uint64_t> count_revision;
    std::uint64_t count_generation{};
    bool counting{};
    bool count_ready{};
    bool search_control_height{};
    std::shared_ptr<void> password_lifetime;
    std::size_t hovering_pointers{};
    std::optional<input::PointerIdentity> selecting_pointer;
    runtime::Point last_selection_position;
    ControlSize size{ControlSize::Middle};
    InputStatus status{InputStatus::Default};
    InputVariant variant{InputVariant::Outlined};
    String placeholder;
    std::function<void(String)> on_change;
    std::function<void(String)> on_submit;
    std::function<void()> on_focus;
    std::function<void()> on_blur;
    runtime::NodeId viewport;
    TextSceneId text_scene;
    TextSceneId selected_scene;
    TextSceneId placeholder_scene;
    runtime::SceneFragmentId text_fragment;
    std::vector<graphics::SceneDrawCommand> text_commands;
    std::vector<graphics::SceneDrawCommand> pending_text_commands;
    runtime::SceneFragmentId container_fragment;
    component::RetainedSurfaceId selection_surface;
    component::RetainedSurfaceId overlay_surface;
    // Both shadow kinds retain all slots; changing a typed list never changes topology.
    std::vector<graphics::RoundedEffectId> container_effects;
    std::optional<CompactMetadata> compact;
    std::weak_ptr<CompactContext> compact_context;
    std::optional<InputContainerPresentation> container_presentation;
    std::vector<graphics::SceneDrawCommand> container_commands;
    std::optional<runtime::Rect> container_clip;
    runtime::Rect next_container_clip;
    layout::InputContentLayout layout;
    runtime::SemanticTypography typography;
    Signal<runtime::SemanticTypography> slot_typography{runtime::SemanticTypography{}};
    Signal<runtime::SemanticForeground> slot_foreground{runtime::SemanticForeground{0, 0, 0, 1}};
    theme_runtime::Subscription theme_subscription;
    InputLayoutSnapshot geometry;
    InputCaretBlink caret_blink;
    InputDisplayState display;
    std::unique_ptr<InputMaterialTransition> transition;
    text::TextCaretMap carets;
    text::TextCaretAffinity affinity{text::TextCaretAffinity::Downstream};
    std::vector<TextCoverageClip> selected_clips;
    std::vector<graphics::QuadInstance> selection_quads;
    std::vector<graphics::QuadInstance> overlay_quads;
    std::uint64_t measured_value_revision{};
    std::unique_ptr<TextAreaState> textarea;
    std::shared_ptr<OTPInputCellConfig> otp;
    String mask_glyph{u8"•"};
};

struct InputSlotState {};

struct InputVisuals {
    Color background;
    Color border;
    Color foreground;
    Color affix;
    Color caret;
    const ShadowList* shadow{};
    bool shadow_visible{};
};

input::TextInputProperties input_properties(const InputState& state) noexcept {
    input::TextInputProperties result;
    result.type = static_cast<input::TextInputType>(state.purpose);
    result.capitalization = static_cast<input::TextCapitalization>(state.capitalization);
    result.autocorrect = state.autocorrect;
    if (state.password) {
        result.type = state.visible ? input::TextInputType::password_visible : input::TextInputType::password_hidden;
        result.autocorrect = false;
    }
    return result;
}

InputVisuals resolve_visuals(const InputState& state, const InputTokenSet& tokens) {
    const auto& colors = tokens.colors;
    const bool error =
        state.status == InputStatus::Error || (state.status == InputStatus::Default && state.count_exceeded);
    const bool warning = state.status == InputStatus::Warning;
    const Color status_color = error ? colors.error_border : warning ? colors.warning_border : colors.border;
    const Color hover_border = error     ? colors.error_hover_border
                               : warning ? colors.warning_hover_border
                                         : colors.hover_border;
    const Color active_border = error || warning ? status_color : colors.active_border;
    const Color foreground = state.disabled ? colors.disabled_foreground : colors.foreground;
    InputVisuals result{
        state.disabled            ? colors.disabled_background
        : state.focused           ? colors.active_background
        : state.hovering_pointers ? colors.hover_background
                                  : colors.background,
        state.disabled            ? (error || warning ? status_color : colors.disabled_border)
        : state.focused           ? active_border
        : state.hovering_pointers ? hover_border
                                  : status_color,
        foreground,
        state.disabled     ? foreground
        : error || warning ? status_color
                           : foreground,
        error     ? colors.error_caret
        : warning ? colors.warning_caret
                  : colors.caret,
        error     ? &tokens.error_active_shadow
        : warning ? &tokens.warning_active_shadow
                  : &tokens.active_shadow,
        state.focused && !state.disabled,
    };
    const Color transparent{0, 0, 0, 0};
    if (state.variant == InputVariant::Borderless) {
        result.background = transparent;
        result.foreground = error || warning ? status_color : foreground;
        result.affix = result.foreground;
        result.shadow_visible = false;
    } else if (state.variant == InputVariant::Filled) {
        result.background =
            state.disabled  ? colors.disabled_background
            : state.focused ? colors.active_background
            : error         ? (state.hovering_pointers ? colors.error_hover_background : colors.error_background)
            : warning       ? (state.hovering_pointers ? colors.warning_hover_background : colors.warning_background)
            : state.hovering_pointers ? colors.filled_hover_background
                                      : colors.filled_background;
        result.border = state.disabled ? colors.disabled_border : state.focused ? active_border : transparent;
        result.foreground = state.disabled ? colors.disabled_foreground
                            : error        ? colors.error_foreground
                            : warning      ? colors.warning_foreground
                                           : foreground;
        result.shadow_visible = false;
    } else if (state.variant == InputVariant::Underlined) {
        if (state.disabled) {
            result.background = colors.background;
        }
        result.shadow_visible = false;
    }
    return result;
}

std::array<float, 4> channels(Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

InputMaterialValues material_values(const InputState& state, const InputTokenSet& tokens) {
    const auto visual = resolve_visuals(state, tokens);
    InputMaterialValues result;
    result.colors[0] = visual.background;
    result.colors[1] = visual.border;
    result.colors[2] = visual.foreground;
    result.colors[3] = visual.affix;
    result.colors[4] = visual.caret;
    result.colors[5] = tokens.colors.placeholder;
    result.colors[6] = tokens.colors.selection_background;
    result.colors[7] = tokens.colors.selection_foreground;
    for (std::size_t i = 0; i < input_shadow_layer_capacity; ++i) {
        result.colors[8 + i] = i < visual.shadow->size() ? (*visual.shadow)[i].color : Color(0, 0, 0, 0);
    }
    result.shadow_opacity = visual.shadow_visible ? 1.0F : 0.0F;
    return result;
}

runtime::Rect translated_bounds(const runtime::NodeStore& nodes, runtime::NodeId id) {
    const auto& node = nodes.require(id);
    auto result = node.bounds;
    // Layout bounds are absolute, and scene translation is stored per node.
    result.x += node.translation.x;
    result.y += node.translation.y;
    return result;
}

graphics::QuadInstance clipped_quad(runtime::Rect bounds, runtime::Rect clip, runtime::Size, std::array<float, 4> color,
                                    float opacity) {
    bounds = graphics::intersect_effect_bounds(bounds, clip);
    return {{bounds.x, bounds.y, bounds.width, bounds.height}, color, opacity};
}

runtime::Rect pixel_clip(runtime::Rect clip, float scale) {
    const float left = std::ceil(clip.x * scale) / scale;
    const float top = std::ceil(clip.y * scale) / scale;
    const float right = std::floor((clip.x + clip.width) * scale) / scale;
    const float bottom = std::floor((clip.y + clip.height) * scale) / scale;
    return {left, top, std::max(0.0F, right - left), std::max(0.0F, bottom - top)};
}

void check(bool result) {
    if (!result) {
        throw std::runtime_error("Input visual coverage mapping failed");
    }
}

void check(input::TextEditResult result) {
    if (!result) {
        throw std::runtime_error("Input editor update failed");
    }
}

void validate(ControlSize value) {
    if (value != ControlSize::Small && value != ControlSize::Middle && value != ControlSize::Large) {
        throw std::invalid_argument("Invalid Input size");
    }
}

void validate(InputStatus value) {
    if (value != InputStatus::Default && value != InputStatus::Warning && value != InputStatus::Error) {
        throw std::invalid_argument("Invalid Input status");
    }
}

void validate(InputPurpose value) {
    if (value < InputPurpose::Text || value > InputPurpose::Number) {
        throw std::invalid_argument("Invalid Input purpose");
    }
}

void validate(InputCapitalization value) {
    if (value < InputCapitalization::None || value > InputCapitalization::Letters) {
        throw std::invalid_argument("Invalid Input capitalization");
    }
}

void validate(InputVariant value) {
    if (value < InputVariant::Outlined || value > InputVariant::Underlined) {
        throw std::invalid_argument("Invalid Input variant");
    }
}

void validate(InputCountOptions value) {
    if (value.unit < InputCountUnit::Scalar || value.unit > InputCountUnit::Grapheme) {
        throw std::invalid_argument("Invalid Input count unit");
    }
}

std::size_t count_value(StringView value, InputCountUnit unit, const std::function<std::size_t(StringView)>& strategy) {
    if (strategy) {
        return strategy(value);
    }
    input::TextBoundaryMap boundaries;
    if (!boundaries.assign(value.bytes())) {
        throw std::invalid_argument("Invalid Input count value");
    }
    return unit == InputCountUnit::Grapheme ? boundaries.grapheme_count() : boundaries.scalar_count();
}
} // namespace

struct InputPropsAccess {
    static void mount(InputComponentHost& owner, const InputProps& props, const std::optional<InputPrefix>& prefix,
                      const std::optional<InputSuffix>& suffix) {
        // Resolve and validate before allocating component, editor or interaction identities.
        if (props.common_.value_ && props.common_.default_value_) {
            throw std::invalid_argument("Input value and defaultValue are mutually exclusive");
        }
        const auto initial =
            props.common_.value_ ? read_prop(*props.common_.value_) : props.common_.default_value_.value_or(String{});
        if (props.otp_) {
            validate_otp_mask(read_prop(props.otp_->mask));
        }
        if (props.textarea_) {
            const auto rows = read_prop(props.textarea_->rows);
            const auto autosize = read_prop(props.textarea_->auto_size);
            const auto resize = read_prop(props.textarea_->resize);
            if (rows == 0 || autosize.min_rows == 0 || (autosize.max_rows && *autosize.max_rows < autosize.min_rows) ||
                resize < TextAreaResize::None || resize > TextAreaResize::Both) {
                throw std::invalid_argument("Invalid TextArea sizing configuration");
            }
        }
        auto& build = runtime::require_component_build_context();
        const auto compact = nearest_compact(build);
        const auto size =
            compact && !props.common_.explicit_size_ ? compact->metadata.size : read_prop(props.common_.size_);
        const auto status = read_prop(props.common_.status_);
        const auto variant = read_prop(props.common_.variant_);
        validate(size);
        validate(status);
        validate(variant);
        const auto count_options = read_prop(props.common_.count_);
        validate(count_options);
        const auto purpose = read_prop(props.common_.purpose_);
        const auto capitalization = read_prop(props.common_.capitalization_);
        validate(purpose);
        validate(capitalization);
        if (props.common_.reference_) {
            props.common_.reference_->ensure_owner();
            if (props.common_.reference_->binding) {
                throw std::invalid_argument("InputRef is already bound to a live Input");
            }
        }
        const auto disabled = read_prop(props.common_.disabled_);
        const auto read_only = read_prop(props.common_.read_only_);
        const input::TextEditorLimits limits{props.common_.max_length_ ? read_prop(*props.common_.max_length_)
                                                                       : std::numeric_limits<std::size_t>::max()};
        auto& host = *owner.host_;
        const auto component = build.mount_component<InputState>();
        auto& state = build.state<InputState>(component);
        state.mounted.component = component;
        state.mounted.node = build.root(component);
        state.otp = props.otp_;
        if (props.textarea_) {
            state.textarea = std::make_unique<TextAreaState>();
            state.textarea->rows = read_prop(props.textarea_->rows);
            state.textarea->auto_size = read_prop(props.textarea_->auto_size);
            state.textarea->wrap = read_prop(props.textarea_->wrap);
            state.textarea->resize = read_prop(props.textarea_->resize);
            state.textarea->on_resize = props.textarea_->on_resize;
        }
        if (compact) {
            compact->claim(component);
            state.compact = compact->metadata;
            state.compact_context = compact;
            build.on_resource_cleanup(component, [compact, component] { compact->detach(component); });
        }
        state.controlled = props.common_.value_.has_value();
        state.size = size;
        state.status = status;
        state.variant = variant;
        state.disabled = disabled;
        state.read_only = read_only;
        state.password = props.password_visible_.has_value();
        state.visible = props.password_visible_ ? read_prop(*props.password_visible_) : true;
        if (state.otp) {
            const auto mask = read_prop(state.otp->mask);
            state.password = mask.enabled;
            state.visible = !mask.enabled;
            state.mask_glyph = mask.glyph;
        }
        state.allow_clear = props.common_.allow_clear_ && read_prop(*props.common_.allow_clear_);
        state.clear_disabled = read_prop(props.common_.clear_disabled_);
        state.on_clear = props.common_.on_clear_;
        state.custom_suffix = props.suffix_presence_ ? read_prop(*props.suffix_presence_) : suffix.has_value();
        state.search_control_height = props.common_.search_control_height_;
        state.show_count = props.common_.show_count_ && read_prop(*props.common_.show_count_);
        state.count_options = count_options;
        state.hard_max =
            props.common_.max_length_ ? std::optional{read_prop(*props.common_.max_length_)} : std::nullopt;
        state.count_strategy = props.common_.count_strategy_;
        state.count_formatter = props.common_.count_formatter_;
        state.exceed_formatter = props.common_.exceed_formatter_;
        state.clear_visible.set(state.allow_clear && !initial.empty() && !disabled && !read_only);
        state.password_lifetime = props.password_lifetime_;
        state.on_change = props.common_.on_change_;
        state.on_submit = props.common_.on_submit_;
        state.on_focus = props.common_.on_focus_;
        state.on_blur = props.common_.on_blur_;
        state.purpose = purpose;
        state.capitalization = capitalization;
        state.autocorrect = read_prop(props.common_.autocorrect_);
        state.reference = props.common_.reference_;
        // Install cleanup before subsequent resource acquisition can fail.
        build.on_resource_cleanup(component, [&owner, component] {
            auto& host = *owner.host_;
            if (auto* current = host.components().state<InputState>(component)) {
                if (const auto reference = current->reference; reference && reference->binding == component) {
                    reference->binding.reset();
                    reference->focus = {};
                    reference->blur = {};
                    reference->select = {};
                }
                std::erase(owner.auto_focus_requests_, component);
                current->transition.reset();
                const auto mounted = current->mounted;
                host.focus().cancel_interaction(mounted.interaction);
                host.pointer().cancel_interaction(mounted.interaction);
                static_cast<void>(host.interactions().remove(mounted.interaction));
                static_cast<void>(owner.editors_.destroy(mounted.editor));
                static_cast<void>(host.surfaces().destroy_content_range(current->selection_surface));
                static_cast<void>(host.surfaces().destroy_content_range(current->overlay_surface));
                for (const auto effect : current->container_effects) {
                    static_cast<void>(host.rounded_effects().remove(effect));
                }
                static_cast<void>(host.scene_composer().remove_fragment(current->container_fragment));
                static_cast<void>(host.scene_composer().remove_fragment(current->text_fragment));
                static_cast<void>(host.text().scene_service().destroy(current->selected_scene));
                static_cast<void>(host.text().scene_service().destroy(current->placeholder_scene));
                static_cast<void>(host.text().scene_service().destroy(current->text_scene));
                static_cast<void>(host.layout().remove_intrinsic_measure(current->viewport));
                static_cast<void>(host.layout().remove_layout(mounted.node));
                std::erase_if(owner.mounted_, [component](const auto& item) { return item.component == component; });
            }
        });
        if (state.reference) {
            // Reserve before prefix/suffix content can reuse the same reference.
            state.reference->binding = component;
        }
        state.mounted.editor = owner.editors_.create(
            initial.bytes(), limits, state.textarea ? input::TextEditMode::MultiLine : input::TextEditMode::SingleLine);
        owner.editors_.require(state.mounted.editor).set_eligibility(disabled, read_only);
        std::optional<input::InteractionId> parent;
        for (auto ancestor = host.components().parent(component); ancestor && !parent;
             ancestor = host.components().parent(*ancestor)) {
            for (const auto interaction : host.interactions().declaration_order()) {
                if (const auto* record = host.interactions().find(interaction);
                    record && record->component == *ancestor) {
                    parent = interaction;
                    break;
                }
            }
        }
        state.mounted.interaction =
            host.interactions().create({component, state.mounted.node, parent, !disabled, true, {}});
        input::InteractionHandlers pointer_handlers;
        pointer_handlers.target = [&owner, component](input::PointerDispatchContext& context) {
            if (auto* current = owner.host_->components().state<InputState>(component)) {
                const auto before = current->hovering_pointers;
                if (context.kind() == input::PointerEventKind::enter) {
                    ++current->hovering_pointers;
                } else if (context.kind() == input::PointerEventKind::leave && current->hovering_pointers) {
                    --current->hovering_pointers;
                }
                if ((before == 0) != (current->hovering_pointers == 0)) {
                    owner.invalidate(component, runtime::DirtyFlags::Material);
                }
                owner.dispatch_pointer(component, context);
            }
        };
        host.interactions().set_handlers(state.mounted.interaction, std::move(pointer_handlers));
        input::FocusHandlers handlers;
        handlers.state_changed = [&owner, component](input::FocusPresentation focus) {
            if (auto* current = owner.host_->components().state<InputState>(component)) {
                const bool was_focused = current->focused;
                current->focused = focus.focused;
                current->focus_visible = focus.focus_visible;
                if (!focus.focused) {
                    current->caret_blink.stop();
                    current->selecting_pointer.reset();
                    owner.host_->pointer().cancel_pointer_interaction(current->mounted.interaction);
                    current->hovering_pointers = 0;
                }
                if (focus.focused) {
                    current->session_properties = input_properties(*current);
                    static_cast<void>(owner.sessions_.focus(current->mounted.editor, *current->session_properties));
                } else if (was_focused) {
                    static_cast<void>(owner.sessions_.blur());
                }
                owner.invalidate(component, runtime::DirtyFlags::Material);
                auto focus_callback = !was_focused && focus.focused ? current->on_focus : std::function<void()>{};
                auto blur_callback = was_focused && !focus.focused ? current->on_blur : std::function<void()>{};
                if (was_focused && !focus.focused && current->active && current->on_internal_blur) {
                    auto callback = current->on_internal_blur;
                    auto value = String::from_utf8(owner.editors_.require(current->mounted.editor).value()).value();
                    callback(std::move(value));
                }
                if (focus_callback) {
                    focus_callback();
                }
                if (blur_callback) {
                    blur_callback();
                }
            }
        };
        // Enter submission is routed separately from Button's Space/Enter activation.
        handlers.activation_allowed = [] {
            return false;
        };
        handlers.text_edit = [&owner, component](const input::KeyboardInputEvent& event) {
            return owner.dispatch_keyboard(component, event);
        };
        host.interactions().set_focus_handlers(state.mounted.interaction, std::move(handlers));
        state.container_fragment =
            build.register_scene_fragment(component, runtime::SceneFragmentPlacement::before_children);
        state.container_effects.resize(input_effect_layer_count * (compact ? 4 : 1));
        for (auto& effect : state.container_effects) {
            effect = host.rounded_effects().add({});
        }
        const auto overlay_fragment =
            build.register_scene_fragment(component, runtime::SceneFragmentPlacement::after_children);
        const std::array<graphics::QuadInstance, 2> empty_overlays{};
        state.overlay_surface = host.surfaces().create_content_range(
            overlay_fragment, state.textarea ? std::span<const graphics::QuadInstance>{} : empty_overlays);
        const auto count_text = state.count_text;
        std::optional<InputSuffix> composed_suffix = suffix;
        if (props.common_.allow_clear_ || props.common_.show_count_) {
            const auto clear_visible = state.clear_visible;
            const bool has_clear = props.common_.allow_clear_.has_value();
            const bool has_count = props.common_.show_count_.has_value();
            composed_suffix =
                InputSuffix{[&owner, component, clear_visible, suffix, count_text, has_clear, has_count,
                             clear_icon = props.common_.clear_icon_, clear_disabled = props.common_.clear_disabled_] {
                    if (has_clear) {
                        mount_input_affix_action(
                            *owner.host_, clear_icon, clear_disabled, [&owner, component] { owner.clear(component); },
                            clear_visible);
                    }
                    if (suffix) {
                        SlotContentAccess::function (*suffix)();
                    }
                    if (!has_count) {
                        return;
                    }
                    auto& build = runtime::require_component_build_context();
                    const auto counter = build.mount_component<InputSlotState>();
                    const auto node = build.root(counter);
                    owner.host_->components().state<InputState>(component)->count_node = node;
                    owner.host_->layout().set_layout(node, layout::BoxLayout{});
                    owner.host_->nodes().require(node).clip_content = true;
                    owner.host_->nodes().require(node).external_layout.width = 0.0F;
                    owner.host_->nodes().require(node).external_layout.height = 0.0F;
                    build.on_resource_cleanup(
                        counter, [host = owner.host_, node] { static_cast<void>(host->layout().remove_layout(node)); });
                    build.mount_slot(counter, Content{[count_text] { Text(TextProps{}.content(count_text)); }});
                }};
        }
        state.layout.prefix = prefix.has_value();
        state.layout.suffix = state.custom_suffix || state.clear_visible.get();
        build.mount_slot(
            component, Content{[&] {
                auto& slots = runtime::require_component_build_context();
                const auto make_slot = [&] {
                    const auto slot = slots.mount_component<InputSlotState>();
                    const auto node = slots.root(slot);
                    host.layout().set_layout(node, layout::BoxLayout{});
                    slots.on_resource_cleanup(
                        slot, [layout = &host.layout(), node] { static_cast<void>(layout->remove_layout(node)); });
                    return slot;
                };
                const auto prefix_component = make_slot();
                if (prefix) {
                    slots.mount_slot_with_semantic_text_style(prefix_component, *prefix,
                                                              Prop<runtime::SemanticForeground>{state.slot_foreground},
                                                              Prop<runtime::SemanticTypography>{state.slot_typography});
                }
                const auto editable = make_slot();
                state.viewport = slots.root(editable);
                const auto selection_fragment =
                    slots.register_scene_fragment(editable, runtime::SceneFragmentPlacement::before_children);
                const std::array<graphics::QuadInstance, 1> empty_selection{};
                state.selection_surface = host.surfaces().create_content_range(
                    selection_fragment, state.textarea ? std::span<const graphics::QuadInstance>{} : empty_selection);
                state.text_fragment =
                    slots.register_scene_fragment(editable, runtime::SceneFragmentPlacement::before_children);
                host.layout().set_layout(state.viewport, layout::LeafLayout{});
                const auto suffix_component = make_slot();
                layout::FlexLayout suffix_layout;
                suffix_layout.align = layout::FlexAlign::center;
                suffix_layout.item_policy = layout::FlexItemPolicy::sequential;
                host.layout().set_layout(slots.root(suffix_component), suffix_layout);
                if (composed_suffix) {
                    slots.mount_slot_with_semantic_text_style(suffix_component, *composed_suffix,
                                                              Prop<runtime::SemanticForeground>{state.slot_foreground},
                                                              Prop<runtime::SemanticTypography>{state.slot_typography});
                }
            }});
        owner.update_theme(component);
        state.transition = std::make_unique<InputMaterialTransition>(
            host.animations(),
            material_values(state, derive_input_tokens(host.components().theme_scope(component)->snapshot())),
            [&owner, component] { owner.apply_material_transition(component); });
        state.selected_scene = host.text().scene_service().create_view(state.text_scene, state.viewport);
        state.placeholder_scene = host.text().scene_service().create_view(state.text_scene, state.viewport);
        owner.configure_count_transform(component);
        owner.update_text(component);
        host.layout().set_intrinsic_measure(
            state.viewport, 1, [&owner, component](layout::Constraints constraints) -> layout::IntrinsicMeasurement {
                auto* current = owner.host_->components().state<InputState>(component);
                if (!current) {
                    return runtime::Size{};
                }
                auto& scene = owner.host_->text().scene_service();
                const auto width = current->textarea && current->textarea->wrap
                                       ? constraints.max_width
                                       : std::numeric_limits<float>::infinity();
                if (!scene.synchronize_measurement(current->text_scene, width)) {
                    throw std::runtime_error("Input intrinsic text measurement failed");
                }
                const auto& measurement = scene.text_state(current->text_scene).measurement();
                return {{measurement.width, measurement.height}, measurement.first_baseline};
            });
        auto& scope = build.scope(component);
        runtime::connect_layout_style(scope, props.common_.layout_, state.mounted.node, host.nodes(), host.dirty());
        const auto connect = [&]<typename T, typename Apply>(const Prop<T>& prop, Apply apply) {
            static_cast<void>(connect_prop(scope, prop, [&owner, component, apply](T value) {
                if (auto* current = owner.host_->components().state<InputState>(component)) {
                    apply(owner, *current, std::move(value));
                }
            }));
        };
        if (props.textarea_) {
            connect(props.textarea_->rows, [](auto& owner, auto& current, std::size_t rows) {
                if (rows == 0) {
                    throw std::invalid_argument("TextArea rows must be positive");
                }
                current.textarea->rows = rows;
                owner.invalidate(current.mounted.component, text_dirty);
            });
            connect(props.textarea_->auto_size, [](auto& owner, auto& current, TextAreaAutoSize value) {
                if (value.min_rows == 0 || (value.max_rows && *value.max_rows < value.min_rows)) {
                    throw std::invalid_argument("Invalid TextArea autoSize bounds");
                }
                current.textarea->auto_size = value;
                current.textarea->resized_height.reset();
                current.textarea->resized_width.reset();
                if (value.enabled) {
                    owner.host_->pointer().cancel_pointer_interaction(current.mounted.interaction);
                    current.textarea->resizing_pointer.reset();
                }
                owner.invalidate(current.mounted.component, text_dirty);
            });
            connect(props.textarea_->wrap, [](auto& owner, auto& current, bool value) {
                current.textarea->wrap = value;
                owner.invalidate(current.mounted.component, text_dirty);
            });
            connect(props.textarea_->resize, [](auto& owner, auto& current, TextAreaResize value) {
                if (value < TextAreaResize::None || value > TextAreaResize::Both) {
                    throw std::invalid_argument("Invalid TextArea resize direction");
                }
                current.textarea->resize = value;
                owner.host_->pointer().cancel_pointer_interaction(current.mounted.interaction);
                current.textarea->resizing_pointer.reset();
                owner.invalidate(current.mounted.component,
                                 runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
            });
        }
        if (props.common_.value_) {
            connect(*props.common_.value_, [](auto& owner, auto& current, String value) {
                const auto result = owner.editors_.require(current.mounted.editor).reconcile(value.bytes());
                check(result.edit);
                if (result.edit.value_changed) {
                    owner.update_text(current.mounted.component);
                }
            });
        }
        if (props.password_visible_) {
            connect(*props.password_visible_, [](auto& owner, auto& current, bool value) {
                if (current.visible == value) {
                    return;
                }
                current.visible = value;
                owner.update_text(current.mounted.component);
            });
        }
        connect(props.common_.placeholder_, [](auto& owner, auto& current, String value) {
            if (current.placeholder == value) {
                return;
            }
            current.placeholder = std::move(value);
            if (owner.editors_.require(current.mounted.editor).value().empty()) {
                owner.update_text(current.mounted.component);
            }
        });
        if (!compact || props.common_.explicit_size_) {
            connect(props.common_.size_, [](auto& owner, auto& current, ControlSize value) {
                validate(value);
                if (current.size == value) {
                    return;
                }
                current.size = value;
                owner.update_theme(current.mounted.component);
            });
        }
        connect(props.common_.status_, [](auto& owner, auto& current, InputStatus value) {
            validate(value);
            if (current.status == value) {
                return;
            }
            current.status = value;
            owner.invalidate(current.mounted.component, runtime::DirtyFlags::Material);
        });
        connect(props.common_.variant_, [](auto& owner, auto& current, InputVariant value) {
            validate(value);
            if (current.variant != value) {
                current.variant = value;
                owner.update_theme(current.mounted.component);
                if (const auto compact = current.compact_context.lock()) {
                    compact->refresh();
                }
            }
        });
        const auto eligibility = [](auto& owner, auto& current) {
            const auto component = current.mounted.component;
            if (current.disabled || current.read_only) {
                current.caret_blink.stop();
            }
            if (current.disabled && current.focused) {
                current.selecting_pointer.reset();
                current.hovering_pointers = 0;
                static_cast<void>(owner.sessions_.blur());
            }
            owner.editors_.require(current.mounted.editor)
                .set_eligibility(current.disabled || !current.active, current.read_only);
            owner.host_->interactions().set_eligible(current.mounted.interaction, current.active && !current.disabled);
            if (current.disabled) {
                owner.host_->pointer().cancel_interaction(current.mounted.interaction);
            }
            owner.host_->focus().synchronize();
            owner.update_clear_visibility(component);
            owner.invalidate(component, runtime::DirtyFlags::Material | runtime::DirtyFlags::HitTest);
        };
        connect(props.common_.disabled_, [eligibility](auto& owner, auto& current, bool value) {
            if (current.disabled == value) {
                return;
            }
            current.disabled = value;
            eligibility(owner, current);
        });
        connect(props.common_.read_only_, [eligibility](auto& owner, auto& current, bool value) {
            if (current.read_only == value) {
                return;
            }
            current.read_only = value;
            eligibility(owner, current);
        });
        connect(props.common_.purpose_, [](auto& owner, auto& current, InputPurpose value) {
            validate(value);
            if (current.purpose != value) {
                current.purpose = value;
                owner.update_text(current.mounted.component, false);
            }
        });
        connect(props.common_.capitalization_, [](auto& owner, auto& current, InputCapitalization value) {
            validate(value);
            if (current.capitalization != value) {
                current.capitalization = value;
                owner.update_text(current.mounted.component, false);
            }
        });
        connect(props.common_.autocorrect_, [](auto& owner, auto& current, bool value) {
            if (current.autocorrect != value) {
                current.autocorrect = value;
                owner.update_text(current.mounted.component, false);
            }
        });
        if (props.common_.allow_clear_) {
            connect(*props.common_.allow_clear_, [](auto& owner, auto& current, bool value) {
                if (current.allow_clear == value) {
                    return;
                }
                current.allow_clear = value;
                owner.update_clear_visibility(current.mounted.component);
            });
        }
        connect(props.common_.clear_disabled_,
                [](auto&, auto& current, bool value) { current.clear_disabled = value; });
        if (props.suffix_presence_) {
            connect(*props.suffix_presence_, [](auto& owner, auto& current, bool value) {
                if (current.custom_suffix != value) {
                    current.custom_suffix = value;
                    owner.update_suffix_layout(current.mounted.component);
                }
            });
        }
        if (props.common_.max_length_) {
            connect(*props.common_.max_length_, [](auto& owner, auto& current, std::size_t value) {
                auto& editor = owner.editors_.require(current.mounted.editor);
                if (editor.limits().max_scalars == value) {
                    return;
                }
                auto limits = editor.limits();
                limits.max_scalars = value;
                current.hard_max = value;
                ++current.count_generation;
                current.count_revision.reset();
                const auto result = editor.set_limits(limits);
                check(result);
                owner.configure_count_transform(current.mounted.component);
                owner.update_text(current.mounted.component);
            });
        }
        if (props.common_.show_count_) {
            connect(*props.common_.show_count_, [](auto& owner, auto& current, bool value) {
                if (current.show_count != value) {
                    current.show_count = value;
                    ++current.count_generation;
                    current.count_revision.reset();
                    owner.update_text(current.mounted.component, false);
                }
            });
        }
        connect(props.common_.count_, [](auto& owner, auto& current, InputCountOptions value) {
            validate(value);
            if (current.count_options != value) {
                current.count_options = value;
                ++current.count_generation;
                current.count_revision.reset();
                owner.configure_count_transform(current.mounted.component);
                owner.update_text(current.mounted.component, false);
            }
        });
        const auto theme = build.theme_scope();
        state.theme_subscription =
            theme->capture([&owner, component](theme_runtime::DirtyPhase) { owner.update_theme(component); },
                           [theme] {
                               static_cast<void>(theme->text_font_family());
                               static_cast<void>(theme->text_font_weight());
                               static_cast<void>(theme->input_layout_metrics());
                               static_cast<void>(theme->input_typography());
                               static_cast<void>(theme->input_border_radius());
                               static_cast<void>(theme->input_colors());
                               static_cast<void>(theme->input_shadows());
                               static_cast<void>(theme->motion_unit());
                               static_cast<void>(theme->motion_base());
                               static_cast<void>(theme->motion_enabled());
                           });
        owner.mounted_.push_back(state.mounted);
        if (state.otp) {
            static_cast<void>(
                connect_prop(build.scope(component), state.otp->mask, [&owner, component](const OTPMask& mask) {
                    validate_otp_mask(mask);
                    if (auto* current = owner.host_->components().state<InputState>(component)) {
                        current->password = mask.enabled;
                        current->visible = !mask.enabled;
                        current->mask_glyph = mask.glyph;
                        owner.update_text(component);
                    }
                }));
        }
        if (state.reference) {
            state.reference->binding = component;
            state.reference->focus = [&owner, component](InputFocusOptions options) {
                return owner.focus(component, options);
            };
            state.reference->blur = [&owner, component] {
                return owner.blur(component);
            };
            state.reference->select = [&owner, component](std::size_t anchor, std::size_t caret) {
                return owner.select(component, anchor, caret);
            };
        }
        if (props.common_.auto_focus_) {
            owner.auto_focus_requests_.push_back(component);
        }
        if (compact) {
            compact->attach(
                component,
                [&owner, component, explicit_size = props.common_.explicit_size_](const CompactMetadata& value) {
                    if (auto* current = owner.host_->components().state<InputState>(component);
                        current && current->compact != value) {
                        current->compact = value;
                        if (!explicit_size && current->size != value.size) {
                            current->size = value.size;
                            owner.update_theme(component);
                        }
                        owner.invalidate(component, runtime::DirtyFlags::Geometry);
                    }
                },
                [&owner, component] {
                    const auto& current = *owner.host_->components().state<InputState>(component);
                    const auto& theme = owner.host_->components().theme_scope(component)->snapshot();
                    const auto bounds = translated_bounds(owner.host_->nodes(), current.mounted.node);
                    const auto& tokens = derive_input_tokens(theme);
                    return CompactBorder{
                        .shape = {bounds, std::clamp(tokens.size(current.size).border_radius, 0.0F,
                                                     0.5F * std::min(bounds.width, bounds.height))},
                        .corners = current.compact->corners,
                        .color = current.transition->value().colors[1],
                        .width = current.variant == InputVariant::Outlined || current.variant == InputVariant::Filled
                                     ? current.layout.border_width
                                     : 0.0F,
                        .priority = current.disabled            ? 0
                                    : current.hovering_pointers ? 4
                                    : current.focused           ? 3
                                                                : 2,
                        .clip = current.container_clip ? std::optional{graphics::EffectClip{1, *current.container_clip}}
                                                       : std::nullopt};
                },
                &host.surfaces());
        }
        owner.invalidate(component, text_dirty | runtime::DirtyFlags::HitTest);
        state.count_ready = true;
    }
};

struct TextAreaPropsAccess final {
    static void mount(TextAreaProps props) {
        InputProps input;
        input.common_ = std::move(props.common_);
        input.textarea_ = std::make_shared<TextAreaPropsData>(std::move(props.textarea_));
        Input(std::move(input));
    }
};

struct PasswordPropsAccess final {
    struct VisibilityBridge {
        explicit VisibilityBridge(bool initial) : visible(initial) {}

        Signal<bool> visible;
        Scope scope;
    };

    static void mount(PasswordProps props, std::optional<InputPrefix> prefix,
                      std::optional<InputSuffix> custom_suffix) {
        if (!active_input_host) {
            throw std::logic_error("Password requires an active InputComponentHost");
        }
        if (props.common_.value_ && props.common_.default_value_) {
            throw std::invalid_argument("Password value and defaultValue are mutually exclusive");
        }
        if (props.visible_ && props.default_visible_) {
            throw std::invalid_argument("Password visible and defaultVisible are mutually exclusive");
        }
        validate(read_prop(props.common_.size_));
        validate(read_prop(props.common_.status_));
        const auto action = read_prop(props.action_);
        if (action < PasswordAction::Click || action > PasswordAction::Hover) {
            throw std::invalid_argument("Invalid Password action");
        }
        const bool controlled = props.visible_.has_value();
        auto bridge = std::make_shared<VisibilityBridge>(controlled ? read_prop(*props.visible_)
                                                                    : props.default_visible_.value_or(false));
        if (controlled) {
            const std::weak_ptr<VisibilityBridge> weak = bridge;
            static_cast<void>(connect_prop(bridge->scope, *props.visible_, [weak](bool value) {
                if (const auto current = weak.lock()) {
                    current->visible.set(value);
                }
            }));
        }
        InputProps input;
        input.common_ = std::move(props.common_);
        input.password_visible_ = bridge->visible;
        input.password_lifetime_ = bridge;
        std::optional<InputSuffix> suffix = custom_suffix;
        const auto* fixed_toggle = PropAccess::static_value(props.visibility_toggle_);
        if (!fixed_toggle || *fixed_toggle) {
            input.suffix_presence_ = bind([toggle = props.visibility_toggle_, has_suffix = custom_suffix.has_value()] {
                return read_prop(toggle) || has_suffix;
            });
            suffix = InputSuffix{[bridge, controlled, disabled = input.common_.disabled_,
                                  callback = std::move(props.on_visible_change_), custom_suffix,
                                  toggle = props.visibility_toggle_, focusable = props.toggle_focusable_,
                                  action = props.action_, renderer = props.icon_render_] {
                mount_input_affix_action(
                    *active_input_host->host_, bind([bridge, renderer] {
                        const auto visible = bridge->visible.get();
                        return renderer ? renderer(visible)
                                        : IconSource{visible ? IconName::EyeOutlined : IconName::EyeInvisibleOutlined};
                    }),
                    disabled,
                    [disabled, bridge, controlled, callback] {
                        if (read_prop(disabled)) {
                            return;
                        }
                        const bool next = !bridge->visible.get();
                        if (!controlled) {
                            bridge->visible.set(next);
                        }
                        if (callback) {
                            callback(next);
                        }
                    },
                    toggle, focusable, focusable, bind([action] {
                        const auto value = read_prop(action);
                        if (value < PasswordAction::Click || value > PasswordAction::Hover) {
                            throw std::invalid_argument("Invalid Password action");
                        }
                        return value == PasswordAction::Hover;
                    }));
                if (custom_suffix) {
                    SlotContentAccess::function (*custom_suffix)();
                }
            }};
        }
        Input(std::move(input), std::move(prefix), std::move(suffix));
    }
};

InputComponentHost::InputComponentHost(WindowComponentServices& host, input::TextInputPlatform& platform,
                                       input::TextClipboard& clipboard)
    : host_(&host), edit_services_(&host.bind_text_edit(platform, clipboard)), editors_(edit_services_->editors()),
      sessions_(edit_services_->sessions()), clipboard_(edit_services_->clipboard()) {
    host_->attach_input_host(*this);
    host_->set_input_runtime(this);
}

InputComponentHost::~InputComponentHost() {
    dispose();
    host_->set_input_runtime(nullptr);
    host_->detach_input_host(*this);
}

void InputComponentHost::mount(const Content& content) {
    host_->mount(content);
}

void* InputComponentHost::begin_mount() noexcept {
    return std::exchange(active_input_host, this);
}

void InputComponentHost::end_mount(void* previous) noexcept {
    active_input_host = static_cast<InputComponentHost*>(previous);
}

void InputComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](const auto& mounted) { return !host_->components().contains(mounted.component); });
}

void InputComponentHost::on_dispose() noexcept {
    mounted_.clear();
    auto_focus_requests_.clear();
}

bool InputComponentHost::focus(runtime::ComponentId component, InputFocusOptions options) {
    if (options.cursor < InputFocusCursor::Keep || options.cursor > InputFocusCursor::All) {
        throw std::invalid_argument("Invalid Input focus cursor");
    }
    auto* state = host_->components().state<InputState>(component);
    if (!state || !state->active || state->disabled || !host_->components().branch_active(component) ||
        !host_->focus().state().window_active) {
        return false;
    }
    const auto interaction = state->mounted.interaction;
    if (options.cursor != InputFocusCursor::Keep) {
        state->affinity = text::TextCaretAffinity::Downstream;
        if (state->textarea) {
            state->textarea->preferred_x.reset();
            state->textarea->reveal_caret = true;
        }
        auto& editor = editors_.require(state->mounted.editor);
        if (editor.composition().active) {
            static_cast<void>(sessions_.cancel_composition());
        }
        const auto result = options.cursor == InputFocusCursor::All     ? editor.select_all()
                            : options.cursor == InputFocusCursor::Start ? editor.place(0)
                                                                        : editor.place(editor.value().size());
        if (!result) {
            return false;
        }
        update_text(component, false);
    }
    // Safe from onFocus/onBlur handlers: the manager flushes generation-checked
    // transfers after the enclosing focus transaction.
    host_->focus().defer_focus(interaction, input::FocusModality::keyboard);
    return host_->components().contains(component);
}

bool InputComponentHost::blur(runtime::ComponentId component) {
    const auto* state = host_->components().state<InputState>(component);
    if (!state || host_->focus().state().focused != state->mounted.interaction) {
        return false;
    }
    host_->focus().defer_focus({}, input::FocusModality::keyboard);
    return true;
}

bool InputComponentHost::select(runtime::ComponentId component, std::size_t anchor, std::size_t caret) {
    auto* state = host_->components().state<InputState>(component);
    if (!state || !state->active || state->disabled || !host_->components().branch_active(component)) {
        return false;
    }
    auto& editor = editors_.require(state->mounted.editor);
    if (!editor.boundaries().is_boundary(anchor) || !editor.boundaries().is_boundary(caret)) {
        return false;
    }
    state->affinity = text::TextCaretAffinity::Downstream;
    if (state->textarea) {
        state->textarea->preferred_x.reset();
        state->textarea->reveal_caret = true;
    }
    if (editor.composition().active) {
        static_cast<void>(sessions_.cancel_composition());
    }
    const auto result = editor.select({anchor, caret});
    if (result) {
        update_text(component, false);
    }
    return static_cast<bool>(result);
}

void InputComponentHost::dispose() noexcept {
    while (!mounted_.empty()) {
        const auto id = mounted_.back().component;
        if (!host_->destroy(id)) {
            mounted_.pop_back();
        }
    }
}

void InputComponentHost::set_window_active(bool active) {
    if (!active) {
        for (const auto& mounted : mounted_) {
            if (auto* state = host_->components().state<InputState>(mounted.component)) {
                state->caret_blink.stop();
            }
        }
    }
    static_cast<void>(sessions_.set_window_active(active));
    host_->set_window_active(active);
}

void InputComponentHost::set_display_scale(float scale) {
    if (!std::isfinite(scale) || scale <= 0) {
        throw std::invalid_argument("Input display scale must be positive and finite");
    }
    if (display_scale_ == scale) {
        return;
    }
    display_scale_ = scale;
    for (const auto& mounted : mounted_) {
        if (auto* state = host_->components().state<InputState>(mounted.component)) {
            // The window owner updates its font resolver before this call.
            host_->text().scene_service().set_font_chain(state->text_scene,
                                                         host_->text().resolve_fonts(state->typography));
            invalidate(mounted.component, runtime::DirtyFlags::Geometry);
        }
    }
}

bool InputComponentHost::synchronize_input_area(double scale, int width, int height) {
    if (!std::isfinite(scale) || scale <= 0 || width <= 0 || height <= 0) {
        throw std::invalid_argument("Input window coordinate transform must be positive and finite");
    }
    const auto session = sessions_.active();
    if (!session.valid()) {
        return true;
    }
    for (const auto& mounted : mounted_) {
        if (mounted.editor == session.owner) {
            const auto geometry = layout_snapshot(mounted.component);
            auto bounds = geometry.viewport;
            if (const auto* state = host_->components().state<InputState>(mounted.component);
                state && state->textarea) {
                bounds.height = std::min(bounds.height, state->typography.line_height);
                bounds.y = std::clamp(geometry.caret.y, geometry.viewport.y,
                                      geometry.viewport.y + geometry.viewport.height - bounds.height);
            }
            const auto clip = geometry.clip;
            return sessions_.set_input_area({{bounds.x, bounds.y, bounds.width, bounds.height},
                                             {clip.x, clip.y, clip.width, clip.height},
                                             0,
                                             0,
                                             geometry.caret_x,
                                             scale,
                                             width,
                                             height});
        }
    }
    return false;
}

void InputComponentHost::invalidate(runtime::ComponentId component, runtime::DirtyFlags flags) {
    if (auto* current = host_->components().state<InputState>(component)) {
        host_->dirty().invalidate(current->mounted.node, flags);
        if (runtime::has_any(flags, runtime::DirtyFlags::Material)) {
            update_caret(component);
            const auto& theme = host_->components().theme_scope(component)->snapshot();
            const auto values = material_values(*current, derive_input_tokens(theme));
            if (current->transition) {
                const auto spec =
                    animation::resolve_motion_policy(theme, host_->motion_preference())
                        .transition(animation::MotionDurationToken::mid, animation::MotionEasingToken::ease_in_out);
                current->transition->retarget(values, spec, host_->animation_time());
            }
            const auto color = current->transition ? current->transition->value().colors[3] : values.colors[3];
            current->slot_foreground.set({color.red(), color.green(), color.blue(), color.alpha()});
        }
    }
}

void InputComponentHost::apply_material_transition(runtime::ComponentId component) {
    if (auto* state = host_->components().state<InputState>(component); state && state->transition) {
        host_->dirty().invalidate(state->mounted.node, runtime::DirtyFlags::Material | runtime::DirtyFlags::Animation);
        const auto color = state->transition->value().colors[3];
        state->slot_foreground.set({color.red(), color.green(), color.blue(), color.alpha()});
    }
}

void InputComponentHost::synchronize_auxiliary_motion() {
    for (const auto& mounted : mounted_) {
        invalidate(mounted.component, runtime::DirtyFlags::Material);
    }
}

void InputComponentHost::update_caret(runtime::ComponentId component, bool reset) {
    auto* state = host_->components().state<InputState>(component);
    if (!state) {
        return;
    }
    const bool eligible =
        state->focused && !state->disabled && !state->read_only && host_->focus().state().window_active;
    const auto policy = animation::resolve_motion_policy(host_->components().theme_scope(component)->snapshot(),
                                                         host_->motion_preference());
    if (state->caret_blink.configure(eligible, policy.enabled() && !policy.reduced(), host_->animation_time(), reset)) {
        host_->dirty().invalidate(state->mounted.node, runtime::DirtyFlags::Geometry);
    }
}

std::size_t InputComponentHost::tick_auxiliary(animation::AnimationTime now) {
    std::size_t changed{};
    for (const auto& mounted : mounted_) {
        if (auto* state = host_->components().state<InputState>(mounted.component);
            state && state->caret_blink.tick(now)) {
            host_->dirty().invalidate_in_frame(mounted.node, runtime::DirtyFlags::Material);
            ++changed;
        }
    }
    return changed;
}

void InputComponentHost::dispatch_pointer(runtime::ComponentId component, input::PointerDispatchContext& context) {
    auto* state = host_->components().state<InputState>(component);
    if (!state) {
        return;
    }
    if (state->textarea && dispatch_text_area_resize(component, context)) {
        return;
    }
    const auto& event = context.event();
    const auto kind = context.kind();
    const bool owned = state->selecting_pointer == event.pointer;
    if (kind == input::PointerEventKind::cancel) {
        if (owned) {
            state->selecting_pointer.reset();
            static_cast<void>(context.release_pointer_capture());
        }
        return;
    }
    const bool down = kind == input::PointerEventKind::down && event.button == input::PointerButton::primary;
    const bool up = kind == input::PointerEventKind::up && event.button == input::PointerButton::primary;
    if (!down && !(owned && (kind == input::PointerEventKind::move || up))) {
        return;
    }
    if (state->disabled || !state->focused || (down && state->selecting_pointer && !owned)) {
        return;
    }
    if (state->otp) {
        auto callback = state->otp->clicked;
        if (down && callback) {
            callback();
        }
        return;
    }
    const auto viewport = state->geometry.viewport;
    const auto clip = state->geometry.clip;
    // Affixes/padding may focus the Input, but are not editable text hit areas.
    if (down && (event.x < clip.x || event.x > clip.x + clip.width || event.y < clip.y ||
                 event.y > clip.y + clip.height || clip.width <= 0 || clip.height <= 0)) {
        return;
    }
    update_text(component, false);
    auto& scene = host_->text().scene_service();
    if (state->carets.revision() != scene.text_state(state->text_scene).revision() &&
        !(state->textarea ? scene.synchronize_line_caret_map(state->text_scene, state->carets)
                          : scene.synchronize_caret_map(state->text_scene, state->carets))) {
        throw std::runtime_error("Input pointer caret mapping failed");
    }
    const auto x = event.x - viewport.x + state->geometry.scroll_offset;
    const auto stop = state->textarea
                          ? state->carets.nearest(x, event.y - viewport.y + state->textarea->vertical_scroll,
                                                  state->carets.revision())
                          : state->carets.nearest(x, state->carets.revision());
    if (!stop) {
        return;
    }
    state->affinity = stop->affinity;
    if (state->textarea) {
        state->textarea->preferred_x.reset();
        state->textarea->reveal_caret = true;
    }
    const auto byte = state->display.display_to_committed(stop->byte);
    auto& editor = editors_.require(state->mounted.editor);
    if (down) {
        if (editor.composition().active) {
            static_cast<void>(sessions_.cancel_composition());
        }
        check(event.click_count == 2 ? editor.select_word(byte) : editor.place(byte));
        if (context.capture_pointer()) {
            state->selecting_pointer = event.pointer;
        }
        state->last_selection_position = {event.x, event.y};
        update_caret(component, true);
    } else {
        // A release at the last position preserves a double-click word selection.
        if (state->last_selection_position != runtime::Point{event.x, event.y}) {
            check(editor.place(byte, true));
        }
        state->last_selection_position = {event.x, event.y};
        if (up) {
            state->selecting_pointer.reset();
            static_cast<void>(context.release_pointer_capture());
        }
    }
    update_text(component, false);
}

bool InputComponentHost::dispatch_text_area_resize(runtime::ComponentId component,
                                                   input::PointerDispatchContext& context) {
    auto& state = *host_->components().state<InputState>(component);
    auto& area = *state.textarea;
    const auto& event = context.event();
    const auto kind = context.kind();
    const bool owned = area.resizing_pointer == event.pointer;
    if (kind == input::PointerEventKind::cancel && owned) {
        area.resizing_pointer.reset();
        static_cast<void>(context.release_pointer_capture());
        return true;
    }
    const bool primary_up = kind == input::PointerEventKind::up && event.button == input::PointerButton::primary;
    if (owned && (kind == input::PointerEventKind::move || primary_up)) {
        if (!state.disabled && !area.auto_size.enabled && area.resize != TextAreaResize::None) {
            if (area.resize == TextAreaResize::Horizontal || area.resize == TextAreaResize::Both) {
                area.resized_width =
                    std::max(state.layout.control_height, area.resize_initial.width + event.x - area.resize_start.x);
            }
            if (area.resize == TextAreaResize::Vertical || area.resize == TextAreaResize::Both) {
                area.resized_height =
                    std::max(state.layout.control_height, area.resize_initial.height + event.y - area.resize_start.y);
            }
            invalidate(component, text_dirty | runtime::DirtyFlags::HitTest);
        }
        if (primary_up) {
            area.resizing_pointer.reset();
            static_cast<void>(context.release_pointer_capture());
        }
        return true;
    }
    if (area.resizing_pointer) {
        return true;
    }
    if (kind != input::PointerEventKind::down || event.button != input::PointerButton::primary || state.disabled ||
        area.auto_size.enabled || area.resize == TextAreaResize::None || area.resizing_pointer) {
        return false;
    }
    auto bounds = translated_bounds(host_->nodes(), state.mounted.node);
    bounds.height = std::max(0.0F, bounds.height - area.footer_height);
    const auto grip = std::min(12.0F, std::min(bounds.width, bounds.height));
    if (event.x < bounds.x + bounds.width - grip || event.x > bounds.x + bounds.width ||
        event.y < bounds.y + bounds.height - grip || event.y > bounds.y + bounds.height) {
        return false;
    }
    if (context.capture_pointer()) {
        area.resizing_pointer = event.pointer;
        area.resize_start = {event.x, event.y};
        area.resize_initial = {bounds.width, bounds.height};
        context.stop_propagation();
    }
    return true;
}

bool InputComponentHost::dispatch(const input::ScrollInputEvent& event) {
    if (!input::is_valid(event) || !host_->focus().state().window_active) {
        return false;
    }
    auto target = host_->hit_test().hit_test({event.x, event.y});
    while (target) {
        const auto& record = host_->interactions().require(*target);
        auto* state = host_->components().state<InputState>(record.component);
        if (state && state->textarea && !state->disabled && state->active &&
            host_->components().branch_active(state->mounted.component)) {
            auto& area = *state->textarea;
            const auto& viewport = state->geometry.viewport;
            if (event.x < viewport.x || event.x > viewport.x + viewport.width || event.y < viewport.y ||
                event.y > viewport.y + viewport.height) {
                return false;
            }
            const auto max_y =
                std::ceil(std::max(0.0F, state->geometry.text_height - viewport.height) * display_scale_) /
                display_scale_;
            const auto next_y =
                std::clamp(area.vertical_scroll - event.delta_y * state->typography.line_height * 3, 0.0F, max_y);
            const auto max_x = std::ceil(std::max(0.0F, state->geometry.text_width - viewport.width) * display_scale_) /
                               display_scale_;
            const auto next_x =
                area.wrap
                    ? state->geometry.scroll_offset
                    : std::clamp(state->geometry.scroll_offset - event.delta_x * state->typography.line_height * 3,
                                 0.0F, max_x);
            if (next_y == area.vertical_scroll && next_x == state->geometry.scroll_offset) {
                return false;
            }
            area.vertical_scroll = next_y;
            state->geometry.scroll_offset = next_x;
            area.reveal_caret = false;
            invalidate(state->mounted.component, runtime::DirtyFlags::Geometry);
            return true;
        }
        target = record.parent;
    }
    return false;
}

bool InputComponentHost::dispatch_keyboard(runtime::ComponentId component, const input::KeyboardInputEvent& event) {
    using input::Key;
    using input::KeyModifier;
    auto* state = host_->components().state<InputState>(component);
    if (!state || !state->active || state->disabled || !state->focused) {
        return false;
    }
    auto& editor = editors_.require(state->mounted.editor);
    if (editor.composition().active) {
        if (event.key == Key::escape && event.action == input::KeyAction::down && !event.repeat) {
            static_cast<void>(sessions_.cancel_composition());
            update_text(component, false);
        }
        // IME owns navigation, deletion, candidate Tab/Enter and shortcuts until
        // it commits or cancels. Never mutate committed text from these keys.
        return true;
    }
    if (state->otp && state->otp->keyboard) {
        auto callback = state->otp->keyboard;
        if (callback(event)) {
            return true;
        }
        state = host_->components().state<InputState>(component);
        if (!state) {
            return true;
        }
    }
    if (event.key == Key::escape) {
        if (event.action == input::KeyAction::down && !event.repeat && state->on_internal_cancel) {
            auto callback = state->on_internal_cancel;
            callback();
            return true;
        }
        return false;
    }
    if (event.key == Key::tab) {
        return false;
    }
    const bool shift = input::has_modifier(event.modifiers, KeyModifier::shift);
    const auto other = event.primary_modifier == KeyModifier::control ? KeyModifier::meta : KeyModifier::control;
    const bool primary = input::has_modifier(event.modifiers, event.primary_modifier) &&
                         !input::has_modifier(event.modifiers, other) &&
                         !input::has_modifier(event.modifiers, KeyModifier::alt);
    const bool plain = !input::has_modifier(event.modifiers, KeyModifier::control) &&
                       !input::has_modifier(event.modifiers, KeyModifier::meta) &&
                       !input::has_modifier(event.modifiers, KeyModifier::alt);
    const bool shortcut = primary && (event.key == Key::a || event.key == Key::c || event.key == Key::x ||
                                      event.key == Key::v || event.key == Key::z || event.key == Key::y);
    const bool navigation =
        (plain &&
         (event.key == Key::left || event.key == Key::right || event.key == Key::home || event.key == Key::end ||
          (state->textarea && (event.key == Key::up || event.key == Key::down || event.key == Key::page_up ||
                               event.key == Key::page_down)))) ||
        (state->textarea && primary && (event.key == Key::home || event.key == Key::end));
    const bool deletion = plain && (event.key == Key::backspace || event.key == Key::delete_forward);
    if (!shortcut && !navigation && !deletion && event.key != Key::enter && event.key != Key::space) {
        return false;
    }
    if (event.action == input::KeyAction::up) {
        return true;
    }
    update_caret(component, true);
    if (event.key == Key::space) {
        return true; // Only TextCommitted inserts characters.
    }
    if (event.key == Key::enter) {
        if (state->textarea && plain && !state->read_only) {
            const auto owner = state->mounted.editor;
            editor.break_history_merge();
            const auto result = editor.commit_text("\n");
            if (auto* current = editors_.find(owner)) {
                current->break_history_merge();
            }
            if (result.value_changed) {
                notify_change(owner);
            }
        } else if ((!state->textarea && plain || state->textarea && primary) && !event.repeat) {
            submit(component);
        }
        return true;
    }
    input::TextEditResult result;
    const auto owner = state->mounted.editor;
    if (shortcut) {
        if (event.repeat) {
            return true;
        }
        if (event.key == Key::a) {
            result = editor.select_all();
        } else if (event.key == Key::c) {
            if (!state->password || state->visible) {
                result = clipboard_.copy(owner).edit;
            }
        } else if (!state->read_only) {
            if (event.key == Key::x) {
                if (!state->password || state->visible) {
                    result = clipboard_.cut(owner).edit;
                }
            } else if (event.key == Key::v) {
                result = clipboard_.paste(owner).edit;
            } else if (event.key == Key::y || (event.key == Key::z && shift)) {
                result = editor.redo();
            } else {
                result = editor.undo();
            }
        }
    } else if (navigation) {
        update_text(component, false);
        auto& scene = host_->text().scene_service();
        if (state->carets.revision() != scene.text_state(state->text_scene).revision() &&
            !(state->textarea ? scene.synchronize_line_caret_map(state->text_scene, state->carets)
                              : scene.synchronize_caret_map(state->text_scene, state->carets))) {
            throw std::runtime_error("Input navigation caret mapping failed");
        }
        const auto revision = state->carets.revision();
        const auto current =
            state->carets.at(state->display.committed_to_display(editor.selection().caret), revision, state->affinity)
                .value();
        std::optional<text::TextCaretStop> target;
        if (state->textarea && (event.key == Key::up || event.key == Key::down || event.key == Key::page_up ||
                                event.key == Key::page_down)) {
            auto& area = *state->textarea;
            if (!area.preferred_x) {
                area.preferred_x = current.x;
            }
            const auto page =
                std::max(1, static_cast<int>(state->geometry.viewport.height / state->typography.line_height));
            const auto delta = event.key == Key::up        ? -1
                               : event.key == Key::down    ? 1
                               : event.key == Key::page_up ? -page
                                                           : page;
            target = state->carets.adjacent_line(current, delta, *area.preferred_x, revision);
        } else {
            if (state->textarea) {
                state->textarea->preferred_x.reset();
            }
            if (event.key == Key::home || event.key == Key::end) {
                target = primary ? state->carets.at(state->display.committed_to_display(
                                                        event.key == Key::home ? 0 : editor.value().size()),
                                                    revision)
                                 : state->carets.line_edge(current.line, event.key == Key::end, revision);
            } else if (!shift && !editor.selection().empty()) {
                const auto selected = state->display.snapshot().selection;
                const bool right = event.key == Key::right;
                for (const auto& stop : state->carets.stops()) {
                    if (stop.byte < selected.begin() || stop.byte > selected.end() ||
                        (stop.byte == selected.begin() && stop != state->carets.at(stop.byte, revision).value()) ||
                        (stop.byte == selected.end() &&
                         stop != state->carets.at(stop.byte, revision, text::TextCaretAffinity::Upstream).value())) {
                        continue;
                    }
                    if (!target ||
                        (right ? stop.line > target->line || (stop.line == target->line && stop.x > target->x)
                               : stop.line < target->line || (stop.line == target->line && stop.x < target->x))) {
                        target = stop;
                    }
                }
            } else {
                target = state->carets.adjacent_visual(current, event.key == Key::left ? -1 : 1, revision);
            }
        }
        if (target) {
            result = editor.place(state->display.display_to_committed(target->byte), shift);
            state->affinity = target->affinity;
            if (state->textarea) {
                state->textarea->reveal_caret = true;
            }
            invalidate(component, runtime::DirtyFlags::Geometry);
        }
    } else if (deletion && !state->read_only) {
        result = event.key == Key::backspace ? editor.erase_backward() : editor.erase_forward();
    }
    // Clipboard and onChange callbacks may synchronously destroy/reuse the owner.
    notify_otp_edit(owner, bool(result));
    if (result.value_changed) {
        notify_change(owner);
    } else if (result) {
        update_text(component, false);
    }
    return true;
}

void InputComponentHost::update_theme(runtime::ComponentId component) {
    auto* state = host_->components().state<InputState>(component);
    if (!state) {
        return;
    }
    const auto& theme = host_->components().theme_scope(component)->snapshot();
    const auto tokens = derive_input_tokens(theme);
    const auto& size_tokens = tokens.size(state->size);
    auto model = state->layout;
    model.control_height = size_tokens.control_height;
    if (state->search_control_height && state->size == ControlSize::Small) {
        model.control_height = std::max(
            model.control_height, size_tokens.line_height + 2 * (size_tokens.padding_block + tokens.border_width));
    }
    model.border_width = tokens.border_width;
    model.padding_inline = size_tokens.padding_inline;
    if (state->otp) {
        const auto small_padding = theme.map().size_xs * .5F;
        model.padding_inline = state->size == ControlSize::Small   ? small_padding * .5F
                               : state->size == ControlSize::Large ? theme.map().size_xs
                                                                   : small_padding;
        model.preferred_width = size_tokens.font_size + 2 * (model.padding_inline + tokens.border_width);
    }
    model.padding_block = size_tokens.padding_block;
    if (state->variant == InputVariant::Borderless || state->variant == InputVariant::Underlined) {
        model.border_width = 0;
        model.padding_block += tokens.border_width;
    }
    model.gap = tokens.affix_padding;
    runtime::SemanticTypography typography = state->inherited_typography.value_or(runtime::SemanticTypography{
        theme.text().font_family, theme.text().font_weight, false, size_tokens.font_size, size_tokens.line_height});
    if (state->inherited_typography) {
        model.control_height =
            std::max(model.control_height, typography.line_height + 2 * (model.padding_block + model.border_width));
    }
    if (!state->text_scene.valid() || model != state->layout) {
        state->layout = model;
        configure_layout(component);
        invalidate(component, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                  runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    }
    auto& scene = host_->text().scene_service();
    if (!state->text_scene.valid()) {
        state->text_scene = scene.create(state->viewport, String{}, host_->text().resolve_fonts(typography),
                                         static_cast<std::uint32_t>(std::lround(typography.font_size)),
                                         {typography.line_height, std::numeric_limits<float>::infinity()});
        state->typography = typography;
    } else if (state->typography != typography) {
        scene.set_font_chain(state->text_scene, host_->text().resolve_fonts(typography));
        scene.set_pixel_size(state->text_scene, static_cast<std::uint32_t>(std::lround(typography.font_size)));
        scene.set_line_height(state->text_scene, typography.line_height);
        state->typography = typography;
        const auto revisions = scene.revisions(state->text_scene);
        host_->layout().set_intrinsic_revision(state->viewport, revisions.content + revisions.layout);
        invalidate(component, text_dirty);
    }
    state->slot_typography.set(typography);
    invalidate(component, runtime::DirtyFlags::Material);
}

void InputComponentHost::configure_layout(runtime::ComponentId component) {
    auto* state = host_->components().state<InputState>(component);
    if (!state) {
        return;
    }
    if (!state->textarea) {
        host_->layout().set_layout(state->mounted.node, state->layout);
        return;
    }
    const auto suffix = host_->nodes().require(state->mounted.node).children[2];
    host_->layout().set_layout(
        suffix,
        layout::ComponentLayout{
            [this, component](layout::LayoutEngine& engine, runtime::NodeId node, layout::Constraints constraints) {
                auto& state = *host_->components().state<InputState>(component);
                auto& area = *state.textarea;
                area.clear_size = {};
                area.count_size = {};
                for (const auto child : host_->nodes().require(node).children) {
                    const auto size = engine.measure_child(
                        child, {0, constraints.max_width, 0, std::numeric_limits<float>::infinity()});
                    if (child == state.count_node) {
                        area.count_size = size;
                    } else {
                        area.clear_size = size;
                    }
                }
                area.footer_height = area.count_size.height;
                return runtime::Size{std::max(area.clear_size.width, area.count_size.width),
                                     area.clear_size.height + area.footer_height};
            },
            [this, component](layout::LayoutEngine& engine, runtime::NodeId node, runtime::Rect) {
                const auto& state = *host_->components().state<InputState>(component);
                const auto& area = *state.textarea;
                const auto root = host_->nodes().require(state.mounted.node).bounds;
                const auto inline_inset = state.layout.padding_inline + state.layout.border_width;
                const auto block_inset = state.layout.padding_block + state.layout.border_width;
                for (const auto child : host_->nodes().require(node).children) {
                    if (child == state.count_node) {
                        engine.place_child(child, {root.x + root.width - area.count_size.width,
                                                   root.y + root.height - area.footer_height, area.count_size.width,
                                                   area.count_size.height});
                    } else {
                        engine.place_child(child,
                                           {root.x + root.width - inline_inset - area.clear_size.width,
                                            root.y + block_inset, area.clear_size.width, area.clear_size.height});
                    }
                }
            }});
    host_->layout().set_layout(
        state->mounted.node,
        layout::ComponentLayout{
            [this, component](layout::LayoutEngine&, runtime::NodeId, layout::Constraints constraints) {
                return measure_text_area(component, constraints);
            },
            [this, component](layout::LayoutEngine&, runtime::NodeId, runtime::Rect bounds) {
                place_text_area(component, bounds);
            }});
}

runtime::Size InputComponentHost::measure_text_area(runtime::ComponentId component, layout::Constraints constraints) {
    auto& state = *host_->components().state<InputState>(component);
    auto& area = *state.textarea;
    auto& engine = host_->layout();
    auto& root = host_->nodes().require(state.mounted.node);
    const auto frame = 2 * (state.layout.padding_inline + state.layout.border_width);
    const auto block_frame = 2 * (state.layout.padding_block + state.layout.border_width);
    const auto requested =
        area.resized_width.value_or(std::isfinite(constraints.max_width) ? constraints.max_width : 320.0F);
    const auto width = std::clamp(requested, constraints.min_width, constraints.max_width);
    static_cast<void>(engine.measure_child(root.children[0], layout::Constraints::fixed(0, 0)));
    static_cast<void>(engine.measure_child(root.children[2], {0, width, 0, std::numeric_limits<float>::infinity()}));
    const auto clear_inline = area.clear_size.width > 0 ? area.clear_size.width + state.layout.gap : 0;
    const auto inner_width = std::max(0.0F, width - frame - clear_inline);
    static_cast<void>(
        engine.measure_child(state.viewport, {inner_width, inner_width, 0, std::numeric_limits<float>::infinity()}));
    const auto& measurement = host_->text().scene_service().text_state(state.text_scene).measurement();
    auto rows = area.rows;
    if (area.auto_size.enabled) {
        rows = std::max(area.auto_size.min_rows, measurement.lines.size());
        if (area.auto_size.max_rows) {
            rows = std::min(rows, *area.auto_size.max_rows);
        }
    }
    const auto height =
        std::max(state.layout.control_height, static_cast<float>(rows) * state.typography.line_height + block_frame);
    if (!std::isfinite(height)) {
        throw std::overflow_error("TextArea height exceeds logical range");
    }
    const auto body = area.auto_size.enabled ? height : area.resized_height.value_or(height);
    const auto size = constraints.constrain({width, body + area.footer_height});
    root.first_baseline =
        std::min(size.height, state.layout.padding_block + state.layout.border_width + measurement.first_baseline);
    return size;
}

void InputComponentHost::place_text_area(runtime::ComponentId component, runtime::Rect bounds) {
    const auto& state = *host_->components().state<InputState>(component);
    const auto& area = *state.textarea;
    const auto& root = host_->nodes().require(state.mounted.node);
    const auto inset = std::min(bounds.width * .5F, state.layout.padding_inline + state.layout.border_width);
    const auto body_height = std::max(0.0F, bounds.height - area.footer_height);
    const auto top = std::min(body_height * .5F, state.layout.padding_block + state.layout.border_width);
    const auto clear_inline = area.clear_size.width > 0 ? area.clear_size.width + state.layout.gap : 0;
    host_->layout().place_child(root.children[0], {bounds.x, bounds.y, 0, 0});
    host_->layout().place_child(state.viewport,
                                {bounds.x + inset, bounds.y + top,
                                 std::max(0.0F, bounds.width - 2 * inset - clear_inline),
                                 std::max(0.0F, body_height - 2 * top)},
                                true, true);
    host_->layout().place_child(root.children[2], bounds, true, true);
}

void InputComponentHost::update_text(runtime::ComponentId component, bool measure_layout, bool refresh_count) {
    auto* state = host_->components().state<InputState>(component);
    if (!state || !state->text_scene.valid()) {
        return;
    }
    if (refresh_count && !update_count(component)) {
        return;
    }
    state = host_->components().state<InputState>(component);
    if (!state) {
        return;
    }
    const auto& editor = editors_.require(state->mounted.editor);
    update_clear_visibility(component);
    const auto changed = state->display.update(editor, state->placeholder.view(), state->password && !state->visible,
                                               state->mask_glyph.view());
    if (changed.text_changed) {
        state->affinity = text::TextCaretAffinity::Downstream;
    }
    if (state->textarea && changed.geometry_changed) {
        state->textarea->reveal_caret = true;
        if (changed.text_changed) {
            state->textarea->preferred_x.reset();
        }
    }
    auto& scene = host_->text().scene_service();
    if (changed.text_changed &&
        scene.text_state(state->text_scene).content().bytes() != state->display.snapshot().text) {
        scene.set_content(state->text_scene, String::from_utf8(state->display.snapshot().text).value());
        const auto revisions = scene.revisions(state->text_scene);
        host_->layout().set_intrinsic_revision(state->viewport, revisions.content + revisions.layout);
        invalidate(component, measure_layout ? text_dirty : runtime::DirtyFlags::Geometry);
    }
    if (changed.geometry_changed) {
        invalidate(component, runtime::DirtyFlags::Geometry);
        update_caret(component, true);
    }
    if (measure_layout && state->measured_value_revision != editor.revision()) {
        state->measured_value_revision = editor.revision();
        invalidate(component,
                   runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry);
    }
    const auto properties = input_properties(*state);
    if (state->focused && state->session_properties != properties && !editor.composition().active) {
        state->session_properties = properties;
        static_cast<void>(sessions_.focus(state->mounted.editor, properties));
    }
}

void InputComponentHost::update_clear_visibility(runtime::ComponentId component) {
    auto* state = host_->components().state<InputState>(component);
    if (!state) {
        return;
    }
    const bool visible = state->allow_clear && !state->disabled && !state->read_only &&
                         !editors_.require(state->mounted.editor).value().empty();
    if (state->clear_visible.get() == visible) {
        return;
    }
    state->clear_visible.set(visible);
    update_suffix_layout(component);
}

void InputComponentHost::prepare_auxiliary_layout() {
    std::optional<std::vector<runtime::ComponentId>> pending;
    for (const auto& mounted : mounted_) {
        const auto* state = host_->components().state<InputState>(mounted.component);
        const auto* editor = editors_.find(mounted.editor);
        if (state && editor && state->count_revision != editor->revision()) {
            if (!pending) {
                pending.emplace();
            }
            pending->push_back(mounted.component);
        }
    }
    // Callback-owned snapshots remain valid if a formatter removes any sibling.
    if (pending) {
        for (const auto component : *pending) {
            static_cast<void>(update_count(component));
        }
    }
}

void InputComponentHost::update_suffix_layout(runtime::ComponentId component) {
    auto* state = host_->components().state<InputState>(component);
    if (!state) {
        return;
    }
    const bool suffix =
        state->custom_suffix || state->clear_visible.get() || (state->show_count && !state->count_text.get().empty());
    if (state->layout.suffix != suffix) {
        state->layout.suffix = suffix;
        configure_layout(component);
        invalidate(component, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                  runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    }
}

void InputComponentHost::configure_count_transform(runtime::ComponentId component) {
    auto* state = host_->components().state<InputState>(component);
    if (!state) {
        return;
    }
    input::TextEditorState::EditTransform transform;
    if (state->otp) {
        transform = state->otp->transform;
    } else if (state->exceed_formatter) {
        transform = [this, component](std::string_view candidate) {
            const auto* state = host_->components().state<InputState>(component);
            if (!state) {
                throw std::runtime_error("Retired Input formatter");
            }
            const auto maximum = state->count_options.max ? state->count_options.max : state->hard_max;
            const auto unit = state->count_options.unit;
            auto strategy = state->count_strategy;
            auto formatter = state->exceed_formatter;
            auto value = String::from_utf8(candidate).value();
            if (maximum && detail::count_value(value.view(), unit, strategy) > *maximum) {
                return std::string(formatter(std::move(value), *maximum).bytes());
            }
            return std::string(value.bytes());
        };
    }
    editors_.require(state->mounted.editor).set_edit_transform(std::move(transform));
}

bool InputComponentHost::update_count(runtime::ComponentId component) {
    auto* state = host_->components().state<InputState>(component);
    if (!state) {
        return false;
    }
    if (!state->count_ready) {
        return true;
    }
    auto* editor = editors_.find(state->mounted.editor);
    if (!editor) {
        return false;
    }
    if (state->count_revision == editor->revision()) {
        return true;
    }
    if (state->counting) {
        return false;
    }
    const auto editor_id = editor->id();
    const auto revision = editor->revision();
    const auto generation = state->count_generation;
    const auto options = state->count_options;
    const auto maximum = options.max ? options.max : state->hard_max;
    const bool show = state->show_count;
    auto strategy = state->count_strategy;
    auto formatter = state->count_formatter;
    const bool needed = show || options.max.has_value();
    auto value = needed ? String::from_utf8(editor->value()).value() : String{};
    state->counting = true;
    std::size_t count{};
    String label;
    try {
        count = needed ? detail::count_value(value.view(), options.unit, strategy) : 0;
        if (show) {
            if (formatter) {
                try {
                    label = formatter({std::move(value), count, maximum, maximum && count > *maximum});
                } catch (...) {
                    if (const auto* current = host_->components().state<InputState>(component)) {
                        label = current->count_text.get();
                    }
                }
            } else {
                auto text = std::to_string(count);
                if (maximum) {
                    text += " / " + std::to_string(*maximum);
                }
                label = String::from_utf8(text).value();
            }
        }
    } catch (...) {
        // A display formatter is presentation only. Retain the preceding label
        // and avoid retrying a throwing callback on every frame.
        if (auto* current = host_->components().state<InputState>(component)) {
            current->counting = false;
            if (current->count_generation == generation && editors_.find(editor_id) &&
                editors_.require(editor_id).revision() == revision) {
                current->count_revision = revision;
            }
        }
        return host_->components().state<InputState>(component) != nullptr;
    }
    state = host_->components().state<InputState>(component);
    editor = editors_.find(editor_id);
    if (!state) {
        return false;
    }
    state->counting = false;
    if (!editor || editor->revision() != revision || state->count_generation != generation) {
        invalidate(component, text_dirty);
        return false;
    }
    state->count_revision = revision;
    state->count_value = count;
    const bool exceeded = options.max && count > *options.max;
    if (state->count_exceeded != exceeded) {
        state->count_exceeded = exceeded;
        invalidate(component, runtime::DirtyFlags::Material);
    }
    const bool was_present = !state->count_text.get().empty();
    const bool present = !label.empty();
    if (state->count_node.valid() && was_present != present) {
        auto& node = host_->nodes().require(state->count_node);
        node.external_layout.width = present ? std::nullopt : std::optional{0.0F};
        node.external_layout.height = present ? std::nullopt : std::optional{0.0F};
        layout::BoxLayout box;
        box.padding.left = present ? 4.0F : 0.0F;
        host_->layout().set_layout(state->count_node, box);
        host_->dirty().invalidate(state->count_node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                         runtime::DirtyFlags::Geometry);
    }
    state->count_text.set(std::move(label));
    update_suffix_layout(component);
    return host_->components().state<InputState>(component) != nullptr;
}

void InputComponentHost::clear(runtime::ComponentId component) {
    auto* state = host_->components().state<InputState>(component);
    if (!state || !state->allow_clear || state->clear_disabled || state->disabled || state->read_only) {
        return;
    }
    const auto editor_id = state->mounted.editor;
    auto callback = state->on_clear;
    auto& editor = editors_.require(editor_id);
    if (editor.value().empty()) {
        return;
    }
    if (editor.composition().active) {
        static_cast<void>(sessions_.cancel_composition());
    }
    const auto result = editor.replace_range({0, editor.value().size()}, {});
    check(result);
    if (result.value_changed) {
        notify_change(editor_id);
        if (callback) {
            callback();
        }
    }
}

InputLayoutSnapshot InputComponentHost::layout_snapshot(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if (!state) {
        throw std::out_of_range("Input component is stale");
    }
    return state->geometry;
}

TextSceneId InputComponentHost::text_scene(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if (!state) {
        throw std::out_of_range("Input component is stale");
    }
    return state->text_scene;
}

InputTextLayers InputComponentHost::text_layers(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if (!state) {
        throw std::out_of_range("Input component is stale");
    }
    return {state->text_scene, state->selected_scene, state->placeholder_scene};
}

InputDisplaySnapshot InputComponentHost::display_snapshot(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if (!state) {
        throw std::out_of_range("Input component is stale");
    }
    return state->display.snapshot();
}

InputStatus InputComponentHost::status(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if (!state) {
        throw std::out_of_range("Input component is stale");
    }
    return state->status == InputStatus::Default && state->count_exceeded ? InputStatus::Error : state->status;
}

ControlSize InputComponentHost::size(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if (!state) {
        throw std::out_of_range("Input component is stale");
    }
    return state->size;
}

std::optional<std::array<bool, 4>> InputComponentHost::compact_corners(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if (!state) {
        throw std::out_of_range("Input component is stale");
    }
    return state->compact ? std::optional{state->compact->corners} : std::nullopt;
}

InputVariant InputComponentHost::variant(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if (!state) {
        throw std::out_of_range("Input component is stale");
    }
    return state->variant;
}

String InputComponentHost::count_text(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if (!state) {
        throw std::out_of_range("Input component is stale");
    }
    return state->count_text.get();
}

std::size_t InputComponentHost::count_value(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if (!state) {
        throw std::out_of_range("Input component is stale");
    }
    return state->count_value;
}

const text::TextCaretMap& InputComponentHost::caret_map(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if (!state) {
        throw std::out_of_range("Input component is stale");
    }
    return state->carets;
}

bool InputComponentHost::set_caret_deadline(runtime::ComponentId component,
                                            std::optional<animation::AnimationTime> deadline) {
    auto* state = host_->components().state<InputState>(component);
    if (!state || (deadline &&
                   (!state->focused || state->disabled || state->read_only || !host_->focus().state().window_active))) {
        return false;
    }
    return state->caret_blink.override_deadline(deadline);
}

std::optional<animation::AnimationTime> InputComponentHost::next_caret_deadline() const {
    std::optional<animation::AnimationTime> next;
    for (const auto& mounted : mounted_) {
        const auto* state = host_->components().state<InputState>(mounted.component);
        const auto candidate = state ? state->caret_blink.deadline() : std::nullopt;
        if (candidate && (!next || *candidate < *next)) {
            next = candidate;
        }
    }
    return next;
}

void InputComponentHost::set_horizontal_scroll(runtime::ComponentId component, float offset) {
    if (!std::isfinite(offset)) {
        throw std::invalid_argument("Input scroll must be finite");
    }
    auto* state = host_->components().state<InputState>(component);
    if (!state) {
        return;
    }
    const auto next =
        std::clamp(offset, 0.0F, std::max(0.0F, state->geometry.text_width - state->geometry.viewport.width));
    if (next == state->geometry.scroll_offset) {
        return;
    }
    state->geometry.scroll_offset = next;
    invalidate(component, runtime::DirtyFlags::Geometry);
}

void InputComponentHost::synchronize_auxiliary_geometry(runtime::Size window, runtime::Rect clip) {
    struct ResizeNotification {
        runtime::ComponentId component;
        TextAreaSize size;
        std::function<void(TextAreaSize)> callback;
    };

    std::optional<std::vector<ResizeNotification>> resize_notifications;
    if (!auto_focus_requests_.empty()) {
        const auto autofocus = std::exchange(auto_focus_requests_, {});
        for (const auto component : autofocus) {
            static_cast<void>(focus(component, {}));
        }
    }
    const auto profile_started =
        sync_profiling_enabled_ ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto record_phase = [this](auto started, std::uint64_t& total) {
        if (sync_profiling_enabled_) {
            total += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started)
                    .count());
        }
    };
    for (const auto& mounted : mounted_) {
        if (sync_profiling_enabled_) {
            ++sync_profile_.mounted_visited;
        }
        auto* state = host_->components().state<InputState>(mounted.component);
        if (!state || !state->active || !host_->components().branch_active(mounted.component)) {
            continue;
        }
        const auto update_started =
            sync_profiling_enabled_ ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        update_text(mounted.component, false, false);
        record_phase(update_started, sync_profile_.update_text_nanoseconds);
        auto& text_scene = host_->text().scene_service();
        if (state->carets.revision() != text_scene.text_state(state->text_scene).revision() &&
            !(state->textarea ? text_scene.synchronize_line_caret_map(state->text_scene, state->carets)
                              : text_scene.synchronize_caret_map(state->text_scene, state->carets))) {
            throw std::runtime_error("Input caret mapping failed");
        }
        auto viewport = translated_bounds(host_->nodes(), state->viewport);
        const auto right = std::min(viewport.x + viewport.width, clip.x + clip.width);
        const auto bottom = std::min(viewport.y + viewport.height, clip.y + clip.height);
        const auto left = std::max(viewport.x, clip.x);
        const auto top = std::max(viewport.y, clip.y);
        const auto& measurement = host_->text().scene_service().text_state(state->text_scene).measurement();
        state->geometry.viewport = viewport;
        state->geometry.clip =
            pixel_clip({left, top, std::max(0.0F, right - left), std::max(0.0F, bottom - top)}, display_scale_);
        state->geometry.baseline = viewport.y + measurement.first_baseline;
        state->geometry.text_width = measurement.width;
        state->geometry.text_height = measurement.height;
        const float thickness = std::max(1.0F, std::round(display_scale_)) / display_scale_;
        const auto caret_clip = pixel_clip(viewport, display_scale_);
        const float usable_width = std::max(0.0F, caret_clip.x + caret_clip.width - viewport.x);
        const auto scroll =
            state->textarea && !state->textarea->reveal_caret
                ? std::clamp(state->geometry.scroll_offset, 0.0F, std::max(0.0F, measurement.width - usable_width))
                : state->display
                      .scroll_for_caret(state->carets, state->carets.revision(), usable_width,
                                        state->geometry.scroll_offset, thickness, state->affinity)
                      .value();
        // Whole physical pixels preserve the cached glyph raster phase.
        state->geometry.scroll_offset = std::ceil(scroll * display_scale_) / display_scale_;
        if (state->otp && measurement.width < usable_width) {
            state->geometry.scroll_offset =
                -std::round((usable_width - measurement.width) * .5F * display_scale_) / display_scale_;
        }
        const auto display = state->display.snapshot();
        const auto caret_stop = state->carets.at(display.caret, state->carets.revision(), state->affinity).value();
        if (state->textarea) {
            auto& area = *state->textarea;
            const auto row_top = static_cast<float>(caret_stop.line) * state->typography.line_height;
            auto scroll_y =
                std::clamp(area.vertical_scroll, 0.0F, std::max(0.0F, measurement.height - viewport.height));
            if (area.reveal_caret) {
                if (row_top < scroll_y) {
                    scroll_y = row_top;
                } else if (row_top + state->typography.line_height > scroll_y + viewport.height) {
                    scroll_y = row_top + state->typography.line_height - viewport.height;
                }
            }
            area.vertical_scroll = std::ceil(std::max(0.0F, scroll_y) * display_scale_) / display_scale_;
            area.reveal_caret = false;
            state->geometry.vertical_scroll = area.vertical_scroll;
            state->geometry.baseline = viewport.y + caret_stop.baseline - area.vertical_scroll;
        }
        const auto x = [&](std::size_t byte) {
            return viewport.x + state->carets.at(byte, state->carets.revision()).value().x -
                   state->geometry.scroll_offset;
        };
        state->geometry.caret_x = viewport.x + caret_stop.x - state->geometry.scroll_offset;
        const auto range_extents = [&](input::TextSelection range) {
            auto first = x(range.begin());
            auto last = first;
            bool covered{};
            check(state->carets.visit_coverage(
                range.begin(), range.end(), state->carets.revision(), [&](const auto& piece) {
                    const auto left = viewport.x + piece.x - state->geometry.scroll_offset;
                    first = covered ? std::min(first, left) : left;
                    last = covered ? std::max(last, left + piece.width) : left + piece.width;
                    covered = true;
                }));
            return std::pair{first, last};
        };
        const auto selected_extents = range_extents(display.selection);
        const auto composition_extents = range_extents(display.composition);
        state->geometry.selection_start = selected_extents.first;
        state->geometry.selection_end = selected_extents.second;
        state->geometry.composition_start = composition_extents.first;
        state->geometry.composition_end = composition_extents.second;
        const auto snap = [&](float value) {
            return std::round(value * display_scale_) / display_scale_;
        };
        const auto caret_left = std::clamp(snap(state->geometry.caret_x), caret_clip.x,
                                           std::max(caret_clip.x, caret_clip.x + caret_clip.width - thickness));
        const auto caret_top = state->textarea
                                   ? viewport.y + static_cast<float>(caret_stop.line) * state->typography.line_height -
                                         state->textarea->vertical_scroll
                                   : caret_clip.y;
        const auto caret_height = state->textarea ? state->typography.line_height : caret_clip.height;
        state->geometry.caret = {caret_left, caret_top, std::min(thickness, caret_clip.width), caret_height};
        const auto underline_left = snap(state->geometry.composition_start);
        state->geometry.underline = {underline_left,
                                     std::max(caret_clip.y, caret_clip.y + caret_clip.height - thickness),
                                     std::max(0.0F, snap(state->geometry.composition_end) - underline_left),
                                     std::min(thickness, caret_clip.height)};
        const auto& theme = host_->components().theme_scope(mounted.component)->snapshot();
        auto root_bounds = translated_bounds(host_->nodes(), mounted.node);
        if (state->textarea) {
            root_bounds.height = std::max(0.0F, root_bounds.height - state->textarea->footer_height);
            const TextAreaSize actual{root_bounds.width, root_bounds.height};
            auto& area = *state->textarea;
            if (area.published_size != actual) {
                area.published_size = actual;
                if (area.on_resize) {
                    if (!resize_notifications) {
                        resize_notifications.emplace();
                    }
                    resize_notifications->push_back({mounted.component, actual, area.on_resize});
                }
            }
        }
        state->next_container_clip = clip;
        const auto theme_started =
            sync_profiling_enabled_ ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        const auto& tokens = derive_input_tokens(theme);
        const float border = std::min(tokens.border_width, 0.5F * std::min(root_bounds.width, root_bounds.height));
        const float radius = state->variant == InputVariant::Underlined ? 0.0F : tokens.size(state->size).border_radius;
        const auto visual = resolve_visuals(*state, tokens);
        record_phase(theme_started, sync_profile_.theme_nanoseconds);
        const auto& presentation = state->transition->value();
        std::array<Color, input_shadow_layer_capacity> shadow_colors;
        std::copy_n(presentation.colors.begin() + 8, input_shadow_layer_capacity, shadow_colors.begin());
        const InputContainerPresentation next_container{root_bounds,
                                                        clip,
                                                        radius,
                                                        border,
                                                        presentation.colors[0],
                                                        presentation.colors[1],
                                                        *visual.shadow,
                                                        shadow_colors,
                                                        presentation.shadow_opacity,
                                                        state->compact ? state->compact->corners
                                                                       : std::array{true, true, true, true},
                                                        state->variant,
                                                        visual.border,
                                                        tokens.focus_width,
                                                        state->focus_visible && !state->disabled};
        if (state->container_presentation != next_container) {
            const std::size_t stride = state->compact ? 4 : 1;
            for (std::size_t layer = 0; layer < input_effect_layer_count; ++layer) {
                const bool outer = layer < input_shadow_layer_capacity;
                const bool inner = layer >= input_inset_shadow_layer && layer < input_focus_layer;
                auto bounds = root_bounds;
                const float inset = layer == input_background_layer || inner ? border : 0.0F;
                bounds.x += inset;
                bounds.y += inset;
                bounds.width -= 2 * inset;
                bounds.height -= 2 * inset;
                graphics::RoundedEffectInstance effect;
                effect.geometry.shape = {
                    bounds, std::clamp(radius - inset, 0.0F, 0.5F * std::min(bounds.width, bounds.height))};
                effect.geometry.ancestor_clip = graphics::EffectClip{1, clip};
                effect.material = {layer == input_background_layer ? presentation.colors[0] : presentation.colors[1],
                                   layer == input_border_layer || layer == input_background_layer ? 1.0F : 0.0F, true};
                if (layer == input_border_layer && state->variant == InputVariant::Borderless) {
                    effect.material.opacity = 0;
                }
                if (layer == input_background_layer && state->variant != InputVariant::Outlined) {
                    effect.geometry.shape = {
                        root_bounds, std::clamp(radius, 0.0F, 0.5F * std::min(root_bounds.width, root_bounds.height))};
                    if (state->variant == InputVariant::Underlined) {
                        effect.geometry.shape.rect.height = std::max(0.0F, root_bounds.height - border);
                    }
                }
                if (layer == input_border_layer && state->variant == InputVariant::Underlined) {
                    effect.geometry.shape = {
                        {root_bounds.x, root_bounds.y + root_bounds.height - border, root_bounds.width, border}, 0};
                } else if (layer == input_border_layer && state->variant == InputVariant::Filled) {
                    effect.geometry.shape = {
                        {root_bounds.x + border, root_bounds.y + border, root_bounds.width - 2 * border,
                         root_bounds.height - 2 * border},
                        std::clamp(radius - border, 0.0F,
                                   0.5F * std::min(root_bounds.width - 2 * border, root_bounds.height - 2 * border))};
                    effect.geometry.kind = graphics::RoundedEffectKind::outline;
                    effect.geometry.outline_width = std::max(0.001F, border);
                    effect.material.opacity = border > 0 ? 1.0F : 0.0F;
                }
                if (outer || inner) {
                    const auto slot = outer ? layer : layer - input_inset_shadow_layer;
                    const auto source = input_shadow_layer_capacity - 1 - slot;
                    if (source < visual.shadow->size()) {
                        const auto& shadow = (*visual.shadow)[source];
                        if ((shadow.kind == ShadowKind::outer) == outer) {
                            effect = graphics::make_shadow_effect(effect.geometry.shape, shadow, {},
                                                                  graphics::EffectClip{1, clip});
                            effect.material.color = shadow_colors[source];
                            effect.material.opacity = presentation.shadow_opacity;
                        }
                    }
                } else if (layer == input_focus_layer) {
                    // A -lineWidth CSS outline offset is an inset shape whose
                    // positive outline reaches lineWidthFocus outside that shape.
                    const float focus_inset = state->variant == InputVariant::Borderless ? border : 0.0F;
                    effect.geometry.shape = {{root_bounds.x + focus_inset, root_bounds.y + focus_inset,
                                              root_bounds.width - 2 * focus_inset,
                                              root_bounds.height - 2 * focus_inset},
                                             std::clamp(radius - focus_inset, 0.0F,
                                                        0.5F * std::min(root_bounds.width - 2 * focus_inset,
                                                                        root_bounds.height - 2 * focus_inset))};
                    effect.geometry.kind = graphics::RoundedEffectKind::outline;
                    effect.geometry.outline_width = std::max(0.001F, tokens.focus_width);
                    effect.geometry.outline_offset = 0;
                    effect.material.color = visual.border;
                    effect.material.opacity = state->variant == InputVariant::Borderless && state->focus_visible &&
                                                      !state->disabled && tokens.focus_width > 0
                                                  ? 1.0F
                                                  : 0.0F;
                }
                std::array<graphics::RoundedEffectInstance, 4> corners;
                if (state->compact) {
                    if (outer || inner) {
                        const ShadowLayer shadow{effect.geometry.kind == graphics::RoundedEffectKind::inset_shadow
                                                     ? ShadowKind::inset
                                                     : ShadowKind::outer,
                                                 effect.geometry.offset, effect.geometry.blur, effect.geometry.spread,
                                                 effect.material.color};
                        corners = graphics::make_corner_shadow_effects(effect.geometry.shape, state->compact->corners,
                                                                       shadow, {}, effect.geometry.ancestor_clip);
                        for (auto& corner : corners) {
                            corner.material = effect.material;
                        }
                    } else if (effect.geometry.kind == graphics::RoundedEffectKind::outline) {
                        corners = graphics::make_corner_outline_effects(
                            effect.geometry.shape, state->compact->corners, effect.geometry.outline_width,
                            effect.geometry.outline_offset, effect.material.color, effect.material.opacity, {},
                            effect.geometry.ancestor_clip);
                    } else {
                        corners = graphics::make_corner_fill_effects(effect.geometry.shape, state->compact->corners,
                                                                     effect.material.color, effect.material.opacity, {},
                                                                     effect.geometry.ancestor_clip);
                    }
                }
                for (std::size_t corner = 0; corner < stride; ++corner) {
                    const auto& candidate = state->compact ? corners[corner] : effect;
                    const auto id = state->container_effects[layer * stride + corner];
                    static_cast<void>(host_->rounded_effects().update_geometry(id, candidate.geometry));
                    static_cast<void>(host_->rounded_effects().update_material(id, candidate.material));
                }
            }
            state->container_presentation = next_container;
        }
        const auto foreground = channels(presentation.colors[2]);
        const auto selection_color = presentation.colors[6];
        state->selection_quads.clear();
        state->overlay_quads.clear();
        state->selected_clips.clear();
        const auto coverage_rect = [&](const text::TextCoverageSegment& piece) {
            return runtime::Rect{
                viewport.x + piece.x - state->geometry.scroll_offset,
                viewport.y + (state->textarea ? static_cast<float>(piece.line) * state->typography.line_height : 0) -
                    state->geometry.vertical_scroll,
                piece.width, state->textarea ? state->typography.line_height : viewport.height};
        };
        const bool selection_visible = state->focused && !state->disabled && !display.placeholder;
        check(state->carets.visit_coverage(
            display.selection.begin(), display.selection.end(), state->carets.revision(), [&](const auto& piece) {
                const auto selected = graphics::intersect_effect_bounds(state->geometry.clip, coverage_rect(piece));
                state->selected_clips.push_back(
                    {piece.line, display.selection.begin(), display.selection.end(), selected});
                if (selected.width > 0 && selected.height > 0 && selection_visible) {
                    state->selection_quads.push_back(clipped_quad(selected, state->geometry.clip, window,
                                                                  {selection_color.red(), selection_color.green(),
                                                                   selection_color.blue(), selection_color.alpha()},
                                                                  1));
                }
            }));
        if (display.composing) {
            check(state->carets.visit_coverage(display.composition.begin(), display.composition.end(),
                                               state->carets.revision(), [&](const auto& piece) {
                                                   auto underline = coverage_rect(piece);
                                                   underline.y += std::max(0.0F, underline.height - thickness);
                                                   underline.height = std::min(thickness, underline.height);
                                                   underline = graphics::intersect_effect_bounds(state->geometry.clip,
                                                                                                 underline);
                                                   if (underline.width > 0 && underline.height > 0) {
                                                       state->overlay_quads.push_back(clipped_quad(
                                                           underline, state->geometry.clip, window, foreground, 1));
                                                   }
                                               }));
        }
        if (!state->textarea && state->selection_quads.empty()) {
            state->selection_quads.push_back(clipped_quad({}, state->geometry.clip, window, {}, 0));
        }
        if (!state->textarea && state->overlay_quads.empty()) {
            state->overlay_quads.push_back(
                clipped_quad(state->geometry.underline, state->geometry.clip, window, foreground, 0));
        }
        state->overlay_quads.push_back(
            clipped_quad(state->geometry.caret, state->geometry.clip, window, channels(presentation.colors[4]),
                         state->focused && !state->disabled && !state->read_only &&
                                 host_->focus().state().window_active && state->caret_blink.visible()
                             ? 1.0F
                             : 0.0F));
        if (state->textarea) {
            auto& area = *state->textarea;
            if (area.resize != TextAreaResize::None && !area.auto_size.enabled && !state->disabled) {
                const auto grip_color = channels(tokens.colors.border);
                for (int diagonal = 0; diagonal < 3; ++diagonal) {
                    for (int point = 0; point <= diagonal; ++point) {
                        const auto gx = root_bounds.x + root_bounds.width - 3 - static_cast<float>(point) * 3;
                        const auto gy =
                            root_bounds.y + root_bounds.height - 3 - static_cast<float>(diagonal - point) * 3;
                        state->overlay_quads.push_back(clipped_quad({gx, gy, 1.5F, 1.5F}, clip, window, grip_color, 1));
                    }
                }
            }
        }
        static_cast<void>(host_->surfaces().update_content_range(state->selection_surface, state->selection_quads));
        static_cast<void>(host_->surfaces().update_content_range(state->overlay_surface, state->overlay_quads));
        static_cast<void>(text_scene.set_coverage_clips(state->selected_scene, state->selected_clips));
        text_scene.set_color(state->text_scene, foreground);
        text_scene.set_color(state->selected_scene, channels(presentation.colors[7]));
        text_scene.set_color(state->placeholder_scene, channels(presentation.colors[5]));
        text_scene.set_opacity(state->text_scene, display.placeholder ? 0.0F : 1.0F);
        text_scene.set_opacity(state->selected_scene, state->focused && !state->disabled && !display.placeholder &&
                                                              display.selection.begin() != display.selection.end()
                                                          ? 1.0F
                                                          : 0.0F);
        text_scene.set_opacity(state->placeholder_scene, display.placeholder ? 1.0F : 0.0F);
        const auto& viewport_node = host_->nodes().require(state->viewport);
        graphics::GlyphPlacement placement{
            {viewport_node.bounds.x, viewport_node.bounds.y}, window, state->geometry.clip};
        const auto scene_started =
            sync_profiling_enabled_ ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        for (const auto id : {state->text_scene, state->selected_scene, state->placeholder_scene}) {
            if (sync_profiling_enabled_) {
                ++sync_profile_.text_scene_calls;
            }
            auto layer_placement = placement;
            // Input's horizontal caret scroll is already aligned to the window
            // display scale; retain that exact offset when adding node motion.
            layer_placement.translation_pixels = text_scene.set_phase_preserving_scroll_translation(
                id, viewport_node.translation, {-state->geometry.scroll_offset, -state->geometry.vertical_scroll});
            if (!text_scene.synchronize(id, layer_placement)) {
                throw std::runtime_error("Input glyph synchronization failed");
            }
        }
        record_phase(scene_started, sync_profile_.text_scene_nanoseconds);
        if (const auto compact = state->compact_context.lock()) {
            compact->publish_seams();
        }
    }
    record_phase(profile_started, sync_profile_.total_nanoseconds);
    if (resize_notifications) {
        for (const auto& notification : *resize_notifications) {
            if (host_->components().contains(notification.component) &&
                host_->components().branch_active(notification.component)) {
                notification.callback(notification.size);
            }
        }
    }
}

bool InputComponentHost::synchronize_auxiliary_fragments() {
    bool changed{};
    for (const auto& mounted : mounted_) {
        auto* state = host_->components().state<InputState>(mounted.component);
        if (!state || !state->active || !host_->components().branch_active(mounted.component)) {
            continue;
        }
        std::array<graphics::SceneDrawCommand, input_effect_layer_count * 4> container;
        std::size_t count{};
        const auto stride = state->compact ? 4U : 1U;
        for (std::size_t layer = 0; layer < input_effect_layer_count; ++layer) {
            const auto paint_layer =
                state->variant == InputVariant::Filled && layer == input_border_layer       ? input_background_layer
                : state->variant == InputVariant::Filled && layer == input_background_layer ? input_border_layer
                                                                                            : layer;
            for (std::size_t corner = 0; corner < stride; ++corner) {
                const auto effect = state->container_effects[paint_layer * stride + corner];
                if (const auto index = host_->rounded_effects().packed_index(effect)) {
                    container[count++] = {graphics::SceneDrawKind::rounded_effect, *index, 1};
                }
            }
        }
        const auto commands = std::span{container}.first(count);
        if (!std::ranges::equal(commands, state->container_commands) ||
            state->container_clip != state->next_container_clip) {
            host_->scene_composer().set_fragment(state->container_fragment, commands, mounted.interaction,
                                                 state->next_container_clip);
            state->container_commands.assign(commands.begin(), commands.end());
            state->container_clip = state->next_container_clip;
            changed = true;
        }
        auto& pending = state->pending_text_commands;
        pending.clear();
        for (const auto id : {state->text_scene, state->selected_scene, state->placeholder_scene}) {
            for (const auto& range : host_->text().scene_service().primitive(id).draw_ranges) {
                pending.push_back(
                    {graphics::SceneDrawKind::glyph, range.instances.first, range.instances.count, range.atlas_page});
            }
        }
        if (pending == state->text_commands) {
            continue;
        }
        host_->scene_composer().set_fragment(state->text_fragment, pending);
        state->text_commands.swap(pending);
        changed = true;
    }
    return changed;
}

void InputComponentHost::notify_change(input::TextInputOwnerId editor_id) {
    const auto found = std::find_if(mounted_.begin(), mounted_.end(),
                                    [editor_id](const auto& value) { return value.editor == editor_id; });
    if (found == mounted_.end()) {
        return;
    }
    const auto component = found->component;
    auto* state = host_->components().state<InputState>(component);
    auto* editor = editors_.find(editor_id);
    if (!state || !editor) {
        return;
    }
    // Own both callback and value across synchronous Signal echoes or self-unmount.
    auto callback = state->on_change;
    auto value = String::from_utf8(editor->value()).value();
    check(editor->note_emitted_value());
    update_text(component);
    if (callback && host_->components().state<InputState>(component) && editors_.find(editor_id)) {
        callback(std::move(value));
    }
}

void InputComponentHost::notify_otp_edit(input::TextInputOwnerId editor_id, bool succeeded) {
    const auto found = std::ranges::find(mounted_, editor_id, &MountedInputComponent::editor);
    if (found == mounted_.end()) {
        return;
    }
    const auto* state = host_->components().state<InputState>(found->component);
    if (state && state->otp) {
        auto callback = succeeded ? state->otp->committed : state->otp->aborted;
        if (callback) {
            callback();
        }
    }
}

input::TextEditResult InputComponentHost::dispatch(const input::TextCommitted& event) {
    auto result = sessions_.dispatch(event);
    notify_otp_edit(event.session.owner, bool(result));
    if (result) {
        for (const auto& mounted : mounted_) {
            if (mounted.editor == event.session.owner) {
                update_caret(mounted.component, true);
            }
        }
    }
    if (result.value_changed) {
        notify_change(event.session.owner);
    } else if (result) {
        for (const auto& mounted : mounted_) {
            if (mounted.editor == event.session.owner) {
                update_text(mounted.component, false);
            }
        }
    }
    return result;
}

input::TextEditResult InputComponentHost::dispatch(const input::CompositionChanged& event) {
    auto result = sessions_.dispatch(event);
    if (result) {
        for (const auto& mounted : mounted_) {
            if (mounted.editor == event.session.owner) {
                update_caret(mounted.component, true);
                update_text(mounted.component, false);
            }
        }
    }
    return result;
}

input::TextEditResult InputComponentHost::dispatch(const input::CandidatesChanged& event) {
    return sessions_.dispatch(event);
}

void InputComponentHost::set_active(runtime::ComponentId component, bool active) {
    auto* state = host_->components().state<InputState>(component);
    if (!state || state->active == active) {
        return;
    }
    state->active = active;
    host_->components().set_branch_active(component, active);
    editors_.require(state->mounted.editor).set_eligibility(state->disabled || !active, state->read_only);
    static_cast<void>(host_->interactions().set_eligible(state->mounted.interaction, active && !state->disabled));
    if (!active) {
        host_->pointer().cancel_interaction(state->mounted.interaction);
        host_->focus().cancel_interaction(state->mounted.interaction);
        state = host_->components().state<InputState>(component);
        if (!state) {
            return;
        }
        if (sessions_.active().owner == state->mounted.editor) {
            static_cast<void>(sessions_.blur());
        }
        state->focused = false;
        state->caret_blink.stop();
        state->selecting_pointer.reset();
        state->hovering_pointers = 0;
        for (auto effect : state->container_effects) {
            auto material = host_->rounded_effects().at(effect).material;
            material.visible = false;
            static_cast<void>(host_->rounded_effects().update_material(effect, material));
        }
    }
    host_->mark_scene_structure_dirty();
    invalidate(component, text_dirty | runtime::DirtyFlags::HitTest);
}

void InputComponentHost::configure_typography_editor(runtime::ComponentId component,
                                                     Prop<runtime::SemanticTypography> typography, Prop<bool> active,
                                                     std::function<void(String)> commit, std::function<void()> cancel,
                                                     std::function<void(String)> blur) {
    auto* state = host_->components().state<InputState>(component);
    if (!state) {
        throw std::invalid_argument("typography editor requires a mounted Input");
    }
    state->on_internal_commit = std::move(commit);
    state->on_internal_cancel = std::move(cancel);
    state->on_internal_blur = std::move(blur);
    auto& scope = host_->components().scope(component);
    static_cast<void>(connect_prop(scope, typography, [this, component](runtime::SemanticTypography value) {
        if (auto* state = host_->components().state<InputState>(component)) {
            state->inherited_typography = value;
            update_theme(component);
        }
    }));
    static_cast<void>(connect_prop(scope, active, [this, component](bool value) { set_active(component, value); }));
}

void InputComponentHost::submit(runtime::ComponentId component) {
    const auto* state = host_->components().state<InputState>(component);
    if (!state || !state->active || state->disabled || state->read_only) {
        return;
    }
    auto& editor = editors_.require(state->mounted.editor);
    if (editor.composition().active) {
        return;
    }
    editor.break_history_merge();
    auto callback = state->on_internal_commit ? state->on_internal_commit : state->on_submit;
    if (callback) {
        auto value = String::from_utf8(editor.value()).value();
        callback(std::move(value));
    }
}
} // namespace ryn::detail

namespace ryn {
InputRef::InputRef() : state_(std::make_shared<detail::InputRefState>()) {}

bool InputRef::focus(InputFocusOptions options) const {
    if (!state_) {
        return false;
    }
    state_->ensure_owner();
    const auto callback = state_->focus;
    return callback && callback(options);
}

bool InputRef::blur() const {
    if (!state_) {
        return false;
    }
    state_->ensure_owner();
    const auto callback = state_->blur;
    return callback && callback();
}

bool InputRef::select(std::size_t anchor, std::size_t caret) const {
    if (!state_) {
        return false;
    }
    state_->ensure_owner();
    const auto callback = state_->select;
    return callback && callback(anchor, caret);
}

bool InputRef::bound() const {
    if (!state_) {
        return false;
    }
    state_->ensure_owner();
    return state_->binding.has_value();
}

void Input(InputProps props, std::optional<InputPrefix> prefix, std::optional<InputSuffix> suffix) {
    if (!detail::active_input_host) {
        throw std::logic_error("Input requires an active InputComponentHost");
    }
    detail::InputPropsAccess::mount(*detail::active_input_host, props, prefix, suffix);
}

void TextArea(TextAreaProps props) {
    detail::TextAreaPropsAccess::mount(std::move(props));
}

void Password(PasswordProps props, std::optional<InputPrefix> prefix, std::optional<InputSuffix> suffix) {
    detail::PasswordPropsAccess::mount(std::move(props), std::move(prefix), std::move(suffix));
}
} // namespace ryn
