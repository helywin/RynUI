#include "gallery_document_viewport.hpp"
#include "gallery_scroll_region.hpp"

#include "runtime/frame_scheduler.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

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

void test_component_identity_anchors_follow_reflow_and_reject_stale_ids() {
    using namespace rynui::example;
    GalleryDocumentViewport viewport;
    viewport.set_extents(300.0F, 5000.0F);
    std::array<std::string, 73> names;
    std::array<GalleryComponentAnchor, 73> anchors;
    for (std::size_t index = 0; index < anchors.size(); ++index) {
        names[index] = "ant.component.fixture-" + std::to_string(index);
        anchors[index] = {names[index], 1000.0F + 40.0F * static_cast<float>(index)};
    }
    require(viewport.replace_component_anchors(anchors),
        "component anchors were not registered");
    const auto target = viewport.component_anchor(names[10]);
    require(target && viewport.jump_to(*target)
        && near(viewport.snapshot().offset, 1400.0F),
        "component identity did not jump to its own card");
    const auto generation = viewport.snapshot().anchor_generation;
    require(!viewport.replace_component_anchors(anchors)
        && viewport.snapshot().anchor_generation == generation,
        "identical component anchors changed generation");
    anchors[10].offset = 1600.0F;
    require(viewport.replace_component_anchors(anchors)
        && !viewport.jump_to(*target),
        "reflowed component anchor accepted a stale generation");
    const auto reflowed = viewport.component_anchor(names[10]);
    require(reflowed && viewport.jump_to(*reflowed)
        && near(viewport.snapshot().offset, 1600.0F)
        && !viewport.component_anchor("missing"),
        "component identity did not survive reflow");
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

void test_two_scroll_ranges_and_translations_remain_independent() {
    using namespace rynui::example;
    GalleryScrollRange navigation;
    GalleryScrollRange document;
    navigation.set_extents(300.0F, 900.0F);
    document.set_extents(300.0F, 1800.0F);
    require(document.scroll_by(360.0F)
                && navigation.snapshot().offset == 0.0F,
            "document scroll changed navigation offset");
    require(navigation.scroll_to(450.0F)
                && near(document.snapshot().offset, 360.0F),
            "navigation scroll changed document offset");

    ryn::runtime::FrameRequestState frames;
    ryn::runtime::NodeStore nodes;
    ryn::runtime::DirtyQueues dirty(nodes, &frames);
    const auto root = nodes.create_root();
    const auto nav_root = nodes.create_child(root);
    const auto nav_child = nodes.create_child(nav_root);
    const auto doc_root = nodes.create_child(root);
    const auto doc_child = nodes.create_child(doc_root);
    GalleryScrollTranslation nav_translation;
    GalleryScrollTranslation doc_translation;
    const auto doc_result = doc_translation.apply(
        doc_root, document.snapshot().offset, nodes, dirty);
    require(doc_result.valid && doc_result.changed
                && doc_result.translated_nodes == 2
                && nodes.require(nav_root).translation == ryn::runtime::Point{}
                && nodes.require(nav_child).translation == ryn::runtime::Point{}
                && nodes.require(doc_root).translation
                    == ryn::runtime::Point{0.0F, -360.0F}
                && nodes.require(doc_child).translation
                    == ryn::runtime::Point{0.0F, -360.0F},
            "document translation changed the navigation subtree");
    const auto nav_result = nav_translation.apply(
        nav_root, navigation.snapshot().offset, nodes, dirty);
    require(nav_result.valid && nav_result.changed
                && nav_result.translated_nodes == 2
                && nodes.require(nav_root).translation
                    == ryn::runtime::Point{0.0F, -450.0F}
                && nodes.require(doc_root).translation
                    == ryn::runtime::Point{0.0F, -360.0F},
            "navigation translation changed the document subtree");
    dirty.clear();
    require(!doc_translation.apply(
                 doc_root, document.snapshot().offset, nodes, dirty).changed
                && dirty.transform_nodes().empty(),
            "unchanged document region dirtied the scene");
    require(nodes.destroy(nav_root)
                && !nav_translation.apply(
                    nav_root, navigation.snapshot().offset, nodes, dirty).valid,
            "stale navigation root generation was accepted");
}

void test_scrollbar_geometry_and_pointer_mapping() {
    using namespace rynui::example;
    GalleryScrollRange range;
    range.set_extents(200.0F, 1000.0F);
    range.scroll_to(400.0F);
    const ryn::runtime::Rect track{220.0F, 80.0F, 8.0F, 200.0F};
    auto geometry = gallery_scrollbar_geometry(track, range.snapshot());
    require(near(geometry.thumb.height, 40.0F)
                && near(geometry.thumb.y, 160.0F)
                && near(gallery_scrollbar_offset_for_thumb_top(
                    geometry, 240.0F), 800.0F),
            "scrollbar thumb did not reflect viewport and offset");

    GalleryScrollbarController controller;
    auto pointer = ryn::input::PointerInputEvent{
        ryn::input::PointerIdentity::mouse(),
        ryn::input::PointerAction::down,
        ryn::input::PointerButton::primary,
        224.0F, 170.0F};
    const auto start = controller.dispatch(pointer, geometry, 200.0F, 400.0F);
    require(start.consumed && !start.requested_offset.has_value()
                && controller.dragging(),
            "scrollbar thumb did not start dragging");
    pointer.action = ryn::input::PointerAction::move;
    pointer.button = ryn::input::PointerButton::none;
    pointer.y = 250.0F;
    const auto moved = controller.dispatch(pointer, geometry, 200.0F, 400.0F);
    require(moved.consumed && moved.requested_offset.has_value()
                && near(*moved.requested_offset, 800.0F),
            "scrollbar drag did not map to content offset");
    pointer.action = ryn::input::PointerAction::up;
    require(controller.dispatch(pointer, geometry, 200.0F, 800.0F).consumed
                && !controller.dragging(),
            "scrollbar drag was not released");

    pointer.action = ryn::input::PointerAction::down;
    pointer.button = ryn::input::PointerButton::primary;
    pointer.y = 270.0F;
    const auto page = controller.dispatch(pointer, geometry, 200.0F, 400.0F);
    require(page.consumed && page.requested_offset.has_value()
                && near(*page.requested_offset, 580.0F),
            "scrollbar track click did not page forward");

    range.set_extents(200.0F, 100.0F);
    geometry = gallery_scrollbar_geometry(track, range.snapshot());
    require(geometry.thumb == track && geometry.maximum_offset == 0.0F,
            "short content left a scrollable thumb");
    range.set_extents(200.0F, 10000.0F);
    geometry = gallery_scrollbar_geometry(track, range.snapshot());
    require(near(geometry.thumb.height, 28.0F),
            "long content thumb violated minimum grab size");
}

void test_scrollbar_placement_and_column_routing() {
    using namespace rynui::example;
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::NodeStore nodes;
    ryn::runtime::DirtyQueues dirty(nodes, &frames);
    const auto bar = nodes.create_root();
    nodes.require(bar).bounds = {24.0F, 5000.0F, 8.0F, 200.0F};
    const ryn::runtime::Rect target{234.0F, 88.0F, 8.0F, 200.0F};
    require(gallery_place_scrollbar(bar, target, nodes, dirty)
                && nodes.require(bar).translation
                    == ryn::runtime::Point{210.0F, -4912.0F}
                && dirty.transform_nodes().size() == 1,
            "scrollbar overlay was not translated to its visible lane");
    dirty.clear();
    require(!gallery_place_scrollbar(bar, target, nodes, dirty)
                && dirty.transform_nodes().empty(),
            "unchanged scrollbar placement dirtied the scene");

    const ryn::runtime::Rect navigation{24.0F, 88.0F, 220.0F, 600.0F};
    const ryn::runtime::Rect document{260.0F, 88.0F, 916.0F, 600.0F};
    require(gallery_scroll_target(100.0F, 200.0F, false,
                navigation, document) == GalleryScrollTarget::navigation
                && gallery_scroll_target(500.0F, 200.0F, false,
                    navigation, document) == GalleryScrollTarget::document
                && gallery_scroll_target(250.0F, 200.0F, false,
                    navigation, document) == GalleryScrollTarget::none
                && gallery_scroll_target(100.0F, 40.0F, false,
                    navigation, document) == GalleryScrollTarget::none
                && gallery_scroll_target(100.0F, 200.0F, true,
                    navigation, document) == GalleryScrollTarget::none,
            "Gallery wheel routing crossed columns, header, or narrow mode");
    require(nodes.destroy(bar)
                && !gallery_place_scrollbar(bar, target, nodes, dirty),
            "stale scrollbar node was accepted");
}

} // namespace

int main() {
    try {
        test_empty_short_and_long_extent_clamping();
        test_anchor_jump_current_section_and_stale_generation();
        test_component_identity_anchors_follow_reflow_and_reject_stale_ids();
        test_resize_anchor_restores_intra_section_distance();
        test_subtree_translation_is_generation_checked_and_minimal();
        test_two_scroll_ranges_and_translations_remain_independent();
        test_scrollbar_geometry_and_pointer_mapping();
        test_scrollbar_placement_and_column_routing();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
