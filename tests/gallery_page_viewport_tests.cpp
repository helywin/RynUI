#include "gallery_page_layout.hpp"
#include "gallery_page_viewport.hpp"
#include "runtime/frame_scheduler.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace rynui::example;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void test_responsive_lanes() {
    for (const float width : {1440, 960, 728, 480, 360}) {
        const auto m = gallery_page_layout({width, 900});
        require(m.document_width >= 280 && m.document_width <= 920 && m.content.x >= 0 &&
                    m.content.x + m.content.width <= width - 8 && m.content.y >= m.header_height &&
                    m.document.height > 0 && m.document_track.x >= m.content.x + m.content.width,
                "page content overlaps header/scrollbar or escapes the viewport");
        require(m.narrow == (width < 640) && m.wide == (width >= 1200), "page breakpoint changed");
        if (!m.narrow) {
            require(m.navigation_width >= 184 && m.navigation_width <= 240 &&
                        m.navigation.x + m.navigation.width <= m.document.x,
                    "independent navigation lane overlaps the document");
        }
        if (m.wide) {
            require(m.toc.width == 160 && m.toc.x >= m.document.x + m.document.width && m.toc.x + m.toc.width <= width,
                    "wide TOC is outside its own lane");
        }
    }
}

void test_named_generation_and_resize() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto usage = nodes.create_child(root);
    const auto demo = nodes.create_child(root);
    const auto popup = nodes.create_child(root);
    const auto popup_child = nodes.create_child(popup);
    nodes.require(popup).clip_boundary = true;
    nodes.require(root).bounds = {100, 80, 600, 2000};
    nodes.require(usage).bounds = {100, 80, 600, 80};
    nodes.require(demo).bounds = {100, 680, 600, 800};
    const std::array anchors{GalleryPageAnchorNode{"usage", usage}, GalleryPageAnchorNode{"sample.button", demo}};
    GalleryPageViewport viewport;
    require(viewport.set_page(1, root, anchors) && viewport.refresh(600, nodes), "page anchors were not registered");
    const auto handle = viewport.anchor("sample.button");
    require(handle && viewport.jump(*handle) && viewport.snapshot().offset == 600, "named sample jump failed");
    viewport.scroll_by(37.5F);
    const auto saved = viewport.capture();
    require(saved.identity == "sample.button" && saved.distance == 37.5F, "anchor distance was not captured");
    nodes.require(demo).bounds.y += 120;
    viewport.refresh(600, nodes);
    viewport.restore(saved);
    require(viewport.snapshot().offset == 757.5F && viewport.generation() == handle->generation,
            "resize lost local distance or invalidated a live page handle");
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::DirtyQueues dirty{nodes, &frames};
    const auto first = viewport.apply(nodes, dirty);
    require(first.changed && nodes.require(demo).translation.y == -757.5F && nodes.require(popup).translation.y == 0 &&
                nodes.require(popup_child).translation.y == 0,
            "page translation missed its sample or moved a window layer");
    require(!viewport.apply(nodes, dirty).changed && viewport.work().translation_passes == 1,
            "unchanged offset translated the page again");
    const auto appended = nodes.create_child(root);
    require(viewport.apply(nodes, dirty).changed && nodes.require(appended).translation.y == -757.5F,
            "a newly appended current-page node missed the unchanged scroll offset");
    require(viewport.set_page(2, root, anchors) && !viewport.jump(*handle) && viewport.snapshot().offset == 0,
            "retired page anchor remained usable");
    viewport.refresh(600, nodes);
    viewport.scroll_to(10000);
    require(viewport.snapshot().offset == 1400, "page bottom did not clamp");
    nodes.require(root).bounds.height = 300;
    viewport.refresh(600, nodes);
    require(viewport.snapshot().offset == 0, "short page retained an out-of-range offset");
    const auto foreign = nodes.create_root();
    const std::array invalid{GalleryPageAnchorNode{"foreign", foreign}};
    viewport.set_page(3, root, invalid);
    bool rejected{};
    try {
        viewport.refresh(600, nodes);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "foreign page geometry was accepted as a current anchor");
    rejected = false;
    try {
        viewport.set_page(2, root, anchors);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && viewport.generation() == 3, "an old generation replaced current ownership");
}
} // namespace

int main() {
    try {
        test_responsive_lanes();
        test_named_generation_and_resize();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
