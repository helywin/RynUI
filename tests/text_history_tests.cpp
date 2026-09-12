#include "input/text_editor.hpp"
#include "input/text_clipboard_commands.hpp"
#include "support/allocation_probe.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn::input;
void check(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
void ok(TextEditResult value) { check(bool(value), "editor operation failed"); }
void merging() {
    TextEditorStore store;
    auto& editor = store.require(store.create());
    ok(editor.commit_text("a"));
    ok(editor.commit_text("b"));
    ok(editor.commit_text("c"));
    check(editor.history().undo_count == 1 && editor.history().merges == 2, "typing not merged");
    ok(editor.undo());
    check(editor.value().empty() && editor.selection() == TextSelection{}, "merged undo");
    ok(editor.redo());
    check(editor.value() == "abc" && editor.selection() == TextSelection{3, 3}, "merged redo");
    ok(editor.select({1, 2}));
    ok(editor.commit_text("X"));
    check(editor.history().undo_count == 2, "selection jump merged");
    ok(editor.undo());
    check(editor.value() == "abc" && editor.selection() == TextSelection{1, 2}, "selection not restored");
    ok(editor.commit_text("Y"));
    check(editor.history().redo_count == 0 && editor.value() == "aYc", "redo branch retained");
    ok(editor.undo());
    ok(editor.redo());
    editor.break_history_merge(); // submit / input-batch boundary
    ok(editor.commit_text("z"));
    check(editor.history().undo_count == 3, "explicit merge boundary ignored");
    ok(editor.select_all());
    const auto count = editor.history().undo_count;
    ok(editor.update_composition({ryn::String(u8"ni"), {2, 0}}));
    ok(editor.update_composition({ryn::String(u8"你"), {0, 1}}));
    check(editor.history().undo_count == count, "preedit entered history");
    ok(editor.commit_text(ryn::String(u8"你").bytes()));
    check(editor.history().undo_count == count + 1, "composition merged with typing");
    ok(editor.undo());
    check(editor.value() == "aYzc" && editor.selection() == TextSelection{0, 4}, "composition undo selection");
    editor.set_eligibility(false, true);
    check(editor.redo().error == TextEditError::read_only && editor.history().redo_count == 1, "read-only redo");
    editor.set_eligibility(false, false);
    ok(editor.set_value("external"));
    check(editor.history().undo_count == 0 && editor.history().redo_count == 0, "external baseline retained history");
    ok(editor.commit_text("x"));
    const auto selection = editor.selection();
    ok(editor.set_value(editor.value()));
    check(editor.history().undo_count == 1 && editor.selection() == selection, "same-value echo discarded history");
}
void ring_and_budget() {
    TextEditorStore store;
    auto& editor = store.require(store.create("item-0"));
    for(int index = 1; index <= 1000; ++index) {
        ok(editor.select_all());
        ok(editor.replace_selection("item-" + std::to_string(index)));
    }
    check(editor.history().undo_count == 128 && editor.history().evictions == 872, "transaction bound");
    for(int index = 999; index >= 872; --index) {
        ok(editor.undo());
        check(editor.value() == "item-" + std::to_string(index), "wrapped arena undo corrupted");
    }
    check(editor.history().undo_count == 0 && editor.history().redo_count == 128, "undo cursor");
    for(int index = 873; index <= 1000; ++index) {
        ok(editor.redo());
        check(editor.value() == "item-" + std::to_string(index), "wrapped arena redo corrupted");
    }
    TextHistory history;
    const std::string before(10000, 'a'), after(10000, 'b');
    for(int index = 0; index < 1000; ++index) {
        history.prepare(before, {}, after, {1, 1}, 0, false);
        history.commit();
    }
    check(history.snapshot().undo_count == 52 && history.snapshot().payload_bytes == 1040000
        && history.snapshot().storage_bytes <= TextHistory::max_payload_bytes, "byte budget eviction");
    std::string output;
    TextSelection selection;
    for(int index = 0; index < 52; ++index) {
        check(history.read_undo(output, selection) && output == before, "budget ring read corruption");
        history.commit_undo();
    }
    const auto unchanged = history.snapshot();
    bool rejected = false;
    try { history.prepare(std::string(TextHistory::max_payload_bytes, 'x'), {}, "y", {}, 0, false); }
    catch(const std::length_error&) { rejected = true; }
    check(rejected && history.snapshot().redo_count == unchanged.redo_count
        && history.snapshot().payload_bytes == unchanged.payload_bytes, "oversize transaction not atomic");
    auto& large = store.require(store.create(std::string(530000, 'a')));
    const auto result = large.replace_selection("b");
    check(!result && result.error == TextEditError::capacity_exceeded && large.value().size() == 530000
        && large.revision() == 0 && large.history().undo_count == 0, "oversize editor history not atomic");
}
void atomicity() {
    const std::string large(4096, 'x');
    std::size_t failures = 0;
    for(std::size_t point = 0; point < 200; ++point) {
        TextEditorStore store;
        auto& editor = store.require(store.create(std::string(128, 'b')));
        ok(editor.select_all());
        ok(editor.replace_selection(large));
        ryn_test::allocation::begin(point);
        const auto result = editor.undo();
        ryn_test::allocation::end();
        if(result) break;
        check(result.error == TextEditError::allocation_failure && editor.value() == large
            && editor.history().undo_count == 1 && editor.history().redo_count == 0
            && editor.revision() == 1, "failed undo moved history/value");
        ++failures;
    }
    check(failures > 0 && failures < 200, "undo preparation not failure tested");
    std::cout << "undo_atomic_points=" << failures << '\n';
}
void benchmark() {
    TextEditorStore store;
    auto& editor = store.require(store.create("abcdefgh"));
    editor.reserve(64);
    const auto cycle = [&](int index) {
        ok(editor.select_all());
        ok(editor.replace_selection(index % 2 ? "abcdefgh" : "ABCDEFGH"));
        ok(editor.undo()); ok(editor.redo());
    };
    for(int index = 0; index < 10; ++index) cycle(index);
    const auto capacity = editor.retained_capacity();
    ryn_test::allocation::begin();
    for(int index = 0; index < 20000; ++index) cycle(index);
    const auto allocations = ryn_test::allocation::end();
    check(allocations == 0 && editor.retained_capacity() == capacity && editor.history().undo_count == 128,
        "history hot path allocation/capacity growth");
    std::cout << "history_cycles=20000 allocations=" << allocations << " undo_count=" << editor.history().undo_count
        << " payload_bytes=" << editor.history().payload_bytes << " arena_bytes=" << editor.history().storage_bytes << '\n';
}
}
int main() {
    try { merging(); ring_and_budget(); atomicity(); benchmark(); }
    catch(const std::exception& error) {
        ryn_test::allocation::end(); std::cerr << error.what() << '\n'; return 1;
    }
}
