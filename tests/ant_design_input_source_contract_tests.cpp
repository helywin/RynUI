#include "theme/input_tokens.hpp"
#include "theme/theme_runtime.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
bool near(float a, float b) { return std::abs(a - b) < 0.0001F; }
void size(const detail::InputSizeTokens& actual, float height, float font, float line,
          float horizontal, float vertical, float radius) {
    const bool matches = near(actual.control_height, height) && near(actual.font_size, font)
        && near(actual.line_height, line) && near(actual.padding_inline, horizontal)
        && near(actual.padding_block, vertical) && near(actual.border_radius, radius);
    if(!matches) std::cerr << "actual: " << actual.control_height << ',' << actual.font_size << ','
        << actual.line_height << ',' << actual.padding_inline << ',' << actual.padding_block << ','
        << actual.border_radius << " expected: " << height << ',' << font << ',' << line << ','
        << horizontal << ',' << vertical << ',' << radius << '\n';
    require(matches, "Input source geometry differs from locked Ant Design");
}
void defaults() {
    const auto theme = resolve_theme();
    require(theme.source_version() == "6.5.0"
        && theme.source_commit() == "740ad964dc2397f33e40944367b0536a7314cc32", "Input source identity drift");
    const auto tokens = detail::derive_input_tokens(theme);
    size(tokens.size(ControlSize::Small), 24, 14, 22, 7, 0, 4);
    size(tokens.size(ControlSize::Middle), 32, 14, 22, 11, 4, 6);
    size(tokens.size(ControlSize::Large), 40, 16, 24, 11, 7, 8);
    require(tokens.affix_padding == 4 && tokens.border_width == 1, "Input affix/border contract drift");
    ThemeConfig dark; dark.algorithms = {ThemeAlgorithm::Dark};
    require(detail::derive_input_tokens(resolve_theme(dark)) == tokens, "Dark changed Input metrics");
    ThemeConfig compact; compact.algorithms = {ThemeAlgorithm::Compact};
    const auto small = detail::derive_input_tokens(resolve_theme(compact));
    size(small.size(ControlSize::Small), 21, 12, 20, 7, 0, 4);
    size(small.size(ControlSize::Middle), 28, 12, 20, 7, 3, 6);
    size(small.size(ControlSize::Large), 35, 14, 22, 11, 5.5F, 8);
    require(small.affix_padding == 4, "Compact affix gap incorrectly derived from sizeXS");
}
void customized_seed() {
    ThemeConfig config;
    config.seed.line_width = dp(2);
    config.seed.control_height = dp(33.3F);
    config.seed.size_unit = dp(5);
    const auto parent = resolve_theme(config);
    const auto tokens = detail::derive_input_tokens(parent);
    size(tokens.size(ControlSize::Small), 24.975F, 14, 22, 6, 0, 4);
    size(tokens.size(ControlSize::Middle), 33.3F, 14, 22, 13, 3.6F, 6);
    size(tokens.size(ControlSize::Large), 41.625F, 16, 24, 10, 6.9F, 8);
    require(tokens.affix_padding == 5, "Input custom sizeUnit ignored");
    require(detail::derive_input_tokens(resolve_theme({}, &parent)) == tokens, "Input nested seed inheritance lost");
    ThemeConfig text_only; text_only.text.tokens.font_size = dp(30);
    text_only.text.tokens.line_height = dp(40);
    require(detail::derive_input_tokens(resolve_theme(text_only, &parent)) == tokens,
        "Text component override leaked into Input component geometry");
    ThemeConfig isolated; isolated.inherit = false;
    require(detail::derive_input_tokens(resolve_theme(isolated, &parent))
        == detail::derive_input_tokens(resolve_theme()), "Input inherit=false retained parent metrics");
    bool rejected{};
    try { static_cast<void>(tokens.size(static_cast<ControlSize>(99))); }
    catch(const std::invalid_argument&) { rejected = true; }
    require(rejected, "Invalid Input token size accepted");
}
void overrides_and_identity() {
    using namespace theme_runtime;
    ThemeConfig config;
    config.input.tokens.padding_inline = dp(19);
    config.input.tokens.input_font_size = dp(18);
    config.input.tokens.border_radius = dp(9);
    config.input.tokens.affix_padding = dp(6);
    const auto parent = resolve_theme(config);
    const auto& tokens = detail::derive_input_tokens(parent);
    require(tokens.size(ControlSize::Middle).padding_inline == 19
        && tokens.size(ControlSize::Small).font_size == 18
        && tokens.size(ControlSize::Middle).font_size == 18
        && tokens.size(ControlSize::Large).font_size == 16
        && tokens.size(ControlSize::Middle).border_radius == 9 && tokens.affix_padding == 6,
        "Typed Input geometry override not resolved");
    require(parent.identity() != resolve_theme().identity() && parent != resolve_theme()
        && parent.diagnostic_json().find("\"input\":") != std::string::npos,
        "Input override missing from snapshot identity/diagnostics");
    ThemeConfig nested; nested.input.tokens.padding_inline_small = dp(13);
    const auto child = resolve_theme(nested, &parent);
    require(detail::derive_input_tokens(child).size(ControlSize::Middle).padding_inline == 19
        && detail::derive_input_tokens(child).size(ControlSize::Small).padding_inline == 13,
        "Nested Input override lost parent fields");
    config.input.tokens.padding_block = dp(6);
    config.input.tokens.input_font_size_small = dp(11);
    const auto explicit_parent = resolve_theme(config);
    nested = {}; nested.input.tokens.input_font_size = dp(20);
    const auto inherited = resolve_theme(nested, &explicit_parent);
    require(detail::derive_input_tokens(inherited).size(ControlSize::Middle).padding_block == 6
        && detail::derive_input_tokens(inherited).size(ControlSize::Small).font_size == 11,
        "Nested typography overwrote inherited explicit padding/small font");
    ThemeConfig algorithm; algorithm.input.algorithm = true;
    algorithm.input.seed.control_height = dp(48);
    const auto component_algorithm = resolve_theme(algorithm);
    require(detail::derive_input_tokens(component_algorithm).size(ControlSize::Middle).control_height == 48
        && component_algorithm.map().control_height == 32 && component_algorithm.button().control_height == 32,
        "Input component algorithm leaked into global/Button tokens");
    algorithm.input.algorithm = false;
    require(detail::derive_input_tokens(resolve_theme(algorithm)).size(ControlSize::Middle).control_height == 32,
        "Input algorithm=false consumed component seed");
    for(const auto invalid : {auto_length, dp(-1), dp(0)}) {
        ThemeConfig bad; bad.input.tokens.input_font_size = invalid;
        bool rejected{};
        try { static_cast<void>(resolve_theme(bad)); } catch(const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid Input font override accepted");
    }
    const auto scope = ThemeScope::create_default();
    int layout{}, typography{}, radius{};
    auto a = scope->capture([&](DirtyPhase p) {
        require(has_any(p, DirtyPhase::measure_layout) && has_any(p, DirtyPhase::hit_test), "Input layout phase incorrect"); ++layout;
    }, [&] { static_cast<void>(scope->input_layout_metrics()); });
    auto b = scope->capture([&](DirtyPhase p) { require(has_any(p, DirtyPhase::text), "Input font phase incorrect"); ++typography; },
        [&] { static_cast<void>(scope->input_typography()); });
    auto c = scope->capture([&](DirtyPhase p) {
        require(p == (DirtyPhase::geometry | DirtyPhase::paint_material), "Input radius phase incorrect"); ++radius;
    }, [&] { static_cast<void>(scope->input_border_radius()); });
    ThemeConfig next; next.input.tokens.padding_inline = dp(20);
    require(scope->update(next) && layout == 1 && typography == 0 && radius == 0, "Input padding notified unrelated tokens");
    require(!scope->update(next) && layout == 1, "Same Input override notified again");
    next.input.tokens.border_radius = dp(12); scope->update(next);
    require(layout == 1 && typography == 0 && radius == 1, "Input radius invalidated layout/text");
    next.input.tokens.input_font_size_large = dp(18); scope->update(next);
    require(layout == 2 && typography == 1 && radius == 1, "Input font failed to update computed padding");
    const auto identity = scope->snapshot().identity();
    auto invalid = next; invalid.input.tokens.padding_inline = auto_length;
    bool rejected{};
    try { scope->update(invalid); } catch(const std::invalid_argument&) { rejected = true; }
    require(rejected && scope->snapshot().identity() == identity && layout == 2 && typography == 1,
        "Invalid Input override partially published a Theme update");
    require(token_identity_name(TokenIdentity::input_typography) == "Input.typography", "Input identity name drifted");
}
}
int main() {
    try { defaults(); customized_seed(); overrides_and_identity(); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
