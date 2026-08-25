#include <ryn/rynui.hpp>

#include <iostream>

int main() {
    const auto current = ryn::version();
    std::cout << "RynUI " << current.major << '.' << current.minor << '.'
              << current.patch << '\n';
    return 0;
}
