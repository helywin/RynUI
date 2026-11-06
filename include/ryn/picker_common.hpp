#pragma once
#include <ryn/component_locales.hpp>

#include <ryn/component.hpp>
#include <ryn/string.hpp>
#include <ryn/text_direction.hpp>
#include <array>
#include <functional>
#include <optional>
#include <vector>

namespace ryn {

enum class PickerEndpoint { Start, End };
enum class PickerPlacement { BottomLeft, BottomRight, TopLeft, TopRight };

template <typename T> struct PickerRange final {
    std::optional<T> start;
    std::optional<T> end;
    friend bool operator==(const PickerRange&, const PickerRange&) = default;
};

template <typename T> struct PickerRangePreset final {
    String key;
    String label;
    std::function<PickerRange<T>()> value;
};

struct PickerRangePolicy final {
    std::array<bool, 2> allow_empty{false, false};
    std::array<bool, 2> disabled{false, false};
    bool order{true};
    friend bool operator==(const PickerRangePolicy&, const PickerRangePolicy&) = default;
};

struct PickerIconSlot final {};

using PickerIcon = SlotContent<PickerIconSlot>;

} // namespace ryn
