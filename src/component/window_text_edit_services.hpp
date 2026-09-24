#pragma once

#include "input/text_clipboard_commands.hpp"
#include "input/text_input_session.hpp"

namespace ryn::detail {

// The platform text-input session belongs to the window, independent of the
// concrete text control that currently owns the active editor.
class WindowTextEditServices final {
public:
    WindowTextEditServices(input::TextInputPlatform& platform, input::TextClipboard& clipboard)
        : platform_(&platform), clipboard_port_(&clipboard),
          sessions_(editors_, platform), clipboard_(editors_, clipboard) {}
    WindowTextEditServices(const WindowTextEditServices&) = delete;
    WindowTextEditServices& operator=(const WindowTextEditServices&) = delete;

    [[nodiscard]] bool uses(input::TextInputPlatform& platform,
        input::TextClipboard& clipboard) const noexcept {
        return platform_ == &platform && clipboard_port_ == &clipboard;
    }
    [[nodiscard]] input::TextEditorStore& editors() noexcept { return editors_; }
    [[nodiscard]] input::TextInputSessionHost& sessions() noexcept { return sessions_; }
    [[nodiscard]] input::TextClipboardCommands& clipboard() noexcept { return clipboard_; }

private:
    input::TextInputPlatform* platform_;
    input::TextClipboard* clipboard_port_;
    input::TextEditorStore editors_;
    input::TextInputSessionHost sessions_;
    input::TextClipboardCommands clipboard_;
};

} // namespace ryn::detail
