#pragma once

#include "input/text_clipboard_commands.hpp"
#include "input/text_input_session.hpp"
#include "input/interaction_registry.hpp"

#include <utility>

namespace ryn::detail {

// The platform text-input session belongs to the window, independent of the
// concrete text control that currently owns the active editor.
class WindowTextEditServices final : private input::TextInputAdmission {
public:
    WindowTextEditServices(input::TextInputPlatform& platform, input::TextClipboard& clipboard,
                           const input::InteractionRegistry& registry)
        : platform_(&platform), clipboard_port_(&clipboard), registry_(&registry), sessions_(editors_, platform),
          clipboard_(editors_, clipboard) {
        sessions_.bind_admission(this);
    }

    WindowTextEditServices(const WindowTextEditServices&) = delete;
    WindowTextEditServices& operator=(const WindowTextEditServices&) = delete;

    [[nodiscard]] bool uses(input::TextInputPlatform& platform, input::TextClipboard& clipboard) const noexcept {
        return platform_ == &platform && clipboard_port_ == &clipboard;
    }

    [[nodiscard]] input::TextEditorStore& editors() noexcept {
        return editors_;
    }

    [[nodiscard]] input::TextInputSessionHost& sessions() noexcept {
        return sessions_;
    }

    [[nodiscard]] input::TextClipboardCommands& clipboard() noexcept {
        return clipboard_;
    }

    void bind_editor(input::TextInputOwnerId editor, runtime::ComponentId component, input::InteractionId interaction) {
        if (!registry_->is_owner_thread()) {
            throw std::logic_error("Text admission registration requires the window owner thread");
        }
        const auto* record = registry_->find(interaction);
        if (!editors_.find(editor) || !record || record->component != component) {
            throw std::invalid_argument("Text admission requires the exact live editor and interaction");
        }
        if (editor.index >= bindings_.size()) {
            bindings_.resize(std::size_t(editor.index) + 1);
        }
        bindings_[editor.index] = {editor, component, interaction};
    }

    void unbind_editor(input::TextInputOwnerId editor) noexcept {
        if (editor.index < bindings_.size() && bindings_[editor.index].editor == editor) {
            bindings_[editor.index] = {};
        }
    }

    void bind_text_pointer_query(input::TextPointerQuery query) noexcept {
        platform_->bind_text_pointer_query(std::move(query));
    }

    void synchronize_pointer_cursor() noexcept {
        static_cast<void>(platform_->synchronize_pointer_cursor());
    }

private:
    struct Binding final {
        input::TextInputOwnerId editor;
        runtime::ComponentId component;
        input::InteractionId interaction;
    };

    [[nodiscard]] bool permits(input::TextInputOwnerId editor) const noexcept override {
        if (!registry_->is_owner_thread() || editor.index >= bindings_.size()) {
            return false;
        }
        const auto& binding = bindings_[editor.index];
        if (binding.editor != editor) {
            return false;
        }
        const auto* record = registry_->find(binding.interaction);
        return record && record->component == binding.component && record->eligible &&
               registry_->branch_active(binding.interaction);
    }

    input::TextInputPlatform* platform_;
    input::TextClipboard* clipboard_port_;
    const input::InteractionRegistry* registry_;
    std::vector<Binding> bindings_;
    input::TextEditorStore editors_;
    input::TextInputSessionHost sessions_;
    input::TextClipboardCommands clipboard_;
};

} // namespace ryn::detail
