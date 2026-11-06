#include <ryn/theme.hpp>

#include <concepts>
#include <type_traits>

namespace {

template <typename Value>
concept ThemeConfigValue = requires(ryn::ThemeProps props, Value value) { props.config(value); };

static_assert(ThemeConfigValue<ryn::ThemeConfig>);
static_assert(ThemeConfigValue<ryn::Prop<ryn::ThemeConfig>>);
static_assert(ThemeConfigValue<ryn::Signal<ryn::ThemeConfig>>);
static_assert(!ThemeConfigValue<const char*>);
static_assert(!ThemeConfigValue<int>);
static_assert(!std::is_assignable_v<decltype(ryn::ThemeConfig{}.input.tokens.padding_inline)&, float>);
static_assert(!std::is_assignable_v<decltype(ryn::ThemeConfig{}.button.tokens.padding_inline_small)&, float>);
static_assert(!std::is_assignable_v<decltype(ryn::ThemeConfig{}.button.tokens.padding_inline_large)&, float>);
static_assert(!std::is_assignable_v<decltype(ryn::ThemeConfig{}.input.tokens.input_font_size)&, const char*>);
static_assert(!std::is_assignable_v<decltype(ryn::ThemeConfig{}.color_picker.tokens.panel_width)&, float>);
static_assert(std::same_as<decltype(ryn::resolve_theme().color_picker()), const ryn::ColorPickerThemeToken&>);
static_assert(!std::is_assignable_v<decltype(ryn::ThemeConfig{}.transfer.tokens.list_width)&, float>);
static_assert(!std::is_assignable_v<decltype(ryn::ThemeConfig{}.transfer.tokens.header)&, const char*>);
static_assert(std::same_as<decltype(ryn::resolve_theme().transfer()), const ryn::TransferThemeToken&>);
static_assert(!std::constructible_from<ryn::ThemeContent, ryn::Content>);
static_assert(!std::constructible_from<ryn::Content, ryn::ThemeContent>);
static_assert(std::is_same_v<decltype(&ryn::Theme), void (*)(ryn::ThemeProps, ryn::ThemeContent)>);

} // namespace

int main() {
    ryn::ThemeConfig config;
    config.algorithms = {ryn::ThemeAlgorithm::Dark, ryn::ThemeAlgorithm::Compact};
    ryn::ThemeProps props;
    props.config(config);
    ryn::ThemeContent content{[] {}};
    static_cast<void>(props);
    static_cast<void>(content);
    const auto baseline = ryn::resolve_theme();
    ryn::ThemeConfig button;
    button.button.tokens.padding_inline_small = ryn::dp(0);
    button.button.tokens.padding_inline_large = ryn::dp(3);
    const auto padded = ryn::resolve_theme(button);
    if (padded.button().padding_inline_small != 0 || padded.button().padding_inline_large != 3 ||
        padded.button().padding_inline != baseline.button().padding_inline ||
        padded.identity() == baseline.identity()) {
        return 1;
    }
    ryn::ThemeConfig picker;
    picker.color_picker.tokens.panel_width = ryn::dp(300);
    const auto metrics = ryn::resolve_theme(picker);
    picker.color_picker.tokens.focus = ryn::Color::rgba8(17, 34, 51);
    const auto colors = ryn::resolve_theme(picker);
    if (baseline.identity() == metrics.identity() || metrics.identity() == colors.identity() ||
        metrics.color_picker().panel_width != 300 || colors.color_picker().focus != ryn::Color::rgba8(17, 34, 51) ||
        baseline.select() != colors.select() || baseline.input_number() != colors.input_number()) {
        return 1;
    }
    return 0;
}
