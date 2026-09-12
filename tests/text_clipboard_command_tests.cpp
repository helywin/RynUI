#include "input/text_clipboard_commands.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace ryn::input;
using ryn::String;
void check(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
void ok(TextEditResult value) { check(bool(value), "editor operation failed"); }
struct Clipboard final : TextClipboard {
    String value;
    ClipboardError error{ClipboardError::none};
    std::function<void()> during_read, during_write;
    int reads{}, writes{};
    ClipboardReadResult read_text() override {
        ++reads;
        auto snapshot = value;
        if(during_read) during_read();
        return error == ClipboardError::none ? ClipboardReadResult{{}, std::move(snapshot)}
                                            : ClipboardReadResult{error, {}};
    }
    ClipboardError write_text(ryn::StringView text) override {
        ++writes;
        if(during_write) during_write();
        if(error != ClipboardError::none) return error;
        value = String::from_utf8(text.bytes()).value();
        return ClipboardError::none;
    }
    ClipboardAvailability has_text() const noexcept override { return {error, !value.empty()}; }
};
void commands() {
    TextEditorStore store;
    Clipboard clipboard;
    TextClipboardCommands commands(store, clipboard);
    const auto id = store.create(String(u8"a你😀z").bytes());
    auto& editor = store.require(id);
    ok(editor.select({1, 8}));
    check(bool(commands.copy(id)) && clipboard.value == String(u8"你😀"), "copy selected UTF8");
    editor.set_eligibility(false, true);
    check(bool(commands.copy(id)), "read-only copy rejected");
    check(commands.cut(id).edit.error == TextEditError::read_only
        && commands.paste(id).edit.error == TextEditError::read_only, "read-only edit allowed");
    editor.set_eligibility(false, false);
    clipboard.error = ClipboardError::platform_failure;
    const auto revision = editor.revision();
    check(!commands.cut(id) && !commands.paste(id), "platform error accepted");
    check(editor.value() == String(u8"a你😀z").bytes() && editor.selection() == TextSelection{1, 8}
        && editor.revision() == revision, "clipboard failure mutated editor");
    clipboard.error = ClipboardError::none;
    clipboard.during_write = [&] { ok(editor.select({0, 1})); };
    check(bool(commands.cut(id)) && editor.value() == "az" && clipboard.value == String(u8"你😀"),
        "cut did not use command-time selection snapshot");
    clipboard.during_write = {};
    ok(editor.select({1, 1}));
    clipboard.value = String(u8"你\r\n好\n😀");
    clipboard.during_read = [&] {
        clipboard.value = String(u8"changed clipboard");
        ok(editor.select_all());
    };
    const auto before_paste = editor.revision();
    check(bool(commands.paste(id)) && editor.value() == String(u8"a你好😀z").bytes()
        && editor.revision() == before_paste + 1, "paste snapshot/newline/single transaction");
    clipboard.during_read = {};
    ok(editor.select_all());
    clipboard.value = String{};
    check(bool(commands.paste(id)) && editor.value() == String(u8"a你好😀z").bytes(), "empty paste deleted selection");
    clipboard.value = String(u8"conflict");
    clipboard.during_read = [&] { ok(editor.set_value("external")); };
    check(commands.paste(id).edit.error == TextEditError::revision_conflict && editor.value() == "external",
        "paste overwrote callback's newer value");
    clipboard.during_read = {};
    ok(editor.set_value(""));
    ok(editor.set_limits({2, 100}));
    clipboard.value = String(u8"a👩‍💻b");
    const auto limited = commands.paste(id);
    check(bool(limited) && limited.edit.truncated && editor.value() == "a", "cluster-safe maxLength paste");
    ok(editor.set_limits({100, 2}));
    clipboard.value = String(u8"你好");
    check(commands.paste(id).edit.error == TextEditError::capacity_exceeded && editor.value() == "a", "large paste not atomic");
    editor.set_eligibility(true, false);
    check(commands.copy(id).edit.error == TextEditError::disabled, "disabled copy accepted");
    editor.set_eligibility(false, false);
    bool rejected = false;
    std::thread worker([&] { rejected = commands.copy(id).clipboard_error == ClipboardError::wrong_thread; });
    worker.join(); check(rejected, "off-thread clipboard command");
    clipboard.during_read = [&] {
        check(store.destroy(id), "destroy during clipboard read");
        static_cast<void>(store.create("replacement"));
    };
    check(commands.paste(id).edit.error == TextEditError::stale_owner, "destroy/reuse clipboard mutation");
    check(commands.diagnostics().value_changes == 3 && commands.diagnostics().failures >= 7, "command diagnostics");
}
}
int main() {
    try { commands(); std::cout << "Clipboard commands, snapshots, races and eligibility passed\n"; }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
