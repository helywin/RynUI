#pragma once

#include <cstdint>
#include <limits>

namespace ryn::input {

struct TextInputOwnerId final {
    static constexpr auto invalid_index = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t index{invalid_index};
    std::uint32_t generation{};
    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != invalid_index && generation != 0;
    }
    friend constexpr bool operator==(TextInputOwnerId, TextInputOwnerId) = default;
};

} // namespace ryn::input
