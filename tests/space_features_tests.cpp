#include "support/input_fixture.hpp"
#include "component/space_component.hpp"

#include <algorithm>
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

void separator_order_retention_and_deletion() {
    Fixture fixture;
    Signal<bool> wrap{false};
    Signal<FlexDirection> direction{FlexDirection::LeftToRight};
    int content_runs{};
    int separators{};
    ThemeConfig large;
    large.text.tokens.font_size = dp(24);
    fixture.buttons.mount(Content{[&] {
        Space(SpaceProps{}.wrap(wrap).direction(direction).size(dp(4)).separator(SpaceSeparator{[&] {
            ++separators;
            Text(u8"/");
            Button(ButtonProps{}, [] { Text(u8"分隔操作"); });
        }}),
              SpaceContent{[&] {
                  ++content_runs;
                  Button(ButtonProps{}, [] { Text(u8"一"); });
                  Theme(ThemeProps{}.config(large), ThemeContent{[] {
                            Button(ButtonProps{}, [] { Text(u8"二"); });
                            Button(ButtonProps{}, [] { Text(u8"三"); });
                        }});
              }});
    }});
    fixture.synchronize(640, {0, 0, 640, 240});
    auto& host = fixture.services.components();
    const auto component = host.root_components().front();
    const auto node = host.root(component);
    const auto children = host.children(component);
    require(children.size() == 5 && separators == 2 && content_runs == 1,
            "separator count or transparent slots failed");
    const auto buttons = fixture.buttons.mounted_buttons();
    require(buttons.size() == 5, "rich separator did not mount all controls");
    const auto order = fixture.services.interactions().declaration_order();
    const auto paint = host.paint_traversal();
    std::size_t last_paint{};
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        require(order[i] == buttons[i].interaction, "separator focus registration is not interleaved");
        fixture.services.focus().dispatch({input::Key::tab, input::KeyAction::down});
        require(fixture.services.focus().state().focused == buttons[i].interaction,
                "Tab did not traverse primary and separator controls in order");
        const auto found = std::find_if(paint.begin(), paint.end(),
                                        [&](const auto& entry) { return entry.fragment == buttons[i].fragment; });
        require(found != paint.end(), "separator button scene fragment missing");
        const auto position = static_cast<std::size_t>(found - paint.begin());
        require(i == 0 || position > last_paint, "separator scene order is not interleaved");
        last_paint = position;
        if (i > 0) {
            require(fixture.nodes.require(buttons[i].node).bounds.x >
                        fixture.nodes.require(buttons[i - 1].node).bounds.x,
                    "separator layout is not interleaved");
        }
    }
    const auto texts = fixture.services.text().mounted_texts();
    require(host.theme_scope(children[1]) == host.theme_scope(component) &&
                host.theme_scope(children[3]) == host.theme_scope(component),
            "separator inherited adjacent item Theme instead of Space Theme");
    const auto shape_count = fixture.scene.text_state(texts.front().scene).counters().shape_count;
    direction.set(FlexDirection::RightToLeft);
    fixture.synchronize(640, {0, 0, 640, 240});
    require(host.children(component) == children && content_runs == 1 && separators == 2 &&
                fixture.scene.text_state(texts.front().scene).counters().shape_count == shape_count &&
                fixture.nodes.require(buttons.front().node).bounds.x >
                    fixture.nodes.require(buttons.back().node).bounds.x,
            "RTL separator update remounted or reshaped content");
    wrap.set(true);
    fixture.synchronize(140, {0, 0, 140, 240});
    require(host.children(component) == children && separators == 2, "wrap rebuilt separator branches");
    static_cast<void>(fixture.layout.layout(node, layout::Constraints::fixed(0, 0)));
    require(fixture.buttons.destroy(children[2]), "middle primary deletion failed");
    fixture.synchronize();
    require(host.children(component).size() == 3 && !host.contains(children[1]) && host.contains(children[3]),
            "middle deletion left duplicate separators");
    require(fixture.buttons.destroy(children[0]), "first primary deletion failed");
    fixture.synchronize();
    require(host.children(component).size() == 1 && !host.contains(children[3]),
            "first deletion left a leading separator");
    fixture.buttons.dispose();
    require(fixture.nodes.size() == 0 && fixture.services.text().mounted_texts().empty() &&
                fixture.services.interactions().size() == 0,
            "separator disposal leaked resources");
    wrap.set(false);
    direction.set(FlexDirection::LeftToRight);
}

void separator_boundaries_and_rollback() {
    Fixture fixture;
    int separators{};
    fixture.buttons.mount(Content{[&] {
        Space(SpaceProps{}.separator([&] { ++separators; }), [] {});
        Space(SpaceProps{}.split([&] { ++separators; }), [] { Text(u8"单项"); });
    }});
    fixture.synchronize();
    require(separators == 0, "empty or single item executed separator");
    Fixture failed;
    bool rejected{};
    try {
        failed.buttons.mount(Content{[] {
            Space(SpaceProps{}.separator([] {
                Text(u8"已挂载分隔");
                throw std::runtime_error("separator failure");
            }),
                  [] {
                      Button(ButtonProps{}, [] { Text(u8"一"); });
                      Text(u8"二");
                  });
        }});
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected && failed.nodes.size() == 0 && failed.services.components().component_count() == 0 &&
                failed.services.text().mounted_texts().empty() && failed.services.interactions().size() == 0,
            "separator exception did not roll back mount");
}
} // namespace

int main() {
    try {
        defaults_orientation_and_last_setter();
        real_baseline_and_nested_direction();
        separator_order_retention_and_deletion();
        separator_boundaries_and_rollback();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
