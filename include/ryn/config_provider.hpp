#pragma once

#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/i18n.hpp>
#include <ryn/text_direction.hpp>
#include <ryn/theme.hpp>

namespace ryn {
namespace detail {
struct ConfigProviderPropsAccess;
}

struct ConfigProviderContentSlot final {};

using ConfigProviderContent = SlotContent<ConfigProviderContentSlot>;

class ConfigProviderProps final {
public:
    ConfigProviderProps& locale(Prop<LocaleId> value) {
        locale_ = std::move(value);
        return *this;
    }

    ConfigProviderProps& formatLocale(Prop<LocaleId> value) {
        format_locale_ = std::move(value);
        return *this;
    }

    ConfigProviderProps& catalog(Prop<LocaleCatalog> value) {
        catalog_ = std::move(value);
        return *this;
    }

    ConfigProviderProps& messages(Prop<MessageCatalog> value) {
        messages_ = std::move(value);
        return *this;
    }

    ConfigProviderProps& direction(Prop<TextDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    ConfigProviderProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    ConfigProviderProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    ConfigProviderProps& theme(Prop<ThemeConfig> value) {
        theme_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::ConfigProviderPropsAccess;
    std::optional<Prop<LocaleId>> locale_;
    std::optional<Prop<LocaleId>> format_locale_;
    std::optional<Prop<LocaleCatalog>> catalog_;
    std::optional<Prop<MessageCatalog>> messages_;
    std::optional<Prop<TextDirection>> direction_;
    std::optional<Prop<ControlSize>> size_;
    std::optional<Prop<bool>> disabled_;
    std::optional<Prop<ThemeConfig>> theme_;
};

void ConfigProvider(ConfigProviderProps props, ConfigProviderContent content);

} // namespace ryn
