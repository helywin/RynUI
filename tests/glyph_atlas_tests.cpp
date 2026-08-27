#include "graphics/glyph_atlas.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

using ryn::font::FontIdentity;
using ryn::font::FontRuntime;
using ryn::font::GlyphBitmap;
using ryn::font::GlyphRasterMode;
using ryn::graphics::GlyphAtlas;
using ryn::graphics::GlyphAtlasErrorKind;
using ryn::graphics::GlyphAtlasKey;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

GlyphBitmap bitmap(std::uint32_t width, std::uint32_t height, std::uint8_t seed) {
    GlyphBitmap result;
    result.width = width;
    result.height = height;
    result.row_stride = width;
    result.bearing_x = 1;
    result.bearing_y = static_cast<int>(height);
    result.advance_x = static_cast<float>(width + 1U);
    result.coverage.resize(static_cast<std::size_t>(width) * height);
    for (std::size_t index = 0; index < result.coverage.size(); ++index) {
        result.coverage[index] = static_cast<std::uint8_t>(seed + index);
    }
    return result;
}

GlyphAtlasKey key(std::uint32_t glyph_id) {
    return {
        FontIdentity{0, 1},
        glyph_id,
        14,
        GlyphRasterMode::grayscale,
    };
}

void test_shelf_allocation_padding_and_stability() {
    GlyphAtlas atlas{{12, 12, 2}};
    require(ryn::graphics::glyph_atlas_format
                == ryn::graphics::GlyphAtlasFormat::r8_unorm,
            "Glyph atlas format is not the R8_UNORM coverage contract");
    const auto first = atlas.insert(key(1), bitmap(3, 3, 10));
    const auto second = atlas.insert(key(2), bitmap(3, 4, 40));
    const auto third = atlas.insert(key(3), bitmap(4, 3, 80));
    require(first && second && third, "valid glyph atlas allocation failed");
    require(atlas.page_count() == 1 && atlas.entry_count() == 3,
            "shelf allocator used an unexpected page count");
    require(!ryn::graphics::overlaps(first.entry->padded_rect, second.entry->padded_rect)
                && !ryn::graphics::overlaps(first.entry->padded_rect, third.entry->padded_rect)
                && !ryn::graphics::overlaps(second.entry->padded_rect, third.entry->padded_rect),
            "padded glyph atlas allocations overlap");

    const auto page = atlas.page_bytes(0);
    for (const auto* entry : {first.entry, second.entry, third.entry}) {
        const auto& padded = entry->padded_rect;
        const auto& coverage = entry->coverage_rect;
        require(coverage.x == padded.x + 1 && coverage.y == padded.y + 1
                    && coverage.width + 2 == padded.width
                    && coverage.height + 2 == padded.height,
                "glyph atlas entry did not preserve one-pixel padding");
        require(entry->uv.left == static_cast<float>(padded.x) / 12.0F
                    && entry->uv.top == static_cast<float>(padded.y) / 12.0F
                    && entry->uv.right
                        == static_cast<float>(padded.x + padded.width) / 12.0F
                    && entry->uv.bottom
                        == static_cast<float>(padded.y + padded.height) / 12.0F,
                "glyph sampling UV did not include the transparent guard pixels");
        for (std::uint32_t x = padded.x; x < padded.x + padded.width; ++x) {
            require(page[static_cast<std::size_t>(padded.y) * 12 + x] == 0
                        && page[static_cast<std::size_t>(padded.y + padded.height - 1) * 12 + x] == 0,
                    "horizontal glyph padding is not clear");
        }
        for (std::uint32_t y = padded.y; y < padded.y + padded.height; ++y) {
            require(page[static_cast<std::size_t>(y) * 12 + padded.x] == 0
                        && page[static_cast<std::size_t>(y) * 12 + padded.x + padded.width - 1] == 0,
                    "vertical glyph padding is not clear");
        }
    }

    const auto first_copy = *first.entry;
    const auto repeated = atlas.insert(key(1), bitmap(3, 3, 99));
    require(repeated && repeated.cache_hit && *repeated.entry == first_copy,
            "repeated atlas key did not preserve its stable entry");
    require(atlas.entry_count() == 3 && atlas.dirty_regions().size() == 3,
            "atlas cache hit allocated or dirtied storage");
    for (const auto& dirty : atlas.dirty_regions()) {
        require(dirty.source_row_pitch == 12
                    && dirty.uploaded_bytes
                        == static_cast<std::size_t>(dirty.rectangle.width)
                            * dirty.rectangle.height,
                "atlas upload plan lost its row pitch or byte count");
    }
}

void test_page_boundary_capacity_and_empty_glyph() {
    GlyphAtlas atlas{{6, 6, 2}};
    const auto first = atlas.insert(key(10), bitmap(4, 4, 1));
    const auto second = atlas.insert(key(11), bitmap(4, 4, 2));
    require(first && second && first.entry->page == 0 && second.entry->page == 1,
            "boundary-sized glyph did not paginate deterministically");

    const auto exhausted = atlas.insert(key(12), bitmap(1, 1, 3));
    require(!exhausted
                && exhausted.error.kind == GlyphAtlasErrorKind::capacity_exhausted
                && exhausted.error.page_count == 2,
            "full atlas did not report explicit capacity exhaustion");

    GlyphAtlas too_small{{6, 6, 1}};
    const auto oversized = too_small.insert(key(20), bitmap(5, 5, 1));
    require(!oversized && oversized.error.kind == GlyphAtlasErrorKind::glyph_too_large,
            "oversized padded glyph was accepted");

    GlyphBitmap empty;
    empty.advance_x = 4.0F;
    const auto blank = too_small.insert(key(21), empty);
    require(blank && blank.entry->empty
                && blank.entry->page == ryn::graphics::invalid_glyph_atlas_page
                && too_small.page_count() == 0
                && too_small.dirty_regions().empty(),
            "empty glyph allocated atlas storage");
}

void test_real_font_cache_and_dirty_plan() {
    auto created = FontRuntime::create();
    require(static_cast<bool>(created), "Font Runtime initialization failed");
    auto fonts = std::move(created.runtime);
    const auto loaded = fonts->load_font_file(
        RYNUI_VALIDATION_LATIN_FONT,
        0,
        ryn::font::FontRasterConfig{14, 1.5F});
    require(static_cast<bool>(loaded), "atlas validation font failed to load");
    const auto a = fonts->glyph_index(loaded.font, U'A');
    const auto space = fonts->glyph_index(loaded.font, U' ');
    require(a && space, "atlas validation glyph lookup failed");

    GlyphAtlas atlas;
    const auto first = atlas.ensure(*fonts, loaded.font, a.glyph.glyph_id);
    require(first && !first.cache_hit && !first.entry->empty
                && first.entry->key.pixel_size == 21
                && first.entry->raster_scale == 1.5F
                && atlas.page_count() == 1 && atlas.dirty_regions().size() == 1,
            "first real glyph did not allocate one dirty atlas region");
    const auto dirty = atlas.dirty_regions().front();
    require(dirty.rectangle == first.entry->padded_rect
                && dirty.source_row_pitch == ryn::graphics::glyph_atlas_default_extent,
            "real glyph upload plan does not match its atlas entry");

    atlas.clear_dirty_regions();
    const auto repeated = atlas.ensure(*fonts, loaded.font, a.glyph.glyph_id);
    require(repeated && repeated.cache_hit && repeated.entry == first.entry
                && atlas.dirty_regions().empty()
                && fonts->counters().rasterizations == 1,
            "repeated real glyph bypassed the stable atlas/font cache");

    const auto blank = atlas.ensure(*fonts, loaded.font, space.glyph.glyph_id);
    require(blank && blank.entry->empty && atlas.dirty_regions().empty(),
            "space glyph produced texture dirty state");
}

} // namespace

int main() {
    try {
        test_shelf_allocation_padding_and_stability();
        test_page_boundary_capacity_and_empty_glyph();
        test_real_font_cache_and_dirty_plan();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
