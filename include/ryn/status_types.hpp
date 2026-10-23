#pragma once
#include <ryn/design_token.hpp>
#include <variant>

namespace ryn {
enum class Status { None, Default, Success, Processing, Error, Warning };

enum class PresetColor {
    Default,
    Magenta,
    Red,
    Volcano,
    Orange,
    Gold,
    Lime,
    Green,
    Cyan,
    Blue,
    Geekblue,
    Purple,
    Pink
};

using TagColor = std::variant<PresetColor, Status, Color>;
} // namespace ryn
