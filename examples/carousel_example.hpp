#pragma once
#include <ryn/rynui.hpp>

inline void CarouselExample() {
    using namespace ryn;
    std::vector<CarouselSlide> slides;
    slides.emplace_back(
        u8"notes", CarouselContent{[] {
            Flex(FlexProps{}.vertical(true).gap(dp(8)).layout(
                     LayoutStyle{}.margin_left(dp(48)).margin_right(dp(48)).margin_top(dp(16)).margin_bottom(dp(40))),
                 [] {
                     Text(u8"Native retained draft");
                     Input(InputProps{}.defaultValue(u8"Switch slides and come back"));
                 });
        }});
    slides.emplace_back(
        u8"status", CarouselContent{[] {
            Flex(FlexProps{}.vertical(true).gap(dp(8)).layout(LayoutStyle{}.margin_left(dp(48)).margin_top(dp(16))),
                 [] {
                     Icon(IconProps{}.name(IconName::CheckCircleOutlined));
                     Text(u8"Second native panel");
                 });
        }});
    Carousel(CarouselProps{}.slides(std::move(slides)).arrows().draggable().speed(Duration::milliseconds(500)));
}
