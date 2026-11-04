#pragma once
#include <ryn/badge.hpp>
#include <ryn/button.hpp>
#include <ryn/icon.hpp>
#include <ryn/theme.hpp>
#include <ryn/tooltip.hpp>
#include <optional>

namespace ryn {
namespace detail {
class FloatButtonComponentHost;
}
enum class FloatButtonType { Default, Primary };
enum class FloatButtonShape { Circle, Square };
enum class FloatButtonPlacement { Top, Bottom, Left, Right };
enum class FloatButtonTrigger { None, Click, Hover };
enum class FloatButtonAnchor { BottomRight, BottomLeft, TopRight, TopLeft };

struct FloatButtonScrollState final {
    float offset{};
    float extent{};
    float viewport{};
    friend bool operator==(const FloatButtonScrollState&, const FloatButtonScrollState&) = default;
};

struct FloatButtonIconSlot final {};

struct FloatButtonContentSlot final {};

struct FloatButtonGroupContentSlot final {};

using FloatButtonIcon = SlotContent<FloatButtonIconSlot>;
using FloatButtonContent = SlotContent<FloatButtonContentSlot>;
using FloatButtonGroupContent = SlotContent<FloatButtonGroupContentSlot>;

struct FloatButtonSlots final {
    std::optional<FloatButtonIcon> icon;
    std::optional<FloatButtonContent> content;
};

class FloatButtonProps final {
public:
    FloatButtonProps& type(Prop<FloatButtonType> value) {
        type_ = std::move(value);
        return *this;
    }

    FloatButtonProps& shape(Prop<FloatButtonShape> value) {
        shape_ = std::move(value);
        return *this;
    }

    FloatButtonProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    FloatButtonProps& tooltip(Prop<String> value) {
        tooltip_ = std::move(value);
        tooltip_config_.reset();
        return *this;
    }

    FloatButtonProps& tooltip(TooltipProps value) {
        tooltip_config_ = std::move(value);
        return *this;
    }

    template <std::size_t N> FloatButtonProps& tooltip(const char8_t (&value)[N]) {
        return tooltip(String{value});
    }

    FloatButtonProps& floating(Prop<bool> value) {
        floating_ = std::move(value);
        return *this;
    }

    FloatButtonProps& anchor(Prop<FloatButtonAnchor> value) {
        anchor_ = std::move(value);
        return *this;
    }

    FloatButtonProps& draggable(Prop<bool> value) {
        draggable_ = std::move(value);
        return *this;
    }

    FloatButtonProps& badge(BadgeProps value) {
        badge_ = std::move(value);
        return *this;
    }

    FloatButtonProps& onClick(std::function<void()> value) {
        on_click_ = std::move(value);
        return *this;
    }

    FloatButtonProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::FloatButtonComponentHost;
    Prop<FloatButtonType> type_{FloatButtonType::Default};
    Prop<FloatButtonShape> shape_{FloatButtonShape::Circle};
    Prop<bool> disabled_{false};
    Prop<String> tooltip_{String{}};
    std::optional<TooltipProps> tooltip_config_;
    Prop<bool> floating_{true};
    Prop<FloatButtonAnchor> anchor_{FloatButtonAnchor::BottomRight};
    Prop<bool> draggable_{false};
    std::optional<BadgeProps> badge_;
    std::function<void()> on_click_;
    LayoutStyle layout_;
};

class FloatButtonGroupProps final {
public:
    FloatButtonGroupProps& shape(Prop<FloatButtonShape> value) {
        shape_ = std::move(value);
        return *this;
    }

    FloatButtonGroupProps& type(Prop<FloatButtonType> value) {
        type_ = std::move(value);
        return *this;
    }

    FloatButtonGroupProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    FloatButtonGroupProps& trigger(Prop<FloatButtonTrigger> value) {
        trigger_ = std::move(value);
        return *this;
    }

    FloatButtonGroupProps& placement(Prop<FloatButtonPlacement> value) {
        placement_ = std::move(value);
        return *this;
    }

    FloatButtonGroupProps& floating(Prop<bool> value) {
        floating_ = std::move(value);
        return *this;
    }

    FloatButtonGroupProps& anchor(Prop<FloatButtonAnchor> value) {
        anchor_ = std::move(value);
        return *this;
    }

    FloatButtonGroupProps& draggable(Prop<bool> value) {
        draggable_ = std::move(value);
        return *this;
    }

    FloatButtonGroupProps& open(Prop<bool> value) {
        open_ = std::move(value);
        return *this;
    }

    FloatButtonGroupProps& defaultOpen(bool value) {
        default_open_ = value;
        return *this;
    }

    FloatButtonGroupProps& onClick(std::function<void()> value) {
        on_click_ = std::move(value);
        return *this;
    }

    FloatButtonGroupProps& onOpenChange(std::function<void(bool)> value) {
        on_open_ = std::move(value);
        return *this;
    }

    FloatButtonGroupProps& icon(FloatButtonIcon value) {
        icon_ = std::move(value);
        return *this;
    }

    FloatButtonGroupProps& closeIcon(FloatButtonIcon value) {
        close_icon_ = std::move(value);
        return *this;
    }

    FloatButtonGroupProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::FloatButtonComponentHost;
    Prop<FloatButtonShape> shape_{FloatButtonShape::Circle};
    Prop<FloatButtonType> type_{FloatButtonType::Default};
    Prop<bool> disabled_{false};
    Prop<FloatButtonTrigger> trigger_{FloatButtonTrigger::None};
    Prop<FloatButtonPlacement> placement_{FloatButtonPlacement::Top};
    Prop<bool> floating_{true};
    Prop<FloatButtonAnchor> anchor_{FloatButtonAnchor::BottomRight};
    Prop<bool> draggable_{false};
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    std::optional<FloatButtonIcon> icon_;
    std::optional<FloatButtonIcon> close_icon_;
    std::function<void(bool)> on_open_;
    std::function<void()> on_click_;
    LayoutStyle layout_;
};

class FloatButtonBackTopProps final {
public:
    FloatButtonBackTopProps& scroll(Prop<FloatButtonScrollState> value) {
        scroll_ = std::move(value);
        return *this;
    }

    FloatButtonBackTopProps& visibilityHeight(Prop<float> value) {
        visibilityHeight_ = std::move(value);
        return *this;
    }

    FloatButtonBackTopProps& duration(Prop<Duration> value) {
        duration_ = std::move(value);
        return *this;
    }

    FloatButtonBackTopProps& showProgress(Prop<bool> value) {
        showProgress_ = std::move(value);
        return *this;
    }

    FloatButtonBackTopProps& button(FloatButtonProps value) {
        button_ = std::move(value);
        return *this;
    }

    FloatButtonBackTopProps& onScrollTo(std::function<void(float)> value) {
        on_scroll_ = std::move(value);
        return *this;
    }

private:
    friend class detail::FloatButtonComponentHost;
    Prop<FloatButtonScrollState> scroll_{FloatButtonScrollState{}};
    Prop<float> visibilityHeight_{400};
    Prop<Duration> duration_{Duration::milliseconds(450)};
    Prop<bool> showProgress_{false};
    FloatButtonProps button_;
    std::function<void(float)> on_scroll_;
};

void FloatButton(FloatButtonProps props = {}, FloatButtonSlots slots = {});
void FloatButtonGroup(FloatButtonGroupProps props, FloatButtonGroupContent content);
void FloatButtonBackTop(FloatButtonBackTopProps props, FloatButtonSlots slots = {});
} // namespace ryn
