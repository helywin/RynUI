#pragma once

#include <ryn/input.hpp>
#include <initializer_list>
#include <vector>

namespace ryn {
namespace detail {
class AutoCompleteComponentHost;
}

struct AutoCompleteOptionLabelSlot final {};

using AutoCompleteOptionLabel = SlotContent<AutoCompleteOptionLabelSlot>;

// Copies preserve the identity of a retained rich label.
class AutoCompleteRichLabel final {
public:
    AutoCompleteRichLabel() = default;

    explicit AutoCompleteRichLabel(AutoCompleteOptionLabel content)
        : content_(std::make_shared<const AutoCompleteOptionLabel>(std::move(content))) {}

    [[nodiscard]] const AutoCompleteOptionLabel* content() const noexcept {
        return content_.get();
    }

    friend bool operator==(const AutoCompleteRichLabel&, const AutoCompleteRichLabel&) = default;

private:
    std::shared_ptr<const AutoCompleteOptionLabel> content_;
};

struct AutoCompleteOption final {
    String key;
    String value;
    String label;
    bool disabled{};
    AutoCompleteRichLabel rich_label;
    friend bool operator==(const AutoCompleteOption&, const AutoCompleteOption&) = default;
};

class AutoCompleteFilter final {
public:
    // The locked combobox source does not filter by default.
    AutoCompleteFilter() = default;

    explicit AutoCompleteFilter(bool value_substring) : substring_(value_substring) {}

    template <typename F>
        requires std::is_invocable_r_v<bool, F, StringView, const AutoCompleteOption&>
    AutoCompleteFilter(F predicate)
        : predicate_(std::make_shared<const std::function<bool(StringView, const AutoCompleteOption&)>>(
              std::move(predicate))) {}

    [[nodiscard]] bool accepts(StringView query, const AutoCompleteOption& option) const;
    friend bool operator==(const AutoCompleteFilter&, const AutoCompleteFilter&) = default;

private:
    bool substring_{};
    std::shared_ptr<const std::function<bool(StringView, const AutoCompleteOption&)>> predicate_;
};

using AutoCompleteRef = InputRef;

class AutoCompleteProps final {
public:
    AutoCompleteProps& options(Prop<std::vector<AutoCompleteOption>> value) {
        options_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& options(std::initializer_list<AutoCompleteOption> value) {
        return options(Prop<std::vector<AutoCompleteOption>>{std::vector<AutoCompleteOption>{value}});
    }

    AutoCompleteProps& value(Prop<String> value) {
        value_ = std::move(value);
        return *this;
    }

    template <std::size_t N> AutoCompleteProps& value(const char8_t (&value)[N]) {
        return this->value(String{value});
    }

    AutoCompleteProps& defaultValue(String value) {
        default_value_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& placeholder(Prop<String> value) {
        placeholder_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& filterOption(Prop<AutoCompleteFilter> value) {
        filter_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& defaultActiveFirstOption(Prop<bool> value) {
        active_first_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& backfill(Prop<bool> value = true) {
        backfill_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& allowClear(Prop<bool> value = true) {
        allow_clear_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& readOnly(Prop<bool> value = true) {
        read_only_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& variant(Prop<InputVariant> value) {
        variant_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& status(Prop<InputStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& open(Prop<bool> value) {
        open_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& defaultOpen(bool value = true) {
        default_open_ = value;
        return *this;
    }

    AutoCompleteProps& listHeight(Prop<float> value) {
        list_height_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& onChange(std::function<void(String)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& onSearch(std::function<void(String)> value) {
        on_search_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& onSelect(std::function<void(String, AutoCompleteOption)> value) {
        on_select_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& onClear(std::function<void()> value) {
        on_clear_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& onOpenChange(std::function<void(bool)> value) {
        on_open_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& onFocus(std::function<void()> value) {
        on_focus_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& onBlur(std::function<void()> value) {
        on_blur_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& ref(AutoCompleteRef value) {
        ref_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& autoFocus(bool value = true) {
        auto_focus_ = value;
        return *this;
    }

    AutoCompleteProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::AutoCompleteComponentHost;
    Prop<std::vector<AutoCompleteOption>> options_{std::vector<AutoCompleteOption>{}};
    std::optional<Prop<String>> value_;
    std::optional<String> default_value_;
    Prop<String> placeholder_{String{}};
    Prop<AutoCompleteFilter> filter_{AutoCompleteFilter{}};
    Prop<bool> active_first_{true};
    Prop<bool> backfill_{false};
    Prop<bool> allow_clear_{false};
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<InputVariant> variant_{InputVariant::Outlined};
    Prop<InputStatus> status_{InputStatus::Default};
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    Prop<float> list_height_{256};
    std::function<void(String)> on_change_;
    std::function<void(String)> on_search_;
    std::function<void(String, AutoCompleteOption)> on_select_;
    std::function<void()> on_clear_;
    std::function<void(bool)> on_open_;
    std::function<void()> on_focus_;
    std::function<void()> on_blur_;
    std::optional<AutoCompleteRef> ref_;
    bool auto_focus_{};
    LayoutStyle layout_;
};

struct AutoCompletePrefixSlot final {};

struct AutoCompleteSuffixSlot final {};

using AutoCompletePrefix = SlotContent<AutoCompletePrefixSlot>;
using AutoCompleteSuffix = SlotContent<AutoCompleteSuffixSlot>;

void AutoComplete(AutoCompleteProps props, std::optional<AutoCompletePrefix> prefix = {},
                  std::optional<AutoCompleteSuffix> suffix = {});
} // namespace ryn
