#include <ryn/form.hpp>

// This translation unit intentionally includes only the standalone Form header.
void form_header_compile() {
    ryn::FormField<ryn::String> field;
    field.required();
    field.required(u8"This field is required");
    ryn::FormRule rule;
    rule.message = ryn::String{u8"Invalid field value"};
    field.rules({rule});
    const ryn::String literal = rule.message.value_or(ryn::String{});
    static_cast<void>(literal);
    static_cast<void>(ryn::FormProps{}.locale(ryn::FormLocale{}));
}
