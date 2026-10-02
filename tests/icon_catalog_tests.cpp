#include "support/input_fixture.hpp"
#include "icons/bundled_icon_catalog.hpp"

#include <algorithm>
#include <iostream>
#include <set>

namespace {
void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void all_locked_layers_rasterize() {
    using namespace ryn;
    static_assert(sizeof(IconName) == sizeof(std::uint16_t));
    static_assert(static_cast<int>(IconName::EyeOutlined) == 0);
    static_assert(static_cast<int>(IconName::UpOutlined) == 13);
    ryn_test::input_component::Fixture fixture;
    const auto reference = fixture.resolve(48).front();
    const auto font = fixture.scene.icon_font(reference, 48);
    std::set<char32_t> codepoints;
    std::size_t outlined{};
    std::size_t filled{};
    std::size_t two_tone{};
    std::size_t maximum_layers{};
    for (std::size_t index = 0; index < bundled_icon_count; ++index) {
        const auto name = static_cast<IconName>(index);
        const auto& entry = detail::bundled_icon_entry(name);
        outlined += entry.name.ends_with("Outlined");
        filled += entry.name.ends_with("Filled");
        two_tone += entry.name.ends_with("TwoTone");
        const auto layers = detail::bundled_layers(name);
        require(!layers.empty() && layers.front().codepoint == 0xE000 + index, "catalog layer/index differs");
        maximum_layers = std::max(maximum_layers, layers.size());
        for (const auto& layer : layers) {
            require(codepoints.insert(layer.codepoint).second && layer.opacity > 0 && layer.opacity <= 1,
                    "layer identity/material differs");
            const auto lookup = fixture.fonts->glyph_index(font, layer.codepoint);
            require(static_cast<bool>(lookup) && lookup.glyph.glyph_id > 0, "catalog layer has no glyph");
            const auto raster = fixture.fonts->rasterize(font, lookup.glyph.glyph_id);
            require(static_cast<bool>(raster) && raster.glyph && !raster.glyph->coverage.empty() &&
                        std::ranges::any_of(raster.glyph->coverage, [](auto value) { return value > 0; }),
                    "catalog layer has no real FreeType coverage");
        }
    }
    require(outlined == 447 && filled == 251 && two_tone == 150 && maximum_layers == 4,
            "locked catalog family/layer inventory differs");
    for (char32_t point = 0xF000; point <= 0xF003; ++point) {
        const auto glyph = fixture.fonts->glyph_index(font, point);
        require(static_cast<bool>(glyph) && glyph.glyph.glyph_id > 0 && !codepoints.contains(point),
                "Tooltip primitive disappeared or collided");
    }
    bool invalid{};
    try {
        static_cast<void>(detail::bundled_layers(static_cast<IconName>(bundled_icon_count)));
    } catch (const std::invalid_argument&) {
        invalid = true;
    }
    require(invalid, "invalid catalog name was accepted");
    std::cout << "icons=848 outlined=447 filled=251 two_tone=150 layers=" << codepoints.size()
              << " maximum_layers=4 coverage=FreeType\n";
}
} // namespace

int main() {
    try {
        all_locked_layers_rasterize();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
