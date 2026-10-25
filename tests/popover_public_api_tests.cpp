#include <ryn/rynui.hpp>
#include <type_traits>

static_assert(!std::is_convertible_v<ryn::PopoverTitle, ryn::PopoverContent>);
static_assert(!std::is_convertible_v<ryn::TooltipTitle, ryn::PopoverTitle>);
static_assert(std::is_same_v<ryn::PopoverPlacement, ryn::TooltipPlacement>);

int main() {
    ryn::Signal<bool> open{false};
    ryn::Signal<ryn::String> title{ryn::String{u8"Title"}};
    ryn::Signal<ryn::PopoverTriggers> triggers{ryn::PopoverTriggers{true, true, true}};
    auto props = ryn::PopoverProps{}
                     .title(title)
                     .content(u8"Content")
                     .open(open)
                     .disabled(false)
                     .triggers(triggers)
                     .placement(ryn::PopoverPlacement::BottomLeft)
                     .arrow(true)
                     .pointAtCenter()
                     .autoAdjustOverflow(true)
                     .mouseEnterDelay(ryn::Duration::milliseconds(100))
                     .mouseLeaveDelay(ryn::Duration::milliseconds(200))
                     .onOpenChange([](bool) {})
                     .layout(ryn::LayoutStyle{}.margin_top(ryn::dp(8)));
    auto defaults = ryn::PopoverProps{}.defaultOpen().trigger(ryn::PopoverTriggerMode::HoverFocus);
    ryn::PopoverSlots slots{.title = ryn::PopoverTitle{[] { ryn::Text(u8"Rich title"); }},
                            .content = ryn::PopoverContent{[] { ryn::Input(ryn::InputProps{}); }}};
    static_cast<void>(props);
    static_cast<void>(defaults);
    static_cast<void>(slots);
}
