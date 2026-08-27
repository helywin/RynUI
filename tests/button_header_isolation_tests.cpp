#include <ryn/button.hpp>

#include <type_traits>

static_assert(std::is_enum_v<ryn::ButtonType>);
static_assert(std::is_enum_v<ryn::ControlSize>);
static_assert(std::is_move_constructible_v<ryn::ButtonProps>);
static_assert(std::is_move_constructible_v<ryn::ButtonContent>);

int main() {
    auto declaration = [] {
        ryn::Button(
            ryn::ButtonProps{}
                .type(ryn::ButtonType::Primary)
                .size(ryn::ControlSize::Large)
                .disabled(false)
                .loading(false)
                .onClick([] {}),
            [] {});
    };
    static_cast<void>(declaration);
    return 0;
}
