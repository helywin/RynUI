#include "component/descriptions_model.hpp"
#include <climits>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::detail;

void check(bool value) {
    if (!value) {
        throw std::runtime_error("Descriptions packing contract failed");
    }
}
} // namespace

int main() {
    try {
        std::vector<DescriptionsModelItem> items{{u8"a", 2}, {u8"b", 2}, {u8"c", 1}};
        check(pack_descriptions(3, items) == DescriptionsRows{{{0, 0, 2}, {1, 2, 1}}, {{2, 0, 3}}});
        items[1].visible = false;
        check(pack_descriptions(3, items) == DescriptionsRows{{{0, 0, 2}, {2, 2, 1}}});
        check(pack_descriptions(1, items) == DescriptionsRows{{{0, 0, 1}}, {{2, 0, 1}}});
        check(pack_descriptions(INT_MAX, items) == DescriptionsRows{{{0, 0, 2}, {2, 2, INT_MAX - 2}}});
        items[0].span = INT_MAX;
        check(pack_descriptions(3, items) == DescriptionsRows{{{0, 0, 3}}, {{2, 0, 3}}});
        check(pack_descriptions(3, {}).empty());
        for (int invalid = 0; invalid < 5; ++invalid) {
            auto bad = items;
            int columns = 3;
            if (invalid == 0) {
                columns = 0;
            }
            if (invalid == 1) {
                bad[0].span = 0;
            }
            if (invalid == 2) {
                bad[1].span = -1;
            }
            if (invalid == 3) {
                bad[1].key = bad[0].key;
            }
            if (invalid == 4) {
                bad[0].key = {};
            }
            bool rejected{};
            try {
                static_cast<void>(pack_descriptions(columns, bad));
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            check(rejected);
        }
        std::cout << "Descriptions packing contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
