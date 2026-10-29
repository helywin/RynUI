#include "graphics/glyph_scene.hpp"
#include "support/allocation_probe.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using ryn::graphics::GlyphInstance;
using ryn::graphics::GlyphInstanceRange;
using ryn::graphics::GlyphInstanceStore;
namespace allocation = ryn_test::allocation;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct Options {
    std::size_t iterations{2000};
    std::size_t rounds{3};
    bool baseline{};
};

Options options(int argc, char** argv) {
    Options result;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--baseline") {
            result.baseline = true;
        } else if ((argument == "--iterations" || argument == "--rounds") && index + 1 < argc) {
            const auto value = std::stoull(argv[++index]);
            require(value > 0 && value <= 1000000, "benchmark count must be within [1, 1000000]");
            if (argument == "--iterations") {
                result.iterations = static_cast<std::size_t>(value);
            } else {
                result.rounds = static_cast<std::size_t>(value);
            }
        } else {
            throw std::invalid_argument("usage: glyph_replace_benchmark [--iterations N] [--rounds N] [--baseline]");
        }
    }
    return result;
}

void run(std::uint32_t store_count, std::string_view mode, std::size_t round, const Options& options) {
    std::vector<GlyphInstance> initial(store_count);
    for (std::uint32_t index = 0; index < store_count; ++index) {
        initial[index].position_size = {static_cast<float>(index), 0, 12, 14};
    }
    GlyphInstanceStore store;
    static_cast<void>(store.append(initial));
    const GlyphInstanceRange target{store_count / 2, 4};
    std::array<GlyphInstance, 4> external;
    const auto source = [&](std::size_t iteration) -> std::span<const GlyphInstance> {
        if (mode == "external") {
            for (std::size_t index = 0; index < external.size(); ++index) {
                external[index].position_size = {-static_cast<float>(1 + index + iteration % 2), 0, 12, 14};
            }
            return external;
        }
        const auto first = mode == "overlap-left"    ? target.first - 1
                           : mode == "overlap-right" ? target.first + 1
                                                     : target.first;
        return store.instances().subspan(first, target.count);
    };
    const auto check_update = [&](GlyphInstanceRange result, const std::array<GlyphInstance, 4>& expected) {
        require(result == target && store.size() == store_count &&
                    std::ranges::equal(store.instances().subspan(target.first, target.count), expected) &&
                    std::ranges::equal(store.geometry_dirty_ranges(), std::array{target}) &&
                    store.material_dirty_ranges().empty(),
                "benchmark replacement changed content, range or dirty contract");
    };

    auto input = source(0);
    std::array<GlyphInstance, 4> expected;
    std::copy(input.begin(), input.end(), expected.begin());
    const auto* cold_data = store.instances().data();
    allocation::begin();
    const auto cold_result = store.replace(target, input);
    const auto cold_allocations = allocation::end();
    const auto cold_bytes = allocation::requested_bytes;
    const auto cold_largest = allocation::largest_request;
    const bool cold_reallocated = store.instances().data() != cold_data;
    check_update(cold_result, expected);
    if (options.baseline) {
        require(cold_reallocated && cold_largest >= store_count * sizeof(GlyphInstance),
                "--baseline requires the pinned full-store replacement implementation");
    } else {
        require(!cold_reallocated && cold_allocations <= 1 && cold_bytes <= sizeof(expected),
                "cold equal-length replacement allocated background-sized storage");
    }

    std::vector<std::int64_t> samples;
    samples.reserve(options.iterations);
    std::size_t allocations{};
    std::size_t requested_bytes{};
    std::size_t largest_request{};
    std::size_t store_reallocations{};
    std::int64_t elapsed_ns{};
    for (std::size_t iteration = 0; iteration < options.iterations; ++iteration) {
        store.clear_dirty_ranges();
        input = source(iteration);
        std::copy(input.begin(), input.end(), expected.begin());
        const auto* retained = store.instances().data();
        allocation::begin();
        const auto start = std::chrono::steady_clock::now();
        const auto result = store.replace(target, input);
        const auto stop = std::chrono::steady_clock::now();
        allocations += allocation::end();
        requested_bytes += allocation::requested_bytes;
        largest_request = std::max(largest_request, allocation::largest_request);
        store_reallocations += store.instances().data() != retained;
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
        samples.push_back(ns);
        elapsed_ns += ns;
        check_update(result, expected);
    }
    require(std::ranges::equal(store.instances().first(target.first), std::span{initial}.first(target.first)) &&
                std::ranges::equal(store.instances().subspan(target.first + target.count),
                                   std::span{initial}.subspan(target.first + target.count)),
            "benchmark replacement changed neighboring glyphs");
    std::ranges::sort(samples);
    // This is a source-derived bound, NOT an instrumented memcpy/memmove byte counter.
    // Pinned baseline: prefix + replacement + suffix inserts copy store_count glyphs.
    // Local path: one memmove of target.count glyphs; identical aliases may move fewer bytes.
    const auto copy_bound = (options.baseline ? store_count : target.count) * sizeof(GlyphInstance);
    std::cout << "round=" << round << " mode=" << mode << " store_glyphs=" << store_count
              << " updates=" << options.iterations << " glyph_bytes=" << sizeof(GlyphInstance)
              << " cold_allocations=" << cold_allocations << " cold_requested_bytes=" << cold_bytes
              << " cold_largest_request=" << cold_largest << " cold_store_reallocated=" << cold_reallocated
              << " allocations=" << allocations << " requested_bytes=" << requested_bytes
              << " largest_request=" << largest_request << " store_reallocations=" << store_reallocations
              << " dirty_first=" << target.first << " dirty_count=" << target.count
              << " dirty_bytes_per_update=" << store.bytes(target).size()
              << " copy_model=" << (options.baseline ? "pinned-full-store-inserts" : "local-memmove")
              << " copy_bytes_source_upper_bound_per_update=" << copy_bound
              << " copy_bytes_source_upper_bound_total=" << copy_bound * options.iterations
              << " replace_total_ns=" << elapsed_ns
              << " mean_ns=" << static_cast<double>(elapsed_ns) / static_cast<double>(options.iterations)
              << " p50_ns=" << samples[(samples.size() - 1) / 2]
              << " p95_ns=" << samples[(samples.size() * 95 + 99) / 100 - 1] << '\n';
    if (!options.baseline) {
        require(allocations == 0 && requested_bytes == 0 && store_reallocations == 0,
                "warmed equal-length replacement is not local");
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto config = options(argc, argv);
        std::cout << "glyph_replace_benchmark copy_accounting=source_upper_bound_not_runtime_counter"
                  << " allocation_accounting=operator_new_requested_bytes"
                  << " timing_scope=replace_only\n";
        for (std::size_t round = 1; round <= config.rounds; ++round) {
            for (const auto count : {1000U, 10000U, 100000U}) {
                for (const auto mode : {"external", "self", "overlap-left", "overlap-right"}) {
                    run(count, mode, round, config);
                }
            }
        }
    } catch (const std::exception& error) {
        allocation::end();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
