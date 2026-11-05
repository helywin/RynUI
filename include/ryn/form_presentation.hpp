#pragma once
#include <ryn/form_validation.hpp>
#include <ryn/form_geometry.hpp>
#include <ryn/breakpoints.hpp>
#include <ryn/control_size.hpp>
#include <ryn/input_types.hpp>
#include <ryn/text_direction.hpp>

namespace ryn {
enum class FormLayout { Horizontal, Vertical, Inline };
enum class FormLabelAlign { Left, Right };
enum class FormRequiredMark { Required, Optional, None };

struct FormColumn final {
    int span{6};
    int offset{};
    friend bool operator==(const FormColumn&, const FormColumn&) = default;
};

struct FormLocale final {
    String optional{u8"(optional)"};
    String validating{u8"Validating"};
    String required{u8"This field is required"};
    friend bool operator==(const FormLocale&, const FormLocale&) = default;
};

struct FormPresentation final {
    FormLayout layout{FormLayout::Vertical};
    Responsive<FormColumn> label_col{FormColumn{6, 0}};
    Responsive<FormColumn> wrapper_col{FormColumn{18, 0}};
    FormLabelAlign label_align{FormLabelAlign::Right};
    bool label_wrap{};
    bool colon{true};
    FormRequiredMark required_mark{FormRequiredMark::Required};
    ControlSize size{ControlSize::Middle};
    InputVariant variant{InputVariant::Outlined};
    TextDirection direction{TextDirection::Auto};
    FormLocale locale;
    friend bool operator==(const FormPresentation&, const FormPresentation&) = default;
};

struct FormScrollRequest final {
    FormNamePath path;
    FormControlGeometry control;
};

// The native host reveals the target, synchronizes geometry, and then acknowledges completion.
using FormScrollTarget = std::function<void(FormScrollRequest, std::function<void(bool)>)>;

struct FormScrollOptions final {
    bool focus{true};
};

struct FormControlTarget final {
    std::function<bool()> focus;
    std::function<void()> reset;
    std::function<std::optional<FormControlGeometry>()> geometry;
};
} // namespace ryn
