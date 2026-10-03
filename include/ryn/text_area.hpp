#pragma once

#include <ryn/input.hpp>

namespace ryn {

enum class TextAreaResize : std::uint8_t { None, Vertical, Horizontal, Both };

struct TextAreaAutoSize final {
    bool enabled{};
    std::size_t min_rows{1};
    std::optional<std::size_t> max_rows;
    friend bool operator==(const TextAreaAutoSize&, const TextAreaAutoSize&) = default;
};

struct TextAreaSize final {
    float width{};
    float height{};
    friend bool operator==(TextAreaSize, TextAreaSize) = default;
};

using TextAreaRef = InputRef;

namespace detail {
struct TextAreaPropsAccess;

struct TextAreaPropsData final {
    Prop<std::size_t> rows{std::size_t{4}};
    Prop<TextAreaAutoSize> auto_size{TextAreaAutoSize{}};
    Prop<bool> wrap{true};
    Prop<TextAreaResize> resize{TextAreaResize::Vertical};
    std::function<void(TextAreaSize)> on_resize;
};
} // namespace detail

class TextAreaProps final : public InputPropsBase<TextAreaProps> {
public:
    TextAreaProps& rows(Prop<std::size_t> value) {
        textarea_.rows = std::move(value);
        return *this;
    }

    TextAreaProps& autoSize(Prop<TextAreaAutoSize> value) {
        textarea_.auto_size = std::move(value);
        return *this;
    }

    TextAreaProps& autoSize(bool enabled = true) {
        return autoSize(TextAreaAutoSize{enabled});
    }

    TextAreaProps& wrap(Prop<bool> value) {
        textarea_.wrap = std::move(value);
        return *this;
    }

    TextAreaProps& resize(Prop<TextAreaResize> value) {
        textarea_.resize = std::move(value);
        return *this;
    }

    TextAreaProps& onResize(std::function<void(TextAreaSize)> callback) {
        textarea_.on_resize = std::move(callback);
        return *this;
    }

private:
    friend struct detail::TextAreaPropsAccess;
    detail::TextAreaPropsData textarea_;
};

void TextArea(TextAreaProps props = {});

} // namespace ryn
