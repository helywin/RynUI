#include "support/input_fixture.hpp"
#include "support/input_counting_gpu.hpp"
#include <iostream>

namespace {
using namespace ryn; using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
void journey(bool controlled) {
    Fixture f; Signal<String> value{String{}};
    Signal<std::size_t> maximum{20}; Signal<InputStatus> status{InputStatus::Default};
    Signal<bool> read_only{false}, disabled{false}; Signal<ThemeConfig> theme{ThemeConfig{}};
    int changes{}, submits{}, prefix_runs{}, suffix_runs{}; String emitted;
    f.inputs.mount(Content{[&] { Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
        auto props = InputProps{}.placeholder(u8"请输入 / Input").maxLength(maximum).status(status).readOnly(read_only).disabled(disabled)
            .onChange([&](String next) { ++changes; emitted = next; if(controlled) value.set(std::move(next)); })
            .onSubmit([&](String next) { ++submits; emitted = std::move(next); });
        if(controlled) props.value(value); else props.defaultValue(u8"");
        Input(std::move(props), InputPrefix{[&] { ++prefix_runs; Text(u8"前"); }}, InputSuffix{[&] { ++suffix_runs; Text(u8"后"); }});
    }}); }});
    const auto input = f.inputs.mounted_inputs().front(); f.synchronize(180);
    const auto layers = f.inputs.text_layers(input.component);
    const auto mount_runs = f.buttons.components().mount_runs();
    auto& editor = f.inputs.editors().require(input.editor);
    ryn_test::input_component::CountingGpu gpu;
    graphics::QuadGpuBuffer quads{gpu, f.buttons.button_scene().instances()};
    detail::GlyphGpuResources glyphs{gpu}; detail::RoundedEffectGpuResources effects{gpu};
    int stages{};
    const auto sync = [&](const char* stage) {
        f.synchronize(180);
        require(f.inputs.synchronize_input_area(1, 180, 240), "journey input area failed");
        quads.synchronize(f.buttons.button_scene().instances());
        glyphs.synchronize(f.scene.atlas(), f.scene.glyph_scene().instances());
        effects.synchronize(f.buttons.rounded_effects(), {180, 240, 1});
        require(f.inputs.mounted_inputs().front().editor == input.editor
            && f.inputs.mounted_inputs().front().component == input.component
            && f.inputs.text_layers(input.component).base == layers.base
            && f.inputs.text_layers(input.component).selected == layers.selected
            && f.inputs.text_layers(input.component).placeholder == layers.placeholder
            && f.buttons.components().mount_runs() == mount_runs && prefix_runs == 1 && suffix_runs == 1,
            "journey remounted owner/text/slots");
        require(f.dirty.layout_roots().empty() && f.dirty.geometry_nodes().empty(), "journey did not drain dirty work");
        const auto selection = editor.selection();
        require(selection.anchor <= editor.value().size() && selection.caret <= editor.value().size()
            && (!controlled || value.get().bytes() == editor.value()), "journey authoritative value/selection mismatch");
        require(f.buttons.button_scene().instances().size() == 3 && f.buttons.rounded_effects().live_count() == 19,
            "journey changed retained scene topology");
        std::cout << "mode=" << (controlled ? "controlled" : "uncontrolled") << " stage=" << stage
            << " bytes=" << editor.value().size() << " anchor=" << selection.anchor << " caret=" << selection.caret
            << " composition=" << editor.composition().active << " undo=" << editor.history().undo_count
            << " area_cursor=" << f.platform.area.cursor << " area_updates=" << f.platform.areas
            << " shapes=" << f.scene.text_state(layers.base).counters().shape_count
            << " quads=" << f.buttons.button_scene().instances().size() << " effects=" << f.buttons.rounded_effects().live_count()
            << " glyph_uploads=" << gpu.glyph_uploads << '\n';
        ++stages; static_cast<void>(f.frames.consume_request());
    };
    const auto key = [&](Key k, KeyModifier mod = KeyModifier::none) { f.buttons.focus().dispatch({k, KeyAction::down, mod}); };
    const auto commit = [&](String text) { return f.inputs.dispatch(TextCommitted{std::move(text), f.inputs.sessions().active()}); };
    require(f.inputs.display_snapshot(input.component).placeholder, "initial placeholder absent");
    require(f.buttons.focus().request_focus(input.interaction, FocusModality::keyboard), "journey focus failed");
    const auto stamp = f.inputs.sessions().active(); require(stamp.valid(), "journey session absent");
    sync("focused-empty");
    require(bool(commit(String{u8"A"})) && editor.value() == "A" && changes == 1, "Latin commit/echo failed"); sync("latin");
    const auto revision = editor.revision();
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"ni"}, {2, 0}, stamp})), "composition failed");
    require(bool(f.inputs.dispatch(CandidatesChanged{{String{u8"你"}, String{u8"泥"}}, 0, CandidateOrientation::vertical, stamp})), "candidates failed");
    require(editor.revision() == revision && editor.value() == "A" && changes == 1 && editor.composition().candidates.size() == 2,
        "preedit/candidates mutated authoritative state"); sync("candidates");
    require(bool(commit(String{u8"你"})) && editor.value() == String{u8"A你"}.bytes() && changes == 2
        && !editor.composition().active, "CJK commit failed"); sync("cjk-commit");
    require(bool(commit(String{u8"👩‍👩‍👧‍👦"})) && changes == 3, "emoji commit failed"); sync("emoji");
    key(Key::z, KeyModifier::control); require(editor.value() == String{u8"A你"}.bytes(), "emoji undo was not atomic"); sync("undo");
    key(Key::y, KeyModifier::control); require(editor.value() == String{u8"A你👩‍👩‍👧‍👦"}.bytes(), "emoji redo failed"); sync("redo");
    key(Key::a, KeyModifier::control); key(Key::c, KeyModifier::control);
    require(f.platform.clipboard && f.platform.clipboard->bytes() == editor.value(), "journey copy failed");
    key(Key::x, KeyModifier::control); require(editor.value().empty(), "journey cut failed"); sync("cut-placeholder");
    require(f.inputs.display_snapshot(input.component).placeholder, "cut did not restore placeholder");
    key(Key::z, KeyModifier::control); key(Key::a, KeyModifier::control);
    f.platform.clipboard = String{u8"x\r\n中\n🙂"}; key(Key::v, KeyModifier::control);
    require(editor.value() == String{u8"x中🙂"}.bytes(), "journey paste normalization failed"); sync("paste");
    maximum.set(4); const auto limited = commit(String{u8"yz"});
    require(bool(limited) && limited.truncated && editor.value() == String{u8"x中🙂y"}.bytes(), "scalar maxLength failed"); sync("max-length");
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"pin"}, {3, 0}, stamp})), "cancel composition failed");
    key(Key::escape); require(!editor.composition().active && editor.value() == String{u8"x中🙂y"}.bytes(), "cancel changed value"); sync("cancel");
    if(controlled) {
        require(bool(f.inputs.dispatch(CompositionChanged{String{u8"wai"}, {3, 0}, stamp})), "external preedit failed");
        const auto before = changes; value.set(String{u8"外"});
        require(!editor.composition().active && editor.value() == String{u8"外"}.bytes() && changes == before,
            "external conflict recirculated callback"); sync("external-reconcile");
    }
    key(Key::home); sync("home");
    const auto shapes = f.scene.text_state(layers.base).counters().shape_count;
    key(Key::end, KeyModifier::shift); sync("selection");
    require(f.scene.text_state(layers.base).counters().shape_count == shapes, "selection reshaped text");
    status.set(InputStatus::Warning); ThemeConfig dark; dark.algorithms = {ThemeAlgorithm::Dark}; theme.set(dark); sync("theme-warning");
    read_only.set(true); key(Key::delete_forward); key(Key::a, KeyModifier::control); key(Key::c, KeyModifier::control); sync("read-only");
    require(!f.inputs.sessions().active().valid() && f.buttons.focus().state().focused == input.interaction, "readOnly session/focus mismatch");
    read_only.set(false); key(Key::enter); require(submits == 1 && emitted.bytes() == editor.value(), "submit mismatch"); sync("submit");
    f.buttons.set_motion_preference(animation::MotionPreference::normal);
    require(f.inputs.next_caret_deadline().has_value(), "journey blink absent");
    static_cast<void>(f.buttons.tick_animations(animation::AnimationTime::microseconds(500000))); sync("blink");
    disabled.set(true); sync("disabled"); require(!f.inputs.next_caret_deadline(), "disabled blink retained");
    require(stages >= 18 && f.buttons.destroy(input.component) && !f.buttons.next_deadline(), "journey cleanup retained work");
    require(!f.inputs.dispatch(TextCommitted{String{u8"late"}, stamp}), "destroyed owner accepted late commit");
    f.inputs.dispose(); f.synchronize(); static_cast<void>(f.frames.consume_request());
    require(!f.buttons.next_deadline() && !f.frames.pending() && f.inputs.editors().size() == 0
        && f.platform.starts == f.platform.stops, "journey failed to restore idle");
}
void reused_owner() {
    Fixture f; detail::MountedInputComponent old; TextInputSessionStamp stamp;
    f.inputs.mount(Content{[&] {
        Input(InputProps{}.defaultValue(u8"old"));
        old = f.inputs.mounted_inputs().front();
        require(f.buttons.focus().request_focus(old.interaction, FocusModality::keyboard), "reuse focus failed");
        stamp = f.inputs.sessions().active();
        require(stamp.valid() && f.buttons.destroy(old.component), "reuse destroy failed");
        Input(InputProps{}.defaultValue(u8"new"));
    }});
    const auto fresh = f.inputs.mounted_inputs().front(); f.synchronize();
    require(fresh.editor.index == old.editor.index && fresh.editor.generation != old.editor.generation,
        "reuse did not exercise a new generation");
    require(f.buttons.focus().request_focus(fresh.interaction, FocusModality::keyboard), "fresh focus failed");
    require(!f.inputs.dispatch(TextCommitted{String{u8"late"}, stamp})
        && !f.inputs.dispatch(CompositionChanged{String{u8"late"}, {0, 0}, stamp})
        && f.inputs.editors().require(fresh.editor).value() == "new", "reused owner accepted old event");
    f.inputs.dispose(); f.synchronize(); static_cast<void>(f.frames.consume_request());
    require(!f.buttons.next_deadline() && !f.frames.pending() && f.platform.starts == f.platform.stops,
        "reused owner retained work");
}
void area_projection() {
    for(float scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
        Fixture f; f.font_scale = scale; f.inputs.set_display_scale(scale);
        f.inputs.mount(Content{[] { Input(InputProps{}.defaultValue(u8"abcdefghijklmnopqrstuvwxyz中文"), InputPrefix{[] { Text(u8"前"); }}); }});
        const auto input = f.inputs.mounted_inputs().front();
        require(f.buttons.focus().request_focus(input.interaction, FocusModality::keyboard), "area focus failed");
        f.buttons.focus().dispatch({Key::end, KeyAction::down});
        f.synchronize(120, {10, 0, 100, 240});
        const int width = static_cast<int>(120 * scale), height = static_cast<int>(240 * scale);
        require(f.inputs.synchronize_input_area(scale, width, height), "area projection failed");
        const auto geometry = f.inputs.layout_snapshot(input.component);
        const auto expected = map_text_input_area({{geometry.viewport.x, geometry.viewport.y, geometry.viewport.width, geometry.viewport.height},
            {geometry.clip.x, geometry.clip.y, geometry.clip.width, geometry.clip.height}, 0, 0, geometry.caret_x, scale, width, height});
        require(expected && f.platform.area == *expected && geometry.scroll_offset > 0, "scrolled caret area mismatch");
        const auto calls = f.platform.areas;
        require(f.inputs.synchronize_input_area(scale, width, height) && f.platform.areas == calls, "unchanged area was resubmitted");
        f.buttons.focus().dispatch({Key::home, KeyAction::down}); f.synchronize(120, {10, 0, 100, 240});
        f.platform.area_failure = true;
        require(!f.inputs.synchronize_input_area(scale, width, height), "native area failure was hidden");
        f.platform.area_failure = false;
        require(f.inputs.synchronize_input_area(scale, width, height), "Home area retry failed");
        const auto home = f.inputs.layout_snapshot(input.component);
        const auto home_expected = map_text_input_area({{home.viewport.x, home.viewport.y, home.viewport.width, home.viewport.height},
            {home.clip.x, home.clip.y, home.clip.width, home.clip.height}, 0, 0, home.caret_x, scale, width, height});
        require(home_expected && f.platform.area == *home_expected && home.scroll_offset == 0
            && f.platform.area.cursor <= 1, "Home area retry did not preserve outward rounding");
        f.inputs.set_window_active(false); const auto stopped = f.platform.areas;
        require(f.inputs.synchronize_input_area(scale, width, height) && f.platform.areas == stopped, "unfocused owner moved input area");
    }
}
}
int main() {
    try { journey(false); journey(true); reused_owner(); area_projection(); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
