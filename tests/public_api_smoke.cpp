#include <ryn/rynui.hpp>

#include <iostream>

int main() {
    const auto current = ryn::version();
    constexpr ryn::Version expected{0, 1, 0};

    if(current != expected) {
        std::cerr << "Unexpected RynUI version: " << current.major << '.'
                  << current.minor << '.' << current.patch << '\n';
        return 1;
    }

    return 0;
}
