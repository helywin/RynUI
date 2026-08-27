#include <ryn/space.hpp>

void invalid_visual_entry() {
    auto props = ryn::SpaceProps{};
    props.color(0x1677FF).background(0xFFFFFF).radius(6);
}
