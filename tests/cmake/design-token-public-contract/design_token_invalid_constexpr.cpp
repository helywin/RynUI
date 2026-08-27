#include <ryn/design_token.hpp>

constexpr ryn::Color invalid_color{2.0F, 0.0F, 0.0F};

int main() {
    return invalid_color.red() > 0.0F ? 0 : 1;
}
