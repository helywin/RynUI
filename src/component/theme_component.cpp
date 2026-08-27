#include "runtime/component_host.hpp"
#include "runtime/prop_connection.hpp"

#include <ryn/theme.hpp>

#include <memory>
#include <utility>

namespace ryn::detail {

struct ThemePropsAccess final {
    [[nodiscard]] static const Prop<ThemeConfig>& config(
        const ThemeProps& props) noexcept {
        return props.config_;
    }
};

} // namespace ryn::detail

namespace ryn {

void Theme(ThemeProps props, ThemeContent content) {
    auto& build = runtime::require_component_build_context();
    const auto scope = theme_runtime::ThemeScope::create(
        build.theme_scope(),
        detail::read_prop(detail::ThemePropsAccess::config(props)));
    const std::weak_ptr<theme_runtime::ThemeScope> weak_scope = scope;
    static_cast<void>(detail::connect_prop(
        build.lifetime_scope(),
        detail::ThemePropsAccess::config(props),
        [weak_scope](ThemeConfig config) {
            if (const auto live_scope = weak_scope.lock()) {
                static_cast<void>(live_scope->update(std::move(config)));
            }
        }));
    build.mount_slot_with_theme_scope(content, scope);
}

} // namespace ryn
