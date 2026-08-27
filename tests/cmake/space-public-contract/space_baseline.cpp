#include <ryn/space.hpp>

void invalid_baseline() {
    ryn::SpaceProps{}.baseline(true);
}
