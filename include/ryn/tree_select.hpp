#pragma once

#include <ryn/input.hpp>
#include <ryn/tree.hpp>

namespace ryn {
namespace detail {
class TreeSelectComponentHost;
}

using TreeSelectRef = InputRef;

// Keys and hierarchy are fixed at mount; an empty selection represents no value.
class TreeSelectItem final {
public:
    TreeSelectItem(String key, Prop<String> title, std::vector<TreeSelectItem> children = {})
        : key_(std::move(key)), title_(std::move(title)), children_(std::move(children)) {}

    TreeSelectItem(String key, String title, std::vector<TreeSelectItem> children = {})
        : TreeSelectItem(std::move(key), Prop<String>{std::move(title)}, std::move(children)) {}

    TreeSelectItem& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    TreeSelectItem& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TreeSelectComponentHost;
    String key_;
    Prop<String> title_;
    std::vector<TreeSelectItem> children_;
    Prop<bool> disabled_{false};
    Prop<bool> visible_{true};
};

class TreeSelectProps final {
public:
    TreeSelectProps& value(Prop<String> value) {
        value_ = std::move(value);
        return *this;
    }

    template <std::size_t N> TreeSelectProps& value(const char8_t (&value)[N]) {
        return this->value(String{value});
    }

    TreeSelectProps& defaultValue(String value) {
        default_value_ = std::move(value);
        return *this;
    }

    TreeSelectProps& open(Prop<bool> value) {
        open_ = std::move(value);
        return *this;
    }

    TreeSelectProps& defaultOpen(bool value = true) {
        default_open_ = value;
        return *this;
    }

    TreeSelectProps& treeExpandedKeys(Prop<TreeKeys> value) {
        expanded_ = std::move(value);
        return *this;
    }

    TreeSelectProps& treeDefaultExpandedKeys(TreeKeys value) {
        default_expanded_ = std::move(value);
        return *this;
    }

    TreeSelectProps& placeholder(Prop<String> value) {
        placeholder_ = std::move(value);
        return *this;
    }

    template <std::size_t N> TreeSelectProps& placeholder(const char8_t (&value)[N]) {
        return placeholder(String{value});
    }

    TreeSelectProps& notFoundContent(Prop<String> value) {
        not_found_content_ = std::move(value);
        return *this;
    }

    template <std::size_t N> TreeSelectProps& notFoundContent(const char8_t (&value)[N]) {
        return notFoundContent(String{value});
    }

    TreeSelectProps& allowClear(Prop<bool> value = true) {
        allow_clear_ = std::move(value);
        return *this;
    }

    TreeSelectProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    TreeSelectProps& readOnly(Prop<bool> value = true) {
        read_only_ = std::move(value);
        return *this;
    }

    TreeSelectProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    TreeSelectProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    TreeSelectProps& variant(Prop<InputVariant> value) {
        variant_ = std::move(value);
        return *this;
    }

    TreeSelectProps& status(Prop<InputStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    TreeSelectProps& listHeight(Prop<float> value) {
        list_height_ = std::move(value);
        return *this;
    }

    TreeSelectProps& ref(TreeSelectRef value) {
        ref_ = std::move(value);
        return *this;
    }

    TreeSelectProps& autoFocus(bool value = true) {
        auto_focus_ = value;
        return *this;
    }

    TreeSelectProps& onChange(std::function<void(String)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    TreeSelectProps& onOpenChange(std::function<void(bool)> value) {
        on_open_ = std::move(value);
        return *this;
    }

    TreeSelectProps& onTreeExpand(std::function<void(TreeExpandInfo)> value) {
        on_expand_ = std::move(value);
        return *this;
    }

    TreeSelectProps& onFocus(std::function<void()> value) {
        on_focus_ = std::move(value);
        return *this;
    }

    TreeSelectProps& onBlur(std::function<void()> value) {
        on_blur_ = std::move(value);
        return *this;
    }

    TreeSelectProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TreeSelectComponentHost;
    std::optional<Prop<String>> value_;
    std::optional<String> default_value_;
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    std::optional<Prop<TreeKeys>> expanded_;
    std::optional<TreeKeys> default_expanded_;
    Prop<String> placeholder_{String{}};
    Prop<String> not_found_content_{String{u8"No data"}};
    Prop<bool> allow_clear_{false};
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    Prop<bool> visible_{true};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<InputVariant> variant_{InputVariant::Outlined};
    Prop<InputStatus> status_{InputStatus::Default};
    Prop<float> list_height_{256};
    std::optional<TreeSelectRef> ref_;
    bool auto_focus_{};
    std::function<void(String)> on_change_;
    std::function<void(bool)> on_open_;
    std::function<void(TreeExpandInfo)> on_expand_;
    std::function<void()> on_focus_;
    std::function<void()> on_blur_;
    LayoutStyle layout_;
};

void TreeSelect(TreeSelectProps props, std::vector<TreeSelectItem> items);
} // namespace ryn
