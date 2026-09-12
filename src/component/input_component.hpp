#pragma once

#include "component/button_component.hpp"
#include "component/input_display.hpp"
#include "input/text_input_session.hpp"
#include "input/text_clipboard_commands.hpp"
#include <ryn/input.hpp>

namespace ryn::detail {

struct MountedInputComponent {
    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId interaction;
    input::TextInputOwnerId editor;
};

struct InputLayoutSnapshot {
    runtime::Rect viewport, clip;
    float baseline{}, scroll_offset{}, text_width{};
    float caret_x{}, selection_start{}, selection_end{}, composition_start{}, composition_end{};
};

// The containing component host and platform ports must outlive this host.
class InputComponentHost final : private AuxiliaryComponentSynchronizer {
public:
    InputComponentHost(ButtonComponentHost&, input::TextInputPlatform&, input::TextClipboard&);
    ~InputComponentHost();
    InputComponentHost(const InputComponentHost&) = delete;
    InputComponentHost& operator=(const InputComponentHost&) = delete;
    void mount(const Content&);
    void dispose() noexcept;
    void set_window_active(bool);
    [[nodiscard]] std::span<const MountedInputComponent> mounted_inputs() const noexcept { return mounted_; }
    [[nodiscard]] input::TextEditorStore& editors() noexcept { return editors_; }
    [[nodiscard]] input::TextInputSessionHost& sessions() noexcept { return sessions_; }
    [[nodiscard]] input::TextEditResult dispatch(const input::TextCommitted&);
    [[nodiscard]] input::TextEditResult dispatch(const input::CompositionChanged&);
    [[nodiscard]] input::TextEditResult dispatch(const input::CandidatesChanged&);
    void submit(runtime::ComponentId);
    [[nodiscard]] InputLayoutSnapshot layout_snapshot(runtime::ComponentId) const;
    void set_horizontal_scroll(runtime::ComponentId, float offset);
    [[nodiscard]] TextSceneId text_scene(runtime::ComponentId) const;
    [[nodiscard]] InputDisplaySnapshot display_snapshot(runtime::ComponentId) const;
    [[nodiscard]] const text::TextCaretMap& caret_map(runtime::ComponentId) const;
    // Retained owner-scoped deadline slot. Blink policy/ticking is layered on it.
    bool set_caret_deadline(runtime::ComponentId, std::optional<animation::AnimationTime>);
    [[nodiscard]] std::optional<animation::AnimationTime> next_caret_deadline() const;
private:
    friend struct InputPropsAccess;
    void notify_change(input::TextInputOwnerId);
    void invalidate(runtime::ComponentId, runtime::DirtyFlags);
    void update_text(runtime::ComponentId, bool measure_layout = true);
    void update_theme(runtime::ComponentId);
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    ButtonComponentHost* host_;
    input::TextEditorStore editors_;
    input::TextInputSessionHost sessions_;
    input::TextClipboardCommands clipboard_;
    std::vector<MountedInputComponent> mounted_;
};

} // namespace ryn::detail
