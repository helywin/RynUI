#pragma once
#include "component/window_component_services.hpp"

namespace ryn::detail {
// Retained local mutations also occur while an enclosing declaration is mounting.
inline void append_retained_slot(WindowComponentServices& services, runtime::ComponentId parent,
                                 const Content& content) {
    runtime::ComponentBuildContext* build{};
    try {
        build = &runtime::require_component_build_context();
    } catch (const std::logic_error&) {
    }
    if (build && &build->host() == &services.components()) {
        build->mount_slot_with_theme_scope(
            Content{[&] { runtime::require_component_build_context().mount_slot(parent, content); }},
            services.components().theme_scope(parent));
    } else {
        services.append_slot(parent, content);
    }
}
} // namespace ryn::detail
