#include "renderer/common/glyph_texture_upload.hpp"
#include "renderer/sdl/glyph_texture_upload_layout.hpp"
#include "renderer/sdl/texture_upload_batch_layout.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <vector>

namespace {
using namespace ryn::detail;

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class F> void rejects(F action) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    check(failed, "invalid texture upload/layout was accepted");
}

void literal_source_and_sdl_layout() {
    // Prefix, three pixels, a stride gap, and a final row without trailing padding.
    const std::array source{std::byte{99}, std::byte{98}, std::byte{1}, std::byte{2},
        std::byte{3}, std::byte{97}, std::byte{96}, std::byte{4}, std::byte{5}, std::byte{6}};
    const GlyphTextureUpload upload{3, {2, 1, 3, 2}, 2, 5, source};
    validate_glyph_texture_upload(upload, 5, 3);
    const auto layout = sdl_glyph_texture_layout(upload);
    check(layout.pixels_per_row == 256 && layout.rows_per_layer == 2 && layout.byte_count == 512,
          "SDL transfer layout differs from literal 256-byte rows");
    std::array<std::byte, 520> packed;
    packed.fill(std::byte{88});
    pack_sdl_glyph_texture_rows(upload, packed);
    check(packed[0] == std::byte{1} && packed[2] == std::byte{3}
        && packed[256] == std::byte{4} && packed[258] == std::byte{6}, "source offset/stride lost pixels");
    for (std::size_t row : {0U, 256U})
        check(std::all_of(packed.begin() + row + 3, packed.begin() + row + 256,
                         [](auto value) { return value == std::byte{}; }), "SDL padding is not zero");
    check(packed[512] == std::byte{88} && packed.back() == std::byte{88}, "packing wrote beyond transfer");
    TextureUploadBatchLayout batch;
    batch.reset(1024);
    int texture{};
    check(batch.append(&texture, upload.rectangle, layout.pixels_per_row, layout.rows_per_layer,
                       layout.byte_count) == 0, "first region offset differs");
    check(batch.append(&texture, upload.rectangle, layout.pixels_per_row, layout.rows_per_layer,
                       layout.byte_count) == 512 && !batch.can_fit(1), "batch alignment/chunk boundary differs");
    rejects([&] { batch.append(&texture, upload.rectangle, 256, 2, layout.byte_count); });
    batch.reset(2048);
    check(batch.can_fit(2048) && batch.append(&texture, {0, 0, 3, 8}, 256, 8, 2048) == 0,
          "oversize chunk dropped bytes");
}

void invalid_ranges_do_not_write() {
    std::array<std::byte, 16> source{};
    const GlyphTextureUpload valid{0, {1, 1, 3, 2}, 2, 5, source};
    std::array<std::byte, 512> target;
    target.fill(std::byte{77});
    for (int mode = 0; mode < 8; ++mode) {
        auto upload = valid;
        switch (mode) {
        case 0: upload.rectangle.width = 0; break;
        case 1: upload.rectangle.height = 0; break;
        case 2: upload.source_row_pitch = 2; break;
        case 3: upload.source_offset = std::numeric_limits<std::size_t>::max(); break;
        case 4: upload.bytes = std::span(source).first(9); break;
        case 5: upload.rectangle.x = std::numeric_limits<std::uint32_t>::max(); break;
        case 6: upload.rectangle.y = std::numeric_limits<std::uint32_t>::max(); break;
        case 7: upload.source_row_pitch = std::numeric_limits<std::uint32_t>::max(); break;
        }
        rejects([&] { validate_glyph_texture_upload(upload, 4, 3); });
        rejects([&] { pack_sdl_glyph_texture_rows(upload, target); });
        check(std::all_of(target.begin(), target.end(), [](auto value) { return value == std::byte{77}; }),
              "invalid source mutated transfer bytes");
    }
    rejects([&] { pack_sdl_glyph_texture_rows(valid, std::span(target).first(511)); });
    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    rejects([&] { static_cast<void>(sdl_glyph_texture_layout(maximum, 1)); });
    rejects([&] { static_cast<void>(sdl_glyph_texture_layout(1, maximum)); });
    check(sdl_glyph_texture_layout(1, maximum / 256).byte_count == maximum / 256 * 256,
          "largest representable aligned SDL transfer was rejected");
    validate_glyph_texture_upload({0, {0, 0, 1, 1}, 15, 1, source}, 1, 1);
}
} // namespace

int main() {
    try { literal_source_and_sdl_layout(); invalid_ranges_do_not_write(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
