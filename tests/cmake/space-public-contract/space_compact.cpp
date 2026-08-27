#include <ryn/space.hpp>

void invalid_compact() {
    ryn::SpaceProps{}.compact(true);
}
