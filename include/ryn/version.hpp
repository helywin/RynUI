#pragma once

#include <cstdint>

namespace ryn {

struct Version final {
    std::uint16_t major;
    std::uint16_t minor;
    std::uint16_t patch;

    friend constexpr bool operator==(const Version&, const Version&) = default;
};

[[nodiscard]] Version version() noexcept;

} // namespace ryn
