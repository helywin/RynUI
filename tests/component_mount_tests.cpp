#include "runtime/component_host.hpp"
#include "runtime/component_mount.hpp"
#include "runtime/prop_connection.hpp"

#include <ryn/component.hpp>
#include <ryn/reactive.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct TestState final {
    int value{};
    int* destructor_count{};
    std::vector<std::string>* lifecycle{};

    explicit TestState(int initial = 0) : value(initial) {}

    TestState(
        int initial,
        int& destroyed,
        std::vector<std::string>& events)
        : value(initial), destructor_count(&destroyed), lifecycle(&events) {}

    ~TestState() {
        if (destructor_count != nullptr) {
            ++*destructor_count;
        }
        if (lifecycle != nullptr) {
            lifecycle->push_back("state");
        }
    }
};

struct ContainerContentSlot final {};
using ContainerContent = ryn::SlotContent<ContainerContentSlot>;

ryn::runtime::ComponentId mount_test_component(int initial = 0) {
    return ryn::runtime::require_component_build_context()
        .mount_component<TestState>(initial);
}

ryn::runtime::ComponentId mount_container(const ContainerContent& content) {
    auto& context = ryn::runtime::require_component_build_context();
    const auto id = context.mount_component<TestState>(100);
    context.mount_slot(id, content);
    return id;
}

void test_property_update_does_not_rerun_component() {
    ryn::runtime::NodeStore nodes;
    ryn::Signal<float> opacity{1.0F};
    ryn::runtime::NodeId root;
    int component_runs = 0;
    int property_updates = 0;

    ryn::runtime::ComponentInstance component(
        nodes,
        [&](ryn::runtime::MountContext& context) {
            ++component_runs;
            root = context.create_root();
            ryn::effect(context.scope(), [&] {
                nodes.require(root).opacity = opacity.get();
                ++property_updates;
            });
        });

    require(component_runs == 1, "Component did not mount exactly once");
    require(component.mount_runs() == 1, "Component mount metric is incorrect");
    require(property_updates == 1, "mounted property was not initialized");

    opacity.set(0.5F);
    require(component_runs == 1, "property update reran the Component");
    require(property_updates == 2, "property binding did not update");
    require(nodes.require(root).opacity == 0.5F, "retained Node property was not updated");
}

void test_component_disposal_stops_scope_and_unmounts_nodes() {
    ryn::runtime::NodeStore nodes;
    ryn::Signal<float> opacity{1.0F};
    ryn::runtime::NodeId root;
    int property_updates = 0;

    ryn::runtime::ComponentInstance component(
        nodes,
        [&](ryn::runtime::MountContext& context) {
            root = context.create_root();
            static_cast<void>(context.create_child(root));
            ryn::effect(context.scope(), [&] {
                nodes.require(root).opacity = opacity.get();
                ++property_updates;
            });
        });

    component.dispose();
    component.dispose();
    require(!component.active(), "disposed Component remained active");
    require(nodes.size() == 0, "Component disposal did not unmount its Node tree");
    require(nodes.find(root) == nullptr, "Component root remained accessible after disposal");

    opacity.set(0.25F);
    require(property_updates == 1, "disposed Component Scope received a property update");
}

void test_host_registry_order_state_and_stale_identity() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost host(nodes);
    ryn::runtime::ComponentId stale;
    ryn::runtime::ComponentId replacement;
    ryn::runtime::ComponentId sibling;

    host.mount(ryn::Content{[&] {
        stale = mount_test_component(1);
        require(host.destroy(stale), "live component could not be destroyed during mount");
        replacement = mount_test_component(2);
        sibling = mount_test_component(3);
    }});

    require(replacement.index == stale.index,
            "ComponentHost did not reuse a free component slot");
    require(replacement.generation != stale.generation,
            "reused component slot kept its generation");
    require(!host.contains(stale) && host.state<TestState>(stale) == nullptr,
            "stale ComponentId accessed reused type state");
    require(host.contains(replacement) && host.contains(sibling),
            "live ComponentId was not registered");
    require(host.state<TestState>(replacement)->value == 2
                && host.state<TestState>(sibling)->value == 3,
            "component type state was not retained");
    require(host.declaration_order(replacement)
                < host.declaration_order(sibling),
            "sibling declaration order was not stable");
    require(nodes.size() == 2 && host.component_count() == 2,
            "component roots were not retained one-to-one");
}

void test_nested_typed_slot_preserves_parent_and_order() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost host(nodes);
    ryn::runtime::ComponentId parent;
    ryn::runtime::ComponentId first_child;
    ryn::runtime::ComponentId second_child;

    host.mount(ryn::Content{[&] {
        parent = mount_container(ContainerContent{[&] {
            first_child = mount_test_component(1);
            second_child = mount_test_component(2);
        }});
    }});

    require(host.children(parent)
                == std::vector<ryn::runtime::ComponentId>({first_child, second_child}),
            "typed slot children did not keep declaration order");
    require(host.parent(first_child) == parent
                && host.parent(second_child) == parent,
            "typed slot children mounted under the wrong component");
    require(nodes.require(host.root(first_child)).parent == host.root(parent)
                && nodes.require(host.root(second_child)).parent == host.root(parent),
            "typed slot Node roots mounted under the wrong parent Node");
}

void test_prop_update_reuses_mounted_components() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost host(nodes);
    ryn::Signal<int> source{4};
    ryn::runtime::ComponentId target;
    ryn::runtime::ComponentId sibling;
    int content_runs = 0;
    int target_updates = 0;

    host.mount(ryn::Content{[&] {
        ++content_runs;
        auto& context = ryn::runtime::require_component_build_context();
        target = context.mount_component<TestState>();
        sibling = context.mount_component<TestState>(9);
        static_cast<void>(ryn::detail::connect_prop(
            context.scope(target),
            ryn::Prop<int>{source},
            [&](int value) {
                host.state<TestState>(target)->value = value;
                ++target_updates;
            }));
    }});

    const auto sibling_root = host.root(sibling);
    source.set(8);
    require(content_runs == 1 && host.mount_runs() == 1,
            "Prop update reran Host content");
    require(target_updates == 2 && host.state<TestState>(target)->value == 8,
            "Prop update did not reach retained component state");
    require(host.root(sibling) == sibling_root
                && host.state<TestState>(sibling)->value == 9,
            "Prop update changed sibling identity or state");
}

void test_mount_stack_fail_fast_and_recovers() {
    bool no_host_diagnosed = false;
    try {
        static_cast<void>(mount_test_component());
    } catch (const std::logic_error&) {
        no_host_diagnosed = true;
    }
    require(no_host_diagnosed, "component declaration without Host did not fail fast");

    ryn::runtime::NodeStore wrong_thread_nodes;
    ryn::runtime::ComponentHost wrong_thread_host(wrong_thread_nodes);
    bool wrong_thread_diagnosed = false;
    std::thread worker([&] {
        try {
            wrong_thread_host.mount(ryn::Content{[] {
                static_cast<void>(mount_test_component());
            }});
        } catch (const std::logic_error&) {
            wrong_thread_diagnosed = true;
        }
    });
    worker.join();
    require(wrong_thread_diagnosed,
            "ComponentHost mount on a non-owner thread did not fail fast");
    require(wrong_thread_nodes.size() == 0,
            "wrong-thread Host mount changed the Node tree");

    ryn::runtime::NodeStore throwing_nodes;
    ryn::runtime::ComponentHost throwing_host(throwing_nodes);
    bool exception_observed = false;
    try {
        throwing_host.mount(ryn::Content{[] {
            static_cast<void>(mount_test_component());
            throw std::runtime_error("mount failure");
        }});
    } catch (const std::runtime_error&) {
        exception_observed = true;
    }
    require(exception_observed && throwing_nodes.size() == 0,
            "throwing content did not clean up partial mount");

    ryn::runtime::NodeStore recovered_nodes;
    ryn::runtime::ComponentHost recovered_host(recovered_nodes);
    recovered_host.mount(ryn::Content{[] {
        static_cast<void>(mount_test_component());
    }});
    require(recovered_host.component_count() == 1,
            "mount stack was not restored after exceptional exit");
}

void test_mount_reentry_is_rejected_and_stack_recovers() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost host(nodes);
    bool diagnosed = false;
    try {
        host.mount(ryn::Content{[&] {
            host.mount(ryn::Content{[] {}});
        }});
    } catch (const std::logic_error&) {
        diagnosed = true;
    }
    require(diagnosed && !host.active() && nodes.size() == 0,
            "reentrant Host mount was not rejected atomically");

    bool no_host_diagnosed = false;
    try {
        static_cast<void>(mount_test_component());
    } catch (const std::logic_error&) {
        no_host_diagnosed = true;
    }
    require(no_host_diagnosed,
            "reentrant mount left a stale active build context");
}

void test_host_dispose_order_and_idempotence() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost host(nodes);
    ryn::Signal<int> source{1};
    std::vector<std::string> lifecycle;
    ryn::runtime::ComponentId parent;
    ryn::runtime::ComponentId child;
    int state_destructions = 0;
    int prop_updates = 0;
    int resource_cleanups = 0;

    host.mount(ryn::Content{[&] {
        auto& context = ryn::runtime::require_component_build_context();
        parent = context.mount_component<TestState>(
            0,
            state_destructions,
            lifecycle);
        context.scope(parent).on_cleanup([&] { lifecycle.push_back("scope"); });
        context.on_resource_cleanup(parent, [&] {
            ++resource_cleanups;
            lifecycle.push_back("resource");
        });
        static_cast<void>(ryn::detail::connect_prop(
            context.scope(parent),
            ryn::Prop<int>{source},
            [&](int value) {
                ++prop_updates;
                if (auto* state = host.state<TestState>(parent)) {
                    state->value = value;
                }
            }));
        context.mount_slot(parent, ContainerContent{[&] {
            child = mount_test_component(2);
        }});
    }});

    const auto parent_root = host.root(parent);
    const auto child_root = host.root(child);
    require(prop_updates == 1, "component Prop did not initialize before disposal");
    host.dispose();
    host.dispose();

    require(!host.active() && host.component_count() == 0,
            "Host repeated disposal retained components");
    require(nodes.find(parent_root) == nullptr && nodes.find(child_root) == nullptr,
            "Host disposal retained a component Node subtree");
    require(resource_cleanups == 1 && state_destructions == 1,
            "Host disposal repeated resource or type-state cleanup");
    require(lifecycle == std::vector<std::string>({"scope", "resource", "state"}),
            "Host disposal did not follow Scope/resource/state ordering");

    source.set(3);
    require(prop_updates == 1,
            "destroyed component Prop requested an update after Host disposal");
}

} // namespace

int main() {
    try {
        test_property_update_does_not_rerun_component();
        test_component_disposal_stops_scope_and_unmounts_nodes();
        test_host_registry_order_state_and_stale_identity();
        test_nested_typed_slot_preserves_parent_and_order();
        test_prop_update_reuses_mounted_components();
        test_mount_stack_fail_fast_and_recovers();
        test_mount_reentry_is_rejected_and_stack_recovers();
        test_host_dispose_order_and_idempotence();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
