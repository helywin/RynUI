#pragma once

#include <ryn/input.hpp>
#include <ryn/popover.hpp>
#include <variant>
#include <concepts>
#include <type_traits>
#include <ryn/tree.hpp>

namespace ryn {
namespace detail {
class TreeSelectComponentHost;
}

using TreeSelectRef = InputRef;

// Stable keys retain nodes across reactive data updates; empty String remains the compatible single value.
class TreeSelectItem final {
public:
    TreeSelectItem(String key, Prop<String> title, std::vector<TreeSelectItem> children = {})
        : key_(std::move(key)), title_(std::move(title)), children_(std::move(children)) {}

    TreeSelectItem(String key, String title, std::vector<TreeSelectItem> children = {})
        : TreeSelectItem(std::move(key), Prop<String>{std::move(title)}, std::move(children)) {}

    TreeSelectItem& disabled(Prop<bool> value = true) {
        identity_ = std::make_shared<const int>(0);
        disabled_ = std::move(value);
        return *this;
    }

    TreeSelectItem& visible(Prop<bool> value) {
        identity_ = std::make_shared<const int>(0);
        visible_ = std::move(value);
        return *this;
    }

    TreeSelectItem& richTitle(TreeTitle value) {
        identity_ = std::make_shared<const int>(0);
        rich_title_ = std::move(value);
        return *this;
    }

    TreeSelectItem& icon(TreeIcon value) {
        identity_ = std::make_shared<const int>(0);
        icon_ = std::move(value);
        return *this;
    }

    TreeSelectItem& selectable(Prop<bool> value = true) {
        identity_ = std::make_shared<const int>(0);
        selectable_ = std::move(value);
        return *this;
    }

    TreeSelectItem& checkable(Prop<bool> value = true) {
        identity_ = std::make_shared<const int>(0);
        checkable_ = std::move(value);
        return *this;
    }

    TreeSelectItem& disableCheckbox(Prop<bool> value = true) {
        identity_ = std::make_shared<const int>(0);
        check_disabled_ = std::move(value);
        return *this;
    }

    TreeSelectItem& isLeaf(Prop<bool> value = true) {
        identity_ = std::make_shared<const int>(0);
        leaf_ = std::move(value);
        return *this;
    }

    TreeSelectItem& contentRevision(std::uint64_t value) {
        identity_ = std::make_shared<const int>(0);
        revision_ = value;
        return *this;
    }

    friend bool operator==(const TreeSelectItem& a, const TreeSelectItem& b) {
        return a.identity_ == b.identity_ && a.revision_ == b.revision_;
    }

private:
    friend class detail::TreeSelectComponentHost;
    String key_;
    Prop<String> title_;
    std::vector<TreeSelectItem> children_;
    Prop<bool> disabled_{false};
    Prop<bool> visible_{true};
    Prop<bool> selectable_{true};
    Prop<bool> checkable_{true};
    Prop<bool> check_disabled_{false};
    Prop<bool> leaf_{false};
    std::optional<TreeTitle> rich_title_;
    std::optional<TreeIcon> icon_;
    std::uint64_t revision_{};
    std::shared_ptr<const int> identity_{std::make_shared<const int>(0)};
};

enum class TreeSelectCheckedStrategy { ShowAll, ShowParent, ShowChild };
enum class TreeSelectTagCount { Responsive };
using TreeSelectMaxTagCount = std::variant<std::size_t, TreeSelectTagCount>;

struct TreeSelectLabelValue final {
    String value;
    String label;
    bool half_checked{};
    friend bool operator==(const TreeSelectLabelValue&, const TreeSelectLabelValue&) = default;
};

using TreeSelectLabelValues = std::vector<TreeSelectLabelValue>;

struct TreeSelectTagSlot final {};

using TreeSelectTag = SlotContent<TreeSelectTagSlot>;

struct TreeSelectTagInfo final {
    String value;
    Prop<String> label;
    Prop<bool> disabled;
    std::function<void()> onClose;
};

struct TreeSelectPopupSlot final {};

using TreeSelectPopup = SlotContent<TreeSelectPopupSlot>;

struct TreeSelectAffixSlot final {};

using TreeSelectAffix = SlotContent<TreeSelectAffixSlot>;

class TreeSelectProps final {
public:
    TreeSelectProps& items(Prop<std::vector<TreeSelectItem>> value) {
        items_ = std::move(value);
        return *this;
    }

    TreeSelectProps& value(Prop<TreeKeys> value) {
        values_ = std::move(value);
        return *this;
    }

    TreeSelectProps& value(Prop<TreeSelectLabelValues> value) {
        labels_ = std::move(value);
        return *this;
    }

    template <class T>
        requires std::same_as<std::remove_cvref_t<T>, TreeKeys>
    TreeSelectProps& defaultValue(T&& value) {
        default_values_ = std::forward<T>(value);
        return *this;
    }

    TreeSelectProps& multiple(Prop<bool> value = true) {
        multiple_ = std::move(value);
        return *this;
    }

    TreeSelectProps& treeCheckable(Prop<bool> value = true) {
        checkable_ = std::move(value);
        return *this;
    }

    TreeSelectProps& treeCheckStrictly(Prop<bool> value = true) {
        strict_ = std::move(value);
        return *this;
    }

    TreeSelectProps& showCheckedStrategy(Prop<TreeSelectCheckedStrategy> value) {
        strategy_ = std::move(value);
        return *this;
    }

    TreeSelectProps& labelInValue(Prop<bool> value = true) {
        label_value_ = std::move(value);
        return *this;
    }

    TreeSelectProps& searchValue(Prop<String> value) {
        query_ = std::move(value);
        return *this;
    }

    TreeSelectProps& showSearch(Prop<bool> value = true) {
        search_ = std::move(value);
        return *this;
    }

    TreeSelectProps& onSearch(std::function<void(String)> value) {
        on_search_ = std::move(value);
        return *this;
    }

    TreeSelectProps& filterTreeNode(std::function<bool(const String&, const String&)> value) {
        filter_ = std::move(value);
        return *this;
    }

    TreeSelectProps& loadData(TreeLoader value) {
        loader_ = std::move(value);
        return *this;
    }

    TreeSelectProps& treeLoadedKeys(Prop<TreeKeys> value) {
        loaded_ = std::move(value);
        return *this;
    }

    TreeSelectProps& onTreeLoad(std::function<void(TreeLoadInfo)> value) {
        on_load_ = std::move(value);
        return *this;
    }

    TreeSelectProps& virtualized(Prop<bool> value = true) {
        virtual_ = std::move(value);
        return *this;
    }

    TreeSelectProps& rowHeight(Prop<float> value) {
        row_height_ = std::move(value);
        return *this;
    }

    TreeSelectProps& treeLine(Prop<bool> value = true) {
        lines_ = std::move(value);
        return *this;
    }

    TreeSelectProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        return *this;
    }

    TreeSelectProps& placement(Prop<PopoverPlacement> value) {
        placement_ = std::move(value);
        return *this;
    }

    TreeSelectProps& maxTagCount(std::size_t value) {
        return maxTagCount(Prop<TreeSelectMaxTagCount>{TreeSelectMaxTagCount{value}});
    }

    TreeSelectProps& maxTagCount(TreeSelectTagCount value) {
        return maxTagCount(Prop<TreeSelectMaxTagCount>{TreeSelectMaxTagCount{value}});
    }

    TreeSelectProps& maxTagCount(Prop<TreeSelectMaxTagCount> value) {
        max_tags_ = std::move(value);
        return *this;
    }

    TreeSelectProps& maxTagTextLength(std::size_t value) {
        max_tag_text_ = value;
        return *this;
    }

    TreeSelectProps& tagRender(std::function<TreeSelectTag(TreeSelectTagInfo)> value) {
        tag_ = std::move(value);
        return *this;
    }

    TreeSelectProps& popupRender(std::function<TreeSelectPopup(std::function<void()>)> value) {
        popup_ = std::move(value);
        return *this;
    }

    TreeSelectProps& prefix(TreeSelectAffix value) {
        prefix_ = std::move(value);
        return *this;
    }

    TreeSelectProps& suffix(TreeSelectAffix value) {
        suffix_ = std::move(value);
        return *this;
    }

    TreeSelectProps& treeRef(TreeRef value) {
        tree_ref_ = std::move(value);
        return *this;
    }

    TreeSelectProps& onValuesChange(std::function<void(TreeKeys)> value) {
        on_values_ = std::move(value);
        return *this;
    }

    TreeSelectProps& onLabelChange(std::function<void(TreeSelectLabelValues)> value) {
        on_labels_ = std::move(value);
        return *this;
    }

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
    std::optional<Prop<std::vector<TreeSelectItem>>> items_;
    std::optional<Prop<TreeKeys>> values_;
    std::optional<Prop<TreeSelectLabelValues>> labels_;
    std::optional<TreeKeys> default_values_;
    Prop<bool> multiple_{false};
    Prop<bool> checkable_{false};
    Prop<bool> strict_{false};
    Prop<bool> label_value_{false};
    Prop<bool> search_{false};
    Prop<bool> virtual_{false};
    Prop<bool> rtl_{false};
    Prop<bool> lines_{false};
    Prop<float> row_height_{24};
    Prop<TreeSelectCheckedStrategy> strategy_{TreeSelectCheckedStrategy::ShowChild};
    Prop<TreeSelectMaxTagCount> max_tags_{TreeSelectMaxTagCount{std::size_t{100000}}};
    std::size_t max_tag_text_{100000};
    std::optional<Prop<String>> query_;
    std::optional<Prop<TreeKeys>> loaded_;
    Prop<PopoverPlacement> placement_{PopoverPlacement::BottomLeft};
    TreeLoader loader_;
    std::function<void(TreeLoadInfo)> on_load_;
    std::function<void(String)> on_search_;
    std::function<bool(const String&, const String&)> filter_;
    std::function<TreeSelectTag(TreeSelectTagInfo)> tag_;
    std::function<TreeSelectPopup(std::function<void()>)> popup_;
    std::optional<TreeSelectAffix> prefix_;
    std::optional<TreeSelectAffix> suffix_;
    std::optional<TreeRef> tree_ref_;
    std::function<void(TreeKeys)> on_values_;
    std::function<void(TreeSelectLabelValues)> on_labels_;
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

inline void TreeSelect(TreeSelectProps props) {
    TreeSelect(std::move(props), {});
}
} // namespace ryn
