#pragma once

#include <ryn/input.hpp>
#include <ryn/async_options.hpp>

#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <limits>
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
using SelectValues = std::vector<SelectValue>;
enum class SelectMode { Single, Multiple, Tags };
enum class SelectOptionField { Label, Value };
enum class SelectPlacement { BottomLeft, BottomRight, TopLeft, TopRight };

struct SelectOptionContentSlot final {};

using SelectOptionContent = SlotContent<SelectOptionContentSlot>;

class SelectRichContent final {
public:
    SelectRichContent() = default;

    explicit SelectRichContent(SelectOptionContent content)
        : content_(std::make_shared<const SelectOptionContent>(std::move(content))) {}

    [[nodiscard]] const SelectOptionContent* content() const {
        return content_.get();
    }

    friend bool operator==(const SelectRichContent&, const SelectRichContent&) = default;

private:
    std::shared_ptr<const SelectOptionContent> content_;
};

using SelectRef = InputRef;

struct SelectOption final {
    SelectValue value;
    String label;
    bool disabled{};
    SelectRichContent rich_label;
    bool group{};
    String key;
    friend bool operator==(const SelectOption&, const SelectOption&) = default;
};

struct SelectOptionGroup final {
    String key;
    String label;
    std::vector<SelectOption> options;
    SelectRichContent rich_label;
    friend bool operator==(const SelectOptionGroup&, const SelectOptionGroup&) = default;
};

struct SelectRenderInfo final {
    SelectValue value;
    Prop<String> label;
    Prop<bool> selected;
    Prop<bool> disabled;
    std::function<void()> remove;
};

template <class Tag> class SelectRenderer;

namespace detail {
template <class T> struct IsSelectRenderer : std::false_type {};

template <class Tag> struct IsSelectRenderer<SelectRenderer<Tag>> : std::true_type {};
} // namespace detail

template <class Tag> class SelectRenderer final {
public:
    SelectRenderer() = default;

    template <class F>
        requires(!detail::IsSelectRenderer<std::remove_cvref_t<F>>::value &&
                 std::is_invocable_r_v<void, F, SelectRenderInfo>)
    SelectRenderer(F function)
        : function_(std::make_shared<const std::function<void(SelectRenderInfo)>>(std::move(function))) {}

    void operator()(SelectRenderInfo info) const {
        if (function_) {
            (*function_)(std::move(info));
        }
    }

    [[nodiscard]] explicit operator bool() const {
        return bool(function_);
    }

    friend bool operator==(const SelectRenderer&, const SelectRenderer&) = default;

private:
    std::shared_ptr<const std::function<void(SelectRenderInfo)>> function_;
};

struct SelectOptionRendererTag final {};

struct SelectLabelRendererTag final {};

struct SelectTagRendererTag final {};

using SelectOptionRenderer = SelectRenderer<SelectOptionRendererTag>;
using SelectLabelRenderer = SelectRenderer<SelectLabelRendererTag>;
using SelectTagRenderer = SelectRenderer<SelectTagRendererTag>;

class SelectPopupRenderer final {
public:
    SelectPopupRenderer() = default;

    template <class F>
        requires std::is_invocable_r_v<void, F, std::function<void()>>
    SelectPopupRenderer(F function)
        : function_(std::make_shared<const std::function<void(std::function<void()>)>>(std::move(function))) {}

    void operator()(std::function<void()> origin) const;
    friend bool operator==(const SelectPopupRenderer&, const SelectPopupRenderer&) = default;

private:
    std::shared_ptr<const std::function<void(std::function<void()>)>> function_;
};

class SelectFilter final {
public:
    SelectFilter() = default;

    SelectFilter(bool enabled) : enabled_(enabled) {}

    template <class F>
        requires std::is_invocable_r_v<bool, F, StringView, const SelectOption&>
    SelectFilter(F function)
        : function_(std::make_shared<const std::function<bool(StringView, const SelectOption&)>>(std::move(function))) {
    }

    [[nodiscard]] bool accepts(StringView, const SelectOption&, SelectOptionField) const;
    friend bool operator==(const SelectFilter&, const SelectFilter&) = default;

private:
    bool enabled_{true};
    std::shared_ptr<const std::function<bool(StringView, const SelectOption&)>> function_;
};

class SelectSort final {
public:
    SelectSort() = default;

    template <class F>
        requires std::is_invocable_r_v<bool, F, const SelectOption&, const SelectOption&, StringView>
    SelectSort(F function)
        : function_(std::make_shared<const std::function<bool(const SelectOption&, const SelectOption&, StringView)>>(
              std::move(function))) {}

    [[nodiscard]] bool before(const SelectOption& a, const SelectOption& b, StringView query) const {
        return function_ && (*function_)(a, b, query);
    }

    [[nodiscard]] explicit operator bool() const {
        return bool(function_);
    }

    friend bool operator==(const SelectSort&, const SelectSort&) = default;

private:
    std::shared_ptr<const std::function<bool(const SelectOption&, const SelectOption&, StringView)>> function_;
};

using SelectSource = AsyncOptionsSource<SelectOption>;

struct SelectLabeledValue final {
    SelectValue value;
    String label;
    friend bool operator==(const SelectLabeledValue&, const SelectLabeledValue&) = default;
};

using SelectLabeledSelection = std::optional<SelectLabeledValue>;
using SelectLabeledValues = std::vector<SelectLabeledValue>;

enum class SelectInputKey { Other, Enter, Escape, Tab, Up, Down, Home, End, Backspace, Space, Left, Right };

struct SelectKeyEvent final {
    SelectInputKey key{SelectInputKey::Other};
    bool repeat{};
    bool composing{};
    bool shift{};
    bool control{};
    bool alt{};
    bool meta{};
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

    SelectProps& clearIcon(Prop<IconSource> value) {
        clear_icon_ = std::move(value);
        return *this;
    }

    SelectProps& suffixIcon(Prop<IconSource> value) {
        suffix_icon_ = std::move(value);
        return *this;
    }

    SelectProps& searchIcon(Prop<IconSource> value) {
        search_icon_ = std::move(value);
        return *this;
    }

    SelectProps& loadingIcon(Prop<IconSource> value) {
        loading_icon_ = std::move(value);
        return *this;
    }

    SelectProps& removeIcon(Prop<IconSource> value) {
        remove_icon_ = std::move(value);
        return *this;
    }

    SelectProps& menuItemSelectedIcon(Prop<IconSource> value) {
        selected_icon_ = std::move(value);
        return *this;
    }

    SelectProps& onInputKeyDown(std::function<bool(SelectKeyEvent)> value) {
        on_key_ = std::move(value);
        return *this;
    }

    SelectProps& labeledValue(Prop<SelectLabeledSelection> value) {
        labeled_value_ = std::move(value);
        return *this;
    }

    SelectProps& labeledValues(Prop<SelectLabeledValues> value) {
        labeled_values_ = std::move(value);
        return *this;
    }

    SelectProps& mode(Prop<SelectMode> value) {
        mode_ = std::move(value);
        return *this;
    }

    SelectProps& groups(Prop<std::vector<SelectOptionGroup>> value) {
        groups_ = std::move(value);
        return *this;
    }

    SelectProps& values(Prop<SelectValues> value) {
        values_ = std::move(value);
        return *this;
    }

    SelectProps& defaultValues(SelectValues value) {
        default_values_ = std::move(value);
        return *this;
    }

    SelectProps& searchValue(Prop<String> value) {
        search_value_ = std::move(value);
        return *this;
    }

    SelectProps& filterOption(Prop<SelectFilter> value) {
        filter_ = std::move(value);
        return *this;
    }

    SelectProps& filterSort(Prop<SelectSort> value) {
        sort_ = std::move(value);
        return *this;
    }

    SelectProps& optionFilterProp(Prop<SelectOptionField> value) {
        filter_field_ = std::move(value);
        return *this;
    }

    SelectProps& optionLabelProp(Prop<SelectOptionField> value) {
        label_field_ = std::move(value);
        return *this;
    }

    SelectProps& defaultActiveFirstOption(Prop<bool> value) {
        active_first_ = std::move(value);
        return *this;
    }

    SelectProps& autoClearSearchValue(Prop<bool> value) {
        auto_clear_search_ = std::move(value);
        return *this;
    }

    SelectProps& hideSelected(Prop<bool> value = true) {
        hide_selected_ = std::move(value);
        return *this;
    }

    SelectProps& maxCount(Prop<std::size_t> value) {
        max_count_ = std::move(value);
        return *this;
    }

    SelectProps& maxTagCount(Prop<std::size_t> value) {
        max_tag_count_ = std::move(value);
        responsive_tags_ = false;
        return *this;
    }

    SelectProps& responsiveMaxTagCount(Prop<bool> value = true) {
        responsive_tags_ = std::move(value);
        return *this;
    }

    SelectProps& maxTagTextLength(Prop<std::size_t> value) {
        max_tag_text_ = std::move(value);
        return *this;
    }

    SelectProps& maxTagPlaceholder(std::function<String(SelectValues)> value) {
        max_tag_placeholder_ = std::move(value);
        return *this;
    }

    SelectProps& tokenSeparators(std::vector<String> value) {
        token_separators_ = std::move(value);
        return *this;
    }

    SelectProps& tokenizer(std::function<std::vector<String>(StringView)> value) {
        tokenizer_ = std::move(value);
        return *this;
    }

    SelectProps& optionRender(Prop<SelectOptionRenderer> value) {
        option_render_ = std::move(value);
        return *this;
    }

    SelectProps& labelRender(Prop<SelectLabelRenderer> value) {
        label_render_ = std::move(value);
        return *this;
    }

    SelectProps& tagRender(Prop<SelectTagRenderer> value) {
        tag_render_ = std::move(value);
        return *this;
    }

    SelectProps& popupRender(Prop<SelectPopupRenderer> value) {
        popup_render_ = std::move(value);
        return *this;
    }

    SelectProps& source(Prop<SelectSource> value) {
        source_ = std::move(value);
        return *this;
    }

    SelectProps& virtualList(Prop<bool> value = true) {
        virtual_ = std::move(value);
        return *this;
    }

    SelectProps& popupMatchSelectWidth(Prop<bool> value) {
        match_width_ = std::move(value);
        return *this;
    }

    SelectProps& popupWidth(Prop<float> value) {
        popup_width_ = std::move(value);
        return *this;
    }

    SelectProps& placement(Prop<SelectPlacement> value) {
        placement_ = std::move(value);
        return *this;
    }

    SelectProps& direction(Prop<TextDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    SelectProps& motion(Prop<bool> value = true) {
        motion_ = std::move(value);
        return *this;
    }

    SelectProps& readOnly(Prop<bool> value = true) {
        read_only_prop_ = std::move(value);
        return *this;
    }

    SelectProps& onValuesChange(std::function<void(SelectValues)> value) {
        on_values_change_ = std::move(value);
        return *this;
    }

    SelectProps& onLabeledChange(std::function<void(std::vector<SelectLabeledValue>)> value) {
        on_labeled_change_ = std::move(value);
        return *this;
    }

    SelectProps& onSelect(std::function<void(SelectValue, SelectOption)> value) {
        on_select_ = std::move(value);
        return *this;
    }

    SelectProps& onDeselect(std::function<void(SelectValue, SelectOption)> value) {
        on_deselect_ = std::move(value);
        return *this;
    }

    SelectProps& onActive(std::function<void(SelectValue)> value) {
        on_active_ = std::move(value);
        return *this;
    }

    SelectProps& onClear(std::function<void()> value) {
        on_clear_ = std::move(value);
        return *this;
    }

    SelectProps& onPopupScroll(std::function<void(float)> value) {
        on_scroll_ = std::move(value);
        return *this;
    }

    SelectProps& onLoadError(std::function<void(String)> value) {
        on_load_error_ = std::move(value);
        return *this;
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
    Prop<IconSource> clear_icon_{IconSource{IconName::CloseCircleFilled}};
    Prop<IconSource> suffix_icon_{IconSource{IconName::DownOutlined}};
    Prop<IconSource> search_icon_{IconSource{IconName::SearchOutlined}};
    Prop<IconSource> loading_icon_{IconSource{IconName::LoadingOutlined}};
    Prop<IconSource> remove_icon_{IconSource{IconName::CloseOutlined}};
    Prop<IconSource> selected_icon_{IconSource{IconName::CheckOutlined}};
    std::function<bool(SelectKeyEvent)> on_key_;
    std::optional<Prop<SelectLabeledSelection>> labeled_value_;
    std::optional<Prop<SelectLabeledValues>> labeled_values_;
    Prop<SelectMode> mode_{SelectMode::Single};
    std::optional<Prop<std::vector<SelectOptionGroup>>> groups_;
    std::optional<Prop<SelectValues>> values_;
    std::optional<SelectValues> default_values_;
    std::optional<Prop<String>> search_value_;
    Prop<SelectFilter> filter_{SelectFilter{}};
    Prop<SelectSort> sort_{SelectSort{}};
    Prop<SelectOptionField> filter_field_{SelectOptionField::Label};
    Prop<SelectOptionField> label_field_{SelectOptionField::Label};
    Prop<bool> active_first_{true};
    Prop<bool> auto_clear_search_{true};
    Prop<bool> hide_selected_{false};
    Prop<std::size_t> max_count_{std::numeric_limits<std::size_t>::max()};
    Prop<std::size_t> max_tag_count_{std::numeric_limits<std::size_t>::max()};
    Prop<bool> responsive_tags_{false};
    Prop<std::size_t> max_tag_text_{std::numeric_limits<std::size_t>::max()};
    std::function<String(SelectValues)> max_tag_placeholder_;
    std::vector<String> token_separators_;
    std::function<std::vector<String>(StringView)> tokenizer_;
    Prop<SelectOptionRenderer> option_render_{SelectOptionRenderer{}};
    Prop<SelectLabelRenderer> label_render_{SelectLabelRenderer{}};
    Prop<SelectTagRenderer> tag_render_{SelectTagRenderer{}};
    Prop<SelectPopupRenderer> popup_render_{SelectPopupRenderer{}};
    Prop<SelectSource> source_{SelectSource{}};
    Prop<bool> virtual_{true};
    Prop<bool> match_width_{true};
    Prop<float> popup_width_{0};
    Prop<SelectPlacement> placement_{SelectPlacement::BottomLeft};
    Prop<TextDirection> direction_{TextDirection::Auto};
    Prop<bool> motion_{false};
    Prop<bool> read_only_prop_{false};
    std::function<void(SelectValues)> on_values_change_;
    std::function<void(std::vector<SelectLabeledValue>)> on_labeled_change_;
    std::function<void(SelectValue, SelectOption)> on_select_;
    std::function<void(SelectValue, SelectOption)> on_deselect_;
    std::function<void(SelectValue)> on_active_;
    std::function<void()> on_clear_;
    std::function<void(float)> on_scroll_;
    std::function<void(String)> on_load_error_;
    Prop<std::vector<SelectOption>> options_{std::vector<SelectOption>{}};
    std::optional<Prop<SelectSelection>> value_;
    SelectSelection default_value_;
    Prop<String> placeholder_{String{}};
    std::optional<Prop<bool>> show_search_;
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
