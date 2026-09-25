#include "renderer/sdl/texture_upload_batch_layout.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_multiple_pages_and_padded_rows() {
    ryn::detail::TextureUploadBatchLayout layout;
    layout.reset(2048);
    std::array<std::byte, 2048> transfer{};
    std::array<std::byte, 64> first_page{};
    std::array<std::byte, 64> second_page{};
    first_page.fill(std::byte{0x55});
    second_page.fill(std::byte{0x66});
    std::array<std::byte, 512> first{};
    std::array<std::byte, 512> second{};
    first[0] = std::byte{1};
    first[1] = std::byte{2};
    first[256] = std::byte{3};
    first[257] = std::byte{4};
    second[0] = std::byte{5};
    second[1] = std::byte{6};
    second[256] = std::byte{7};
    second[257] = std::byte{8};
    const auto first_offset = layout.append(
        &first_page, {2, 3, 2, 2}, 256, 2, first.size());
    std::memcpy(transfer.data() + first_offset, first.data(), first.size());
    const auto second_offset = layout.append(
        &second_page, {4, 1, 2, 2}, 256, 2, second.size());
    std::memcpy(transfer.data() + second_offset, second.data(), second.size());
    require(first_offset == 0 && second_offset == 512
                && layout.regions().size() == 2 && layout.used_bytes() == 1024,
            "texture batch lost source offset alignment or region order");
    first.fill(std::byte{});
    second.fill(std::byte{});
    for (const auto region : layout.regions()) {
        auto* page = static_cast<std::array<std::byte, 64>*>(region.target);
        for (std::uint32_t row = 0; row < region.rectangle.height; ++row) {
            std::memcpy(
                page->data() + (region.rectangle.y + row) * 8 + region.rectangle.x,
                transfer.data() + region.source_offset + row * region.pixels_per_row,
                region.rectangle.width);
        }
    }
    require(first_page[3 * 8 + 2] == std::byte{1}
                && first_page[3 * 8 + 3] == std::byte{2}
                && first_page[4 * 8 + 2] == std::byte{3}
                && first_page[4 * 8 + 3] == std::byte{4}
                && first_page[3 * 8 + 1] == std::byte{0x55}
                && second_page[1 * 8 + 4] == std::byte{5}
                && second_page[2 * 8 + 5] == std::byte{8}
                && second_page[1 * 8 + 3] == std::byte{0x66},
            "packed texture replay changed row pitch, target page, or untouched texels");
}

void test_alignment_chunk_boundary_and_oversize() {
    ryn::detail::TextureUploadBatchLayout layout;
    int page{};
    layout.reset(1024);
    require(layout.append(&page, {0, 0, 1, 1}, 256, 1, 257) == 0,
            "first texture source offset is wrong");
    require(layout.append(&page, {1, 0, 1, 1}, 256, 1, 257) == 512,
            "second texture source offset is not 512-byte aligned");
    require(!layout.can_fit(1), "texture chunk accepted a region past capacity");
    layout.reset(1536);
    require(layout.regions().empty() && layout.can_fit(1536)
                && layout.append(&page, {0, 0, 1, 1}, 256, 1, 1536) == 0
                && layout.used_bytes() == 1536 && !layout.can_fit(1),
            "oversize texture region was truncated or reused");
    layout.reset(std::numeric_limits<std::uint32_t>::max());
    require(!layout.can_fit(std::numeric_limits<std::size_t>::max()),
            "texture chunk accepted a size outside SDL's 32-bit range");
}

} // namespace

int main() {
    try {
        test_multiple_pages_and_padded_rows();
        test_alignment_chunk_boundary_and_oversize();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
