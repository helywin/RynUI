#pragma once

#include "input/text_clipboard.hpp"
#include "input/text_editor.hpp"

namespace ryn::input {

struct ClipboardCommandResult {
    ClipboardError clipboard_error{ClipboardError::none};
    TextEditResult edit;

    [[nodiscard]] explicit operator bool() const noexcept {
        return clipboard_error == ClipboardError::none && bool(edit);
    }
};

struct ClipboardCommandDiagnostics {
    std::uint64_t copies{};
    std::uint64_t cuts{};
    std::uint64_t pastes{};
    std::uint64_t failures{};
    std::uint64_t value_changes{};
};

class TextClipboardCommands final {
public:
    TextClipboardCommands(TextEditorStore& editors, TextClipboard& clipboard) noexcept
        : editors_(&editors), clipboard_(&clipboard) {}

    [[nodiscard]] ClipboardCommandResult copy(TextInputOwnerId);
    [[nodiscard]] ClipboardCommandResult cut(TextInputOwnerId);
    [[nodiscard]] ClipboardCommandResult paste(TextInputOwnerId);
    [[nodiscard]] const ClipboardCommandDiagnostics& diagnostics() const;

private:
    enum class Command { copy, cut, paste };
    ClipboardCommandResult execute(TextInputOwnerId, Command);
    ClipboardCommandResult perform(TextInputOwnerId, Command);
    TextEditorStore* editors_;
    TextClipboard* clipboard_;
    ClipboardCommandDiagnostics diagnostics_;
};

} // namespace ryn::input
