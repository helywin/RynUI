#pragma once
#include "component/timeline_component.hpp"
#include "support/input_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace timeline_test {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

inline void check(bool ok, const char* message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}

inline void sync(Fixture& f) {
    check(f.services.layout_and_synchronize({600, 800}, {0, 0, 600, 800}), "Timeline layout failed");
}

inline runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    const auto& n = f.nodes.require(f.services.components().root(id));
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

inline void click(Fixture& f, runtime::ComponentId id) {
    const auto b = bounds(f, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, b.x + b.width / 2, b.y + b.height / 2});
    }
    sync(f);
}

inline bool inside(runtime::Rect r, runtime::Rect c) {
    return r.width <= 0 || r.height <= 0 ||
           (r.x >= c.x - .01F && r.y >= c.y - .01F && r.x + r.width <= c.x + c.width + .01F &&
            r.y + r.height <= c.y + c.height + .01F);
}

inline bool contains(runtime::Rect r, runtime::Point p) {
    return r.width > 0 && r.height > 0 && p.x >= r.x && p.y >= r.y && p.x <= r.x + r.width && p.y <= r.y + r.height;
}

inline bool descendant(Fixture& f, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto current = std::optional{child}; current; current = f.services.components().parent(*current)) {
        if (*current == parent) {
            return true;
        }
    }
    return false;
}

struct BlockedRootsMember final {
    using Pointer = std::vector<runtime::ComponentId> InteractionRegistry::*;
    friend Pointer blocked_roots_member(BlockedRootsMember);
};

template <BlockedRootsMember::Pointer Member> struct ReadBlockedRoots final {
    friend BlockedRootsMember::Pointer blocked_roots_member(BlockedRootsMember) {
        return Member;
    }
};
template struct ReadBlockedRoots<&InteractionRegistry::blocked_roots_>;

inline const auto& blocked_roots(const InteractionRegistry& registry) {
    return registry.*blocked_roots_member(BlockedRootsMember{});
}
} // namespace timeline_test
