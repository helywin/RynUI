#include <ryn/flex.hpp>

void invalid_flex_strings() {
    auto props = ryn::FlexProps{};
    props.justify("center").align("stretch").gap("middle");
}
