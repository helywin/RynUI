#pragma once
#include <ryn/form.hpp>
#include "runtime/component_host.hpp"

namespace ryn::detail {
// Private compound editors must not claim their outer Item's public control or Enter slot.
// A nested FormItem still owns its independent binding context.
class FormPrivateEditorScope final {
public:
    FormPrivateEditorScope();
    ~FormPrivateEditorScope();
    FormPrivateEditorScope(const FormPrivateEditorScope&) = delete;
    FormPrivateEditorScope& operator=(const FormPrivateEditorScope&) = delete;

private:
    std::optional<runtime::ComponentId> previous_;
    const runtime::ComponentHost* previous_host_{};
};

// Typed, opt-in by FormItem ancestry. A claimed slot never injects into another control.
struct FormAutoControlAccess final {
    static std::function<void()> bind(InputProps&);
    static std::function<void()> bind(TextAreaProps&);
    static std::function<void()> bind(InputNumberProps&);
    static std::function<void()> bind(SelectProps&);
    static std::function<void()> bind(CheckboxProps&);
    static std::function<void()> bind(SwitchProps&);
};
} // namespace ryn::detail
