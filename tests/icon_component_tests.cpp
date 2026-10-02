#include "support/input_fixture.hpp"
#include "component/tooltip_component.hpp"
#include "icons/bundled_icon_catalog.hpp"
#include "theme/semantic_background.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <class F> void rejects(F action) {
    bool rejected{};
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "invalid Icon input accepted");
}

std::array<float, 4> channels(Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

const graphics::GlyphInstance& glyph(Fixture& f, detail::TextSceneId scene) {
    const auto range = f.scene.primitive(scene).instances;
    check(range.count == 1, "Icon layer did not retain exactly one glyph");
    return f.scene.glyph_scene().instances().at(range.first);
}

void check_order(Fixture& f, const detail::IconComponentSnapshot& icon, detail::TextSceneId sibling) {
    std::vector<std::uint32_t> actual;
    for (const auto& command : f.scene.ordered_scene().commands()) {
        for (std::uint32_t index = 0; index < command.instance_count; ++index) {
            actual.push_back(command.first_instance + index);
        }
    }
    std::vector<std::uint32_t> expected;
    for (const auto scene : icon.layers) {
        const auto range = f.scene.primitive(scene).instances;
        for (std::uint32_t index = 0; index < range.count; ++index) {
            expected.push_back(range.first + index);
        }
    }
    const auto range = f.scene.primitive(sibling).instances;
    for (std::uint32_t index = 0; index < range.count; ++index) {
        expected.push_back(range.first + index);
    }
    check(actual == expected, "Icon layers crossed the following sibling's paint order");
}

void retained_layers_and_material() {
    Fixture f;
    Signal<IconSource> source{IconSource{IconName::HeartTwoTone}};
    Signal<IconTwoToneColor> colors{{Color{.8F, .2F, .1F, .7F}, Color{.1F, .8F, .3F, .4F}}};
    Signal<float> angle{0};
    Signal<bool> visible{true};
    int content_runs{};
    f.services.mount(Content{[&] {
        ++content_runs;
        Icon(IconProps{}.source(source).twoToneColor(colors).rotate(angle).visible(visible));
        Text(u8"sibling");
    }});
    f.synchronize();
    const auto mounted = f.services.text().mounted_texts();
    const auto id = mounted[0].component;
    const auto primary = mounted[0].scene;
    const auto sibling = mounted[1].scene;
    const auto node = f.services.components().root(id);
    auto icon = f.services.text().icon_snapshot(id);
    check(icon.layers.size() == 2 && icon.layers.front() == primary && content_runs == 1,
          "two-tone mounted extra Components or changed the primary identity");
    check_order(f, icon, sibling);
    const auto roles = detail::bundled_layers(IconName::HeartTwoTone);
    for (std::size_t index = 0; index < icon.layers.size(); ++index) {
        check(f.scene.node(icon.layers[index]) == node &&
                  glyph(f, icon.layers[index]).color ==
                      channels(roles[index].secondary ? *colors.get().secondary : colors.get().primary),
              "two-tone role/node differs");
        check(glyph(f, icon.layers[index]).transform == glyph(f, primary).transform, "layers do not share a pivot");
    }
    const auto sibling_shapes = f.scene.text_state(sibling).counters().shape_count;
    const auto measures = f.nodes.require(node).measure_count;
    std::vector<std::size_t> shapes;
    for (const auto scene : icon.layers) {
        shapes.push_back(f.scene.text_state(scene).counters().shape_count);
    }
    const auto coverage = f.scene.atlas().dirty_regions().size();
    colors.set({Color{.2F, .4F, .7F, .6F}, {}});
    angle.set(90);
    f.synchronize();
    const auto secondary = detail::palette_lightest(colors.get().primary);
    for (std::size_t index = 0; index < icon.layers.size(); ++index) {
        check(glyph(f, icon.layers[index]).color == channels(roles[index].secondary ? secondary : colors.get().primary),
              "derived secondary/alpha differs");
        check(glyph(f, icon.layers[index]).transform.angle_degrees == 90 &&
                  f.scene.text_state(icon.layers[index]).counters().shape_count == shapes[index],
              "rotation/material reshaped a layer");
    }
    check(f.nodes.require(node).measure_count == measures && f.scene.atlas().dirty_regions().size() == coverage &&
              f.scene.text_state(sibling).counters().shape_count == sibling_shapes && content_runs == 1,
          "geometry/material change measured, reran Content or uploaded coverage");
    const auto second = icon.layers[1];
    source.set(IconSource{IconName::WalletTwoTone});
    f.synchronize();
    icon = f.services.text().icon_snapshot(id);
    check(icon.layers.size() == 4 && icon.layers[0] == primary && icon.layers[1] == second,
          "source switch replaced retained layer prefix");
    check_order(f, icon, sibling);
    visible.set(false);
    f.synchronize();
    for (const auto scene : icon.layers) {
        check(f.scene.primitive(scene).instances.count == 0, "hidden layer retained glyphs");
    }
    visible.set(true);
    f.synchronize();
    check_order(f, icon, sibling);
    source.set(IconSource{IconName::EyeOutlined});
    f.synchronize();
    check(f.scene.size() == 2 && f.services.text().icon_snapshot(id).layers == std::vector{primary},
          "smaller source leaked surplus scenes");
    check_order(f, f.services.text().icon_snapshot(id), sibling);
    rejects([&] { source.set(IconSource{static_cast<IconName>(bundled_icon_count)}); });
    check(f.services.text().icon_snapshot(id).source == IconSource{IconName::EyeOutlined},
          "invalid source changed retained state");
    source.set(IconSource{IconName::HeartFilled});
    rejects([&] { angle.set(std::numeric_limits<float>::quiet_NaN()); });
    check(f.services.text().icon_snapshot(id).angle_degrees == 90, "invalid angle changed retained state");
    angle.set(45);
    f.synchronize();
    check_order(f, f.services.text().icon_snapshot(id), sibling);
    check(f.scene.text_state(sibling).counters().shape_count == sibling_shapes, "source switch reshaped sibling");
    check(f.services.destroy(id) && f.scene.size() == 1, "Icon destroy leaked layers");
    f.synchronize();
    check(f.scene.primitive(sibling).instances.first == 0, "layer removal corrupted physical range compaction");
}

void theme_and_source_priority() {
    Fixture f;
    Signal<ThemeConfig> config{ThemeConfig{}};
    Signal<IconName> name{IconName::WalletTwoTone};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  Icon(IconProps{}.source(IconSource{IconName::EyeOutlined}).name(name));
                  Icon(IconProps{}.name(name).source(IconSource{IconName::HeartTwoTone}).tone(TextTone::Secondary));
                  Icon(IconProps{}.name(IconName::HeartTwoTone));
              }});
    }});
    f.synchronize();
    const auto texts = f.services.text().mounted_texts();
    const auto first = f.services.text().icon_snapshot(texts[0].component);
    const auto explicit_tone = f.services.text().icon_snapshot(texts[1].component);
    const auto themed = f.services.text().icon_snapshot(texts[2].component);
    check(first.source == IconSource{IconName::WalletTwoTone} &&
              explicit_tone.source == IconSource{IconName::HeartTwoTone},
          "last name/source setter did not win");
    auto changed = ThemeConfig{};
    changed.seed.color_primary = Color{.6F, .1F, .8F};
    changed.algorithms = {ThemeAlgorithm::Dark};
    config.set(changed);
    f.synchronize();
    const auto snapshot = resolve_theme(changed);
    const auto roles = detail::bundled_layers(IconName::HeartTwoTone);
    for (std::size_t index = 0; index < themed.layers.size(); ++index) {
        const auto primary = snapshot.map().color_primary;
        check(glyph(f, themed.layers[index]).color ==
                  channels(roles[index].secondary ? detail::palette_lightest(primary) : primary),
              "Theme primary update did not reach two-tone colors");
        const auto danger = snapshot.alias().color_text_secondary;
        check(glyph(f, explicit_tone.layers[index]).color ==
                  channels(roles[index].secondary ? detail::palette_lightest(danger) : danger),
              "explicit semantic tone lost precedence");
    }
    name.set(IconName::CheckOutlined);
    f.synchronize();
    check(f.services.text().icon_snapshot(texts[0].component).source == IconSource{IconName::CheckOutlined} &&
              f.services.text().icon_snapshot(texts[1].component).source == explicit_tone.source,
          "inactive name binding affected explicit source");
    changed.seed.font_size = dp(24);
    config.set(changed);
    f.synchronize();
    for (const auto scene : themed.layers) {
        check(f.scene.text_state(scene).shaped().glyphs.front().font ==
                  f.scene.text_state(themed.layers[0]).shaped().glyphs.front().font,
              "font size update diverged layer fonts");
    }
    const auto font = f.scene.text_state(themed.layers[0]).shaped().glyphs.front().font;
    f.font_scale = 1.5F;
    f.chains.clear();
    check(f.services.text().set_font_resolver(
              [&](SystemFontFamily, std::uint32_t, bool, std::uint32_t pixels) { return f.resolve(pixels); }),
          "DPI resolver did not update fonts");
    f.synchronize();
    for (const auto scene : themed.layers) {
        check(f.scene.text_state(scene).shaped().glyphs.front().font != font &&
                  f.scene.text_state(scene).shaped().glyphs.front().font ==
                      f.scene.text_state(themed.layers[0]).shaped().glyphs.front().font,
              "DPI update retained stale layer font");
    }
    Fixture bad;
    rejects([&] { bad.services.mount(Content{[] { Icon(IconProps{}.name(static_cast<IconName>(9999))); }}); });
    check(bad.scene.size() == 0 && bad.services.text().mounted_texts().empty(), "invalid mount leaked scenes");
    Fixture bad_angle;
    rejects([&] {
        bad_angle.services.mount(Content{[] { Icon(IconProps{}.rotate(std::numeric_limits<float>::infinity())); }});
    });
    check(bad_angle.scene.size() == 0 && bad_angle.services.text().mounted_texts().empty(),
          "invalid angle mount leaked scenes");
}

void spin_lifecycle() {
    Fixture f;
    Signal<bool> spin{true};
    Signal<bool> visible{true};
    Signal<ThemeConfig> config{ThemeConfig{}};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  Icon(IconProps{}.name(IconName::WalletTwoTone).spin(spin).visible(visible).rotate(15));
              }});
    }});
    f.synchronize();
    const auto id = f.services.text().mounted_texts()[0].component;
    check(!f.services.next_frame_deadline(), "reduced motion started spin");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    check(f.services.next_frame_deadline().has_value(), "spin did not schedule retained frames");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(250000)));
    f.synchronize();
    check(std::abs(f.services.text().icon_snapshot(id).angle_degrees - 105) < .01F,
          "spin did not advance linear quarter turn");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(1000000)));
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(1250000)));
    f.synchronize();
    check(std::abs(f.services.text().icon_snapshot(id).angle_degrees - 105) < .01F,
          "completed spin cycle did not restart");
    visible.set(false);
    f.synchronize();
    check(!f.services.next_frame_deadline() && f.services.text().icon_snapshot(id).angle_degrees == 15,
          "hidden spin retained a deadline or phase");
    visible.set(true);
    f.services.set_window_active(false);
    check(!f.services.next_frame_deadline(), "inactive window retained spin");
    f.services.set_window_active(true);
    check(f.services.next_frame_deadline().has_value(), "active window failed to resume spin");
    auto no_motion = ThemeConfig{};
    no_motion.seed.motion = false;
    config.set(no_motion);
    check(!f.services.next_frame_deadline(), "Theme motion=false retained spin");
    config.set(ThemeConfig{});
    spin.set(false);
    check(!f.services.next_frame_deadline(), "spin=false retained deadline");
    spin.set(true);
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    check(!f.services.next_frame_deadline(), "reduced motion failed to cancel spin");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    check(f.services.destroy(id) && !f.services.next_frame_deadline() && f.scene.size() == 0 &&
              f.services.animations().diagnostics().scopes == 0 && f.services.animations().diagnostics().targets == 0,
          "Icon disposal leaked animation or scenes");

    Fixture popup;
    popup.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> open{false};
    popup.services.mount(Content{[&] {
        Tooltip(TooltipProps{}.open(open), TooltipTrigger{[] { Text(u8"trigger"); }},
                TooltipTitle{[] { Icon(IconProps{}.spin(true)); }});
    }});
    popup.synchronize();
    check(!popup.services.next_frame_deadline(), "inactive Tooltip title started spin");
    open.set(true);
    popup.synchronize();
    check(popup.services.next_frame_deadline().has_value(), "active Tooltip title did not spin");
    open.set(false);
    popup.synchronize();
    check(!popup.services.next_frame_deadline(), "closed Tooltip title retained spin");
    popup.services.dispose();
    check(popup.scene.size() == 0 && popup.services.animations().diagnostics().scopes == 0 &&
              popup.services.animations().diagnostics().targets == 0,
          "popup teardown leaked Icon animation target");
}
} // namespace

int main() {
    try {
        retained_layers_and_material();
        theme_and_source_priority();
        spin_lifecycle();
        std::cout << "Icon retained layers, colors, geometry, Theme, DPI and motion lifecycle passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
