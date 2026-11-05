#include "runtime/frame_scheduler.hpp"
#include "runtime/invalidation.hpp"
#include "runtime/node_store.hpp"
#include "../examples/token_gallery/gallery_scroll_region.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn::runtime;
using rynui::example::GalleryScrollTranslation;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void ownership_preserves_local_offsets() {
    NodeStore nodes;
    FrameRequestState frames;
    DirtyQueues dirty{nodes, &frames};
    NodePropertyWriter writer{nodes, dirty};
    const auto root = nodes.create_root();
    const auto sibling = nodes.create_child(root);
    const auto owner = nodes.create_child(root);
    const auto content = nodes.create_child(owner);
    const auto inner = nodes.create_child(content);
    const auto leaf = nodes.create_child(inner);
    nodes.require(owner).owns_child_translation = true;
    nodes.require(inner).owns_child_translation = true;
    nodes.require(owner).clip_content = true;
    nodes.require(owner).bounds = {10, 20, 100, 80};
    nodes.require(owner).content_opacity = .5F;
    nodes.require(content).content_opacity = .8F;
    writer.set_translation(content, {0, -40});
    writer.set_translation(inner, {0, -40});
    writer.set_translation(leaf, {0, -51});
    GalleryScrollTranslation outer;
    const auto moved = outer.apply(root, 75, nodes, dirty);
    check(moved.changed && moved.translated_nodes == 3, "ancestor walker entered owned descendants");
    check(nodes.require(root).translation == Point{0, -75} && nodes.require(sibling).translation == Point{0, -75} &&
              nodes.require(owner).translation == Point{0, -75},
          "owned root lost inherited parent translation");
    check(nodes.require(content).translation == Point{0, -40} && nodes.require(leaf).translation == Point{0, -51},
          "parent overwrote local/inner offsets");
    // Owned containers combine the received absolute offset with their local
    // scroll in parent-before-child order, in the same normal synchronization.
    const auto compose = [&] {
        const auto inherited = nodes.require(owner).translation;
        writer.set_translation(content, {inherited.x, inherited.y - 40});
        writer.set_translation(inner, {inherited.x, inherited.y - 40});
        const auto nested = nodes.require(inner).translation;
        writer.set_translation(leaf, {nested.x, nested.y - 11});
    };
    compose();
    check(nodes.require(leaf).translation == Point{0, -126}, "nested owners did not compose inherited/local offsets");
    const auto clip = nodes.content_clip(content, {0, 0, 200, 200});
    check(clip == Rect{10, 0, 100, 25} && !nodes.require(owner).clip_boundary,
          "translation ownership escaped ancestor clipping");
    check(nodes.content_opacity(content) == .4F, "translation ownership changed opacity ancestry");
    static_cast<void>(frames.consume_request());
    const auto revision = frames.revision();
    const auto transforms = dirty.transform_nodes();
    for (int i = 0; i < 20; ++i) {
        check(!outer.apply(root, 75, nodes, dirty).changed, "same inherited translation repeated traversal");
        compose();
    }
    check(!frames.pending() && frames.revision() == revision && dirty.transform_nodes() == transforms,
          "unchanged ownership composition requested more work");
    check(outer.apply(root, 0, nodes, dirty).changed, "parent return to zero was lost");
    compose();
    check(nodes.require(owner).translation == Point{} && nodes.require(content).translation == Point{0, -40} &&
              nodes.require(leaf).translation == Point{0, -51},
          "return-to-zero reset local scroll state");
    check(nodes.destroy(owner), "owned subtree destroy");
    const auto reused = nodes.create_child(root);
    check(reused.index == owner.index && !nodes.require(reused).owns_child_translation &&
              !nodes.require(reused).clip_boundary,
          "Node slot reused stale ownership");
    const auto reused_child = nodes.create_child(reused);
    check(outer.apply(root, 12, nodes, dirty).changed && nodes.require(reused_child).translation == Point{0, -12},
          "ordinary reused slot still blocked inherited translation");
}

void failed_owner_setup_cleans_the_marker() {
    NodeStore nodes;
    const auto root = nodes.create_root();
    NodeId failed;
    try {
        failed = nodes.create_child(root);

        struct Cleanup {
            NodeStore& nodes;
            NodeId id;

            ~Cleanup() {
                nodes.destroy(id);
            }
        } cleanup{nodes, failed};

        nodes.require(failed).owns_child_translation = true;
        static_cast<void>(nodes.create_child(failed));
        throw std::runtime_error("owner factory");
    } catch (const std::runtime_error&) {
    }
    check(nodes.size() == 1 && !nodes.find(failed), "failed owner setup left owned nodes");
    const auto reused = nodes.create_child(root);
    check(reused.index == failed.index && !nodes.require(reused).owns_child_translation,
          "failed owner leaked boundary into next factory");
}
} // namespace

int main() {
    try {
        ownership_preserves_local_offsets();
        failed_owner_setup_cleans_the_marker();
        std::cout << "Translation ownership primitive contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
