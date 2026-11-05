#pragma once

#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/icon.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <ryn/tooltip.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace ryn {
namespace detail {
class RateComponentHost;
struct RateRefState;
} // namespace detail

enum class RateDirection { LeftToRight, RightToLeft };

enum class RateKey { Other, Tab, Enter, Space, Left, Right, Home, End, Escape, Up, Down };

struct RateKeyEvent final {
    RateKey key{RateKey::Other};
    bool repeat{};
    bool shift{};
    bool control{};
    bool alt{};
    bool meta{};
};

class RateRef final {
public:
    RateRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] bool focus() const;
    [[nodiscard]] bool blur() const;

private:
    friend class detail::RateComponentHost;
    std::shared_ptr<detail::RateRefState> state_;
};

// Pure paint content. Unlike a Component slot, it can safely be painted in two clipped layers.
class RateCharacter final {
public:
    RateCharacter() = default;

    RateCharacter(IconName value) : value_(IconSource{value}) {}

    RateCharacter(IconSource value) : value_(std::move(value)) {}

    RateCharacter(String value) : value_(std::move(value)) {}

    template <std::size_t N> RateCharacter(const char8_t (&value)[N]) : value_(String{value}) {}

    [[nodiscard]] const auto& value() const noexcept {
        return value_;
    }

    friend bool operator==(const RateCharacter&, const RateCharacter&) = default;

private:
    std::variant<IconSource, String> value_{IconSource{IconName::StarFilled}};
};

class RateTooltip final {
public:
    RateTooltip(Prop<String> title) : title_(std::move(title)) {}

    RateTooltip(String title) : title_(std::move(title)) {}

    template <std::size_t N> RateTooltip(const char8_t (&title)[N]) : title_(String{title}) {}

    explicit RateTooltip(TooltipTitle content) : content_(std::move(content)) {}

private:
    friend class detail::RateComponentHost;
    Prop<String> title_{String{}};
    std::optional<TooltipTitle> content_;
};

class RateProps final {
public:
    RateProps& count(Prop<int> value) {
        count_ = std::move(value);
        return *this;
    }

    RateProps& value(Prop<float> value) {
        value_ = std::move(value);
        return *this;
    }

    RateProps& defaultValue(float value) {
        default_value_ = value;
        return *this;
    }

    RateProps& allowHalf(Prop<bool> value = true) {
        allow_half_ = std::move(value);
        return *this;
    }

    RateProps& allowClear(Prop<bool> value = true) {
        allow_clear_ = std::move(value);
        return *this;
    }

    RateProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    RateProps& readOnly(Prop<bool> value = true) {
        read_only_ = std::move(value);
        return *this;
    }

    RateProps& keyboard(Prop<bool> value = true) {
        keyboard_ = std::move(value);
        return *this;
    }

    RateProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    RateProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    RateProps& character(Prop<RateCharacter> value) {
        character_ = std::move(value);
        return *this;
    }

    RateProps& character(RateCharacter value) {
        return character(Prop<RateCharacter>{std::move(value)});
    }

    RateProps& character(std::function<RateCharacter(std::size_t)> value) {
        character_for_ = std::move(value);
        return *this;
    }

    RateProps& characterFor(std::function<Prop<RateCharacter>(std::size_t)> value) {
        reactive_character_for_ = std::move(value);
        return *this;
    }

    RateProps& direction(Prop<RateDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    RateProps& ref(RateRef value) {
        ref_ = std::move(value);
        return *this;
    }

    RateProps& autoFocus(bool value = true) {
        auto_focus_ = value;
        return *this;
    }

    RateProps& onFocus(std::function<void()> value) {
        on_focus_ = std::move(value);
        return *this;
    }

    RateProps& onBlur(std::function<void()> value) {
        on_blur_ = std::move(value);
        return *this;
    }

    RateProps& onKeyDown(std::function<void(RateKeyEvent)> value) {
        on_key_ = std::move(value);
        return *this;
    }

    RateProps& tooltips(std::vector<RateTooltip> value) {
        tooltips_ = std::move(value);
        return *this;
    }

    RateProps& onChange(std::function<void(float)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    RateProps& onHoverChange(std::function<void(std::optional<float>)> value) {
        on_hover_ = std::move(value);
        return *this;
    }

    RateProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::RateComponentHost;
    Prop<int> count_{5};
    std::optional<Prop<float>> value_;
    std::optional<float> default_value_;
    Prop<bool> allow_half_{false};
    Prop<bool> allow_clear_{true};
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    Prop<bool> keyboard_{true};
    Prop<bool> visible_{true};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<RateCharacter> character_{RateCharacter{}};
    std::function<RateCharacter(std::size_t)> character_for_;
    std::function<Prop<RateCharacter>(std::size_t)> reactive_character_for_;
    Prop<RateDirection> direction_{RateDirection::LeftToRight};
    std::optional<RateRef> ref_;
    bool auto_focus_{};
    std::function<void()> on_focus_;
    std::function<void()> on_blur_;
    std::function<void(RateKeyEvent)> on_key_;
    std::vector<RateTooltip> tooltips_;
    std::function<void(float)> on_change_;
    std::function<void(std::optional<float>)> on_hover_;
    LayoutStyle layout_;
};

void Rate(RateProps props = {});
} // namespace ryn
