#include "graphics/arc_cap.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace ryn::graphics {
namespace {

constexpr double tau = 2 * std::numbers::pi_v<double>;
constexpr double tolerance = 128 * std::numeric_limits<double>::epsilon();

struct Vec final {
    double x{};
    double y{};

    Vec operator+(Vec value) const noexcept {
        return {x + value.x, y + value.y};
    }

    Vec operator-(Vec value) const noexcept {
        return {x - value.x, y - value.y};
    }

    Vec operator*(double value) const noexcept {
        return {x * value, y * value};
    }
};

double dot(Vec first, Vec second) noexcept {
    return first.x * second.x + first.y * second.y;
}

double cross(Vec first, Vec second) noexcept {
    return first.x * second.y - first.y * second.x;
}

double length(Vec value) noexcept {
    return std::hypot(value.x, value.y);
}

Vec direction(double angle) noexcept {
    return {std::cos(angle), std::sin(angle)};
}

double angle_from(Vec point, double start) noexcept {
    auto value = std::fmod(std::atan2(point.y, point.x) - start, tau);
    if (value < 0) {
        value += tau;
    }
    return value >= tau - tolerance ? 0 : value;
}

struct Primitive final {
    bool rectangle{};
    double inner{};
    double outer{};
    double start{};
    double sweep{};
    Vec center;
    Vec radial;
    Vec tangent;
    double half_width{};
    double half_length{};

    bool contains(Vec point, bool strict) const noexcept {
        if (rectangle) {
            const auto relative = point - center;
            const auto x = std::abs(dot(relative, radial));
            const auto y = std::abs(dot(relative, tangent));
            return strict ? x < half_width - tolerance && y < half_length - tolerance
                          : x <= half_width + tolerance && y <= half_length + tolerance;
        }
        const auto radius = length(point);
        if (strict ? radius >= outer - tolerance || (inner > 0 && radius <= inner + tolerance)
                   : radius > outer + tolerance || radius < inner - tolerance) {
            return false;
        }
        if (sweep == tau) {
            return true;
        }
        if (radius == 0) {
            return !strict;
        }
        const auto angle = angle_from(point, start);
        return strict ? angle > tolerance && angle < sweep - tolerance : angle <= sweep + tolerance;
    }
};

struct Boundary final {
    bool circular{};
    std::size_t owner{};
    Vec first;
    Vec last;
    Vec outward;
    double radius{};
    double start{};
    double sweep{};
    double radial_sign{1};

    Vec at(double parameter) const noexcept {
        return circular ? direction(start + sweep * parameter) * radius : first + (last - first) * parameter;
    }

    Vec normal(Vec point) const noexcept {
        return circular ? point * (radial_sign / radius) : outward;
    }

    bool parameter(Vec point, double& result) const noexcept {
        if (circular) {
            if (std::abs(length(point) - radius) > tolerance) {
                return false;
            }
            const auto angle = angle_from(point, start);
            result = angle / sweep;
        } else {
            const auto axis = last - first;
            const auto relative = point - first;
            const auto extent = dot(axis, axis);
            result = dot(relative, axis) / extent;
            if (std::abs(cross(relative, axis)) > tolerance * std::sqrt(extent)) {
                return false;
            }
        }
        return result >= -tolerance && result <= 1 + tolerance;
    }

    double closest(Vec point, double low, double high) const noexcept {
        auto result = std::min(length(point - at(low)), length(point - at(high)));
        double parameter{};
        if (circular) {
            parameter = angle_from(point, start) / sweep;
        } else {
            const auto axis = last - first;
            parameter = dot(point - first, axis) / dot(axis, axis);
        }
        if (parameter >= low && parameter <= high) {
            result = std::min(result, length(point - at(parameter)));
        }
        return result;
    }
};

struct Splits final {
    // At most two intersections with each of the sixteen boundary curves.
    std::array<double, 34> values{0, 1};
    std::size_t count{2};

    void add(double value) noexcept {
        if (value >= -tolerance && value <= 1 + tolerance) {
            values[count++] = std::clamp(value, 0.0, 1.0);
        }
    }
};

void intersections(const Boundary& first, const Boundary& second, Splits& splits) noexcept {
    if (!first.circular && !second.circular) {
        const auto axis = first.last - first.first;
        const auto other = second.last - second.first;
        const auto offset = second.first - first.first;
        const auto determinant = cross(axis, other);
        if (std::abs(determinant) > tolerance * length(axis) * length(other)) {
            const auto parameter = cross(offset, axis) / determinant;
            if (parameter >= -tolerance && parameter <= 1 + tolerance) {
                splits.add(cross(offset, other) / determinant);
            }
        } else {
            double parameter{};
            if (first.parameter(second.first, parameter)) {
                splits.add(parameter);
            }
            if (first.parameter(second.last, parameter)) {
                splits.add(parameter);
            }
        }
        return;
    }
    if (first.circular && second.circular) {
        if (first.radius == second.radius) {
            double parameter{};
            if (first.parameter(second.at(0), parameter)) {
                splits.add(parameter);
            }
            if (first.parameter(second.at(1), parameter)) {
                splits.add(parameter);
            }
        }
        return;
    }
    const auto& circle = first.circular ? first : second;
    const auto& line = first.circular ? second : first;
    const auto axis = line.last - line.first;
    const auto a = dot(axis, axis);
    const auto b = dot(line.first, axis);
    const auto c = dot(line.first, line.first) - circle.radius * circle.radius;
    const auto discriminant = b * b - a * c;
    if (discriminant < -tolerance * a) {
        return;
    }
    const auto root = std::sqrt(std::max(0.0, discriminant));
    for (const auto parameter : {(-b - root) / a, (-b + root) / a}) {
        if (parameter >= -tolerance && parameter <= 1 + tolerance) {
            double along{};
            if (circle.parameter(line.at(parameter), along)) {
                splits.add(first.circular ? along : parameter);
            }
        }
    }
}

struct ArcUnion final {
    std::array<Primitive, 4> primitives;
    std::size_t primitive_count{};
    std::array<Boundary, 16> boundaries;
    std::size_t boundary_count{};

    void line(Vec first, Vec last, Vec normal, std::size_t owner) noexcept {
        if (first.x != last.x || first.y != last.y) {
            boundaries[boundary_count++] = {false, owner, first, last, normal};
        }
    }

    void sector(double inner, double outer, double start, double sweep) noexcept {
        const auto owner = primitive_count++;
        primitives[owner] = {false, inner, outer, start, sweep};
        boundaries[boundary_count++] = {true, owner, {}, {}, {}, outer, start, sweep, 1};
        if (inner > 0) {
            boundaries[boundary_count++] = {true, owner, {}, {}, {}, inner, start, sweep, -1};
        }
        if (sweep < tau) {
            const auto first = direction(start);
            const auto last = direction(start + sweep);
            line(first * inner, first * outer, {first.y, -first.x}, owner);
            line(last * inner, last * outer, {-last.y, last.x}, owner);
        }
    }

    void cap(double radius, double half_width, double angle, double sign) noexcept {
        const auto owner = primitive_count++;
        const auto radial = direction(angle);
        const Vec tangent{-radial.y, radial.x};
        const auto center = radial * radius + tangent * (sign * half_width / 2);
        primitives[owner] = {true, 0, 0, 0, 0, center, radial, tangent, half_width, half_width / 2};
        const auto x = radial * half_width;
        const auto y = tangent * (half_width / 2);
        const std::array corners{center - x - y, center + x - y, center + x + y, center - x + y};
        for (std::size_t index = 0; index < corners.size(); ++index) {
            const auto first = corners[index];
            const auto last = corners[(index + 1) % corners.size()];
            const auto edge = last - first;
            const auto extent = length(edge);
            line(first, last, {edge.y / extent, -edge.x / extent}, owner);
        }
    }

    bool exposed(std::size_t index, Vec point) const noexcept {
        const auto& boundary = boundaries[index];
        for (std::size_t owner = 0; owner < primitive_count; ++owner) {
            if (owner != boundary.owner && primitives[owner].contains(point, true)) {
                return false;
            }
        }
        // Opposite coincident boundaries are internal joins. Same-facing
        // coincident boundaries are part of the union's outside boundary.
        for (std::size_t other = 0; other < boundary_count; ++other) {
            double parameter{};
            if (boundaries[other].owner != boundary.owner && boundaries[other].parameter(point, parameter) &&
                dot(boundary.normal(point), boundaries[other].normal(point)) < -1 + tolerance) {
                return false;
            }
        }
        return true;
    }

    double distance(Vec point) const noexcept {
        bool inside = false;
        for (std::size_t index = 0; index < primitive_count; ++index) {
            inside = inside || primitives[index].contains(point, false);
        }
        auto nearest = std::numeric_limits<double>::infinity();
        for (std::size_t index = 0; index < boundary_count; ++index) {
            Splits splits;
            for (std::size_t other = 0; other < boundary_count; ++other) {
                if (other != index) {
                    intersections(boundaries[index], boundaries[other], splits);
                }
            }
            std::sort(splits.values.begin(), splits.values.begin() + splits.count);
            for (std::size_t part = 1; part < splits.count; ++part) {
                const auto low = splits.values[part - 1];
                const auto high = splits.values[part];
                if (high > low && exposed(index, boundaries[index].at((low + high) / 2))) {
                    nearest = std::min(nearest, boundaries[index].closest(point, low, high));
                }
            }
        }
        return inside ? -nearest : nearest;
    }
};

} // namespace

float nonround_arc_signed_distance(runtime::Point point, LogicalRoundedRect centerline, float width,
                                   RoundedArcPath arc) noexcept {
    const auto radius = double(centerline.rect.width) / 2;
    if (arc.sweep_radians <= 0 || width <= 0 || radius <= 0) {
        return std::numeric_limits<float>::infinity();
    }
    const auto half_width = double(width) / 2;
    const auto scale = std::max(radius, half_width);
    const Vec relative{(double(point.x) - centerline.rect.x - radius) / scale,
                       (double(point.y) - centerline.rect.y - radius) / scale};
    const auto r = radius / scale;
    const auto h = half_width / scale;
    if (arc.sweep_radians >= 2 * std::numbers::pi_v<float>) {
        const auto distance = length(relative);
        return static_cast<float>((r > h ? std::max(distance - r - h, r - h - distance) : distance - r - h) * scale);
    }
    ArcUnion shape;
    shape.sector(std::max(0.0, r - h), r + h, arc.start_radians, arc.sweep_radians);
    if (h > r) {
        // The normal strip crosses the center: its negative radial part is
        // another sector, rotated pi. Its buried boundaries must also vanish.
        shape.sector(0, h - r, arc.start_radians + std::numbers::pi_v<double>, arc.sweep_radians);
    }
    if (arc.cap == ArcCap::Square) {
        shape.cap(r, h, arc.start_radians, -1);
        shape.cap(r, h, double(arc.start_radians) + arc.sweep_radians, 1);
    }
    return static_cast<float>(shape.distance(relative) * scale);
}

} // namespace ryn::graphics
