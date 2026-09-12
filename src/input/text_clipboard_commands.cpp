#include "input/text_clipboard_commands.hpp"

#include <stdexcept>

namespace ryn::input {

ClipboardCommandResult TextClipboardCommands::copy(TextInputOwnerId id) { return execute(id, Command::copy); }
ClipboardCommandResult TextClipboardCommands::cut(TextInputOwnerId id) { return execute(id, Command::cut); }
ClipboardCommandResult TextClipboardCommands::paste(TextInputOwnerId id) { return execute(id, Command::paste); }
const ClipboardCommandDiagnostics& TextClipboardCommands::diagnostics() const {
    if(!editors_->is_owner_thread()) throw std::logic_error("Clipboard commands accessed from non-owner thread");
    return diagnostics_;
}
ClipboardCommandResult TextClipboardCommands::execute(TextInputOwnerId id, Command command) {
    if(!editors_->is_owner_thread()) return {ClipboardError::wrong_thread, {}};
    switch(command) {
    case Command::copy: ++diagnostics_.copies; break;
    case Command::cut: ++diagnostics_.cuts; break;
    case Command::paste: ++diagnostics_.pastes; break;
    }
    ClipboardCommandResult result;
    try { result = perform(id, command); }
    catch(const std::bad_alloc&) { result.clipboard_error = ClipboardError::allocation_failure; }
    catch(const std::length_error&) { result.clipboard_error = ClipboardError::capacity_exceeded; }
    if(!result) ++diagnostics_.failures;
    if(result.edit.value_changed) ++diagnostics_.value_changes;
    return result;
}
ClipboardCommandResult TextClipboardCommands::perform(TextInputOwnerId id, Command command) {
    auto* editor = editors_->find(id);
    if(!editor) return {{}, {TextEditError::stale_owner}};
    if(editor->disabled()) return {{}, {TextEditError::disabled}};
    if(command != Command::copy && editor->read_only()) return {{}, {TextEditError::read_only}};
    const auto selection = editor->selection();
    const auto revision = editor->revision();
    if(command != Command::paste) {
        if(selection.empty()) return {};
        auto snapshot = String::from_utf8(editor->value().substr(selection.begin(), selection.end() - selection.begin()));
        if(!snapshot) return {ClipboardError::invalid_utf8, {}};
        const auto written = clipboard_->write_text(snapshot.value().view());
        if(written != ClipboardError::none) return {written, {}};
        if(command == Command::copy) return {};
        // A platform callback may change selection or destroy/reuse the owner.
        // Resolve again, never dereference the pre-call editor pointer.
        editor = editors_->find(id);
        if(!editor) return {{}, {TextEditError::stale_owner}};
        if(editor->revision() != revision) return {{}, {TextEditError::revision_conflict}};
        return {{}, editor->replace_range(selection, {})};
    }
    auto read = clipboard_->read_text();
    if(!read) return {read.error == ClipboardError::none ? ClipboardError::platform_failure : read.error, {}};
    if(read.text->empty()) return {};
    editor = editors_->find(id);
    if(!editor) return {{}, {TextEditError::stale_owner}};
    if(editor->revision() != revision) return {{}, {TextEditError::revision_conflict}};
    return {{}, editor->replace_range(selection, read.text->bytes())};
}

} // namespace ryn::input
