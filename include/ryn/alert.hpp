#pragma once
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/status_types.hpp>
#include <ryn/string.hpp>
#include <optional>

namespace ryn {
namespace detail {
class FeedbackComponentHost;
}

struct AlertMessageSlot final {};

struct AlertDescriptionSlot final {};

struct AlertIconSlot final {};

struct AlertActionSlot final {};

struct AlertCloseSlot final {};

using AlertMessage = SlotContent<AlertMessageSlot>;
using AlertDescription = SlotContent<AlertDescriptionSlot>;
using AlertIcon = SlotContent<AlertIconSlot>;
using AlertAction = SlotContent<AlertActionSlot>;
using AlertClose = SlotContent<AlertCloseSlot>;

struct AlertSlots final {
    std::optional<AlertMessage> message;
    std::optional<AlertDescription> description;
    std::optional<AlertIcon> icon;
    std::optional<AlertAction> action;
    std::optional<AlertClose> close;
};

struct AlertMarquee final {
    bool enabled{true};
    float speed{40}; // Logical pixels per second.
    float gap{24};
    bool pause_on_hover{true};
    friend bool operator==(AlertMarquee, AlertMarquee) = default;
};

class AlertProps final {
public:
    AlertProps& status(Prop<Status> value) {
        status_ = std::move(value);
        status_explicit_ = true;
        return *this;
    }

    AlertProps& message(Prop<String> value) {
        message_ = std::move(value);
        return *this;
    }

    template <std::size_t N> AlertProps& message(const char8_t (&value)[N]) {
        return message(String{value});
    }

    AlertProps& description(Prop<String> value) {
        description_ = std::move(value);
        return *this;
    }

    template <std::size_t N> AlertProps& description(const char8_t (&value)[N]) {
        return description(String{value});
    }

    AlertProps& showIcon(Prop<bool> value = true) {
        show_icon_ = std::move(value);
        icon_explicit_ = true;
        return *this;
    }

    AlertProps& closable(Prop<bool> value = true) {
        closable_ = std::move(value);
        return *this;
    }

    AlertProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    AlertProps& onClose(std::function<void()> value) {
        on_close_ = std::move(value);
        return *this;
    }

    AlertProps& banner(Prop<bool> value = true) {
        banner_ = std::move(value);
        return *this;
    }

    AlertProps& marquee(Prop<AlertMarquee> value) {
        marquee_ = std::move(value);
        return *this;
    }

    AlertProps& motion(Prop<bool> value = true) {
        motion_ = std::move(value);
        return *this;
    }

    AlertProps& afterClose(std::function<void()> value) {
        after_close_ = std::move(value);
        return *this;
    }

    AlertProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::FeedbackComponentHost;
    Prop<Status> status_{Status::Default};
    Prop<String> message_{String{}};
    Prop<String> description_{String{}};
    Prop<bool> show_icon_{false};
    Prop<bool> closable_{false};
    std::optional<Prop<bool>> visible_;
    Prop<bool> banner_{false};
    Prop<AlertMarquee> marquee_{AlertMarquee{false}};
    Prop<bool> motion_{false};
    bool status_explicit_{};
    bool icon_explicit_{};
    std::function<void()> on_close_;
    std::function<void()> after_close_;
    LayoutStyle layout_;
};

void Alert(AlertProps props, AlertSlots slots = {});
} // namespace ryn
