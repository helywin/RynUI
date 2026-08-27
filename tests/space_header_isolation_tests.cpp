#include <ryn/space.hpp>

#include <type_traits>

static_assert(std::is_move_constructible_v<ryn::SpaceProps>);
static_assert(std::is_constructible_v<ryn::SpaceContent, void (*)()>);
static_assert(ryn::SpaceAlign::Start != ryn::SpaceAlign::End);

int main() {
    auto declaration = [] {
        ryn::Space(
            ryn::SpaceProps{}
                .vertical(false)
                .wrap(true)
                .align(ryn::SpaceAlign::Center)
                .size(ryn::SpaceSize::Large),
            [] {});
    };
    static_cast<void>(declaration);
    return 0;
}
