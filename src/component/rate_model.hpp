#pragma once
#include <ryn/control_size.hpp>
#include <cstddef>

namespace ryn::detail {
void validate_rate_count(int);
void validate_rate_size(ControlSize);
float normalize_rate_value(float value, int count, bool half);
float rate_pointer_value(std::size_t index, float fraction, bool half);
} // namespace ryn::detail
