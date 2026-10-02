#pragma once

#include <ryn/icon.hpp>

#include <span>
#include <stdexcept>
#include <string_view>

namespace ryn::detail {

struct BundledIconLayer final {
    char32_t codepoint;
    bool secondary;
    float opacity;
};

struct BundledIconEntry final {
    std::uint16_t first_layer;
    std::uint8_t layer_count;
    std::string_view name;
};

#include "icons/ant_design_icon_catalog.inc"

static_assert(std::size(bundled_icon_entries) == bundled_icon_count);

[[nodiscard]] inline const BundledIconEntry& bundled_icon_entry(IconName name) {
    const auto index = static_cast<std::size_t>(name);
    if (index >= bundled_icon_count) {
        throw std::invalid_argument("Icon name is outside the bundled catalog");
    }
    return bundled_icon_entries[index];
}

[[nodiscard]] inline std::span<const BundledIconLayer> bundled_layers(IconName name) {
    const auto& entry = bundled_icon_entry(name);
    return std::span{bundled_icon_layers}.subspan(entry.first_layer, entry.layer_count);
}

} // namespace ryn::detail
