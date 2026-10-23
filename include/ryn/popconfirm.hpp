#pragma once

#include <ryn/tooltip.hpp>

namespace ryn {
namespace detail {
class PopconfirmComponentHost;
}

using PopconfirmPlacement = TooltipPlacement;

struct PopconfirmTriggerSlot final {};

struct PopconfirmTitleSlot final {};

struct PopconfirmDescriptionSlot final {};

using PopconfirmTrigger = SlotContent<PopconfirmTriggerSlot>;
using PopconfirmTitle = SlotContent<PopconfirmTitleSlot>;
using PopconfirmDescription = SlotContent<PopconfirmDescriptionSlot>;

struct PopconfirmSlots final {
    std::optional<PopconfirmTitle> title;
    std::optional<PopconfirmDescription> description;
};

class PopconfirmProps final {
public:
    PopconfirmProps& title(Prop<String> value) {
        title_ = std::move(value);
        explicit_title_ = true;
        return *this;
    }

    template <std::size_t N> PopconfirmProps& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    PopconfirmProps& description(Prop<String> value) {
        description_ = std::move(value);
        explicit_description_ = true;
        return *this;
    }

    template <std::size_t N> PopconfirmProps& description(const char8_t (&value)[N]) {
        return description(String{value});
    }

    PopconfirmProps& open(Prop<bool> value = true) {
        open_ = std::move(value);
        return *this;
    }

    PopconfirmProps& defaultOpen(bool value = true) {
        default_open_ = value;
        return *this;
    }

    PopconfirmProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    PopconfirmProps& showCancel(Prop<bool> value = true) {
        show_cancel_ = std::move(value);
        return *this;
    }

    PopconfirmProps& confirmLoading(Prop<bool> value = true) {
        loading_ = std::move(value);
        return *this;
    }

    PopconfirmProps& okText(Prop<String> value) {
        ok_text_ = std::move(value);
        return *this;
    }

    template <std::size_t N> PopconfirmProps& okText(const char8_t (&value)[N]) {
        return okText(String{value});
    }

    PopconfirmProps& cancelText(Prop<String> value) {
        cancel_text_ = std::move(value);
        return *this;
    }

    template <std::size_t N> PopconfirmProps& cancelText(const char8_t (&value)[N]) {
        return cancelText(String{value});
    }

    PopconfirmProps& placement(Prop<PopconfirmPlacement> value) {
        placement_ = std::move(value);
        return *this;
    }

    PopconfirmProps& arrow(Prop<bool> value) {
        arrow_ = std::move(value);
        return *this;
    }

    PopconfirmProps& pointAtCenter(Prop<bool> value = true) {
        point_at_center_ = std::move(value);
        return *this;
    }

    PopconfirmProps& autoAdjustOverflow(Prop<bool> value) {
        adjust_ = std::move(value);
        return *this;
    }

    PopconfirmProps& onOpenChange(std::function<void(bool)> value) {
        on_open_ = std::move(value);
        return *this;
    }

    PopconfirmProps& onConfirm(std::function<void()> value) {
        on_confirm_ = std::move(value);
        return *this;
    }

    PopconfirmProps& onCancel(std::function<void()> value) {
        on_cancel_ = std::move(value);
        return *this;
    }

    PopconfirmProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::PopconfirmComponentHost;
    Prop<String> title_{String{}};
    Prop<String> description_{String{}};
    bool explicit_title_{};
    bool explicit_description_{};
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    Prop<bool> disabled_{false};
    Prop<bool> show_cancel_{true};
    Prop<bool> loading_{false};
    Prop<String> ok_text_{String{u8"OK"}};
    Prop<String> cancel_text_{String{u8"Cancel"}};
    Prop<PopconfirmPlacement> placement_{PopconfirmPlacement::Top};
    Prop<bool> arrow_{true};
    Prop<bool> point_at_center_{false};
    Prop<bool> adjust_{true};
    std::function<void(bool)> on_open_;
    std::function<void()> on_confirm_;
    std::function<void()> on_cancel_;
    LayoutStyle layout_;
};

void Popconfirm(PopconfirmProps props, PopconfirmTrigger trigger, PopconfirmSlots slots = {});
} // namespace ryn
