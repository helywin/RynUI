#pragma once
#include <ryn/form.hpp>
#include "runtime/component_host.hpp"

namespace ryn::detail {
std::function<void()> form_enter_handler(runtime::ComponentId);

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

class FormControlMount final {
public:
    FormControlMount() = default;
    FormControlMount(std::shared_ptr<FormControlConnection>, std::function<void()>);
    FormControlMount(FormControlMount&&) = default;
    FormControlMount(const FormControlMount&) = delete;
    ~FormControlMount();
    explicit operator bool() const noexcept;
    void operator()() const;

private:
    std::shared_ptr<FormControlConnection> connection_;
    std::function<void()> attach_;
    mutable bool complete_{};
};

// Typed, opt-in by FormItem ancestry. A claimed slot never injects into another control.
struct FormAutoControlAccess final {
    static FormControlMount bind(InputProps&);
    static FormControlMount bind(TextAreaProps&);
    static FormControlMount bind(InputNumberProps&);
    static FormControlMount bind(SelectProps&);
    static FormControlMount bind(CheckboxProps&);
    static FormControlMount bind(SwitchProps&);
};
} // namespace ryn::detail
