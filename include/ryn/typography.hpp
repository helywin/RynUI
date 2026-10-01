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

// Typography props shared by `Title`, `Text` and `Paragraph`. `type` selects a
// semantic colour, `strong`/`italic` select real faces from the font chain, and
// the decoration flags are carried as data so a later change can render them
// without touching this public shape.
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

    TypographyProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::TypographyPropsAccess;

    Prop<String> content_{String{}};
    std::optional<Prop<TypographyType>> type_;
    std::optional<Prop<bool>> disabled_;
    std::optional<Prop<bool>> strong_;
    std::optional<Prop<bool>> italic_;
    std::optional<Prop<bool>> underline_;
    std::optional<Prop<bool>> strikethrough_;
    LayoutStyle layout_;
};

// `level` is a plain value rather than a `Prop`: mounting a different heading
// level would change the element identity, and RynUI keeps one component
// identity per level instead of re-levelling an existing heading.
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

    TitleProps& level(TypographyLevel value) noexcept {
        level_ = value;
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
    TypographyLevel level_{TypographyLevel::H1};
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
