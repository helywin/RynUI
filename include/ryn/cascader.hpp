#pragma once
#include <ryn/button.hpp>
#include <ryn/input.hpp>
#include <ryn/popover.hpp>
#include <atomic>
#include <thread>
#include <variant>

namespace ryn {
namespace detail {
class CascaderComponentHost;
}

using CascaderValue = std::variant<String, double>;
using CascaderPath = std::vector<CascaderValue>;
using CascaderPaths = std::vector<CascaderPath>;
enum class CascaderCheckedStrategy { ShowParent, ShowChild };
enum class CascaderExpandTrigger { Click, Hover };
enum class CascaderTagCount { Responsive };
using CascaderMaxTagCount = std::variant<std::size_t, CascaderTagCount>;

struct CascaderTriggerSlot final {};

using CascaderTriggerContent = SlotContent<CascaderTriggerSlot>;

struct CascaderEmptySlot final {};

using CascaderEmptyContent = SlotContent<CascaderEmptySlot>;

struct CascaderTagSlot final {};

using CascaderTagContent = SlotContent<CascaderTagSlot>;

struct CascaderTagInfo final {
    CascaderPath value;
    Prop<String> label;
    Prop<bool> disabled;
    std::function<void()> onClose;
};

class CascaderRef final {
public:
    [[nodiscard]] bool focus() const {
        return input_.bound() ? input_.focus() : button_.focus();
    }

    [[nodiscard]] bool blur() const {
        return input_.bound() ? input_.blur() : button_.blur();
    }

    [[nodiscard]] bool bound() const {
        return input_.bound() || button_.bound();
    }

private:
    friend class detail::CascaderComponentHost;
    InputRef input_;
    ButtonRef button_;
};

struct CascaderOptionSlot final {};

using CascaderOptionContent = SlotContent<CascaderOptionSlot>;

struct CascaderPopupSlot final {};

using CascaderPopupContent = SlotContent<CascaderPopupSlot>;

struct CascaderOption final {
    String key;
    CascaderValue value{String{}};
    String label;
    std::vector<CascaderOption> children;
    bool disabled{};
    bool disable_checkbox{};
    bool is_leaf{true};
    std::optional<CascaderOptionContent> content;
    // Same key/revision retains the factory and its local draft. Increment to replace it.
    std::uint64_t content_revision{};

    friend bool operator==(const CascaderOption& a, const CascaderOption& b) {
        return a.key == b.key && a.value == b.value && a.label == b.label && a.children == b.children &&
               a.disabled == b.disabled && a.disable_checkbox == b.disable_checkbox && a.is_leaf == b.is_leaf &&
               a.content.has_value() == b.content.has_value() && a.content_revision == b.content_revision;
    }
};

using CascaderSelectedOptions = std::vector<std::vector<CascaderOption>>;

struct CascaderLoadResult final {
    std::vector<CascaderOption> children;
    String error;
};

// App dispatches completion to the mounting thread. Foreign-thread, cancelled,
// superseded and unmounted completions return false without touching UI state.
struct CascaderLoadRequest final {
    CascaderPath path;
    std::shared_ptr<const std::atomic_bool> cancelled;
    std::function<bool(CascaderLoadResult)> complete;
};

using CascaderLoader = std::function<void(CascaderLoadRequest)>;

struct CascaderSearch final {
    std::function<bool(const String&, const std::vector<CascaderOption>&)> filter;
    std::function<bool(const CascaderPath&, const CascaderPath&, const String&)> sort;
    std::function<String(const String&, const std::vector<CascaderOption>&)> render;
    std::size_t limit{50};
    bool auto_clear_search_value{true};
    bool match_input_width{true};
};

class CascaderProps final {
public:
    CascaderProps& options(Prop<std::vector<CascaderOption>> value) {
        options_ = std::move(value);
        return *this;
    }

    CascaderProps& multiple(Prop<bool> value) {
        multiple_ = std::move(value);
        return *this;
    }

    CascaderProps& changeOnSelect(Prop<bool> value) {
        change_on_select_ = std::move(value);
        return *this;
    }

    CascaderProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    CascaderProps& readOnly(Prop<bool> value) {
        read_only_ = std::move(value);
        return *this;
    }

    CascaderProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    CascaderProps& showSearch(Prop<bool> value) {
        show_search_ = std::move(value);
        return *this;
    }

    CascaderProps& allowClear(Prop<bool> value) {
        allow_clear_ = std::move(value);
        return *this;
    }

    CascaderProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    CascaderProps& status(Prop<InputStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    CascaderProps& variant(Prop<InputVariant> value) {
        variant_ = std::move(value);
        return *this;
    }

    CascaderProps& direction(Prop<TextDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    CascaderProps& placement(Prop<PopoverPlacement> value) {
        placement_ = std::move(value);
        explicit_placement_ = true;
        return *this;
    }

    CascaderProps& expandTrigger(Prop<CascaderExpandTrigger> value) {
        expand_trigger_ = std::move(value);
        return *this;
    }

    CascaderProps& showCheckedStrategy(Prop<CascaderCheckedStrategy> value) {
        strategy_ = std::move(value);
        return *this;
    }

    CascaderProps& placeholder(Prop<String> value) {
        placeholder_ = std::move(value);
        return *this;
    }

    CascaderProps& listHeight(Prop<float> value) {
        height_ = std::move(value);
        return *this;
    }

    CascaderProps& columnWidth(Prop<float> value) {
        width_ = std::move(value);
        return *this;
    }

    CascaderProps& value(Prop<CascaderPaths> value) {
        value_ = std::move(value);
        return *this;
    }

    CascaderProps& defaultValue(CascaderPaths value) {
        default_value_ = std::move(value);
        return *this;
    }

    CascaderProps& open(Prop<bool> value) {
        open_ = std::move(value);
        return *this;
    }

    CascaderProps& defaultOpen(bool value) {
        default_open_ = std::move(value);
        return *this;
    }

    CascaderProps& expandedPath(Prop<CascaderPath> value) {
        expanded_ = std::move(value);
        return *this;
    }

    CascaderProps& defaultExpandedPath(CascaderPath value) {
        default_expanded_ = std::move(value);
        return *this;
    }

    CascaderProps& search(CascaderSearch value) {
        search_ = std::move(value);
        return *this;
    }

    CascaderProps& loadData(CascaderLoader value) {
        loader_ = std::move(value);
        return *this;
    }

    CascaderProps& displayRender(std::function<String(const CascaderPaths&)> value) {
        display_ = std::move(value);
        return *this;
    }

    CascaderProps& triggerContent(CascaderTriggerContent value) {
        trigger_ = std::move(value);
        return *this;
    }

    CascaderProps& emptyContent(CascaderEmptyContent value) {
        empty_content_ = std::move(value);
        return *this;
    }

    CascaderProps& popupContent(CascaderPopupContent value) {
        popup_ = std::move(value);
        return *this;
    }

    CascaderProps& onChange(std::function<void(CascaderPaths)> value) {
        on_change_ = std::move(value);
        on_change_options_ = {};
        return *this;
    }

    CascaderProps& onChange(std::function<void(CascaderPaths, CascaderSelectedOptions)> value) {
        on_change_options_ = std::move(value);
        on_change_ = {};
        return *this;
    }

    CascaderProps& onOpenChange(std::function<void(bool)> value) {
        on_open_ = std::move(value);
        return *this;
    }

    CascaderProps& onExpand(std::function<void(CascaderPath)> value) {
        on_expand_ = std::move(value);
        return *this;
    }

    CascaderProps& onSearch(std::function<void(String)> value) {
        on_search_ = std::move(value);
        return *this;
    }

    CascaderProps& maxTagCount(std::size_t value) {
        return maxTagCount(Prop<CascaderMaxTagCount>{CascaderMaxTagCount{value}});
    }

    CascaderProps& maxTagCount(CascaderTagCount value) {
        return maxTagCount(Prop<CascaderMaxTagCount>{CascaderMaxTagCount{value}});
    }

    CascaderProps& maxTagCount(Prop<CascaderMaxTagCount> value) {
        max_tags_ = std::move(value);
        return *this;
    }

    CascaderProps& maxTagTextLength(Prop<std::size_t> value) {
        max_tag_text_ = std::move(value);
        return *this;
    }

    CascaderProps& maxTagPlaceholder(std::function<String(const CascaderPaths&)> value) {
        tag_placeholder_ = std::move(value);
        return *this;
    }

    CascaderProps& tagRender(std::function<CascaderTagContent(CascaderTagInfo)> value) {
        tag_render_ = std::move(value);
        return *this;
    }

    CascaderProps& searchValue(Prop<String> value) {
        query_ = std::move(value);
        return *this;
    }

    CascaderProps& defaultSearchValue(String value) {
        default_query_ = std::move(value);
        return *this;
    }

    CascaderProps& notFoundContent(Prop<String> value) {
        empty_ = std::move(value);
        return *this;
    }

    CascaderProps& prefix(InputPrefix value) {
        prefix_ = std::move(value);
        return *this;
    }

    CascaderProps& suffixIcon(Prop<IconSource> value) {
        suffix_icon_ = std::move(value);
        return *this;
    }

    CascaderProps& searchIcon(Prop<IconSource> value) {
        search_icon_ = std::move(value);
        return *this;
    }

    CascaderProps& expandIcon(Prop<IconSource> value) {
        expand_icon_ = std::move(value);
        explicit_expand_icon_ = true;
        return *this;
    }

    CascaderProps& clearIcon(Prop<IconSource> value) {
        clear_icon_ = std::move(value);
        return *this;
    }

    CascaderProps& removeIcon(Prop<IconSource> value) {
        remove_icon_ = std::move(value);
        return *this;
    }

    CascaderProps& loadingIcon(Prop<IconSource> value) {
        loading_icon_ = std::move(value);
        return *this;
    }

    CascaderProps& ref(CascaderRef value) {
        reference_ = std::move(value);
        return *this;
    }

    CascaderProps& onClear(std::function<void()> value) {
        on_clear_ = std::move(value);
        return *this;
    }

    CascaderProps& onFocus(std::function<void()> value) {
        on_focus_ = std::move(value);
        return *this;
    }

    CascaderProps& onBlur(std::function<void()> value) {
        on_blur_ = std::move(value);
        return *this;
    }

    CascaderProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::CascaderComponentHost;
    Prop<std::vector<CascaderOption>> options_{std::vector<CascaderOption>{}};
    Prop<bool> multiple_{false};
    Prop<bool> change_on_select_{false};
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    Prop<bool> visible_{true};
    Prop<bool> show_search_{false};
    Prop<bool> allow_clear_{true};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<InputStatus> status_{InputStatus::Default};
    Prop<InputVariant> variant_{InputVariant::Outlined};
    Prop<TextDirection> direction_{TextDirection::LeftToRight};
    Prop<PopoverPlacement> placement_{PopoverPlacement::BottomLeft};
    bool explicit_placement_{};
    bool explicit_expand_icon_{};
    Prop<CascaderExpandTrigger> expand_trigger_{CascaderExpandTrigger::Click};
    Prop<CascaderCheckedStrategy> strategy_{CascaderCheckedStrategy::ShowParent};
    Prop<String> placeholder_{String{}};
    std::optional<Prop<float>> height_;
    std::optional<Prop<float>> width_;
    std::optional<Prop<CascaderMaxTagCount>> max_tags_;
    std::optional<Prop<std::size_t>> max_tag_text_;
    std::function<String(const CascaderPaths&)> tag_placeholder_;
    std::function<CascaderTagContent(CascaderTagInfo)> tag_render_;
    std::optional<Prop<String>> query_;
    std::optional<String> default_query_;
    Prop<String> empty_{String{u8"No data"}};
    std::optional<InputPrefix> prefix_;
    Prop<IconSource> search_icon_{IconSource{IconName::SearchOutlined}};
    Prop<IconSource> suffix_icon_{IconSource{IconName::DownOutlined}};
    Prop<IconSource> expand_icon_{IconSource{IconName::RightOutlined}};
    Prop<IconSource> clear_icon_{IconSource{IconName::CloseCircleFilled}};
    Prop<IconSource> remove_icon_{IconSource{IconName::CloseOutlined}};
    Prop<IconSource> loading_icon_{IconSource{IconName::LoadingOutlined}};
    CascaderRef reference_;
    std::function<void()> on_clear_;
    std::function<void()> on_focus_;
    std::function<void()> on_blur_;
    std::optional<Prop<CascaderPaths>> value_;
    std::optional<CascaderPaths> default_value_;
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    std::optional<Prop<CascaderPath>> expanded_;
    std::optional<CascaderPath> default_expanded_;
    CascaderSearch search_;
    CascaderLoader loader_;
    std::function<String(const CascaderPaths&)> display_;
    std::optional<CascaderPopupContent> popup_;
    std::optional<CascaderTriggerContent> trigger_;
    std::optional<CascaderEmptyContent> empty_content_;
    std::function<void(CascaderPaths)> on_change_;
    std::function<void(CascaderPaths, CascaderSelectedOptions)> on_change_options_;
    std::function<void(bool)> on_open_;
    std::function<void(CascaderPath)> on_expand_;
    std::function<void(String)> on_search_;
    LayoutStyle layout_;
};

void Cascader(CascaderProps props);
void CascaderPanel(CascaderProps props);
} // namespace ryn
