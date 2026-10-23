#include <ryn/popconfirm.hpp>
#include <type_traits>

static_assert(!std::is_convertible_v<ryn::PopconfirmTitle, ryn::PopconfirmDescription>);
static_assert(!std::is_convertible_v<ryn::PopconfirmTrigger, ryn::TooltipTrigger>);

int main() {
    ryn::Signal<bool> flag{false};
    ryn::Signal<ryn::String> text{ryn::String{u8"Delete?"}};
    ryn::Signal<ryn::PopconfirmPlacement> placement{ryn::PopconfirmPlacement::RightTop};
    auto props = ryn::PopconfirmProps{}
                     .open(flag)
                     .title(text)
                     .description(u8"Cannot undo")
                     .disabled(flag)
                     .showCancel(ryn::bind([flag] { return !flag.get(); }))
                     .confirmLoading(flag)
                     .okText(u8"Delete")
                     .cancelText(text)
                     .placement(placement)
                     .arrow(flag)
                     .pointAtCenter()
                     .autoAdjustOverflow(true)
                     .onConfirm([] {})
                     .onCancel([] {})
                     .onOpenChange([](bool) {})
                     .layout(ryn::LayoutStyle{}.width(ryn::dp(100)));
    auto defaults = ryn::PopconfirmProps{}.defaultOpen();
    ryn::PopconfirmSlots slots{.title = ryn::PopconfirmTitle{[] {}}, .description = ryn::PopconfirmDescription{[] {}}};
    static_cast<void>(props);
    static_cast<void>(defaults);
    static_cast<void>(slots);
}
