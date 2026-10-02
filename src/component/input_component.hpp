#pragma once

#include "component/window_component_services.hpp"
#include "component/input_display.hpp"
#include <ryn/input.hpp>

namespace ryn::detail {

inline constexpr std::size_t input_shadow_layer_capacity = ShadowList::capacity;
inline constexpr std::size_t input_border_layer = input_shadow_layer_capacity;
inline constexpr std::size_t input_background_layer = input_border_layer + 1;
inline constexpr std::size_t input_inset_shadow_layer = input_background_layer + 1;
inline constexpr std::size_t input_focus_layer = input_inset_shadow_layer + input_shadow_layer_capacity;
inline constexpr std::size_t input_effect_layer_count = input_focus_layer + 1;

struct MountedInputComponent {
    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId interaction;
    input::TextInputOwnerId editor;
};

struct InputLayoutSnapshot {
    runtime::Rect viewport;
    runtime::Rect clip;
    runtime::Rect caret;
    runtime::Rect underline;
    float baseline{};
    float scroll_offset{};
    float text_width{};
    float caret_x{};
    float selection_start{};
    float selection_end{};
    float composition_start{};
    float composition_end{};
};

struct InputTextLayers {
    TextSceneId base;
    TextSceneId selected;
    TextSceneId placeholder;
};

struct InputSyncProfile final {
    std::uint64_t total_nanoseconds{};
    std::uint64_t update_text_nanoseconds{};
    std::uint64_t theme_nanoseconds{};
    std::uint64_t text_scene_nanoseconds{};
    std::uint64_t mounted_visited{};
    std::uint64_t text_scene_calls{};
};

// The containing component host and platform ports must outlive this host.
class InputComponentHost final : private AuxiliaryComponentSynchronizer {
public:
    InputComponentHost(WindowComponentServices&, input::TextInputPlatform&, input::TextClipboard&);
    ~InputComponentHost();
    InputComponentHost(const InputComponentHost&) = delete;
    InputComponentHost& operator=(const InputComponentHost&) = delete;
    void mount(const Content&);
    void dispose() noexcept;
    void set_window_active(bool);
    // Matches the owning window/font resolver's physical-to-logical scale.
    void set_display_scale(float);
    // Call after layout/scroll synchronization. The window adapter supplies its
    // coordinate transform independently of glyph raster/display scale.
    bool synchronize_input_area(double logical_to_window_scale, int window_width, int window_height);

    [[nodiscard]] std::span<const MountedInputComponent> mounted_inputs() const noexcept {
        return mounted_;
    }

    void set_sync_profiling_enabled(bool enabled) noexcept {
        sync_profiling_enabled_ = enabled;
    }

    void reset_sync_profile() noexcept {
        sync_profile_ = {};
    }

    [[nodiscard]] InputSyncProfile sync_profile() const noexcept {
        return sync_profile_;
    }

    [[nodiscard]] input::TextEditorStore& editors() noexcept {
        return editors_;
    }

    [[nodiscard]] input::TextInputSessionHost& sessions() noexcept {
        return sessions_;
    }

    [[nodiscard]] input::TextEditResult dispatch(const input::TextCommitted&);
    [[nodiscard]] input::TextEditResult dispatch(const input::CompositionChanged&);
    [[nodiscard]] input::TextEditResult dispatch(const input::CandidatesChanged&);
    void submit(runtime::ComponentId);
    void configure_typography_editor(runtime::ComponentId, Prop<runtime::SemanticTypography>, Prop<bool> active,
                                     std::function<void(String)> commit, std::function<void()> cancel,
                                     std::function<void(String)> blur);
    void set_active(runtime::ComponentId, bool);
    [[nodiscard]] InputLayoutSnapshot layout_snapshot(runtime::ComponentId) const;
    void set_horizontal_scroll(runtime::ComponentId, float offset);
    [[nodiscard]] TextSceneId text_scene(runtime::ComponentId) const;
    [[nodiscard]] InputTextLayers text_layers(runtime::ComponentId) const;
    [[nodiscard]] InputDisplaySnapshot display_snapshot(runtime::ComponentId) const;
    [[nodiscard]] InputStatus status(runtime::ComponentId) const;
    [[nodiscard]] ControlSize size(runtime::ComponentId) const;
    [[nodiscard]] InputVariant variant(runtime::ComponentId) const;
    [[nodiscard]] String count_text(runtime::ComponentId) const;
    [[nodiscard]] std::size_t count_value(runtime::ComponentId) const;
    [[nodiscard]] std::optional<std::array<bool, 4>> compact_corners(runtime::ComponentId) const;
    [[nodiscard]] const text::TextCaretMap& caret_map(runtime::ComponentId) const;
    // Internal deadline injection seam for controlled-clock/lifecycle tests.
    bool set_caret_deadline(runtime::ComponentId, std::optional<animation::AnimationTime>);
    [[nodiscard]] std::optional<animation::AnimationTime> next_caret_deadline() const;

private:
    friend struct InputPropsAccess;
    friend struct PasswordPropsAccess;
    void notify_change(input::TextInputOwnerId);
    void clear(runtime::ComponentId);
    void update_clear_visibility(runtime::ComponentId);
    bool update_count(runtime::ComponentId);
    void update_suffix_layout(runtime::ComponentId);
    void configure_count_transform(runtime::ComponentId);
    bool focus(runtime::ComponentId, InputFocusOptions);
    bool blur(runtime::ComponentId);
    bool select(runtime::ComponentId, std::size_t anchor, std::size_t caret);
    void invalidate(runtime::ComponentId, runtime::DirtyFlags);
    void update_text(runtime::ComponentId, bool measure_layout = true, bool refresh_count = true);
    void update_theme(runtime::ComponentId);
    void apply_material_transition(runtime::ComponentId);
    void dispatch_pointer(runtime::ComponentId, input::PointerDispatchContext&);
    bool dispatch_keyboard(runtime::ComponentId, const input::KeyboardInputEvent&);
    void* begin_mount() noexcept override;
    void end_mount(void* previous) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void synchronize_auxiliary_motion() override;
    void prepare_auxiliary_layout() override;
    void update_caret(runtime::ComponentId, bool reset = false);
    std::size_t tick_auxiliary(animation::AnimationTime) override;

    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override {
        return next_caret_deadline();
    }

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    bool synchronize_auxiliary_fragments() override;
    WindowComponentServices* host_;
    WindowTextEditServices* edit_services_;
    input::TextEditorStore& editors_;
    input::TextInputSessionHost& sessions_;
    input::TextClipboardCommands& clipboard_;
    std::vector<MountedInputComponent> mounted_;
    std::vector<runtime::ComponentId> auto_focus_requests_;
    float display_scale_{1.0F};
    bool sync_profiling_enabled_{};
    InputSyncProfile sync_profile_{};
};

} // namespace ryn::detail
