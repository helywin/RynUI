#pragma once

#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>
#include <ryn/text_direction.hpp>
#include <ryn/text_area.hpp>
#include <ryn/theme.hpp>

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <utility>
#include <vector>
#include <span>

namespace ryn {
namespace detail {
struct ConfigDefaults;
}

namespace detail {

struct TypographyPropsAccess;
class TypographyComponentHost;

} // namespace detail

enum class TypographyType : std::uint8_t {
    Default,
    Secondary,
    Success,
    Warning,
    Danger,
};

// Inline semantics that change the rendered shape rather than only the colour.
// `Code` and `Keyboard` switch to the code font family at the token scale,
// `Mark` paints a highlight behind the glyphs, and the two decoration kinds draw
// a line through the run.
enum class TypographyInline : std::uint8_t {
    None,
    Strong,
    Italic,
    Code,
    Keyboard,
    Mark,
    Underline,
    Strikethrough,
};

// Value runs combine semantic flags. The immutable content owns both runs and
// their concatenated source; no markup is accepted or interpreted.
struct TypographyRun final {
    String content;
    std::optional<TypographyType> type;
    bool strong{};
    bool italic{};
    bool code{};
    bool keyboard{};
    bool mark{};
    bool underline{};
    bool strikethrough{};
    friend bool operator==(const TypographyRun&, const TypographyRun&) = default;
};

class TypographyContent final {
public:
    TypographyContent() = default;

    explicit TypographyContent(std::vector<TypographyRun> runs) : runs_(std::move(runs)) {
        std::string bytes;
        for (const auto& run : runs_) {
            bytes += run.content.bytes();
        }
        plain_text_ = String::from_utf8(bytes).value();
    }

    [[nodiscard]] std::span<const TypographyRun> runs() const noexcept {
        return runs_;
    }

    [[nodiscard]] const String& plain_text() const noexcept {
        return plain_text_;
    }

    friend bool operator==(const TypographyContent&, const TypographyContent&) = default;

private:
    std::vector<TypographyRun> runs_;
    String plain_text_;
};

enum class TypographyCopyFormat : std::uint8_t { PlainText, Html };

struct TypographyEllipsis final {
    std::optional<std::size_t> rows{1};
    String suffix{u8"…"};
    bool expandable{};
    bool expanded{};
    std::optional<String> tooltip;
    String expand_text{u8"展开"};
    String collapse_text{u8"收起"};
    friend bool operator==(const TypographyEllipsis&, const TypographyEllipsis&) = default;
};

struct TypographyCopyable final {
    bool enabled{true};
    String tooltip{u8"复制"};
    String copied_tooltip{u8"已复制"};
    float feedback_milliseconds{3000};
    TypographyCopyFormat format{TypographyCopyFormat::PlainText};
    friend bool operator==(const TypographyCopyable&, const TypographyCopyable&) = default;
};

struct TypographyEditable final {
    bool enabled{true};
    std::size_t max_length{std::numeric_limits<std::size_t>::max()};
    String tooltip{u8"编辑"};
    TextAreaAutoSize auto_size{true};
    friend bool operator==(const TypographyEditable&, const TypographyEditable&) = default;
};

// Typography props shared by `Title`, `Text` and `Paragraph`. `type` selects a
// semantic colour, `strong`/`italic` select real faces from the font chain, and
// decoration flags control retained background and foreground layers.
class TypographyProps final {
public:
    TypographyProps& content(Prop<String> value) {
        content_ = std::move(value);
        rich_content_.reset();
        return *this;
    }

    TypographyProps& content(Prop<TypographyContent> value) {
        rich_content_ = std::move(value);
        return *this;
    }

    template <std::size_t N> TypographyProps& content(const char8_t (&literal)[N]) {
        return content(String{literal});
    }

    TypographyProps& type(Prop<TypographyType> value) {
        type_ = std::move(value);
        return *this;
    }

    TypographyProps& direction(Prop<TextDirection> value) {
        direction_ = std::move(value);
        explicit_direction_ = true;
        return *this;
    }

    TypographyProps& code(Prop<bool> value) {
        code_ = std::move(value);
        return *this;
    }

    TypographyProps& keyboard(Prop<bool> value) {
        keyboard_ = std::move(value);
        return *this;
    }

    TypographyProps& mark(Prop<bool> value) {
        mark_ = std::move(value);
        return *this;
    }

    TypographyProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    TypographyProps& strong(Prop<bool> value) {
        strong_ = std::move(value);
        return *this;
    }

    TypographyProps& italic(Prop<bool> value) {
        italic_ = std::move(value);
        return *this;
    }

    TypographyProps& underline(Prop<bool> value) {
        underline_ = std::move(value);
        return *this;
    }

    TypographyProps& strikethrough(Prop<bool> value) {
        strikethrough_ = std::move(value);
        return *this;
    }

    TypographyProps& ellipsis(Prop<TypographyEllipsis> value) {
        ellipsis_ = std::move(value);
        return *this;
    }

    TypographyProps& copyable(Prop<TypographyCopyable> value) {
        copyable_ = std::move(value);
        return *this;
    }

    TypographyProps& editable(Prop<TypographyEditable> value) {
        editable_ = std::move(value);
        return *this;
    }

    TypographyProps& onEdit(std::function<void(String)> value) {
        on_edit_ = std::move(value);
        return *this;
    }

    TypographyProps& onCopy(std::function<void(bool)> value) {
        on_copy_ = std::move(value);
        return *this;
    }

    TypographyProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::ConfigDefaults;
    friend struct detail::TypographyPropsAccess;
    friend class detail::TypographyComponentHost;

    Prop<String> content_{String{}};
    std::optional<Prop<TypographyContent>> rich_content_;
    Prop<TextDirection> direction_{TextDirection::Auto};
    bool explicit_direction_{};
    std::optional<Prop<TypographyType>> type_;
    std::optional<Prop<bool>> code_;
    std::optional<Prop<bool>> keyboard_;
    std::optional<Prop<bool>> mark_;
    std::optional<Prop<bool>> disabled_;
    std::optional<Prop<bool>> strong_;
    std::optional<Prop<bool>> italic_;
    std::optional<Prop<bool>> underline_;
    std::optional<Prop<bool>> strikethrough_;
    std::optional<Prop<TypographyEllipsis>> ellipsis_;
    std::optional<Prop<TypographyCopyable>> copyable_;
    std::optional<Prop<TypographyEditable>> editable_;
    std::function<void(String)> on_edit_;
    std::function<void(bool)> on_copy_;
    LayoutStyle layout_;
};

// `level` is a reactive `Prop<T>`, so changing it after mount re-resolves the
// heading tokens while the component keeps its identity.
class TitleProps final {
public:
    TitleProps& content(Prop<String> value) {
        typography_.content(std::move(value));
        return *this;
    }

    TitleProps& content(Prop<TypographyContent> value) {
        typography_.content(std::move(value));
        return *this;
    }

    template <std::size_t N> TitleProps& content(const char8_t (&literal)[N]) {
        return content(String{literal});
    }

    TitleProps& level(Prop<TypographyLevel> value) {
        level_ = std::move(value);
        return *this;
    }

    TitleProps& direction(Prop<TextDirection> value) {
        typography_.direction(std::move(value));
        return *this;
    }

    TitleProps& type(Prop<TypographyType> value) {
        typography_.type(std::move(value));
        return *this;
    }

    TitleProps& disabled(Prop<bool> value) {
        typography_.disabled(std::move(value));
        return *this;
    }

    TitleProps& code(Prop<bool> value) {
        typography_.code(std::move(value));
        return *this;
    }

    TitleProps& keyboard(Prop<bool> value) {
        typography_.keyboard(std::move(value));
        return *this;
    }

    TitleProps& mark(Prop<bool> value) {
        typography_.mark(std::move(value));
        return *this;
    }

    TitleProps& ellipsis(Prop<TypographyEllipsis> value) {
        typography_.ellipsis(std::move(value));
        return *this;
    }

    TitleProps& copyable(Prop<TypographyCopyable> value) {
        typography_.copyable(std::move(value));
        return *this;
    }

    TitleProps& editable(Prop<TypographyEditable> value) {
        typography_.editable(std::move(value));
        return *this;
    }

    TitleProps& onEdit(std::function<void(String)> value) {
        typography_.onEdit(std::move(value));
        return *this;
    }

    TitleProps& onCopy(std::function<void(bool)> value) {
        typography_.onCopy(std::move(value));
        return *this;
    }

    TitleProps& strong(Prop<bool> value) {
        typography_.strong(std::move(value));
        return *this;
    }

    TitleProps& italic(Prop<bool> value) {
        typography_.italic(std::move(value));
        return *this;
    }

    TitleProps& underline(Prop<bool> value) {
        typography_.underline(std::move(value));
        return *this;
    }

    TitleProps& strikethrough(Prop<bool> value) {
        typography_.strikethrough(std::move(value));
        return *this;
    }

    TitleProps& layout(LayoutStyle value) {
        typography_.layout(std::move(value));
        return *this;
    }

private:
    friend struct detail::ConfigDefaults;
    friend struct detail::TypographyPropsAccess;

    TypographyProps typography_;
    Prop<TypographyLevel> level_{TypographyLevel::H1};
};

void Title(TitleProps props);
void Text(TypographyProps props);
void Paragraph(TypographyProps props);

class LinkProps final {
public:
    LinkProps& content(Prop<String> value) {
        content_ = std::move(value);
        return *this;
    }

    template <std::size_t N> LinkProps& content(const char8_t (&value)[N]) {
        return content(String{value});
    }

    LinkProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    LinkProps& type(Prop<TypographyType> value) {
        type_ = std::move(value);
        return *this;
    }

    LinkProps& onClick(std::function<void()> value) {
        click_ = std::move(value);
        return *this;
    }

    LinkProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TypographyComponentHost;
    Prop<String> content_{String{}};
    Prop<bool> disabled_{false};
    Prop<TypographyType> type_{TypographyType::Default};
    std::function<void()> click_;
    LayoutStyle layout_;
};

void Link(LinkProps props);

inline void Title(TypographyLevel level, String content) {
    TitleProps props;
    props.level(level).content(std::move(content));
    Title(std::move(props));
}

} // namespace ryn
