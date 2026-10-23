#pragma once
#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <optional>

namespace ryn {
namespace detail {
class FeedbackComponentHost;
}

struct SpinContentSlot final {};

struct SpinIndicatorSlot final {};

using SpinContent = SlotContent<SpinContentSlot>;
using SpinIndicator = SlotContent<SpinIndicatorSlot>;

struct SpinSlots final {
    std::optional<SpinIndicator> indicator;
};

class SpinProps final {
public:
    SpinProps& spinning(Prop<bool> value) {
        spinning_ = std::move(value);
        return *this;
    }

    SpinProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    SpinProps& description(Prop<String> value) {
        description_ = std::move(value);
        return *this;
    }

    template <std::size_t N> SpinProps& description(const char8_t (&value)[N]) {
        return description(String{value});
    }

    SpinProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::FeedbackComponentHost;
    Prop<bool> spinning_{true};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<String> description_{String{}};
    LayoutStyle layout_;
};

void Spin(SpinProps props = {});
void Spin(SpinProps props, SpinContent content, SpinSlots slots = {});
} // namespace ryn
