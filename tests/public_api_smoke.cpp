#include <ryn/rynui.hpp>

#include <iostream>

int main() {
    const auto current = ryn::version();
    constexpr ryn::Version expected{0, 1, 0};
    ryn::String title = u8"RynUI 设备监控";

    if(current != expected) {
        std::cerr << "Unexpected RynUI version: " << current.major << '.'
                  << current.minor << '.' << current.patch << '\n';
        return 1;
    }
    if(title.empty() || title.view().size_bytes() != title.size_bytes()) {
        std::cerr << "Unexpected RynUI String state\n";
        return 1;
    }

    return 0;
}
