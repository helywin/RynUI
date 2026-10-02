#include "support/input_fixture.hpp"
#include "component/flex_component.hpp"
#include "component/text_component.hpp"

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

float baseline(Fixture& fixture, runtime::NodeId id) {
    const auto& node = fixture.nodes.require(id);
    require(node.first_baseline.has_value(), "real text baseline missing");
    return node.bounds.y + *node.first_baseline;
}

void real_text_controls_and_reactive_baselines() {
    Fixture fixture;
    ThemeConfig large;
    large.text.tokens.font_size = dp(28);
    large.text.tokens.line_height = dp(40);
    Signal<ThemeConfig> theme{large};
    Signal<FlexAlign> align{FlexAlign::Baseline};
    Signal<FlexDirection> direction{FlexDirection::LeftToRight};
    Signal<LogicalLength> margin{dp(3)};
    int labels{};
    fixture.buttons.mount(Content{[&] {
        Flex(FlexProps{}.align(align).direction(direction).gap(dp(8)), FlexContent{[&] {
                 Text(TextProps{}.content(u8"小号 Ag").layout(LayoutStyle{}.margin_top(margin)));
                 Theme(ThemeProps{}.config(theme), ThemeContent{[&] { Text(u8"大号 Ag\n第二行"); }});
                 Button(ButtonProps{}, [&] {
                     ++labels;
                     Text(u8"按钮 Ag");
                 });
                 Input(InputProps{}.defaultValue(u8"输入 Ag").layout(LayoutStyle{}.width(dp(100))));
                 Text(TypographyProps{}.content(u8"排版 Ag"));
             }});
    }});
    fixture.synchronize(640, {0, 0, 640, 240});
    const auto root_component = fixture.services.components().root_components().front();
    const auto root = fixture.services.components().root(root_component);
    const auto children = fixture.nodes.require(root).children;
    require(children.size() == 5 && labels == 1, "mixed baseline row topology changed");
    const auto expected = baseline(fixture, children[0]);
    for (auto id : children) {
        require(near(baseline(fixture, id), expected),
                "text, nested Theme, Button, Input or Typography baseline differs");
    }
    for (const auto& mounted : fixture.services.text().mounted_texts()) {
        const auto& node = fixture.nodes.require(fixture.services.components().root(mounted.component));
        const auto& measurement = fixture.scene.text_state(mounted.scene).measurement();
        require(near(node.bounds.y + measurement.first_baseline, expected),
                "rendered Text or Button label differs from exported baseline");
    }
    const auto input = fixture.inputs.mounted_inputs().front();
    require(near(fixture.inputs.layout_snapshot(input.component).baseline, expected),
            "Input published a different rendered baseline");
    const auto shape_count =
        fixture.scene.text_state(fixture.services.text().mounted_texts().front().scene).counters().shape_count;
    const auto first_measure = fixture.nodes.require(children[0]).measure_count;
    direction.set(FlexDirection::RightToLeft);
    fixture.synchronize(640, {0, 0, 640, 240});
    require(
        fixture.nodes.require(children[0]).measure_count == first_measure &&
            fixture.scene.text_state(fixture.services.text().mounted_texts().front().scene).counters().shape_count ==
                shape_count,
        "direction-only placement remeasured or reshaped text");
    margin.set(dp(15));
    fixture.synchronize(640, {0, 0, 640, 240});
    for (auto id : children) {
        require(near(baseline(fixture, id), baseline(fixture, children[0])),
                "margin update retained stale line ascent");
    }
    large.text.tokens.font_size = dp(36);
    large.text.tokens.line_height = dp(50);
    theme.set(large);
    fixture.synchronize(640, {0, 0, 640, 240});
    for (auto id : children) {
        require(near(baseline(fixture, id), baseline(fixture, children[0])), "font update retained stale baseline");
    }
    align.set(FlexAlign::Center);
    fixture.synchronize(640, {0, 0, 640, 240});
    align.set(FlexAlign::Baseline);
    fixture.synchronize(640, {0, 0, 640, 240});
    require(labels == 1 && fixture.nodes.require(root).children == children, "baseline changes remounted labels");
    static_cast<void>(fixture.layout.layout(root, layout::Constraints::fixed(0, 0)));
    fixture.buttons.dispose();
    require(fixture.nodes.size() == 0 && fixture.services.text().mounted_texts().empty(), "baseline row leaked nodes");
    margin.set(dp(0));
    direction.set(FlexDirection::LeftToRight);
}

void default_stretch_and_align_self() {
    Fixture fixture;
    Signal<FlexAlignSelf> self{FlexAlignSelf::baseline};
    fixture.buttons.mount(Content{[&] {
        Flex(FlexProps{}.layout(LayoutStyle{}.height(dp(90))), FlexContent{[&] {
                 Text(u8"默认拉伸");
                 Text(TextProps{}.content(u8"基线").layout(LayoutStyle{}.align_self(self)));
             }});
    }});
    fixture.synchronize();
    const auto root = fixture.services.components().root(fixture.services.components().root_components().front());
    const auto children = fixture.nodes.require(root).children;
    require(near(fixture.nodes.require(children[0]).bounds.height, 90) &&
                fixture.nodes.require(children[1]).bounds.height < 90 &&
                fixture.nodes.require(children[1]).baseline_participant,
            "default Stretch or explicit align-self Baseline failed");
    self.set(FlexAlignSelf::stretch);
    fixture.synchronize();
    require(near(fixture.nodes.require(children[1]).bounds.height, 90), "align-self baseline removal did not stretch");
}

void nested_stretched_control_exports_actual_baseline() {
    Fixture fixture;
    fixture.buttons.mount(Content{[&] {
        Flex(FlexProps{}.align(FlexAlign::Baseline), FlexContent{[&] {
                 Flex(FlexProps{}.layout(LayoutStyle{}.height(dp(70))), FlexContent{[] {
                          Button(ButtonProps{}, [] { Text(u8"拉伸按钮 Ag"); });
                          Text(u8"相邻 Ag");
                      }});
                 Text(u8"外层 Ag");
             }});
    }});
    fixture.synchronize();
    const auto mounted = fixture.services.text().mounted_texts();
    const auto first = fixture.services.components().root(mounted[0].component);
    const auto last = fixture.services.components().root(mounted.back().component);
    const auto real_button_baseline =
        fixture.nodes.require(first).bounds.y + fixture.scene.text_state(mounted[0].scene).measurement().first_baseline;
    require(near(real_button_baseline, baseline(fixture, last)),
            "nested stretched Button exported its old height baseline");
}
} // namespace

int main() {
    try {
        real_text_controls_and_reactive_baselines();
        default_stretch_and_align_self();
        nested_stretched_control_exports_actual_baseline();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
