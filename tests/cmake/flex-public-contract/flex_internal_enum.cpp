#include "layout/layout_engine.hpp"

#include <ryn/flex.hpp>

void invalid_internal_enum() {
    auto props = ryn::FlexProps{};
    props.justify(ryn::layout::FlexJustify::center);
}
