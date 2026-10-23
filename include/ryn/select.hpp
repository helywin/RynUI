#pragma once

#include <ryn/input.hpp>

#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <variant>
#include <vector>

namespace ryn {
namespace detail {
struct SelectPropsAccess;
class SelectComponentHost;
} // namespace detail

class SelectValue final {
public:
    using Storage = std::variant<String, std::int64_t>;

    SelectValue() = default;

    SelectValue(String value) : storage_(std::move(value)) {}

    template <std::size_t N> SelectValue(const char8_t (&value)[N]) : storage_(String{value}) {}

    template <std::signed_integral Integer> SelectValue(Integer value) : storage_(std::int64_t{value}) {}

    [[nodiscard]] const Storage& storage() const noexcept {
        return storage_;
    }

    friend bool operator==(const SelectValue&, const SelectValue&) = default;

private:
    Storage storage_;
};

using SelectSelection = std::optional<SelectValue>;
using SelectRef = InputRef;

struct SelectOption final {
    SelectValue value;
    String label;
    bool disabled{};
    friend bool operator==(const SelectOption&, const SelectOption&) = default;
};

class SelectProps final {
public:
    SelectProps& options(Prop<std::vector<SelectOption>> value) {
        options_ = std::move(value);
        return *this;
    }

    SelectProps& options(std::initializer_list<SelectOption> value) {
        return options(Prop<std::vector<SelectOption>>{std::vector<SelectOption>{value}});
    }

    SelectProps& value(Prop<SelectSelection> value) {
        value_ = std::move(value);
        return *this;
    }

    SelectProps& value(SelectSelection value) {
        return this->value(Prop<SelectSelection>{std::move(value)});
    }

    SelectProps& defaultValue(SelectValue value) {
        default_value_ = std::move(value);
        return *this;
    }

    SelectProps& placeholder(Prop<String> value) {
        placeholder_ = std::move(value);
        return *this;
    }

    template <std::size_t N> SelectProps& placeholder(const char8_t (&value)[N]) {
        return placeholder(String{value});
    }

    SelectProps& showSearch(Prop<bool> value = true) {
        show_search_ = std::move(value);
        return *this;
    }

    SelectProps& allowClear(Prop<bool> value = true) {
        allow_clear_ = std::move(value);
        return *this;
    }

    SelectProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    SelectProps& loading(Prop<bool> value = true) {
        loading_ = std::move(value);
        return *this;
    }

    SelectProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    SelectProps& variant(Prop<InputVariant> value) {
        variant_ = std::move(value);
        return *this;
    }

    SelectProps& status(Prop<InputStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    SelectProps& open(Prop<bool> value) {
        open_ = std::move(value);
        return *this;
    }

    SelectProps& defaultOpen(bool value) {
        default_open_ = value;
        return *this;
    }

    SelectProps& notFoundContent(Prop<String> value) {
        not_found_content_ = std::move(value);
        return *this;
    }

    template <std::size_t N> SelectProps& notFoundContent(const char8_t (&value)[N]) {
        return notFoundContent(String{value});
    }

    SelectProps& listHeight(Prop<float> value) {
        list_height_ = std::move(value);
        return *this;
    }

    SelectProps& onChange(std::function<void(SelectSelection)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    SelectProps& onSearch(std::function<void(String)> value) {
        on_search_ = std::move(value);
        return *this;
    }

    SelectProps& onOpenChange(std::function<void(bool)> value) {
        on_open_change_ = std::move(value);
        return *this;
    }

    SelectProps& onFocus(std::function<void()> value) {
        on_focus_ = std::move(value);
        return *this;
    }

    SelectProps& onBlur(std::function<void()> value) {
        on_blur_ = std::move(value);
        return *this;
    }

    SelectProps& ref(SelectRef value) {
        ref_ = std::move(value);
        return *this;
    }

    SelectProps& autoFocus(bool value = true) {
        auto_focus_ = value;
        return *this;
    }

    SelectProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::SelectPropsAccess;
    friend class detail::SelectComponentHost;
    Prop<std::vector<SelectOption>> options_{std::vector<SelectOption>{}};
    std::optional<Prop<SelectSelection>> value_;
    SelectSelection default_value_;
    Prop<String> placeholder_{String{}};
    Prop<bool> show_search_{false};
    Prop<bool> allow_clear_{false};
    Prop<bool> disabled_{false};
    Prop<bool> loading_{false};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<InputVariant> variant_{InputVariant::Outlined};
    Prop<InputStatus> status_{InputStatus::Default};
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    Prop<String> not_found_content_{String{u8"No data"}};
    Prop<float> list_height_{256};
    std::function<void(SelectSelection)> on_change_;
    std::function<void(String)> on_search_;
    std::function<void(bool)> on_open_change_;
    std::function<void()> on_focus_;
    std::function<void()> on_blur_;
    std::optional<SelectRef> ref_;
    bool auto_focus_{};
    LayoutStyle layout_;
};

struct SelectPrefixSlot final {};

struct SelectSuffixSlot final {};

using SelectPrefix = SlotContent<SelectPrefixSlot>;
using SelectSuffix = SlotContent<SelectSuffixSlot>;

void Select(SelectProps props, std::optional<SelectPrefix> prefix = {}, std::optional<SelectSuffix> suffix = {});

} // namespace ryn
