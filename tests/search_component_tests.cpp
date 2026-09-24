#include "support/input_fixture.hpp"

#include <ryn/search.hpp>

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

runtime::Point center(runtime::Rect bounds) {
    return {bounds.x + bounds.width / 2.0F, bounds.y + bounds.height / 2.0F};
}

void click(Fixture& fixture, runtime::NodeId node) {
    const auto point = center(fixture.nodes.require(node).bounds);
    fixture.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down,
        PointerButton::primary, point.x, point.y});
    fixture.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up,
        PointerButton::primary, point.x, point.y});
}

void enter(Fixture& fixture, bool repeat = false, KeyAction action = KeyAction::down) {
    fixture.services.focus().dispatch({Key::enter, action, KeyModifier::none, repeat});
}

void controlled_value_and_submit() {
    Fixture fixture;
    Signal<String> authoritative{String{u8"abc"}};
    int changes{}, submits{}, button_runs{};
    String submitted;
    SearchSource source{SearchSource::Input};
    fixture.buttons.mount(Content{[&] {
        Search(SearchProps{}.value(authoritative).placeholder(u8"查找")
            .enterButton(true)
            .onChange([&](String next) { ++changes; authoritative.set(std::move(next)); })
            .onSearch([&](String value, SearchSource origin) {
                ++submits; submitted = std::move(value); source = origin;
            })
            .layout(LayoutStyle{}.width(dp(280.0F))),
            SearchButtonContent{[&] { ++button_runs; Text(u8"查询"); }});
    }});
    require(fixture.inputs.mounted_inputs().size() == 1
        && fixture.buttons.mounted_buttons().size() == 1,
        "Search did not compose one Input and one Button");
    fixture.synchronize(400, {0, 0, 400, 240});
    const auto input = fixture.inputs.mounted_inputs().front();
    const auto button = fixture.buttons.mounted_buttons().front();
    require(button_runs == 1, "Search button slot did not mount exactly once");
    require(fixture.buttons.snapshot(button.component).type == ButtonType::Primary,
        "enterButton did not select primary Button semantics");
    const auto input_bounds = fixture.nodes.require(input.node).bounds;
    const auto button_bounds = fixture.nodes.require(button.node).bounds;
    require(input_bounds.x + input_bounds.width <= button_bounds.x + 0.1F,
        "Search Input overlaps Button");
    require(fixture.services.focus().request_focus(input.interaction, FocusModality::keyboard),
        "Search Input focus failed");
    require(bool(fixture.inputs.editors().require(input.editor).move(TextCaretMove::end)),
        "Search caret did not move to end");
    require(bool(fixture.inputs.dispatch(TextCommitted{String{u8"中"}, fixture.inputs.sessions().active()})),
        "Search Input commit failed");
    require(changes == 1 && authoritative.get() == String{u8"abc中"},
        "Search controlled echo failed");
    enter(fixture);
    enter(fixture, true);
    enter(fixture, false, KeyAction::up);
    require(submits == 1 && submitted == authoritative.get() && source == SearchSource::Input,
        "Search Enter duplicated or submitted stale value");
    authoritative.set(String{u8"程序更新"});
    click(fixture, button.node);
    require(submits == 2 && submitted == String{u8"程序更新"}
        && button_runs == 1,
        "Search button did not submit programmatic value or reran slot");
    require(fixture.services.text_edit() != nullptr
        && &fixture.inputs.editors() == &fixture.services.text_edit()->editors(),
        "Search created separate editor service");
}

void uncontrolled_and_gates() {
    Fixture fixture;
    Signal<bool> loading{false}, disabled{false}, read_only{false};
    int submits{}, changes{};
    String submitted;
    fixture.buttons.mount(Content{[&] {
        Search(SearchProps{}.defaultValue(u8"base").loading(loading)
            .disabled(disabled).readOnly(read_only)
            .onChange([&](String) { ++changes; })
            .onSearch([&](String value, SearchSource) { ++submits; submitted = std::move(value); })
            .layout(LayoutStyle{}.width(dp(260.0F))));
    }});
    fixture.synchronize(400, {0, 0, 400, 240});
    const auto input = fixture.inputs.mounted_inputs().front();
    const auto button = fixture.buttons.mounted_buttons().front();
    require(fixture.buttons.snapshot(button.component).type == ButtonType::Default,
        "default Search did not select default Button semantics");
    require(fixture.services.focus().request_focus(input.interaction, FocusModality::keyboard),
        "uncontrolled Search focus failed");
    require(bool(fixture.inputs.editors().require(input.editor).move(TextCaretMove::end)),
        "uncontrolled Search caret did not move to end");
    require(bool(fixture.inputs.dispatch(TextCommitted{String{u8"中"}, fixture.inputs.sessions().active()})),
        "uncontrolled Search commit failed");
    require(changes == 1, "uncontrolled Search onChange missing");
    loading.set(true);
    require(fixture.buttons.snapshot(button.component).loading,
        "Search loading did not reach Button");
    enter(fixture);
    click(fixture, button.node);
    require(submits == 0, "loading Search submitted");
    loading.set(false);
    disabled.set(true);
    click(fixture, button.node);
    require(submits == 0, "disabled Search submitted");
    disabled.set(false);
    read_only.set(true);
    click(fixture, button.node);
    require(submits == 0, "readOnly Search submitted");
    read_only.set(false);
    click(fixture, button.node);
    require(submits == 1 && submitted == String{u8"base中"},
        "uncontrolled Search did not submit current committed value");
}

void controlled_without_echo_and_multiple_searches() {
    Fixture fixture;
    Signal<String> authoritative{String{u8"fixed"}};
    int proposed{}, first_submits{}, second_submits{};
    String first_value, second_value;
    fixture.buttons.mount(Content{[&] {
        Search(SearchProps{}.value(authoritative)
            .onChange([&](String) { ++proposed; })
            .onSearch([&](String value, SearchSource) {
                ++first_submits; first_value = std::move(value);
            }).layout(LayoutStyle{}.width(dp(230.0F))));
        Search(SearchProps{}.defaultValue(u8"second")
            .onSearch([&](String value, SearchSource) {
                ++second_submits; second_value = std::move(value);
            }).layout(LayoutStyle{}.width(dp(230.0F))));
    }});
    fixture.synchronize(400, {0, 0, 400, 240});
    require(fixture.inputs.mounted_inputs().size() == 2
        && fixture.buttons.mounted_buttons().size() == 2,
        "two Searches did not mount independent children");
    const auto first_input = fixture.inputs.mounted_inputs()[0];
    const auto second_input = fixture.inputs.mounted_inputs()[1];
    const auto first_button = fixture.buttons.mounted_buttons()[0];
    const auto second_button = fixture.buttons.mounted_buttons()[1];
    require(fixture.services.focus().request_focus(first_input.interaction, FocusModality::keyboard),
        "first Search focus failed");
    require(bool(fixture.inputs.editors().require(first_input.editor).move(TextCaretMove::end)),
        "first Search caret did not move");
    require(bool(fixture.inputs.dispatch(TextCommitted{String{u8"x"}, fixture.inputs.sessions().active()})),
        "controlled no-echo edit failed");
    require(proposed == 1 && authoritative.get() == String{u8"fixed"},
        "controlled Search changed authoritative value without echo");
    enter(fixture);
    require(first_submits == 1 && first_value == String{u8"fixed"},
        "controlled Search Enter ignored authoritative value");
    const auto inside = center(fixture.nodes.require(first_button.node).bounds);
    fixture.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down,
        PointerButton::primary, inside.x, inside.y});
    fixture.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move,
        PointerButton::none, 390.0F, 230.0F});
    fixture.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up,
        PointerButton::primary, 390.0F, 230.0F});
    require(first_submits == 1, "canceled Search pointer press submitted");
    authoritative.set(String{u8"remote"});
    click(fixture, first_button.node);
    click(fixture, second_button.node);
    require(first_submits == 2 && first_value == String{u8"remote"}
        && second_submits == 1 && second_value == String{u8"second"},
        "multiple Searches shared submitted values or stale binding");
    const auto first_root = *fixture.buttons.components().parent(first_input.component);
    require(fixture.services.destroy(first_root), "controlled Search destroy failed");
    authoritative.set(String{u8"late"});
    require(fixture.inputs.mounted_inputs().size() == 1
        && fixture.inputs.mounted_inputs().front().component == second_input.component
        && fixture.buttons.mounted_buttons().size() == 1
        && fixture.inputs.editors().size() == 1,
        "destroyed Search binding affected sibling or retained editor");
}

void composition_and_lifecycle() {
    Fixture fixture;
    int submits{};
    String submitted;
    runtime::ComponentId root;
    fixture.buttons.mount(Content{[&] {
        Search(SearchProps{}.defaultValue(u8"已提交")
            .onSearch([&](String value, SearchSource) {
                ++submits; submitted = std::move(value);
                require(fixture.services.destroy(root), "Search self destroy failed");
            }).layout(LayoutStyle{}.width(dp(250.0F))));
    }});
    fixture.synchronize(400, {0, 0, 400, 240});
    const auto input = fixture.inputs.mounted_inputs().front();
    const auto button = fixture.buttons.mounted_buttons().front();
    root = *fixture.buttons.components().parent(input.component);
    require(fixture.buttons.components().parent(button.component) == root,
        "Search children have different roots");
    require(fixture.services.focus().request_focus(input.interaction, FocusModality::keyboard),
        "Search composition focus failed");
    const auto stamp = fixture.inputs.sessions().active();
    require(bool(fixture.inputs.dispatch(CompositionChanged{String{u8"临时"}, {2, 0}, stamp})),
        "Search composition event failed");
    enter(fixture);
    require(submits == 0, "Search submitted active composition on Enter");
    click(fixture, button.node);
    require(submits == 1 && submitted == String{u8"已提交"},
        "Search button submitted composition text");
    require(fixture.inputs.mounted_inputs().empty()
        && fixture.buttons.mounted_buttons().empty()
        && fixture.inputs.editors().size() == 0,
        "Search self destroy leaked child identities");
    require(!fixture.inputs.dispatch(TextCommitted{String{u8"迟到"}, stamp}),
        "Search accepted stale input after destroy");
}

void invalid_mount() {
    Fixture fixture;
    bool rejected{};
    try {
        fixture.buttons.mount(Content{[] {
            Search(SearchProps{}.value(u8"a").defaultValue(u8"b"));
        }});
    } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected && fixture.inputs.mounted_inputs().empty()
        && fixture.buttons.mounted_buttons().empty()
        && fixture.services.interactions().size() == 0,
        "conflicting Search Props acquired identities");
}

void layout_theme_and_scale() {
    for (const float scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
        for (const auto algorithm : {ThemeAlgorithm::Default, ThemeAlgorithm::Dark,
                                     ThemeAlgorithm::Compact}) {
            for (const auto size : {ControlSize::Small, ControlSize::Middle,
                                    ControlSize::Large}) {
                Fixture fixture;
                fixture.font_scale = scale;
                fixture.inputs.set_display_scale(scale);
                ThemeConfig config;
                config.algorithms = {algorithm};
                Signal<ThemeConfig> theme{config};
                int content_runs{};
                fixture.buttons.mount(Content{[&] {
                    Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                        ++content_runs;
                        Search(SearchProps{}.defaultValue(u8"很长的 Search 查询文本 abcdefghijklmnop")
                            .size(size).enterButton(true)
                            .layout(LayoutStyle{}.width(dp(130.0F))));
                    }});
                }});
                fixture.synchronize(320);
                const auto input = fixture.inputs.mounted_inputs().front();
                const auto button = fixture.buttons.mounted_buttons().front();
                const auto input_bounds = fixture.nodes.require(input.node).bounds;
                const auto button_bounds = fixture.nodes.require(button.node).bounds;
                const auto root = *fixture.buttons.components().parent(input.component);
                const auto root_bounds = fixture.nodes.require(fixture.buttons.components().root(root)).bounds;
                require(input_bounds.width > 0.0F && button_bounds.width > 0.0F
                    && input_bounds.x + input_bounds.width <= button_bounds.x + 0.1F
                    && button_bounds.x + button_bounds.width <= root_bounds.x + root_bounds.width + 0.1F,
                    "narrow Search geometry overlaps or escapes root");
                require(std::fabs(input_bounds.height - button_bounds.height) < 0.1F
                    && fixture.buttons.snapshot(button.component).size == size,
                    "Search Input/Button control heights or sizes differ");
                require(fixture.services.focus().request_focus(input.interaction, FocusModality::keyboard),
                    "scaled Search focus failed");
                require(bool(fixture.inputs.editors().require(input.editor).move(TextCaretMove::end)),
                    "scaled Search caret failed to move");
                fixture.synchronize(320);
                const auto geometry = fixture.inputs.layout_snapshot(input.component);
                require(geometry.caret.x >= geometry.clip.x - 0.01F
                    && geometry.caret.x + geometry.caret.width <=
                        geometry.clip.x + geometry.clip.width + 0.01F,
                    "scaled Search caret escaped clip");
                const auto scene = fixture.inputs.text_scene(input.component);
                const auto shapes = fixture.scene.text_state(scene).counters().shape_count;
                const auto measures = fixture.nodes.require(input.node).measure_count;
                const auto mounts = fixture.buttons.components().mount_runs();
                const auto rebuilds = fixture.buttons.scene_composer().diagnostics().rebuilds;
                config.seed.color_primary = Color::rgba8(114, 46, 209);
                theme.set(config);
                fixture.synchronize(320);
                require(content_runs == 1
                    && fixture.buttons.components().mount_runs() == mounts
                    && fixture.scene.text_state(scene).counters().shape_count == shapes
                    && fixture.nodes.require(input.node).measure_count == measures
                    && fixture.buttons.scene_composer().diagnostics().rebuilds == rebuilds,
                    "Search color change reran content, shape, measure or scene topology");
            }
        }
    }
}

} // namespace

int main() {
    try {
        invalid_mount();
        controlled_value_and_submit();
        uncontrolled_and_gates();
        controlled_without_echo_and_multiple_searches();
        composition_and_lifecycle();
        layout_theme_and_scale();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
