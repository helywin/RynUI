#include "runtime/component_host.hpp"

#include <ryn/component.hpp>

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

struct TestState final {};
struct ChildrenSlot final {};
using Children = ryn::SlotContent<ChildrenSlot>;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_depth_first_fragment_order_destroy_and_generation_reuse() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId root;
    ryn::runtime::ComponentId first_child;
    ryn::runtime::ComponentId second_child;
    ryn::runtime::SceneFragmentId root_before_first;
    ryn::runtime::SceneFragmentId root_before_second;
    ryn::runtime::SceneFragmentId root_after;
    ryn::runtime::SceneFragmentId first_before;
    ryn::runtime::SceneFragmentId first_after;
    ryn::runtime::SceneFragmentId second_before;
    ryn::runtime::SceneFragmentId stale;
    ryn::runtime::SceneFragmentId replacement;

    components.mount(ryn::Content{[&] {
        auto& build = ryn::runtime::require_component_build_context();
        root = build.mount_component<TestState>();
        root_before_first = build.register_scene_fragment(
            root, ryn::runtime::SceneFragmentPlacement::before_children);
        stale = build.register_scene_fragment(
            root, ryn::runtime::SceneFragmentPlacement::before_children);
        require(components.remove_scene_fragment(stale),
                "live fragment could not be removed during mount");
        replacement = build.register_scene_fragment(
            root, ryn::runtime::SceneFragmentPlacement::before_children);
        root_before_second = replacement;
        root_after = build.register_scene_fragment(
            root, ryn::runtime::SceneFragmentPlacement::after_children);
        build.mount_slot(root, Children{[&] {
            auto& child_build = ryn::runtime::require_component_build_context();
            first_child = child_build.mount_component<TestState>();
            first_before = child_build.register_scene_fragment(
                first_child,
                ryn::runtime::SceneFragmentPlacement::before_children);
            first_after = child_build.register_scene_fragment(
                first_child,
                ryn::runtime::SceneFragmentPlacement::after_children);
            second_child = child_build.mount_component<TestState>();
            second_before = child_build.register_scene_fragment(
                second_child,
                ryn::runtime::SceneFragmentPlacement::before_children);
        }});
    }});

    require(stale.index == replacement.index
                && stale.generation != replacement.generation
                && !components.contains(stale)
                && components.contains(replacement),
            "fragment slot reuse did not advance generation");
    const std::vector expected{
        root_before_first,
        root_before_second,
        first_before,
        first_after,
        second_before,
        root_after,
    };
    std::vector<ryn::runtime::SceneFragmentId> actual;
    for (const auto& entry : components.paint_traversal()) {
        actual.push_back(entry.fragment);
    }
    require(actual == expected,
            "Component fragment traversal is not stable depth-first order");

    std::atomic<bool> wrong_thread_rejected{false};
    std::thread worker([&] {
        try {
            static_cast<void>(components.paint_traversal());
        } catch (const std::logic_error&) {
            wrong_thread_rejected.store(true, std::memory_order_relaxed);
        }
    });
    worker.join();
    require(wrong_thread_rejected.load(std::memory_order_relaxed),
            "wrong-thread component paint traversal was not rejected");

    require(components.destroy(first_child), "child component destroy failed");
    require(!components.contains(first_before)
                && !components.contains(first_after),
            "destroyed component retained scene fragments");
    actual.clear();
    for (const auto& entry : components.paint_traversal()) {
        actual.push_back(entry.fragment);
    }
    require(actual == std::vector<ryn::runtime::SceneFragmentId>({
                root_before_first,
                root_before_second,
                second_before,
                root_after,
            }),
            "destroyed subtree remained in paint traversal");

    require(components.destroy(root), "root component destroy failed");
    require(components.paint_traversal().empty()
                && !components.contains(root_before_first)
                && !components.contains(root_after)
                && !components.contains(second_before),
            "root destroy retained traversal fragments");
}

} // namespace

int main() {
    try {
        test_depth_first_fragment_order_destroy_and_generation_reuse();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
