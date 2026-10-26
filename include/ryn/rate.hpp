#pragma once

#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/icon.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <ryn/tooltip.hpp>
#include <functional>
#include <optional>
#include <variant>
#include <vector>

namespace ryn {
namespace detail {
class RateComponentHost;
}

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
    std::vector<RateTooltip> tooltips_;
    std::function<void(float)> on_change_;
    std::function<void(std::optional<float>)> on_hover_;
    LayoutStyle layout_;
};

void Rate(RateProps props = {});
} // namespace ryn
