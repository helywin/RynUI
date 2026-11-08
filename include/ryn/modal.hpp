#pragma once
#include <functional>
#include <optional>
#include <ryn/component.hpp>
#include <ryn/component_locales.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>
#include <ryn/confirm_action.hpp>
#include <ryn/breakpoints.hpp>

namespace ryn {
namespace detail {
class ModalComponentHost;
class AppPanelService;
} // namespace detail

enum class ModalFooterMode { Default, None };

enum class ModalCancelReason { CancelButton, CloseButton, Mask, Escape };

struct ModalMask final {
    bool enabled{true};
    bool closable{true};
    bool blur{false};
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
    ModalProps& locale(Prop<CommonLocale> value) {
        locale_ = std::move(value);
        return *this;
    }

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
        responsive_width_.reset();
        return *this;
    }

    ModalProps& width(Prop<Responsive<float>> value) {
        responsive_width_ = std::move(value);
        return *this;
    }

    ModalProps& lazy(Prop<bool> value = true) {
        lazy_ = std::move(value);
        return *this;
    }

    ModalProps& destroyOnHidden(Prop<bool> value = true) {
        destroy_hidden_ = std::move(value);
        return *this;
    }

    ModalProps& animated(Prop<bool> value = true) {
        animated_ = std::move(value);
        return *this;
    }

    ModalProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        rtl_set_ = true;
        return *this;
    }

    ModalProps& draggable(Prop<bool> value = true) {
        draggable_ = std::move(value);
        return *this;
    }

    ModalProps& resizable(Prop<bool> value = true) {
        resizable_ = std::move(value);
        return *this;
    }

    ModalProps& onDrag(std::function<void(float, float)> value) {
        on_drag_ = std::move(value);
        return *this;
    }

    ModalProps& onResize(std::function<void(float, float)> value) {
        on_resize_ = std::move(value);
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

    ModalProps& focusTrap(Prop<bool> value = true) {
        focus_trap_ = std::move(value);
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

    ModalProps& showCancel(Prop<bool> value = true) {
        show_cancel_ = std::move(value);
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

    ModalProps& onOkAsync(ConfirmAction value) {
        action_ = std::move(value);
        return *this;
    }

    ModalProps& onActionError(std::function<void(ConfirmActionError)> value) {
        action_error_ = std::move(value);
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
    friend class detail::AppPanelService;
    Prop<bool> open_{false};
    Prop<String> title_{String{}};
    Prop<float> width_{520};
    std::optional<Prop<Responsive<float>>> responsive_width_;
    Prop<bool> lazy_{false};
    Prop<bool> destroy_hidden_{false};
    Prop<bool> animated_{false};
    Prop<bool> rtl_{false};
    bool rtl_set_{};
    Prop<bool> draggable_{false};
    Prop<bool> resizable_{false};
    std::function<void(float, float)> on_drag_;
    std::function<void(float, float)> on_resize_;
    Prop<bool> centered_{false};
    Prop<bool> closable_{true};
    Prop<bool> keyboard_{true};
    Prop<bool> focus_trap_{true};
    Prop<ModalMask> mask_{ModalMask{}};
    Prop<bool> confirm_loading_{false};
    Prop<bool> show_cancel_{true};
    Prop<ModalFooterMode> footer_{ModalFooterMode::Default};
    std::optional<Prop<String>> ok_text_;
    std::optional<Prop<String>> cancel_text_;
    std::optional<Prop<CommonLocale>> locale_;
    Prop<bool> restore_focus_{true};
    std::function<void()> on_ok_;
    ConfirmAction action_;
    std::function<bool()> action_live_;
    std::function<void(ConfirmActionError)> action_error_;
    std::function<void(ModalCancelReason)> on_cancel_;
    std::function<void()> after_close_;
    std::function<void(bool)> after_open_change_;
};

void Modal(ModalProps props, ModalContent content, ModalSlots slots = {});
} // namespace ryn
