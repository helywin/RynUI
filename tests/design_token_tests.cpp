#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>

#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <unordered_set>

namespace {

static_assert(!std::is_constructible_v<ryn::Color, std::string_view>);
static_assert(!std::is_constructible_v<ryn::ShadowList, std::string_view>);
static_assert(!std::is_constructible_v<ryn::BorderToken, std::string_view>);
static_assert(std::is_trivially_copyable_v<ryn::Color>);
static_assert(std::is_trivially_copyable_v<ryn::ShadowLayer>);
static_assert(ryn::dp(8.0F).value() == 8.0F);

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Operation>
void require_invalid(Operation&& operation, const char* message) {
    try {
        operation();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(message);
}

void test_typed_values() {
    constexpr auto blue = ryn::Color::rgba8(22, 119, 255);
    static_assert(blue == ryn::Color(22.0F / 255.0F, 119.0F / 255.0F, 1.0F));
    constexpr ryn::CubicBezier easing{0.08F, 0.82F, 0.17F, 1.0F};
    static_assert(easing.y1 == 0.82F);
    constexpr auto duration = ryn::Duration::seconds(0.1F);
    static_assert(duration.count_milliseconds() == 100.0F);
    constexpr ryn::BorderToken border{1.0F, ryn::BorderStyle::solid, blue};
    static_assert(border.width == 1.0F);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    require_invalid([&] { static_cast<void>(ryn::Color(nan, 0.0F, 0.0F)); },
                    "Color accepted NaN");
    require_invalid([&] { static_cast<void>(ryn::Color(1.1F, 0.0F, 0.0F)); },
                    "Color accepted an out-of-range channel");
    require_invalid([&] { static_cast<void>(ryn::LogicalOffset(infinity, 0.0F)); },
                    "logical offset accepted Infinity");
    require_invalid([&] { static_cast<void>(ryn::Duration::milliseconds(-1.0F)); },
                    "Duration accepted a negative value");
    require_invalid([&] { static_cast<void>(ryn::CubicBezier(-0.1F, 0.0F, 1.0F, 1.0F)); },
                    "CubicBezier accepted an invalid x control point");
    require_invalid([&] {
        static_cast<void>(ryn::ShadowLayer{
            ryn::ShadowKind::outer, {}, -1.0F, 0.0F, {}});
    }, "ShadowLayer accepted negative blur");
}

void test_default_seed() {
    const auto& seed = ryn::ant_design_default_seed();
    require(seed.color_primary == ryn::Color::rgba8(22, 119, 255)
                && seed.color_success == ryn::Color::rgba8(82, 196, 26)
                && seed.color_warning == ryn::Color::rgba8(250, 173, 20)
                && seed.color_error == ryn::Color::rgba8(255, 77, 79)
                && seed.color_info == seed.color_primary,
            "default semantic seed colors drifted");
    require(seed.font_family == ryn::SystemFontFamily::ui_sans
                && seed.font_family_code == ryn::SystemFontFamily::ui_monospace
                && !seed.color_link.has_value() && !seed.color_text_base.has_value()
                && !seed.color_background_base.has_value()
                && seed.font_size == 14 && seed.line_width == 1.0F
                && seed.border_radius == 6.0F && seed.size_unit == 4.0F
                && seed.size_step == 4.0F && seed.size_popup_arrow == 16.0F
                && seed.control_height == 32.0F && seed.z_index_base == 0
                && seed.z_index_popup_base == 1000 && seed.opacity_image == 1.0F
                && seed.motion_unit == ryn::Duration::seconds(0.1F)
                && seed.motion_base == ryn::Duration{} && !seed.wireframe && seed.motion,
            "default numeric or motion seed values drifted");
}

void test_shadow_normalization() {
    const auto& shadows = ryn::ant_design_default_shadows();
    require(shadows.box_shadow.size() == 3
                && shadows.box_shadow[0].offset == ryn::LogicalOffset(0.0F, 6.0F)
                && shadows.box_shadow[0].blur == 16.0F
                && shadows.box_shadow[0].color.alpha() == 0.08F
                && shadows.box_shadow[1].spread == -4.0F
                && shadows.box_shadow[1].color.alpha() == 0.12F
                && shadows.box_shadow[2].offset.y == 9.0F
                && shadows.box_shadow[2].spread == 8.0F,
            "primary elevation shadow order or values drifted");
    require(shadows.box_shadow == shadows.box_shadow_secondary,
            "Ant Design default secondary elevation is no longer normalized identically");
    require(shadows.box_shadow_tertiary.size() == 3
                && shadows.box_shadow_tertiary[1].blur == 6.0F
                && shadows.box_shadow_tertiary[1].spread == -1.0F,
            "tertiary elevation shadow drifted");
    require(shadows.button_default[0].offset.y == 2.0F
                && shadows.button_primary[0].color.alpha() == 0.1F
                && shadows.button_danger[0].color.alpha() == 0.06F,
            "Button shadow normalization drifted");
    require(shadows.card.size() == 3 && shadows.card[0].spread == -2.0F
                && shadows.popover_drop.size() == 3
                && shadows.popover_drop[1].spread == 0.0F,
            "Card or Popover shadow normalization drifted");
    require(shadows.drawer_right[0].offset.x == -6.0F
                && shadows.drawer_left[0].offset.x == 6.0F
                && shadows.drawer_up[0].offset.y == 6.0F
                && shadows.drawer_down[0].offset.y == -6.0F,
            "Drawer directional shadow normalization drifted");
    require(shadows.tabs_overflow_left[0].kind == ryn::ShadowKind::inset
                && shadows.tabs_overflow_left[0].offset.x == 10.0F
                && shadows.tabs_overflow_right[0].offset.x == -10.0F
                && shadows.tabs_overflow_top[0].offset.y == 10.0F
                && shadows.tabs_overflow_bottom[0].offset.y == -10.0F
                && shadows.tabs_overflow_bottom[0].spread == -8.0F,
            "Tabs inset overflow shadow normalization drifted");
}

void test_generated_metadata() {
    const auto entries = ryn::ant_design_token_metadata();
    require(entries.size() == 1194, "generated metadata does not cover the catalog");
    std::unordered_set<std::uint64_t> stable_ids;
    std::string_view previous;
    for (const auto& entry : entries) {
        require(!entry.identity.empty() && entry.stable_id != 0,
                "generated metadata contains an empty identity");
        require(previous.empty() || previous < entry.identity,
                "generated metadata is not in stable identity order");
        require(stable_ids.insert(entry.stable_id).second,
                "generated metadata contains a stable id collision");
        previous = entry.identity;
    }
    const auto* runtime = ryn::find_ant_design_token("ant.alias.boxShadow");
    const auto* error_hover = ryn::find_ant_design_token("ant.map.colorErrorHover");
    const auto* error_active = ryn::find_ant_design_token("ant.map.colorErrorActive");
    const auto* web = ryn::find_ant_design_token("ant.alias.linkDecoration");
    const auto* future = ryn::find_ant_design_token("ant.component.Affix.zIndexPopup");
    require(runtime != nullptr && runtime->support == ryn::TokenSupportStatus::runtime
                && runtime->value_kind == ryn::TokenValueKind::shadow_list,
            "runtime metadata query failed");
    require(error_hover != nullptr && error_active != nullptr
                && error_hover->support == ryn::TokenSupportStatus::runtime
                && error_active->support == ryn::TokenSupportStatus::runtime
                && error_hover->invalidation
                    == ryn::TokenInvalidationDomain::paint_material
                && error_active->invalidation
                    == ryn::TokenInvalidationDomain::paint_material,
            "typed error palette metadata was not promoted to runtime support");
    require(web != nullptr && web->support == ryn::TokenSupportStatus::web_only,
            "web-only catalog metadata is not queryable");
    require(future != nullptr
                && future->support
                    == ryn::TokenSupportStatus::component_not_yet_implemented
                && future->component_owner == "Affix",
            "unsupported component metadata is not queryable");
    require(ryn::find_ant_design_token("ant.alias.doesNotExist") == nullptr,
            "unknown token metadata query did not fail closed");
}

} // namespace

int main() {
    try {
        test_typed_values();
        test_default_seed();
        test_shadow_normalization();
        test_generated_metadata();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
