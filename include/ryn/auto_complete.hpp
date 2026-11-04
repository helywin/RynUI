#pragma once

#include <ryn/input.hpp>
#include <ryn/async_options.hpp>
#include <ryn/text_area.hpp>
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
    bool group{};
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

struct AutoCompleteOptionGroup final {
    String key;
    String label;
    std::vector<AutoCompleteOption> options;
    AutoCompleteRichLabel rich_label;
    friend bool operator==(const AutoCompleteOptionGroup&, const AutoCompleteOptionGroup&) = default;
};

using AutoCompleteSource = AsyncOptionsSource<AutoCompleteOption>;

class AutoCompletePopupRenderer final {
public:
    AutoCompletePopupRenderer() = default;

    template <class F>
        requires std::is_invocable_r_v<void, F, std::function<void()>>
    AutoCompletePopupRenderer(F render)
        : render_(std::make_shared<const std::function<void(std::function<void()>)>>(std::move(render))) {}

    void operator()(std::function<void()> origin) const {
        if (render_) {
            (*render_)(std::move(origin));
        } else {
            origin();
        }
    }

    [[nodiscard]] explicit operator bool() const {
        return bool(render_);
    }

    friend bool operator==(const AutoCompletePopupRenderer&, const AutoCompletePopupRenderer&) = default;

private:
    std::shared_ptr<const std::function<void(std::function<void()>)>> render_;
};

struct AutoCompleteEditorContext final {
    InputProps input;
    TextAreaProps text_area;
    std::optional<InputPrefix> prefix;
    std::optional<InputSuffix> suffix;
};

// Mount exactly one Input or TextArea using the supplied value/ref/event bindings.
// The context and origin slots are synchronous mount inputs, not reusable factories.
class AutoCompleteEditor final {
public:
    AutoCompleteEditor() = default;

    template <class F>
        requires std::is_invocable_r_v<void, F, InputProps, std::optional<InputPrefix>, std::optional<InputSuffix>>
    AutoCompleteEditor(F render) : render_(std::make_shared<const Function>(std::move(render))) {}

    template <class F>
        requires std::is_invocable_r_v<void, F, AutoCompleteEditorContext>
    AutoCompleteEditor(F render)
        : context_(std::make_shared<const std::function<void(AutoCompleteEditorContext)>>(std::move(render))) {}

    void operator()(AutoCompleteEditorContext context) const {
        if (context_) {
            (*context_)(std::move(context));
        } else if (render_) {
            (*render_)(std::move(context.input), std::move(context.prefix), std::move(context.suffix));
        } else {
            Input(std::move(context.input), std::move(context.prefix), std::move(context.suffix));
        }
    }

    friend bool operator==(const AutoCompleteEditor&, const AutoCompleteEditor&) = default;

private:
    using Function = std::function<void(InputProps, std::optional<InputPrefix>, std::optional<InputSuffix>)>;
    std::shared_ptr<const Function> render_;
    std::shared_ptr<const std::function<void(AutoCompleteEditorContext)>> context_;
};

class AutoCompleteProps final {
public:
    AutoCompleteProps& options(Prop<std::vector<AutoCompleteOption>> value) {
        options_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& options(std::initializer_list<AutoCompleteOption> value) {
        return options(Prop<std::vector<AutoCompleteOption>>{std::vector<AutoCompleteOption>{value}});
    }

    AutoCompleteProps& clearIcon(Prop<IconSource> value) {
        clear_icon_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& notFoundContent(Prop<String> value) {
        not_found_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& groups(Prop<std::vector<AutoCompleteOptionGroup>> value) {
        groups_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& source(Prop<AutoCompleteSource> value) {
        source_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& virtualList(Prop<bool> value = true) {
        virtual_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& direction(Prop<TextDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& motion(Prop<bool> value = true) {
        motion_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& popupRender(Prop<AutoCompletePopupRenderer> value) {
        popup_render_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& editor(Prop<AutoCompleteEditor> value) {
        editor_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& popupMatchSelectWidth(Prop<bool> value) {
        match_width_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& popupWidth(Prop<float> value) {
        popup_width_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& onLoadError(std::function<void(String)> value) {
        on_load_error_ = std::move(value);
        return *this;
    }

    AutoCompleteProps& onPopupScroll(std::function<void(float)> value) {
        on_scroll_ = std::move(value);
        return *this;
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

    template <std::size_t N> AutoCompleteProps& placeholder(const char8_t (&value)[N]) {
        return placeholder(String{value});
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
    Prop<IconSource> clear_icon_{IconSource{IconName::CloseCircleFilled}};
    Prop<String> not_found_{String{}};
    std::optional<Prop<std::vector<AutoCompleteOptionGroup>>> groups_;
    Prop<AutoCompleteSource> source_{AutoCompleteSource{}};
    Prop<bool> virtual_{true};
    Prop<TextDirection> direction_{TextDirection::Auto};
    Prop<bool> motion_{false};
    Prop<AutoCompletePopupRenderer> popup_render_{AutoCompletePopupRenderer{}};
    Prop<AutoCompleteEditor> editor_{AutoCompleteEditor{}};
    Prop<bool> match_width_{true};
    Prop<float> popup_width_{0};
    std::function<void(String)> on_load_error_;
    std::function<void(float)> on_scroll_;
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
