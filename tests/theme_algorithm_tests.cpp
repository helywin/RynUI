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
    require(snapshot.source_version() == "6.5.0"
                && snapshot.source_commit()
                    == "740ad964dc2397f33e40944367b0536a7314cc32",
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

} // namespace

int main(int argc, char**) {
    try {
        if (argc > 1) {
            dump_goldens();
            return 0;
        }
        test_default_parity();
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
