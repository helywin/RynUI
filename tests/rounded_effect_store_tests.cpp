#include "graphics/rounded_effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ryn::graphics::RoundedEffectInstance effect(float x, ryn::Color color) {
    return ryn::graphics::make_shadow_effect(
        {{x, 10.0F, 10.0F, 10.0F}, 2.0F},
        {ryn::ShadowKind::outer, {}, 0.0F, 0.0F, color});
}

void test_sparse_material_updates_and_geometry_compaction() {
    ryn::graphics::RoundedEffectStore store;
    store.reserve(8);
    const std::array values{
        effect(10.0F, ryn::Color::rgba8(10, 20, 30)),
        effect(30.0F, ryn::Color::rgba8(40, 50, 60)),
        effect(50.0F, ryn::Color::rgba8(70, 80, 90)),
    };
    const auto ids = store.add_batch(values);
    require(store.compact({0.0F, 0.0F, 100.0F, 100.0F})
                && store.packed_instances().size() == 3,
            "rounded-effect store did not compact live instances");
    store.clear_dirty_ranges();

    auto first = store.at(ids[0]).material;
    first.opacity = 0.8F;
    auto third = store.at(ids[2]).material;
    third.opacity = 0.7F;
    require(store.update_material(ids[0], first)
                && store.update_material(ids[2], third)
                && store.material_dirty_ranges().size() == 2,
            "sparse rounded-effect material updates lost independent dirty ranges");
    auto second = store.at(ids[1]).material;
    second.opacity = 0.6F;
    require(store.update_material(ids[1], second)
                && store.material_dirty_ranges().size() == 1
                && store.material_dirty_ranges().front()
                    == ryn::graphics::RoundedEffectInstanceRange{0, 3},
            "adjacent rounded-effect dirty ranges did not merge");
    require(!store.update_material(ids[1], second),
            "equal rounded-effect material update requested work");

    auto geometry = store.at(ids[1]).geometry;
    geometry.translation = {4.0F, 5.0F};
    require(store.update_geometry(ids[1], geometry)
                && store.compact({0.0F, 0.0F, 100.0F, 100.0F})
                && store.geometry_dirty_ranges().size() == 1
                && store.geometry_dirty_ranges().front().count == 3,
            "geometry update did not rebuild the packed effect range");
    require(store.bytes({0, 3}).size()
                == sizeof(ryn::graphics::RoundedEffectInstance) * 3,
            "rounded-effect byte view does not match the independent instance store");
}

void test_culling_atomic_validation_and_identity_reuse() {
    ryn::graphics::RoundedEffectStore store;
    store.reserve(4);
    const auto visible = store.add(effect(10.0F, ryn::Color::rgba8(0, 0, 0, 80)));
    const auto outside = store.add(effect(500.0F, ryn::Color::rgba8(0, 0, 0, 80)));
    auto clipped_value = effect(20.0F, ryn::Color::rgba8(0, 0, 0, 80));
    clipped_value.geometry.ancestor_clip = ryn::graphics::EffectClip{
        9, {300.0F, 300.0F, 10.0F, 10.0F}};
    const auto clipped = store.add(clipped_value);
    auto hidden_value = effect(30.0F, ryn::Color::rgba8(0, 0, 0, 80));
    hidden_value.material.visible = false;
    const auto hidden = store.add(hidden_value);

    require(store.compact({0.0F, 0.0F, 100.0F, 100.0F})
                && store.packed_instances().size() == 1
                && store.packed_index(visible) == std::uint32_t{0}
                && !store.packed_index(outside).has_value()
                && !store.packed_index(clipped).has_value()
                && !store.packed_index(hidden).has_value()
                && store.diagnostics().culled_instances == 3,
            "visibility, window, or ancestor-clip culling is incorrect");

    const auto prior = store.at(visible).material;
    auto invalid = prior;
    invalid.opacity = 2.0F;
    try {
        static_cast<void>(store.update_material(visible, invalid));
        throw std::runtime_error("invalid material update was accepted");
    } catch (const std::invalid_argument&) {
    }
    require(store.at(visible).material == prior,
            "invalid rounded-effect update partially mutated the store");

    require(store.remove(visible) && !store.contains(visible),
            "rounded-effect removal left the old identity live");
    const auto reused = store.add(effect(12.0F, ryn::Color::rgba8(1, 2, 3, 80)));
    require(reused.index == visible.index && reused.generation != visible.generation
                && store.contains(reused) && !store.contains(visible)
                && store.diagnostics().slot_reuses == 1,
            "rounded-effect slot reuse did not advance stable identity generation");
}

void test_capacity_reuse_and_idle_compaction() {
    ryn::graphics::RoundedEffectStore store;
    store.reserve(16);
    const auto capacity = store.slot_capacity();
    const auto id = store.add(effect(10.0F, ryn::Color::rgba8(0, 0, 0, 80)));
    const ryn::runtime::Rect clip{0.0F, 0.0F, 100.0F, 100.0F};
    require(store.compact(clip) && !store.compact(clip)
                && store.diagnostics().idle_compactions == 1,
            "idle rounded-effect compaction was not suppressed");
    require(store.remove(id), "rounded-effect capacity test could not remove identity");
    const auto reused = store.add(effect(20.0F, ryn::Color::rgba8(0, 0, 0, 80)));
    require(reused.index == id.index && store.slot_capacity() == capacity,
            "rounded-effect capacity was not reused after removal");
}

} // namespace

int main() {
    try {
        test_sparse_material_updates_and_geometry_compaction();
        test_culling_atomic_validation_and_identity_reuse();
        test_capacity_reuse_and_idle_compaction();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
