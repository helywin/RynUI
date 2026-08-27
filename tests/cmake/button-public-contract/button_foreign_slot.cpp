#include <ryn/button.hpp>

struct ForeignSlot final {};

void declare_invalid_slot() {
    ryn::Button(
        ryn::ButtonProps{},
        ryn::SlotContent<ForeignSlot>{[] {}});
}
