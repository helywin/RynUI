#include <ryn/app_panel.hpp>
#include <ryn/app.hpp>
#include <ryn/input.hpp>
#include <ryn/text.hpp>

void public_panel_services(const ryn::AppRef& app, const ryn::PopconfirmRef& anchor) {
    ryn::Popconfirm(ryn::PopconfirmProps{}.serviceAnchor(anchor),
                    ryn::PopconfirmTrigger{[] { ryn::Text(u8"Anchor"); }});
    auto modal = app.modal().confirm(
        ryn::ModalProps{}.onOkAsync(ryn::ConfirmAction::synchronous([] { return ryn::ConfirmActionResult{}; })),
        ryn::ModalContent{[] { ryn::Input(ryn::InputProps{}); }});
    modal.update({.title = ryn::Prop<ryn::String>{ryn::String{u8"Updated"}}});
    modal.close();
    auto drawer = app.drawer().open(ryn::DrawerProps{}, ryn::DrawerContent{[] { ryn::Text(u8"Details"); }});
    drawer.update({.width = ryn::Prop<float>{420.0F}});
    drawer.destroy();
    auto popconfirm = app.popconfirm().confirm(anchor, ryn::PopconfirmProps{}.title(u8"Delete?"));
    popconfirm.update({.description = ryn::Prop<ryn::String>{ryn::String{u8"Cannot undo"}}});
    popconfirm.close();
    const auto future = modal.closed();
    static_cast<void>(future);
}
