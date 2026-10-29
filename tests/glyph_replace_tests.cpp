#include "graphics/glyph_scene.hpp"
#include "support/allocation_probe.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
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

GlyphInstance glyph(float marker) {
    GlyphInstance result;
    result.position_size = {marker, marker + 1, 12, 14};
    result.uv_rect = {0.1F, 0.2F, 0.3F, 0.4F};
    result.clip_bounds = {marker, 0, marker + 20, 30};
    result.color = {0.2F, 0.4F, 0.6F, 0.8F};
    result.translation_opacity = {marker + 2, marker + 3, 0.75F, 0};
    result.transform = {{marker + 4, marker + 5}, 15};
    return result;
}

std::vector<GlyphInstance> background(std::uint32_t count) {
    std::vector<GlyphInstance> result;
    result.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        result.push_back(glyph(static_cast<float>(index)));
    }
    return result;
}

void test_external_replacement_is_local() {
    const std::array replacement{glyph(-4), glyph(-3), glyph(-2), glyph(-1)};
    for (const auto count : {1000U, 10000U, 100000U}) {
        auto expected = background(count);
        GlyphInstanceStore store;
        static_cast<void>(store.append(expected));
        const auto* retained = store.instances().data();
        const GlyphInstanceRange target{count / 2, 4};

        allocation::begin();
        const auto replaced = store.replace(target, replacement);
        const auto allocations = allocation::end();
        const auto bytes = allocation::requested_bytes;
        const auto largest = allocation::largest_request;
        std::copy(replacement.begin(), replacement.end(), expected.begin() + target.first);
        require(replaced == target && store.instances().data() == retained && store.size() == count,
                "four-glyph replacement rebuilt the retained store or changed its range");
        require(allocations <= 1 && bytes <= replacement.size() * sizeof(GlyphInstance) &&
                    largest < count * sizeof(GlyphInstance),
                "four-glyph replacement allocated background-sized storage");
        require(std::ranges::equal(store.instances(), expected), "four-glyph replacement changed a neighbor");
        require(std::ranges::equal(store.geometry_dirty_ranges(), std::array{target}) &&
                    store.material_dirty_ranges().empty(),
                "four-glyph replacement expanded its dirty range");

        store.clear_dirty_ranges();
        allocation::begin(0);
        static_cast<void>(store.replace(target, replacement));
        const auto warm_allocations = allocation::end();
        require(warm_allocations == 0 && allocation::requested_bytes == 0 && store.instances().data() == retained &&
                    std::ranges::equal(store.instances(), expected),
                "warmed equal-length replacement allocated or changed retained neighbors");
    }
}

void test_self_and_overlapping_sources() {
    constexpr GlyphInstanceRange target{6, 4};
    // Identical, overlapping in both directions, and disjoint aliases in both directions.
    for (const auto source_first : {6U, 5U, 7U, 0U, 12U}) {
        auto expected = background(16);
        GlyphInstanceStore store;
        static_cast<void>(store.append(expected));
        const auto* retained = store.instances().data();
        const auto source = store.instances().subspan(source_first, target.count);
        const std::vector<GlyphInstance> saved_source(source.begin(), source.end());
        std::copy(saved_source.begin(), saved_source.end(), expected.begin() + target.first);
        require(store.replace(target, source) == target && store.instances().data() == retained &&
                    std::ranges::equal(store.instances(), expected),
                "aliased equal-length replacement did not preserve the original source and neighbors");
        require(std::ranges::equal(store.geometry_dirty_ranges(), std::array{target}),
                "aliased equal-length replacement dirtied the wrong range");
    }
}

void test_pending_dirty_neighbors_survive() {
    GlyphInstanceStore store;
    static_cast<void>(store.append(background(16)));
    const std::array material{GlyphInstanceRange{1, 2}, GlyphInstanceRange{6, 1}, GlyphInstanceRange{14, 2}};
    for (const auto range : material) {
        static_cast<void>(store.update_material(range, {1, 0, 0, 1}, 0.5F));
    }
    for (const auto range :
         {GlyphInstanceRange{15, 1}, GlyphInstanceRange{5, 1}, GlyphInstanceRange{0, 1}, GlyphInstanceRange{11, 1}}) {
        static_cast<void>(store.update_geometry(range, {10, 20, 30, 40}, {1, 2}));
    }
    const auto* retained = store.instances().data();
    std::vector<GlyphInstance> expected(store.instances().begin(), store.instances().end());
    const std::array replacement{glyph(-4), glyph(-3), glyph(-2), glyph(-1)};
    std::copy(replacement.begin(), replacement.end(), expected.begin() + 6);
    require(store.replace({6, 4}, replacement) == GlyphInstanceRange{6, 4} && store.instances().data() == retained &&
                std::ranges::equal(store.instances(), expected),
            "replacement changed dirty neighbors' retained data");
    require(std::ranges::equal(store.material_dirty_ranges(), material) &&
                std::ranges::equal(store.geometry_dirty_ranges(),
                                   std::array{GlyphInstanceRange{0, 1}, GlyphInstanceRange{5, 5},
                                              GlyphInstanceRange{11, 1}, GlyphInstanceRange{15, 1}}),
            "replacement dropped pending material/geometry work or dirtied an untouched gap");
}

void test_empty_and_invalid_ranges_are_atomic() {
    GlyphInstanceStore empty;
    allocation::begin(0);
    const auto empty_result = empty.replace({}, {});
    const auto empty_allocations = allocation::end();
    require(empty_result == GlyphInstanceRange{} && empty_allocations == 0 && empty.size() == 0 &&
                empty.geometry_dirty_ranges().empty(),
            "empty-store replacement performed work");

    GlyphInstanceStore store;
    static_cast<void>(store.append(background(16)));
    static_cast<void>(store.update_material({1, 2}, {1, 0, 0, 1}, 0.5F));
    static_cast<void>(store.update_geometry({12, 1}, {0, 0, 20, 20}, {1, 2}));
    const auto* retained = store.instances().data();
    const std::vector<GlyphInstance> expected(store.instances().begin(), store.instances().end());
    const auto unchanged = [&] {
        require(store.instances().data() == retained && std::ranges::equal(store.instances(), expected) &&
                    std::ranges::equal(store.material_dirty_ranges(), std::array{GlyphInstanceRange{1, 2}}) &&
                    std::ranges::equal(store.geometry_dirty_ranges(), std::array{GlyphInstanceRange{12, 1}}),
                "empty or invalid replacement changed retained data or pending dirties");
    };
    for (const auto first : {0U, 8U, 16U}) {
        allocation::begin(0);
        const auto result = store.replace({first, 0}, {});
        const auto allocations = allocation::end();
        require(result == GlyphInstanceRange{first, 0} && allocations == 0,
                "empty replacement allocated or changed its insertion point");
        unchanged();
    }
    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    for (const auto range :
         {GlyphInstanceRange{17, 0}, GlyphInstanceRange{16, 1}, GlyphInstanceRange{14, 4},
          GlyphInstanceRange{maximum, 0}, GlyphInstanceRange{maximum, 4}, GlyphInstanceRange{0, maximum}}) {
        const std::array source{glyph(-4), glyph(-3), glyph(-2), glyph(-1)};
        for (const bool empty_input : {false, true}) {
            bool rejected = false;
            const auto input = empty_input ? std::span<const GlyphInstance>{} : std::span<const GlyphInstance>{source};
            try {
                static_cast<void>(store.replace(range, input));
            } catch (const std::out_of_range&) {
                rejected = true;
            }
            require(rejected, "out-of-bounds replacement was accepted");
            unchanged();
        }
    }
}

void test_dirty_preparation_failure_preserves_state_and_retry() {
    constexpr GlyphInstanceRange target{256, 4};
    const std::array external{glyph(-4), glyph(-3), glyph(-2), glyph(-1)};
    // -1 is external input; the others are identical/left/right overlapping store spans.
    for (const auto source_first : {-1, 256, 255, 257}) {
        for (const bool existing_geometry : {false, true}) {
            bool exercised_failure = false;
            // Find a real dirty-vector growth boundary without assuming a vendor's capacity policy.
            for (std::uint32_t dirty_count = existing_geometry ? 1U : 0U; dirty_count < 64; ++dirty_count) {
                GlyphInstanceStore store;
                static_cast<void>(store.append(background(512)));
                static_cast<void>(store.update_material({480, 2}, {1, 0, 0, 1}, 0.5F));
                for (std::uint32_t index = 0; index < dirty_count; ++index) {
                    static_cast<void>(store.update_geometry({index * 2, 1}, {10, 20, 30, 40}, {1, 2}));
                }
                const auto geometry = store.geometry_dirty_ranges();
                const std::vector<GlyphInstanceRange> saved_geometry(geometry.begin(), geometry.end());
                const auto* retained = store.instances().data();
                std::vector<GlyphInstance> expected(store.instances().begin(), store.instances().end());
                const auto source =
                    source_first < 0 ? std::span<const GlyphInstance>{external}
                                     : store.instances().subspan(static_cast<std::size_t>(source_first), target.count);
                const std::vector<GlyphInstance> saved_source(source.begin(), source.end());
                bool failed = false;
                allocation::begin(0);
                try {
                    static_cast<void>(store.replace(target, source));
                } catch (const std::bad_alloc&) {
                    failed = true;
                }
                const auto attempts = allocation::end();
                if (!failed) {
                    require(existing_geometry, "cold dirty preparation did not reach the allocation failpoint");
                    continue;
                }
                require(attempts == 1 && store.instances().data() == retained &&
                            std::ranges::equal(store.instances(), expected) &&
                            std::ranges::equal(store.geometry_dirty_ranges(), saved_geometry) &&
                            std::ranges::equal(store.material_dirty_ranges(), std::array{GlyphInstanceRange{480, 2}}),
                        "dirty preparation failure left partial data or changed pending dirties");
                // The failpoint must be metadata preparation, not an attempted full-store replacement.
                require(allocation::largest_request <= (dirty_count + 1) * sizeof(GlyphInstanceRange),
                        "failure was injected into a full-store allocation instead of dirty preparation");
                std::copy(saved_source.begin(), saved_source.end(), expected.begin() + target.first);
                auto expected_geometry = saved_geometry;
                expected_geometry.push_back(target);
                require(store.replace(target, source) == target && store.instances().data() == retained &&
                            std::ranges::equal(store.instances(), expected) &&
                            std::ranges::equal(store.geometry_dirty_ranges(), expected_geometry) &&
                            std::ranges::equal(store.material_dirty_ranges(), std::array{GlyphInstanceRange{480, 2}}),
                        "equal-length replacement did not recover after dirty preparation failed");
                exercised_failure = true;
                break;
            }
            require(exercised_failure, "no real dirty allocation failure was exercised");
        }
    }
}

} // namespace

int main() {
    try {
        test_external_replacement_is_local();
        test_self_and_overlapping_sources();
        test_pending_dirty_neighbors_survive();
        test_empty_and_invalid_ranges_are_atomic();
        test_dirty_preparation_failure_preserves_state_and_retry();
    } catch (const std::exception& error) {
        allocation::end();
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Glyph equal-length replacement contracts passed\n";
}
