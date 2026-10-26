#include <ryn/rate.hpp>
#include <ryn/theme.hpp>
#include <type_traits>

static_assert(std::is_same_v<decltype(ryn::RateProps{}
                                          .value(ryn::Signal<float>{2.5F})
                                          .count(ryn::Signal<int>{5})
                                          .allowHalf()
                                          .allowClear(false)
                                          .disabled(false)
                                          .readOnly()
                                          .keyboard()
                                          .visible(true)
                                          .size(ryn::ControlSize::Small)
                                          .character(ryn::RateCharacter{ryn::IconName::HeartFilled})
                                          .tooltips({ryn::String{u8"Poor"}, ryn::String{u8"Good"}})
                                          .onChange([](float) {})
                                          .onHoverChange([](std::optional<float>) {})
                                          .layout(ryn::LayoutStyle{}.width(ryn::dp(180)))),
                             ryn::RateProps&>);

int main() {
    auto config = std::make_unique<ryn::ThemeConfig>();
    config->rate.tokens.star_size = ryn::dp(24);
    config->rate.tokens.star = ryn::Color::rgba8(1, 2, 3);
    return ryn::resolve_theme(*config).rate().metrics.star_size == 24 ? 0 : 1;
}
