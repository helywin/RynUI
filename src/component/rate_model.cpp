#include "component/rate_model.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
void validate_rate_count(int count) {
    if (count < 0 || count > 1000) {
        throw std::invalid_argument("Rate count must be in [0, 1000]");
    }
}

void validate_rate_size(ControlSize size) {
    if (size != ControlSize::Small && size != ControlSize::Middle && size != ControlSize::Large) {
        throw std::invalid_argument("Rate size is invalid");
    }
}

float normalize_rate_value(float value, int count, bool half) {
    validate_rate_count(count);
    if (!std::isfinite(value)) {
        throw std::invalid_argument("Rate value must be finite");
    }
    const float scale = half ? 2.0F : 1.0F;
    return std::round(std::clamp(value, 0.0F, static_cast<float>(count)) * scale) / scale;
}

float rate_pointer_value(std::size_t index, float fraction, bool half) {
    return static_cast<float>(index) + (half && fraction < .5F ? .5F : 1.0F);
}
} // namespace ryn::detail
