#pragma once
#include <ryn/component.hpp>
#include <ryn/async_options.hpp>
#include <memory>
#include <cstdint>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <functional>
#include <optional>
#include <vector>

namespace ryn {
namespace detail {
class TreeComponentHost;
class TreeSelectComponentHost;
} // namespace detail

using TreeKeys = std::vector<String>;

struct TreeTitleSlot final {};

struct TreeIconSlot final {};

using TreeTitle = SlotContent<TreeTitleSlot>;
using TreeIcon = SlotContent<TreeIconSlot>;

// Stable keys retain row/editor identity across topology updates.
class TreeItem final {
public:
    TreeItem(String key, Prop<String> title, std::vector<TreeItem> children = {})
        : key_(std::move(key)), title_(std::move(title)), children_(std::move(children)) {}

    TreeItem(String key, String title, std::vector<TreeItem> children = {})
        : TreeItem(std::move(key), Prop<String>{std::move(title)}, std::move(children)) {}

    [[nodiscard]] const String& key() const noexcept {
        return key_;
    }

    TreeItem& richTitle(TreeTitle value) {
        identity_ = std::make_shared<const int>(0);
        rich_title_ = std::move(value);
        return *this;
    }

    TreeItem& icon(TreeIcon value) {
        identity_ = std::make_shared<const int>(0);
        icon_ = std::move(value);
        return *this;
    }

    TreeItem& disabled(Prop<bool> value = true) {
        identity_ = std::make_shared<const int>(0);
        disabled_ = std::move(value);
        return *this;
    }

    TreeItem& visible(Prop<bool> value) {
        identity_ = std::make_shared<const int>(0);
        visible_ = std::move(value);
        return *this;
    }

    TreeItem& selectable(Prop<bool> value = true) {
        identity_ = std::make_shared<const int>(0);
        selectable_ = std::move(value);
        return *this;
    }

    TreeItem& checkable(Prop<bool> value = true) {
        identity_ = std::make_shared<const int>(0);
        checkable_ = std::move(value);
        return *this;
    }

    TreeItem& disableCheckbox(Prop<bool> value = true) {
        identity_ = std::make_shared<const int>(0);
        check_disabled_ = std::move(value);
        return *this;
    }

    TreeItem& isLeaf(Prop<bool> value = true) {
        identity_ = std::make_shared<const int>(0);
        leaf_ = std::move(value);
        return *this;
    }

    // Increment to replace rich factories. Same key/revision retains the mounted draft.
    TreeItem& contentRevision(std::uint64_t value) {
        identity_ = std::make_shared<const int>(0);
        revision_ = value;
        return *this;
    }

    friend bool operator==(const TreeItem& a, const TreeItem& b) {
        return a.identity_ == b.identity_ && a.revision_ == b.revision_;
    }

private:
    friend class detail::TreeComponentHost;
    friend class detail::TreeSelectComponentHost;
    String key_;
    Prop<String> title_;
    std::vector<TreeItem> children_;
    std::optional<TreeTitle> rich_title_;
    std::optional<TreeIcon> icon_;
    Prop<bool> disabled_{false};
    Prop<bool> visible_{true};
    Prop<bool> selectable_{true};
    Prop<bool> checkable_{true};
    Prop<bool> check_disabled_{false};
    Prop<bool> leaf_{false};
    std::uint64_t revision_{};
    std::shared_ptr<const int> identity_{std::make_shared<const int>(0)};
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

struct TreeSwitcherSlot final {};

using TreeSwitcher = SlotContent<TreeSwitcherSlot>;

struct TreeSwitcherInfo final {
    String key;
    Prop<bool> expanded;
    Prop<bool> loading;
};

using TreeLoader = AsyncOptionsSource<TreeItem>;
enum class TreeDropPosition { Before, Inside, After };
enum class DirectoryExpandAction { Click, DoubleClick, None };

struct TreeDragInfo final {
    String key;
    String target_key;
    TreeKeys drag_keys;
    TreeDropPosition position{TreeDropPosition::Inside};
    bool allowed{};
};

struct TreeLoadInfo final {
    String key;
    TreeKeys loaded_keys;
    String error;
};

struct TreeScrollTo final {
    String key;
    float offset{};
};

namespace detail {
struct TreeRefAccess;
}

class TreeRef final {
public:
    TreeRef() : state_(std::make_shared<State>()) {}

    [[nodiscard]] bool scrollTo(TreeScrollTo value) const {
        return state_->scroll && state_->scroll(std::move(value));
    }

    [[nodiscard]] bool bound() const {
        return static_cast<bool>(state_->scroll);
    }

    // Cancels all outstanding node requests, including same-value Form resets.
    bool reset() const {
        if (!state_->reset) {
            return false;
        }
        state_->reset();
        return true;
    }

private:
    friend struct detail::TreeRefAccess;

    struct State {
        std::function<bool(TreeScrollTo)> scroll;
        std::function<void()> reset;
        std::uint64_t generation{};
    };

    std::shared_ptr<State> state_;
};

class TreeProps final {
public:
    TreeProps& items(Prop<std::vector<TreeItem>> value) {
        items_ = std::move(value);
        return *this;
    }

    TreeProps& multiple(Prop<bool> value = true) {
        multiple_ = std::move(value);
        return *this;
    }

    TreeProps& selectable(Prop<bool> value = true) {
        selectable_ = std::move(value);
        return *this;
    }

    TreeProps& checkStrictly(Prop<bool> value = true) {
        strict_ = std::move(value);
        return *this;
    }

    TreeProps& halfCheckedKeys(Prop<TreeKeys> value) {
        half_ = std::move(value);
        return *this;
    }

    TreeProps& autoExpandParent(Prop<bool> value = true) {
        auto_expand_ = std::move(value);
        return *this;
    }

    TreeProps& defaultExpandAll(bool value = true) {
        expand_all_ = value;
        return *this;
    }

    TreeProps& loadedKeys(Prop<TreeKeys> value) {
        loaded_ = std::move(value);
        return *this;
    }

    TreeProps& defaultLoadedKeys(TreeKeys value) {
        default_loaded_ = std::move(value);
        return *this;
    }

    TreeProps& loadData(TreeLoader value) {
        loader_ = std::move(value);
        return *this;
    }

    TreeProps& onLoad(std::function<void(TreeLoadInfo)> value) {
        on_load_ = std::move(value);
        return *this;
    }

    TreeProps& filterTreeNode(std::function<bool(const String&, const String&)> value) {
        filter_ = std::move(value);
        return *this;
    }

    TreeProps& searchValue(Prop<String> value) {
        search_ = std::move(value);
        return *this;
    }

    TreeProps& virtualized(Prop<bool> value = true) {
        virtual_ = std::move(value);
        return *this;
    }

    TreeProps& itemHeight(std::function<float(const String&)> value) {
        item_height_ = std::move(value);
        return *this;
    }

    TreeProps& rowHeight(Prop<float> value) {
        row_height_ = std::move(value);
        return *this;
    }

    TreeProps& switcherIcon(std::function<TreeSwitcher(TreeSwitcherInfo)> value) {
        switcher_ = std::move(value);
        return *this;
    }

    TreeProps& showLine(Prop<bool> value = true) {
        lines_ = std::move(value);
        return *this;
    }

    TreeProps& showIcon(Prop<bool> value = true) {
        icons_ = std::move(value);
        return *this;
    }

    TreeProps& blockNode(Prop<bool> value = true) {
        block_ = std::move(value);
        return *this;
    }

    TreeProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        return *this;
    }

    TreeProps& draggable(Prop<bool> value = true) {
        draggable_ = std::move(value);
        return *this;
    }

    TreeProps& allowDrag(std::function<bool(String)> value) {
        allow_drag_ = std::move(value);
        return *this;
    }

    TreeProps& allowDrop(std::function<bool(TreeDragInfo)> value) {
        allow_drop_ = std::move(value);
        return *this;
    }

    TreeProps& onDragStart(std::function<void(TreeDragInfo)> value) {
        drag_start_ = std::move(value);
        return *this;
    }

    TreeProps& onDragEnter(std::function<void(TreeDragInfo)> value) {
        drag_enter_ = std::move(value);
        return *this;
    }

    TreeProps& onDragOver(std::function<void(TreeDragInfo)> value) {
        drag_over_ = std::move(value);
        return *this;
    }

    TreeProps& onDragLeave(std::function<void(TreeDragInfo)> value) {
        drag_leave_ = std::move(value);
        return *this;
    }

    TreeProps& onDragEnd(std::function<void(TreeDragInfo)> value) {
        drag_end_ = std::move(value);
        return *this;
    }

    TreeProps& onDrop(std::function<void(TreeDragInfo)> value) {
        drop_ = std::move(value);
        return *this;
    }

    TreeProps& directory(std::optional<DirectoryExpandAction> value = {}) {
        directory_ = true;
        if (value) {
            expand_action_ = *value;
        }
        return *this;
    }

    TreeProps& ref(TreeRef value) {
        ref_ = std::move(value);
        return *this;
    }

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
    std::optional<Prop<std::vector<TreeItem>>> items_;
    Prop<bool> multiple_{false};
    Prop<bool> selectable_{true};
    Prop<bool> strict_{false};
    Prop<bool> auto_expand_{false};
    Prop<bool> virtual_{false};
    Prop<bool> lines_{false};
    Prop<bool> icons_{true};
    Prop<bool> block_{false};
    Prop<bool> rtl_{false};
    Prop<bool> draggable_{false};
    Prop<float> row_height_{24};
    std::function<float(const String&)> item_height_;
    std::function<TreeSwitcher(TreeSwitcherInfo)> switcher_;
    Prop<String> search_{String{}};
    std::optional<Prop<TreeKeys>> half_;
    std::optional<Prop<TreeKeys>> loaded_;
    TreeKeys default_loaded_;
    TreeLoader loader_;
    std::function<void(TreeLoadInfo)> on_load_;
    std::function<bool(const String&, const String&)> filter_;
    std::function<bool(String)> allow_drag_;
    std::function<bool(TreeDragInfo)> allow_drop_;
    std::function<void(TreeDragInfo)> drag_start_;
    std::function<void(TreeDragInfo)> drag_enter_;
    std::function<void(TreeDragInfo)> drag_over_;
    std::function<void(TreeDragInfo)> drag_leave_;
    std::function<void(TreeDragInfo)> drag_end_;
    std::function<void(TreeDragInfo)> drop_;
    bool directory_{};
    bool expand_all_{};
    DirectoryExpandAction expand_action_{DirectoryExpandAction::Click};
    std::optional<TreeRef> ref_;
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

inline void Tree(TreeProps props) {
    Tree(std::move(props), {});
}

inline void DirectoryTree(TreeProps props, std::vector<TreeItem> items) {
    props.directory();
    Tree(std::move(props), std::move(items));
}
} // namespace ryn
