#include <ryn/space.hpp>

struct ForeignSlot final {};

void invalid_foreign_slot() {
    ryn::Space(ryn::SpaceProps{}, ryn::SlotContent<ForeignSlot>{[] {}});
}
