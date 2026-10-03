#include "support/input_fixture.hpp"

#include <cmath>
#include <iostream>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void fonts(Fixture& f) {
    for (const auto size : {14U, 16U, 18U}) {
        auto chain = f.resolve(size);
        const auto arabic = f.fonts->load_font_file(RYNUI_VALIDATION_ARABIC_FONT, 0, size);
        const auto hebrew = f.fonts->load_font_file(RYNUI_VALIDATION_HEBREW_FONT, 0, size);
        check(arabic && hebrew, "bidi input fonts");
        chain.push_back(arabic.font);
        chain.push_back(hebrew.font);
        f.chains[size] = std::move(chain);
    }
}

void key(Fixture& f, input::Key key, input::KeyModifier modifiers = input::KeyModifier::none) {
    f.services.focus().dispatch({key, input::KeyAction::down, modifiers});
    f.services.focus().dispatch({key, input::KeyAction::up, modifiers});
}

void visual_navigation_and_transactions() {
    Fixture f;
    fonts(f);
    InputRef ref;
    f.inputs.mount(Content{[&] { Input(InputProps{}.defaultValue(u8"אבג").ref(ref)); }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    auto& editor = f.inputs.editors().require(mounted.editor);
    check(ref.focus({InputFocusCursor::Start}), "RTL focus");
    f.synchronize();
    const auto right_x = f.inputs.layout_snapshot(mounted.component).caret_x;
    key(f, input::Key::left);
    f.synchronize();
    check(editor.selection().caret == 2 && f.inputs.layout_snapshot(mounted.component).caret_x < right_x,
          "Left did not move visually left in RTL");
    key(f, input::Key::home);
    check(editor.selection().caret == 6, "Home is not physical left");
    key(f, input::Key::end);
    check(editor.selection().caret == 0, "End is not physical right");
    key(f, input::Key::left, input::KeyModifier::shift);
    check(editor.selection() == input::TextSelection{0, 2}, "RTL Shift selection lost logical anchor");
    key(f, input::Key::c, input::KeyModifier::control);
    check(f.platform.clipboard == String{u8"א"}, "copy reordered logical UTF-8");
    key(f, input::Key::backspace);
    check(editor.value() == "בג", "logical range deletion");
    key(f, input::Key::z, input::KeyModifier::control);
    check(editor.value() == "אבג", "RTL history restore");
    check(ref.select(2, 2), "RTL caret placement");
    key(f, input::Key::backspace);
    check(editor.value() == "בג", "Backspace must delete logical preceding grapheme");
    key(f, input::Key::z, input::KeyModifier::control);
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(input::CompositionChanged{String{u8"مرحبا 12"}, {}, stamp})), "RTL preedit");
    f.synchronize();
    check(editor.value() == "אבג" && f.inputs.synchronize_input_area(1, 320, 240),
          "preedit changed committed bytes or IME area");
    check(bool(f.inputs.dispatch(input::TextCommitted{String{u8"م"}, stamp})), "RTL commit");
    check(editor.value() == "אمבג", "preedit commit replaced wrong logical range");
    f.inputs.dispose();
    check(!ref.bound() && f.scene.size() == 0 && !f.inputs.dispatch(input::TextCommitted{String{u8"x"}, stamp}),
          "disposed RTL session accepted stale commit");
}

void disjoint_selection_and_affinity() {
    Fixture f;
    fonts(f);
    InputRef ref;
    Signal<String> value{String{u8"A אבג 12 B"}};
    int runs{};
    f.inputs.mount(Content{[&] {
        ++runs;
        Input(InputProps{}.value(value).ref(ref));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto layers = f.inputs.text_layers(mounted.component);
    const auto shapes = f.scene.text_state(layers.base).counters().shape_count;
    check(ref.focus() && ref.select(0, 4), "mixed range");
    f.synchronize();
    const auto& map = f.inputs.caret_map(mounted.component);
    std::vector<text::TextCoverageSegment> pieces;
    check(map.visit_coverage(0, 4, map.revision(), [&](auto piece) { pieces.push_back(piece); }), "mixed coverage");
    check(pieces.size() == 2 && pieces[0].x + pieces[0].width < pieces[1].x, "selection filled bidi gap");
    const auto& primitive = f.scene.primitive(layers.selected);
    const auto viewport = f.inputs.layout_snapshot(mounted.component).viewport;
    for (std::size_t i = 0; i < primitive.coverage.size(); ++i) {
        const auto& owner = primitive.coverage[i];
        const auto& clip =
            f.scene.glyph_scene().instances().at(primitive.instances.first + static_cast<std::uint32_t>(i)).clip_bounds;
        const bool visible = clip[2] > clip[0] && clip[3] > clip[1];
        check(visible == (owner.byte_begin < 4), "selected view exposed unselected glyph cluster");
        if (visible) {
            check(clip[2] <= viewport.x + pieces[0].width + .01F || clip[0] >= viewport.x + pieces[1].x - .01F,
                  "selected glyph clip crossed visual gap");
        }
    }
    key(f, input::Key::c, input::KeyModifier::control);
    check(f.platform.clipboard == String{u8"A א"}, "mixed clipboard order");
    check(ref.select(2, 2), "run boundary placement");
    f.synchronize();
    const auto downstream = f.inputs.layout_snapshot(mounted.component).caret_x;
    const auto upstream = map.at(2, map.revision(), text::TextCaretAffinity::Upstream).value();
    const auto mouse = input::PointerIdentity::mouse();
    const auto hit_x = viewport.x + upstream.x;
    f.services.pointer().dispatch(
        {mouse, input::PointerAction::down, input::PointerButton::primary, hit_x, viewport.y + 5});
    f.services.pointer().dispatch(
        {mouse, input::PointerAction::up, input::PointerButton::primary, hit_x, viewport.y + 5});
    f.synchronize();
    check(std::abs(f.inputs.layout_snapshot(mounted.component).caret_x - hit_x) < .01F &&
              std::abs(downstream - hit_x) > 1,
          "same-line pointer discarded run affinity");
    value.set(String{u8"אבג 12"});
    f.synchronize();
    check(runs == 1 && f.inputs.mounted_inputs().front().editor == mounted.editor && ref.bound() &&
              f.scene.text_state(layers.base).counters().shape_count == shapes + 1,
          "controlled bidi update rebuilt owner");
}

void wrapped_and_masked() {
    Fixture f;
    fonts(f);
    TextAreaRef ref;
    f.inputs.mount(
        Content{[&] { TextArea(TextAreaProps{}.defaultValue(u8"אבג 12 דהו 34\nمرحبا\n\nאבג").rows(2).ref(ref)); }});
    f.synchronize(100);
    const auto mounted = f.inputs.mounted_inputs().front();
    check(ref.focus({InputFocusCursor::Start}), "multiline focus");
    f.synchronize(100);
    key(f, input::Key::down);
    f.synchronize(100);
    key(f, input::Key::end, input::KeyModifier::control);
    f.synchronize(100);
    check(f.inputs.layout_snapshot(mounted.component).vertical_scroll > 0 &&
              f.inputs.synchronize_input_area(1.25F, 125, 300),
          "RTL vertical scroll/IME area");
    check(ref.select(0, 8), "wrapped mixed selection");
    f.synchronize(100);
    key(f, input::Key::c, input::KeyModifier::control);
    check(f.platform.clipboard == String{u8"אבג 1"}, "wrapped logical clipboard");
    InputRef password;
    Fixture masked_fixture;
    fonts(masked_fixture);
    masked_fixture.inputs.mount(
        Content{[&] { Password(PasswordProps{}.defaultValue(u8"א👩‍💻ب").ref(password)); }});
    masked_fixture.synchronize();
    const auto masked = masked_fixture.inputs.mounted_inputs().front();
    auto& editor = masked_fixture.inputs.editors().require(masked.editor);
    check(password.focus({InputFocusCursor::End}), "masked focus");
    masked_fixture.synchronize();
    key(masked_fixture, input::Key::left);
    check(editor.selection().caret == editor.boundaries().grapheme_bytes()[2],
          "mask visual caret did not map grapheme to committed bytes");
}
} // namespace

int main() {
    try {
        visual_navigation_and_transactions();
        disjoint_selection_and_affinity();
        wrapped_and_masked();
        std::cout << "Bidi input navigation, selection, session, clipboard and mask contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
