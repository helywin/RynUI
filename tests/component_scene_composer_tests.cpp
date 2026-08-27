#include "component/component_scene.hpp"

#include <ryn/component.hpp>

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {

struct TestState final {};
struct ChildrenSlot final {};
using Children = ryn::SlotContent<ChildrenSlot>;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_ordered_scene_and_hit_test_share_component_traversal() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId first_component;
    ryn::runtime::ComponentId text_component;
    ryn::runtime::ComponentId second_component;
    ryn::runtime::SceneFragmentId first_fragment;
    ryn::runtime::SceneFragmentId text_fragment;
    ryn::runtime::SceneFragmentId second_fragment;
    components.mount(ryn::Content{[&] {
        auto& build = ryn::runtime::require_component_build_context();
        first_component = build.mount_component<TestState>();
        first_fragment = build.register_scene_fragment(
            first_component,
            ryn::runtime::SceneFragmentPlacement::before_children);
        build.mount_slot(first_component, Children{[&] {
            auto& text_build = ryn::runtime::require_component_build_context();
            text_component = text_build.mount_component<TestState>();
            text_fragment = text_build.register_scene_fragment(
                text_component,
                ryn::runtime::SceneFragmentPlacement::after_children);
        }});
        second_component = build.mount_component<TestState>();
        second_fragment = build.register_scene_fragment(
            second_component,
            ryn::runtime::SceneFragmentPlacement::before_children);
    }});

    for (const auto component : {first_component, second_component}) {
        auto& node = nodes.require(components.root(component));
        node.bounds = {10.0F, 10.0F, 80.0F, 40.0F};
        node.place_generation = 1;
    }
    auto& text_node = nodes.require(components.root(text_component));
    text_node.bounds = {20.0F, 20.0F, 40.0F, 20.0F};
    text_node.place_generation = 1;

    ryn::input::InteractionRegistry interactions(components, nodes);
    const auto first_interaction = interactions.create({
        first_component,
        components.root(first_component),
        std::nullopt,
        true,
        true,
        {},
    });
    const auto second_interaction = interactions.create({
        second_component,
        components.root(second_component),
        std::nullopt,
        true,
        true,
        {},
    });
    ryn::input::HitTestSnapshot hit_test(interactions, nodes);
    ryn::component::ComponentSceneComposer composer(
        components, interactions, hit_test);
    composer.reserve(3, 3, 2);

    const ryn::graphics::SceneDrawCommand first_command{
        ryn::graphics::SceneDrawKind::quad,
        0,
        4,
        ryn::graphics::invalid_glyph_atlas_page,
    };
    const ryn::graphics::SceneDrawCommand text_command{
        ryn::graphics::SceneDrawKind::glyph,
        0,
        2,
        0,
    };
    const ryn::graphics::SceneDrawCommand second_command{
        ryn::graphics::SceneDrawKind::quad,
        4,
        4,
        ryn::graphics::invalid_glyph_atlas_page,
    };
    composer.set_fragment(first_fragment, {&first_command, 1}, first_interaction);
    composer.set_fragment(text_fragment, {&text_command, 1});
    composer.set_fragment(second_fragment, {&second_command, 1}, second_interaction);
    composer.rebuild({0.0F, 0.0F, 100.0F, 100.0F});

    const std::array expected_commands{
        first_command,
        text_command,
        second_command,
    };
    require(std::ranges::equal(
                composer.ordered_scene().commands(), expected_commands),
            "Quad/Glyph scene order diverged from component traversal");
    require(composer.interaction_order().size() == 2
                && composer.interaction_order()[0].interaction == first_interaction
                && composer.interaction_order()[1].interaction == second_interaction,
            "HitTest interaction order diverged from component traversal");
    require(hit_test.hit_test({20.0F, 20.0F}) == second_interaction,
            "visually topmost overlapping component was not the HitTest target");

    require(components.destroy(second_component),
            "second overlapping component destroy failed");
    composer.rebuild({0.0F, 0.0F, 100.0F, 100.0F});
    require(composer.ordered_scene().commands().size() == 2
                && composer.interaction_order().size() == 1
                && hit_test.hit_test({20.0F, 20.0F}) == first_interaction,
            "structure destroy did not update scene and HitTest together");
}

} // namespace

int main() {
    try {
        test_ordered_scene_and_hit_test_share_component_traversal();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
