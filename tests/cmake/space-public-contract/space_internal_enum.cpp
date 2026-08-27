#include "layout/layout_engine.hpp"

#include <ryn/space.hpp>

void invalid_internal_enum() {
    auto props = ryn::SpaceProps{};
    props.align(ryn::layout::FlexAlign::center);
}
