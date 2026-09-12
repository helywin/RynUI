#include "input/text_input_session.hpp"

#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace ryn::input;
using ryn::String;
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
void ok(TextEditResult value) { require(bool(value), "editor operation failed"); }
struct Platform final : TextInputPlatform {
    int starts{}, stops{}, cancels{}, areas{};
    bool fail_start{}, fail_stop{}, fail_cancel{}, fail_area{};
    TextInputSessionStamp stamp;
    WindowTextInputArea area;
    bool start(TextInputSessionStamp value, const TextInputProperties&) noexcept override {
        ++starts; if(fail_start) return false; stamp = value; return true;
    }
    bool stop() noexcept override { ++stops; stamp = {}; return !fail_stop; }
    bool cancel() noexcept override { ++cancels; return !fail_cancel; }
    bool set_area(const WindowTextInputArea& value) noexcept override {
        ++areas; if(fail_area) return false; area = value; return true;
    }
};
void lifecycle() {
    TextEditorStore store;
    Platform platform;
    const auto a = store.create("a");
    const auto b = store.create("b");
    TextInputSessionHost host(store, platform);
    require(!host.focus({}), "invalid focus accepted");
    require(host.focus(a), "focus failed");
    const auto first = host.active();
    require(host.focus(a) && platform.starts == 1, "duplicate start");
    ok(host.dispatch(CompositionChanged{String(u8"ni"), {2, 0}, first}));
    require(store.require(a).composition().active, "composition missing");
    require(host.focus(b), "transfer failed");
    require(!store.require(a).composition().active && platform.stops == 1, "transfer cleanup failed");
    require(!host.dispatch(TextCommitted{String(u8"你"), first}), "late event accepted");
    require(store.require(b).value() == "b", "late event changed new owner");
    auto second = host.active();
    store.require(b).set_eligibility(false, true);
    require(!host.active().valid() && platform.stops == 2, "read-only session active");
    store.require(b).set_eligibility(false, false);
    require(host.active().valid() && host.active() != second, "same-owner epoch reused");
    require(!host.dispatch(TextCommitted{String(u8"x"), second}), "late same-owner event accepted");
    require(host.set_window_active(false), "window blur failed");
    require(!host.active().valid(), "inactive window owns session");
    const auto stops = platform.stops;
    require(host.set_window_active(false) && platform.stops == stops, "duplicate stop");
    require(host.set_window_active(true), "window focus failed");
    second = host.active();
    require(store.destroy(b), "destroy failed");
    require(!host.active().valid(), "destroy left session active");
    const auto reused = store.create("new");
    require(reused.index == b.index && reused.generation != b.generation, "slot not reused safely");
    require(host.focus(reused), "reused focus failed");
    require(!host.dispatch(TextCommitted{String(u8"late"), second}), "destroyed generation accepted");
    bool wrong_thread = false;
    std::thread worker([&] { try { host.blur(); } catch(const std::logic_error&) { wrong_thread = true; } });
    worker.join();
    require(wrong_thread, "wrong thread accepted");
    bool duplicate = false;
    try { TextInputSessionHost other(store, platform); } catch(const std::logic_error&) { duplicate = true; }
    require(duplicate, "two sessions attached to a window store");
    TextEditorStore other_store;
    duplicate = false;
    try { TextInputSessionHost other(other_store, platform); } catch(const std::logic_error&) { duplicate = true; }
    require(duplicate && host.active().owner == reused, "second store stole the window session");
    store.require(reused).set_eligibility(true, false);
    require(!host.active().valid(), "disabled session active");
}
void failures() {
    TextEditorStore store;
    Platform platform;
    TextInputSessionHost host(store, platform);
    const auto a = store.create();
    const auto b = store.create();
    platform.fail_start = true;
    require(!host.focus(a) && !host.active().valid(), "failed start published identity");
    platform.fail_start = false;
    require(host.synchronize(), "start retry failed");
    const auto old = host.active();
    platform.fail_stop = true;
    require(!host.focus(b) && !host.active().valid(), "failed stop started replacement");
    require(!host.dispatch(TextCommitted{String(u8"late"), old}), "failed stop accepted late data");
    require(platform.starts == 2, "new start occurred before stop succeeded");
    platform.fail_stop = false;
    require(host.synchronize() && host.active().owner == b, "stop retry failed");
    platform.fail_cancel = true;
    require(!host.cancel_composition(), "cancel failure hidden");
    require(host.diagnostics().failures >= 4, "failure diagnostics missing");
}
void composition() {
    TextEditorStore store;
    Platform platform;
    TextInputSessionHost host(store, platform);
    const auto id = store.create("ab");
    auto& editor = store.require(id);
    ok(editor.select({1, 2}));
    require(host.focus(id), "focus failed");
    const auto stamp = host.active();
    const auto revision = editor.revision();
    ok(host.dispatch(CandidatesChanged{{String(u8"你"), String(u8"拟")}, 0,
        CandidateOrientation::horizontal, stamp}));
    ok(host.dispatch(CompositionChanged{String(u8"ni"), {2, 0}, stamp}));
    ok(host.dispatch(CompositionChanged{String(u8"你"), {0, 1}, stamp}));
    require(editor.value() == "ab" && editor.revision() == revision, "preedit changed committed value");
    require(editor.composition().candidates.size() == 2, "candidate-first order lost snapshot");
    ok(editor.set_value("ab"));
    require(editor.composition().active, "same-value echo cancelled composition");
    ok(host.dispatch(TextCommitted{String(u8"你"), stamp}));
    require(editor.value() == String(u8"a你").bytes() && editor.revision() == revision + 1, "commit was not one edit");
    require(!editor.composition().active && editor.composition().candidates.empty(), "commit left transient state");
    ok(host.dispatch(CompositionChanged{{}, {}, stamp}));
    require(editor.value() == String(u8"a你").bytes(), "post-commit empty event erased value");
    ok(host.dispatch(CompositionChanged{String(u8"ㅎ"), {1, 0}, stamp}));
    ok(host.dispatch(CompositionChanged{String(u8"하"), {1, 0}, stamp}));
    ok(host.dispatch(CompositionChanged{String(u8"한"), {1, 0}, stamp}));
    ok(host.dispatch(TextCommitted{String(u8"한"), stamp}));
    require(editor.value() == String(u8"a你한").bytes(), "Korean composition not committed");
    ok(host.dispatch(CompositionChanged{String(u8"👩‍💻"), {0, 3}, stamp}));
    ok(editor.set_value("external"));
    require(!editor.composition().active, "external conflict retained old replacement range");
    ok(host.dispatch(CompositionChanged{String(u8"emoji"), {0, 0, false}, stamp}));
    const auto before = editor.value();
    ok(host.dispatch(TextCommitted{{}, stamp}));
    require(editor.value() == before && !editor.composition().active, "empty commit deletes selection");
    ok(editor.select_all());
    ok(host.dispatch(CompositionChanged{String(u8"👩‍💻"), {0, 3}, stamp}));
    ok(editor.set_limits({2, 100}));
    ok(host.dispatch(TextCommitted{String(u8"👩‍💻"), stamp}));
    require(editor.boundaries().scalar_count() <= 2, "commit exceeded scalar budget");
    ok(editor.set_value("ok"));
    ok(editor.set_limits({100, 2}));
    ok(host.dispatch(CompositionChanged{String(u8"你"), {0, 1}, stamp}));
    require(!host.dispatch(TextCommitted{String(u8"你"), stamp}), "byte capacity failure accepted");
    require(editor.value() == "ok" && editor.composition().active, "failed commit not atomic");
    require(host.blur() && !editor.composition().active, "blur failed to clean composition");
}
void area() {
    TextEditorStore store;
    Platform platform;
    TextInputSessionHost host(store, platform);
    require(host.focus(store.create()), "focus failed");
    for(double scale : {1.0, 1.25, 1.5, 2.0}) {
        TextInputAreaGeometry g{{10, 20, 100, 24}, {0, 0, 800, 600}, 5, 3, 50, scale, 1600, 1200};
        const auto mapped = map_text_input_area(g);
        require(mapped.has_value(), "scale mapping rejected");
        require(host.set_input_area(g) && platform.area == *mapped, "platform area mismatch");
        const auto calls = platform.areas;
        require(host.set_input_area(g) && calls == platform.areas, "same area not elided");
        g.translation_x -= 30;
        require(host.set_input_area(g) && platform.area.x == 0, "negative scroll not clipped");
        g.caret_x = 5000;
        require(host.set_input_area(g) && platform.area.cursor == platform.area.width, "offscreen cursor not clamped");
    }
    TextInputAreaGeometry g{{0.2, 0.3, 2.2, 3.2}, {0, 0, 100, 100}, 0, 0, 1, 1, 100, 100};
    require(map_text_input_area(g) == WindowTextInputArea{0, 0, 3, 4, 1}, "outward rounding mismatch");
    platform.fail_area = true;
    require(!host.set_input_area(g), "area failure hidden");
    platform.fail_area = false;
    require(host.synchronize() && platform.area.width == 3, "area retry failed");
    const auto calls = platform.areas;
    require(host.set_window_active(false) && host.set_window_active(true), "window focus failed");
    require(platform.areas == calls + 1, "area not restored on focus gain");
    g.logical_to_window_scale = std::numeric_limits<double>::infinity();
    require(!host.set_input_area(g), "nonfinite scale accepted");
}
}
int main() {
    try { lifecycle(); failures(); composition(); area(); std::cout << "Text session, composition and area passed\n"; }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
