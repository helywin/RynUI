#pragma once

#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace ryn {
namespace detail {
class SegmentedComponentHost;
}

struct SegmentedLabelSlot final {};

struct SegmentedIconSlot final {};

using SegmentedLabel = SlotContent<SegmentedLabelSlot>;
using SegmentedIcon = SlotContent<SegmentedIconSlot>;

class SegmentedOption final {
public:
    SegmentedOption(String key, String label) : key_(std::move(key)), label_(std::move(label)) {}

    SegmentedOption& label(Prop<String> value) {
        label_ = std::move(value);
        return *this;
    }

    template <std::size_t N> SegmentedOption& label(const char8_t (&value)[N]) {
        return label(String{value});
    }

    SegmentedOption& content(SegmentedLabel value) {
        content_ = std::move(value);
        return *this;
    }

    SegmentedOption& icon(SegmentedIcon value) {
        icon_ = std::move(value);
        return *this;
    }

    SegmentedOption& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

private:
    friend class detail::SegmentedComponentHost;
    String key_;
    Prop<String> label_;
    std::optional<SegmentedLabel> content_;
    std::optional<SegmentedIcon> icon_;
    Prop<bool> disabled_{false};
};

class SegmentedProps final {
public:
    SegmentedProps& value(Prop<String> value) {
        value_ = std::move(value);
        return *this;
    }

    template <std::size_t N> SegmentedProps& value(const char8_t (&value)[N]) {
        return this->value(String{value});
    }

    SegmentedProps& defaultValue(String value) {
        default_value_ = std::move(value);
        return *this;
    }

    SegmentedProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    SegmentedProps& block(Prop<bool> value = true) {
        block_ = std::move(value);
        return *this;
    }

    SegmentedProps& vertical(Prop<bool> value = true) {
        vertical_ = std::move(value);
        return *this;
    }

    SegmentedProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    SegmentedProps& onChange(std::function<void(String)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    SegmentedProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::SegmentedComponentHost;
    std::optional<Prop<String>> value_;
    std::optional<String> default_value_;
    Prop<bool> disabled_{false};
    Prop<bool> block_{false};
    Prop<bool> vertical_{false};
    Prop<ControlSize> size_{ControlSize::Middle};
    std::function<void(String)> on_change_;
    LayoutStyle layout_;
};

void Segmented(SegmentedProps props, std::vector<SegmentedOption> options);
} // namespace ryn
