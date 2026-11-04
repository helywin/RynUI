#include "component/splitter_model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace ryn::detail {

SplitterSizes solve_splitter_sizes(float total, std::span<const float> desired,
                                   std::span<const SplitterLimits> limits) {
    if (!std::isfinite(total) || total < 0 || desired.empty() || desired.size() != limits.size()) {
        throw std::invalid_argument("Splitter solver requires a finite total and matching nonempty inputs");
    }
    std::vector<double> low(desired.size());
    std::vector<double> high(desired.size());
    std::vector<double> values(desired.size());
    double sum_low{};
    double sum_high{};
    std::size_t visible{};
    for (std::size_t i = 0; i < desired.size(); ++i) {
        if (!std::isfinite(desired[i]) || desired[i] < 0 || !std::isfinite(limits[i].minimum) ||
            limits[i].minimum < 0 || !std::isfinite(limits[i].maximum) || limits[i].maximum < 0) {
            throw std::invalid_argument("Splitter solver inputs must be finite and non-negative");
        }
        if (limits[i].collapsed) {
            continue;
        }
        ++visible;
        low[i] = std::min(total, limits[i].minimum);
        high[i] = std::max(low[i], static_cast<double>(std::min(total, limits[i].maximum)));
        sum_low += low[i];
        sum_high += high[i];
    }
    if (visible == 0) {
        return SplitterSizes(desired.size(), 0);
    }
    if (sum_low > total && sum_low > 0) {
        for (auto& value : low) {
            value *= total / sum_low;
        }
    }
    if (sum_high < total) {
        const double extra = (total - sum_high) / static_cast<double>(visible);
        for (std::size_t i = 0; i < high.size(); ++i) {
            if (!limits[i].collapsed) {
                high[i] += extra;
            }
        }
    }
    for (std::size_t i = 0; i < desired.size(); ++i) {
        values[i] = std::clamp<double>(desired[i], low[i], high[i]);
    }
    double difference = total - std::accumulate(values.begin(), values.end(), 0.0);
    for (std::size_t iteration = 0; iteration <= values.size() && std::abs(difference) > 1e-7; ++iteration) {
        std::size_t count{};
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (difference > 0 ? values[i] < high[i] - 1e-8 : values[i] > low[i] + 1e-8) {
                ++count;
            }
        }
        if (count == 0) {
            break;
        }
        const double share = difference / static_cast<double>(count);
        for (std::size_t i = 0; i < values.size(); ++i) {
            const double before = values[i];
            values[i] = std::clamp(values[i] + share, low[i], high[i]);
            difference -= values[i] - before;
        }
    }
    SplitterSizes result;
    result.reserve(values.size());
    float remaining = total;
    std::size_t last_visible{};
    for (std::size_t i = 0; i < limits.size(); ++i) {
        if (!limits[i].collapsed) {
            last_visible = i;
        }
    }
    for (std::size_t i = 0; i < values.size(); ++i) {
        const float value = i == last_visible ? remaining : std::clamp(static_cast<float>(values[i]), 0.0F, remaining);
        result.push_back(value);
        remaining = std::max(0.0F, remaining - value);
    }
    return result;
}

SplitterSizes resize_splitter_pair(std::span<const float> sizes, std::size_t index, float desired_start,
                                   std::span<const SplitterLimits> limits) {
    if (index + 1 >= sizes.size() || sizes.size() != limits.size() || !std::isfinite(desired_start)) {
        throw std::invalid_argument("Splitter pair is invalid");
    }
    const float total = sizes[index] + sizes[index + 1];
    const float desired = std::clamp(desired_start, 0.0F, total);
    const std::array pair{desired, total - desired};
    auto pair_limits = std::array{limits[index], limits[index + 1]};
    pair_limits[0].collapsed = false;
    pair_limits[1].collapsed = false;
    const auto resolved = solve_splitter_sizes(total, pair, pair_limits);
    auto result = SplitterSizes{sizes.begin(), sizes.end()};
    result[index] = resolved[0];
    result[index + 1] = resolved[1];
    return result;
}

} // namespace ryn::detail
