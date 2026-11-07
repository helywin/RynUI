#pragma once

#include <ryn/i18n.hpp>

namespace ryn::detail {

void validate_number_locale(const StatisticLocale& locale);
[[nodiscard]] String number_to_text(const I18nNumber& value, bool fixed = false);
// Shared Statistic display algorithm. Non-decimal business strings pass through unchanged.
[[nodiscard]] String format_decimal_text(const String& raw, const StatisticLocale& locale,
                                         const NumberFormatOptions& options = {});
// InputNumber does not group its editable text. A custom parser bypasses this conversion.
[[nodiscard]] String parse_decimal_text(const String& text, const StatisticLocale& locale);

} // namespace ryn::detail
