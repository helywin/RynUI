#pragma once

#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>

#include <functional>
#include <optional>
#include <vector>
#include <variant>
#include <cstdint>

namespace ryn {
namespace detail {
class SegmentedComponentHost;
}

using SegmentedValue = std::variant<String, double>;
enum class SegmentedShape { Default, Round };

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

    SegmentedOption& value(SegmentedValue value) {
        value_ = std::move(value);
        return *this;
    }

    SegmentedOption& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    SegmentedOption& contentRevision(std::uint64_t value) {
        content_revision_ = value;
        return *this;
    }

    SegmentedOption& iconRevision(std::uint64_t value) {
        icon_revision_ = value;
        return *this;
    }

private:
    friend class detail::SegmentedComponentHost;
    String key_;
    Prop<String> label_;
    std::optional<SegmentedLabel> content_;
    std::optional<SegmentedIcon> icon_;
    Prop<bool> disabled_{false};
    Prop<bool> visible_{true};
    std::optional<SegmentedValue> value_;
    std::uint64_t content_revision_{};
    std::uint64_t icon_revision_{};
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

    template <std::size_t N> SegmentedProps& defaultValue(const char8_t (&value)[N]) {
        return defaultValue(String{value});
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

    SegmentedProps& value(Prop<SegmentedValue> value) {
        typed_value_ = std::move(value);
        return *this;
    }

    SegmentedProps& value(double value) {
        return this->value(SegmentedValue{value});
    }

    SegmentedProps& defaultValue(double value) {
        typed_default_ = SegmentedValue{value};
        return *this;
    }

    SegmentedProps& defaultValue(SegmentedValue value) {
        typed_default_ = std::move(value);
        return *this;
    }

    SegmentedProps& onValueChange(std::function<void(SegmentedValue)> value) {
        on_value_change_ = std::move(value);
        return *this;
    }

    SegmentedProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        return *this;
    }

    SegmentedProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    SegmentedProps& shape(Prop<SegmentedShape> value) {
        shape_ = std::move(value);
        return *this;
    }

    SegmentedProps& animated(Prop<bool> value) {
        animated_ = std::move(value);
        return *this;
    }

private:
    friend class detail::SegmentedComponentHost;
    std::optional<Prop<SegmentedValue>> typed_value_;
    std::optional<SegmentedValue> typed_default_;
    std::function<void(SegmentedValue)> on_value_change_;
    Prop<bool> rtl_{false};
    Prop<bool> visible_{true};
    Prop<bool> animated_{true};
    Prop<SegmentedShape> shape_{SegmentedShape::Default};
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
void Segmented(SegmentedProps props, Prop<std::vector<SegmentedOption>> options);
} // namespace ryn
