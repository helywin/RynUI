#pragma once
#include "support/input_fixture.hpp"
#include <cmath>
#include <iostream>

namespace ryn_test::navigation {
using namespace ryn;
using namespace ryn::input;
using Fixture = input_component::Fixture;

inline void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

inline void sync(Fixture& f, float width = 640) {
    check(f.services.layout_and_synchronize({width, 800}, {0, 0, width, 800}), "navigation layout failed");
}

inline runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    const auto& n = f.nodes.require(f.services.components().root(id));
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

inline InteractionId interaction(Fixture& f, runtime::ComponentId id) {
    for (auto hit : f.services.interactions().declaration_order()) {
        if (f.services.interactions().require(hit).component == id) {
            return hit;
        }
    }
    throw std::runtime_error("navigation item has no interaction");
}

inline void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    }
    sync(f);
}

inline void key(Fixture& f, Key value) {
    f.services.focus().dispatch({value, KeyAction::down});
    f.services.focus().dispatch({value, KeyAction::up});
    sync(f);
}

inline bool descendant(Fixture& f, runtime::ComponentId child, runtime::ComponentId ancestor) {
    for (auto c = std::optional{child}; c; c = f.services.components().parent(*c)) {
        if (*c == ancestor) {
            return true;
        }
    }
    return false;
}

inline const detail::MountedTextComponent& find_text(Fixture& f, StringView value) {
    for (const auto& t : f.services.text().mounted_texts()) {
        if (f.scene.text_state(t.scene).content() == value) {
            return t;
        }
    }
    throw std::runtime_error("navigation Text missing");
}

inline std::size_t glyphs_inside(Fixture& f, runtime::ComponentId ancestor, runtime::Rect allowed) {
    std::size_t visible = 0;
    for (const auto& t : f.services.text().mounted_texts()) {
        if (!descendant(f, t.component, ancestor) || !f.services.components().branch_active(t.component)) {
            continue;
        }
        std::vector<detail::TextSceneId> scenes{t.scene};
        try {
            for (auto scene : f.services.text().icon_snapshot(t.component).layers) {
                if (scene != t.scene) {
                    scenes.push_back(scene);
                }
            }
        } catch (const std::out_of_range&) {
        }
        for (auto scene : scenes) {
            const auto range = f.scene.primitive(scene).instances;
            for (std::uint32_t i = 0; i < range.count; ++i) {
                const auto& g = f.scene.glyph_scene().instances().at(range.first + i);
                const auto& p = g.position_size;
                const auto& c = g.clip_bounds;
                const auto& tr = g.translation_opacity;
                const auto r = graphics::intersect_effect_bounds({p[0] + tr[0], p[1] + tr[1], p[2], p[3]},
                                                                 {c[0], c[1], c[2] - c[0], c[3] - c[1]});
                if (r.width <= 0 || r.height <= 0 || tr[2] <= 0 || g.color[3] <= 0) {
                    continue;
                }
                ++visible;
                if (!(r.x >= allowed.x - .01F && r.y >= allowed.y - .01F &&
                      r.x + r.width <= allowed.x + allowed.width + .01F &&
                      r.y + r.height <= allowed.y + allowed.height + .01F)) {
                    std::cerr << "escaped text=" << f.scene.text_state(t.scene).content().bytes() << " visible=" << r.x
                              << ',' << r.y << ',' << r.width << ',' << r.height << " allowed=" << allowed.x << ','
                              << allowed.y << ',' << allowed.width << ',' << allowed.height << '\n';
                }
                check(r.x >= allowed.x - .01F && r.y >= allowed.y - .01F &&
                          r.x + r.width <= allowed.x + allowed.width + .01F &&
                          r.y + r.height <= allowed.y + allowed.height + .01F,
                      "actual navigation glyph escaped allocation");
            }
        }
    }
    return visible;
}

inline void surfaces_and_hits_inside(Fixture& f, runtime::Rect allowed) {
    const auto contains = [&](runtime::Rect r) {
        return r.width <= 0 || r.height <= 0 ||
               (r.x >= allowed.x - .01F && r.y >= allowed.y - .01F &&
                r.x + r.width <= allowed.x + allowed.width + .01F &&
                r.y + r.height <= allowed.y + allowed.height + .01F);
    };
    for (const auto& effect : f.services.rounded_effects().packed_instances()) {
        if (!effect.material.visible || effect.material.opacity <= 0 || effect.material.color.alpha() <= 0) {
            continue;
        }
        auto visible = graphics::rounded_effect_bounds(effect);
        if (effect.geometry.ancestor_clip) {
            visible = graphics::intersect_effect_bounds(visible, effect.geometry.ancestor_clip->bounds);
        }
        check(contains(visible), "actual navigation surface escaped ancestor clip");
    }
    for (const auto& hit : f.services.hit_test().records()) {
        if (!hit.effective_eligible || !f.services.interactions().branch_active(hit.interaction)) {
            continue;
        }
        check(contains(graphics::intersect_effect_bounds(hit.translated_bounds, hit.effective_clip)),
              "navigation hit escaped ancestor clip");
    }
}
} // namespace ryn_test::navigation
