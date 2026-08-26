#pragma once

#include "font/font_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <span>
#include <vector>

namespace ryn::graphics {

inline constexpr std::uint32_t glyph_atlas_default_extent = 1024;
inline constexpr std::uint32_t glyph_atlas_default_max_pages = 8;
inline constexpr std::uint32_t glyph_atlas_padding = 1;
inline constexpr std::uint32_t invalid_glyph_atlas_page =
    std::numeric_limits<std::uint32_t>::max();

enum class GlyphAtlasFormat : std::uint8_t {
    r8_unorm,
};

inline constexpr GlyphAtlasFormat glyph_atlas_format = GlyphAtlasFormat::r8_unorm;

struct GlyphAtlasConfig {
    std::uint32_t page_width{glyph_atlas_default_extent};
    std::uint32_t page_height{glyph_atlas_default_extent};
    std::uint32_t max_pages{glyph_atlas_default_max_pages};

    friend bool operator==(GlyphAtlasConfig, GlyphAtlasConfig) = default;
};

struct GlyphAtlasKey {
    font::FontIdentity font{};
    std::uint32_t glyph_id{};
    std::uint32_t pixel_size{};
    font::GlyphRasterMode mode{font::GlyphRasterMode::grayscale};

    friend bool operator==(GlyphAtlasKey, GlyphAtlasKey) = default;
};

struct GlyphAtlasRect {
    std::uint32_t x{};
    std::uint32_t y{};
    std::uint32_t width{};
    std::uint32_t height{};

    friend bool operator==(GlyphAtlasRect, GlyphAtlasRect) = default;
};

struct GlyphAtlasUvRect {
    float left{};
    float top{};
    float right{};
    float bottom{};

    friend bool operator==(GlyphAtlasUvRect, GlyphAtlasUvRect) = default;
};

struct GlyphAtlasEntry {
    GlyphAtlasKey key{};
    std::uint32_t page{invalid_glyph_atlas_page};
    GlyphAtlasRect padded_rect{};
    GlyphAtlasRect coverage_rect{};
    GlyphAtlasUvRect uv{};
    int bearing_x{};
    int bearing_y{};
    float advance_x{};
    bool empty{};

    friend bool operator==(const GlyphAtlasEntry&, const GlyphAtlasEntry&) = default;
};

struct GlyphAtlasUploadPlan {
    std::uint32_t page{};
    GlyphAtlasRect rectangle{};
    std::size_t source_offset{};
    std::uint32_t source_row_pitch{};
    std::size_t uploaded_bytes{};

    friend bool operator==(const GlyphAtlasUploadPlan&, const GlyphAtlasUploadPlan&) = default;
};

enum class GlyphAtlasErrorKind : std::uint8_t {
    none,
    invalid_config,
    invalid_bitmap,
    glyph_too_large,
    capacity_exhausted,
    font_failure,
};

struct GlyphAtlasError {
    GlyphAtlasErrorKind kind{GlyphAtlasErrorKind::none};
    std::uint32_t requested_width{};
    std::uint32_t requested_height{};
    std::uint32_t page_count{};
    font::FontError font_error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return kind != GlyphAtlasErrorKind::none;
    }
};

struct GlyphAtlasResult {
    const GlyphAtlasEntry* entry{};
    bool cache_hit{};
    GlyphAtlasError error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return entry != nullptr && !error;
    }
};

class GlyphAtlas final {
public:
    explicit GlyphAtlas(GlyphAtlasConfig config = {});
    GlyphAtlas(const GlyphAtlas&) = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;
    GlyphAtlas(GlyphAtlas&&) = delete;
    GlyphAtlas& operator=(GlyphAtlas&&) = delete;
    ~GlyphAtlas();

    [[nodiscard]] GlyphAtlasResult ensure(
        font::FontRuntime& fonts,
        font::FontIdentity font,
        std::uint32_t glyph_id,
        font::GlyphRasterMode mode = font::GlyphRasterMode::grayscale);

    [[nodiscard]] GlyphAtlasResult insert(
        GlyphAtlasKey key,
        const font::GlyphBitmap& glyph);

    [[nodiscard]] const GlyphAtlasConfig& config() const noexcept;
    [[nodiscard]] std::size_t page_count() const noexcept;
    [[nodiscard]] std::size_t entry_count() const noexcept;
    [[nodiscard]] const std::deque<GlyphAtlasEntry>& entries() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> page_bytes(std::uint32_t page) const;
    [[nodiscard]] std::span<const GlyphAtlasUploadPlan> dirty_regions() const noexcept;
    void clear_dirty_regions() noexcept;

private:
    struct Page;

    [[nodiscard]] const GlyphAtlasEntry* find(GlyphAtlasKey key) const noexcept;
    [[nodiscard]] GlyphAtlasResult allocate(
        GlyphAtlasKey key,
        const font::GlyphBitmap& glyph);

    GlyphAtlasConfig config_;
    std::vector<Page> pages_;
    std::deque<GlyphAtlasEntry> entries_;
    std::vector<GlyphAtlasUploadPlan> dirty_regions_;
};

[[nodiscard]] bool overlaps(GlyphAtlasRect left, GlyphAtlasRect right) noexcept;

} // namespace ryn::graphics
