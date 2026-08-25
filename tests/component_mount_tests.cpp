#include "runtime/component_mount.hpp"

#include <ryn/reactive.hpp>

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
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

} // namespace

int main() {
    try {
        test_property_update_does_not_rerun_component();
        test_component_disposal_stops_scope_and_unmounts_nodes();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
