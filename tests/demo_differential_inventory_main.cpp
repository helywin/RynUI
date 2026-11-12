#include "gallery_demo_inventory.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (const auto result = rynui::example::run_gallery_demo_inventory(argc, argv, RYNUI_GALLERY_SOURCE_ROOT)) {
            return *result;
        }
        std::cerr << "expected exclusive --page-inventory=<out/file.json>\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
