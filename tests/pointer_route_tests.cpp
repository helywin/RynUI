#include "input/pointer_router.hpp"

#include <ryn/component.hpp>

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

struct Fixture final {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components{nodes};
    ryn::input::InteractionRegistry registry{components, nodes};
    ryn::input::HitTestSnapshot hit_test{registry, nodes};
    ryn::input::PointerRouter router{registry, hit_test};
    ryn::runtime::ComponentId root_component;
    ryn::runtime::ComponentId parent_component;
    ryn::runtime::ComponentId target_component;
    ryn::input::InteractionId root;
    ryn::input::InteractionId parent;
    ryn::input::InteractionId target;

    Fixture() {
        components.mount(ryn::Content{[&] {
            root_component = mount_parent(Children{[&] {
                parent_component = mount_parent(Children{[&] {
                    target_component = mount_leaf();
                }});
            }});
        }});
        commit(components.root(root_component), {0.0F, 0.0F, 100.0F, 100.0F});
        commit(components.root(parent_component), {5.0F, 5.0F, 90.0F, 90.0F});
        commit(components.root(target_component), {10.0F, 10.0F, 80.0F, 80.0F});
        root = registry.create({
            root_component, components.root(root_component), std::nullopt, true, false, {}});
        parent = registry.create({
            parent_component, components.root(parent_component), root, true, false, {}});
        target = registry.create({
            target_component, components.root(target_component), parent, true, false, {}});
        const std::array entries{
            ryn::input::HitTestPaintEntry{root, std::nullopt},
            ryn::input::HitTestPaintEntry{parent, std::nullopt},
            ryn::input::HitTestPaintEntry{target, std::nullopt},
        };
        hit_test.rebuild(entries, {0.0F, 0.0F, 100.0F, 100.0F});
        router.reserve(1, 8);
    }

    void commit(ryn::runtime::NodeId id, ryn::runtime::Rect bounds) {
        auto& node = nodes.require(id);
        node.bounds = bounds;
        node.place_generation = 1;
    }
};

ryn::input::PointerInputEvent move_at(float x = 20.0F, float y = 20.0F) {
    return {
        ryn::input::PointerIdentity::mouse(),
        ryn::input::PointerAction::move,
        ryn::input::PointerButton::none,
        x,
        y,
    };
}

ryn::input::PointerEventHandler log_handler(
    std::vector<std::string>& log,
    std::string value,
    bool stop = false) {
    return [&log, value = std::move(value), stop](ryn::input::PointerDispatchContext& event) {
        if (event.kind() != ryn::input::PointerEventKind::move) {
            return;
        }
        log.push_back(value);
        if (stop) {
            event.stop_propagation();
        }
    };
}

void install_route_handlers(
    Fixture& fixture,
    std::vector<std::string>& log,
    std::optional<ryn::input::PointerPropagationPhase> stop_phase = std::nullopt) {
    ryn::input::InteractionHandlers root_handlers;
    root_handlers.capture = log_handler(
        log,
        "root-capture",
        stop_phase == ryn::input::PointerPropagationPhase::capture);
    root_handlers.bubble = log_handler(log, "root-bubble");
    fixture.registry.set_handlers(fixture.root, std::move(root_handlers));

    ryn::input::InteractionHandlers parent_handlers;
    parent_handlers.capture = log_handler(log, "parent-capture");
    parent_handlers.bubble = log_handler(
        log,
        "parent-bubble",
        stop_phase == ryn::input::PointerPropagationPhase::bubble);
    fixture.registry.set_handlers(fixture.parent, std::move(parent_handlers));

    ryn::input::InteractionHandlers target_handlers;
    target_handlers.target = log_handler(
        log,
        "target",
        stop_phase == ryn::input::PointerPropagationPhase::target);
    fixture.registry.set_handlers(fixture.target, std::move(target_handlers));
}

void test_complete_capture_target_bubble_order() {
    Fixture fixture;
    std::vector<std::string> log;
    install_route_handlers(fixture, log);
    fixture.router.dispatch(move_at());

    require(log == std::vector<std::string>({
                "root-capture",
                "parent-capture",
                "target",
                "parent-bubble",
                "root-bubble",
            }),
            "Capture/Target/Bubble order differs");
    require(fixture.router.diagnostics().routes_dispatched == 1
                && fixture.router.diagnostics().route_entries == 3
                && fixture.router.diagnostics().handlers_invoked == 6
                && fixture.router.diagnostics().hover_enters == 3,
            "route diagnostics differ");
}

void test_stop_propagation_at_each_phase() {
    {
        Fixture fixture;
        std::vector<std::string> log;
        install_route_handlers(
            fixture, log, ryn::input::PointerPropagationPhase::capture);
        fixture.router.dispatch(move_at());
        require(log == std::vector<std::string>({"root-capture"}),
                "Capture stop did not suppress Target/Bubble");
    }
    {
        Fixture fixture;
        std::vector<std::string> log;
        install_route_handlers(
            fixture, log, ryn::input::PointerPropagationPhase::target);
        fixture.router.dispatch(move_at());
        require(log == std::vector<std::string>({
                    "root-capture", "parent-capture", "target"}),
                "Target stop did not suppress Bubble");
    }
    {
        Fixture fixture;
        std::vector<std::string> log;
        install_route_handlers(
            fixture, log, ryn::input::PointerPropagationPhase::bubble);
        fixture.router.dispatch(move_at());
        require(log == std::vector<std::string>({
                    "root-capture",
                    "parent-capture",
                    "target",
                    "parent-bubble",
                }),
                "Bubble stop did not suppress remaining ancestors");
    }
}

void test_empty_route_and_target_handler_are_not_duplicated() {
    Fixture fixture;
    int target_calls = 0;
    ryn::input::InteractionHandlers handlers;
    handlers.target = [&](ryn::input::PointerDispatchContext& event) {
        if (event.kind() == ryn::input::PointerEventKind::move) {
            ++target_calls;
            require(event.phase() == ryn::input::PointerPropagationPhase::target,
                    "target handler received the wrong phase");
            require(event.current_target() == fixture.target
                        && event.dispatch_target() == fixture.target,
                    "target handler identity differs");
        }
    };
    fixture.registry.set_handlers(fixture.target, std::move(handlers));

    fixture.router.dispatch(move_at(150.0F, 150.0F));
    require(target_calls == 0
                && fixture.router.diagnostics().routes_dispatched == 0,
            "empty HitTest route dispatched a handler");
    fixture.router.dispatch(move_at());
    require(target_calls == 1,
            "target handler executed more than once for one route");
}

} // namespace

int main() {
    try {
        test_complete_capture_target_bubble_order();
        test_stop_propagation_at_each_phase();
        test_empty_route_and_target_handler_are_not_duplicated();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
