#include <ryn/space.hpp>

void invalid_separator() {
    ryn::SpaceProps{}.separator([] {});
}
