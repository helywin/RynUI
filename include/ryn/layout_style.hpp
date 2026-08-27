#pragma once

#include <ryn/prop.hpp>

#include <optional>
#include <utility>

namespace ryn {
namespace detail {

struct LayoutStyleAccess;

} // namespace detail

class LogicalLength final {
public:
    constexpr LogicalLength() noexcept = default;

    [[nodiscard]] constexpr bool is_auto() const noexcept {
        return automatic_;
    }

    [[nodiscard]] constexpr float value() const noexcept {
        return value_;
    }

    friend constexpr bool operator==(
        LogicalLength,
        LogicalLength) = default;

private:
    friend constexpr LogicalLength dp(float value) noexcept;

    constexpr LogicalLength(bool automatic, float value) noexcept
        : automatic_(automatic), value_(value) {}

    bool automatic_{true};
    float value_{0.0F};
};

[[nodiscard]] constexpr LogicalLength dp(float value) noexcept {
    return LogicalLength(false, value);
}

inline constexpr LogicalLength auto_length{};

enum class FlexAlignSelf {
    automatic,
    start,
    center,
    end,
    stretch,
};

class LayoutStyle final {
public:
    LayoutStyle& width(Prop<LogicalLength> value) {
        width_ = std::move(value);
        return *this;
    }

    LayoutStyle& height(Prop<LogicalLength> value) {
        height_ = std::move(value);
        return *this;
    }

    LayoutStyle& min_width(Prop<LogicalLength> value) {
        min_width_ = std::move(value);
        return *this;
    }

    LayoutStyle& max_width(Prop<LogicalLength> value) {
        max_width_ = std::move(value);
        return *this;
    }

    LayoutStyle& min_height(Prop<LogicalLength> value) {
        min_height_ = std::move(value);
        return *this;
    }

    LayoutStyle& max_height(Prop<LogicalLength> value) {
        max_height_ = std::move(value);
        return *this;
    }

    LayoutStyle& margin_left(Prop<LogicalLength> value) {
        margin_left_ = std::move(value);
        return *this;
    }

    LayoutStyle& margin_top(Prop<LogicalLength> value) {
        margin_top_ = std::move(value);
        return *this;
    }

    LayoutStyle& margin_right(Prop<LogicalLength> value) {
        margin_right_ = std::move(value);
        return *this;
    }

    LayoutStyle& margin_bottom(Prop<LogicalLength> value) {
        margin_bottom_ = std::move(value);
        return *this;
    }

    LayoutStyle& flex_grow(Prop<float> value) {
        flex_grow_ = std::move(value);
        return *this;
    }

    LayoutStyle& flex_shrink(Prop<float> value) {
        flex_shrink_ = std::move(value);
        return *this;
    }

    LayoutStyle& flex_basis(Prop<LogicalLength> value) {
        flex_basis_ = std::move(value);
        return *this;
    }

    LayoutStyle& align_self(Prop<FlexAlignSelf> value) {
        align_self_ = std::move(value);
        return *this;
    }

    LayoutStyle& order(Prop<int> value) {
        order_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::LayoutStyleAccess;

    std::optional<Prop<LogicalLength>> width_;
    std::optional<Prop<LogicalLength>> height_;
    std::optional<Prop<LogicalLength>> min_width_;
    std::optional<Prop<LogicalLength>> max_width_;
    std::optional<Prop<LogicalLength>> min_height_;
    std::optional<Prop<LogicalLength>> max_height_;
    std::optional<Prop<LogicalLength>> margin_left_;
    std::optional<Prop<LogicalLength>> margin_top_;
    std::optional<Prop<LogicalLength>> margin_right_;
    std::optional<Prop<LogicalLength>> margin_bottom_;
    std::optional<Prop<float>> flex_grow_;
    std::optional<Prop<float>> flex_shrink_;
    std::optional<Prop<LogicalLength>> flex_basis_;
    std::optional<Prop<FlexAlignSelf>> align_self_;
    std::optional<Prop<int>> order_;
};

} // namespace ryn
