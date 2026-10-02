#include "support/input_fixture.hpp"
#include "component/space_component.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using ryn_test::input_component::Fixture;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float a, float b) {
    return std::abs(a - b) < .001F;
}

void defaults_orientation_and_last_setter() {
    Fixture fixture;
    Signal<SpaceOrientation> orientation{SpaceOrientation::Horizontal};
    Signal<bool> legacy{false};
    int runs{};
    fixture.buttons.mount(Content{[&] {
        Space(SpaceProps{}.orientation(orientation).layout(LayoutStyle{}.width(dp(200)).height(dp(80))),
              SpaceContent{[&] {
                  ++runs;
                  Text(u8"自动对齐 Ag");
                  Text(TextProps{}.content(u8"固定").layout(LayoutStyle{}.width(dp(50))));
              }});
        Space(SpaceProps{}.orientation(orientation).vertical(legacy), SpaceContent{[] { Text(u8"旧 bool 优先"); }});
        Space(SpaceProps{}.vertical(legacy).orientation(orientation), SpaceContent{[] { Text(u8"typed 优先"); }});
    }});
    fixture.synchronize();
    const auto roots = fixture.services.components().root_components();
    const auto root = fixture.services.components().root(roots[0]);
    const auto children = fixture.nodes.require(root).children;
    require(
        near(fixture.nodes.require(children[0]).bounds.y, (80 - fixture.nodes.require(children[0]).bounds.height) / 2),
        "horizontal default is not Center");
    orientation.set(SpaceOrientation::Vertical);
    fixture.synchronize();
    require(near(fixture.nodes.require(children[0]).bounds.width, 200) &&
                near(fixture.nodes.require(children[1]).bounds.width, 50),
            "vertical default ignored Stretch or explicit width");
    const auto* old = fixture.services.components().state<detail::SpaceComponentState>(roots[1]);
    const auto* typed = fixture.services.components().state<detail::SpaceComponentState>(roots[2]);
    require(old->model.direction == layout::FlexDirection::horizontal &&
                typed->model.direction == layout::FlexDirection::vertical,
            "last orientation setter did not determine subscription");
    bool rejected{};
    try {
        orientation.set(static_cast<SpaceOrientation>(99));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && typed->model.direction == layout::FlexDirection::vertical, "invalid orientation mutated model");
    orientation.set(SpaceOrientation::Horizontal);
    fixture.synchronize();
    legacy.set(true);
    fixture.synchronize();
    require(old->model.direction == layout::FlexDirection::vertical &&
                typed->model.direction == layout::FlexDirection::horizontal && runs == 1,
            "legacy setter reran content or changed typed subscription");
    fixture.buttons.dispose();
    orientation.set(SpaceOrientation::Vertical);
    legacy.set(false);
    require(fixture.nodes.size() == 0, "orientation retained disposed nodes");
}

void real_baseline_and_nested_direction() {
    Fixture fixture;
    Signal<FlexDirection> direction{FlexDirection::LeftToRight};
    Signal<SpaceAlign> align{SpaceAlign::Baseline};
    ThemeConfig large;
    large.text.tokens.font_size = dp(28);
    large.text.tokens.line_height = dp(40);
    fixture.buttons.mount(Content{[&] {
        Flex(FlexProps{}.align(FlexAlign::Baseline).gap(dp(8)), FlexContent{[&] {
                 Space(SpaceProps{}.align(align).direction(direction).size(dp(8)), SpaceContent{[&] {
                           Text(u8"Ag 小字");
                           Theme(ThemeProps{}.config(large), ThemeContent{[] { Text(u8"Ag 大字"); }});
                           Button(ButtonProps{}, [] { Text(u8"Ag 控件"); });
                       }});
                 Text(u8"Ag 外层");
             }});
    }});
    fixture.synchronize(640, {0, 0, 640, 240});
    const auto texts = fixture.services.text().mounted_texts();
    const auto first = fixture.services.components().root(texts[0].component);
    const auto last = fixture.services.components().root(texts.back().component);
    const auto baseline = [&](std::size_t index) {
        const auto& node = fixture.nodes.require(fixture.services.components().root(texts[index].component));
        return node.bounds.y + fixture.scene.text_state(texts[index].scene).measurement().first_baseline;
    };
    for (std::size_t i = 1; i < texts.size(); ++i) {
        require(near(baseline(0), baseline(i)), "nested Space or Button real baseline differs");
    }
    const auto measured = fixture.nodes.require(first).measure_count;
    const auto unrelated = fixture.nodes.require(last).measure_count;
    const auto shaped = fixture.scene.text_state(texts[0].scene).counters().shape_count;
    direction.set(FlexDirection::RightToLeft);
    fixture.synchronize(640, {0, 0, 640, 240});
    require(fixture.nodes.require(first).measure_count == measured &&
                fixture.nodes.require(last).measure_count == unrelated &&
                fixture.scene.text_state(texts[0].scene).counters().shape_count == shaped,
            "nested Space RTL remeasured baseline ancestor");
    align.set(SpaceAlign::Center);
    fixture.synchronize(640, {0, 0, 640, 240});
    align.set(SpaceAlign::Baseline);
    fixture.synchronize(640, {0, 0, 640, 240});
    for (std::size_t i = 1; i < texts.size(); ++i) {
        require(near(baseline(0), baseline(i)), "baseline re-entry retained stale line metrics");
    }
}
} // namespace

int main() {
    try {
        defaults_orientation_and_last_setter();
        real_baseline_and_nested_direction();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
