#pragma once

#include "allocation_probe.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace ryn_test::scaling {

using Clock = std::chrono::steady_clock;

inline void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct Options final {
    std::size_t iterations{8};
    std::size_t warmup{2};
    std::size_t rounds{1};
    bool smoke{};
    bool help{};
};

inline Options options(int argc, char** argv) {
    Options result;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--help") {
            result.help = true;
            continue;
        }
        if (argument == "--smoke") {
            result.smoke = true;
            continue;
        }
        require(argument == "--iterations" || argument == "--warmup" || argument == "--rounds", "unknown option");
        require(++index < argc, "missing numeric option value");
        const std::string_view value(argv[index]);
        std::size_t parsed{};
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        require(error == std::errc{} && end == value.data() + value.size(), "invalid numeric option value");
        require(parsed <= 100'000 && (parsed > 0 || argument == "--warmup"), "option value out of range");
        if (argument == "--iterations") {
            result.iterations = parsed;
        } else if (argument == "--warmup") {
            result.warmup = parsed;
        } else {
            result.rounds = parsed;
        }
    }
    return result;
}

inline std::int64_t nanoseconds(Clock::duration duration) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

struct Measurement final {
    std::int64_t elapsed_ns{};
    std::size_t allocations{};
    std::size_t requested_bytes{};
    std::size_t largest_request{};
};

template <typename Action> Measurement measure(Action&& action) {
    // Clock reads and allocation-probe toggles bound the same operation in both executables.
    allocation::begin();
    const auto start = Clock::now();
    try {
        action();
    } catch (...) {
        allocation::end();
        throw;
    }
    const auto end = Clock::now();
    const auto allocations = allocation::end();
    return {nanoseconds(end - start), allocations, allocation::requested_bytes, allocation::largest_request};
}

inline void accumulate(Measurement& total, Measurement sample) {
    total.elapsed_ns += sample.elapsed_ns;
    total.allocations += sample.allocations;
    total.requested_bytes += sample.requested_bytes;
    total.largest_request = std::max(total.largest_request, sample.largest_request);
}

inline std::int64_t p95(std::vector<std::int64_t> values) {
    require(!values.empty(), "cannot summarize an empty sample");
    std::ranges::sort(values);
    return values[(values.size() * 95 + 99) / 100 - 1];
}

} // namespace ryn_test::scaling
