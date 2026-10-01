#pragma once

#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>
#include <ryn/theme.hpp>

#include <cstdint>
#include <optional>
#include <utility>

namespace ryn {
namespace detail {

struct TypographyPropsAccess;

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

// Typography props shared by `Title`, `Text` and `Paragraph`. `type` selects a
// semantic colour, `strong`/`italic` select real faces from the font chain, and
// decoration flags control retained background and foreground layers.
class TypographyProps final {
public:
    TypographyProps& content(Prop<String> value) {
        content_ = std::move(value);
        return *this;
    }

    template <std::size_t N>
    TypographyProps& content(const char8_t (&literal)[N]) {
        return content(String{literal});
    }

    TypographyProps& type(Prop<TypographyType> value) {
        type_ = std::move(value);
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
        ellipsis_ = std::move(value); return *this;
    }

    TypographyProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::TypographyPropsAccess;

    Prop<String> content_{String{}};
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

    template <std::size_t N>
    TitleProps& content(const char8_t (&literal)[N]) {
        return content(String{literal});
    }

    TitleProps& level(Prop<TypographyLevel> value) {
        level_ = std::move(value);
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

    TitleProps& code(Prop<bool> value) { typography_.code(std::move(value)); return *this; }
    TitleProps& keyboard(Prop<bool> value) { typography_.keyboard(std::move(value)); return *this; }
    TitleProps& mark(Prop<bool> value) { typography_.mark(std::move(value)); return *this; }
    TitleProps& ellipsis(Prop<TypographyEllipsis> value) { typography_.ellipsis(std::move(value)); return *this; }

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
    friend struct detail::TypographyPropsAccess;

    TypographyProps typography_;
    Prop<TypographyLevel> level_{TypographyLevel::H1};
};

void Title(TitleProps props);
void Text(TypographyProps props);
void Paragraph(TypographyProps props);

inline void Title(TypographyLevel level, String content) {
    TitleProps props;
    props.level(level).content(std::move(content));
    Title(std::move(props));
}

} // namespace ryn
