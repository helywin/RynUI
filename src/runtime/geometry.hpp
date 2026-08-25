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

struct Color {
    float red{1.0F};
    float green{1.0F};
    float blue{1.0F};
    float alpha{1.0F};

    friend constexpr bool operator==(Color, Color) = default;
};

} // namespace ryn::runtime
