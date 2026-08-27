#include <ryn/flex.hpp>

void invalid_visual_entry() {
    auto props = ryn::FlexProps{};
    props.color(0x1677FF).background(0xFFFFFF).radius(6);
}
