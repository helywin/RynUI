#include "runtime/component_host.hpp"
#include "input/focus_manager.hpp"
#include "component/component_scene.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void layering_and_input() {
    using namespace ryn;
    using namespace ryn::runtime;
    using namespace ryn::input;
    NodeStore nodes;
    ComponentHost host{nodes};
    InteractionRegistry registry{host, nodes};
    FocusManager focus{registry};
    ComponentId owner;
    ComponentId popup;
    ComponentId text;
    ComponentId peer;
    InteractionId outer;
    InteractionId inner;
    SceneFragmentId a;
    SceneFragmentId b;
    SceneFragmentId c;
    SceneFragmentId d;
    int clicks{};
    host.mount(Content{[&] {
        auto& build = require_component_build_context();
        owner = build.mount_component<int>(0);
        a = build.register_scene_fragment(owner, SceneFragmentPlacement::before_children);
        outer = registry.create({owner, build.root(owner)});
        build.mount_slot(owner, Content{[&] {
                             auto& children = require_component_build_context();
                             popup = children.mount_component<int>(0);
                             b = children.register_scene_fragment(popup, SceneFragmentPlacement::before_children);
                             host.set_window_layer(popup, 100);
                             children.mount_slot(popup, Content{[&] {
                                                     auto& nested = require_component_build_context();
                                                     text = nested.mount_component<int>(0);
                                                     c = nested.register_scene_fragment(
                                                         text, SceneFragmentPlacement::before_children);
                                                     inner = registry.create({text, nested.root(text), {}, true, true});
                                                 }});
                         }});
        peer = build.mount_component<int>(0);
        d = build.register_scene_fragment(peer, SceneFragmentPlacement::before_children);
    }});
    const auto order = [&] {
        std::vector<SceneFragmentId> result;
        for (const auto& entry : host.paint_traversal()) {
            result.push_back(entry.fragment);
        }
        return result;
    };
    check(order() == std::vector<SceneFragmentId>{a, d, b, c}, "overlay subtree was covered by a later sibling");
    check(host.in_window_layer(text) && !host.in_window_layer(owner), "layer inheritance changed normal owner");
    HitTestSnapshot hits{registry, nodes};
    component::ComponentSceneComposer composer{host, registry, hits};
    const graphics::SceneDrawCommand command{graphics::SceneDrawKind::quad, 0, 1};
    composer.set_fragment(b, std::span{&command, 1});
    nodes.require(host.root(popup)).bounds = {250, 150, 50, 30};
    composer.rebuild({0, 0, 100, 100});
    graphics::OrderedScene visible;
    composer.build_visible_scene(nodes, {0, 0, 100, 100}, visible);
    check(visible.commands().size() == 1, "document clip culled a window overlay");
    check(registry.require(inner).parent == outer, "nearest interaction ancestor was not inherited");
    registry.set_focus_handlers(inner, {{}, {}, [&] { ++clicks; }, {}});
    check(focus.request_focus(inner, FocusModality::keyboard), "child focus failed");
    focus.set_command_filter([](const KeyboardInputEvent& event) { return event.key == Key::escape; });
    focus.dispatch({Key::escape, KeyAction::down});
    check(focus.state().focused == inner && clicks == 0, "window command consumed focus or activated child");
    focus.dispatch({Key::enter, KeyAction::down});
    check(clicks == 1, "window filter swallowed normal child activation");
    host.set_branch_active(owner, false);
    check(order() == std::vector<SceneFragmentId>{d}, "hidden ancestor left a visible overlay");
    host.set_branch_active(owner, true);
    host.set_window_layer(popup, {});
    check(order() == std::vector<SceneFragmentId>{a, b, c, d}, "clearing layer did not restore tree paint order");
    host.set_window_layer(popup, 100);
    check(host.destroy(popup) && order() == std::vector<SceneFragmentId>{a, d}, "destroy left stale overlay fragments");
    check(!registry.contains(inner), "destroy left a live stale child interaction");
}
} // namespace

int main() {
    try {
        layering_and_input();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
