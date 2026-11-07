#pragma once
#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <optional>
#include <chrono>
#include <variant>

namespace ryn {
namespace detail {
struct ConfigDefaults;
}

namespace detail {
class FeedbackComponentHost;
}

struct SpinContentSlot final {};

struct SpinIndicatorSlot final {};

using SpinContent = SlotContent<SpinContentSlot>;
using SpinIndicator = SlotContent<SpinIndicatorSlot>;

struct SpinAutomaticPercent final {
    friend bool operator==(SpinAutomaticPercent, SpinAutomaticPercent) = default;
};

using SpinPercent = std::variant<std::monostate, float, SpinAutomaticPercent>;

struct SpinSlots final {
    std::optional<SpinIndicator> indicator;
};

class SpinProps final {
public:
    SpinProps& fullscreen(Prop<bool> value) {
        fullscreen_ = std::move(value);
        return *this;
    }

    SpinProps& delay(Prop<std::chrono::milliseconds> value) {
        delay_ = std::move(value);
        return *this;
    }

    SpinProps& percent(Prop<SpinPercent> value) {
        percent_ = std::move(value);
        return *this;
    }

    SpinProps& percent(float value) {
        return percent(SpinPercent{value});
    }

    SpinProps& percent(SpinAutomaticPercent value) {
        return percent(SpinPercent{value});
    }

    SpinProps& spinning(Prop<bool> value) {
        spinning_ = std::move(value);
        return *this;
    }

    SpinProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        explicit_size_ = true;
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
    friend struct detail::ConfigDefaults;
    friend class detail::FeedbackComponentHost;
    Prop<bool> spinning_{true};
    Prop<ControlSize> size_{ControlSize::Middle};
    bool explicit_size_{};
    Prop<String> description_{String{}};
    Prop<bool> fullscreen_{false};
    Prop<std::chrono::milliseconds> delay_{std::chrono::milliseconds{0}};
    Prop<SpinPercent> percent_{SpinPercent{}};
    LayoutStyle layout_;
};

void Spin(SpinProps props = {});
void Spin(SpinProps props, SpinSlots slots);
void Spin(SpinProps props, SpinContent content, SpinSlots slots = {});
} // namespace ryn
