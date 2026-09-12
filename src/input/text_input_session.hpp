#pragma once

#include "input/text_editor.hpp"
#include "input/text_input_platform.hpp"

namespace ryn::input {

struct TextInputSessionDiagnostics {
    std::uint64_t starts{}, stops{}, cancels{}, areas{}, stale_events{}, failures{};
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
    TextInputOwnerId focused_;
    TextInputSessionStamp active_;
    TextInputProperties properties_, started_properties_;
    std::optional<WindowTextInputArea> requested_area_, applied_area_;
    TextInputSessionDiagnostics diagnostics_;
    std::uint64_t epoch_{};
    bool window_active_{true};
    bool stop_pending_{};
};

} // namespace ryn::input
