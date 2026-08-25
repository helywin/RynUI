#pragma once

namespace ryn::runtime {

struct Point {
    float x{0.0F};
    float y{0.0F};

    friend constexpr bool operator==(Point, Point) = default;
};

struct Size {
    float width{0.0F};
    float height{0.0F};

    friend constexpr bool operator==(Size, Size) = default;
};

struct Rect {
    float x{0.0F};
    float y{0.0F};
    float width{0.0F};
    float height{0.0F};

    friend constexpr bool operator==(Rect, Rect) = default;
};

} // namespace ryn::runtime
