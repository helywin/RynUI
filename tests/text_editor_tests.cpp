#include "input/text_editor.hpp"

#include <iostream>
#include <stdexcept>
#include <thread>

using namespace ryn::input;
namespace {
void check(bool value, const char* message) { if(!value) { throw std::runtime_error(message); } }
void ok(TextEditResult result) { check(static_cast<bool>(result), "editor operation rejected"); }
std::string bytes(std::u8string_view value) { return {reinterpret_cast<const char*>(value.data()), value.size()}; }
void expected(TextEditorState& editor, std::string_view value, TextSelection selection) {
    check(editor.value() == value, "unexpected value");
    check(editor.selection() == selection, "unexpected selection");
    check(editor.boundaries().is_boundary(selection.anchor) && editor.boundaries().is_boundary(selection.caret), "split grapheme");
}
void lifecycle() {
    TextEditorStore store;
    store.reserve(2);
    check(!store.find({}) && !store.destroy({}), "invalid owner");
    bool threw = false;
    try { static_cast<void>(store.require({})); } catch(const std::invalid_argument&) { threw = true; }
    check(threw, "invalid require");
    threw = false;
    try { static_cast<void>(store.create("\xff")); } catch(const std::invalid_argument&) { threw = true; }
    check(threw && store.size() == 0, "initial failure leaked owner");
    const auto owner = store.create("old");
    auto& editor = store.require(owner);
    bool rejected_store = false, rejected_editor = false;
    std::thread worker([&] {
        try { static_cast<void>(store.find(owner)); } catch(const std::logic_error&) { rejected_store = true; }
        try { static_cast<void>(editor.replace_selection("bad")); } catch(const std::logic_error&) { rejected_editor = true; }
    });
    worker.join();
    check(rejected_store && rejected_editor && editor.value() == "old", "wrong thread accepted");
    check(store.destroy(owner), "destroy");
    const auto replacement = store.create();
    check(replacement.index == owner.index && replacement.generation != owner.generation, "generation reuse");
    check(!store.find(owner) && !store.destroy(owner), "stale owner accepted");
    expected(store.require(replacement), "", {});
}
void selection_and_editing() {
    TextEditorStore store;
    auto& editor = store.require(store.create("abc"));
    ok(editor.move(TextCaretMove::end));
    ok(editor.move(TextCaretMove::left, true));
    expected(editor, "abc", {3,2});
    ok(editor.move(TextCaretMove::left));
    expected(editor, "abc", {2,2});
    ok(editor.select({3,0}));
    ok(editor.move(TextCaretMove::right));
    expected(editor, "abc", {3,3});
    ok(editor.move(TextCaretMove::home, true));
    ok(editor.replace_selection("x"));
    expected(editor, "x", {1,1});
    check(editor.revision() == 1, "revision counted selection");
    ok(editor.erase_backward());
    expected(editor, "", {});
    ok(editor.erase_forward());
    check(editor.revision() == 2, "no-op deletion counted mutation");
    const auto combined = bytes(u8"á中👨‍👩‍👧‍👦");
    ok(editor.set_value(combined));
    ok(editor.move(TextCaretMove::end));
    ok(editor.erase_backward());
    expected(editor, bytes(u8"á中"), {6,6});
    ok(editor.move(TextCaretMove::home));
    ok(editor.erase_forward());
    expected(editor, bytes(u8"中"), {});
    ok(editor.select({1,9999}));
    expected(editor, bytes(u8"中"), {0,3});
    ok(editor.replace_selection("Q"));
    expected(editor, "Q", {1,1});
    // Inserting a combining mark merges with the preceding grapheme.
    ok(editor.replace_selection(bytes(u8"́")));
    expected(editor, bytes(u8"Q́"), {3,3});
    ok(editor.erase_backward());
    expected(editor, "", {});
    ok(editor.set_value(bytes(u8"🇨🇳🇺🇸")));
    ok(editor.place(8));
    ok(editor.erase_backward());
    expected(editor, bytes(u8"🇺🇸"), {});
    ok(editor.replace_selection(bytes(u8"👍🏽")));
    expected(editor, bytes(u8"👍🏽🇺🇸"), {8,8});
    const auto previous_revision = editor.revision();
    const auto previous_value = std::string(editor.value());
    const auto previous_selection = editor.selection();
    check(editor.replace_selection("valid\xff").error == TextEditError::invalid_utf8, "invalid UTF-8");
    expected(editor, previous_value, previous_selection);
    check(editor.revision() == previous_revision, "invalid mutation revision");
    editor.set_eligibility(false, true);
    ok(editor.select_all());
    check(editor.erase_backward().error == TextEditError::read_only, "read-only delete");
    check(editor.replace_selection("bad").error == TextEditError::read_only, "read-only insert");
    ok(editor.move(TextCaretMove::home));
    editor.set_eligibility(true, false);
    check(editor.select_all().error == TextEditError::disabled, "disabled selection");
    check(editor.replace_selection("bad").error == TextEditError::disabled, "disabled edit");
    ok(editor.set_value("authoritative"));
    expected(editor, "authoritative", {});
}
void limits_and_words() {
    TextEditorStore store;
    auto& editor = store.require(store.create("", {3}));
    auto result = editor.replace_selection(bytes(u8"a中👍🏽"));
    check(result && result.truncated, "cluster-safe max length");
    expected(editor, bytes(u8"a中"), {4,4});
    ok(editor.select({0,1}));
    result = editor.replace_selection(bytes(u8"👍🏽"));
    check(result && !result.truncated, "selection frees scalar capacity");
    expected(editor, bytes(u8"👍🏽中"), {8,8});
    ok(editor.set_value(""));
    result = editor.replace_selection("a\r\nb\nc\rd");
    check(result && result.truncated, "newline before length");
    expected(editor, "abc", {3,3});
    ok(editor.set_limits({2}));
    expected(editor, "ab", {2,2});
    const auto revision = editor.revision();
    check(editor.set_limits({2,1}).error == TextEditError::capacity_exceeded, "capacity failure");
    expected(editor, "ab", {2,2});
    check(editor.revision() == revision && editor.limits().max_bytes > 1, "limits failure rollback");
    ok(editor.set_limits({}));
    ok(editor.set_value(bytes(u8"hello_world  中文!?👍🏽👍")));
    ok(editor.select_word(3));
    check(editor.value().substr(editor.selection().begin(), editor.selection().end() - editor.selection().begin()) == "hello_world", "word run");
    ok(editor.select_word(12));
    check(editor.selection() == TextSelection{11,13}, "whitespace run");
    ok(editor.select_word(14));
    check(editor.selection() == TextSelection{13,19}, "CJK word run");
    ok(editor.select_word(20));
    check(editor.selection() == TextSelection{19,21}, "punctuation run");
    ok(editor.select_word(22));
    check(editor.selection() == TextSelection{21,29}, "emoji whole cluster only");
    ok(editor.select_word(99999));
    check(editor.selection() == TextSelection{29,33}, "word at end clamp");
    ok(editor.set_value("abc"));
    ok(editor.select_all());
    const auto alias = editor.value();
    ok(editor.replace_selection(alias));
    expected(editor, "abc", {3,3});
    ok(editor.set_value(""));
    ok(editor.select_word(99));
    expected(editor, "", {});
}
}
int main() {
    try { lifecycle(); selection_and_editing(); limits_and_words(); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "editor lifecycle/selection/Unicode/limits/word contracts passed\n";
}
