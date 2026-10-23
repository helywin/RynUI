#include <ryn/modal.hpp>
#include <type_traits>

using namespace ryn;
static_assert(requires(Signal<bool> open, Signal<String> title, Signal<float> width, Signal<ModalMask> mask,
                       Signal<ModalFooterMode> footer, ModalContent body, ModalTitle heading, ModalFooter actions) {
    Modal(ModalProps{}
              .open(open)
              .title(title)
              .width(width)
              .centered(open)
              .closable(open)
              .keyboard(open)
              .mask(mask)
              .confirmLoading(open)
              .footer(footer)
              .okText(title)
              .cancelText(title)
              .restoreFocusAfterClose(open)
              .onOk([] {})
              .onCancel([](ModalCancelReason) {})
              .afterClose([] {})
              .afterOpenChange([](bool) {}),
          body, ModalSlots{.title = heading, .footer = actions});
    Modal(ModalProps{}
              .open()
              .title(u8"Confirm / 确认")
              .width(520.0F)
              .centered()
              .closable(false)
              .keyboard(false)
              .mask(ModalMask{false, true})
              .confirmLoading()
              .footer(ModalFooterMode::None)
              .okText(u8"确定")
              .cancelText(u8"取消")
              .restoreFocusAfterClose(false),
          body);
});
static_assert(!std::is_convertible_v<Content, ModalContent>);
static_assert(!std::is_convertible_v<ModalTitle, ModalFooter>);

int main() {
    return 0;
}
