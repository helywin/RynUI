#pragma once
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <functional>
#include <optional>
#include <vector>

namespace ryn {
namespace detail {
class TreeComponentHost;
}

using TreeKeys = std::vector<String>;

struct TreeTitleSlot final {};

struct TreeIconSlot final {};

using TreeTitle = SlotContent<TreeTitleSlot>;
using TreeIcon = SlotContent<TreeIconSlot>;

// Keys and hierarchy are fixed at mount. Presentation and controlled state remain reactive.
class TreeItem final {
public:
    TreeItem(String key, Prop<String> title, std::vector<TreeItem> children = {})
        : key_(std::move(key)), title_(std::move(title)), children_(std::move(children)) {}

    TreeItem(String key, String title, std::vector<TreeItem> children = {})
        : TreeItem(std::move(key), Prop<String>{std::move(title)}, std::move(children)) {}

    TreeItem& richTitle(TreeTitle value) {
        rich_title_ = std::move(value);
        return *this;
    }

    TreeItem& icon(TreeIcon value) {
        icon_ = std::move(value);
        return *this;
    }

    TreeItem& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    TreeItem& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TreeComponentHost;
    String key_;
    Prop<String> title_;
    std::vector<TreeItem> children_;
    std::optional<TreeTitle> rich_title_;
    std::optional<TreeIcon> icon_;
    Prop<bool> disabled_{false};
    Prop<bool> visible_{true};
};

struct TreeExpandInfo final {
    String key;
    TreeKeys expanded_keys;
    bool expanded{};
};

struct TreeSelectInfo final {
    String key;
    TreeKeys selected_keys;
    bool selected{};
};

struct TreeCheckInfo final {
    String key;
    TreeKeys checked_keys;
    TreeKeys half_checked_keys;
    bool checked{};
};

class TreeProps final {
public:
    TreeProps& expandedKeys(Prop<TreeKeys> value) {
        expanded_ = std::move(value);
        return *this;
    }

    TreeProps& defaultExpandedKeys(TreeKeys value) {
        default_expanded_ = std::move(value);
        return *this;
    }

    TreeProps& selectedKeys(Prop<TreeKeys> value) {
        selected_ = std::move(value);
        return *this;
    }

    TreeProps& defaultSelectedKeys(TreeKeys value) {
        default_selected_ = std::move(value);
        return *this;
    }

    TreeProps& checkedKeys(Prop<TreeKeys> value) {
        checked_ = std::move(value);
        return *this;
    }

    TreeProps& defaultCheckedKeys(TreeKeys value) {
        default_checked_ = std::move(value);
        return *this;
    }

    TreeProps& checkable(Prop<bool> value = true) {
        checkable_ = std::move(value);
        return *this;
    }

    TreeProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    TreeProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    TreeProps& onExpand(std::function<void(TreeExpandInfo)> value) {
        on_expand_ = std::move(value);
        return *this;
    }

    TreeProps& onSelect(std::function<void(TreeSelectInfo)> value) {
        on_select_ = std::move(value);
        return *this;
    }

    TreeProps& onCheck(std::function<void(TreeCheckInfo)> value) {
        on_check_ = std::move(value);
        return *this;
    }

    TreeProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TreeComponentHost;
    std::optional<Prop<TreeKeys>> expanded_;
    std::optional<TreeKeys> default_expanded_;
    std::optional<Prop<TreeKeys>> selected_;
    std::optional<TreeKeys> default_selected_;
    std::optional<Prop<TreeKeys>> checked_;
    std::optional<TreeKeys> default_checked_;
    Prop<bool> checkable_{false};
    Prop<bool> disabled_{false};
    Prop<bool> visible_{true};
    std::function<void(TreeExpandInfo)> on_expand_;
    std::function<void(TreeSelectInfo)> on_select_;
    std::function<void(TreeCheckInfo)> on_check_;
    LayoutStyle layout_;
};

void Tree(TreeProps props, std::vector<TreeItem> items);
} // namespace ryn
