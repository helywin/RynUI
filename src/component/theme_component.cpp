#include "runtime/component_host.hpp"
#include "runtime/prop_connection.hpp"

#include <ryn/theme.hpp>

#include <memory>
#include <utility>

namespace ryn::detail {

struct ThemePropsAccess final {
    [[nodiscard]] static const Prop<ThemeConfig>& config(const ThemeProps& props) noexcept {
        return props.config_;
    }
};

} // namespace ryn::detail

namespace ryn {

namespace {

// Finish the large configuration work before mounting nested content. Otherwise
// each transparent Theme retains its configuration temporaries on the stack.
std::shared_ptr<theme_runtime::ThemeScope> initialize_scope(runtime::ComponentBuildContext& build,
                                                            const ThemeProps& props) {
    const auto scope = theme_runtime::ThemeScope::create(build.theme_scope(),
                                                         detail::read_prop(detail::ThemePropsAccess::config(props)));
    const std::weak_ptr<theme_runtime::ThemeScope> weak_scope = scope;
    if (const auto* binding = detail::PropAccess::binding(detail::ThemePropsAccess::config(props))) {
        // Finish evaluating the large value before resolving a snapshot, keeping
        // Binding's value temporaries out of the nested Theme resolution stack.
        const auto updates = bind([binding = *binding] { return std::make_shared<const ThemeConfig>(binding.get()); });
        static_cast<void>(connect_binding(
            build.lifetime_scope(), updates,
            [weak_scope, last_value = std::shared_ptr<const ThemeConfig>{}](const auto& config) mutable {
                if (last_value && *last_value == *config) {
                    return;
                }
                last_value = config;
                if (const auto live_scope = weak_scope.lock()) {
                    static_cast<void>(live_scope->update(*config));
                }
            }));
    }
    return scope;
}

} // namespace

void Theme(ThemeProps props, ThemeContent content) {
    auto& build = runtime::require_component_build_context();
    const auto scope = initialize_scope(build, props);
    build.mount_slot_with_theme_scope(content, scope);
}

} // namespace ryn
