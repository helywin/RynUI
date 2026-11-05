#pragma once

#include "component/window_component_services.hpp"

namespace ryn::detail {

// Only Form's captured ref adapter adds visibility to the normal ref contract.
struct FormFocusContext final {
    WindowComponentServices& window;
    runtime::Size viewport;
    runtime::Rect document_clip;

    [[nodiscard]] std::optional<FormControlGeometry> geometry(const WindowComponentServices& owner,
                                                              runtime::ComponentId id) const {
        if (&owner != &window || !window.components().contains(id)) {
            return {};
        }
        const auto node = window.components().root(id);
        const auto& value = window.nodes().require(node);
        const auto clip = window.nodes().content_clip(node, window.component_clip(id, viewport, document_clip));
        return FormControlGeometry{{value.bounds.x + value.translation.x, value.bounds.y + value.translation.y,
                                    value.bounds.width, value.bounds.height},
                                   {clip.x, clip.y, clip.width, clip.height},
                                   window.focus().state().window_active && window.interactions().component_active(id)};
    }

    [[nodiscard]] bool visible(const WindowComponentServices& owner, runtime::ComponentId id) const {
        if (&owner != &window || !window.components().contains(id) || !window.focus().state().window_active ||
            !window.interactions().component_active(id)) {
            return false;
        }
        const auto node = window.components().root(id);
        const auto& value = window.nodes().require(node);
        const auto clip = window.nodes().content_clip(node, window.component_clip(id, viewport, document_clip));
        const runtime::Rect bounds{value.bounds.x + value.translation.x, value.bounds.y + value.translation.y,
                                   value.bounds.width, value.bounds.height};
        return bounds.width > 0 && bounds.height > 0 && clip.width > 0 && clip.height > 0 &&
               bounds.x < clip.x + clip.width && bounds.x + bounds.width > clip.x && bounds.y < clip.y + clip.height &&
               bounds.y + bounds.height > clip.y;
    }
};

} // namespace ryn::detail
