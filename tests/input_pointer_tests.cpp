#include "support/input_fixture.hpp"
#include "support/allocation_probe.hpp"
#include <iostream>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
struct Journey {
    Fixture f;
    detail::MountedInputComponent input;
    float width{320};
    void mount(String value, bool affixes = false) {
        f.inputs.mount(Content{[&] {
            Input(InputProps{}.defaultValue(value),
                affixes ? std::optional<InputPrefix>{InputPrefix{[] { Text(u8"前"); }}} : std::nullopt,
                affixes ? std::optional<InputSuffix>{InputSuffix{[] { Text(u8"后"); }}} : std::nullopt);
        }});
        input = f.inputs.mounted_inputs().front(); f.synchronize(width);
    }
    TextEditorState& editor() { return f.inputs.editors().require(input.editor); }
    detail::InputLayoutSnapshot geometry() { return f.inputs.layout_snapshot(input.component); }
    float x(std::size_t byte) {
        const auto& map = f.inputs.caret_map(input.component);
        return geometry().viewport.x + map.at(byte, map.revision()).value().x - geometry().scroll_offset;
    }
    void send(PointerAction action, float x, std::uint8_t clicks = 0,
        PointerIdentity pointer = PointerIdentity::mouse(), bool sync = true) {
        const auto button = action == PointerAction::down || action == PointerAction::up
            ? PointerButton::primary : PointerButton::none;
        f.buttons.pointer().dispatch({pointer, action, button, x, geometry().viewport.y + 5, clicks});
        if(sync) f.synchronize(width);
    }
};
void placement() {
    for(const auto value : {String{u8"office abc"}, String{u8"中文测试"}, String{u8"a é 👩‍👩‍👧‍👦 z"}}) {
        Journey j; j.mount(value, true);
        const auto& map = j.f.inputs.caret_map(j.input.component);
        for(const auto stop : map.stops()) {
            // Duplicate x stops intentionally resolve to the earliest boundary.
            const auto expected = map.nearest(stop.x, map.revision()).value().byte;
            const float x = j.geometry().viewport.x + stop.x;
            j.send(PointerAction::down, x);
            require(j.editor().selection() == TextSelection{expected, expected}, "pointer split/missed grapheme");
            require(j.f.buttons.pointer().state(PointerIdentity::mouse())->capture == j.input.interaction, "click capture absent");
            j.send(PointerAction::up, x);
        }
        require(j.editor().value() == value.bytes() && j.editor().history().undo_count == 0, "selection changed value/history");
        const auto selection = j.editor().selection();
        const auto root = j.f.nodes.require(j.input.node).bounds;
        j.send(PointerAction::down, root.x + 2); j.send(PointerAction::up, root.x + 2);
        require(j.editor().selection() == selection, "prefix/padding changed text selection");
        j.send(PointerAction::down, root.x + root.width - 2); j.send(PointerAction::up, root.x + root.width - 2);
        require(j.editor().selection() == selection, "suffix/padding changed text selection");
    }
}
void drag_and_lifecycle() {
    Journey j; j.mount(String{u8"abcdef"});
    j.send(PointerAction::down, j.x(2));
    const auto touch = PointerIdentity::touch(4, 2);
    j.send(PointerAction::down, j.x(5), 0, touch);
    j.send(PointerAction::move, j.x(4), 0, touch);
    require(j.editor().selection() == TextSelection{2, 2}, "second pointer stole selection");
    j.send(PointerAction::up, j.x(4), 0, touch);
    j.send(PointerAction::move, 500);
    require(j.editor().selection() == TextSelection{2, 6}, "outside drag did not extend/clamp");
    j.send(PointerAction::up, -100);
    require(j.editor().selection() == TextSelection{2, 0}, "outside release ignored final position");
    require(!j.f.buttons.pointer().state(PointerIdentity::mouse())->capture, "release retained capture");
    j.send(PointerAction::move, j.x(4));
    require(j.editor().selection() == TextSelection{2, 0}, "released pointer kept selecting");
    j.send(PointerAction::down, j.x(1)); j.send(PointerAction::cancel, j.x(1));
    j.send(PointerAction::move, j.x(4));
    require(j.editor().selection() == TextSelection{1, 1}, "cancel kept selecting");
    j.send(PointerAction::down, j.x(2)); j.f.inputs.set_window_active(false);
    require(!j.f.buttons.pointer().state(PointerIdentity::mouse())->capture, "window loss retained capture");
    j.f.inputs.set_window_active(true);
    j.send(PointerAction::down, j.x(2));
    require(j.f.buttons.destroy(j.input.component), "destroy failed");
    require(!j.f.buttons.pointer().state(PointerIdentity::mouse())->capture, "destroy retained capture");
}
void words_and_composition() {
    Journey j; j.mount(String{u8"hello world 中文 👩‍👩‍👧‍👦"});
    j.send(PointerAction::down, j.x(2), 2); j.send(PointerAction::move, j.x(2)); j.send(PointerAction::up, j.x(2), 2);
    require(j.editor().selection() == TextSelection{0, 5}, "double click word collapsed on release");
    j.send(PointerAction::down, j.x(12), 2); j.send(PointerAction::up, j.x(12), 2);
    require(j.editor().selection() == TextSelection{12, 18}, "CJK logical word mismatch");
    j.send(PointerAction::down, j.x(19), 2); j.send(PointerAction::up, j.x(19), 2);
    require(j.editor().selection() == TextSelection{19, j.editor().value().size()}, "emoji word split cluster");
    require(bool(j.editor().place(6)), "composition position failed");
    require(bool(j.f.inputs.dispatch(CompositionChanged{String{u8"你好"}, {2, 0}, j.f.inputs.sessions().active()})), "composition failed");
    j.f.synchronize();
    // Display suffix byte 12 is committed byte 6 after the inserted preedit.
    j.send(PointerAction::down, j.x(12));
    require(!j.editor().composition().active && j.editor().selection() == TextSelection{6, 6}, "preedit pointer mapping/cancel failed");
    j.send(PointerAction::up, j.x(6));
}
void scroll_and_allocation() {
    Journey j; j.width = 90; j.mount(String{u8"abcdefghijklmnopqrstuvwxyz"});
    j.send(PointerAction::down, j.x(0)); j.send(PointerAction::move, 200);
    require(j.geometry().scroll_offset > 0, "drag did not reveal caret");
    j.send(PointerAction::up, 200);
    const auto geometry = j.geometry(); const auto& map = j.f.inputs.caret_map(j.input.component);
    const float local = geometry.viewport.width / 2;
    const auto expected = map.nearest(local + geometry.scroll_offset, map.revision()).value().byte;
    j.send(PointerAction::down, geometry.viewport.x + local);
    require(j.editor().selection().caret == expected, "scroll hit used unscrolled text position");
    j.send(PointerAction::up, geometry.viewport.x + local);
    j.send(PointerAction::down, j.geometry().viewport.x + 1);
    const auto cycle = [&](int i) { j.send(PointerAction::move, i % 2 ? -100.0F : 200.0F); };
    for(int i = 0; i < 20; ++i) cycle(i);
    const auto scene = j.f.inputs.text_scene(j.input.component);
    const auto shapes = j.f.scene.text_state(scene).counters().shape_count;
    const auto measures = j.f.nodes.require(j.input.node).measure_count;
    const auto rebuilds = j.f.buttons.scene_composer().diagnostics().rebuilds;
    ryn_test::allocation::begin();
    for(int i = 0; i < 20000; ++i) cycle(i);
    const auto allocations = ryn_test::allocation::end();
    require(allocations == 0, "pointer selection allocated after warmup");
    require(j.f.scene.text_state(scene).counters().shape_count == shapes
        && j.f.nodes.require(j.input.node).measure_count == measures
        && j.f.buttons.scene_composer().diagnostics().rebuilds == rebuilds, "pointer selection rebuilt text/layout/scene");
}
void eligibility() {
    Fixture f; Signal<bool> disabled{false}, read_only{true};
    f.inputs.mount(Content{[&] { Input(InputProps{}.defaultValue(u8"hello").disabled(disabled).readOnly(read_only)); }});
    f.synchronize(); const auto input = f.inputs.mounted_inputs().front();
    auto& editor = f.inputs.editors().require(input.editor);
    const auto geometry = f.inputs.layout_snapshot(input.component);
    const auto& map = f.inputs.caret_map(input.component);
    const float x = geometry.viewport.x + map.at(2, map.revision()).value().x;
    const PointerInputEvent down{PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, x, geometry.viewport.y + 5};
    f.buttons.pointer().dispatch(down);
    require(editor.selection().caret == 2 && !f.inputs.sessions().active().valid(), "readOnly selection/session mismatch");
    disabled.set(true); f.synchronize();
    require(!f.buttons.pointer().state(PointerIdentity::mouse())->capture, "disabled retained capture");
    f.buttons.pointer().dispatch(down);
    require(editor.selection().caret == 2, "disabled changed selection");
}
void stale_text_and_clip() {
    for(float scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
        Fixture f; Signal<String> value{String{u8"abcdef"}};
        f.font_scale = scale; f.inputs.set_display_scale(scale);
        f.inputs.mount(Content{[&] { Input(InputProps{}.value(value)); }});
        f.synchronize(320, {25, 0, 100, 240});
        const auto input = f.inputs.mounted_inputs().front();
        auto& editor = f.inputs.editors().require(input.editor);
        const auto geometry = f.inputs.layout_snapshot(input.component);
        const auto& map = f.inputs.caret_map(input.component);
        const auto send = [&](PointerAction action, float x) {
            f.buttons.pointer().dispatch({PointerIdentity::mouse(), action,
                action == PointerAction::down || action == PointerAction::up ? PointerButton::primary : PointerButton::none,
                x, geometry.viewport.y + 5});
        };
        const auto initial = editor.selection();
        send(PointerAction::down, geometry.clip.x - 1);
        require(editor.selection() == initial, "ancestor-clipped text accepted down");
        send(PointerAction::up, geometry.clip.x - 1);
        // A Props update and a pointer event may arrive before the next frame.
        value.set(String{u8"中文"});
        send(PointerAction::down, geometry.clip.x + geometry.clip.width - 1);
        require(editor.selection().caret == editor.value().size()
            && map.revision() == f.scene.text_state(f.inputs.text_scene(input.component)).revision(),
            "pointer reused stale caret revision after controlled update");
        send(PointerAction::up, geometry.clip.x + geometry.clip.width - 1);
        f.synchronize(320, {25, 0, 100, 240});
    }
}
}
int main() {
    try { placement(); drag_and_lifecycle(); words_and_composition(); scroll_and_allocation(); eligibility(); stale_text_and_clip(); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
