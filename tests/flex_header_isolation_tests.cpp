#include <ryn/flex.hpp>

#include <type_traits>

static_assert(std::is_copy_constructible_v<ryn::LayoutGap>);
static_assert(std::is_move_constructible_v<ryn::FlexProps>);
static_assert(std::is_constructible_v<ryn::FlexContent, void (*)()>);
static_assert(ryn::SpaceAlign::Start != ryn::SpaceAlign::End);

int main() {
    auto declaration = [] {
        ryn::Flex(ryn::FlexProps{}
                      .vertical(false)
                      .wrap(true)
                      .justify(ryn::FlexJustify::SpaceBetween)
                      .align(ryn::FlexAlign::Center)
                      .gap(ryn::SpaceSize::Large),
                  [] {});
    };
    static_cast<void>(declaration);
    return 0;
}
