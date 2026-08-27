#include <ryn/space.hpp>

void invalid_space_strings() {
    auto props = ryn::SpaceProps{};
    props.align("center").size("middle");
}
