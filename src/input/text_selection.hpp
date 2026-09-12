#pragma once

#include <algorithm>
#include <cstddef>

namespace ryn::input {
struct TextSelection final {
    std::size_t anchor{};
    std::size_t caret{};
    [[nodiscard]] std::size_t begin() const noexcept { return std::min(anchor, caret); }
    [[nodiscard]] std::size_t end() const noexcept { return std::max(anchor, caret); }
    [[nodiscard]] bool empty() const noexcept { return anchor == caret; }
    friend bool operator==(TextSelection, TextSelection) = default;
};
}
