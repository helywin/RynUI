#include "component/rate_model.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

int main() {
    using namespace ryn::detail;
    try {
        if (normalize_rate_value(2.25F, 5, true) != 2.5F || normalize_rate_value(2.25F, 5, false) != 2 ||
            normalize_rate_value(-4, 5, true) != 0 || normalize_rate_value(9, 3, true) != 3 ||
            normalize_rate_value(4, 0, true) != 0 || rate_pointer_value(2, .49F, true) != 2.5F ||
            rate_pointer_value(2, .5F, true) != 3 || rate_pointer_value(2, .1F, false) != 3) {
            throw std::runtime_error("Rate normalization or half boundary drifted");
        }
        int rejected = 0;
        for (float value : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
            try {
                static_cast<void>(normalize_rate_value(value, 5, true));
            } catch (const std::invalid_argument&) {
                ++rejected;
            }
        }
        for (int count : {-1, 1001}) {
            try {
                validate_rate_count(count);
            } catch (const std::invalid_argument&) {
                ++rejected;
            }
        }
        if (rejected != 4) {
            throw std::runtime_error("Rate accepted invalid values");
        }
        std::cout << "Rate model passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
