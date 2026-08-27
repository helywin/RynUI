#include "input/interaction_registry.hpp"

#include <ryn/component.hpp>

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

struct TestState final {};
struct ChildrenSlot final {};
using Children = ryn::SlotContent<ChildrenSlot>;

ryn::runtime::ComponentId mount_leaf() {
    return ryn::runtime::require_component_build_context()
        .mount_component<TestState>();
}

ryn::runtime::ComponentId mount_parent(const Children& children) {
    auto& context = ryn::runtime::require_component_build_context();
    const auto parent = context.mount_component<TestState>();
    context.mount_slot(parent, children);
    return parent;
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ryn::input::InteractionHandlers test_handlers() {
    ryn::input::InteractionHandlers handlers;
    handlers.target = [](ryn::input::PointerDispatchContext&) {};
    return handlers;
}

void test_create_find_remove_and_slot_reuse() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId parent_component;
    ryn::runtime::ComponentId child_component;
    ryn::runtime::ComponentId sibling_component;
    components.mount(ryn::Content{[&] {
        parent_component = mount_parent(Children{[&] {
            child_component = mount_leaf();
        }});
        sibling_component = mount_leaf();
    }});

    ryn::input::InteractionRegistry registry(components, nodes);
    const auto parent = registry.create({
        parent_component,
        components.root(parent_component),
        std::nullopt,
        true,
        true,
        test_handlers(),
    });
    const auto child = registry.create({
        child_component,
        components.root(child_component),
        parent,
        true,
        false,
        test_handlers(),
    });

    const auto* parent_record = registry.find(parent);
    const auto* child_record = registry.find(child);
    require(parent_record != nullptr && child_record != nullptr,
            "live interactions were not registered");
    require(parent_record->component == parent_component
                && parent_record->node == components.root(parent_component)
                && parent_record->focusable
                && static_cast<bool>(parent_record->handlers.target),
            "interaction registration fields differ");
    require(child_record->parent == parent,
            "nested interaction parent was not preserved");
    require(registry.declaration_order().size() == 2
                && registry.declaration_order()[0] == parent
                && registry.declaration_order()[1] == child,
            "interaction declaration order differs");

    require(registry.remove(child), "live interaction could not be removed");
    const auto replacement = registry.create({
        child_component,
        components.root(child_component),
        parent,
        false,
        true,
        {},
    });
    require(replacement.index == child.index,
            "interaction free slot was not reused");
    require(replacement.generation != child.generation,
            "reused interaction slot kept its generation");
    require(registry.find(child) == nullptr && registry.find(replacement) != nullptr,
            "stale interaction accessed a reused slot");
    require(registry.size() == 2, "interaction live count differs");

    bool wrong_owner_rejected = false;
    try {
        static_cast<void>(registry.create({
            child_component,
            components.root(sibling_component),
            parent,
            true,
            false,
            {},
        }));
    } catch (const std::invalid_argument&) {
        wrong_owner_rejected = true;
    }
    require(wrong_owner_rejected,
            "interaction accepted a Node owned by another component");

    const auto sibling = registry.create({
        sibling_component,
        components.root(sibling_component),
        std::nullopt,
        true,
        false,
        {},
    });
    bool unrelated_parent_rejected = false;
    try {
        static_cast<void>(registry.create({
            child_component,
            components.root(child_component),
            sibling,
            true,
            false,
            {},
        }));
    } catch (const std::invalid_argument&) {
        unrelated_parent_rejected = true;
    }
    require(unrelated_parent_rejected,
            "interaction accepted a parent from an unrelated component");
}

void test_component_and_node_generations_cannot_resurrect_records() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::input::InteractionRegistry registry(components, nodes);
    ryn::runtime::ComponentId stale_component;
    ryn::runtime::ComponentId replacement_component;
    ryn::input::InteractionId stale_interaction;
    ryn::input::InteractionId replacement_interaction;

    components.mount(ryn::Content{[&] {
        stale_component = mount_leaf();
        stale_interaction = registry.create({
            stale_component,
            components.root(stale_component),
            std::nullopt,
            true,
            false,
            {},
        });
        require(components.destroy(stale_component),
                "component setup destroy failed");
        replacement_component = mount_leaf();
        replacement_interaction = registry.create({
            replacement_component,
            components.root(replacement_component),
            std::nullopt,
            true,
            false,
            {},
        });
    }});

    require(replacement_component.index == stale_component.index
                && replacement_component.generation != stale_component.generation,
            "component setup did not reuse its generation slot");
    require(registry.find(stale_interaction) == nullptr,
            "stale ComponentId resurrected an interaction record");
    require(registry.find(replacement_interaction) != nullptr,
            "replacement component interaction is inaccessible");

    const auto replacement_node = components.root(replacement_component);
    require(nodes.destroy(replacement_node), "node setup destroy failed");
    const auto reused_node = nodes.create_root();
    require(reused_node.index == replacement_node.index
                && reused_node.generation != replacement_node.generation,
            "node setup did not reuse its generation slot");
    require(registry.find(replacement_interaction) == nullptr,
            "stale NodeId resurrected an interaction record");
}

void test_component_resource_cleanup_removes_interaction() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::input::InteractionRegistry registry(components, nodes);
    ryn::runtime::ComponentId component;
    ryn::input::InteractionId interaction;

    components.mount(ryn::Content{[&] {
        auto& context = ryn::runtime::require_component_build_context();
        component = context.mount_component<TestState>();
        interaction = registry.create({
            component,
            context.root(component),
            std::nullopt,
            true,
            true,
            {},
        });
        context.on_resource_cleanup(component, [&registry, interaction] {
            static_cast<void>(registry.remove(interaction));
        });
    }});

    require(components.destroy(component), "component cleanup destroy failed");
    require(registry.size() == 0 && registry.find(interaction) == nullptr,
            "component resource cleanup retained its interaction");
}

void test_wrong_thread_fails_before_registry_mutation() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId component;
    components.mount(ryn::Content{[&] { component = mount_leaf(); }});
    ryn::input::InteractionRegistry registry(components, nodes);
    const auto interaction = registry.create({
        component,
        components.root(component),
        std::nullopt,
        true,
        false,
        {},
    });
    const auto size_before = registry.size();

    std::atomic<int> rejected{0};
    std::thread worker([&] {
        try {
            static_cast<void>(registry.find(interaction));
        } catch (const std::logic_error&) {
            rejected.fetch_add(1, std::memory_order_relaxed);
        }
        try {
            static_cast<void>(registry.remove(interaction));
        } catch (const std::logic_error&) {
            rejected.fetch_add(1, std::memory_order_relaxed);
        }
    });
    worker.join();

    require(rejected.load(std::memory_order_relaxed) == 2,
            "wrong-thread registry operation did not fail fast");
    require(registry.size() == size_before && registry.find(interaction) != nullptr,
            "wrong-thread registry operation mutated state");
}

} // namespace

int main() {
    try {
        test_create_find_remove_and_slot_reuse();
        test_component_and_node_generations_cannot_resurrect_records();
        test_component_resource_cleanup_removes_interaction();
        test_wrong_thread_fails_before_registry_mutation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
