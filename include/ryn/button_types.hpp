#pragma once

#include <cstddef>
#include <cstdint>

namespace ryn {

enum class ButtonType { Default, Primary, Danger, Text, Dashed, Link };

enum class ButtonVariant : std::uint8_t { Outlined, Dashed, Solid, Filled, Text, Link };

enum class ButtonColor : std::uint8_t {
    Default,
    Primary,
    Danger,
    Blue,
    Purple,
    Cyan,
    Green,
    Magenta,
    Pink,
    Red,
    Orange,
    Yellow,
    Volcano,
    Geekblue,
    Lime,
    Gold
};

inline constexpr std::size_t button_color_count = 16;

enum class ButtonShape : std::uint8_t { Default, Circle, Round, Square };

enum class ButtonIconPlacement : std::uint8_t { Start, End };

} // namespace ryn
