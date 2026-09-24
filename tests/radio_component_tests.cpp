#include "component/selection_component.hpp"
#include "support/input_fixture.hpp"

#include <ryn/rynui.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

runtime::Point center(const Fixture& fixture, runtime::NodeId node) {
    const auto bounds = fixture.nodes.require(node).bounds;
    return {bounds.x + bounds.width / 2.0F, bounds.y + bounds.height / 2.0F};
}

runtime::Rect quad_bounds(const graphics::QuadInstance& quad,
    runtime::Size viewport = {320.0F, 240.0F}) {
    return {(quad.clip_rect[0] + 1.0F) * viewport.width / 2.0F,
        (1.0F - quad.clip_rect[1]) * viewport.height / 2.0F,
        quad.clip_rect[2] * viewport.width / 2.0F,
        -quad.clip_rect[3] * viewport.height / 2.0F};
}

bool near(float left, float right) { return std::fabs(left - right) < 0.02F; }

void click(Fixture& fixture, runtime::Point point) {
    fixture.services.pointer().dispatch({PointerIdentity::mouse(),
        PointerAction::down, PointerButton::primary, point.x, point.y});
    fixture.services.pointer().dispatch({PointerIdentity::mouse(),
        PointerAction::up, PointerButton::primary, point.x, point.y});
}

KeyboardInputEvent key(Key value, KeyAction action, bool repeat = false) {
    return {value, action, KeyModifier::none, repeat};
}

void standalone_modes() {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    Signal<bool> controlled{false}, disabled{false};
    int controlled_calls{}, local_calls{}, labels{};
    host.mount(Content{[&] {
        Radio(RadioProps{}.checked(controlled).onChange([&](bool value) {
            ++controlled_calls;
            controlled.set(value);
        }), RadioLabel{[&] { ++labels; Text(u8"中文 Choice"); }});
        Radio(RadioProps{}.defaultChecked(false).disabled(disabled)
            .onChange([&](bool value) { require(value, "Radio toggled off"); ++local_calls; }),
            RadioLabel{[] { Text(u8"Second"); }});
    }});
    fixture.synchronize();
    require(host.mounted().size() == 2 && labels == 1, "Radio children did not mount once");
    const auto first = host.mounted()[0], second = host.mounted()[1];
    require(first.radio && second.radio && !host.snapshot(first.component).checked,
        "Radio initial state mismatch");
    require(fixture.services.focus().request_focus(first.interaction, FocusModality::keyboard),
        "Radio focus failed");
    fixture.services.focus().dispatch(key(Key::enter, KeyAction::down));
    require(controlled_calls == 0, "Enter activated Radio");
    fixture.services.focus().dispatch(key(Key::space, KeyAction::down));
    fixture.services.focus().dispatch(key(Key::space, KeyAction::down, true));
    fixture.services.focus().dispatch(key(Key::space, KeyAction::up));
    require(controlled_calls == 1 && controlled.get()
        && host.snapshot(first.component).checked, "controlled Radio did not echo once");
    fixture.services.focus().dispatch(key(Key::space, KeyAction::down));
    fixture.services.focus().dispatch(key(Key::space, KeyAction::up));
    require(controlled_calls == 1 && labels == 1, "checked Radio repeated or remounted label");
    click(fixture, center(fixture, second.node));
    click(fixture, center(fixture, second.node));
    require(local_calls == 1 && host.snapshot(second.component).checked,
        "standalone Radio did not select exactly once");
    disabled.set(true);
    require(host.snapshot(second.component).disabled, "Radio disabled binding failed");
    click(fixture, center(fixture, second.node));
    require(local_calls == 1, "disabled Radio activated");
}

void group_modes() {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    Signal<std::optional<String>> selected{std::optional<String>{String{u8"a"}}};
    Signal<bool> disabled{false};
    int calls{};
    host.mount(Content{[&] {
        RadioGroup(RadioGroupProps{}.options({
            {String{u8"a"}, String{u8"甲 Alpha"}},
            {String{u8"b"}, String{u8"乙 Beta"}},
            {String{u8"c"}, String{u8"丙 Gamma"}, true},
        }).value(selected).disabled(disabled).onChange([&](const String& value) {
            ++calls;
            selected.set(std::optional<String>{value});
        }));
    }});
    fixture.synchronize();
    require(host.mounted().size() == 3, "RadioGroup options did not mount");
    const auto a = host.mounted()[0], b = host.mounted()[1], c = host.mounted()[2];
    require(host.snapshot(a.component).checked && !host.snapshot(b.component).checked
        && host.snapshot(c.component).disabled, "RadioGroup initial state mismatch");
    click(fixture, center(fixture, a.node));
    require(calls == 0, "RadioGroup repeated current value");
    click(fixture, center(fixture, b.node));
    require(calls == 1 && selected.get() == String{u8"b"}
        && !host.snapshot(a.component).checked && host.snapshot(b.component).checked,
        "RadioGroup failed single selection echo");
    click(fixture, center(fixture, c.node));
    require(calls == 1, "disabled RadioGroup option activated");
    disabled.set(true);
    require(host.snapshot(a.component).disabled && host.snapshot(b.component).disabled,
        "RadioGroup disabled did not fan out");
    disabled.set(false);
    selected.set(std::optional<String>{String{u8"a"}});
    require(host.snapshot(a.component).checked && !host.snapshot(b.component).checked
        && calls == 1, "external RadioGroup value did not update locally");
    require(fixture.services.focus().request_focus(b.interaction, FocusModality::keyboard),
        "RadioGroup option focus failed");
    fixture.services.focus().dispatch(key(Key::enter, KeyAction::down));
    require(calls == 1, "Enter activated RadioGroup option");
    fixture.services.focus().dispatch(key(Key::space, KeyAction::down));
    fixture.services.focus().dispatch(key(Key::space, KeyAction::up));
    require(calls == 2 && host.snapshot(b.component).checked,
        "RadioGroup Space did not select the focused option");
    const auto group = fixture.services.components().parent(a.component);
    require(group && fixture.services.destroy(*group), "RadioGroup destroy failed");
    require(host.mounted().empty() && fixture.services.interactions().size() == 0,
        "RadioGroup destroy leaked children");
}

void group_uncontrolled_and_invalid() {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    int calls{};
    host.mount(Content{[&] {
        RadioGroup(RadioGroupProps{}.options({
            {String{u8"one"}, String{u8"One"}},
            {String{u8"two"}, String{u8"Two"}},
        }).defaultValue(String{u8"one"})
            .orientation(RadioGroupOrientation::Vertical)
            .onChange([&](const String&) { ++calls; }));
    }});
    fixture.synchronize();
    const auto one = host.mounted()[0], two = host.mounted()[1];
    require(fixture.nodes.require(two.node).bounds.y
        > fixture.nodes.require(one.node).bounds.y, "vertical RadioGroup did not stack");
    click(fixture, center(fixture, two.node));
    require(calls == 1 && !host.snapshot(one.component).checked
        && host.snapshot(two.component).checked, "uncontrolled RadioGroup failed to switch");

    Fixture invalid;
    detail::SelectionComponentHost invalid_host{invalid.services};
    bool rejected{};
    try {
        invalid_host.mount(Content{[] {
            RadioGroup(RadioGroupProps{}.options({
                {String{u8"x"}, String{u8"X"}},
                {String{u8"x"}, String{u8"Duplicate"}},
            }));
        }});
    } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected && invalid_host.mounted().empty()
        && invalid.services.interactions().size() == 0,
        "duplicate RadioGroup values acquired resources");
    Fixture invalid_radio;
    detail::SelectionComponentHost invalid_radio_host{invalid_radio.services};
    rejected = false;
    try {
        invalid_radio_host.mount(Content{[] {
            Radio(RadioProps{}.checked(true).defaultChecked(false));
        }});
    } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected && invalid_radio_host.mounted().empty(),
        "conflicting Radio modes were accepted");
    Fixture invalid_group;
    detail::SelectionComponentHost invalid_group_host{invalid_group.services};
    rejected = false;
    try {
        invalid_group_host.mount(Content{[] {
            RadioGroup(RadioGroupProps{}.value(std::optional<String>{String{u8"a"}})
                .defaultValue(String{u8"b"}));
        }});
    } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected && invalid_group_host.mounted().empty()
        && invalid_group.services.interactions().size() == 0,
        "conflicting RadioGroup modes acquired resources");
}

void self_destroy() {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    runtime::ComponentId id;
    int calls{};
    host.mount(Content{[&] {
        Radio(RadioProps{}.onChange([&](bool) {
            ++calls;
            require(fixture.services.destroy(id), "Radio self-destroy failed");
        }));
    }});
    fixture.synchronize();
    id = host.mounted().front().component;
    click(fixture, center(fixture, host.mounted().front().node));
    require(calls == 1 && host.mounted().empty()
        && fixture.services.interactions().size() == 0,
        "Radio self-destroy leaked or repeated activation");
}

void controlled_group_waits_for_echo() {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    Signal<std::optional<String>> selected{std::optional<String>{String{u8"a"}}};
    int calls{};
    host.mount(Content{[&] {
        RadioGroup(RadioGroupProps{}.options({
            {String{u8"a"}, String{u8"A"}},
            {String{u8"b"}, String{u8"B"}},
        }).value(selected).onChange([&](const String&) { ++calls; }));
    }});
    fixture.synchronize();
    const auto a = host.mounted()[0], b = host.mounted()[1];
    click(fixture, center(fixture, b.node));
    require(calls == 1 && host.snapshot(a.component).checked
        && !host.snapshot(b.component).checked,
        "controlled RadioGroup changed before echo");
    selected.set(std::optional<String>{String{u8"b"}});
    require(!host.snapshot(a.component).checked && host.snapshot(b.component).checked
        && calls == 1, "controlled RadioGroup echo repeated callback");
}

void group_self_destroy() {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    runtime::ComponentId group_id;
    int calls{};
    host.mount(Content{[&] {
        RadioGroup(RadioGroupProps{}.options({
            {String{u8"a"}, String{u8"A"}},
            {String{u8"b"}, String{u8"B"}},
        }).defaultValue(String{u8"a"}).onChange([&](const String&) {
            ++calls;
            require(fixture.services.destroy(group_id), "RadioGroup self-destroy failed");
        }));
    }});
    fixture.synchronize();
    const auto target = host.mounted()[1];
    group_id = *fixture.services.components().parent(target.component);
    click(fixture, center(fixture, target.node));
    require(calls == 1 && host.mounted().empty()
        && fixture.services.interactions().size() == 0,
        "RadioGroup self-destroy leaked or repeated activation");
}

void visuals_and_theme() {
    for (const float scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
        Fixture fixture;
        fixture.font_scale = scale;
        detail::SelectionComponentHost host{fixture.services};
        ThemeConfig config;
        Signal<ThemeConfig> theme{config};
        Signal<bool> checked{false};
        int label_runs{};
        fixture.buttons.mount(Content{[&] {
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                Radio(RadioProps{}.checked(checked),
                    RadioLabel{[&] { ++label_runs; Text(u8"单选 Radio"); }});
                Checkbox(CheckboxProps{}.defaultChecked(true),
                    CheckboxLabel{[] { Text(u8"Sibling"); }});
            }});
        }});
        fixture.synchronize();
        const auto radio = host.mounted()[0], sibling = host.mounted()[1];
        const auto& snapshot = fixture.services.components().theme_scope(radio.component)
            ->snapshot();
        auto& surfaces = fixture.services.surfaces();
        const auto range = surfaces.visual_range(radio.surface);
        const auto sibling_range = surfaces.visual_range(sibling.surface);
        require(range.count == 3 && sibling_range.count == 15,
            "Radio retained topology mismatch");
        const auto ring = quad_bounds(surfaces.instances().at(range.first));
        const auto dot = quad_bounds(surfaces.instances().at(range.first + 2));
        require(near(ring.width, snapshot.map().font_size_large)
            && near(dot.width, snapshot.map().font_size_large
                - 2.0F * (4.0F + snapshot.seed().line_width))
            && near(dot.x + dot.width / 2.0F, ring.x + ring.width / 2.0F),
            "Radio geometry did not map 6.6.5 tokens");
        const auto root = fixture.nodes.require(radio.node).bounds;
        require(root.width > ring.width + 15.0F,
            "Radio label did not extend hit area");
        surfaces.instances().clear_dirty_ranges();
        const auto mount_runs = fixture.services.components().mount_runs();
        const auto scene_creates = surfaces.diagnostics().creates;
        const auto scene_rebuilds = fixture.services.scene_composer().diagnostics().rebuilds;
        checked.set(true);
        fixture.synchronize();
        require(host.snapshot(radio.component).checked && label_runs == 1
            && fixture.services.components().mount_runs() == mount_runs
            && surfaces.diagnostics().creates == scene_creates,
            "Radio checked update rebuilt identities");
        for (const auto dirty : surfaces.instances().material_dirty_ranges()) {
            require(dirty.first >= range.first
                && dirty.first + dirty.count <= range.first + range.count,
                "Radio checked update uploaded sibling material");
        }
        require(fixture.dirty.layout_roots().empty(),
            "Radio checked update measured content");
        config.seed.color_primary = Color::rgba8(114, 46, 209);
        theme.set(config);
        fixture.synchronize();
        require(fixture.services.components().mount_runs() == mount_runs
            && surfaces.diagnostics().creates == scene_creates
            && fixture.services.scene_composer().diagnostics().rebuilds == scene_rebuilds
            && fixture.dirty.layout_roots().empty(),
            "Radio theme recolor rebuilt layout or scene");
        for (const auto algorithm : {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
            config.algorithms = {algorithm};
            theme.set(config);
            fixture.synchronize();
            require(host.mounted()[0].surface == radio.surface && label_runs == 1,
                "Radio theme change remounted label or surface");
        }
    }
}
} // namespace

int main() {
    try {
        standalone_modes();
        group_modes();
        group_uncontrolled_and_invalid();
        self_destroy();
        controlled_group_waits_for_echo();
        group_self_destroy();
        visuals_and_theme();
        std::cout << "radio component tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "radio component tests failed: " << error.what() << '\n';
        return 1;
    }
}
