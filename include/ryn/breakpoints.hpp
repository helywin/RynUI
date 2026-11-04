#pragma once

#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ryn {

enum class Breakpoint { Xs, Sm, Md, Lg, Xl, Xxl, Xxxl };

class Breakpoints final {
public:
    Breakpoints() = default;

    explicit Breakpoints(std::array<float, 7> widths) : widths_(widths) {
        for (std::size_t index = 0; index < widths_.size(); ++index) {
            if (!std::isfinite(widths_[index]) || widths_[index] < 0 ||
                (index != 0 && widths_[index] <= widths_[index - 1])) {
                throw std::invalid_argument("Breakpoints must be finite, non-negative and strictly increasing");
            }
        }
    }

    [[nodiscard]] float width(Breakpoint breakpoint) const {
        return widths_.at(static_cast<std::size_t>(breakpoint));
    }

    [[nodiscard]] bool matches(Breakpoint breakpoint, float window_width) const {
        if (!std::isfinite(window_width) || window_width < 0) {
            throw std::invalid_argument("Window width must be finite and non-negative");
        }
        // xs is the default configuration, including windows below screenXS.
        return breakpoint == Breakpoint::Xs || window_width >= width(breakpoint);
    }

    friend bool operator==(const Breakpoints&, const Breakpoints&) = default;

private:
    std::array<float, 7> widths_{480, 576, 768, 992, 1200, 1600, 1920};
};

template <typename T> class Responsive final {
public:
    Responsive() = default;

    Responsive(T fallback) : fallback_(std::move(fallback)) {}

    Responsive& at(Breakpoint breakpoint, T value) {
        values_.at(static_cast<std::size_t>(breakpoint)) = std::move(value);
        return *this;
    }

    [[nodiscard]] const T& fallback() const noexcept {
        return fallback_;
    }

    [[nodiscard]] const std::array<std::optional<T>, 7>& overrides() const noexcept {
        return values_;
    }

    [[nodiscard]] const T& resolve(float window_width, const Breakpoints& breakpoints = {}) const {
        const T* result = &fallback_;
        visit(window_width, breakpoints, [&](const T& value) { result = &value; });
        return *result;
    }

    template <typename Apply> void visit(float window_width, const Breakpoints& breakpoints, Apply&& apply) const {
        for (std::size_t index = 0; index < values_.size(); ++index) {
            if (values_[index] && breakpoints.matches(static_cast<Breakpoint>(index), window_width)) {
                apply(*values_[index]);
            }
        }
    }

    friend bool operator==(const Responsive&, const Responsive&) = default;

private:
    T fallback_{};
    std::array<std::optional<T>, 7> values_;
};

} // namespace ryn
