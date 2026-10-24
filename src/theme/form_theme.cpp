#include "theme/form_theme.hpp"

#include <cmath>
#include <stdexcept>

namespace ryn::detail {
FormThemeToken derive_form(const AntDesignDefaultSeed& seed, const ThemeMapToken& map, const ThemeAliasToken& alias) {
    // Locked Ant Design 6.6.5 form/style/index.ts: prepareComponentToken and vertical label recipe.
    FormThemeToken result;
    const auto base = map.color_text_base;
    result.colors = {alias.color_text, map.color_error, Color(base.red(), base.green(), base.blue(), 0.45F),
                     map.color_error};
    result.metrics = {map.size_large,  map.size_xs, map.size_xs / 2, map.font_size, map.font_size * map.line_height,
                      seed.font_family};
    return result;
}

void apply_form_override(FormThemeToken& token, const FormTokenOverride& value) {
    if (value.label) {
        token.colors.label = *value.label;
    }
    if (value.required) {
        token.colors.required = *value.required;
    }
    if (value.help) {
        token.colors.help = *value.help;
    }
    if (value.error) {
        token.colors.error = *value.error;
    }
    const auto length = [](float& target, const std::optional<LogicalLength>& value, bool positive = false) {
        if (!value) {
            return;
        }
        if (value->is_auto() || !std::isfinite(value->value()) || value->value() < 0 ||
            (positive && value->value() == 0)) {
            throw std::invalid_argument("Form token length is invalid");
        }
        target = value->value();
    };
    length(token.metrics.item_margin_bottom, value.item_margin_bottom);
    length(token.metrics.label_gap, value.label_gap);
    length(token.metrics.required_gap, value.required_gap);
    length(token.metrics.font_size, value.font_size, true);
    length(token.metrics.line_height, value.line_height, true);
    if (value.font_family) {
        token.metrics.font_family = *value.font_family;
    }
    float extent = 0;
    for (auto metric : token.metrics.values()) {
        extent += metric * 2;
    }
    if (!std::isfinite(extent)) {
        throw std::invalid_argument("Form combined token extents must be finite");
    }
}
} // namespace ryn::detail
