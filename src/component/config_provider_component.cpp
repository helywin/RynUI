#include "component/theme_scope_connection.hpp"
#include "runtime/prop_connection.hpp"

#include <ryn/config_provider.hpp>

namespace ryn::detail {

struct ConfigProviderPropsAccess final {
    template <typename T> static std::optional<T> read(const std::optional<Prop<T>>& value) {
        return value ? std::optional<T>(read_prop(*value)) : std::nullopt;
    }

    static runtime::ConfigOverrides read(const ConfigProviderProps& props) {
        return {read(props.locale_),    read(props.format_locale_), read(props.catalog_), read(props.messages_),
                read(props.direction_), read(props.size_),          read(props.disabled_)};
    }

    static const std::optional<Prop<ThemeConfig>>& theme(const ConfigProviderProps& props) {
        return props.theme_;
    }
};

} // namespace ryn::detail

namespace ryn {
namespace {

std::shared_ptr<runtime::ConfigScope> initialize_config(runtime::ComponentBuildContext& build,
                                                        const ConfigProviderProps& props) {
    const auto scope =
        runtime::ConfigScope::create(build.config_scope(), detail::ConfigProviderPropsAccess::read(props));
    build.lifetime_scope().on_cleanup([scope] { scope->dispose(); });
    const std::weak_ptr<runtime::ConfigScope> weak_scope = scope;
    // Deliberately use connect_binding, whose apply has no pre-success cache.
    // An invalid custom binding is evaluated and rejected on EVERY submission.
    const auto values = bind([weak_scope, props] {
        const auto live = weak_scope.lock();
        if (!live) {
            throw I18nException({I18nErrorCode::StaleContext, {}, {}});
        }
        const auto generation = live->generation();
        auto overrides = detail::ConfigProviderPropsAccess::read(props);
        live->ensure_live();
        return std::pair{generation, std::move(overrides)};
    });
    static_cast<void>(connect_binding(build.lifetime_scope(), values, [weak_scope](auto next) {
        if (const auto live = weak_scope.lock(); live && live->active() && live->generation() == next.first) {
            static_cast<void>(live->update(std::move(next.second)));
        }
    }));
    return scope;
}

} // namespace

void ConfigProvider(ConfigProviderProps props, ConfigProviderContent content) {
    auto& build = runtime::require_component_build_context();
    const auto scope = initialize_config(build, props);
    if (const auto& theme = detail::ConfigProviderPropsAccess::theme(props)) {
        const auto theme_scope = detail::initialize_theme_scope(build, *theme);
        build.mount_slot_with_config_scope(ConfigProviderContent{[theme_scope, content] {
                                               auto& inner = runtime::require_component_build_context();
                                               inner.mount_slot_with_theme_scope(content, theme_scope);
                                           }},
                                           scope);
    } else {
        build.mount_slot_with_config_scope(content, scope);
    }
}

} // namespace ryn
