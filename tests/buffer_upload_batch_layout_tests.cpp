#include "renderer/sdl/buffer_upload_batch_layout.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_multiple_targets_and_source_snapshot() {
    ryn::detail::BufferUploadBatchLayout layout;
    layout.reset(64);
    std::array<std::byte, 64> transfer{};
    std::array<std::byte, 24> target_a{};
    std::array<std::byte, 24> target_b{};
    target_a.fill(std::byte{0x55});
    target_b.fill(std::byte{0x66});
    std::array<std::byte, 3> first{
        std::byte{1}, std::byte{2}, std::byte{3},
    };
    std::array<std::byte, 6> second{
        std::byte{4}, std::byte{5}, std::byte{6},
        std::byte{7}, std::byte{8}, std::byte{9},
    };
    const auto first_offset = layout.append(&target_a, 3, first.size());
    std::memcpy(transfer.data() + first_offset, first.data(), first.size());
    const auto second_offset = layout.append(&target_b, 1, second.size());
    std::memcpy(transfer.data() + second_offset, second.data(), second.size());
    const auto third_offset = layout.append(&target_a, 8, second.size());
    std::memcpy(transfer.data() + third_offset, second.data(), second.size());
    require(first_offset == 0 && second_offset == 16 && third_offset == 32
                && layout.regions().size() == 3 && layout.used_bytes() == 38,
            "batch layout lost 16-byte source alignment or region order");

    first.fill(std::byte{0});
    second.fill(std::byte{0});
    for (const auto region : layout.regions()) {
        auto* target = static_cast<std::array<std::byte, 24>*>(region.target);
        std::memcpy(target->data() + region.target_offset,
            transfer.data() + region.source_offset, region.byte_count);
    }
    require(target_a[2] == std::byte{0x55} && target_a[3] == std::byte{1}
                && target_a[5] == std::byte{3} && target_a[8] == std::byte{4}
                && target_a[13] == std::byte{9} && target_a[14] == std::byte{0x55}
                && target_b[0] == std::byte{0x66} && target_b[1] == std::byte{4}
                && target_b[6] == std::byte{9} && target_b[7] == std::byte{0x66},
            "batch replay changed target bytes or read modified source data");
}

void test_chunk_boundary_and_oversize_region() {
    ryn::detail::BufferUploadBatchLayout layout;
    int target{};
    layout.reset(32);
    require(layout.can_fit(16), "first chunk region did not fit");
    require(layout.append(&target, 0, 16) == 0, "first source offset is wrong");
    require(layout.append(&target, 16, 16) == 16, "exact boundary is wrong");
    require(!layout.can_fit(1), "full chunk accepted another region");
    layout.reset(80);
    require(layout.regions().empty() && layout.can_fit(80)
                && layout.append(&target, 0, 80) == 0
                && layout.used_bytes() == 80 && !layout.can_fit(1),
            "oversize exclusive chunk was truncated or reused");
    layout.reset(std::numeric_limits<std::uint32_t>::max());
    require(!layout.can_fit(std::numeric_limits<std::size_t>::max()),
            "chunk accepted a size outside SDL's 32-bit range");
}

} // namespace

int main() {
    try {
        test_multiple_targets_and_source_snapshot();
        test_chunk_boundary_and_oversize_region();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
