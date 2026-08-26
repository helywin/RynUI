#include "font/font_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <thread>
#include <unordered_map>
#include <utility>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace ryn::font {
namespace {

[[nodiscard]] FontError make_error(
    FontErrorStage stage,
    FontErrorKind kind,
    std::string diagnostic,
    FontIdentity font = {},
    char32_t codepoint = {}) {
    return FontError{stage, kind, font, codepoint, std::move(diagnostic)};
}

[[nodiscard]] bool is_unicode_scalar(char32_t value) noexcept {
    return value <= 0x10FFFF && !(value >= 0xD800 && value <= 0xDFFF);
}

[[nodiscard]] float fixed_26_6_to_pixels(FT_Pos value) noexcept {
    return static_cast<float>(value) / 64.0F;
}

struct GlyphCacheKey {
    FontIdentity font{};
    std::uint32_t glyph_id{};
    std::uint32_t pixel_size{};
    GlyphRasterMode mode{GlyphRasterMode::grayscale};

    friend bool operator==(const GlyphCacheKey&, const GlyphCacheKey&) = default;
};

struct GlyphCacheKeyHash {
    [[nodiscard]] std::size_t operator()(const GlyphCacheKey& key) const noexcept {
        std::size_t value = key.font.slot;
        value = value * 16777619U ^ key.font.generation;
        value = value * 16777619U ^ key.glyph_id;
        value = value * 16777619U ^ key.pixel_size;
        value = value * 16777619U ^ static_cast<std::size_t>(key.mode);
        return value;
    }
};

} // namespace

struct FontRuntime::Impl {
    struct FontRecord {
        explicit FontRecord(std::shared_ptr<FontRuntimeCounters> lifetime_counters)
            : counters(std::move(lifetime_counters)) {}

        FontRecord(const FontRecord&) = delete;
        FontRecord& operator=(const FontRecord&) = delete;

        FontRecord(FontRecord&& other) noexcept
            : bytes(std::move(other.bytes)),
              face(std::exchange(other.face, nullptr)),
              metrics(other.metrics),
              owns_bytes(std::exchange(other.owns_bytes, false)),
              counters(std::move(other.counters)) {}

        FontRecord& operator=(FontRecord&&) = delete;

        ~FontRecord() {
            release();
        }

        void release() noexcept {
            if (face != nullptr) {
                FT_Done_Face(face);
                face = nullptr;
                ++counters->faces_released;
            }
            if (owns_bytes) {
                bytes.clear();
                owns_bytes = false;
                ++counters->byte_resources_released;
            }
        }

        std::vector<std::byte> bytes;
        FT_Face face{};
        FontMetrics metrics{};
        bool owns_bytes{};
        std::shared_ptr<FontRuntimeCounters> counters;
    };

    struct FontSlot {
        std::uint32_t generation{1};
        std::optional<FontRecord> record;
    };

    FT_Library library{};
    std::thread::id owner_thread{std::this_thread::get_id()};
    std::shared_ptr<FontRuntimeCounters> counters;
    std::vector<FontSlot> fonts;
    std::unordered_map<GlyphCacheKey, GlyphBitmap, GlyphCacheKeyHash> glyph_cache;
    bool active{true};

    ~Impl() {
        shutdown_unchecked();
    }

    [[nodiscard]] bool is_owner_thread() const noexcept {
        return std::this_thread::get_id() == owner_thread;
    }

    [[nodiscard]] FontError owner_error(FontErrorStage stage) const {
        return make_error(
            stage,
            FontErrorKind::wrong_thread,
            "Font Runtime operation must run on its owner thread.");
    }

    [[nodiscard]] FontRecord* find(FontIdentity identity) noexcept {
        if (identity.generation == 0 || identity.slot >= fonts.size()) {
            return nullptr;
        }
        FontSlot& slot = fonts[identity.slot];
        if (slot.generation != identity.generation || !slot.record) {
            return nullptr;
        }
        return &*slot.record;
    }

    [[nodiscard]] const FontRecord* find(FontIdentity identity) const noexcept {
        return const_cast<Impl*>(this)->find(identity);
    }

    void release_record(FontRecord& record) noexcept {
        record.release();
    }

    void shutdown_unchecked() noexcept {
        if (!active) {
            return;
        }
        glyph_cache.clear();
        for (FontSlot& slot : fonts) {
            if (slot.record) {
                release_record(*slot.record);
                slot.record.reset();
            }
        }
        if (library != nullptr) {
            FT_Done_FreeType(library);
            library = nullptr;
            ++counters->libraries_released;
        }
        active = false;
    }
};

FontRuntime::FontRuntime(std::unique_ptr<Impl> implementation) noexcept
    : impl_(std::move(implementation)) {}

FontRuntime::~FontRuntime() {
    impl_->shutdown_unchecked();
}

FontRuntimeCreateResult FontRuntime::create(FontRuntimeOptions options) {
    auto counters = options.counters != nullptr
        ? std::move(options.counters)
        : std::make_shared<FontRuntimeCounters>();

    if (options.failure_point == FontFailurePoint::library_initialization) {
        return {
            nullptr,
            make_error(
                FontErrorStage::library_initialization,
                FontErrorKind::runtime_unavailable,
                "Injected FreeType library initialization failure."),
        };
    }

    auto implementation = std::make_unique<Impl>();
    implementation->counters = std::move(counters);
    const FT_Error result = FT_Init_FreeType(&implementation->library);
    if (result != 0) {
        return {
            nullptr,
            make_error(
                FontErrorStage::library_initialization,
                FontErrorKind::runtime_unavailable,
                "FreeType library initialization failed."),
        };
    }
    ++implementation->counters->libraries_acquired;
    return {std::unique_ptr<FontRuntime>(new FontRuntime(std::move(implementation))), {}};
}

FontLoadResult FontRuntime::load_font_file(
    const std::filesystem::path& path,
    long face_index,
    std::uint32_t pixel_size,
    FontFailurePoint failure_point) {
    if (!impl_->is_owner_thread()) {
        return {{}, impl_->owner_error(FontErrorStage::resource_read)};
    }
    if (!impl_->active) {
        return {
            {},
            make_error(
                FontErrorStage::library_initialization,
                FontErrorKind::runtime_unavailable,
                "Font Runtime has been shut down."),
        };
    }
    if (pixel_size == 0) {
        return {
            {},
            make_error(
                FontErrorStage::pixel_size_configuration,
                FontErrorKind::invalid_pixel_size,
                "Font pixel size must be positive."),
        };
    }

    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        return {
            {},
            make_error(
                FontErrorStage::resource_read,
                FontErrorKind::resource_unavailable,
                "Unable to open the explicit font resource."),
        };
    }
    const std::streamoff length = input.tellg();
    if (length <= 0
            || static_cast<std::uintmax_t>(length)
                > std::numeric_limits<std::size_t>::max()) {
        return {
            {},
            make_error(
                FontErrorStage::resource_read,
                FontErrorKind::invalid_font_data,
                "The explicit font resource is empty or too large."),
        };
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    input.seekg(0, std::ios::beg);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), length)) {
        return {
            {},
            make_error(
                FontErrorStage::resource_read,
                FontErrorKind::resource_unavailable,
                "Unable to read the explicit font resource."),
        };
    }
    return load_font_bytes(bytes, face_index, pixel_size, failure_point);
}

FontLoadResult FontRuntime::load_font_bytes(
    std::span<const std::byte> bytes,
    long face_index,
    std::uint32_t pixel_size,
    FontFailurePoint failure_point) {
    if (!impl_->is_owner_thread()) {
        return {{}, impl_->owner_error(FontErrorStage::face_creation)};
    }
    if (!impl_->active) {
        return {
            {},
            make_error(
                FontErrorStage::library_initialization,
                FontErrorKind::runtime_unavailable,
                "Font Runtime has been shut down."),
        };
    }
    if (pixel_size == 0) {
        return {
            {},
            make_error(
                FontErrorStage::pixel_size_configuration,
                FontErrorKind::invalid_pixel_size,
                "Font pixel size must be positive."),
        };
    }

    Impl::FontRecord record{impl_->counters};
    record.bytes.assign(bytes.begin(), bytes.end());
    record.owns_bytes = true;
    ++impl_->counters->byte_resources_acquired;

    const auto fail = [&](FontError error) {
        impl_->release_record(record);
        return FontLoadResult{{}, std::move(error)};
    };

    if (failure_point == FontFailurePoint::after_font_bytes) {
        return fail(make_error(
            FontErrorStage::face_creation,
            FontErrorKind::invalid_font_data,
            "Injected failure after font bytes were acquired."));
    }
    if (record.bytes.empty()
            || record.bytes.size() > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max())) {
        return fail(make_error(
            FontErrorStage::face_creation,
            FontErrorKind::invalid_font_data,
            "Font bytes are empty or exceed the FreeType input limit."));
    }

    const FT_Error face_result = FT_New_Memory_Face(
        impl_->library,
        reinterpret_cast<const FT_Byte*>(record.bytes.data()),
        static_cast<FT_Long>(record.bytes.size()),
        face_index,
        &record.face);
    if (face_result != 0) {
        return fail(make_error(
            FontErrorStage::face_creation,
            face_index == 0
                ? FontErrorKind::invalid_font_data
                : FontErrorKind::invalid_face_index,
            face_index == 0
                ? "FreeType rejected the font bytes."
                : "FreeType rejected the requested face index."));
    }
    ++impl_->counters->faces_acquired;

    if (failure_point == FontFailurePoint::after_face_creation
            || failure_point == FontFailurePoint::charmap_selection) {
        return fail(make_error(
            FontErrorStage::charmap_selection,
            FontErrorKind::no_unicode_charmap,
            "Injected Unicode charmap selection failure."));
    }
    if (FT_Select_Charmap(record.face, FT_ENCODING_UNICODE) != 0) {
        return fail(make_error(
            FontErrorStage::charmap_selection,
            FontErrorKind::no_unicode_charmap,
            "Font has no usable Unicode charmap."));
    }

    if (failure_point == FontFailurePoint::pixel_size_configuration) {
        return fail(make_error(
            FontErrorStage::pixel_size_configuration,
            FontErrorKind::invalid_pixel_size,
            "Injected pixel-size configuration failure."));
    }
    if (FT_Set_Pixel_Sizes(record.face, 0, pixel_size) != 0) {
        return fail(make_error(
            FontErrorStage::pixel_size_configuration,
            FontErrorKind::invalid_pixel_size,
            "FreeType rejected the requested pixel size."));
    }

    const FT_Size_Metrics& metrics = record.face->size->metrics;
    record.metrics.ascent = fixed_26_6_to_pixels(metrics.ascender);
    record.metrics.descent = fixed_26_6_to_pixels(metrics.descender);
    record.metrics.line_gap = std::max(
        0.0F,
        fixed_26_6_to_pixels(metrics.height - metrics.ascender + metrics.descender));
    record.metrics.pixel_size = pixel_size;

    std::size_t slot_index = 0;
    for (; slot_index < impl_->fonts.size(); ++slot_index) {
        if (!impl_->fonts[slot_index].record) {
            break;
        }
    }
    if (slot_index == impl_->fonts.size()) {
        impl_->fonts.emplace_back();
    }
    Impl::FontSlot& slot = impl_->fonts[slot_index];
    slot.record.emplace(std::move(record));
    return {
        FontIdentity{static_cast<std::uint32_t>(slot_index), slot.generation},
        {},
    };
}

FontMetricsResult FontRuntime::metrics(FontIdentity font) const {
    if (!impl_->is_owner_thread()) {
        return {{}, impl_->owner_error(FontErrorStage::coverage_query)};
    }
    const Impl::FontRecord* record = impl_->find(font);
    if (record == nullptr) {
        return {
            {},
            make_error(
                FontErrorStage::coverage_query,
                FontErrorKind::invalid_identity,
                "Font identity is stale or unknown.",
                font),
        };
    }
    return {record->metrics, {}};
}

GlyphLookupResult FontRuntime::glyph_index(
    FontIdentity font,
    char32_t codepoint) const {
    if (!impl_->is_owner_thread()) {
        return {{}, impl_->owner_error(FontErrorStage::coverage_query)};
    }
    if (!is_unicode_scalar(codepoint)) {
        return {
            {},
            make_error(
                FontErrorStage::coverage_query,
                FontErrorKind::invalid_codepoint,
                "Coverage queries require a Unicode scalar value.",
                font,
                codepoint),
        };
    }
    const Impl::FontRecord* record = impl_->find(font);
    if (record == nullptr) {
        return {
            {},
            make_error(
                FontErrorStage::coverage_query,
                FontErrorKind::invalid_identity,
                "Font identity is stale or unknown.",
                font,
                codepoint),
        };
    }
    ++impl_->counters->coverage_queries;
    const std::uint32_t glyph_id = FT_Get_Char_Index(record->face, codepoint);
    return {{font, glyph_id, codepoint, codepoint, false}, {}};
}

GlyphLookupResult FontRuntime::find_glyph(
    std::span<const FontIdentity> fallback_chain,
    char32_t codepoint,
    std::optional<char32_t> replacement) const {
    if (!impl_->is_owner_thread()) {
        return {{}, impl_->owner_error(FontErrorStage::coverage_query)};
    }
    if (!is_unicode_scalar(codepoint)) {
        return {
            {},
            make_error(
                FontErrorStage::coverage_query,
                FontErrorKind::invalid_codepoint,
                "Fallback lookup requires a Unicode scalar value.",
                {},
                codepoint),
        };
    }

    FontError search_error;
    const auto search = [&](char32_t candidate, bool used_replacement)
        -> std::optional<GlyphSelection> {
        for (const FontIdentity font : fallback_chain) {
            const GlyphLookupResult lookup = glyph_index(font, candidate);
            if (!lookup) {
                search_error = lookup.error;
                return std::nullopt;
            }
            if (lookup.glyph.glyph_id != 0) {
                GlyphSelection selection = lookup.glyph;
                selection.requested_codepoint = codepoint;
                selection.resolved_codepoint = candidate;
                selection.used_replacement = used_replacement;
                return selection;
            }
        }
        return std::nullopt;
    };

    if (const auto result = search(codepoint, false)) {
        return {*result, {}};
    }
    if (search_error) {
        return {{}, std::move(search_error)};
    }
    if (replacement && *replacement != codepoint && is_unicode_scalar(*replacement)) {
        if (const auto result = search(*replacement, true)) {
            return {*result, {}};
        }
        if (search_error) {
            return {{}, std::move(search_error)};
        }
    }
    return {
        {},
        make_error(
            FontErrorStage::coverage_query,
            FontErrorKind::missing_glyph,
            "No font in the declared fallback chain covers the codepoint.",
            {},
            codepoint),
    };
}

GlyphRasterResult FontRuntime::rasterize(
    FontIdentity font,
    std::uint32_t glyph_id,
    GlyphRasterMode mode,
    FontFailurePoint failure_point) {
    if (!impl_->is_owner_thread()) {
        return {nullptr, false, impl_->owner_error(FontErrorStage::rasterization)};
    }
    Impl::FontRecord* record = impl_->find(font);
    if (record == nullptr) {
        return {
            nullptr,
            false,
            make_error(
                FontErrorStage::rasterization,
                FontErrorKind::invalid_identity,
                "Font identity is stale or unknown.",
                font),
        };
    }

    const GlyphCacheKey key{font, glyph_id, record->metrics.pixel_size, mode};
    if (const auto existing = impl_->glyph_cache.find(key);
            existing != impl_->glyph_cache.end()) {
        ++impl_->counters->cache_hits;
        return {&existing->second, true, {}};
    }
    if (failure_point == FontFailurePoint::rasterization) {
        return {
            nullptr,
            false,
            make_error(
                FontErrorStage::rasterization,
                FontErrorKind::rasterization_failed,
                "Injected glyph rasterization failure.",
                font),
        };
    }
    if (mode != GlyphRasterMode::grayscale
            || FT_Load_Glyph(record->face, glyph_id, FT_LOAD_DEFAULT) != 0
            || FT_Render_Glyph(record->face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
        return {
            nullptr,
            false,
            make_error(
                FontErrorStage::rasterization,
                FontErrorKind::rasterization_failed,
                "FreeType could not render the requested glyph.",
                font),
        };
    }

    const FT_GlyphSlot slot = record->face->glyph;
    const FT_Bitmap& bitmap = slot->bitmap;
    if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY
            && bitmap.width != 0 && bitmap.rows != 0) {
        return {
            nullptr,
            false,
            make_error(
                FontErrorStage::rasterization,
                FontErrorKind::unsupported_bitmap,
                "FreeType returned a non-grayscale glyph bitmap.",
                font),
        };
    }

    GlyphBitmap result;
    result.width = bitmap.width;
    result.height = bitmap.rows;
    result.row_stride = bitmap.width;
    result.bearing_x = slot->bitmap_left;
    result.bearing_y = slot->bitmap_top;
    result.advance_x = fixed_26_6_to_pixels(slot->advance.x);
    result.visible_bounds = {
        result.bearing_x,
        -result.bearing_y,
        result.width,
        result.height,
    };

    if (bitmap.width != 0 && bitmap.rows != 0) {
        if (bitmap.pitch == std::numeric_limits<int>::min()) {
            return {
                nullptr,
                false,
                make_error(
                    FontErrorStage::rasterization,
                    FontErrorKind::unsupported_bitmap,
                    "FreeType returned an invalid glyph bitmap pitch.",
                    font),
            };
        }
        const std::size_t source_size = static_cast<std::size_t>(std::abs(bitmap.pitch))
            * bitmap.rows;
        const auto normalized = normalize_gray_coverage(
            std::span<const std::uint8_t>{bitmap.buffer, source_size},
            bitmap.width,
            bitmap.rows,
            bitmap.pitch);
        if (!normalized) {
            return {nullptr, false, normalized.error};
        }
        result.coverage = normalized.coverage;
    }

    ++impl_->counters->rasterizations;
    const auto [entry, inserted] = impl_->glyph_cache.emplace(key, std::move(result));
    if (!inserted) {
        return {
            nullptr,
            false,
            make_error(
                FontErrorStage::rasterization,
                FontErrorKind::rasterization_failed,
                "Glyph cache insertion failed.",
                font),
        };
    }
    return {&entry->second, false, {}};
}

FontActionResult FontRuntime::remove_font(FontIdentity font) {
    if (!impl_->is_owner_thread()) {
        return {impl_->owner_error(FontErrorStage::destruction)};
    }
    Impl::FontRecord* record = impl_->find(font);
    if (record == nullptr) {
        return {make_error(
            FontErrorStage::destruction,
            FontErrorKind::invalid_identity,
            "Font identity is stale or unknown.",
            font)};
    }

    std::erase_if(impl_->glyph_cache, [font](const auto& entry) {
        return entry.first.font == font;
    });
    Impl::FontSlot& slot = impl_->fonts[font.slot];
    impl_->release_record(*slot.record);
    slot.record.reset();
    ++slot.generation;
    if (slot.generation == 0) {
        ++slot.generation;
    }
    return {};
}

FontActionResult FontRuntime::shutdown() {
    if (!impl_->is_owner_thread()) {
        return {impl_->owner_error(FontErrorStage::destruction)};
    }
    impl_->shutdown_unchecked();
    return {};
}

const FontRuntimeCounters& FontRuntime::counters() const noexcept {
    return *impl_->counters;
}

std::size_t FontRuntime::glyph_cache_size() const noexcept {
    return impl_->glyph_cache.size();
}

CoverageNormalizationResult normalize_gray_coverage(
    std::span<const std::uint8_t> source,
    std::uint32_t width,
    std::uint32_t height,
    int pitch) {
    if (width == 0 || height == 0) {
        return {};
    }
    if (pitch == 0 || pitch == std::numeric_limits<int>::min()) {
        return {
            {},
            make_error(
                FontErrorStage::rasterization,
                FontErrorKind::unsupported_bitmap,
                "Glyph bitmap pitch is invalid."),
        };
    }
    const std::size_t absolute_pitch = static_cast<std::size_t>(std::abs(pitch));
    if (absolute_pitch < width
            || height > std::numeric_limits<std::size_t>::max() / absolute_pitch
            || source.size() < absolute_pitch * height
            || height > std::numeric_limits<std::size_t>::max() / width) {
        return {
            {},
            make_error(
                FontErrorStage::rasterization,
                FontErrorKind::unsupported_bitmap,
                "Glyph bitmap storage does not match its dimensions and pitch."),
        };
    }

    CoverageNormalizationResult result;
    result.coverage.resize(static_cast<std::size_t>(width) * height);
    for (std::uint32_t row = 0; row < height; ++row) {
        const std::uint32_t source_row = pitch > 0 ? row : height - 1 - row;
        std::copy_n(
            source.begin() + static_cast<std::size_t>(source_row) * absolute_pitch,
            width,
            result.coverage.begin() + static_cast<std::size_t>(row) * width);
    }
    return result;
}

} // namespace ryn::font
