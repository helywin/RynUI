#pragma once

#include "input/text_editor.hpp"
#include "input/text_input_platform.hpp"

namespace ryn::input {

// Window-owned admission only. Implementations perform an exact editor-generation
// lookup and read native input eligibility; they never invoke application code.
class TextInputAdmission {
public:
    virtual ~TextInputAdmission() = default;
    [[nodiscard]] virtual bool permits(TextInputOwnerId) const noexcept = 0;
};

struct TextInputSessionDiagnostics {
    std::uint64_t starts{};
    std::uint64_t stops{};
    std::uint64_t cancels{};
    std::uint64_t areas{};
    std::uint64_t stale_events{};
    std::uint64_t failures{};
};

class TextInputSessionHost final : private TextEditorObserver {
public:
    TextInputSessionHost(TextEditorStore&, TextInputPlatform&);
    ~TextInputSessionHost();
    TextInputSessionHost(const TextInputSessionHost&) = delete;
    TextInputSessionHost& operator=(const TextInputSessionHost&) = delete;
    bool focus(TextInputOwnerId, TextInputProperties = {});
    bool blur();
    bool set_window_active(bool);
    bool synchronize();
    void bind_admission(const TextInputAdmission*);
    bool cancel_composition();
    bool set_input_area(const TextInputAreaGeometry&);
    [[nodiscard]] TextEditResult dispatch(const TextCommitted&);
    [[nodiscard]] TextEditResult dispatch(const CompositionChanged&);
    [[nodiscard]] TextEditResult dispatch(const CandidatesChanged&);
    [[nodiscard]] TextInputSessionStamp active() const;
    [[nodiscard]] const TextInputSessionDiagnostics& diagnostics() const;

private:
    void ensure_thread() const;
    bool synchronize_impl() noexcept;
    void before_destroy(TextInputOwnerId) noexcept override;
    void eligibility_changed(TextInputOwnerId) noexcept override;
    TextEditorState* recipient(TextInputSessionStamp);
    TextEditorStore* store_;
    TextInputPlatform* platform_;
    const TextInputAdmission* admission_{};
    TextInputOwnerId focused_;
    TextInputSessionStamp active_;
    TextInputProperties properties_;
    TextInputProperties started_properties_;
    std::optional<WindowTextInputArea> requested_area_;
    std::optional<WindowTextInputArea> applied_area_;
    TextInputSessionDiagnostics diagnostics_;
    std::uint64_t epoch_{};
    bool window_active_{true};
    bool stop_pending_{};
};

} // namespace ryn::input
