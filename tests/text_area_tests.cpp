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
} // namespace

int main() {
    try {
        rows_and_autosize();
        external_size_and_count();
        variants_and_validation();
        common_editing_and_reactive_validation();
        std::cout << "TextArea sizing/props/retained multiline contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
