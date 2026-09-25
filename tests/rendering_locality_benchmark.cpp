#include "graphics/glyph_atlas.hpp"
#include "graphics/quad_primitive.hpp"
#include "runtime/invalidation.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

template <typename F>
std::int64_t micros(F&& action) {
    const auto start = Clock::now();
    action();
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start)
        .count();
}

void run(std::uint32_t count, std::uint32_t dirty_count) {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    std::vector<ryn::runtime::NodeId> ids;
    ids.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        ids.push_back(nodes.create_child(root));
    }
    ryn::runtime::DirtyQueues dirty(nodes);
    const auto queue_us = micros([&] {
        for (std::uint32_t index = 0; index < dirty_count; ++index) {
            dirty.invalidate(
                ids[static_cast<std::size_t>(index) * count / dirty_count],
                ryn::runtime::DirtyFlags::Material);
        }
    });
    const auto queued = dirty.material_nodes().size();

    ryn::graphics::QuadInstanceStore quads;
    std::vector<ryn::graphics::QuadInstance> initial(count);
    static_cast<void>(quads.append(initial));
    quads.clear_dirty_ranges();
    const std::array changed{ryn::graphics::QuadMaterial{{0.2F, 0.4F, 0.6F, 1.0F}, 0.75F}};
    const auto quad_collect_us = micros([&] {
        for (std::uint32_t index = 0; index < dirty_count; ++index) {
            const auto target = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(index) * count / dirty_count);
            static_cast<void>(quads.update_material({target, 1}, changed));
        }
    });
    std::size_t planned = 0;
    const auto quad_plan_us = micros([&] {
        planned = quads.material_dirty_ranges().size();
    });

    // Empty glyphs exercise the key lookup without page allocation or raster work.
    ryn::graphics::GlyphAtlas atlas;
    ryn::font::GlyphBitmap empty;
    empty.advance_x = 1.0F;
    const auto key = [](std::uint32_t glyph_id) {
        return ryn::graphics::GlyphAtlasKey{
            {0, 1}, glyph_id, 14, ryn::font::GlyphRasterPhase::zero,
            ryn::font::GlyphRasterMode::grayscale};
    };
    for (std::uint32_t index = 0; index < count; ++index) {
        if (!atlas.insert(key(index), empty)) {
            throw std::runtime_error("atlas setup failed");
        }
    }
    std::size_t hits = 0;
    const auto atlas_lookup_us = micros([&] {
        for (std::uint32_t index = 0; index < dirty_count; ++index) {
            const auto target = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(index) * count / dirty_count);
            const auto result = atlas.insert(key(target), empty);
            hits += result && result.cache_hit ? 1U : 0U;
        }
    });

    if (queued != dirty_count || hits != dirty_count || planned == 0) {
        throw std::runtime_error("locality benchmark correctness check failed");
    }
    std::cout << count << ',' << dirty_count << ',' << queued << ',' << planned << ','
              << atlas.entry_count() << ',' << queue_us << ',' << quad_collect_us << ','
              << quad_plan_us << ',' << atlas_lookup_us << '\n';
}

} // namespace

int main() {
    std::cout << "node_count,dirty_count,queued,quad_ranges,atlas_entries,"
                 "queue_cpu_us,quad_collect_cpu_us,quad_plan_cpu_us,atlas_lookup_cpu_us\n";
    for (const auto count : {1024U, 4096U, 16384U}) {
        run(count, 256);
        if (count / 4U != 256U) {
            run(count, count / 4U);
        }
    }
}
