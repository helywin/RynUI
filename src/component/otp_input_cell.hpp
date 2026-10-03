#pragma once
#include "input/text_editor.hpp"
#include "input/focus_manager.hpp"
#include <ryn/otp.hpp>

namespace ryn::detail {
struct OTPInputCellConfig final {
    Prop<OTPMask> mask{OTPMask{}};
    input::TextEditorState::EditTransform transform;
    std::function<void()> committed;
    std::function<void()> aborted;
    std::function<void()> clicked;
    std::function<bool(const input::KeyboardInputEvent&)> keyboard;
};

void validate_otp_mask(const OTPMask&);
} // namespace ryn::detail
