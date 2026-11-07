#pragma once
#include <functional>
#include <optional>
#include <ryn/component.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>

namespace ryn {
namespace detail {
class ModalComponentHost;
class AppPanelService;
} // namespace detail

enum class DrawerPlacement { Top, Bottom, Left, Right };

enum class DrawerCancelReason { CloseButton, Mask, Escape };

struct DrawerMask final {
    bool enabled{true};
    bool closable{true};
    friend bool operator==(DrawerMask, DrawerMask) = default;
};

struct DrawerResize final {
    bool enabled{true};
    float minimum{180};
    float maximum{960};
    float keyboard_step{10};
    friend bool operator==(DrawerResize, DrawerResize) = default;
};

struct DrawerPush final {
    bool enabled{true};
    float distance{180};
    friend bool operator==(DrawerPush, DrawerPush) = default;
};

struct DrawerContentSlot final {};

struct DrawerTitleSlot final {};

struct DrawerFooterSlot final {};

struct DrawerExtraSlot final {};

using DrawerContent = SlotContent<DrawerContentSlot>;
using DrawerTitle = SlotContent<DrawerTitleSlot>;
using DrawerFooter = SlotContent<DrawerFooterSlot>;
using DrawerExtra = SlotContent<DrawerExtraSlot>;

struct DrawerSlots final {
    std::optional<DrawerTitle> title;
    std::optional<DrawerFooter> footer;
    std::optional<DrawerExtra> extra;
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

    DrawerProps& loading(Prop<bool> value = true) {
        loading_ = std::move(value);
        return *this;
    }

    DrawerProps& lazy(Prop<bool> value = true) {
        lazy_ = std::move(value);
        return *this;
    }

    DrawerProps& destroyOnHidden(Prop<bool> value = true) {
        destroy_hidden_ = std::move(value);
        return *this;
    }

    DrawerProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        return *this;
    }

    DrawerProps& resizable(Prop<DrawerResize> value) {
        resize_ = std::move(value);
        return *this;
    }

    DrawerProps& push(Prop<DrawerPush> value) {
        push_ = std::move(value);
        return *this;
    }

    DrawerProps& onResize(std::function<void(float)> value) {
        on_resize_ = std::move(value);
        return *this;
    }

    DrawerProps& onResizeEnd(std::function<void(float)> value) {
        on_resize_end_ = std::move(value);
        return *this;
    }

private:
    friend class detail::ModalComponentHost;
    friend class detail::AppPanelService;
    Prop<bool> open_{false};
    Prop<String> title_{String{}};
    Prop<DrawerPlacement> placement_{DrawerPlacement::Right};
    Prop<float> width_{378};
    Prop<float> height_{378};
    Prop<bool> closable_{true};
    Prop<bool> keyboard_{true};
    Prop<DrawerMask> mask_{DrawerMask{}};
    Prop<bool> restore_focus_{true};
    Prop<bool> loading_{false};
    Prop<bool> lazy_{false};
    Prop<bool> destroy_hidden_{false};
    Prop<bool> rtl_{false};
    Prop<DrawerResize> resize_{DrawerResize{false}};
    Prop<DrawerPush> push_{DrawerPush{}};
    std::function<void(float)> on_resize_;
    std::function<void(float)> on_resize_end_;
    std::function<void(DrawerCancelReason)> on_cancel_;
    std::function<void()> after_close_;
    std::function<void(bool)> after_open_change_;
};

void Drawer(DrawerProps props, DrawerContent content, DrawerSlots slots = {});
} // namespace ryn
