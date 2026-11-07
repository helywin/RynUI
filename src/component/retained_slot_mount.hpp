#pragma once

#include "component/window_component_services.hpp"

namespace ryn::detail {

inline void mount_retained_slot(WindowComponentServices& services, runtime::ComponentId parent,
                                const Content& content) {
    auto& components = services.components();
    if (auto* context = runtime::current_component_build_context(); context && &context->host() == &components) {
        context->mount_slot_from_parent(parent, content);
    } else {
        services.append_slot(parent, content, components.capture_context(parent));
    }
}

} // namespace ryn::detail
