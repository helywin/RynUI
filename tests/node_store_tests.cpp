#include "runtime/node_store.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_parent_child_relationship_and_recursive_destroy() {
    ryn::runtime::NodeStore store;
    const auto root = store.create_root();
    const auto child = store.create_child(root);
    const auto grandchild = store.create_child(child);

    require(store.size() == 3, "NodeStore live count is incorrect");
    require(store.require(root).children == std::vector<ryn::runtime::NodeId>({child}),
            "root child relationship is incorrect");
    require(store.require(child).parent == root, "child parent relationship is incorrect");
    require(store.require(child).children == std::vector<ryn::runtime::NodeId>({grandchild}),
            "grandchild relationship is incorrect");

    require(store.destroy(child), "live child could not be destroyed");
    require(store.size() == 1, "recursive destroy did not remove the subtree");
    require(store.require(root).children.empty(), "destroy did not detach from parent");
    require(store.find(child) == nullptr, "destroyed child remained accessible");
    require(store.find(grandchild) == nullptr, "destroyed grandchild remained accessible");
}

void test_reused_slot_rejects_stale_handle() {
    ryn::runtime::NodeStore store;
    const auto stale = store.create_root();
    require(store.destroy(stale), "initial node destroy failed");

    const auto replacement = store.create_root();
    require(replacement.index == stale.index, "free slot was not reused");
    require(replacement.generation != stale.generation, "reused slot kept its generation");
    require(store.find(stale) == nullptr, "stale handle accessed a reused slot");
    require(store.find(replacement) != nullptr, "replacement handle is inaccessible");

    bool diagnosed = false;
    try {
        static_cast<void>(store.require(stale));
    } catch (const std::out_of_range&) {
        diagnosed = true;
    }
    require(diagnosed, "stale handle was not diagnosed");
}

void test_invalid_parent_is_rejected() {
    ryn::runtime::NodeStore store;
    bool diagnosed = false;
    try {
        static_cast<void>(store.create_child({7, 1}));
    } catch (const std::invalid_argument&) {
        diagnosed = true;
    }
    require(diagnosed, "invalid parent was not diagnosed");
    require(store.size() == 0, "invalid parent created a node");
}

} // namespace

int main() {
    try {
        test_parent_child_relationship_and_recursive_destroy();
        test_reused_slot_rejects_stale_handle();
        test_invalid_parent_is_rejected();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
