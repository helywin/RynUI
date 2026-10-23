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

enum class ModalFooterMode { Default, None };

enum class ModalCancelReason { CancelButton, CloseButton, Mask, Escape };

struct ModalMask final {
    bool enabled{true};
    bool closable{true};
    friend bool operator==(ModalMask, ModalMask) = default;
};

struct ModalContentSlot final {};

struct ModalTitleSlot final {};

struct ModalFooterSlot final {};

using ModalContent = SlotContent<ModalContentSlot>;
using ModalTitle = SlotContent<ModalTitleSlot>;
using ModalFooter = SlotContent<ModalFooterSlot>;

struct ModalSlots final {
    std::optional<ModalTitle> title;
    std::optional<ModalFooter> footer;
};

class ModalProps final {
public:
    ModalProps& open(Prop<bool> value = true) {
        open_ = std::move(value);
        return *this;
    }

    ModalProps& title(Prop<String> value) {
        title_ = std::move(value);
        return *this;
    }

    template <std::size_t N> ModalProps& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    ModalProps& width(Prop<float> value) {
        width_ = std::move(value);
        return *this;
    }

    ModalProps& centered(Prop<bool> value = true) {
        centered_ = std::move(value);
        return *this;
    }

    ModalProps& closable(Prop<bool> value = true) {
        closable_ = std::move(value);
        return *this;
    }

    ModalProps& keyboard(Prop<bool> value = true) {
        keyboard_ = std::move(value);
        return *this;
    }

    ModalProps& mask(Prop<ModalMask> value) {
        mask_ = std::move(value);
        return *this;
    }

    ModalProps& confirmLoading(Prop<bool> value = true) {
        confirm_loading_ = std::move(value);
        return *this;
    }

    ModalProps& footer(Prop<ModalFooterMode> value) {
        footer_ = std::move(value);
        return *this;
    }

    ModalProps& okText(Prop<String> value) {
        ok_text_ = std::move(value);
        return *this;
    }

    template <std::size_t N> ModalProps& okText(const char8_t (&value)[N]) {
        return okText(String{value});
    }

    ModalProps& cancelText(Prop<String> value) {
        cancel_text_ = std::move(value);
        return *this;
    }

    template <std::size_t N> ModalProps& cancelText(const char8_t (&value)[N]) {
        return cancelText(String{value});
    }

    ModalProps& restoreFocusAfterClose(Prop<bool> value = true) {
        restore_focus_ = std::move(value);
        return *this;
    }

    ModalProps& onOk(std::function<void()> value) {
        on_ok_ = std::move(value);
        return *this;
    }

    ModalProps& onCancel(std::function<void(ModalCancelReason)> value) {
        on_cancel_ = std::move(value);
        return *this;
    }

    ModalProps& afterClose(std::function<void()> value) {
        after_close_ = std::move(value);
        return *this;
    }

    ModalProps& afterOpenChange(std::function<void(bool)> value) {
        after_open_change_ = std::move(value);
        return *this;
    }

private:
    friend class detail::ModalComponentHost;
    Prop<bool> open_{false};
    Prop<String> title_{String{}};
    Prop<float> width_{520};
    Prop<bool> centered_{false};
    Prop<bool> closable_{true};
    Prop<bool> keyboard_{true};
    Prop<ModalMask> mask_{ModalMask{}};
    Prop<bool> confirm_loading_{false};
    Prop<ModalFooterMode> footer_{ModalFooterMode::Default};
    Prop<String> ok_text_{String{u8"OK"}};
    Prop<String> cancel_text_{String{u8"Cancel"}};
    Prop<bool> restore_focus_{true};
    std::function<void()> on_ok_;
    std::function<void(ModalCancelReason)> on_cancel_;
    std::function<void()> after_close_;
    std::function<void(bool)> after_open_change_;
};

void Modal(ModalProps props, ModalContent content, ModalSlots slots = {});
} // namespace ryn
