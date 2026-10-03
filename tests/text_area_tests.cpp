#include "support/input_fixture.hpp"
#include "theme/input_tokens.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

bool near(float a, float b) {
    return std::abs(a - b) < .01F;
}

void key(Fixture& f, input::Key key, input::KeyModifier modifiers = input::KeyModifier::none) {
    f.services.focus().dispatch({key, input::KeyAction::down, modifiers});
    f.services.focus().dispatch({key, input::KeyAction::up, modifiers});
}

void rows_and_autosize() {
    Fixture f;
    Signal<std::size_t> rows{4};
    Signal<TextAreaAutoSize> automatic{TextAreaAutoSize{}};
    Signal<bool> wrap{true};
    Signal<String> value{String{u8"first\r\n\r中文\n"}};
    int content_runs{};
    TextAreaRef reference;
    f.inputs.mount(Content{[&] {
        ++content_runs;
        TextArea(TextAreaProps{}.value(value).rows(rows).autoSize(automatic).wrap(wrap).ref(reference));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto scene = f.inputs.text_scene(mounted.component);
    auto& editor = f.inputs.editors().require(mounted.editor);
    require(editor.mode() == input::TextEditMode::MultiLine && editor.value() == "first\n\n中文\n",
            "TextArea did not preserve normalized multiline value");
    const auto& tokens = detail::derive_input_tokens(f.buttons.components().theme_scope(mounted.component)->snapshot());
    const auto& size = tokens.size(ControlSize::Middle);
    const auto frame = 2 * (size.padding_block + tokens.border_width);
    require(near(f.nodes.require(mounted.node).bounds.height, 4 * size.line_height + frame), "default rows geometry");
    rows.set(2);
    f.synchronize();
    require(near(f.nodes.require(mounted.node).bounds.height, 2 * size.line_height + frame), "reactive rows ignored");
    automatic.set({true, 2, 5});
    f.synchronize();
    require(near(f.nodes.require(mounted.node).bounds.height, 4 * size.line_height + frame),
            "autosize lost blank/trailing lines");
    value.set(String{u8"a\nb\nc\nd\ne\nf\ng"});
    f.synchronize();
    require(near(f.nodes.require(mounted.node).bounds.height, 5 * size.line_height + frame),
            "autosize maxRows ignored");
    value.set(String{});
    f.synchronize();
    require(near(f.nodes.require(mounted.node).bounds.height, 2 * size.line_height + frame),
            "autosize minRows ignored");
    value.set(String{u8"abcdefgh abcdefgh abcdefgh abcdefgh"});
    f.synchronize(100);
    const auto narrow_lines = f.scene.text_state(scene).measurement().lines.size();
    require(narrow_lines > 1, "TextArea did not wrap to finite editable width");
    const auto shapes = f.scene.text_state(scene).counters().shape_count;
    f.synchronize(320);
    require(f.scene.text_state(scene).measurement().lines.size() < narrow_lines &&
                f.scene.text_state(scene).counters().shape_count == shapes,
            "width reflow reshaped or retained old wrap");
    wrap.set(false);
    f.synchronize(100);
    require(f.scene.text_state(scene).measurement().lines.size() == 1, "wrap=false did not retain one paragraph");
    require(reference.bound() && reference.focus({InputFocusCursor::All}), "TextArea ref did not focus/select");
    f.synchronize(100);
    require(editor.selection() == input::TextSelection{0, editor.value().size()} && content_runs == 1 &&
                f.inputs.mounted_inputs().front().editor == mounted.editor &&
                f.inputs.text_scene(mounted.component) == scene,
            "TextArea sizing rebuilt content/editor/ref");
    f.inputs.dispose();
    require(!reference.bound() && f.inputs.editors().size() == 0 && f.scene.size() == 0,
            "TextArea dispose leaked resources");
}

void external_size_and_count() {
    Fixture f;
    Signal<bool> count{true};
    Signal<bool> clear{true};
    f.inputs.mount(Content{[&] {
        TextArea(TextAreaProps{}
                     .defaultValue(u8"a\nb\nc\nd\ne\nf")
                     .autoSize(TextAreaAutoSize{true, 2, 3})
                     .layout(LayoutStyle{}.width(dp(180)).height(dp(150)))
                     .showCount(count)
                     .allowClear(clear));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto root = f.nodes.require(mounted.node).bounds;
    const auto viewport = f.inputs.layout_snapshot(mounted.component).viewport;
    require(near(root.width, 180) && near(root.height, 150) && viewport.height > 3 * 20 &&
                f.inputs.count_value(mounted.component) == 11 &&
                f.inputs.count_text(mounted.component) == String{u8"11"},
            "external size/count contract ignored");
    const auto suffix = f.nodes.require(mounted.node).children[2];
    const auto children = f.nodes.require(suffix).children;
    require(children.size() == 2, "TextArea lost retained clear/count children");
    const auto clear_bounds = f.nodes.require(children[0]).bounds;
    const auto count_bounds = f.nodes.require(children[1]).bounds;
    require(near(clear_bounds.y, viewport.y) && count_bounds.y >= viewport.y + viewport.height &&
                near(count_bounds.x + count_bounds.width, root.x + root.width),
            "TextArea clear/count not top-right/below viewport");
    const auto wide = viewport.width;
    clear.set(false);
    count.set(false);
    f.synchronize();
    require(f.inputs.layout_snapshot(mounted.component).viewport.width > wide &&
                near(f.nodes.require(children[1]).bounds.height, 0),
            "hidden TextArea actions/count consumed layout");
}

void variants_and_validation() {
    for (const auto variant :
         {InputVariant::Outlined, InputVariant::Filled, InputVariant::Borderless, InputVariant::Underlined}) {
        Fixture f;
        Signal<bool> disabled{false};
        f.inputs.mount(Content{[&] {
            TextArea(TextAreaProps{}
                         .defaultValue(u8"abc\n中文")
                         .rows(2)
                         .variant(variant)
                         .size(ControlSize::Small)
                         .status(InputStatus::Warning)
                         .disabled(disabled)
                         .showCount()
                         .count(InputCountOptions{1, InputCountUnit::Grapheme}));
        }});
        f.synchronize();
        const auto mounted = f.inputs.mounted_inputs().front();
        require(f.inputs.variant(mounted.component) == variant &&
                    f.inputs.size(mounted.component) == ControlSize::Small &&
                    f.inputs.status(mounted.component) == InputStatus::Warning,
                "TextArea common props not forwarded");
        disabled.set(true);
        f.synchronize();
        require(f.inputs.editors().require(mounted.editor).disabled(), "TextArea disabled did not reach editor");
    }
    for (const auto props : {TextAreaProps{}.rows(0), TextAreaProps{}.autoSize(TextAreaAutoSize{true, 0, 1}),
                             TextAreaProps{}.autoSize(TextAreaAutoSize{true, 3, 2}),
                             TextAreaProps{}.resize(static_cast<TextAreaResize>(99))}) {
        Fixture f;
        bool rejected{};
        try {
            f.inputs.mount(Content{[&] { TextArea(props); }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected && f.inputs.editors().size() == 0 && f.scene.size() == 0 && f.inputs.mounted_inputs().empty(),
                "invalid TextArea configuration leaked resource");
    }
}

void common_editing_and_reactive_validation() {
    Fixture f;
    Signal<std::size_t> rows{2};
    Signal<TextAreaAutoSize> automatic{TextAreaAutoSize{}};
    Signal<bool> readonly{false};
    TextAreaRef reference;
    int focuses{};
    int blurs{};
    int edits{};
    int formatters{};
    f.inputs.mount(Content{[&] {
        TextArea(TextAreaProps{}
                     .defaultValue(u8"a\nb")
                     .rows(rows)
                     .autoSize(automatic)
                     .ref(reference)
                     .autoFocus()
                     .purpose(InputPurpose::Name)
                     .capitalization(InputCapitalization::Words)
                     .autocorrect(false)
                     .readOnly(readonly)
                     .maxLength(4)
                     .showCount()
                     .count(InputCountOptions{2, InputCountUnit::Grapheme})
                     .exceedFormatter([&](String candidate, std::size_t maximum) {
                         ++formatters;
                         require(maximum == 2 && candidate.bytes() == "a\nb\nx", "TextArea formatter input contract");
                         return String{u8"中\r\n文"};
                     })
                     .onChange([&](String value) {
                         ++edits;
                         require(value == String{u8"中\n文"}, "TextArea change normalization");
                     })
                     .onFocus([&] { ++focuses; })
                     .onBlur([&] { ++blurs; }));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    auto& editor = f.inputs.editors().require(mounted.editor);
    require(focuses == 1 && f.platform.last_properties.type == input::TextInputType::name &&
                f.platform.last_properties.capitalization == input::TextCapitalization::words &&
                !f.platform.last_properties.autocorrect,
            "TextArea did not forward native hints/autofocus");
    const auto height = f.nodes.require(mounted.node).bounds.height;
    bool rejected{};
    try {
        rows.set(0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    f.synchronize();
    require(rejected && near(f.nodes.require(mounted.node).bounds.height, height) && reference.bound(),
            "invalid reactive rows replaced sizing/editor");
    rejected = false;
    try {
        automatic.set({true, 4, 2});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    f.synchronize();
    require(rejected && near(f.nodes.require(mounted.node).bounds.height, height),
            "invalid reactive autoSize published");
    rows.set(3);
    f.synchronize();
    require(f.nodes.require(mounted.node).bounds.height > height, "valid rows did not recover after invalid value");
    require(reference.focus({InputFocusCursor::End}), "TextArea end focus failed");
    const auto stamp = f.inputs.sessions().active();
    require(stamp.owner == mounted.editor, "TextArea session missing");
    require(bool(f.inputs.dispatch(input::CompositionChanged{String{u8"预\n编"}, {}, stamp})),
            "TextArea preedit rejected");
    f.synchronize();
    require(editor.value() == "a\nb" && f.inputs.count_value(mounted.component) == 3 && edits == 0,
            "TextArea preedit entered committed value/count");
    require(bool(f.inputs.dispatch(input::TextCommitted{String{u8"\r\nx"}, stamp})), "TextArea commit rejected");
    f.synchronize();
    require(editor.value() == "中\n文" && edits == 1 && formatters == 1 && f.inputs.count_value(mounted.component) == 3,
            "TextArea formatter/count transaction failed");
    readonly.set(true);
    f.synchronize();
    require(reference.select(0, editor.value().size()) && editor.read_only(), "TextArea readOnly selection blocked");
    require(reference.blur() && blurs == 1, "TextArea blur callback missing");
    f.inputs.dispose();
    require(!reference.bound() && f.inputs.editors().size() == 0 && f.scene.size() == 0,
            "TextArea session dispose leaked");
}

void keyboard_scroll_and_resize() {
    Fixture f;
    TextAreaRef reference;
    Signal<bool> readonly{false};
    Signal<bool> disabled{false};
    Signal<TextAreaResize> resize{TextAreaResize::Both};
    int submits{};
    int notifications{};
    TextAreaSize last_size;
    f.inputs.mount(Content{[&] {
        TextArea(TextAreaProps{}
                     .defaultValue(u8"abcdef\nx\nabcdef\n1\n2\n3\n4\n5\n6")
                     .rows(2)
                     .ref(reference)
                     .readOnly(readonly)
                     .disabled(disabled)
                     .resize(resize)
                     .onSubmit([&](String) { ++submits; })
                     .onResize([&](TextAreaSize size) {
                         ++notifications;
                         last_size = size;
                     }));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    auto& editor = f.inputs.editors().require(mounted.editor);
    require(notifications == 1 && last_size.height > 40, "initial TextArea size notification missing");
    require(reference.focus() && reference.select(4, 4), "TextArea navigation focus failed");
    f.synchronize();
    key(f, input::Key::down);
    f.synchronize();
    require(editor.selection().caret == 8, "TextArea Down missed short line end");
    key(f, input::Key::down);
    f.synchronize();
    require(editor.selection().caret == 13 && f.inputs.layout_snapshot(mounted.component).vertical_scroll > 0,
            "TextArea Down lost preferred x or did not reveal caret");
    require(f.inputs.synchronize_input_area(1, 320, 240) && f.platform.area.y > 5 && f.platform.area.height <= 22,
            "TextArea IME area did not follow visible caret row");
    key(f, input::Key::home);
    require(editor.selection().caret == 9, "TextArea Home did not use visual line");
    key(f, input::Key::end);
    require(editor.selection().caret == 15, "TextArea End did not use visual line");
    key(f, input::Key::home, input::KeyModifier::control);
    require(editor.selection().caret == 0, "TextArea primary Home did not use document");
    const auto before = std::string(editor.value());
    key(f, input::Key::enter);
    f.synchronize();
    require(editor.value() == "\n" + before && submits == 0, "TextArea Enter submitted instead of LF");
    key(f, input::Key::enter, input::KeyModifier::control);
    require(submits == 1 && editor.value() == "\n" + before, "TextArea primary Enter did not submit");
    readonly.set(true);
    f.synchronize();
    const auto viewport = f.inputs.layout_snapshot(mounted.component).viewport;
    const auto scene = f.inputs.text_scene(mounted.component);
    const auto shapes = f.scene.text_state(scene).counters().shape_count;
    const auto rasters = f.fonts->counters().rasterizations;
    require(f.inputs.dispatch(input::ScrollInputEvent{0, -1, viewport.x + 4, viewport.y + 4}),
            "TextArea wheel not handled");
    f.synchronize();
    const auto scrolled = f.inputs.layout_snapshot(mounted.component).vertical_scroll;
    f.synchronize();
    require(scrolled > 0 && f.inputs.layout_snapshot(mounted.component).vertical_scroll == scrolled &&
                f.scene.text_state(scene).counters().shape_count == shapes &&
                f.fonts->counters().rasterizations == rasters,
            "TextArea wheel snapped to caret or reshaped/rasterized");
    key(f, input::Key::enter);
    require(editor.value() == "\n" + before, "readOnly TextArea Enter edited");
    const auto mouse = input::PointerIdentity::mouse();
    const auto bounds = f.nodes.require(mounted.node).bounds;
    f.services.pointer().dispatch({mouse, input::PointerAction::down, input::PointerButton::primary,
                                   bounds.x + bounds.width - 4, bounds.y + bounds.height - 4});
    require(f.services.pointer().state(mouse)->capture == mounted.interaction, "TextArea resize did not capture");
    f.services.pointer().dispatch({mouse, input::PointerAction::up, input::PointerButton::secondary,
                                   bounds.x + bounds.width - 4, bounds.y + bounds.height - 4});
    require(f.services.pointer().state(mouse)->capture == mounted.interaction,
            "secondary release cancelled primary resize capture");
    f.services.pointer().dispatch({mouse, input::PointerAction::move, input::PointerButton::none,
                                   bounds.x + bounds.width - 44, bounds.y + bounds.height + 26});
    f.synchronize();
    require(last_size.width < bounds.width && last_size.height > bounds.height && notifications == 2,
            "TextArea resize dimensions/notification incorrect");
    resize.set(TextAreaResize::None);
    require(!f.services.pointer().state(mouse)->capture, "TextArea resize config did not cancel capture");
    disabled.set(true);
    f.synchronize();
    require(!reference.focus() && !f.inputs.dispatch(input::ScrollInputEvent{0, -1, viewport.x + 4, viewport.y + 4}),
            "disabled TextArea accepted focus/scroll");
    f.inputs.dispose();
    require(!reference.bound() && !f.services.pointer().state(mouse)->capture && f.scene.size() == 0 &&
                f.inputs.editors().size() == 0 && !f.inputs.next_caret_deadline(),
            "TextArea resize disposal leaked");
}

void fractional_scroll_boundary_and_clipboard_unmount() {
    Fixture f;
    TextAreaRef reference;
    f.inputs.set_display_scale(1.25F);
    f.inputs.mount(Content{[&] {
        TextArea(TextAreaProps{}
                     .defaultValue(u8"one\ntwo\nthree\nfour\nfive\nsix")
                     .ref(reference)
                     .layout(LayoutStyle{}.height(dp(50.3F)))
                     .resize(TextAreaResize::None));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto viewport = f.inputs.layout_snapshot(mounted.component).viewport;
    const input::ScrollInputEvent bottom{0, -100, viewport.x + 4, viewport.y + 4};
    require(f.inputs.dispatch(bottom), "fractional TextArea wheel did not reach bottom");
    f.synchronize();
    require(!f.inputs.dispatch(bottom), "fractional scroll boundary kept consuming wheel");
    require(reference.focus({InputFocusCursor::All}), "clipboard unmount fixture did not focus");
    f.platform.clipboard = String{u8"replacement\ntext"};
    f.platform.on_clipboard = [&] {
        f.inputs.dispose();
    };
    key(f, input::Key::v, input::KeyModifier::control);
    require(!reference.bound() && f.inputs.editors().size() == 0 && f.scene.size() == 0,
            "clipboard callback unmount retained multiline owner");
}

void resize_callback_can_unmount() {
    Fixture f;
    TextAreaRef reference;
    int calls{};
    f.inputs.mount(Content{[&] {
        TextArea(TextAreaProps{}.ref(reference).onResize([&](TextAreaSize) {
            ++calls;
            f.inputs.dispose();
        }));
    }});
    f.synchronize();
    require(calls == 1 && !reference.bound() && f.inputs.editors().size() == 0 && f.scene.size() == 0,
            "onResize self-unmount retained component/resources");
}

void cross_line_selection_and_pointer() {
    Fixture f;
    TextAreaRef reference;
    f.inputs.mount(Content{[&] {
        TextArea(TextAreaProps{}.defaultValue(u8"abcd\n\nxyz").rows(4).resize(TextAreaResize::None).ref(reference));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    auto& editor = f.inputs.editors().require(mounted.editor);
    require(reference.focus() && reference.select(1, 8), "cross-line ref selection failed");
    f.synchronize();
    const auto layers = f.inputs.text_layers(mounted.component);
    const auto& primitive = f.scene.primitive(layers.selected);
    const auto& carets = f.inputs.caret_map(mounted.component);
    const auto viewport = f.inputs.layout_snapshot(mounted.component).viewport;
    const auto& instances = f.scene.glyph_scene().instances();
    require(primitive.line_ranges.size() == 3 && primitive.line_ranges[1].count == 0,
            "cross-line selection lost empty line metadata");
    for (const auto line : {std::size_t{0}, std::size_t{2}}) {
        const auto range = primitive.line_ranges[line];
        require(range.count > 0, "selection line has no glyphs");
        const auto& clip = instances.at(range.first).clip_bounds;
        const auto start = line == 0 ? carets.line_stops(0)[1].x : 0;
        const auto end = line == 0 ? carets.line_stops(0).back().x + 7 : carets.line_stops(2)[2].x;
        require(near(clip[0], viewport.x + start) && near(clip[1], viewport.y + static_cast<float>(line) * 22) &&
                    near(clip[2], viewport.x + end) && near(clip[3], viewport.y + static_cast<float>(line + 1) * 22),
                "selected glyph coverage crossed line or covered unselected bytes");
    }
    const auto shapes = f.scene.text_state(layers.base).counters().shape_count;
    const auto rasters = f.fonts->counters().rasterizations;
    const auto mouse = input::PointerIdentity::mouse();
    const auto first = carets.line_stops(0)[2];
    const auto last = carets.line_stops(2)[1];
    f.services.pointer().dispatch(
        {mouse, input::PointerAction::down, input::PointerButton::primary, viewport.x + first.x, viewport.y + 5});
    f.services.pointer().dispatch(
        {mouse, input::PointerAction::move, input::PointerButton::none, viewport.x + last.x, viewport.y + 49});
    f.services.pointer().dispatch(
        {mouse, input::PointerAction::up, input::PointerButton::primary, viewport.x + last.x, viewport.y + 49});
    f.synchronize();
    require(editor.selection() == input::TextSelection{2, 7} && !f.services.pointer().state(mouse)->capture &&
                f.scene.text_state(layers.base).counters().shape_count == shapes &&
                f.fonts->counters().rasterizations == rasters,
            "two-dimensional drag selection misplaced byte or rebuilt glyphs");
    key(f, input::Key::c, input::KeyModifier::control);
    require(f.platform.clipboard == String{u8"cd\n\nx"}, "cross-line copy lost LF or selected text");
    key(f, input::Key::x, input::KeyModifier::control);
    require(editor.value() == "abyz", "cross-line cut did not remove one range");
    key(f, input::Key::z, input::KeyModifier::control);
    require(editor.value() == "abcd\n\nxyz", "cross-line cut history did not restore text");
    f.platform.clipboard = String{u8"Q\r\nR\rS"};
    key(f, input::Key::v, input::KeyModifier::control);
    require(editor.value() == "abQ\nR\nSyz", "multiline paste failed CRLF normalization");
    key(f, input::Key::z, input::KeyModifier::control);
    require(editor.value() == "abcd\n\nxyz", "multiline paste did not form one history transaction");
}

void soft_wrap_affinity_and_page_navigation() {
    Fixture f;
    TextAreaRef reference;
    f.inputs.mount(Content{[&] {
        TextArea(TextAreaProps{}
                     .defaultValue(u8"abcdefgh abcdefgh abcdefgh abcdefgh abcdefgh")
                     .rows(2)
                     .resize(TextAreaResize::None)
                     .ref(reference));
    }});
    f.synchronize(90);
    const auto mounted = f.inputs.mounted_inputs().front();
    auto& editor = f.inputs.editors().require(mounted.editor);
    const auto& map = f.inputs.caret_map(mounted.component);
    require(map.line_count() >= 5, "soft wrap navigation fixture has too few lines");
    const auto byte = map.line_stops(1).front().byte;
    require(byte == map.line_stops(0).back().byte && reference.focus() && reference.select(byte, byte),
            "soft wrap boundary was not shared");
    f.synchronize(90);
    const auto lower_y = f.inputs.layout_snapshot(mounted.component).caret.y;
    key(f, input::Key::left);
    f.synchronize(90);
    require(editor.selection().caret == byte && f.inputs.layout_snapshot(mounted.component).caret.y < lower_y,
            "Left skipped upstream visual affinity at soft wrap");
    key(f, input::Key::right);
    f.synchronize(90);
    require(editor.selection().caret == byte && near(f.inputs.layout_snapshot(mounted.component).caret.y, lower_y),
            "Right skipped downstream visual affinity at soft wrap");
    key(f, input::Key::page_down, input::KeyModifier::shift);
    f.synchronize(90);
    require(editor.selection().anchor == byte && editor.selection().caret == map.line_stops(3).front().byte &&
                f.inputs.layout_snapshot(mounted.component).vertical_scroll > 0,
            "Shift PageDown lost anchor/page distance/reveal");
    key(f, input::Key::end, input::KeyModifier::control);
    require(editor.selection().empty() && editor.selection().caret == editor.value().size(),
            "primary End did not collapse to document end");
    key(f, input::Key::page_up);
    f.synchronize(90);
    require(editor.selection().caret < editor.value().size(), "PageUp did not move through visual rows");
}

void multiline_ime_epochs_and_enter_history() {
    Fixture f;
    TextAreaRef reference;
    int edits{};
    int submits{};
    f.inputs.mount(Content{[&] {
        TextArea(TextAreaProps{}
                     .defaultValue(u8"ab\ncd")
                     .ref(reference)
                     .rows(4)
                     .resize(TextAreaResize::None)
                     .onChange([&](String) { ++edits; })
                     .onSubmit([&](String) { ++submits; }));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    auto& editor = f.inputs.editors().require(mounted.editor);
    require(reference.focus({InputFocusCursor::Start}), "IME fixture did not focus");
    const auto old_stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(input::CompositionChanged{String{u8"预\n编"}, {}, old_stamp})),
            "multiline preedit rejected");
    f.synchronize();
    require(f.inputs.caret_map(mounted.component).line_count() == 3 && editor.value() == "ab\ncd" && edits == 0,
            "preedit multiline display changed committed text");
    std::size_t underlines{};
    for (const auto& quad : f.services.surfaces().instances().instances()) {
        if (quad.opacity > 0 && near(quad.bounds[3], 1) && quad.bounds[2] > 0) {
            ++underlines;
        }
    }
    require(underlines == 2, "multiline preedit did not retain separate row underlines");
    key(f, input::Key::enter);
    key(f, input::Key::enter, input::KeyModifier::control);
    key(f, input::Key::down);
    require(editor.composition().active && editor.value() == "ab\ncd" && submits == 0,
            "navigation/Enter escaped IME ownership");
    require(bool(f.inputs.dispatch(input::TextCommitted{String{u8"中\r\n文"}, old_stamp})), "IME LF commit failed");
    require(editor.value() == "中\n文ab\ncd" && edits == 1, "IME commit changed normalization/history contract");
    key(f, input::Key::z, input::KeyModifier::control);
    require(editor.value() == "ab\ncd", "IME multiline commit was not one undo transaction");
    require(reference.focus({InputFocusCursor::End}), "Enter history end focus failed");
    key(f, input::Key::enter);
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(input::TextCommitted{String{u8"x"}, stamp})), "text after Enter rejected");
    key(f, input::Key::z, input::KeyModifier::control);
    require(editor.value() == "ab\ncd\n", "Enter merged into following typing history");
    key(f, input::Key::z, input::KeyModifier::control);
    require(editor.value() == "ab\ncd", "Enter transaction failed undo");
    require(reference.blur() && !f.inputs.dispatch(input::TextCommitted{String{u8"stale"}, old_stamp}) &&
                reference.focus(),
            "blur accepted stale IME epoch");
    f.inputs.dispose();
    require(!f.inputs.dispatch(input::TextCommitted{String{u8"disposed"}, stamp}) && !reference.bound(),
            "disposed multiline session accepted event");
}
} // namespace

int main() {
    try {
        rows_and_autosize();
        external_size_and_count();
        variants_and_validation();
        common_editing_and_reactive_validation();
        keyboard_scroll_and_resize();
        cross_line_selection_and_pointer();
        soft_wrap_affinity_and_page_navigation();
        multiline_ime_epochs_and_enter_history();
        fractional_scroll_boundary_and_clipboard_unmount();
        resize_callback_can_unmount();
        std::cout << "TextArea sizing/props/retained multiline contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
