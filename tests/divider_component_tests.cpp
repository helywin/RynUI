#include "component/divider_component.hpp"
#include "support/input_fixture.hpp"
#include <cmath>
#include <iostream>
#include <limits>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float a, float b) {
    return std::abs(a - b) < 0.002F;
}

void forms() {
    Fixture f;
    f.services.mount(Content{[] {
        Divider();
        Divider(DividerProps{}.content(u8"Center"));
        Divider(DividerProps{}.content(u8"Left").orientation(DividerOrientation::Left));
        Divider(DividerProps{}.content(u8"Right").orientation(DividerOrientation::Right));
        Divider(DividerProps{}.type(DividerType::Vertical).content(u8"Ignored"));
        Divider(DividerProps{}.dashed(true));
        Divider(DividerProps{}.content(u8"Plain").plain(true));
        Divider(DividerProps{}.orientation(DividerOrientation::Left), DividerText{[] { Text(u8"Typed slot"); }});
    }});
    f.synchronize(300, {0, 0, 300, 1000});
    const auto ids = f.services.divider().mounted();
    require(ids.size() == 8, "Divider forms did not mount");
    const auto token = resolve_theme().divider();
    const auto blank = f.services.divider().snapshot(ids[0]);
    require(!blank.has_label && near(blank.left.width, 300) && near(blank.margin, token.metrics.horizontal_margin),
            "empty Divider geometry drifted");
    const auto center = f.services.divider().snapshot(ids[1]);
    require(center.has_label && near(center.left.width, center.right.width) &&
                near(center.label.x, center.left.width + token.metrics.text_padding_inline),
            "center rails/label spacing wrong");
    const auto left = f.services.divider().snapshot(ids[2]);
    const auto right = f.services.divider().snapshot(ids[3]);
    require(near(left.left.width, 15) && near(right.right.width, 15), "Theme orientation ratio not applied");
    const auto vertical = f.services.divider().snapshot(ids[4]);
    require(vertical.vertical && !vertical.has_label && near(vertical.left.height, 19.8F) &&
                near(vertical.left.y, -1.32F) &&
                near(f.nodes.require(f.services.components().root(ids[4])).measured_size.width, 17),
            "vertical fixed line-height geometry wrong");
    require(f.services.divider().snapshot(ids[7]).has_label, "typed Divider slot missing");
    require(f.services.interactions().size() == 0 && !f.inputs.sessions().active().valid(),
            "Divider acquired interaction or IME state");
    const auto scene = f.services.text().mounted_texts()[2].scene;
    const auto text_node = f.scene.node(scene);
    const auto root = f.nodes.require(f.services.components().root(ids[2])).bounds;
    const auto glyph = f.scene.primitive(scene).instances;
    require(glyph.count > 0 && near(f.nodes.require(text_node).bounds.x, root.x + left.label.x),
            "label placement missed same-frame geometry");
    require(f.scene.glyph_scene().instances().at(glyph.first).position_size[0] >= root.x + left.label.x - 4,
            "glyphs remained at previous position");
    bool found_dash = false;
    for (const auto& quad : f.services.surfaces().instances().instances()) {
        if (near(quad.bounds[2], 3)) {
            found_dash = true;
        }
    }
    require(found_dash, "dashed Divider emitted no dash segments");
}

void reactive() {
    Fixture f;
    Signal<String> content{String{}};
    Signal<DividerOrientation> orientation{DividerOrientation::Left};
    Signal<DividerOrientationMargin> margin{DividerOrientationMargin::theme()};
    Signal<bool> plain{false};
    Signal<bool> dashed{false};
    Signal<DividerType> type{DividerType::Horizontal};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Divider(DividerProps{}
                              .content(content)
                              .orientation(orientation)
                              .orientationMargin(margin)
                              .plain(plain)
                              .dashed(dashed)
                              .type(type));
              }});
        Text(u8"Sibling");
    }});
    f.synchronize(300);
    const auto id = f.services.divider().mounted().front();
    const auto component_count = f.services.components().component_count();
    const auto runs = f.services.components().mount_runs();
    const auto label = f.services.text().mounted_texts().front();
    const auto sibling = f.services.text().mounted_texts().back();
    const auto sibling_shapes = f.scene.text_state(sibling.scene).counters().shape_count;
    content.set(String{u8"Label"});
    f.synchronize(300);
    require(f.services.divider().snapshot(id).has_label, "empty->label update failed");
    margin.set(DividerOrientationMargin::fraction(.2F));
    f.synchronize(300);
    require(near(f.services.divider().snapshot(id).left.width, 60), "explicit ratio did not override Theme");
    margin.set(DividerOrientationMargin::fraction(0));
    f.synchronize(300);
    auto geometry = f.services.divider().snapshot(id);
    require(near(geometry.left.width, 0) && near(geometry.label.x, 16), "zero ratio was confused with None");
    margin.set(DividerOrientationMargin::none());
    f.synchronize(300);
    geometry = f.services.divider().snapshot(id);
    require(near(geometry.left.width, 0) && near(geometry.label.x, 0), "None margin failed no-default padding");
    orientation.set(DividerOrientation::Right);
    f.synchronize(300);
    require(near(f.services.divider().snapshot(id).right.width, 0), "right None did not suppress edge rail");
    margin.set(DividerOrientationMargin::theme());
    plain.set(true);
    f.synchronize(300);
    require(f.scene.text_state(label.scene).shaped().default_metrics.logical_pixel_size == 14,
            "plain label typography not inherited");
    type.set(DividerType::Vertical);
    f.synchronize(300);
    require(!f.services.divider().snapshot(id).has_label, "vertical did not suspend label");
    type.set(DividerType::Horizontal);
    f.synchronize(300);
    content.set(String{});
    f.synchronize(300);
    require(!f.services.divider().snapshot(id).has_label && near(f.services.divider().snapshot(id).left.width, 300),
            "label->empty failed full rail");
    content.set(String{u8"A very long label with Chinese 中文和很多内容"});
    f.synchronize(18, {0, 0, 18, 240});
    geometry = f.services.divider().snapshot(id);
    require(geometry.left.width == 0 && geometry.right.width == 0 && geometry.label.x + geometry.label.width <= 18.001F,
            "oversized label overflowed rails");
    f.synchronize(300);
    const auto node = f.services.components().root(id);
    const auto measures = f.nodes.require(node).measure_count;
    const auto places = f.nodes.require(node).place_count;
    const auto shapes = f.scene.text_state(label.scene).counters().shape_count;
    const auto surface = f.services.surfaces().diagnostics();
    auto changed = ThemeConfig{};
    changed.divider.tokens.line = Color::rgba8(255, 0, 0);
    f.dirty.clear();
    theme.set(changed);
    require(f.dirty.layout_roots().empty(), "Divider line color requested layout");
    f.synchronize(300);
    require(f.nodes.require(node).measure_count == measures && f.nodes.require(node).place_count == places &&
                f.scene.text_state(label.scene).counters().shape_count == shapes,
            "color update reshaped or laid out Divider");
    require(f.services.surfaces().diagnostics().geometry_updates == surface.geometry_updates &&
                f.services.surfaces().diagnostics().material_updates > surface.material_updates,
            "line color did not use material path");
    require(f.services.components().component_count() == component_count &&
                f.services.components().mount_runs() == runs &&
                f.scene.text_state(sibling.scene).counters().shape_count == sibling_shapes,
            "Divider update remounted or reshaped sibling");
    require(f.services.destroy(id), "Divider destroy failed");
    require(f.services.divider().mounted().empty() && f.services.surfaces().size() == 0,
            "Divider teardown leaked ranges");
}

void variants_and_dotted_scene() {
    Fixture f;
    Signal<DividerVariant> variant{DividerVariant::Dotted};
    Signal<bool> dashed{false};
    Signal<DividerType> type{DividerType::Horizontal};
    ThemeConfig config;
    config.divider.tokens.line_width = dp(4);
    Signal<ThemeConfig> theme{config};
    int runs{};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Divider(DividerProps{}.variant(variant).dashed(dashed).type(type), DividerText{[&] {
                              ++runs;
                              Text(u8"Retained");
                          }});
              }});
    }});
    f.synchronize(300);
    const auto id = f.services.divider().mounted().front();
    const auto node = f.services.components().root(id);
    const auto text = f.services.text().mounted_texts().front();
    const auto measured = f.nodes.require(node).measure_count;
    const auto shaped = f.scene.text_state(text.scene).counters().shape_count;
    const auto effect = f.services.rounded_effects().packed_instances().front();
    const auto rect = effect.geometry.shape.rect;
    require(near(effect.geometry.shape.radius, 2) && near(rect.width, 4) && near(rect.height, 4),
            "Dotted Divider does not publish circles");
    require(graphics::rounded_effect_coverage({rect.x + 2, rect.y + 2}, effect) == 1 &&
                graphics::rounded_effect_coverage({rect.x, rect.y}, effect) == 0,
            "Dotted reference coverage is square or hollow");
    require(f.services.surfaces().instances().size() == 0 &&
                f.services.rounded_effects().diagnostics().live_instances > 0,
            "Dotted Divider filled its transparent gaps");
    dashed.set(true);
    f.synchronize(300);
    require(f.services.divider().snapshot(id).variant == DividerVariant::Dotted, "legacy dashed overrode Dotted");
    variant.set(DividerVariant::Solid);
    f.synchronize(300);
    require(f.services.divider().snapshot(id).variant == DividerVariant::Dashed &&
                f.services.rounded_effects().diagnostics().live_instances == 0 &&
                f.services.surfaces().instances().size() > 2,
            "legacy dashed fallback failed or leaked dots");
    dashed.set(false);
    f.synchronize(300);
    require(f.services.divider().snapshot(id).variant == DividerVariant::Solid &&
                f.services.surfaces().instances().size() == 2,
            "Solid variant did not restore two rails");
    variant.set(DividerVariant::Dashed);
    f.synchronize(300);
    require(f.services.divider().snapshot(id).dashed, "typed Dashed was ignored");
    require(f.nodes.require(node).measure_count == measured &&
                f.scene.text_state(text.scene).counters().shape_count == shaped && runs == 1,
            "variant updates remeasured or remounted label");
    variant.set(DividerVariant::Dotted);
    f.synchronize(300);
    const auto before = f.services.rounded_effects().diagnostics();
    config.divider.tokens.line = Color::rgba8(255, 0, 0);
    theme.set(config);
    f.synchronize(300);
    require(f.nodes.require(node).measure_count == measured &&
                f.services.rounded_effects().diagnostics().geometry_updates == before.geometry_updates &&
                f.services.rounded_effects().diagnostics().material_updates > before.material_updates,
            "Dotted color update missed Material locality");
    f.nodes.require(node).translation = {11, 7};
    f.dirty.invalidate(node, runtime::DirtyFlags::Geometry);
    const auto y = f.services.divider().snapshot(id).left.y + 7;
    f.synchronize(300, {13, y, 80, 4});
    const auto translated = f.services.rounded_effects().packed_instances().front();
    require(translated.geometry.translation == runtime::Point{11, 7} && translated.geometry.ancestor_clip &&
                translated.geometry.ancestor_clip->bounds.x >= 13 &&
                translated.geometry.ancestor_clip->bounds.width <= 80,
            "Dotted translation/window clip failed");
    type.set(DividerType::Vertical);
    f.synchronize(300);
    require(!f.services.divider().snapshot(id).has_label &&
                f.services.rounded_effects().diagnostics().live_instances > 0 && runs == 1,
            "Vertical Dotted lost retained label or dots");
    require(f.services.destroy(id) && f.services.rounded_effects().diagnostics().live_instances == 0 &&
                f.services.surfaces().size() == 0 && f.services.interactions().size() == 0,
            "Dotted teardown leaked resources");
}

void sizes_positions_and_length() {
    Fixture f;
    Signal<ControlSize> size{ControlSize::Small};
    Signal<DividerOrientation> orientation{DividerOrientation::Start};
    Signal<DividerDirection> direction{DividerDirection::LeftToRight};
    Signal<DividerOrientationMargin> margin{DividerOrientationMargin::theme()};
    Signal<DividerType> type{DividerType::Horizontal};
    ThemeConfig config;
    config.divider.tokens.small_horizontal_margin = dp(3);
    config.divider.tokens.middle_horizontal_margin = dp(11);
    config.divider.tokens.horizontal_with_text_margin = dp(19);
    Signal<ThemeConfig> theme{config};
    int runs{};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Divider(DividerProps{}
                              .size(size)
                              .orientation(orientation)
                              .direction(direction)
                              .orientationMargin(margin)
                              .type(type),
                          DividerText{[&] {
                              ++runs;
                              Text(u8"Title");
                          }});
                  Divider(DividerProps{}.size(size));
              }});
    }});
    f.synchronize(300);
    const auto id = f.services.divider().mounted()[0];
    const auto blank = f.services.divider().mounted()[1];
    for (const auto [value, expected] : {std::pair{ControlSize::Small, 3.0F}, std::pair{ControlSize::Middle, 11.0F}}) {
        size.set(value);
        f.synchronize(300);
        require(near(f.services.divider().snapshot(id).margin, expected) &&
                    near(f.services.divider().snapshot(blank).margin, expected),
                "size margin override ignored labeled/blank Divider");
    }
    size.set(ControlSize::Large);
    f.synchronize(300);
    require(near(f.services.divider().snapshot(id).margin, 19) &&
                near(f.services.divider().snapshot(blank).margin,
                     resolve_theme(config).divider().metrics.horizontal_margin),
            "Large did not preserve legacy margins");
    const auto ltr_start = f.services.divider().snapshot(id);
    direction.set(DividerDirection::RightToLeft);
    f.synchronize(300);
    const auto rtl_start = f.services.divider().snapshot(id);
    require(near(ltr_start.left.width, rtl_start.right.width) && near(ltr_start.right.width, rtl_start.left.width),
            "Start did not mirror in RTL");
    orientation.set(DividerOrientation::End);
    f.synchronize(300);
    require(near(f.services.divider().snapshot(id).left.width, ltr_start.left.width), "RTL End did not point left");
    for (auto physical : {DividerOrientation::Left, DividerOrientation::Right, DividerOrientation::Center}) {
        orientation.set(physical);
        f.synchronize(300);
        const auto rtl = f.services.divider().snapshot(id);
        direction.set(DividerDirection::LeftToRight);
        f.synchronize(300);
        require(f.services.divider().snapshot(id).left == rtl.left &&
                    f.services.divider().snapshot(id).right == rtl.right,
                "direction moved physical/center title");
        direction.set(DividerDirection::RightToLeft);
    }
    orientation.set(DividerOrientation::Left);
    margin.set(DividerOrientationMargin::length(dp(20)));
    f.synchronize(300);
    auto geometry = f.services.divider().snapshot(id);
    require(geometry.left.width == 0 && near(geometry.label.x, 20) &&
                near(geometry.right.x - geometry.label.x - geometry.label.width,
                     resolve_theme(config).divider().metrics.text_padding_inline),
            "length did not preserve near margin/far padding");
    orientation.set(DividerOrientation::Right);
    f.synchronize(300);
    geometry = f.services.divider().snapshot(id);
    require(geometry.right.width == 0 && near(300 - geometry.label.x - geometry.label.width, 20),
            "right length margin failed");
    margin.set(DividerOrientationMargin::length(dp(0)));
    f.synchronize(300);
    require(near(300 - f.services.divider().snapshot(id).label.x - f.services.divider().snapshot(id).label.width, 0),
            "zero length treated as undeclared");
    margin.set(DividerOrientationMargin::length(dp(1000)));
    f.synchronize(18, {0, 0, 18, 240});
    geometry = f.services.divider().snapshot(id);
    require(geometry.left.width >= 0 && geometry.right.width >= 0 && geometry.label.x >= 0 &&
                geometry.label.x + geometry.label.width <= 18.001F,
            "length escaped narrow constraints");
    type.set(DividerType::Vertical);
    f.synchronize(300);
    const auto vertical = f.services.divider().snapshot(id).left;
    size.set(ControlSize::Small);
    f.synchronize(300);
    require(f.services.divider().snapshot(id).left == vertical && runs == 1,
            "size changed vertical height or remounted title");
    const auto base = resolve_theme();
    const auto parent = resolve_theme(config);
    require(parent.identity() != base.identity() &&
                resolve_theme({}, &parent).divider().metrics == parent.divider().metrics,
            "Divider metric identity/inheritance failed");
    for (const auto algorithms :
         {std::vector<ThemeAlgorithm>{}, std::vector{ThemeAlgorithm::Dark}, std::vector{ThemeAlgorithm::Compact}}) {
        ThemeConfig derived;
        derived.algorithms = algorithms;
        const auto snapshot = resolve_theme(derived);
        require(near(snapshot.divider().metrics.small_horizontal_margin, snapshot.map().size_xs) &&
                    near(snapshot.divider().metrics.middle_horizontal_margin, snapshot.map().size),
                "Divider size metrics drifted from Default/Dark/Compact map");
    }
    type.set(DividerType::Horizontal);
    margin.set(DividerOrientationMargin::theme());
    config.divider.tokens.small_horizontal_margin = dp(7);
    theme.set(config);
    f.synchronize(300);
    require(near(f.services.divider().snapshot(id).margin, 7), "Divider size metric subscription failed");
    config.divider.tokens.small_horizontal_margin = dp(-1);
    bool rejected{};
    try {
        static_cast<void>(resolve_theme(config));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "negative size token accepted");
}

void invalid_values_and_primitive_bound() {
    const auto reject = [](DividerProps props) {
        Fixture f;
        bool rejected{};
        try {
            f.services.mount(Content{[&] { Divider(props); }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected && f.services.components().component_count() == 0 && f.services.surfaces().size() == 0,
                "invalid input or mount rollback failed");
    };
    reject(DividerProps{}.variant(static_cast<DividerVariant>(99)));
    reject(DividerProps{}.direction(static_cast<DividerDirection>(99)));
    reject(DividerProps{}.size(static_cast<ControlSize>(99)));
    reject(DividerProps{}.orientation(static_cast<DividerOrientation>(99)));
    reject(DividerProps{}.type(static_cast<DividerType>(99)));
    reject(DividerProps{}.orientationMargin(
        DividerOrientationMargin{DividerOrientationMargin::Source::Length, 0, std::numeric_limits<float>::infinity()}));
    for (auto length : {auto_length, dp(-1), dp(std::numeric_limits<float>::quiet_NaN())}) {
        bool rejected{};
        try {
            static_cast<void>(DividerOrientationMargin::length(length));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "invalid length factory accepted");
    }
    Fixture f;
    ThemeConfig config;
    config.divider.tokens.line_width = dp(0.000001F);
    Signal<ThemeConfig> theme{config};
    Signal<DividerVariant> variant{DividerVariant::Dotted};
    f.services.mount(Content{
        [&] { Theme(ThemeProps{}.config(theme), ThemeContent{[&] { Divider(DividerProps{}.variant(variant)); }}); }});
    for (auto value : {DividerVariant::Dotted, DividerVariant::Dashed}) {
        variant.set(value);
        bool rejected{};
        try {
            f.synchronize(300);
        } catch (const std::length_error&) {
            rejected = true;
        }
        require(rejected && f.services.rounded_effects().diagnostics().live_instances == 0,
                "oversized decoration silently truncated");
    }
    config.divider.tokens.line_width = dp(0);
    theme.set(config);
    f.synchronize(300);
    require(f.services.surfaces().instances().size() == 0, "zero width published segments");
    config.divider.tokens.line_width = dp(4);
    theme.set(config);
    variant.set(DividerVariant::Dotted);
    f.synchronize(300);
    require(f.services.rounded_effects().diagnostics().live_instances > 0,
            "Divider did not recover after rejected geometry");
    bool rejected_update{};
    try {
        variant.set(static_cast<DividerVariant>(99));
    } catch (const std::invalid_argument&) {
        rejected_update = true;
    }
    require(rejected_update &&
                f.services.divider().snapshot(f.services.divider().mounted().front()).variant == DividerVariant::Dotted,
            "invalid reactive variant changed the mounted state");
    variant.set(DividerVariant::Dotted);
    config.divider.tokens.line_width = dp(0.03125F);
    theme.set(config);
    f.synchronize(256);
    require(f.services.rounded_effects().diagnostics().live_instances == 4096, "valid primitive limit was rejected");
    bool exceeded{};
    try {
        f.synchronize(257);
    } catch (const std::length_error&) {
        exceeded = true;
    }
    require(exceeded && f.services.rounded_effects().diagnostics().live_instances == 4096,
            "oversized geometry partially replaced a valid scene");
}
} // namespace

int main() {
    try {
        forms();
        reactive();
        variants_and_dotted_scene();
        sizes_positions_and_length();
        invalid_values_and_primitive_bound();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
