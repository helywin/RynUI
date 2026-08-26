#include "graphics/glyph_atlas.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ryn::graphics {

struct GlyphAtlas::Page {
    std::vector<std::uint8_t> coverage;
    std::uint32_t cursor_x{};
    std::uint32_t cursor_y{};
    std::uint32_t shelf_height{};
};

namespace {

struct ShelfPlacement {
    GlyphAtlasRect rectangle{};
    std::uint32_t next_cursor_x{};
    std::uint32_t next_cursor_y{};
    std::uint32_t next_shelf_height{};
};

[[nodiscard]] bool valid_bitmap(const font::GlyphBitmap& glyph) noexcept {
    if (glyph.width == 0 || glyph.height == 0) {
        return glyph.width == 0 && glyph.height == 0 && glyph.coverage.empty();
    }
    const std::uint64_t expected = static_cast<std::uint64_t>(glyph.width) * glyph.height;
    return glyph.row_stride == glyph.width
        && expected == glyph.coverage.size();
}

[[nodiscard]] std::optional<ShelfPlacement> try_place(
    std::uint32_t page_width,
    std::uint32_t page_height,
    std::uint32_t cursor_x,
    std::uint32_t cursor_y,
    std::uint32_t shelf_height,
    std::uint32_t allocation_width,
    std::uint32_t allocation_height) noexcept {
    std::uint32_t x = cursor_x;
    std::uint32_t y = cursor_y;
    std::uint32_t height = shelf_height;
    if (static_cast<std::uint64_t>(x) + allocation_width > page_width) {
        x = 0;
        y += height;
        height = 0;
    }
    if (static_cast<std::uint64_t>(y) + allocation_height > page_height) {
        return std::nullopt;
    }
    return ShelfPlacement{
        {x, y, allocation_width, allocation_height},
        x + allocation_width,
        y,
        std::max(height, allocation_height),
    };
}

} // namespace

GlyphAtlas::GlyphAtlas(GlyphAtlasConfig config) : config_(config) {
    if (config_.page_width < glyph_atlas_padding * 2U + 1U
            || config_.page_height < glyph_atlas_padding * 2U + 1U
            || config_.max_pages == 0) {
        throw std::invalid_argument("GlyphAtlas configuration cannot hold a padded glyph");
    }
    const std::uint64_t page_bytes =
        static_cast<std::uint64_t>(config_.page_width) * config_.page_height;
    if (page_bytes > std::numeric_limits<std::size_t>::max()) {
        throw std::length_error("GlyphAtlas page storage exceeds size_t");
    }
}

GlyphAtlas::~GlyphAtlas() = default;

GlyphAtlasResult GlyphAtlas::ensure(
    font::FontRuntime& fonts,
    font::FontIdentity font_identity,
    std::uint32_t glyph_id,
    font::GlyphRasterMode mode) {
    const auto metrics = fonts.metrics(font_identity);
    if (!metrics) {
        return {nullptr, false, {
            GlyphAtlasErrorKind::font_failure, 0, 0,
            static_cast<std::uint32_t>(pages_.size()), metrics.error}};
    }
    const GlyphAtlasKey key{
        font_identity,
        glyph_id,
        metrics.metrics.pixel_size,
        mode,
    };
    if (const GlyphAtlasEntry* existing = find(key)) {
        return {existing, true, {}};
    }

    const auto rasterized = fonts.rasterize(font_identity, glyph_id, mode);
    if (!rasterized) {
        return {nullptr, false, {
            GlyphAtlasErrorKind::font_failure, 0, 0,
            static_cast<std::uint32_t>(pages_.size()), rasterized.error}};
    }
    return allocate(key, *rasterized.glyph);
}

GlyphAtlasResult GlyphAtlas::insert(
    GlyphAtlasKey key,
    const font::GlyphBitmap& glyph) {
    if (const GlyphAtlasEntry* existing = find(key)) {
        return {existing, true, {}};
    }
    return allocate(key, glyph);
}

const GlyphAtlasConfig& GlyphAtlas::config() const noexcept {
    return config_;
}

std::size_t GlyphAtlas::page_count() const noexcept {
    return pages_.size();
}

std::size_t GlyphAtlas::entry_count() const noexcept {
    return entries_.size();
}

const std::deque<GlyphAtlasEntry>& GlyphAtlas::entries() const noexcept {
    return entries_;
}

std::span<const std::uint8_t> GlyphAtlas::page_bytes(std::uint32_t page) const {
    return pages_.at(page).coverage;
}

std::span<const GlyphAtlasUploadPlan> GlyphAtlas::dirty_regions() const noexcept {
    return dirty_regions_;
}

void GlyphAtlas::clear_dirty_regions() noexcept {
    dirty_regions_.clear();
}

const GlyphAtlasEntry* GlyphAtlas::find(GlyphAtlasKey key) const noexcept {
    const auto found = std::ranges::find(entries_, key, &GlyphAtlasEntry::key);
    return found == entries_.end() ? nullptr : &*found;
}

GlyphAtlasResult GlyphAtlas::allocate(
    GlyphAtlasKey key,
    const font::GlyphBitmap& glyph) {
    if (!valid_bitmap(glyph)) {
        return {nullptr, false, {
            GlyphAtlasErrorKind::invalid_bitmap,
            glyph.width,
            glyph.height,
            static_cast<std::uint32_t>(pages_.size()),
            {}}};
    }
    if (glyph.width == 0) {
        entries_.push_back({
            key,
            invalid_glyph_atlas_page,
            {},
            {},
            {},
            glyph.bearing_x,
            glyph.bearing_y,
            glyph.advance_x,
            true,
        });
        return {&entries_.back(), false, {}};
    }

    const std::uint64_t allocation_width_64 =
        static_cast<std::uint64_t>(glyph.width) + glyph_atlas_padding * 2U;
    const std::uint64_t allocation_height_64 =
        static_cast<std::uint64_t>(glyph.height) + glyph_atlas_padding * 2U;
    if (allocation_width_64 > config_.page_width
            || allocation_height_64 > config_.page_height) {
        return {nullptr, false, {
            GlyphAtlasErrorKind::glyph_too_large,
            glyph.width,
            glyph.height,
            static_cast<std::uint32_t>(pages_.size()),
            {}}};
    }
    const auto allocation_width = static_cast<std::uint32_t>(allocation_width_64);
    const auto allocation_height = static_cast<std::uint32_t>(allocation_height_64);

    std::uint32_t selected_page = invalid_glyph_atlas_page;
    std::optional<ShelfPlacement> placement;
    for (std::size_t page_index = 0; page_index < pages_.size(); ++page_index) {
        Page& page = pages_[page_index];
        placement = try_place(
            config_.page_width,
            config_.page_height,
            page.cursor_x,
            page.cursor_y,
            page.shelf_height,
            allocation_width,
            allocation_height);
        if (placement) {
            selected_page = static_cast<std::uint32_t>(page_index);
            break;
        }
    }
    if (!placement) {
        if (pages_.size() >= config_.max_pages) {
            return {nullptr, false, {
                GlyphAtlasErrorKind::capacity_exhausted,
                glyph.width,
                glyph.height,
                static_cast<std::uint32_t>(pages_.size()),
                {}}};
        }
        Page page;
        page.coverage.assign(
            static_cast<std::size_t>(config_.page_width) * config_.page_height,
            0);
        pages_.push_back(std::move(page));
        selected_page = static_cast<std::uint32_t>(pages_.size() - 1);
        placement = try_place(
            config_.page_width,
            config_.page_height,
            0,
            0,
            0,
            allocation_width,
            allocation_height);
    }

    Page& page = pages_[selected_page];
    page.cursor_x = placement->next_cursor_x;
    page.cursor_y = placement->next_cursor_y;
    page.shelf_height = placement->next_shelf_height;
    const GlyphAtlasRect coverage_rect{
        placement->rectangle.x + glyph_atlas_padding,
        placement->rectangle.y + glyph_atlas_padding,
        glyph.width,
        glyph.height,
    };
    for (std::uint32_t row = 0; row < glyph.height; ++row) {
        const std::size_t source_offset = static_cast<std::size_t>(row) * glyph.row_stride;
        const std::size_t destination_offset =
            static_cast<std::size_t>(coverage_rect.y + row) * config_.page_width
            + coverage_rect.x;
        std::copy_n(
            glyph.coverage.begin() + source_offset,
            glyph.width,
            page.coverage.begin() + destination_offset);
    }

    entries_.push_back({
        key,
        selected_page,
        placement->rectangle,
        coverage_rect,
        {
            static_cast<float>(coverage_rect.x) / config_.page_width,
            static_cast<float>(coverage_rect.y) / config_.page_height,
            static_cast<float>(coverage_rect.x + coverage_rect.width) / config_.page_width,
            static_cast<float>(coverage_rect.y + coverage_rect.height) / config_.page_height,
        },
        glyph.bearing_x,
        glyph.bearing_y,
        glyph.advance_x,
        false,
    });
    dirty_regions_.push_back({
        selected_page,
        placement->rectangle,
        static_cast<std::size_t>(placement->rectangle.y) * config_.page_width
            + placement->rectangle.x,
        config_.page_width,
        static_cast<std::size_t>(placement->rectangle.width)
            * placement->rectangle.height,
    });
    return {&entries_.back(), false, {}};
}

bool overlaps(GlyphAtlasRect left, GlyphAtlasRect right) noexcept {
    const std::uint64_t left_right = static_cast<std::uint64_t>(left.x) + left.width;
    const std::uint64_t right_right = static_cast<std::uint64_t>(right.x) + right.width;
    const std::uint64_t left_bottom = static_cast<std::uint64_t>(left.y) + left.height;
    const std::uint64_t right_bottom = static_cast<std::uint64_t>(right.y) + right.height;
    return left.x < right_right && right.x < left_right
        && left.y < right_bottom && right.y < left_bottom;
}

} // namespace ryn::graphics
