#pragma once

#include <ryn/input.hpp>
#include <ryn/async_options.hpp>
#include <ryn/text_area.hpp>
#include <initializer_list>
#include <vector>

namespace ryn {
namespace detail {
class MentionsComponentHost;
}

struct MentionsOptionLabelSlot final {};

using MentionsOptionLabel = SlotContent<MentionsOptionLabelSlot>;

// Copies preserve the identity of a retained rich label.
class MentionsRichLabel final {
public:
    MentionsRichLabel() = default;

    explicit MentionsRichLabel(MentionsOptionLabel content)
        : content_(std::make_shared<const MentionsOptionLabel>(std::move(content))) {}

    [[nodiscard]] const MentionsOptionLabel* content() const noexcept {
        return content_.get();
    }

    friend bool operator==(const MentionsRichLabel&, const MentionsRichLabel&) = default;

private:
    std::shared_ptr<const MentionsOptionLabel> content_;
};

struct MentionsOption final {
    String key;
    String value;
    String label;
    bool disabled{};
    MentionsRichLabel rich_label;
    bool group{};
    friend bool operator==(const MentionsOption&, const MentionsOption&) = default;
};

class MentionsFilter final {
public:
    // Native Unicode case-folded value filtering; false disables local filtering.
    MentionsFilter() = default;

    explicit MentionsFilter(bool value_substring) : substring_(value_substring) {}

    template <typename F>
        requires std::is_invocable_r_v<bool, F, StringView, const MentionsOption&>
    MentionsFilter(F predicate)
        : predicate_(
              std::make_shared<const std::function<bool(StringView, const MentionsOption&)>>(std::move(predicate))) {}

    [[nodiscard]] bool accepts(StringView query, const MentionsOption& option) const;
    friend bool operator==(const MentionsFilter&, const MentionsFilter&) = default;

private:
    bool substring_{true};
    std::shared_ptr<const std::function<bool(StringView, const MentionsOption&)>> predicate_;
};

using MentionsRef = InputRef;

struct MentionsOptionGroup final {
    String key;
    String label;
    std::vector<MentionsOption> options;
    MentionsRichLabel rich_label;
    friend bool operator==(const MentionsOptionGroup&, const MentionsOptionGroup&) = default;
};

using MentionsSource = AsyncOptionsSource<MentionsOption>;

class MentionsPopupRenderer final {
public:
    MentionsPopupRenderer() = default;

    template <class F>
        requires std::is_invocable_r_v<void, F, std::function<void()>>
    MentionsPopupRenderer(F render)
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

    friend bool operator==(const MentionsPopupRenderer&, const MentionsPopupRenderer&) = default;

private:
    std::shared_ptr<const std::function<void(std::function<void()>)>> render_;
};

struct MentionsEditorContext final {
    TextAreaProps text_area;
};

// Synchronous mount slot. Preserve the supplied native multiline value/ref/event bindings.
class MentionsEditor final {
public:
    MentionsEditor() = default;

    template <class F>
        requires std::is_invocable_r_v<void, F, MentionsEditorContext>
    MentionsEditor(F render)
        : render_(std::make_shared<const std::function<void(MentionsEditorContext)>>(std::move(render))) {}

    void operator()(MentionsEditorContext context) const {
        if (render_) {
            (*render_)(std::move(context));
        } else {
            TextArea(std::move(context.text_area));
        }
    }

    friend bool operator==(const MentionsEditor&, const MentionsEditor&) = default;

private:
    std::shared_ptr<const std::function<void(MentionsEditorContext)>> render_;
};

enum class MentionsPlacement : std::uint8_t { Bottom, Top };

struct MentionsConfig final {
    std::vector<String> prefix{String{u8"@"}};
    String split{u8" "};
    friend bool operator==(const MentionsConfig&, const MentionsConfig&) = default;
};

struct MentionEntity final {
    String prefix;
    String value;
    friend bool operator==(const MentionEntity&, const MentionEntity&) = default;
};

// Pure parsing, without a mounted editor. Tokens start at split boundaries.
[[nodiscard]] std::vector<MentionEntity> getMentions(StringView value, MentionsConfig config = {});

[[nodiscard]] inline std::vector<MentionEntity> getMentions(const String& value, MentionsConfig config = {}) {
    return getMentions(value.view(), std::move(config));
}

class MentionsSearchValidator final {
public:
    MentionsSearchValidator() = default;

    template <class F>
        requires std::is_invocable_r_v<bool, F, StringView, const MentionsConfig&>
    MentionsSearchValidator(F callback)
        : callback_(
              std::make_shared<const std::function<bool(StringView, const MentionsConfig&)>>(std::move(callback))) {}

    [[nodiscard]] bool accepts(StringView, const MentionsConfig&) const;
    friend bool operator==(const MentionsSearchValidator&, const MentionsSearchValidator&) = default;

private:
    std::shared_ptr<const std::function<bool(StringView, const MentionsConfig&)>> callback_;
};

class MentionsProps final {
public:
    MentionsProps& prefix(Prop<std::vector<String>> value) {
        single_prefix_.reset();
        prefixes_ = std::move(value);
        return *this;
    }

    MentionsProps& prefix(std::initializer_list<String> value) {
        return prefix(std::vector<String>{value});
    }

    MentionsProps& prefix(Prop<String> value) {
        single_prefix_ = std::move(value);
        return *this;
    }

    MentionsProps& prefix(String value) {
        return prefix(Prop<String>{std::move(value)});
    }

    template <std::size_t N> MentionsProps& prefix(const char8_t (&value)[N]) {
        return prefix(String{value});
    }

    MentionsProps& split(Prop<String> value) {
        split_ = std::move(value);
        return *this;
    }

    template <std::size_t N> MentionsProps& split(const char8_t (&value)[N]) {
        return split(String{value});
    }

    MentionsProps& validateSearch(Prop<MentionsSearchValidator> value) {
        validate_search_ = std::move(value);
        return *this;
    }

    MentionsProps& loading(Prop<bool> value = true) {
        loading_ = std::move(value);
        return *this;
    }

    MentionsProps& visible(Prop<bool> value = true) {
        visible_ = std::move(value);
        return *this;
    }

    MentionsProps& placement(Prop<MentionsPlacement> value) {
        placement_ = std::move(value);
        return *this;
    }

    MentionsProps& rows(Prop<std::size_t> value) {
        rows_ = std::move(value);
        return *this;
    }

    MentionsProps& autoSize(Prop<TextAreaAutoSize> value) {
        auto_size_ = std::move(value);
        return *this;
    }

    MentionsProps& autoSize(bool value = true) {
        return autoSize(TextAreaAutoSize{value, 1, {}});
    }

    MentionsProps& maxLength(Prop<std::size_t> value) {
        max_length_ = std::move(value);
        return *this;
    }

    MentionsProps& wrap(Prop<bool> value) {
        wrap_ = std::move(value);
        return *this;
    }

    MentionsProps& resize(Prop<TextAreaResize> value) {
        resize_ = std::move(value);
        return *this;
    }

    MentionsProps& onResize(std::function<void(TextAreaSize)> value) {
        on_resize_ = std::move(value);
        return *this;
    }

    MentionsProps& clearDisabled(Prop<bool> value) {
        clear_disabled_ = std::move(value);
        return *this;
    }

    MentionsProps& options(Prop<std::vector<MentionsOption>> value) {
        options_ = std::move(value);
        return *this;
    }

    MentionsProps& options(std::initializer_list<MentionsOption> value) {
        return options(Prop<std::vector<MentionsOption>>{std::vector<MentionsOption>{value}});
    }

    MentionsProps& clearIcon(Prop<IconSource> value) {
        clear_icon_ = std::move(value);
        return *this;
    }

    MentionsProps& notFoundContent(Prop<String> value) {
        not_found_ = std::move(value);
        return *this;
    }

    template <std::size_t N> MentionsProps& notFoundContent(const char8_t (&value)[N]) {
        return notFoundContent(String{value});
    }

    MentionsProps& notFoundContent(Prop<MentionsRichLabel> value) {
        empty_content_ = std::move(value);
        return *this;
    }

    MentionsProps& groups(Prop<std::vector<MentionsOptionGroup>> value) {
        groups_ = std::move(value);
        return *this;
    }

    MentionsProps& source(Prop<MentionsSource> value) {
        source_ = std::move(value);
        return *this;
    }

    MentionsProps& virtualList(Prop<bool> value = true) {
        virtual_ = std::move(value);
        return *this;
    }

    MentionsProps& direction(Prop<TextDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    MentionsProps& motion(Prop<bool> value = true) {
        motion_ = std::move(value);
        return *this;
    }

    MentionsProps& popupRender(Prop<MentionsPopupRenderer> value) {
        popup_render_ = std::move(value);
        return *this;
    }

    MentionsProps& editor(Prop<MentionsEditor> value) {
        editor_ = std::move(value);
        return *this;
    }

    MentionsProps& popupWidth(Prop<float> value) {
        popup_width_ = std::move(value);
        return *this;
    }

    MentionsProps& onLoadError(std::function<void(String)> value) {
        on_load_error_ = std::move(value);
        return *this;
    }

    MentionsProps& onPopupScroll(std::function<void(float)> value) {
        on_scroll_ = std::move(value);
        return *this;
    }

    MentionsProps& value(Prop<String> value) {
        value_ = std::move(value);
        return *this;
    }

    template <std::size_t N> MentionsProps& value(const char8_t (&value)[N]) {
        return this->value(String{value});
    }

    MentionsProps& defaultValue(String value) {
        default_value_ = std::move(value);
        return *this;
    }

    MentionsProps& placeholder(Prop<String> value) {
        placeholder_ = std::move(value);
        return *this;
    }

    MentionsProps& filterOption(bool value) {
        return filterOption(MentionsFilter{value});
    }

    MentionsProps& filterOption(Prop<MentionsFilter> value) {
        filter_ = std::move(value);
        return *this;
    }

    template <std::size_t N> MentionsProps& placeholder(const char8_t (&value)[N]) {
        return placeholder(String{value});
    }

    MentionsProps& defaultActiveFirstOption(Prop<bool> value) {
        active_first_ = std::move(value);
        return *this;
    }

    MentionsProps& allowClear(Prop<bool> value = true) {
        allow_clear_ = std::move(value);
        return *this;
    }

    MentionsProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    MentionsProps& readOnly(Prop<bool> value = true) {
        read_only_ = std::move(value);
        return *this;
    }

    MentionsProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    MentionsProps& variant(Prop<InputVariant> value) {
        variant_ = std::move(value);
        return *this;
    }

    MentionsProps& status(Prop<InputStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    MentionsProps& open(Prop<bool> value) {
        open_ = std::move(value);
        return *this;
    }

    MentionsProps& defaultOpen(bool value = true) {
        default_open_ = value;
        return *this;
    }

    MentionsProps& listHeight(Prop<float> value) {
        list_height_ = std::move(value);
        return *this;
    }

    MentionsProps& onChange(std::function<void(String)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    MentionsProps& onSearch(std::function<void(String, String)> value) {
        on_search_ = std::move(value);
        return *this;
    }

    MentionsProps& onSelect(std::function<void(MentionsOption, String)> value) {
        on_select_ = std::move(value);
        return *this;
    }

    MentionsProps& onClear(std::function<void()> value) {
        on_clear_ = std::move(value);
        return *this;
    }

    MentionsProps& onOpenChange(std::function<void(bool)> value) {
        on_open_ = std::move(value);
        return *this;
    }

    MentionsProps& onFocus(std::function<void()> value) {
        on_focus_ = std::move(value);
        return *this;
    }

    MentionsProps& onBlur(std::function<void()> value) {
        on_blur_ = std::move(value);
        return *this;
    }

    MentionsProps& ref(MentionsRef value) {
        ref_ = std::move(value);
        return *this;
    }

    MentionsProps& autoFocus(bool value = true) {
        auto_focus_ = value;
        return *this;
    }

    MentionsProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::MentionsComponentHost;
    std::optional<Prop<String>> single_prefix_;
    Prop<std::vector<String>> prefixes_{std::vector<String>{String{u8"@"}}};
    Prop<String> split_{String{u8" "}};
    Prop<MentionsSearchValidator> validate_search_{MentionsSearchValidator{}};
    Prop<bool> loading_{false};
    Prop<bool> visible_{true};
    Prop<MentionsPlacement> placement_{MentionsPlacement::Bottom};
    Prop<std::size_t> rows_{std::size_t{1}};
    Prop<TextAreaAutoSize> auto_size_{TextAreaAutoSize{}};
    std::optional<Prop<std::size_t>> max_length_;
    Prop<bool> wrap_{true};
    Prop<TextAreaResize> resize_{TextAreaResize::Vertical};
    Prop<bool> clear_disabled_{false};
    std::function<void(TextAreaSize)> on_resize_;
    Prop<IconSource> clear_icon_{IconSource{IconName::CloseCircleFilled}};
    Prop<MentionsRichLabel> empty_content_{MentionsRichLabel{}};
    std::optional<Prop<String>> not_found_;
    std::optional<Prop<std::vector<MentionsOptionGroup>>> groups_;
    Prop<MentionsSource> source_{MentionsSource{}};
    Prop<bool> virtual_{true};
    Prop<TextDirection> direction_{TextDirection::Auto};
    Prop<bool> motion_{false};
    Prop<MentionsPopupRenderer> popup_render_{MentionsPopupRenderer{}};
    Prop<MentionsEditor> editor_{MentionsEditor{}};
    Prop<bool> match_width_{true};
    Prop<float> popup_width_{0};
    std::function<void(String)> on_load_error_;
    std::function<void(float)> on_scroll_;
    Prop<std::vector<MentionsOption>> options_{std::vector<MentionsOption>{}};
    std::optional<Prop<String>> value_;
    std::optional<String> default_value_;
    Prop<String> placeholder_{String{}};
    Prop<MentionsFilter> filter_{MentionsFilter{}};
    Prop<bool> active_first_{true};
    Prop<bool> allow_clear_{false};
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<InputVariant> variant_{InputVariant::Outlined};
    Prop<InputStatus> status_{InputStatus::Default};
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    Prop<float> list_height_{250};
    std::function<void(String)> on_change_;
    std::function<void(String, String)> on_search_;
    std::function<void(MentionsOption, String)> on_select_;
    std::function<void()> on_clear_;
    std::function<void(bool)> on_open_;
    std::function<void()> on_focus_;
    std::function<void()> on_blur_;
    std::optional<MentionsRef> ref_;
    bool auto_focus_{};
    LayoutStyle layout_;
};

void Mentions(MentionsProps props = {});
} // namespace ryn
