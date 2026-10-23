#include <ryn/rynui.hpp>
#include <type_traits>

using namespace ryn;
static_assert(requires(Signal<bool> open, Signal<String> title, Signal<float> length, Signal<DrawerPlacement> edge,
                       Signal<DrawerMask> mask, DrawerContent body, DrawerTitle heading, DrawerFooter footer) {
    Drawer(DrawerProps{}
               .open(open)
               .title(title)
               .placement(edge)
               .width(length)
               .height(length)
               .closable(open)
               .keyboard(open)
               .mask(mask)
               .restoreFocusAfterClose(open)
               .onCancel([](DrawerCancelReason) {})
               .afterClose([] {})
               .afterOpenChange([](bool) {}),
           body, DrawerSlots{.title = heading, .footer = footer});
    Drawer(DrawerProps{}
               .open()
               .title(u8"详情")
               .placement(DrawerPlacement::Left)
               .width(420.0F)
               .height(300.0F)
               .closable(false)
               .keyboard(false)
               .mask(DrawerMask{false, false})
               .restoreFocusAfterClose(false),
           body);
});
static_assert(!std::is_convertible_v<Content, DrawerContent>);
static_assert(!std::is_convertible_v<DrawerTitle, DrawerFooter>);
static_assert(!std::is_convertible_v<ModalContent, DrawerContent>);

int main() {
    return 0;
}
