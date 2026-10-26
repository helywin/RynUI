#pragma once
#include "component/auto_complete_component.hpp"
#include "support/input_fixture.hpp"
#include <algorithm>
#include <iostream>

namespace auto_complete_test {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

inline void check(bool ok, const char* what) {
    if (!ok) {
        throw std::runtime_error(what);
    }
}

inline void sync(Fixture& f, float w = 520, float h = 360) {
    check(f.services.layout_and_synchronize({w, h}, {0, 0, w, h}), "sync");
}

inline runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

inline bool descendant(Fixture& f, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto p = std::optional{child}; p && f.services.components().contains(*p);
         p = f.services.components().parent(*p)) {
        if (*p == parent) {
            return true;
        }
    }
    return false;
}

inline void click(Fixture& f, runtime::Rect rect) {
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                       rect.x + rect.width / 2, rect.y + rect.height / 2});
    }
    sync(f);
}

inline void click(Fixture& f, runtime::ComponentId id) {
    click(f, bounds(f, id));
}

inline void key(Fixture& f, Key key, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({key, KeyAction::down, modifiers});
    f.services.focus().dispatch({key, KeyAction::up, modifiers});
    sync(f);
}

inline void edit(Fixture& f, String value) {
    const auto stamp = f.inputs.sessions().active();
    (void)f.inputs.editors().require(stamp.owner).select_all();
    check(bool(f.inputs.dispatch(TextCommitted{std::move(value), stamp})), "commit");
    sync(f);
}

inline auto snapshot(Fixture& f, runtime::ComponentId id) {
    return f.services.auto_complete().snapshot(id);
}

inline std::vector<AutoCompleteOption> options() {
    return {{u8"a", u8"alpha", u8"Alpha"}, {u8"b", u8"beta", u8"Beta", true}, {u8"c", u8"gamma", u8"Gamma"}};
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

inline void clean(Fixture& f) {
    f.services.dispose();
    check(f.nodes.size() == 0 && f.scene.size() == 0 && f.scene.glyph_scene().instances().size() == 0 &&
              f.services.surfaces().size() == 0 && f.services.interactions().size() == 0 &&
              blocked_roots(f.services.interactions()).empty() && !f.services.next_frame_deadline(),
          "AutoComplete teardown leaked nodes/glyphs/surfaces/hits/blocks/deadlines");
}
} // namespace auto_complete_test
