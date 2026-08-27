#include <ryn/design_token.hpp>

#include <type_traits>

static_assert(std::is_copy_constructible_v<ryn::Color>);
static_assert(std::is_copy_constructible_v<ryn::ShadowList>);
static_assert(!std::is_constructible_v<ryn::ShadowList, const char*>);

int main() {
    const auto* token = ryn::find_ant_design_token("ant.seed.colorPrimary");
    return token == nullptr ? 1 : 0;
}
