#include "platform/default_font_chain.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_default_ui_font_chain() {
    auto created = ryn::font::FontRuntime::create();
    require(static_cast<bool>(created), "Font Runtime initialization failed");
    auto fonts = std::move(created.runtime);
    ryn::detail::DefaultFontChainRequest request;
    request.raster = {14, 1.5F};
    request.fallback_latin = RYNUI_VALIDATION_LATIN_FONT;
    request.fallback_cjk = RYNUI_VALIDATION_CJK_FONT;
    const auto chain = ryn::detail::load_default_ui_font_chain(*fonts, request);
    require(static_cast<bool>(chain), "default UI font chain did not load");
    require(!chain.faces.empty() && !chain.telemetry_families().empty(),
            "default UI font chain lost face diagnostics");
    require(chain.telemetry_rendering().find("aa=") != std::string::npos
                && chain.telemetry_rendering().find("hint=") != std::string::npos
                && chain.telemetry_rendering().find("bitmap=") != std::string::npos,
            "default UI font chain lost raster policy telemetry");
    require(!chain.telemetry_source().empty(),
            "default UI font chain lost source diagnostics");
    for (const auto& face : chain.faces) {
        require(std::filesystem::exists(face.source_path),
                "default UI font chain returned a missing source file");
        const auto metrics = fonts->metrics(face.identity);
        require(metrics
                    && metrics.metrics.logical_pixel_size == 14
                    && metrics.metrics.raster_pixel_size == 21,
                "default UI face did not keep high-DPI raster metrics");
    }
    const auto identities = chain.identities();
    require(static_cast<bool>(fonts->find_glyph(identities, U'A', std::nullopt)),
            "default UI font chain does not cover Latin text");
    require(static_cast<bool>(fonts->find_glyph(identities, U'中', std::nullopt)),
            "default UI font chain does not cover Simplified Chinese text");
    auto resolver = ryn::detail::make_default_ui_font_resolver(*fonts, chain, 1.5F);
    const auto initial_resolved = resolver(
        ryn::SystemFontFamily::ui_sans, 400, 14);
    const auto large_resolved = resolver(
        ryn::SystemFontFamily::ui_sans, 400, 16);
    require(initial_resolved == identities
                && !large_resolved.empty()
                && resolver(ryn::SystemFontFamily::ui_sans, 400, 16)
                    == large_resolved,
            "default Theme font resolver did not reuse initial and cached size chains");
    for (const auto identity : large_resolved) {
        const auto metrics = fonts->metrics(identity);
        require(metrics
                    && metrics.metrics.logical_pixel_size == 16
                    && metrics.metrics.raster_pixel_size == 24,
                "default Theme font resolver lost logical-to-device raster sizing");
    }
    auto moved_resolver = ryn::detail::make_default_ui_font_resolver(
        *fonts, chain, 2.0F);
    const auto moved_resolved = moved_resolver(
        ryn::SystemFontFamily::ui_sans, 400, 14);
    require(!moved_resolved.empty() && moved_resolved != identities,
            "display-scale refresh reused the startup font identity");
    for (const auto identity : moved_resolved) {
        const auto metrics = fonts->metrics(identity);
        require(metrics
                    && metrics.metrics.logical_pixel_size == 14
                    && metrics.metrics.raster_pixel_size == 28
                    && std::abs(metrics.metrics.display_scale - 2.0F) < 0.0001F,
                "display-scale refresh retained startup raster density");
    }

#if defined(_WIN32)
    require(chain.uses_system_fonts,
            "Windows default UI font chain did not use system fonts");
    require(chain.faces.front().system_font
                && chain.faces.front().family_name.starts_with("Segoe UI"),
            "Windows default UI font chain did not prefer Segoe UI");
    bool found_yahei = false;
    for (const auto& face : chain.faces) {
        found_yahei = found_yahei
            || face.family_name == std::string_view{"Microsoft YaHei UI"};
    }
    require(found_yahei,
            "Windows default UI font chain did not include Microsoft YaHei UI");
#elif defined(__linux__)
    require(chain.uses_system_fonts,
            "Linux default UI font chain did not use Fontconfig system fonts");
#else
    require(!chain.uses_system_fonts,
            "unsupported platform claimed native system font discovery");
#endif
}

void test_custom_font_precedes_platform_defaults() {
    auto created = ryn::font::FontRuntime::create();
    require(static_cast<bool>(created), "Font Runtime initialization failed");
    auto fonts = std::move(created.runtime);

    ryn::detail::DefaultFontChainRequest request;
    request.raster = {14, 1.0F};
    request.preferred_fonts.push_back({
        RYNUI_VALIDATION_LATIN_FONT,
        0,
        "ConfiguredTestFont",
    });
    request.fallback_latin = RYNUI_VALIDATION_LATIN_FONT;
    request.fallback_cjk = RYNUI_VALIDATION_CJK_FONT;

    const auto chain = ryn::detail::load_default_ui_font_chain(*fonts, request);
    require(static_cast<bool>(chain), "custom UI font chain did not load");
    require(chain.uses_custom_fonts && chain.faces.front().custom_font,
            "configured custom font did not precede platform defaults");
    require(chain.faces.front().family_name == std::string_view{"ConfiguredTestFont"},
            "configured custom font lost its diagnostic family name");
    const auto identities = chain.identities();
    require(static_cast<bool>(fonts->find_glyph(identities, U'A', std::nullopt)),
            "custom UI font chain lost Latin coverage");
    require(static_cast<bool>(fonts->find_glyph(identities, U'中', std::nullopt)),
            "custom UI font chain lost system or bundled CJK fallback");
}

void test_invalid_custom_font_fails_fast() {
    auto created = ryn::font::FontRuntime::create();
    require(static_cast<bool>(created), "Font Runtime initialization failed");
    auto fonts = std::move(created.runtime);

    ryn::detail::DefaultFontChainRequest request;
    request.raster = {14, 1.0F};
    request.preferred_fonts.push_back({
        std::filesystem::path{"missing-custom-font.ttf"},
        0,
        "MissingCustomFont",
    });
    request.fallback_latin = RYNUI_VALIDATION_LATIN_FONT;
    request.fallback_cjk = RYNUI_VALIDATION_CJK_FONT;

    const auto chain = ryn::detail::load_default_ui_font_chain(*fonts, request);
    require(!chain, "invalid custom UI font silently used a fallback");
    require(chain.faces.empty(), "failed custom UI font leaked loaded faces");
    require(chain.diagnostic.find("missing-custom-font.ttf") != std::string::npos,
            "failed custom UI font diagnostic lost the configured path");
}

} // namespace

int main() {
    try {
        test_default_ui_font_chain();
        test_custom_font_precedes_platform_defaults();
        test_invalid_custom_font_fails_fast();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
