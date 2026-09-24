#pragma once

#include "input/pointer_router.hpp"

#include <optional>

namespace ryn::input {

struct PressableResult final {
    bool pressed_changed{false};
    bool activate{false};
};

// Pointer gesture only. Keyboard policy, enabled state and callbacks belong to
// each component that consumes the resulting activation intent.
class PressableBehavior final {
public:
    [[nodiscard]] bool pressed() const noexcept { return pointer_.has_value(); }

    [[nodiscard]] PressableResult begin(PointerIdentity pointer,
        InteractionId target, bool allowed, bool captured) noexcept {
        if (!allowed || !captured || pointer_ || !target.valid()) return {};
        pointer_ = pointer;
        target_ = target;
        return {true, false};
    }

    [[nodiscard]] PressableResult release(PointerIdentity pointer,
        InteractionId target, bool allowed,
        std::optional<InteractionId> origin,
        std::optional<InteractionId> actual_hit) noexcept {
        if (!pointer_ || *pointer_ != pointer) return {};
        const bool activate = allowed && target_ == target
            && origin == target && actual_hit == target;
        static_cast<void>(reset());
        return {true, activate};
    }

    [[nodiscard]] PressableResult cancel(PointerIdentity pointer) noexcept {
        if (!pointer_ || *pointer_ != pointer) return {};
        static_cast<void>(reset());
        return {true, false};
    }

    [[nodiscard]] bool reset() noexcept {
        const bool changed = pressed();
        pointer_.reset();
        target_ = {};
        return changed;
    }

    [[nodiscard]] PressableResult dispatch(PointerDispatchContext& context,
        InteractionId target, bool allowed) {
        const auto pointer = context.event().pointer;
        const bool primary = context.event().button == PointerButton::primary;
        switch (context.kind()) {
        case PointerEventKind::down:
            if (!primary || !allowed || pressed() || !target.valid()) return {};
            return begin(pointer, target, allowed, context.capture_pointer());
        case PointerEventKind::up: {
            if (!primary || !pointer_ || *pointer_ != pointer) return {};
            const auto result = release(pointer, target, allowed,
                context.press_origin(), context.actual_hit_target());
            static_cast<void>(context.release_pointer_capture());
            return result;
        }
        case PointerEventKind::cancel: {
            const auto result = cancel(pointer);
            static_cast<void>(context.release_pointer_capture());
            return result;
        }
        case PointerEventKind::move:
        case PointerEventKind::enter:
        case PointerEventKind::leave:
            return {};
        }
        return {};
    }

private:
    std::optional<PointerIdentity> pointer_;
    InteractionId target_;
};

} // namespace ryn::input
