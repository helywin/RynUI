#include "theme/theme_runtime.hpp"
#include <ryn/theme.hpp>
#include <iostream>
#include <limits>

namespace {
void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
} // namespace

int main() {
    using namespace ryn;
    try {
        const auto initial = resolve_theme();
        check(initial.source_version() == "6.6.5" &&
                  initial.table().colors.header_background == Color::rgba8(250, 250, 250) &&
                  initial.table().colors.selected == Color::rgba8(230, 244, 255),
              "locked Table colors");
        check(initial.table().metrics.padding_block == 16 && initial.table().metrics.padding_block_middle == 12 &&
                  initial.table().metrics.padding_block_small == 8 &&
                  initial.table().metrics.padding_inline_small == 8 && initial.table().metrics.selection_width == 32,
              "locked Table metrics");
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        const auto dark_theme = resolve_theme(dark);
        check(dark_theme.table().colors.header_background == Color::rgba8(29, 29, 29) &&
                  dark_theme.table().colors.selected == Color::rgba8(17, 26, 44),
              "locked dark Table colors");
        ThemeConfig compact;
        compact.algorithms = {ThemeAlgorithm::Compact};
        check(resolve_theme(compact).table().metrics.padding_block < initial.table().metrics.padding_block,
              "compact Table padding");
        const auto scope = theme_runtime::ThemeScope::create_default();
        int color_calls{};
        int metric_calls{};
        auto colors = scope->capture(
            [&](auto phase) {
                check(phase == theme_runtime::DirtyPhase::paint_material, "Table color dirty phase");
                ++color_calls;
            },
            [&] { static_cast<void>(scope->table_colors()); });
        auto metrics =
            scope->capture([&](auto) { ++metric_calls; }, [&] { static_cast<void>(scope->table_metrics()); });
        ThemeConfig override;
        override.table.tokens.selected = Color::rgba8(1, 2, 3);
        check(scope->update(override) && color_calls == 1 && metric_calls == 0 &&
                  scope->snapshot().identity() != initial.identity(),
              "Table token isolation");
        const auto inherited = resolve_theme({}, &scope->snapshot());
        check(inherited.table().colors.selected == *override.table.tokens.selected, "Table nested inheritance");
        override.table.tokens.padding_block = dp(18);
        check(scope->update(override) && color_calls == 1 && metric_calls == 1, "Table metric isolation");
        const auto identity = scope->snapshot().identity();
        override.table.tokens.line_height = dp(0);
        bool rejected{};
        try {
            scope->update(override);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && scope->snapshot().identity() == identity, "Table invalid metric rollback");
        check(initial.diagnostic_json().find("\"table\"") != std::string::npos, "Table token serialization");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
