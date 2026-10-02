#include "support/input_fixture.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

String clip_graphemes(String value, std::size_t maximum) {
    TextBoundaryMap boundaries;
    require(boundaries.assign(value.bytes()), "formatter received invalid UTF-8");
    const auto offsets = boundaries.grapheme_bytes();
    return String::from_utf8(value.bytes().substr(0, offsets[std::min(maximum, offsets.size() - 1)])).value();
}

void key(Fixture& f, Key code, KeyModifier modifiers = KeyModifier::control) {
    f.buttons.focus().dispatch({code, KeyAction::down, modifiers});
    f.buttons.focus().dispatch({code, KeyAction::up, modifiers});
}

void statistics_and_layout() {
    Fixture f;
    Signal<bool> show{true};
    Signal<InputCountOptions> options{InputCountOptions{2, InputCountUnit::Scalar}};
    Signal<InputStatus> status{InputStatus::Default};
    Signal<String> value{String{u8"e\u0301👨‍👩‍👧‍👦"}};
    int content_runs{};
    f.inputs.mount(Content{[&] {
        ++content_runs;
        Input(InputProps{}.value(value).showCount(show).count(options).status(status).allowClear(true), {},
              InputSuffix{[] { Text(u8"suffix"); }});
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    require(f.inputs.count_value(mounted.component) == 9 && f.inputs.count_text(mounted.component) == String{u8"9 / 2"},
            "default scalar count or soft max label incorrect");
    require(f.inputs.status(mounted.component) == InputStatus::Error &&
                value.get() == String{u8"e\u0301👨‍👩‍👧‍👦"},
            "soft max clipped authoritative value or failed validation status");
    const auto suffix_node = f.nodes.require(mounted.node).children[2];
    const auto children = f.nodes.require(suffix_node).children;
    require(children.size() == 3, "clear/custom suffix/counter order missing");
    const auto clear_bounds = f.nodes.require(children[0]).bounds;
    const auto custom_bounds = f.nodes.require(children[1]).bounds;
    const auto count_bounds = f.nodes.require(children[2]).bounds;
    require(clear_bounds.x + clear_bounds.width <= custom_bounds.x + 0.001F &&
                custom_bounds.x + custom_bounds.width <= count_bounds.x + 0.001F,
            "clear/custom suffix/counter overlap");
    options.set({2, InputCountUnit::Grapheme});
    f.synchronize();
    require(f.inputs.count_value(mounted.component) == 2 && f.inputs.status(mounted.component) == InputStatus::Default,
            "grapheme count or status reset incorrect");
    status.set(InputStatus::Warning);
    options.set({1, InputCountUnit::Grapheme});
    f.synchronize();
    require(f.inputs.status(mounted.component) == InputStatus::Warning, "soft max overrode explicit status");
    show.set(false);
    f.synchronize();
    require(f.inputs.count_text(mounted.component).empty() && f.nodes.require(children[2]).bounds.width == 0 &&
                f.nodes.require(children[2]).bounds.height == 0,
            "hidden counter retained layout space");
    show.set(true);
    f.synchronize(90);
    require(f.inputs.layout_snapshot(mounted.component).viewport.width >= 0 && content_runs == 1,
            "narrow affix layout failed or reran content");
}

void editing_transaction() {
    Fixture f;
    InputRef reference;
    int formats{};
    std::vector<String> changes;
    f.inputs.mount(Content{[&] {
        Input(InputProps{}
                  .defaultValue(u8"a")
                  .ref(reference)
                  .showCount()
                  .count(InputCountOptions{2, InputCountUnit::Grapheme})
                  .exceedFormatter([&](String value, std::size_t max) {
                      ++formats;
                      return clip_graphemes(std::move(value), max);
                  })
                  .onChange([&](String value) { changes.push_back(std::move(value)); }));
    }});
    f.synchronize();
    require(reference.focus({InputFocusCursor::End}), "reference focus failed");
    const auto mounted = f.inputs.mounted_inputs().front();
    auto& editor = f.inputs.editors().require(mounted.editor);
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"输入中"}, {0, 3}, stamp})), "preedit rejected");
    f.synchronize();
    require(formats == 0 && changes.empty() && f.inputs.count_value(mounted.component) == 1 &&
                editor.composition().text == String{u8"输入中"}.bytes(),
            "preedit counted or clipped");
    auto result = f.inputs.dispatch(TextCommitted{String{u8"bcd"}, stamp});
    require(bool(result) && result.truncated && editor.value() == "ab" && editor.history().undo_count == 1 &&
                changes.size() == 1 && formats == 1,
            "formatter did not publish one history/value transaction");
    key(f, Key::z);
    require(editor.value() == "a" && formats == 1, "undo ran formatter");
    key(f, Key::y);
    require(editor.value() == "ab" && formats == 1, "redo ran formatter");
    require(reference.select(0, 2), "select failed");
    f.platform.clipboard = String{u8"e\u0301👨‍👩‍👧‍👦x"};
    key(f, Key::v);
    require(editor.value() == String{u8"e\u0301👨‍👩‍👧‍👦"}.bytes() && formats == 2 &&
                editor.boundaries().is_boundary(editor.selection().caret),
            "paste formatter split a grapheme");
    const auto prior_formats = formats;
    for (int i = 0; i < 3; ++i) {
        f.synchronize();
    }
    require(formats == prior_formats, "idle sync ran edit formatter");
}

void authoritative_hard_and_custom() {
    Fixture f;
    Signal<String> value{String{u8"external"}};
    InputRef reference;
    int strategies{};
    int formats{};
    int labels{};
    int runs{};
    f.inputs.mount(Content{[&] {
        ++runs;
        Input(InputProps{}
                  .value(value)
                  .ref(reference)
                  .maxLength(5)
                  .showCount()
                  .count(InputCountOptions{2, InputCountUnit::Scalar})
                  .countStrategy([&](StringView candidate) {
                      ++strategies;
                      return candidate.size_bytes();
                  })
                  .countFormatter([&](InputCountInfo info) {
                      ++labels;
                      return String::from_utf8("bytes=" + std::to_string(info.count)).value();
                  })
                  .exceedFormatter([&](String, std::size_t) {
                      ++formats;
                      return String{u8"123456"};
                  })
                  .onChange([&](String candidate) { value.set(std::move(candidate)); }));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    require(f.inputs.editors().require(mounted.editor).value() == "exter" && formats == 0,
            "authoritative ran formatter");
    require(reference.focus({InputFocusCursor::All}), "focus failed");
    const auto result = f.inputs.dispatch(TextCommitted{String{u8"xyz"}, f.inputs.sessions().active()});
    require(bool(result) && value.get() == String{u8"12345"} && formats == 1 &&
                f.inputs.count_text(mounted.component) == String{u8"bytes=5"},
            "hard limit did not apply after formatter");
    value.set(String{u8"old"});
    f.synchronize();
    require(formats == 1 && f.inputs.editors().require(mounted.editor).value() == "old",
            "authoritative rewrite was clipped");
    const auto count_calls = strategies;
    const auto label_calls = labels;
    for (int i = 0; i < 3; ++i) {
        f.synchronize();
    }
    require(strategies == count_calls && labels == label_calls && runs == 1, "idle stats reran callbacks or content");
    Fixture password;
    password.inputs.mount(Content{[] { Password(PasswordProps{}.defaultValue(u8"secret").showCount().maxLength(8)); }});
    password.synchronize();
    const auto pass = password.inputs.mounted_inputs().front();
    require(password.inputs.count_text(pass.component) == String{u8"6 / 8"},
            "password counter exposed value or missed hard max fallback");
}

void transform_failure_and_reentry() {
    TextEditorStore store;
    const auto owner = store.create("old");
    auto& editor = store.require(owner);
    editor.set_edit_transform([](std::string_view) -> std::string { throw std::runtime_error("test failure"); });
    const auto revision = editor.revision();
    require(editor.commit_text("x").error == TextEditError::formatter_failure && editor.value() == "old" &&
                editor.revision() == revision && editor.history().undo_count == 0,
            "throwing formatter published partial edit");
    editor.set_edit_transform([](std::string_view) { return std::string("\xff"); });
    require(editor.commit_text("x").error == TextEditError::invalid_utf8 && editor.value() == "old",
            "invalid formatter result accepted");
    editor.set_edit_transform([&](std::string_view) {
        require(bool(editor.set_value("external")), "external rewrite failed");
        return "stale";
    });
    require(editor.commit_text("x").error == TextEditError::revision_conflict && editor.value() == "external",
            "reentrant authoritative rewrite was overwritten");
    editor.set_edit_transform([&](std::string_view candidate) {
        require(editor.commit_text("nested").error == TextEditError::revision_conflict, "recursive formatter accepted");
        return std::string(candidate);
    });
    require(bool(editor.commit_text("x")), "outer edit failed after rejected nested edit");
    editor.set_edit_transform([&](std::string_view) {
        require(bool(editor.select_all()), "selection reentry failed");
        return "stale";
    });
    require(editor.commit_text("x").error == TextEditError::revision_conflict, "selection reentry not detected");
    TextInputOwnerId replacement;
    editor.set_edit_transform([&](std::string_view) {
        require(store.destroy(owner), "formatter destroy failed");
        replacement = store.create("new");
        return "stale";
    });
    require(editor.commit_text("x").error == TextEditError::stale_owner && !store.find(owner) &&
                store.require(replacement).value() == "new",
            "retired formatter mutated reused owner");
}

void reactive_limits_and_rejected_controlled_values() {
    Fixture f;
    InputRef reference;
    Signal<String> value{String{u8"baseline"}};
    Signal<InputCountOptions> options{InputCountOptions{3, InputCountUnit::Scalar}};
    bool accept{};
    int formats{};
    int changes{};
    f.inputs.mount(Content{[&] {
        Input(InputProps{}
                  .value(value)
                  .ref(reference)
                  .showCount()
                  .count(options)
                  .exceedFormatter([&](String candidate, std::size_t maximum) {
                      ++formats;
                      return clip_graphemes(std::move(candidate), maximum);
                  })
                  .onChange([&](String candidate) {
                      ++changes;
                      value.set(accept ? candidate : String{u8"rejected"});
                  }));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    require(reference.focus({InputFocusCursor::All}), "controlled focus failed");
    const auto result = f.inputs.dispatch(TextCommitted{String{u8"abcd"}, f.inputs.sessions().active()});
    require(bool(result) && changes == 1 && formats == 1 && value.get() == String{u8"rejected"} &&
                f.inputs.editors().require(mounted.editor).value() == "rejected" &&
                f.inputs.count_text(mounted.component) == String{u8"8 / 3"},
            "controlled rejection was clipped or counted from stale candidate");
    accept = true;
    options.set({0, InputCountUnit::Grapheme});
    require(reference.focus({InputFocusCursor::All}), "controlled selection failed");
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"xyz"}, f.inputs.sessions().active()})) &&
                value.get().empty() && f.inputs.count_text(mounted.component) == String{u8"0 / 0"},
            "zero soft maximum failed");
    require(reference.focus({InputFocusCursor::End}), "composition focus failed");
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"中文"}, {0, 2}, stamp})), "reactive preedit failed");
    options.set({1, InputCountUnit::Grapheme});
    require(f.inputs.sessions().active() == stamp &&
                f.inputs.editors().require(mounted.editor).composition().text == String{u8"中文"}.bytes(),
            "reactive count limit cancelled preedit");
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"中文"}, stamp})) && value.get() == String{u8"中"},
            "reactive count limit was not used at commit");

    TextEditorStore store;
    auto& editor = store.require(store.create("", {2, 2}));
    editor.set_edit_transform([](std::string_view) { return std::string("a\nb\rc"); });
    require(bool(editor.commit_text("long candidate")) && editor.value() == "ab",
            "formatted output was not normalized/limited atomically");
}

void formatter_lifecycle() {
    Fixture f;
    InputRef reference;
    runtime::ComponentId victim;
    f.inputs.mount(Content{[&] {
        Input(InputProps{}
                  .defaultValue(u8"a")
                  .ref(reference)
                  .count(InputCountOptions{1, InputCountUnit::Scalar})
                  .exceedFormatter([&](String value, std::size_t) {
                      require(f.buttons.destroy(victim), "formatter unmount failed");
                      return value;
                  }));
        Input(InputProps{}.defaultValue(u8"survivor"));
    }});
    f.synchronize();
    victim = f.inputs.mounted_inputs()[0].component;
    require(reference.focus({InputFocusCursor::End}), "focus failed");
    require(f.inputs.dispatch(TextCommitted{String{u8"x"}, f.inputs.sessions().active()}).error ==
                    TextEditError::stale_owner &&
                !reference.bound() && f.inputs.mounted_inputs().size() == 1,
            "unmount during formatter unsafe");
    f.synchronize();
    Fixture display;
    runtime::ComponentId display_victim;
    bool remove{};
    bool throws{};
    Signal<String> value{String{u8"a"}};
    display.inputs.mount(Content{[&] {
        Input(InputProps{}.value(value).showCount().countFormatter([&](InputCountInfo info) {
            if (remove) {
                require(display.buttons.destroy(display_victim), "display formatter unmount failed");
            }
            if (throws) {
                throw std::runtime_error("label");
            }
            return String::from_utf8(std::to_string(info.count)).value();
        }));
    }});
    display.synchronize();
    display_victim = display.inputs.mounted_inputs().front().component;
    throws = true;
    value.set(String{u8"ab"});
    display.synchronize();
    require(display.inputs.count_text(display_victim) == String{u8"1"} &&
                display.inputs.count_value(display_victim) == 2,
            "display exception changed value or lost count");
    throws = false;
    remove = true;
    value.set(String{u8"abc"});
    require(display.inputs.mounted_inputs().empty(), "display formatter removal not applied");
}

} // namespace

int main() {
    try {
        statistics_and_layout();
        editing_transaction();
        authoritative_hard_and_custom();
        transform_failure_and_reentry();
        reactive_limits_and_rejected_controlled_values();
        formatter_lifecycle();
        std::cout << "Input statistics and edit transactions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
