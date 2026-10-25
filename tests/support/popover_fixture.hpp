#pragma once
#include "component/popover_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>

namespace ryn_test::popover {
using namespace ryn;
using namespace ryn::input;
using Fixture = input_component::Fixture;

inline void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

inline void sync(Fixture& f, float width = 700, float height = 600) {
    check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}), "Popover layout failed");
}

inline runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    const auto& n = f.nodes.require(f.services.components().root(id));
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

inline bool descendant(Fixture& f, runtime::ComponentId child, runtime::ComponentId ancestor) {
    for (auto p = std::optional{child}; p && f.services.components().contains(*p);
         p = f.services.components().parent(*p)) {
        if (*p == ancestor) {
            return true;
        }
    }
    return false;
}

inline InteractionId interaction(Fixture& f, runtime::ComponentId id) {
    for (auto hit : f.services.interactions().declaration_order()) {
        if (f.services.interactions().require(hit).component == id) {
            return hit;
        }
    }
    throw std::runtime_error("missing interaction");
}

inline runtime::ComponentId trigger(Fixture& f, runtime::ComponentId owner) {
    return f.services.components().children(f.services.popover().snapshot(owner).trigger).front();
}

inline void pointer(Fixture& f, PointerAction action, float x, float y) {
    f.services.pointer().dispatch({PointerIdentity::mouse(), action,
                                   action == PointerAction::move ? PointerButton::none : PointerButton::primary, x, y});
}

inline void click_at(Fixture& f, float x, float y) {
    pointer(f, PointerAction::down, x, y);
    pointer(f, PointerAction::up, x, y);
    sync(f);
}

inline void click(Fixture& f, runtime::ComponentId id) {
    const auto b = bounds(f, id);
    click_at(f, b.x + b.width / 2, b.y + b.height / 2);
}

inline void key(Fixture& f, Key value) {
    f.services.focus().dispatch({value, KeyAction::down});
    f.services.focus().dispatch({value, KeyAction::up});
    sync(f);
}

inline void tick(Fixture& f, std::int64_t micros) {
    const auto time = animation::AnimationTime::microseconds(micros);
    f.services.set_animation_time(time);
    static_cast<void>(f.services.tick_animations(time));
    sync(f);
}

inline bool contains(runtime::Rect outer, runtime::Rect inner) {
    return inner.width <= 0 || inner.height <= 0 ||
           (inner.x >= outer.x - .01F && inner.y >= outer.y - .01F &&
            inner.x + inner.width <= outer.x + outer.width + .01F &&
            inner.y + inner.height <= outer.y + outer.height + .01F);
}
} // namespace ryn_test::popover
