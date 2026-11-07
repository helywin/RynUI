#include <ryn/form_validation.hpp>
#include <concepts>

static_assert(std::same_as<decltype(ryn::FormRule{}.message), std::optional<ryn::String>>);

// Aggregate initialization and both common assignment forms remain public API.
void form_validation_header_compile() {
    ryn::FormRule rule{.required = true, .message = ryn::String{u8"Required"}};
    rule.message = u8"Literal";
    rule.message = ryn::String{u8"Literal String"};
    if (rule.message) {
        static_cast<void>(rule.message->view());
    }
    rule.message.reset();
}
