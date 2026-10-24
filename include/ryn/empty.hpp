#pragma once
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <optional>

namespace ryn {
namespace detail {
class EmptyResultComponentHost;
}

enum class EmptyImage { Default, Compact, None };

struct EmptyImageSlot final {};

struct EmptyExtraSlot final {};

using EmptyImageContent = SlotContent<EmptyImageSlot>;
using EmptyExtra = SlotContent<EmptyExtraSlot>;

struct EmptySlots final {
    std::optional<EmptyImageContent> image;
    std::optional<EmptyExtra> extra;
};

class EmptyProps final {
public:
    EmptyProps& image(Prop<EmptyImage> value) {
        image_ = std::move(value);
        return *this;
    }

    EmptyProps& description(Prop<String> value) {
        description_ = std::move(value);
        return *this;
    }

    template <std::size_t N> EmptyProps& description(const char8_t (&value)[N]) {
        return description(String{value});
    }

    EmptyProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::EmptyResultComponentHost;
    Prop<EmptyImage> image_{EmptyImage::Default};
    Prop<String> description_{String{u8"No data"}};
    LayoutStyle layout_;
};

void Empty(EmptyProps props = {}, EmptySlots slots = {});
} // namespace ryn
