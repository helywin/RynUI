#include "gallery_document_viewport.hpp"

#include "runtime/frame_scheduler.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float left, float right) noexcept {
    return std::abs(left - right) < 0.001F;
}

constexpr std::array<float, 6> initial_anchors{
    0.0F, 100.0F, 250.0F, 500.0F, 900.0F, 1400.0F};

void test_empty_short_and_long_extent_clamping() {
    rynui::example::GalleryDocumentViewport viewport;
    require(viewport.set_extents(300.0F, 0.0F),
            "empty document extents did not update");
    require(!viewport.scroll_by(100.0F) && viewport.snapshot().offset == 0.0F,
            "empty document scrolled");
    viewport.set_extents(300.0F, 200.0F);
    require(!viewport.scroll_to(50.0F) && viewport.snapshot().maximum_offset == 0.0F,
            "short document escaped its zero offset");

    viewport.set_extents(300.0F, 1800.0F);
    require(viewport.scroll_to(9000.0F)
                && near(viewport.snapshot().offset, 1500.0F),
            "bottom overscroll was not clamped");
    require(viewport.scroll_by(-9000.0F)
                && viewport.snapshot().offset == 0.0F,
            "top overscroll was not clamped");

    bool rejected = false;
    try {
        viewport.scroll_by(std::numeric_limits<float>::quiet_NaN());
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "non-finite document scroll was accepted");
}

void test_anchor_jump_current_section_and_stale_generation() {
    using namespace rynui::example;
    GalleryDocumentViewport viewport;
    viewport.set_extents(300.0F, 1800.0F);
    require(viewport.replace_anchors(initial_anchors),
            "initial Gallery anchors were not installed");
    const auto overview = viewport.anchor(
        GalleryDocumentSectionKind::component_overview);
    require(overview.has_value() && viewport.jump_to(*overview)
                && near(viewport.snapshot().offset, 900.0F)
                && viewport.snapshot().current_section
                    == GalleryDocumentSectionKind::component_overview,
            "Gallery anchor jump or current section drifted");
    viewport.scroll_to(viewport.snapshot().maximum_offset - 16.0F);
    require(viewport.snapshot().current_section
                == GalleryDocumentSectionKind::live_samples,
            "Gallery bottom clamp never exposed the final current section");
    viewport.scroll_to(900.0F);
    const auto generation = viewport.snapshot().anchor_generation;
    require(!viewport.replace_anchors(initial_anchors)
                && viewport.snapshot().anchor_generation == generation,
            "identical anchors changed retained identity");

    constexpr std::array<float, 6> reflowed{
        0.0F, 120.0F, 300.0F, 620.0F, 1100.0F, 1700.0F};
    require(viewport.replace_anchors(reflowed),
            "reflowed Gallery anchors were not installed");
    require(!viewport.jump_to(*overview),
            "stale Gallery anchor generation was accepted");
    constexpr std::array<float, 7> categories{
        620.0F, 700.0F, 800.0F, 900.0F, 1200.0F, 1500.0F, 1650.0F};
    viewport.replace_category_anchors(categories);
    const auto feedback = viewport.category_anchor(
        AntDesignGalleryCategory::feedback);
    require(feedback.has_value() && viewport.jump_to(*feedback)
                && near(viewport.snapshot().offset, 1500.0F),
            "Gallery category anchor did not navigate independently");
}

void test_resize_anchor_restores_intra_section_distance() {
    using namespace rynui::example;
    GalleryDocumentViewport viewport;
    viewport.set_extents(300.0F, 1800.0F);
    viewport.replace_anchors(initial_anchors);
    viewport.scroll_to(950.0F);
    const auto captured = viewport.capture_resize_anchor();
    require(captured.section == GalleryDocumentSectionKind::component_overview
                && near(captured.distance, 50.0F),
            "resize capture lost current section distance");

    constexpr std::array<float, 6> reflowed{
        0.0F, 120.0F, 300.0F, 620.0F, 1100.0F, 1700.0F};
    viewport.replace_anchors(reflowed);
    viewport.set_extents(400.0F, 2300.0F);
    require(viewport.restore_resize_anchor(captured)
                && near(viewport.snapshot().offset, 1150.0F),
            "resize anchor did not restore intra-section position");
}

void test_subtree_translation_is_generation_checked_and_minimal() {
    rynui::example::GalleryDocumentViewport viewport;
    viewport.set_extents(200.0F, 1000.0F);
    viewport.scroll_to(300.0F);

    ryn::runtime::FrameRequestState frames;
    ryn::runtime::NodeStore nodes;
    ryn::runtime::DirtyQueues dirty(nodes, &frames);
    const auto root = nodes.create_root();
    const auto child = nodes.create_child(root);
    const auto grandchild = nodes.create_child(child);
    require(viewport.apply_subtree_translation(root, nodes, dirty),
            "live Gallery subtree was not translated");
    require(nodes.require(root).translation == ryn::runtime::Point{0.0F, -300.0F}
                && nodes.require(child).translation
                    == ryn::runtime::Point{0.0F, -300.0F}
                && nodes.require(grandchild).translation
                    == ryn::runtime::Point{0.0F, -300.0F}
                && dirty.transform_nodes().size() == 3
                && dirty.hit_test_nodes().size() == 3,
            "Gallery subtree translation did not synchronize geometry and HitTest");

    dirty.clear();
    require(viewport.apply_subtree_translation(root, nodes, dirty)
                && dirty.transform_nodes().empty()
                && dirty.hit_test_nodes().empty(),
            "unchanged Gallery translation dirtied the subtree");
    require(nodes.destroy(root), "Gallery subtree teardown failed");
    require(!viewport.apply_subtree_translation(root, nodes, dirty),
            "stale Gallery root generation was accepted");
}

} // namespace

int main() {
    try {
        test_empty_short_and_long_extent_clamping();
        test_anchor_jump_current_section_and_stale_generation();
        test_resize_anchor_restores_intra_section_distance();
        test_subtree_translation_is_generation_checked_and_minimal();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
