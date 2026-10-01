#include <ryn/theme.hpp>

#include <array>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

static_assert(!std::is_assignable_v<decltype(ryn::ThemeConfig{}.seed.color_primary)&, const char*>);
static_assert(!std::is_assignable_v<decltype(ryn::ThemeConfig{}.button.tokens.padding_inline)&, float>);

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Operation>
void require_invalid(Operation&& operation, const char* message) {
    try {
        operation();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(message);
}

void test_default_parity() {
    const auto snapshot = ryn::resolve_theme();
    require(snapshot.source_version() == "6.6.5"
                && snapshot.source_commit()
                    == "4a39f54842eade4e565ab336ef6097cd7e723cdd",
            "ThemeSnapshot source identity drifted");
    require(snapshot.algorithms().size() == 1
                && snapshot.algorithms()[0] == ryn::ThemeAlgorithm::Default,
            "default algorithm chain drifted");
    const auto& map = snapshot.map();
    require(map.color_primary == ryn::Color::rgba8(22, 119, 255)
                && map.color_primary_hover == ryn::Color::rgba8(64, 150, 255)
                && map.color_primary_active == ryn::Color::rgba8(9, 88, 217)
                && map.color_primary_border == ryn::Color::rgba8(145, 202, 255),
            "Default primary palette parity drifted");
    require(map.font_size_small == 12.0F && map.font_size == 14.0F
                && map.font_size_large == 16.0F
                && map.size_xs == 8.0F && map.size_small == 12.0F
                && map.size == 16.0F && map.size_large == 24.0F
                && map.control_height_small == 24.0F
                && map.control_height == 32.0F
                && map.control_height_large == 40.0F
                && map.motion_unit == ryn::Duration::milliseconds(100.0F)
                && map.motion_base == ryn::Duration{} && map.motion,
            "Default font, spacing, or control map parity drifted");
    require(snapshot.alias().color_text == ryn::Color(0.0F, 0.0F, 0.0F, 0.88F)
                && snapshot.alias().color_background_container
                    == ryn::Color::rgba8(255, 255, 255)
                && snapshot.alias().color_border == ryn::Color::rgba8(217, 217, 217)
                && snapshot.alias().line_width_focus == 3.0F
                && snapshot.alias().focus_outline_offset == 1.0F,
            "Default Alias projection drifted");
    require(snapshot.button().control_height == 32.0F
                && snapshot.button().padding_inline == 15.0F
                && snapshot.button().border_radius == 6.0F
                && snapshot.button().primary_background == map.color_primary,
            "Default Button component token drifted");
    require(snapshot.text().color == snapshot.alias().color_text
                && snapshot.text().font_family == ryn::SystemFontFamily::ui_sans
                && snapshot.text().font_weight == 400
                && snapshot.text().font_size == 14.0F
                && snapshot.text().line_height == 22.0F,
            "Default Text component token drifted");
}

void test_semantic_text_and_link_colors() {
    const auto snapshot = ryn::resolve_theme();
    const auto& map = snapshot.map();
    // Derived from palette keys 8/9/10, which are the dark-side relative steps
    // 2/3/4 because `palette_variant` steps away from the seed at key 6.
    require(map.color_success_text == ryn::Color::rgba8(19, 82, 0)
                && map.color_warning_text == ryn::Color::rgba8(135, 77, 0)
                && map.color_error_text == ryn::Color::rgba8(140, 21, 35),
            "Semantic text colors are not opaque dark-side palette keys");
    for (const auto color : {map.color_success_text, map.color_warning_text,
             map.color_error_text, map.color_link}) {
        require(color.alpha() == 1.0F,
                "Semantic text color lost its opacity");
        const auto luma = 0.2126F * color.red() + 0.7152F * color.green()
            + 0.0722F * color.blue();
        require(luma < 0.8F, "Semantic text color is too light to read as text");
    }
    require(map.color_link == ryn::Color::rgba8(23, 120, 255)
                && map.color_link_hover == ryn::Color::rgba8(0, 62, 179)
                && map.color_link_active == ryn::Color::rgba8(0, 29, 102),
            "Link palette drifted");
    // `colorLink` falls back to `colorInfo`, whose own palette entry is cyan.
    require(map.color_link != map.color_info || map.color_info == ryn::Color::rgba8(19, 194, 194),
            "colorLink fallback to colorInfo is inconsistent");

    ryn::ThemeConfig explicit_link;
    explicit_link.seed.color_link = ryn::Color::rgba8(255, 0, 0);
    const auto linked = ryn::resolve_theme(explicit_link);
    require(linked.map().color_link != map.color_link
                && linked.map().color_link.alpha() == 1.0F,
            "explicit colorLink seed override was ignored");

    ryn::ThemeConfig dark_config;
    dark_config.algorithms = {ryn::ThemeAlgorithm::Dark};
    const auto dark = ryn::resolve_theme(dark_config);
    require(dark.map().color_success_text == ryn::Color::rgba8(19, 73, 3)
                && dark.map().color_error_text == ryn::Color::rgba8(122, 21, 33)
                && dark.map().color_link == ryn::Color::rgba8(23, 105, 220),
            "dark semantic text colors did not repaint onto the dark surface");
    // Legacy palette values must not move: only the new text colors were added.
    require(dark.map().color_error != map.color_error
                && dark.map().color_error_hover != map.color_error_hover,
            "dark algorithm no longer repaints the legacy error palette");
    require(map.color_error_hover == ryn::Color::rgba8(255, 120, 117)
                && map.color_primary_hover == ryn::Color::rgba8(64, 150, 255),
            "legacy light palette steps changed");
}

void test_semantic_text_color_edges() {
    // Grayscale seeds take the `grayscale` branch of the palette generator, which
    // keeps hue and saturation untouched instead of clamping them.
    const auto grayscale = [](std::uint8_t channel) {
        ryn::ThemeConfig config;
        config.seed.color_success = ryn::Color::rgba8(channel, channel, channel);
        config.seed.color_warning = ryn::Color::rgba8(channel, channel, channel);
        config.seed.color_error = ryn::Color::rgba8(channel, channel, channel);
        return ryn::resolve_theme(config).map();
    };
    const auto gray = grayscale(128);
    require(gray.color_success_text == ryn::Color::rgba8(13, 13, 13)
                && gray.color_warning_text == gray.color_success_text
                && gray.color_error_text == gray.color_success_text
                && gray.color_success_text.alpha() == 1.0F,
            "grayscale seed did not stay on the grayscale palette branch");
    const auto black = grayscale(0);
    require(black.color_success_text == ryn::Color::rgba8(0, 0, 0),
            "black seed produced an unexpected semantic text color");

    // Fully saturated boundary seeds must stay opaque and inside the gamut.
    ryn::ThemeConfig saturated;
    saturated.seed.color_success = ryn::Color::rgba8(255, 0, 0);
    const auto red = ryn::resolve_theme(saturated).map();
    require(red.color_success_text == ryn::Color::rgba8(140, 0, 14),
            "saturated red seed produced an unexpected semantic text color");

    // Semantic text colors are not derived through `color_split` or the alias
    // override path, so overriding the split colour leaves them untouched.
    const auto baseline = ryn::resolve_theme();
    ryn::ThemeConfig split;
    split.alias.color_split = ryn::Color::rgba8(1, 1, 1);
    const auto overridden = ryn::resolve_theme(split);
    require(overridden.alias().color_split == ryn::Color::rgba8(1, 1, 1)
                && overridden.divider().colors.line == ryn::Color::rgba8(1, 1, 1)
                && overridden.map().color_success_text
                    == baseline.map().color_success_text,
            "colorSplit override did not stay confined to the split tokens");
}

void test_typography_and_divider_defaults() {
    const auto snapshot = ryn::resolve_theme();
    const auto& typography = snapshot.typography();
    require(typography.headings[0].font_size == 38.0F
                && typography.headings[1].font_size == 30.0F
                && typography.headings[2].font_size == 24.0F
                && typography.headings[3].font_size == 20.0F
                && typography.headings[4].font_size == 16.0F,
            "Typography heading size chain drifted from 38/30/24/20/16");
    require(typography.headings[0].line_height > 53.19F
                && typography.headings[0].line_height < 53.21F
                && typography.headings[2].line_height > 31.19F
                && typography.headings[2].line_height < 31.21F,
            "Typography heading line heights drifted");
    require(typography.title_margin_top_em == 1.2F
                && typography.title_margin_bottom_em == 0.5F,
            "Typography title margins drifted from the 1.2em/0.5em ratios");
    require(typography.font_weight == 400 && typography.font_weight_strong == 600
                && typography.font_family == ryn::SystemFontFamily::ui_sans
                && typography.font_family_code == ryn::SystemFontFamily::ui_monospace,
            "Typography font defaults drifted");
    require(typography.colors.text == snapshot.alias().color_text
                && typography.colors.description == snapshot.alias().color_text_secondary
                && typography.colors.disabled == snapshot.alias().color_text_disabled
                && typography.colors.mark_background == ryn::Color::rgba8(255, 229, 143),
            "Typography semantic colors are not bound to the alias tokens");
    require(typography.code.background == ryn::Color(0.588F, 0.588F, 0.588F, 0.1F)
                && typography.code.font_scale == 0.85F
                && typography.keyboard.font_scale == 0.9F
                && typography.keyboard.border_bottom_width == 2.0F
                && typography.code.border_bottom_width == 1.0F,
            "inline code/keyboard metrics drifted from the locked reference");

    const auto& divider = snapshot.divider();
    require(divider.colors.line == snapshot.alias().color_split
                && divider.colors.text == snapshot.alias().color_text
                && divider.colors.plain_text == snapshot.alias().color_text,
            "Divider colors are not bound to the alias tokens");
    require(divider.metrics.line_width == snapshot.seed().line_width
                && divider.metrics.orientation_margin == 0.05F
                && divider.metrics.text_padding_inline == 16.0F
                && divider.metrics.vertical_margin_inline == 8.0F
                && divider.metrics.horizontal_margin == 24.0F
                && divider.metrics.horizontal_with_text_margin == 16.0F,
            "Divider metrics drifted from the locked Component Token values");
    require(divider.typography.text_font_size == 16.0F
                && divider.typography.text_font_weight == 500
                && divider.typography.plain_font_size == 14.0F
                && divider.typography.plain_font_weight == 400,
            "Divider typography drifted");

    // Compact must scale spacing from the algorithm instead of freezing 24/16/8.
    ryn::ThemeConfig compact_config;
    compact_config.algorithms = {ryn::ThemeAlgorithm::Compact};
    const auto compact = ryn::resolve_theme(compact_config);
    require(compact.divider().metrics.horizontal_margin
                == compact.map().size_large
                && compact.divider().metrics.vertical_margin_inline
                    == compact.map().size_xs
                && compact.divider().metrics.horizontal_margin != 24.0F
                && compact.divider().metrics.line_width == 1.0F,
            "Divider spacing did not follow the Compact algorithm");
    require(compact.typography().headings[0].font_size == 33.0F
                && compact.typography().headings[4].font_size == 14.0F,
            "Typography headings did not follow the Compact base size");

    // `colorSplit` is the upstream alpha solve of border-secondary on the
    // container, so its alpha changes with the algorithm while the composite
    // stays the 1px split colour.
    require(snapshot.alias().color_split == ryn::Color(5.0F / 255.0F, 5.0F / 255.0F,
                5.0F / 255.0F, 0.06F),
            "Default colorSplit drifted");
}

void test_focus_outline_seed() {
    const auto normal = ryn::resolve_theme();
    require(normal.seed().focus_outline && normal.alias().line_width_focus == 3.0F,
            "6.6.5 focusOutline default drifted");
    ryn::ThemeConfig config;
    config.seed.focus_outline = false;
    const auto hidden = ryn::resolve_theme(config);
    require(!hidden.seed().focus_outline && hidden.alias().line_width_focus == 0.0F
                && hidden.identity() != normal.identity(),
            "focusOutline=false did not disable the visible focus outline");
    const auto inherited = ryn::resolve_theme({}, &hidden);
    require(!inherited.seed().focus_outline && inherited.alias().line_width_focus == 0.0F,
            "focusOutline did not inherit through Theme");
}

void test_algorithm_composition() {
    ryn::ThemeConfig dark_config;
    dark_config.algorithms = {ryn::ThemeAlgorithm::Dark};
    const auto dark = ryn::resolve_theme(dark_config);
    require(dark.map().color_primary == ryn::Color::rgba8(22, 104, 220)
                && dark.map().color_primary_hover == ryn::Color::rgba8(60, 137, 232)
                && dark.alias().color_background_container == ryn::Color::rgba8(20, 20, 20)
                && dark.alias().color_text == ryn::Color(1.0F, 1.0F, 1.0F, 0.85F),
            "Dark algorithm parity drifted");

    ryn::ThemeConfig compact_config;
    compact_config.algorithms = {ryn::ThemeAlgorithm::Compact};
    const auto compact = ryn::resolve_theme(compact_config);
    require(compact.map().font_size_small == 10.0F
                && compact.map().font_size == 12.0F
                && compact.map().font_size_large == 14.0F
                && compact.map().size_xs == 4.0F
                && compact.map().size_small == 8.0F
                && compact.map().control_height == 28.0F
                && compact.map().control_height_small == 21.0F
                && compact.map().control_height_large == 35.0F
                && compact.button().padding_inline == 11.0F
                && compact.button().content_line_height == 20.0F,
            "Compact algorithm parity drifted");

    ryn::ThemeConfig dark_compact_config;
    dark_compact_config.algorithms = {
        ryn::ThemeAlgorithm::Dark, ryn::ThemeAlgorithm::Compact};
    const auto dark_compact = ryn::resolve_theme(dark_compact_config);
    ryn::ThemeConfig compact_dark_config;
    compact_dark_config.algorithms = {
        ryn::ThemeAlgorithm::Compact, ryn::ThemeAlgorithm::Dark};
    const auto compact_dark = ryn::resolve_theme(compact_dark_config);
    require(dark_compact.map() == compact_dark.map(),
            "independent Dark and Compact maps unexpectedly depend on order");
    require(dark_compact.algorithms()[0] == ryn::ThemeAlgorithm::Dark
                && compact_dark.algorithms()[0] == ryn::ThemeAlgorithm::Compact
                && dark_compact.identity() != compact_dark.identity(),
            "declared algorithm order was lost from snapshot identity");
}

void test_overrides_and_component_algorithm() {
    ryn::ThemeConfig brand;
    brand.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
    brand.seed.font_size = ryn::dp(16.0F);
    brand.seed.size_unit = ryn::dp(5.0F);
    brand.alias.color_text = ryn::Color::rgba8(32, 32, 32);
    brand.button.tokens.padding_inline = ryn::dp(19.0F);
    brand.button.tokens.primary_color = ryn::Color::rgba8(255, 255, 0);
    const auto branded = ryn::resolve_theme(brand);
    require(branded.seed().color_primary == ryn::Color::rgba8(114, 46, 209)
                && branded.map().font_size == 16.0F
                && branded.map().size == 20.0F
                && branded.alias().color_text == ryn::Color::rgba8(32, 32, 32)
                && branded.button().padding_inline == 19.0F
                && branded.button().primary_color == ryn::Color::rgba8(255, 255, 0),
            "typed Seed, Alias, or Button override precedence drifted");

    ryn::ThemeConfig component;
    component.button.algorithm = true;
    component.button.seed.color_primary = ryn::Color::rgba8(82, 196, 26);
    const auto component_snapshot = ryn::resolve_theme(component);
    require(component_snapshot.map().color_primary == ryn::Color::rgba8(22, 119, 255)
                && component_snapshot.button().primary_background
                    == ryn::Color::rgba8(82, 196, 26)
                && component_snapshot.text().font_size == 14.0F,
            "component algorithm escaped Button scope");

    ryn::ThemeConfig text_component;
    text_component.text.algorithm = true;
    text_component.text.seed.font_size = ryn::dp(18.0F);
    text_component.text.tokens.font_weight = 500;
    const auto text_snapshot = ryn::resolve_theme(text_component);
    require(text_snapshot.text().font_size == 18.0F
                && text_snapshot.text().line_height == 26.0F
                && text_snapshot.text().font_weight == 500
                && text_snapshot.map().font_size == 14.0F
                && text_snapshot.button().content_font_size == 14.0F,
            "Text component algorithm escaped its component owner");

    component.button.algorithm = false;
    const auto disabled = ryn::resolve_theme(component);
    require(disabled.button().primary_background == disabled.map().color_primary,
            "disabled component algorithm changed Button derivation");

    ryn::ThemeConfig explicit_token = component;
    explicit_token.button.algorithm = true;
    explicit_token.button.tokens.primary_background = ryn::Color::rgba8(250, 173, 20);
    const auto precedence = ryn::resolve_theme(explicit_token);
    require(precedence.button().primary_background == ryn::Color::rgba8(250, 173, 20),
            "explicit component token did not override component algorithm");
}

void test_inheritance_diagnostics_and_atomic_failure() {
    ryn::ThemeConfig parent_config;
    parent_config.algorithms = {ryn::ThemeAlgorithm::Dark};
    parent_config.alias.color_text = ryn::Color::rgba8(200, 200, 200);
    const auto parent = ryn::resolve_theme(parent_config);
    const auto child = ryn::resolve_theme({}, &parent);
    require(std::ranges::equal(child.algorithms(), parent.algorithms())
                && child.alias().color_text == parent.alias().color_text,
            "nested Theme default inheritance drifted");

    ryn::ThemeConfig component_parent_config;
    component_parent_config.button.algorithm = true;
    component_parent_config.button.seed.color_primary = ryn::Color::rgba8(82, 196, 26);
    const auto component_parent = ryn::resolve_theme(component_parent_config);
    ryn::ThemeConfig nested_button_override;
    nested_button_override.button.tokens.padding_inline = ryn::dp(21.0F);
    const auto component_child = ryn::resolve_theme(
        nested_button_override, &component_parent);
    require(component_child.button().primary_background
                    == component_parent.button().primary_background
                && component_child.button().padding_inline == 21.0F,
            "nested component override lost inherited component algorithm values");

    ryn::ThemeConfig reset;
    reset.inherit = false;
    const auto reset_snapshot = ryn::resolve_theme(reset, &parent);
    require(reset_snapshot.algorithms()[0] == ryn::ThemeAlgorithm::Default
                && reset_snapshot.alias().color_text
                    == ryn::Color(0.0F, 0.0F, 0.0F, 0.88F),
            "inherit=false did not reset to Default Seed");

    const auto duplicate = ryn::resolve_theme(parent_config);
    require(parent == duplicate && parent.identity() == duplicate.identity()
                && parent.diagnostic_json() == duplicate.diagnostic_json(),
            "identical Theme input did not produce a byte-identical snapshot");
    require(parent.identity() != reset_snapshot.identity()
                && parent.diagnostic_json() != reset_snapshot.diagnostic_json(),
            "different Theme input did not produce a diagnostic diff");
    require(parent.diagnostic_json().find("\"source\"") != std::string::npos
                && parent.diagnostic_json().find("\"algorithms\"") != std::string::npos
                && parent.diagnostic_json().find("\"identity\"") != std::string::npos,
            "ThemeSnapshot diagnostic is missing provenance");

    ryn::ThemeConfig invalid;
    invalid.seed.opacity_image = std::numeric_limits<float>::quiet_NaN();
    const auto old_identity = parent.identity();
    require_invalid([&] { static_cast<void>(ryn::resolve_theme(invalid, &parent)); },
                    "invalid override did not fail atomically");
    require(parent.identity() == old_identity,
            "failed Theme resolution mutated the parent snapshot");
    ryn::ThemeConfig invalid_switch;
    invalid_switch.switch_.tokens.handle_size = ryn::dp(100.0F);
    require_invalid([&] {
        static_cast<void>(ryn::resolve_theme(invalid_switch, &parent));
    }, "oversized Switch handle token did not fail atomically");
    require(parent.identity() == old_identity,
            "failed Switch token resolution mutated the parent snapshot");
}

[[nodiscard]] std::string read_golden(const char* name) {
    const std::string path = std::string(RYNUI_THEME_GOLDEN_DIRECTORY) + '/' + name + ".json";
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("unable to open Theme golden");
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void test_checked_in_goldens() {
    const auto check = [](const char* name, const ryn::ThemeConfig& config) {
        require(ryn::resolve_theme(config).diagnostic_json() == read_golden(name),
                "resolved Theme snapshot differs from its checked-in golden");
    };
    check("default", {});
    ryn::ThemeConfig dark;
    dark.algorithms = {ryn::ThemeAlgorithm::Dark};
    check("dark", dark);
    ryn::ThemeConfig compact;
    compact.algorithms = {ryn::ThemeAlgorithm::Compact};
    check("compact", compact);
    ryn::ThemeConfig dark_compact;
    dark_compact.algorithms = {ryn::ThemeAlgorithm::Dark, ryn::ThemeAlgorithm::Compact};
    check("dark-compact", dark_compact);
    ryn::ThemeConfig compact_dark;
    compact_dark.algorithms = {ryn::ThemeAlgorithm::Compact, ryn::ThemeAlgorithm::Dark};
    check("compact-dark", compact_dark);
}

void dump_goldens() {
    const auto dump = [](const char* name, const ryn::ThemeConfig& config) {
        std::cout << "=== " << name << " ===\n" << ryn::resolve_theme(config).diagnostic_json();
    };
    dump("default", {});
    ryn::ThemeConfig dark;
    dark.algorithms = {ryn::ThemeAlgorithm::Dark};
    dump("dark", dark);
    ryn::ThemeConfig compact;
    compact.algorithms = {ryn::ThemeAlgorithm::Compact};
    dump("compact", compact);
    ryn::ThemeConfig dark_compact;
    dark_compact.algorithms = {ryn::ThemeAlgorithm::Dark, ryn::ThemeAlgorithm::Compact};
    dump("dark-compact", dark_compact);
    ryn::ThemeConfig compact_dark;
    compact_dark.algorithms = {ryn::ThemeAlgorithm::Compact, ryn::ThemeAlgorithm::Dark};
    dump("compact-dark", compact_dark);
}

void write_goldens() {
    const auto write = [](const char* name, const ryn::ThemeConfig& config) {
        const std::string path = std::string(RYNUI_THEME_GOLDEN_DIRECTORY) + '/' + name + ".json";
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("unable to write Theme golden");
        output << ryn::resolve_theme(config).diagnostic_json();
        if (!output) throw std::runtime_error("unable to finish Theme golden");
    };
    write("default", {});
    ryn::ThemeConfig dark;
    dark.algorithms = {ryn::ThemeAlgorithm::Dark};
    write("dark", dark);
    ryn::ThemeConfig compact;
    compact.algorithms = {ryn::ThemeAlgorithm::Compact};
    write("compact", compact);
    ryn::ThemeConfig dark_compact;
    dark_compact.algorithms = {ryn::ThemeAlgorithm::Dark, ryn::ThemeAlgorithm::Compact};
    write("dark-compact", dark_compact);
    ryn::ThemeConfig compact_dark;
    compact_dark.algorithms = {ryn::ThemeAlgorithm::Compact, ryn::ThemeAlgorithm::Dark};
    write("compact-dark", compact_dark);
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string_view(argv[1]) == "--write-goldens") {
            write_goldens();
            return 0;
        }
        if (argc > 1) {
            dump_goldens();
            return 0;
        }
        test_default_parity();
        test_semantic_text_and_link_colors();
        test_semantic_text_color_edges();
        test_typography_and_divider_defaults();
        test_focus_outline_seed();
        test_algorithm_composition();
        test_overrides_and_component_algorithm();
        test_inheritance_diagnostics_and_atomic_failure();
        test_checked_in_goldens();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
