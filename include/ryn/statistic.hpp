#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <optional>
#include <variant>

namespace ryn {
namespace detail {
class StatisticComponentHost;
}

class StatisticValue final {
public:
    using Storage = std::variant<std::int64_t, std::uint64_t, double, String>;

    StatisticValue() = default;

    template <std::integral T>
        requires(!std::same_as<T, bool>)
    StatisticValue(T value) {
        if constexpr (std::is_signed_v<T>) {
            value_ = static_cast<std::int64_t>(value);
        } else {
            value_ = static_cast<std::uint64_t>(value);
        }
    }

    StatisticValue(double value) : value_(value) {}

    StatisticValue(String value) : value_(std::move(value)) {}

    template <std::size_t N> StatisticValue(const char8_t (&value)[N]) : value_(String{value}) {}

    [[nodiscard]] const Storage& storage() const noexcept {
        return value_;
    }

    friend bool operator==(const StatisticValue& left, const StatisticValue& right) {
        if (left.value_ != right.value_) {
            return false;
        }
        const auto* number = std::get_if<double>(&left.value_);
        return !number || *number != 0 || std::signbit(*number) == std::signbit(std::get<double>(right.value_));
    }

private:
    Storage value_{std::int64_t{0}};
};

using StatisticFormatter = std::function<String(const StatisticValue&)>;

struct StatisticTitleSlot final {};

struct StatisticValueSlot final {};

struct StatisticPrefixSlot final {};

struct StatisticSuffixSlot final {};

using StatisticTitle = SlotContent<StatisticTitleSlot>;
using StatisticValueContent = SlotContent<StatisticValueSlot>;
using StatisticPrefix = SlotContent<StatisticPrefixSlot>;
using StatisticSuffix = SlotContent<StatisticSuffixSlot>;

class StatisticProps final {
public:
    StatisticProps& value(Prop<StatisticValue> value) {
        value_ = std::move(value);
        return *this;
    }

    StatisticProps& value(StatisticValue value) {
        return this->value(Prop<StatisticValue>{std::move(value)});
    }

    template <typename T, typename Equal>
        requires std::constructible_from<StatisticValue, T>
    StatisticProps& value(Signal<T, Equal> value) {
        return this->value(Prop<StatisticValue>{bind([value] { return StatisticValue{value.get()}; })});
    }

    template <typename T>
        requires std::constructible_from<StatisticValue, T>
    StatisticProps& value(Binding<T> value) {
        return this->value(Prop<StatisticValue>{bind([value] { return StatisticValue{value.get()}; })});
    }

    StatisticProps& precision(Prop<std::optional<int>> value) {
        precision_ = std::move(value);
        return *this;
    }

    StatisticProps& precision(int value) {
        return precision(std::optional{value});
    }

    StatisticProps& decimalSeparator(Prop<String> value) {
        decimal_separator_ = std::move(value);
        return *this;
    }

    template <std::size_t N> StatisticProps& decimalSeparator(const char8_t (&value)[N]) {
        return decimalSeparator(String{value});
    }

    StatisticProps& groupSeparator(Prop<String> value) {
        group_separator_ = std::move(value);
        return *this;
    }

    template <std::size_t N> StatisticProps& groupSeparator(const char8_t (&value)[N]) {
        return groupSeparator(String{value});
    }

    StatisticProps& formatter(Prop<StatisticFormatter> value) {
        formatter_ = std::move(value);
        return *this;
    }

    StatisticProps& formatter(StatisticFormatter value) {
        return formatter(Prop<StatisticFormatter>{std::move(value)});
    }

    StatisticProps& title(Prop<String> value) {
        title_ = std::move(value);
        return *this;
    }

    template <std::size_t N> StatisticProps& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    StatisticProps& prefix(Prop<String> value) {
        prefix_ = std::move(value);
        return *this;
    }

    template <std::size_t N> StatisticProps& prefix(const char8_t (&value)[N]) {
        return prefix(String{value});
    }

    StatisticProps& suffix(Prop<String> value) {
        suffix_ = std::move(value);
        return *this;
    }

    template <std::size_t N> StatisticProps& suffix(const char8_t (&value)[N]) {
        return suffix(String{value});
    }

    StatisticProps& richTitle(StatisticTitle value) {
        rich_title_ = std::move(value);
        return *this;
    }

    StatisticProps& richValue(StatisticValueContent value) {
        rich_value_ = std::move(value);
        return *this;
    }

    StatisticProps& richPrefix(StatisticPrefix value) {
        rich_prefix_ = std::move(value);
        return *this;
    }

    StatisticProps& richSuffix(StatisticSuffix value) {
        rich_suffix_ = std::move(value);
        return *this;
    }

    StatisticProps& loading(Prop<bool> value) {
        loading_ = std::move(value);
        return *this;
    }

    StatisticProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    StatisticProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::StatisticComponentHost;
    Prop<StatisticValue> value_{StatisticValue{}};
    Prop<std::optional<int>> precision_{std::nullopt};
    Prop<String> decimal_separator_{String{u8"."}};
    Prop<String> group_separator_{String{u8","}};
    Prop<StatisticFormatter> formatter_{StatisticFormatter{}};
    Prop<String> title_{String{}};
    Prop<String> prefix_{String{}};
    Prop<String> suffix_{String{}};
    std::optional<StatisticTitle> rich_title_;
    std::optional<StatisticValueContent> rich_value_;
    std::optional<StatisticPrefix> rich_prefix_;
    std::optional<StatisticSuffix> rich_suffix_;
    Prop<bool> loading_{false};
    Prop<bool> visible_{true};
    LayoutStyle layout_;
};

void Statistic(StatisticProps props = {});
} // namespace ryn
