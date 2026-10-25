#pragma once
#include <ryn/rynui.hpp>

inline void native_popover_example(ryn::Signal<bool> open, ryn::InputRef draft, std::function<void()> save) {
    ryn::Popover(ryn::PopoverProps{}
                     .title(u8"编辑草稿")
                     .open(open)
                     .triggers(ryn::PopoverTriggers{true, true, true})
                     .onOpenChange([open](bool value) { open.set(value); }),
                 ryn::PopoverTrigger{[] { ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"打开编辑器"); }); }},
                 {.content = ryn::PopoverContent{[draft, save] {
                      ryn::Flex(ryn::FlexProps{}.vertical(true).gap(ryn::dp(8)), ryn::FlexContent{[draft, save] {
                                    ryn::Input(ryn::InputProps{}
                                                   .defaultValue(u8"保留的草稿")
                                                   .ref(draft)
                                                   .layout(ryn::LayoutStyle{}.width(ryn::dp(240))));
                                    ryn::Button(ryn::ButtonProps{}.onClick(save), [] { ryn::Text(u8"保存"); });
                                }});
                  }}});
}
