#include "layout_demo_runtime.hpp"

#include <ryn/rynui.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>

int main(int argc, char** argv) {
    ryn::Signal<bool> vertical{false};
    ryn::Signal<bool> wrap{true};
    ryn::Signal<ryn::FlexJustify> justify{ryn::FlexJustify::Start};
    ryn::Signal<ryn::FlexAlign> align{ryn::FlexAlign::Center};
    ryn::Signal<ryn::LayoutGap> gap{ryn::LayoutGap{ryn::SpaceSize::Middle}};
    ryn::Signal<float> grow{1.0F};
    ryn::Signal<int> order{0};
    std::uint64_t content_runs = 0;
    std::uint64_t prop_updates = 0;
    std::uint64_t activations = 0;

    const auto toggle_layout = [&] {
        vertical.set(!vertical.get());
        wrap.set(!wrap.get());
        justify.set(
            justify.get() == ryn::FlexJustify::Start
                ? ryn::FlexJustify::SpaceBetween
                : ryn::FlexJustify::Start);
        align.set(
            align.get() == ryn::FlexAlign::Center
                ? ryn::FlexAlign::Stretch
                : ryn::FlexAlign::Center);
        gap.set(
            gap.get() == ryn::LayoutGap{ryn::SpaceSize::Middle}
                ? ryn::LayoutGap{ryn::dp(12.0F), ryn::dp(20.0F)}
                : ryn::LayoutGap{ryn::SpaceSize::Middle});
        grow.set(grow.get() == 1.0F ? 2.0F : 1.0F);
        order.set(order.get() == 0 ? -1 : 0);
        prop_updates += 7;
        ++activations;
    };

    rynui::example::LayoutDemoDefinition definition{
        ryn::Content{[&] {
            ++content_runs;
            ryn::Flex(
                ryn::FlexProps{}
                    .vertical(true)
                    .gap(ryn::SpaceSize::Large),
                [&] {
                    ryn::Text(
                        ryn::TextProps{}
                            .content(u8"RynUI Flex + Space / 响应式布局")
                            .tone(ryn::TextTone::Primary));
                    ryn::Flex(
                        ryn::FlexProps{}
                            .vertical(vertical)
                            .wrap(wrap)
                            .justify(justify)
                            .align(align)
                            .gap(gap)
                            .layout(ryn::LayoutStyle{}.width(ryn::dp(880.0F))),
                        [&] {
                            ryn::Button(
                                ryn::ButtonProps{}
                                    .type(ryn::ButtonType::Primary)
                                    .layout(
                                        ryn::LayoutStyle{}
                                            .flex_grow(grow)
                                            .flex_shrink(1.0F)
                                            .order(order))
                                    .onClick(toggle_layout),
                                [] { ryn::Text(u8"切换全部布局属性"); });
                            ryn::Button(
                                ryn::ButtonProps{}
                                    .layout(
                                        ryn::LayoutStyle{}
                                            .flex_grow(1.0F)
                                            .flex_shrink(1.0F)
                                            .order(1))
                                    .onClick(toggle_layout),
                                [] { ryn::Text(u8"Pointer / Keyboard"); });
                            ryn::Text(u8"Latin + 中文 Text child");
                        });
                    ryn::Space(
                        ryn::SpaceProps{}
                            .wrap(true)
                            .align(ryn::SpaceAlign::Center)
                            .size(ryn::dp(8.0F), ryn::dp(16.0F)),
                        [&] {
                            ryn::Text(u8"Space item A");
                            ryn::Button(
                                ryn::ButtonProps{}.onClick(toggle_layout),
                                [] { ryn::Text(u8"Space item B"); });
                            ryn::Text(u8"Space 项目 C");
                        });
                });
        }},
        [&](std::size_t) { toggle_layout(); },
        [&] {
            return rynui::example::LayoutDemoTelemetry{
                content_runs,
                prop_updates,
                activations,
            };
        },
    };
    return rynui::example::run_layout_demo(argc, argv, std::move(definition));
}
