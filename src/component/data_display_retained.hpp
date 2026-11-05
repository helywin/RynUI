#pragma once

#include "component/window_component_services.hpp"
#include "runtime/prop_connection.hpp"
#include "reactive/untracked.hpp"

namespace ryn::detail {
// Local DataDisplay slot operations. A failed application factory retires the
// exact owner generation; no partially appended interactive subtree survives.
inline void display_append(WindowComponentServices& services, runtime::ComponentId owner, runtime::ComponentId parent,
                           const Content& content, bool initial) {
    try {
        run_untracked([&] {
            if (initial) {
                runtime::require_component_build_context().mount_slot(parent, content);
            } else {
                services.append_slot(parent, content);
            }
        });
        if (!services.components().contains(owner)) {
            throw std::logic_error("DataDisplay owner was destroyed by its content factory");
        }
    } catch (...) {
        services.retire_failed_component(owner);
        throw;
    }
}

inline void display_append_styled(WindowComponentServices& services, runtime::ComponentId owner,
                                  runtime::ComponentId parent, const Content& content,
                                  const Signal<runtime::SemanticForeground>& color,
                                  const Signal<runtime::SemanticTypography>& font, bool initial) {
    display_append(services, owner, parent, Content{[&] {
                       auto& build = runtime::require_component_build_context();
                       build.mount_slot_with_semantic_text_style(parent, content,
                                                                 Prop<runtime::SemanticForeground>{color},
                                                                 Prop<runtime::SemanticTypography>{font});
                   }},
                   initial);
}

inline void display_clear(WindowComponentServices& services, runtime::ComponentId owner, runtime::ComponentId parent) {
    const auto children = services.components().children(parent);
    for (auto child : children) {
        services.destroy(child);
        if (!services.components().contains(owner)) {
            return;
        }
    }
}

inline input::InteractionId display_anchor(WindowComponentServices& services, runtime::ComponentId owner) {
    auto& build = runtime::require_component_build_context();
    input::InteractionId hit;
    build.mount_slot(owner, Content{[&] {
                         auto& context = runtime::require_component_build_context();
                         const auto component = context.mount_component<int>(0);
                         const auto node = context.root(component);
                         services.nodes().require(node).out_of_flow = true;
                         services.layout().set_layout(node, layout::LeafLayout{});
                         hit = services.interactions().create({component, node, {}, false, false});
                         context.on_resource_cleanup(component, [&services, node, hit] {
                             services.interactions().remove(hit);
                             services.layout().remove_layout(node);
                         });
                     }});
    return hit;
}

inline void display_reorder(WindowComponentServices& services, input::InteractionId anchor,
                            const std::vector<runtime::ComponentId>& regions) {
    std::vector<input::InteractionId> order;
    for (auto region : regions) {
        for (auto hit : services.interactions().declaration_order()) {
            for (auto ancestor = std::optional{services.interactions().require(hit).component}; ancestor;
                 ancestor = services.components().parent(*ancestor)) {
                if (*ancestor == region) {
                    order.push_back(hit);
                    break;
                }
            }
        }
    }
    services.interactions().reorder_after(anchor, order);
}

inline runtime::ComponentId display_region(WindowComponentServices& services, runtime::ComponentId parent,
                                           const Signal<runtime::SemanticForeground>& color,
                                           const Signal<runtime::SemanticTypography>& font, const Content& content) {
    auto& build = runtime::require_component_build_context();
    runtime::ComponentId result;
    build.mount_slot(parent, Content{[&] {
                         auto& context = runtime::require_component_build_context();
                         result = context.mount_component<int>(0);
                         const auto node = context.root(result);
                         services.nodes().require(node).clip_content = true;
                         services.layout().set_layout(node, layout::BoxLayout{});
                         context.on_resource_cleanup(result, [&services, node, result] {
                             services.interactions().set_input_blocked(result, false);
                             services.layout().remove_layout(node);
                         });
                         context.mount_slot_with_semantic_text_style(result, content,
                                                                     Prop<runtime::SemanticForeground>{color},
                                                                     Prop<runtime::SemanticTypography>{font});
                     }});
    return result;
}
} // namespace ryn::detail
