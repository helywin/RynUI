#pragma once

#include "component/window_component_services.hpp"
#include <algorithm>
#include <optional>
#include <stdexcept>

namespace ryn_test {

inline std::optional<ryn::runtime::ComponentId> find_bundled_icon(ryn::detail::WindowComponentServices& services,
                                                                  ryn::runtime::ComponentId owner, ryn::IconName name,
                                                                  bool visible = true) {
    for (const auto& mounted : services.text().mounted_texts()) {
        bool inside = false;
        for (auto current = std::optional{mounted.component}; current;
             current = services.components().parent(*current)) {
            if (*current == owner) {
                inside = true;
                break;
            }
        }
        if (!inside) {
            continue;
        }
        try {
            const auto icon = services.text().icon_snapshot(mounted.component);
            if (icon.source.bundled_name() == name && icon.visible == visible) {
                return mounted.component;
            }
        } catch (const std::out_of_range&) {
            // Ordinary Text children are intentionally kept alongside controls' icons.
        }
    }
    return std::nullopt;
}

inline ryn::runtime::ComponentId require_bundled_icon(ryn::detail::WindowComponentServices& services,
                                                      ryn::runtime::ComponentId owner, ryn::IconName name) {
    const auto component = find_bundled_icon(services, owner, name);
    if (!component) {
        throw std::runtime_error("Control did not use the required bundled Icon source");
    }
    const auto icon = services.text().icon_snapshot(*component);
    const auto fonts = services.text().resolve_fonts(services.text().resolved_typography(*component));
    if (icon.layers.empty()) {
        throw std::runtime_error("Bundled control Icon has no retained glyph layer");
    }
    for (auto layer : icon.layers) {
        const auto& shaped = services.text().scene_service().text_state(layer).shaped();
        if (shaped.glyphs.empty() || std::ranges::any_of(shaped.glyphs, [&](const auto& glyph) {
                return glyph.glyph_id == 0 || std::ranges::find(fonts, glyph.font) != fonts.end();
            })) {
            throw std::runtime_error("Control Icon used a missing glyph or ordinary text fallback font");
        }
    }
    return *component;
}

} // namespace ryn_test
