#include "platform/default_font_chain.hpp"

#include <algorithm>
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
    auto chain = ryn::detail::load_default_ui_font_chain(*fonts, request);
    require(static_cast<bool>(chain), "default UI font chain did not load");
    require(!chain.faces.empty() && !chain.telemetry_families().empty(), "default UI font chain lost face diagnostics");
    require(chain.telemetry_rendering().find("aa=") != std::string::npos &&
                chain.telemetry_rendering().find("hint=") != std::string::npos &&
                chain.telemetry_rendering().find("bitmap=") != std::string::npos,
            "default UI font chain lost raster policy telemetry");
    require(!chain.telemetry_source().empty(), "default UI font chain lost source diagnostics");
    for (const auto& face : chain.faces) {
        require(std::filesystem::exists(face.source_path), "default UI font chain returned a missing source file");
        const auto metrics = fonts->metrics(face.identity);
        require(metrics && metrics.metrics.logical_pixel_size == 14 && metrics.metrics.raster_pixel_size == 21,
                "default UI face did not keep high-DPI raster metrics");
    }
    const auto identities = chain.identities();
    require(static_cast<bool>(fonts->find_glyph(identities, U'A', std::nullopt)),
            "default UI font chain does not cover Latin text");
    require(static_cast<bool>(fonts->find_glyph(identities, U'中', std::nullopt)),
            "default UI font chain does not cover Simplified Chinese text");
    require(static_cast<bool>(fonts->find_glyph(identities, U'\uFFFD', std::nullopt)),
            "default UI font chain has no Unicode replacement glyph");
    const auto unsupported = fonts->find_glyph(identities, U'\U0010FFFF');
    require(unsupported && unsupported.glyph.used_replacement && unsupported.glyph.resolved_codepoint == U'\uFFFD',
            "default UI font chain cannot display an uncovered Unicode scalar");
    auto resolver = ryn::detail::make_default_ui_font_resolver(*fonts, chain, 1.5F);
    const auto initial_resolved = resolver(ryn::SystemFontFamily::ui_sans, 400, false, 14);
    const auto large_resolved = resolver(ryn::SystemFontFamily::ui_sans, 400, false, 16);
    require(initial_resolved == identities && !large_resolved.empty() &&
                resolver(ryn::SystemFontFamily::ui_sans, 400, false, 16) == large_resolved,
            "default Theme font resolver did not reuse initial and cached size chains");
    for (const auto family : {ryn::SystemFontFamily::ui_sans, ryn::SystemFontFamily::ui_monospace}) {
        for (const auto weight : {400U, 700U}) {
            const auto resized = resolver(family, weight, false, 16);
            require(static_cast<bool>(fonts->find_glyph(resized, U'\uFFFD', std::nullopt)),
                    "resized/styled default font resolver dropped the Unicode replacement glyph");
#if defined(_WIN32)
            for (const auto scalar : {U'م', U'ר', U'ب', U'א'}) {
                require(static_cast<bool>(fonts->find_glyph(resized, scalar, std::nullopt)),
                        "resized/styled default font resolver dropped Arabic/Hebrew coverage");
            }
#endif
        }
    }
    for (const auto identity : large_resolved) {
        const auto metrics = fonts->metrics(identity);
        require(metrics && metrics.metrics.logical_pixel_size == 16 && metrics.metrics.raster_pixel_size == 24,
                "default Theme font resolver lost logical-to-device raster sizing");
    }
    auto moved_resolver = ryn::detail::make_default_ui_font_resolver(*fonts, chain, 2.0F);
    const auto moved_resolved = moved_resolver(ryn::SystemFontFamily::ui_sans, 400, false, 14);
    require(!moved_resolved.empty() && moved_resolved != identities,
            "display-scale refresh reused the startup font identity");
    for (const auto identity : moved_resolved) {
        const auto metrics = fonts->metrics(identity);
        require(metrics && metrics.metrics.logical_pixel_size == 14 && metrics.metrics.raster_pixel_size == 28 &&
                    std::abs(metrics.metrics.display_scale - 2.0F) < 0.0001F,
                "display-scale refresh retained startup raster density");
    }

#if defined(_WIN32)
    require(chain.uses_system_fonts, "Windows default UI font chain did not use system fonts");
    require(chain.faces.front().system_font && chain.faces.front().family_name.starts_with("Segoe UI"),
            "Windows default UI font chain did not prefer Segoe UI");
    bool found_yahei = false;
    for (const auto& face : chain.faces) {
        found_yahei = found_yahei || face.family_name == std::string_view{"Microsoft YaHei UI"};
    }
    require(found_yahei, "Windows default UI font chain did not include Microsoft YaHei UI");
    for (const auto scalar : {U'م', U'ר', U'ب', U'א'}) {
        require(static_cast<bool>(fonts->find_glyph(identities, scalar, std::nullopt)) &&
                    static_cast<bool>(fonts->find_glyph(moved_resolved, scalar, std::nullopt)),
                "Windows default UI font chain lost Arabic/Hebrew coverage at startup or after DPI refresh");
    }
#elif defined(__linux__)
    require(chain.uses_system_fonts, "Linux default UI font chain did not use Fontconfig system fonts");
#else
    require(!chain.uses_system_fonts, "unsupported platform claimed native system font discovery");
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

void test_monospace_chain_resolution() {
    auto created = ryn::font::FontRuntime::create();
    require(static_cast<bool>(created), "Font Runtime initialization failed");
    auto fonts = std::move(created.runtime);

    ryn::detail::DefaultFontChainRequest request;
    request.raster = {14, 1.5F};
    request.fallback_latin = RYNUI_VALIDATION_LATIN_FONT;
    request.fallback_cjk = RYNUI_VALIDATION_CJK_FONT;
    auto chain = ryn::detail::load_default_ui_font_chain(*fonts, request);
    require(static_cast<bool>(chain), "default UI font chain did not load");

    const auto ui = chain.identities();
    const auto mono = chain.monospace_identities();
    // The monospace chain always appends the UI chain, so an uncovered codepoint
    // still resolves to a readable face instead of a missing glyph.
    for (const auto identity : ui) {
        require(std::ranges::find(mono, identity) != mono.end(), "monospace chain dropped the UI fallback faces");
    }

    auto resolver = ryn::detail::make_default_ui_font_resolver(*fonts, chain, 1.5F);
    const auto resolved_ui = resolver(ryn::SystemFontFamily::ui_sans, 400, false, 14);
    const auto resolved_mono = resolver(ryn::SystemFontFamily::ui_monospace, 400, false, 14);
    require(!resolved_ui.empty() && !resolved_mono.empty(), "family-aware resolver returned an empty chain");
    require(resolved_ui == ui, "ui_sans resolution drifted from the loaded UI chain");
    require(resolved_mono == mono, "ui_monospace resolution drifted from the monospace chain");

    if (!chain.monospace_faces.empty()) {
        // A real monospace face must lead the monospace chain while the UI chain
        // keeps its proportional Latin face.
        require(resolved_mono.front() != resolved_ui.front(),
                "monospace resolution reused the proportional Latin face first");
        const auto metrics = fonts->metrics(resolved_mono.front());
        require(metrics && metrics.metrics.logical_pixel_size == 14 && metrics.metrics.raster_pixel_size == 21,
                "monospace face lost high-DPI raster metrics");
        // The monospace family itself must cover ASCII; the UI chain covers CJK.
        const std::vector<ryn::font::FontIdentity> monospace_only{resolved_mono.front()};
        require(static_cast<bool>(fonts->find_glyph(monospace_only, U'M', std::nullopt)),
                "resolved monospace face does not cover ASCII");
        require(static_cast<bool>(fonts->find_glyph(resolved_mono, U'中', std::nullopt)),
                "monospace chain does not fall back for Simplified Chinese");
    }

    // Distinct families must not share a cache entry: the same pixel size has to
    // keep resolving to each family's own chain.
    require(resolver(ryn::SystemFontFamily::ui_monospace, 400, false, 14) == resolved_mono &&
                resolver(ryn::SystemFontFamily::ui_sans, 400, false, 14) == resolved_ui,
            "family caches leaked into each other");

    // A different pixel size re-resolves both families at the new DPI.
    const auto large_mono = resolver(ryn::SystemFontFamily::ui_monospace, 400, false, 16);
    require(!large_mono.empty() && large_mono != resolved_mono, "monospace resolution ignored the pixel size");
    for (const auto identity : large_mono) {
        const auto metrics = fonts->metrics(identity);
        require(metrics && metrics.metrics.logical_pixel_size == 16 && metrics.metrics.raster_pixel_size == 24,
                "monospace chain lost logical-to-device raster sizing");
    }
    // Both families stay available at the new size.
    const auto large_ui = resolver(ryn::SystemFontFamily::ui_sans, 400, false, 16);
    require(large_ui == resolver(ryn::SystemFontFamily::ui_sans, 400, false, 16) && large_ui != large_mono,
            "ui_sans and ui_monospace converged at a second pixel size");

#if defined(_WIN32)
    bool found_monospace_system = false;
    for (const auto& face : chain.monospace_faces) {
        found_monospace_system = found_monospace_system || face.family_name == std::string_view{"Cascadia Mono"} ||
                                 face.family_name == std::string_view{"Consolas"} ||
                                 face.family_name == std::string_view{"Lucida Console"};
    }
    require(found_monospace_system, "Windows monospace resolution found no known monospace family");
#endif
}

void test_weight_and_slant_face_selection() {
#if defined(_WIN32)
    const auto regular_face = ryn::detail::resolve_platform_face(ryn::SystemFontFamily::ui_sans, 400, false);
    require(regular_face.has_value(), "Windows regular face missing");
    for (const auto weight : {600U, 700U}) {
        const auto face = ryn::detail::resolve_platform_face(ryn::SystemFontFamily::ui_sans, weight, false);
        require(face && face->weight >= 500 && !face->italic &&
                    (face->source_path != regular_face->source_path || face->face_index != regular_face->face_index),
                "Windows styled resolution reused default variable coordinates");
    }
    const auto italic_face = ryn::detail::resolve_platform_face(ryn::SystemFontFamily::ui_sans, 400, true);
    require(italic_face && italic_face->italic &&
                (italic_face->source_path != regular_face->source_path ||
                 italic_face->face_index != regular_face->face_index),
            "Windows italic resolution did not select a real slanted face");
#endif
    auto created = ryn::font::FontRuntime::create();
    require(static_cast<bool>(created), "Font Runtime initialization failed");
    auto fonts = std::move(created.runtime);

    ryn::detail::DefaultFontChainRequest request;
    request.raster = {14, 1.0F};
    request.fallback_latin = RYNUI_VALIDATION_LATIN_FONT;
    request.fallback_cjk = RYNUI_VALIDATION_CJK_FONT;
    auto chain = ryn::detail::load_default_ui_font_chain(*fonts, request);
    require(static_cast<bool>(chain), "default UI font chain did not load");
    auto resolver = ryn::detail::make_default_ui_font_resolver(*fonts, chain, 1.0F);

    const auto regular = resolver(ryn::SystemFontFamily::ui_sans, 400, false, 14);
    const auto strong = resolver(ryn::SystemFontFamily::ui_sans, 700, false, 14);
    const auto italic = resolver(ryn::SystemFontFamily::ui_sans, 400, true, 14);
    require(!regular.empty() && !strong.empty() && !italic.empty(), "styled resolution returned an empty chain");
    // Identity values are slot+generation pairs assigned per load, so they are not
    // stable across separate resolver calls. Assert coverage instead: every styled
    // chain keeps Latin and CJK resolvable, which is the contract that matters.
    for (const auto& chain : {strong, italic}) {
        require(static_cast<bool>(fonts->find_glyph(chain, U'A', std::nullopt)),
                "styled chain does not cover Latin text");
        require(static_cast<bool>(fonts->find_glyph(chain, U'中', std::nullopt)),
                "styled chain does not keep the CJK fallback");
    }
    // Requesting the same key twice must reuse the cached entry.
    require(resolver(ryn::SystemFontFamily::ui_sans, 700, false, 14) == strong,
            "styled resolution did not cache per weight");
    const auto italic_again = resolver(ryn::SystemFontFamily::ui_sans, 400, true, 14);
    require(italic_again == italic, "styled resolution did not cache per slant");

    // A styled request must either lead with a genuinely different face or record
    // a precise fallback note. Which one happens depends on the platform: some
    // Platforms can expose only a regular family; that fallback must be explicit.
    // Windows separately requires a real static Segoe style above, since default
    // variation coordinates cannot represent a named DirectWrite instance.
    const auto note_for = [&chain](std::string_view needle) {
        return std::ranges::any_of(chain.diagnostic_fallbacks,
                                   [&](const auto& note) { return note.find(needle) != std::string::npos; });
    };
    const bool bold_swapped = strong.size() > regular.size();
    const bool italic_swapped = italic.size() > regular.size();
    require(bold_swapped || note_for("weight 700"), "bold request neither swapped the face nor recorded a fallback");
    require(italic_swapped || note_for("400 italic"),
            "italic request neither swapped the face nor recorded a fallback");
    if (!bold_swapped || !italic_swapped) {
        // A fallback note must still leave a usable chain.
        require(static_cast<bool>(fonts->find_glyph(strong, U'A', std::nullopt)) &&
                    static_cast<bool>(fonts->find_glyph(italic, U'A', std::nullopt)),
                "fallback styled chain lost Latin coverage");
    }
}

// A styled request must stay resolvable when the application supplies its own
// fonts, so pages that use `strong`/`italic` do not depend on which system faces
// the host exposes.
void test_injected_styled_faces() {
    auto created = ryn::font::FontRuntime::create();
    require(static_cast<bool>(created), "Font Runtime initialization failed");
    auto fonts = std::move(created.runtime);

    ryn::detail::DefaultFontChainRequest request;
    request.raster = {14, 1.0F};
    request.preferred_fonts.push_back({RYNUI_VALIDATION_LATIN_FONT, 0, "InjectedRegular", 400, false});
    // The loader deduplicates by (path, face index), so an application cannot
    // register the same file three times under different styles. The injected
    // style metadata is therefore only meaningful for distinct faces.
    request.preferred_fonts.push_back({RYNUI_VALIDATION_LATIN_FONT, 0, "InjectedRegularAgain", 700, false});
    request.fallback_latin = RYNUI_VALIDATION_LATIN_FONT;
    request.fallback_cjk = RYNUI_VALIDATION_CJK_FONT;

    auto chain = ryn::detail::load_default_ui_font_chain(*fonts, request);
    require(static_cast<bool>(chain), "injected styled chain did not load");
    // The first injected descriptor wins: a later duplicate of the same face must
    // not overwrite the style metadata of the face already registered.
    require(!chain.faces.empty() && chain.faces.front().custom_font &&
                chain.faces.front().family_name == std::string_view{"InjectedRegular"} &&
                chain.faces.front().weight == 400U && !chain.faces.front().italic,
            "duplicate injected face overwrote the registered style metadata");

    auto resolver = ryn::detail::make_default_ui_font_resolver(*fonts, chain, 1.0F);
    const auto resolved_regular = resolver(ryn::SystemFontFamily::ui_sans, 400, false, 14);
    const auto resolved_bold = resolver(ryn::SystemFontFamily::ui_sans, 700, false, 14);
    const auto resolved_italic = resolver(ryn::SystemFontFamily::ui_sans, 400, true, 14);
    require(!resolved_regular.empty() && !resolved_bold.empty() && !resolved_italic.empty(),
            "injected styled resolution returned an empty chain");
    for (const auto& chain_identities : {resolved_bold, resolved_italic}) {
        require(static_cast<bool>(fonts->find_glyph(chain_identities, U'A', std::nullopt)) &&
                    static_cast<bool>(fonts->find_glyph(chain_identities, U'中', std::nullopt)),
                "injected styled chain lost coverage");
    }
}

void test_custom_monospace_font_precedes_platform() {
    auto created = ryn::font::FontRuntime::create();
    require(static_cast<bool>(created), "Font Runtime initialization failed");
    auto fonts = std::move(created.runtime);

    ryn::detail::DefaultFontChainRequest request;
    request.raster = {14, 1.0F};
    request.preferred_monospace_fonts.push_back({
        RYNUI_VALIDATION_LATIN_FONT,
        0,
        "ConfiguredMonospaceFont",
    });
    request.preferred_fonts.push_back({RYNUI_VALIDATION_LATIN_FONT, 0, "ConfiguredUiFont"});
    request.fallback_latin = RYNUI_VALIDATION_LATIN_FONT;
    request.fallback_cjk = RYNUI_VALIDATION_CJK_FONT;

    auto chain = ryn::detail::load_default_ui_font_chain(*fonts, request);
    require(static_cast<bool>(chain), "custom monospace chain did not load");
    require(!chain.monospace_faces.empty() &&
                chain.monospace_faces.front().family_name == std::string_view{"ConfiguredMonospaceFont"},
            "configured monospace font did not precede the platform family");
    // A configured monospace face must not enter the UI chain.
    for (const auto& face : chain.faces) {
        require(face.family_name != std::string_view{"ConfiguredMonospaceFont"},
                "configured monospace font leaked into the UI chain");
    }
    require(chain.monospace_faces.front().identity == chain.faces.front().identity,
            "shared UI/monospace font file was loaded with a different identity");
    auto resolver = ryn::detail::make_default_ui_font_resolver(*fonts, chain, 1.0F);
    const auto mono = resolver(ryn::SystemFontFamily::ui_monospace, 400, false, 16);
    const auto ui = resolver(ryn::SystemFontFamily::ui_sans, 400, false, 16);
    require(!ui.empty() && !mono.empty() && static_cast<bool>(fonts->find_glyph(mono, U'\uFFFD', std::nullopt)),
            "shared preferred font lost its order or replacement coverage after resizing");
}

} // namespace

int main() {
    try {
        test_default_ui_font_chain();
        test_monospace_chain_resolution();
        test_weight_and_slant_face_selection();
        test_injected_styled_faces();
        test_custom_monospace_font_precedes_platform();
        test_custom_font_precedes_platform_defaults();
        test_invalid_custom_font_fails_fast();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
