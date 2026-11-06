#pragma once

#include "component/localized_prop.hpp"

#include <ryn/date_picker.hpp>

namespace ryn::detail {

[[nodiscard]] inline String picker_placeholder(const PickerLocale& locale, DatePickerMode mode,
                                               std::optional<PickerEndpoint> endpoint, bool time) {
    if (endpoint) {
        const auto index = static_cast<std::size_t>(*endpoint);
        return mode == DatePickerMode::Year      ? locale.year_range_placeholder[index]
               : mode == DatePickerMode::Quarter ? locale.quarter_range_placeholder[index]
               : mode == DatePickerMode::Month   ? locale.month_range_placeholder[index]
               : mode == DatePickerMode::Week    ? locale.week_range_placeholder[index]
                                                 : locale.range_placeholder[index];
    }
    return mode == DatePickerMode::Year      ? locale.year_placeholder
           : mode == DatePickerMode::Quarter ? locale.quarter_placeholder
           : mode == DatePickerMode::Month   ? locale.month_placeholder
           : mode == DatePickerMode::Week    ? locale.week_placeholder
           : time                            ? locale.date_time_placeholder
                                             : locale.date_placeholder;
}

[[nodiscard]] inline Prop<PickerLocale> picker_locale_prop(const std::optional<Prop<PickerLocale>>& explicit_locale,
                                                           std::shared_ptr<runtime::ConfigScope> config) {
    return localized_prop(explicit_locale, std::move(config), [](const LocaleData& data) { return data.picker; });
}

// A complete component Locale overrides both text and Gregorian formatting.
// Otherwise formatLocale is independent of the language used by panel actions.
[[nodiscard]] inline Prop<PickerLocale>
picker_format_locale_prop(const std::optional<Prop<PickerLocale>>& explicit_locale,
                          std::shared_ptr<runtime::ConfigScope> config) {
    if (explicit_locale) {
        return *explicit_locale;
    }
    return bind([config = std::move(config)] { return config->snapshot().format_locale.pack.data().picker; });
}

} // namespace ryn::detail
