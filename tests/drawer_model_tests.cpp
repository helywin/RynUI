#include "component/drawer_model.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace ryn;
using namespace ryn::detail;

int main() {
    try {
        const auto check = [](bool value) {
            if (!value) {
                throw std::runtime_error("Drawer geometry contract failed");
            }
        };
        check(drawer_bounds({}, {10, 20, 700, 600}) == runtime::Rect{332, 20, 378, 600});
        check(drawer_bounds({DrawerPlacement::Left, 240, 200}, {10, 20, 700, 600}) == runtime::Rect{10, 20, 240, 600});
        check(drawer_bounds({DrawerPlacement::Top, 240, 200}, {10, 20, 700, 600}) == runtime::Rect{10, 20, 700, 200});
        check(drawer_bounds({DrawerPlacement::Bottom, 240, 200}, {10, 20, 700, 600}) ==
              runtime::Rect{10, 420, 700, 200});
        for (auto edge :
             {DrawerPlacement::Left, DrawerPlacement::Right, DrawerPlacement::Top, DrawerPlacement::Bottom}) {
            check(drawer_bounds({edge, 500, 500}, {0, 0, 30, 20}) == runtime::Rect{0, 0, 30, 20});
            check(drawer_bounds({edge, 500, 500}, {}) == runtime::Rect{});
        }
        for (float value :
             {0.0F, -1.0F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
            for (bool width : {false, true}) {
                bool rejected{};
                try {
                    validate_drawer_geometry({DrawerPlacement::Right, width ? value : 378, width ? 378 : value});
                } catch (const std::invalid_argument&) {
                    rejected = true;
                }
                check(rejected);
            }
        }
        bool rejected{};
        try {
            validate_drawer_geometry({static_cast<DrawerPlacement>(99)});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
