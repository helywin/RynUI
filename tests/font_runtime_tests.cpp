#include "font/font_runtime.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

using ryn::font::FontErrorKind;
using ryn::font::FontErrorStage;
using ryn::font::FontFailurePoint;
using ryn::font::FontIdentity;
using ryn::font::FontRuntime;
using ryn::font::FontRuntimeCounters;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    require(static_cast<bool>(input), "unable to open validation font");
    const std::streamoff length = input.tellg();
    require(length > 0, "validation font is empty");
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    input.seekg(0, std::ios::beg);
    require(
        static_cast<bool>(input.read(reinterpret_cast<char*>(bytes.data()), length)),
        "unable to read validation font");
    return bytes;
}

std::unique_ptr<FontRuntime> create_runtime(
    const std::shared_ptr<FontRuntimeCounters>& counters = {}) {
    auto created = FontRuntime::create({FontFailurePoint::none, counters});
    require(static_cast<bool>(created), "Font Runtime initialization failed");
    return std::move(created.runtime);
}

void require_failed_stage(
    const ryn::font::FontLoadResult& result,
    FontErrorStage stage,
    const char* message) {
    require(!result, message);
    require(result.error.stage == stage, "font failure reported the wrong stage");
}

void test_failure_state_machine_and_generation() {
    auto counters = std::make_shared<FontRuntimeCounters>();
    const auto library_failure = FontRuntime::create(
        {FontFailurePoint::library_initialization, counters});
    require(!library_failure, "injected library failure succeeded");
    require(
        library_failure.error.stage == FontErrorStage::library_initialization,
        "library failure reported the wrong stage");
    require(counters->libraries_acquired == 0 && counters->libraries_released == 0,
            "failed library initialization released an unowned resource");

    {
        auto runtime = create_runtime(counters);
        require(counters->libraries_acquired == 1, "FreeType library was not acquired");

        const auto bytes = read_bytes(RYNUI_VALIDATION_LATIN_FONT);
        std::size_t expected_byte_failures = 0;
        std::size_t expected_face_failures = 0;
        for (const FontFailurePoint failure : {
                 FontFailurePoint::after_font_bytes,
                 FontFailurePoint::after_face_creation,
                 FontFailurePoint::charmap_selection,
                 FontFailurePoint::pixel_size_configuration,
             }) {
            const auto failed = runtime->load_font_bytes(bytes, 0, 14, failure);
            require(!failed, "injected font initialization failure succeeded");
            ++expected_byte_failures;
            if (failure != FontFailurePoint::after_font_bytes) {
                ++expected_face_failures;
            }
            require(
                counters->byte_resources_acquired == expected_byte_failures
                    && counters->byte_resources_released == expected_byte_failures,
                "failed initialization did not release exactly its acquired bytes");
            require(
                counters->faces_acquired == expected_face_failures
                    && counters->faces_released == expected_face_failures,
                "failed initialization released the wrong number of faces");
        }

        const auto loaded = runtime->load_font_bytes(bytes, 0, 14);
        require(static_cast<bool>(loaded), "valid font failed after injected failures");
        require(static_cast<bool>(runtime->metrics(loaded.font)),
                "loaded font has no metrics");
        require(static_cast<bool>(runtime->remove_font(loaded.font)),
                "font removal failed");
        require(!runtime->metrics(loaded.font), "stale Font identity remained valid");

        const auto reused = runtime->load_font_bytes(bytes, 0, 14);
        require(static_cast<bool>(reused), "font slot reuse failed");
        require(reused.font.slot == loaded.font.slot,
                "empty font slot was not reused deterministically");
        require(reused.font.generation != loaded.font.generation,
                "reused font slot did not advance its generation");
        require(!runtime->metrics(loaded.font),
                "old Font identity accessed a reused font slot");
        require(static_cast<bool>(runtime->remove_font(reused.font)),
                "reused font removal failed");
        require(static_cast<bool>(runtime->shutdown()),
                "owner-thread shutdown failed");
    }

    require(counters->libraries_acquired == counters->libraries_released,
            "FreeType library lifetime is unbalanced");
    require(counters->byte_resources_acquired == counters->byte_resources_released,
            "font byte lifetime is unbalanced");
    require(counters->faces_acquired == counters->faces_released,
            "FreeType face lifetime is unbalanced");
}

void test_load_errors_and_metrics() {
    auto runtime = create_runtime();
    const auto bytes = read_bytes(RYNUI_VALIDATION_LATIN_FONT);

    const auto missing = runtime->load_font_file(
        std::filesystem::path{RYNUI_VALIDATION_LATIN_FONT}.parent_path()
            / "missing-font.ttf",
        0,
        14);
    require_failed_stage(missing, FontErrorStage::resource_read,
                         "missing font resource succeeded");

    const auto invalid = runtime->load_font_bytes(
        std::span<const std::byte>{}, 0, 14);
    require_failed_stage(invalid, FontErrorStage::face_creation,
                         "empty font bytes succeeded");
    require(invalid.error.kind == FontErrorKind::invalid_font_data,
            "invalid font bytes returned the wrong error");

    const auto invalid_face = runtime->load_font_bytes(bytes, 1, 14);
    require_failed_stage(invalid_face, FontErrorStage::face_creation,
                         "invalid face index succeeded");
    require(invalid_face.error.kind == FontErrorKind::invalid_face_index,
            "invalid face index returned the wrong error");

    const auto zero_size = runtime->load_font_bytes(bytes, 0, 0);
    require_failed_stage(zero_size, FontErrorStage::pixel_size_configuration,
                         "zero pixel size succeeded");
    const auto invalid_scale = runtime->load_font_bytes(
        bytes, 0, ryn::font::FontRasterConfig{14, 0.0F});
    require_failed_stage(invalid_scale, FontErrorStage::pixel_size_configuration,
                         "zero display scale succeeded");

    const auto no_charmap = runtime->load_font_bytes(
        bytes, 0, 14, FontFailurePoint::charmap_selection);
    require_failed_stage(no_charmap, FontErrorStage::charmap_selection,
                         "font without Unicode charmap succeeded");
    require(no_charmap.error.kind == FontErrorKind::no_unicode_charmap,
            "missing Unicode charmap returned the wrong error");

    const auto first = runtime->load_font_bytes(bytes, 0, 14);
    const auto second = runtime->load_font_bytes(bytes, 0, 14);
    const auto larger = runtime->load_font_bytes(bytes, 0, 28);
    const auto high_density = runtime->load_font_bytes(
        bytes, 0, ryn::font::FontRasterConfig{14, 1.5F});
    require(first && second && larger && high_density, "valid metric font load failed");

    const auto first_metrics = runtime->metrics(first.font);
    const auto second_metrics = runtime->metrics(second.font);
    const auto larger_metrics = runtime->metrics(larger.font);
    const auto high_density_metrics = runtime->metrics(high_density.font);
    require(first_metrics && second_metrics && larger_metrics && high_density_metrics,
            "font metrics query failed");
    require(first_metrics.metrics == second_metrics.metrics,
            "same font and size produced unstable metrics");
    require(first_metrics.metrics.ascent > 0.0F
                && first_metrics.metrics.descent < 0.0F,
            "font metrics use an unexpected coordinate convention");
    require(larger_metrics.metrics.ascent > first_metrics.metrics.ascent,
            "pixel-size conversion did not scale ascent");
    require(high_density_metrics.metrics.logical_pixel_size == 14
                && high_density_metrics.metrics.raster_pixel_size == 21
                && std::abs(high_density_metrics.metrics.display_scale - 1.5F) < 0.00001F
                && std::abs(high_density_metrics.metrics.raster_scale - 1.5F) < 0.00001F,
            "high-density font did not separate logical and raster sizes");

    const auto logical_shape = runtime->shape_utf8_segment(first.font, "RynUI", 0, 5);
    const auto high_density_shape = runtime->shape_utf8_segment(
        high_density.font, "RynUI", 0, 5);
    require(logical_shape && high_density_shape
                && logical_shape.glyphs.size() == high_density_shape.glyphs.size(),
            "high-density shaping failed");
    float logical_advance = 0.0F;
    float high_density_advance = 0.0F;
    for (std::size_t index = 0; index < logical_shape.glyphs.size(); ++index) {
        logical_advance += logical_shape.glyphs[index].advance_x;
        high_density_advance += high_density_shape.glyphs[index].advance_x;
    }
    require(std::abs(logical_advance - high_density_advance) < 1.0F,
            "high-density shaping leaked raster pixels into logical layout");

    const auto logical_a = runtime->glyph_index(first.font, U'A');
    const auto high_density_a = runtime->glyph_index(high_density.font, U'A');
    const auto logical_bitmap = runtime->rasterize(first.font, logical_a.glyph.glyph_id);
    const auto high_density_bitmap = runtime->rasterize(
        high_density.font, high_density_a.glyph.glyph_id);
    require(logical_a && high_density_a && logical_bitmap && high_density_bitmap,
            "high-density glyph rasterization failed");
    require(high_density_bitmap.glyph->width > logical_bitmap.glyph->width,
            "high-density glyph coverage width did not increase");
    require(high_density_bitmap.glyph->height > logical_bitmap.glyph->height,
            "high-density glyph coverage height did not increase");
    require(std::abs(high_density_bitmap.glyph->raster_scale - 1.5F) < 0.00001F,
            "high-density glyph coverage lost its raster scale");
    require(std::abs(high_density_bitmap.glyph->display_scale - 1.5F) < 0.00001F,
            "high-density glyph coverage lost its display scale");

    ryn::font::FontRasterConfig monochrome_config{14, 1.0F};
    monochrome_config.policy.antialias = false;
    monochrome_config.policy.hinting = false;
    monochrome_config.policy.hint_style = ryn::font::FontHintStyle::none;
    monochrome_config.policy.embedded_bitmap = false;
    const auto monochrome = runtime->load_font_bytes(bytes, 0, monochrome_config);
    const auto monochrome_a = runtime->glyph_index(monochrome.font, U'A');
    const auto monochrome_bitmap = runtime->rasterize(
        monochrome.font, monochrome_a.glyph.glyph_id);
    require(monochrome && monochrome_a && monochrome_bitmap
                && !monochrome_bitmap.glyph->coverage.empty()
                && std::ranges::all_of(
                    monochrome_bitmap.glyph->coverage,
                    [](std::uint8_t value) { return value == 0 || value == 255; }),
            "monochrome Fontconfig policy was not normalized for the R8 atlas");
}

void test_fallback_and_missing_glyph_diagnostics() {
    auto runtime = create_runtime();
    const auto latin = runtime->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, 14);
    const auto cjk = runtime->load_font_file(RYNUI_VALIDATION_CJK_FONT, 0, 14);
    require(latin && cjk, "fallback fonts failed to load");
    const std::array chain{latin.font, cjk.font};

    const auto latin_result = runtime->find_glyph(chain, U'A');
    require(latin_result && latin_result.glyph.font == latin.font,
            "Latin codepoint did not select the first font");

    const auto cjk_result = runtime->find_glyph(chain, U'中');
    const auto repeated_cjk = runtime->find_glyph(chain, U'中');
    require(cjk_result && cjk_result.glyph.font == cjk.font,
            "CJK codepoint did not select the fallback font");
    require(repeated_cjk
                && repeated_cjk.glyph.font == cjk_result.glyph.font
                && repeated_cjk.glyph.glyph_id == cjk_result.glyph.glyph_id,
            "repeated fallback lookup was not deterministic");

    constexpr char32_t unavailable = static_cast<char32_t>(0x0378);
    const auto replacement = runtime->find_glyph(chain, unavailable, U'?');
    require(replacement
                && replacement.glyph.font == latin.font
                && replacement.glyph.used_replacement
                && replacement.glyph.requested_codepoint == unavailable
                && replacement.glyph.resolved_codepoint == U'?',
            "replacement glyph policy was not explicit or deterministic");

    const auto missing = runtime->find_glyph(chain, unavailable, std::nullopt);
    require(!missing && missing.error.kind == FontErrorKind::missing_glyph,
            "all-missing fallback did not return a typed error");
    require(missing.error.codepoint == unavailable,
            "missing-glyph diagnostic lost the requested codepoint");

    const auto invalid_scalar = runtime->find_glyph(
        chain, static_cast<char32_t>(0xD800), std::nullopt);
    require(!invalid_scalar && invalid_scalar.error.kind == FontErrorKind::invalid_codepoint,
            "surrogate coverage query was accepted");
}

void test_rasterization_pitch_and_cache() {
    auto runtime = create_runtime();
    const auto font = runtime->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, 14);
    require(static_cast<bool>(font), "rasterization font failed to load");

    const auto a = runtime->glyph_index(font.font, U'A');
    const auto space = runtime->glyph_index(font.font, U' ');
    const auto b = runtime->glyph_index(font.font, U'B');
    require(a && a.glyph.glyph_id != 0 && space && b,
            "rasterization glyph lookup failed");

    const auto first = runtime->rasterize(font.font, a.glyph.glyph_id);
    require(first && !first.cache_hit, "first glyph rasterization failed");
    require(first.glyph->width > 0 && first.glyph->height > 0,
            "visible glyph produced no coverage dimensions");
    require(first.glyph->coverage.size()
                == static_cast<std::size_t>(first.glyph->width) * first.glyph->height,
            "visible glyph coverage is not tightly packed");
    require(std::ranges::any_of(first.glyph->coverage, [](std::uint8_t value) {
                return value != 0;
            }),
            "visible glyph coverage is entirely clear");

    const auto repeated = runtime->rasterize(font.font, a.glyph.glyph_id);
    require(repeated && repeated.cache_hit && repeated.glyph == first.glyph,
            "repeated glyph request did not reuse the cache entry");
    require(runtime->counters().rasterizations == 1
                && runtime->counters().cache_hits == 1
                && runtime->glyph_cache_size() == 1,
            "glyph cache counters are incorrect");

    const auto quarter = runtime->rasterize(
        font.font,
        a.glyph.glyph_id,
        ryn::font::GlyphRasterPhase::quarter);
    const auto half = runtime->rasterize(
        font.font,
        a.glyph.glyph_id,
        ryn::font::GlyphRasterPhase::half);
    const auto three_quarters = runtime->rasterize(
        font.font,
        a.glyph.glyph_id,
        ryn::font::GlyphRasterPhase::three_quarters);
    const auto repeated_half = runtime->rasterize(
        font.font,
        a.glyph.glyph_id,
        ryn::font::GlyphRasterPhase::half);
    require(quarter && half && three_quarters && repeated_half
                && !quarter.cache_hit && !half.cache_hit && !three_quarters.cache_hit
                && repeated_half.cache_hit && repeated_half.glyph == half.glyph,
            "quarter-pixel glyph phases did not use bounded cache variants");
    require(runtime->glyph_cache_size() == 4
                && runtime->counters().rasterizations == 4
                && runtime->counters().cache_hits == 2,
            "phase-aware glyph cache counters are incorrect");
    const bool quarter_differs = quarter.glyph->bearing_x != first.glyph->bearing_x
        || quarter.glyph->width != first.glyph->width
        || quarter.glyph->coverage != first.glyph->coverage;
    const bool half_differs = half.glyph->bearing_x != first.glyph->bearing_x
        || half.glyph->width != first.glyph->width
        || half.glyph->coverage != first.glyph->coverage;
    require(quarter_differs || half_differs,
            "physical glyph phase did not alter raster coverage");

    const auto injected = runtime->rasterize(
        font.font, b.glyph.glyph_id, ryn::font::GlyphRasterMode::grayscale,
        FontFailurePoint::rasterization);
    require(!injected && runtime->glyph_cache_size() == 4,
            "failed rasterization mutated the glyph cache");

    const auto blank = runtime->rasterize(font.font, space.glyph.glyph_id);
    require(blank && blank.glyph->width == 0 && blank.glyph->height == 0,
            "space glyph unexpectedly produced visible coverage");
    require(blank.glyph->coverage.empty() && blank.glyph->advance_x > 0.0F,
            "space glyph did not preserve its advance");

    const std::array<std::uint8_t, 6> positive_source{1, 2, 99, 3, 4, 88};
    const auto positive = ryn::font::normalize_gray_coverage(
        positive_source, 2, 2, 3);
    require(positive && positive.coverage == std::vector<std::uint8_t>({1, 2, 3, 4}),
            "positive bitmap pitch was not normalized");

    const std::array<std::uint8_t, 6> negative_source{3, 4, 88, 1, 2, 99};
    const auto negative = ryn::font::normalize_gray_coverage(
        negative_source, 2, 2, -3);
    require(negative && negative.coverage == positive.coverage,
            "negative bitmap pitch inverted glyph rows");

    const auto invalid_pitch = ryn::font::normalize_gray_coverage(
        positive_source, 4, 2, 3);
    require(!invalid_pitch
                && invalid_pitch.error.kind == FontErrorKind::unsupported_bitmap,
            "undersized bitmap pitch was accepted");
}

void test_owner_thread_guard() {
    auto runtime = create_runtime();
    const auto font = runtime->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, 14);
    const auto glyph = runtime->glyph_index(font.font, U'A');
    require(font && glyph, "owner-thread guard fixture failed");

    const std::size_t queries_before = runtime->counters().coverage_queries;
    const std::size_t rasterizations_before = runtime->counters().rasterizations;
    const std::size_t cache_size_before = runtime->glyph_cache_size();

    ryn::font::GlyphLookupResult wrong_lookup;
    ryn::font::GlyphRasterResult wrong_raster;
    ryn::font::FontActionResult wrong_remove;
    ryn::font::FontActionResult wrong_shutdown;
    std::thread worker([&] {
        wrong_lookup = runtime->glyph_index(font.font, U'A');
        wrong_raster = runtime->rasterize(font.font, glyph.glyph.glyph_id);
        wrong_remove = runtime->remove_font(font.font);
        wrong_shutdown = runtime->shutdown();
    });
    worker.join();

    require(!wrong_lookup && wrong_lookup.error.kind == FontErrorKind::wrong_thread,
            "wrong-thread coverage query did not fail fast");
    require(!wrong_raster && wrong_raster.error.kind == FontErrorKind::wrong_thread,
            "wrong-thread rasterization did not fail fast");
    require(!wrong_remove && wrong_remove.error.kind == FontErrorKind::wrong_thread,
            "wrong-thread font destruction did not fail fast");
    require(!wrong_shutdown && wrong_shutdown.error.kind == FontErrorKind::wrong_thread,
            "wrong-thread runtime destruction did not fail fast");
    require(runtime->counters().coverage_queries == queries_before
                && runtime->counters().rasterizations == rasterizations_before
                && runtime->glyph_cache_size() == cache_size_before,
            "wrong-thread access changed Font Runtime state");
    require(static_cast<bool>(runtime->glyph_index(font.font, U'A')),
            "wrong-thread access damaged the existing font");
    require(static_cast<bool>(runtime->remove_font(font.font)),
            "owner thread could not remove font after rejected access");
}

} // namespace

int main() {
    try {
        test_failure_state_machine_and_generation();
        test_load_errors_and_metrics();
        test_fallback_and_missing_glyph_diagnostics();
        test_rasterization_pitch_and_cache();
        test_owner_thread_guard();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
