#pragma once
#include "component/rate_component.hpp"
#include "component/tooltip_component.hpp"
#include "support/input_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace rate_test {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

inline void check(bool v, const char* message) {
    if (!v) {
        throw std::runtime_error(message);
    }
}

inline void sync(Fixture& f, runtime::Rect clip = {0, 0, 400, 300}, float gap = 0) {
    check(f.services.layout_and_synchronize({400, 300}, clip, {}, gap), "Rate layout failed");
}

inline runtime::Rect bounds(Fixture& f, runtime::NodeId id) {
    const auto& n = f.nodes.require(id);
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

inline void pointer(Fixture& f, const detail::RateItemSnapshot& row, PointerAction action, float fraction = .25F) {
    const auto r = bounds(f, row.node);
    f.services.pointer().dispatch({PointerIdentity::mouse(), action,
                                   action == PointerAction::move || action == PointerAction::cancel
                                       ? PointerButton::none
                                       : PointerButton::primary,
                                   r.x + r.width * fraction, r.y + std::min(8.0F, r.height / 2)});
}

inline void click(Fixture& f, const detail::RateItemSnapshot& row, float fraction = .25F) {
    pointer(f, row, PointerAction::down, fraction);
    pointer(f, row, PointerAction::up, fraction);
    sync(f);
}

inline void key(Fixture& f, Key value) {
    f.services.focus().dispatch({value, KeyAction::down});
    f.services.focus().dispatch({value, KeyAction::up});
    sync(f);
}

inline bool descendant(Fixture& f, runtime::NodeId child, runtime::NodeId ancestor) {
    for (auto n = std::optional{child}; n; n = f.nodes.require(*n).parent) {
        if (*n == ancestor) {
            return true;
        }
    }
    return false;
}

inline std::size_t glyphs(Fixture& f, runtime::NodeId root, runtime::Rect clip) {
    std::size_t count = 0;
    for (const auto& text : f.services.text().mounted_texts()) {
        const auto node = f.services.components().root(text.component);
        if (!descendant(f, node, root)) {
            continue;
        }
        const auto range = f.scene.primitive(text.scene).instances;
        for (std::uint32_t i = 0; i < range.count; ++i) {
            const auto& glyph = f.scene.glyph_scene().instances().at(range.first + i);
            const auto& p = glyph.position_size;
            const auto& c = glyph.clip_bounds;
            const auto& t = glyph.translation_opacity;
            const auto visible = graphics::intersect_effect_bounds({p[0] + t[0], p[1] + t[1], p[2], p[3]},
                                                                   {c[0], c[1], c[2] - c[0], c[3] - c[1]});
            if (visible.width > 0 && visible.height > 0 && t[2] > 0 && glyph.color[3] > 0) {
                check(visible.x >= clip.x - .01F && visible.y >= clip.y - .01F &&
                          visible.x + visible.width <= clip.x + clip.width + .01F &&
                          visible.y + visible.height <= clip.y + clip.height + .01F,
                      "Rate glyph escaped effective clip");
                ++count;
            }
        }
    }
    return count;
}

struct BlockedMember final {
    using Pointer = std::vector<runtime::ComponentId> InteractionRegistry::*;
    friend Pointer rate_blocked_member(BlockedMember);
};

template <BlockedMember::Pointer Member> struct ReadBlocked final {
    friend BlockedMember::Pointer rate_blocked_member(BlockedMember) {
        return Member;
    }
};
template struct ReadBlocked<&InteractionRegistry::blocked_roots_>;

inline const auto& blocked(const InteractionRegistry& registry) {
    return registry.*rate_blocked_member(BlockedMember{});
}

inline void no_capture(Fixture& f) {
    const auto state = f.services.pointer().state(PointerIdentity::mouse());
    check(!state || !state->capture, "Rate retained capture");
}
} // namespace rate_test
