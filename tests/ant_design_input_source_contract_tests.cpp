#include "theme/input_tokens.hpp"

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
}
int main() {
    try { defaults(); customized_seed(); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
