#pragma once

#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>

#include <cstddef>
#include <utility>

namespace ryn {
namespace detail {

struct TextPropsAccess;

} // namespace detail

enum class TextTone {
    Primary,
    Secondary,
    Disabled,
};

class TextProps final {
public:
    TextProps& content(Prop<String> value) {
        content_ = std::move(value);
        return *this;
    }

    template <std::size_t N>
    TextProps& content(const char8_t (&literal)[N]) {
        return content(String{literal});
    }

    TextProps& tone(Prop<TextTone> value) {
        tone_ = std::move(value);
        return *this;
    }

    TextProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::TextPropsAccess;

    Prop<String> content_{String{}};
    Prop<TextTone> tone_{TextTone::Primary};
    LayoutStyle layout_;
};

void Text(TextProps props);

inline void Text(String content) {
    TextProps props;
    props.content(std::move(content));
    Text(std::move(props));
}

template <std::size_t N>
void Text(const char8_t (&literal)[N]) {
    Text(String{literal});
}

} // namespace ryn
