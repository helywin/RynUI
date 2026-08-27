#include <ryn/flex.hpp>

struct ForeignSlot final {};

void invalid_foreign_slot() {
    ryn::Flex(ryn::FlexProps{}, ryn::SlotContent<ForeignSlot>{[] {}});
}
