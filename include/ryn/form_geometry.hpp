#pragma once

namespace ryn {
struct FormRect final {
    float x{};
    float y{};
    float width{};
    float height{};
    friend bool operator==(const FormRect&, const FormRect&) = default;
};

struct FormControlGeometry final {
    FormRect bounds;
    FormRect clip;
    bool eligible{};
};
} // namespace ryn
