#pragma once
#include <functional>
#include <optional>
#include <ryn/component.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>

namespace ryn {
namespace detail {
class ModalComponentHost;
}

enum class DrawerPlacement { Top, Bottom, Left, Right };

enum class DrawerCancelReason { CloseButton, Mask, Escape };

struct DrawerMask final {
    bool enabled{true};
    bool closable{true};
    friend bool operator==(DrawerMask, DrawerMask) = default;
};

struct DrawerContentSlot final {};

struct DrawerTitleSlot final {};

struct DrawerFooterSlot final {};

using DrawerContent = SlotContent<DrawerContentSlot>;
using DrawerTitle = SlotContent<DrawerTitleSlot>;
using DrawerFooter = SlotContent<DrawerFooterSlot>;

struct DrawerSlots final {
    std::optional<DrawerTitle> title;
    std::optional<DrawerFooter> footer;
};

class DrawerProps final {
public:
    DrawerProps& open(Prop<bool> value = true) {
        open_ = std::move(value);
        return *this;
    }

    DrawerProps& title(Prop<String> value) {
        title_ = std::move(value);
        return *this;
    }

    template <std::size_t N> DrawerProps& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    DrawerProps& placement(Prop<DrawerPlacement> value) {
        placement_ = std::move(value);
        return *this;
    }

    DrawerProps& width(Prop<float> value) {
        width_ = std::move(value);
        return *this;
    }

    DrawerProps& height(Prop<float> value) {
        height_ = std::move(value);
        return *this;
    }

    DrawerProps& closable(Prop<bool> value = true) {
        closable_ = std::move(value);
        return *this;
    }

    DrawerProps& keyboard(Prop<bool> value = true) {
        keyboard_ = std::move(value);
        return *this;
    }

    DrawerProps& mask(Prop<DrawerMask> value) {
        mask_ = std::move(value);
        return *this;
    }

    DrawerProps& restoreFocusAfterClose(Prop<bool> value = true) {
        restore_focus_ = std::move(value);
        return *this;
    }

    DrawerProps& onCancel(std::function<void(DrawerCancelReason)> value) {
        on_cancel_ = std::move(value);
        return *this;
    }

    DrawerProps& afterClose(std::function<void()> value) {
        after_close_ = std::move(value);
        return *this;
    }

    DrawerProps& afterOpenChange(std::function<void(bool)> value) {
        after_open_change_ = std::move(value);
        return *this;
    }

private:
    friend class detail::ModalComponentHost;
    Prop<bool> open_{false};
    Prop<String> title_{String{}};
    Prop<DrawerPlacement> placement_{DrawerPlacement::Right};
    Prop<float> width_{378};
    Prop<float> height_{378};
    Prop<bool> closable_{true};
    Prop<bool> keyboard_{true};
    Prop<DrawerMask> mask_{DrawerMask{}};
    Prop<bool> restore_focus_{true};
    std::function<void(DrawerCancelReason)> on_cancel_;
    std::function<void()> after_close_;
    std::function<void(bool)> after_open_change_;
};

void Drawer(DrawerProps props, DrawerContent content, DrawerSlots slots = {});
} // namespace ryn
