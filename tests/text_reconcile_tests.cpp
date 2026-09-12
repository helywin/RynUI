#include "input/text_editor.hpp"
#include "support/allocation_probe.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn::input;
void check(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
void ok(TextEditResult result) { check(bool(result), "edit failed"); }
void reconcile() {
    TextEditorStore store;
    const auto id = store.create();
    auto& editor = store.require(id);
    ok(editor.commit_text("a"));
    ok(editor.note_emitted_value());
    const auto first = editor.edit_echo();
    ok(editor.commit_text("b"));
    ok(editor.note_emitted_value());
    const auto second = editor.edit_echo();
    ok(editor.select({0, 1}));
    auto result = editor.reconcile("a", first);
    check(result.stale && bool(result.edit) && editor.value() == "ab", "delayed tagged echo rolled back newer edit");
    result = editor.reconcile("ab", second);
    check(result.echoed && editor.selection() == TextSelection{0, 1} && editor.history().undo_count == 1,
        "matching echo lost selection/history");
    check(editor.reconcile("ab").echoed, "untagged equal echo not identified");
    ok(editor.update_composition({ryn::String(u8"ni"), {2, 0}}));
    check(editor.reconcile("ab", second).echoed && editor.composition().active, "duplicate echo cancelled preedit");
    result = editor.reconcile("long external value");
    check(bool(result.edit) && !result.echoed && !editor.composition().active && editor.history().undo_count == 0,
        "external conflict not a new baseline");
    ok(editor.select_all());
    ok(editor.reconcile(ryn::String(u8"你").bytes()).edit);
    check(editor.selection() == TextSelection{0, 3}, "shorter UTF8 external value not clamped");
    const auto revision = editor.revision();
    result = editor.reconcile(std::string_view("\xC0\xAF", 2));
    check(!result.edit && editor.revision() == revision && editor.value() == ryn::String(u8"你").bytes(),
        "invalid external value not atomic");
    ok(editor.commit_text("x"));
    ok(editor.note_emitted_value());
    const auto emitted_edit = editor.edit_echo();
    ok(editor.undo());
    ok(editor.note_emitted_value());
    const auto undone = editor.edit_echo();
    check(editor.reconcile(ryn::String(u8"你").bytes(), undone).echoed && editor.history().redo_count == 1,
        "undo echo discarded redo");
    check(editor.reconcile("x", emitted_edit).stale && editor.history().redo_count == 1, "old edit echo overrode undo");
    ok(editor.redo());
    check(editor.value() == "x", "redo after echo failed");
    check(store.destroy(id), "destroy failed");
    auto& next = store.require(store.create("new"));
    result = next.reconcile("old", first);
    check(result.stale && result.edit.error == TextEditError::stale_owner && next.value() == "new",
        "old owner echo changed reused slot");
}
void allocation() {
    TextEditorStore store;
    auto& editor = store.require(store.create("short"));
    editor.reserve(256);
    ok(editor.note_emitted_value());
    const auto echo = editor.edit_echo();
    const auto capacity = editor.retained_capacity();
    ryn_test::allocation::begin();
    for(int index = 0; index < 10000; ++index) {
        ok(editor.note_emitted_value());
        check(editor.reconcile("short", echo).echoed, "repeat echo");
    }
    const auto allocations = ryn_test::allocation::end();
    check(allocations == 0 && editor.retained_capacity() == capacity, "echo hot path allocated");
    ok(editor.set_value(std::string(4096, 'x')));
    ryn_test::allocation::begin(0);
    const auto failed = editor.note_emitted_value();
    ryn_test::allocation::end();
    check(failed.error == TextEditError::allocation_failure && editor.value().size() == 4096,
        "failed emission snapshot changed value");
    ok(editor.note_emitted_value());
    check(editor.reconcile(editor.value(), editor.edit_echo()).echoed, "emission retry failed");
    std::cout << "echo_cycles=10000 allocations=" << allocations << '\n';
}
}
int main() {
    try { reconcile(); allocation(); }
    catch(const std::exception& error) {
        ryn_test::allocation::end(); std::cerr << error.what() << '\n'; return 1;
    }
}
