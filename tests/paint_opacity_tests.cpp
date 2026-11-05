#include "support/input_fixture.hpp"
#include "component/retained_surface_service.hpp"
#include "runtime/prop_connection.hpp"
#include <cmath>
#include <iostream>
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool v, const char* m) {
    if (!v) {
        throw std::runtime_error(m);
    }
}

bool near(float a, float b) {
    return std::abs(a - b) < .0001F;
}

void sync(Fixture& f) {
    check(f.services.layout_and_synchronize({600, 500}, {0, 0, 600, 500}), "paint opacity layout");
}

std::vector<float> quads(Fixture& f) {
    std::vector<float> out;
    for (const auto& q : f.services.surfaces().instances().instances()) {
        out.push_back(q.opacity);
    }
    return out;
}

std::vector<float> glyphs(Fixture& f) {
    std::vector<float> out;
    for (const auto& g : f.scene.glyph_scene().instances().instances()) {
        out.push_back(g.translation_opacity[2]);
    }
    return out;
}

void scaled(const std::vector<float>& before, const std::vector<float>& after, float factor, const char* message) {
    check(before.size() == after.size(), message);
    for (std::size_t i = 0; i < before.size(); ++i) {
        check(near(after[i], before[i] * factor), message);
    }
}

void run() {
    {
        runtime::NodeStore nodes;
        const auto first = nodes.create_root();
        const auto second = nodes.create_root();
        const auto child = nodes.create_child(first);
        nodes.set_paint_opacity(first, .5F);
        nodes.set_paint_opacity(second, .25F);
        check(near(nodes.paint_opacity(child), .5F), "initial paint ancestry");
        std::erase(nodes.require(first).children, child);
        nodes.require(second).children.push_back(child);
        nodes.require(child).parent = second;
        check(near(nodes.paint_opacity(child), .25F), "paint ancestry cached across reparent");
        nodes.destroy(first);
        check(nodes.has_paint_opacity() && near(nodes.paint_opacity(child), .25F), "destroy decremented wrong owner");
        nodes.destroy(second);
        check(!nodes.has_paint_opacity() && nodes.size() == 0, "nested destroy leaked opacity counter");
        const auto reused = nodes.create_root();
        check(near(nodes.paint_opacity(reused), 1), "recycled node retained opacity");
    }
    Fixture f;
    component::RetainedSurfaceId fixed;
    component::RetainedSurfaceId variable;
    runtime::ComponentId manual;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).layout(LayoutStyle{}.width(dp(500))), [&] {
            Button(ButtonProps{}.type(ButtonType::Primary).wave(false), [] { Text(u8"Native button"); });
            Input(InputProps{}.defaultValue(u8"Native input").layout(LayoutStyle{}.width(dp(240))));
            Icon(IconProps{}.name(IconName::CalendarOutlined));
            Text(u8"Native text");
            auto& b = runtime::require_component_build_context();
            manual = b.mount_component<int>(0);
            auto node = b.root(manual);
            f.nodes.require(node).requested_size = {60, 24};
            f.layout.set_layout(node, layout::LeafLayout{});
            auto fragment = b.register_scene_fragment(manual, runtime::SceneFragmentPlacement::before_children);
            graphics::QuadInstance q;
            q.bounds = {0, 0, 60, 24};
            q.opacity = .8F;
            fixed = f.services.surfaces().create_surface(manual, node, fragment, std::span{&q, 1});
            auto content = b.register_scene_fragment(manual, runtime::SceneFragmentPlacement::after_children);
            variable = f.services.surfaces().create_content_range(content, std::span{&q, 1});
            b.on_resource_cleanup(manual, [&, node] {
                f.services.surfaces().destroy(fixed);
                f.services.surfaces().destroy_content_range(variable);
                f.layout.remove_layout(node);
            });
        });
    }});
    sync(f);
    const auto root = f.services.components().root_components().front();
    const auto node = f.services.components().root(root);
    auto r = f.nodes.require(f.services.components().root(manual)).bounds;
    component::RetainedSurfaceEffects effect;
    effect.shape = {r, 0};
    effect.focus_color = Color::rgba8(255, 0, 0);
    effect.focus_width = 1;
    effect.focus_opacity = .7F;
    static_cast<void>(f.services.surfaces().update_effects(fixed, effect));
    auto fill = graphics::make_outline_effect({r, 0}, 1, 0, Color::rgba8(0, 0, 255), .6F);
    f.services.surfaces().update_content_effects(variable, std::span{&fill, 1});
    sync(f);
    const auto original = quads(f);
    const auto text = glyphs(f);
    check(!text.empty(), "glyph fixture empty");
    check(f.nodes.set_paint_opacity(node, .5F), "root opacity change");
    sync(f);
    scaled(original, quads(f), .5F, "native/control/content quad envelope");
    scaled(text, glyphs(f), .5F, "text/input/icon glyph envelope");
    check(near(f.services.surfaces().focus_effect(fixed).material.opacity, .35F),
          "cached fixed effects did not inherit alpha");
    f.nodes.set_paint_opacity(node, 0);
    sync(f);
    for (auto alpha : quads(f)) {
        check(alpha == 0, "zero envelope");
    }
    f.nodes.set_paint_opacity(node, 1);
    sync(f);
    scaled(original, quads(f), 1, "restoration lost unscaled source");
    scaled(text, glyphs(f), 1, "glyph restoration");
    f.nodes.set_paint_opacity(node, .5F);
    f.nodes.set_paint_opacity(f.services.components().root(manual), .5F);
    graphics::QuadInstance a;
    a.bounds = {r.x, r.y, 10, 10};
    a.opacity = .8F;
    auto b = a;
    b.opacity = .4F;
    auto c = a;
    c.opacity = 1;
    std::array changed{a, b, c};
    static_cast<void>(f.services.surfaces().update_content_range(variable, changed));
    sync(f);
    check(near(f.nodes.paint_opacity(f.services.components().root(manual)), .25F), "nested envelope");
    f.nodes.set_paint_opacity(node, 1);
    f.nodes.set_paint_opacity(f.services.components().root(manual), 1);
    sync(f);
    const auto final = quads(f);
    check(final.size() == original.size() + 2 && near(final[final.size() - 3], .8F) &&
              near(final[final.size() - 2], .4F) && near(final.back(), 1),
          "range remap restored painted data instead of source");
    while (f.frames.consume_request()) {
    }
    const auto requests = f.frames.counters().requests;
    sync(f);
    sync(f);
    check(f.frames.counters().requests == requests, "settled envelope requested frames");
    bool rejected{};
    try {
        f.nodes.set_paint_opacity(node, -1);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "invalid envelope accepted");
    // Creating and updating between synchronization passes must still preserve
    // the unpainted source, including a reset before the first active pass.
    f.nodes.set_paint_opacity(node, .25F);
    const auto fragment =
        f.services.components().register_scene_fragment(manual, runtime::SceneFragmentPlacement::after_children);
    const auto created = f.services.surfaces().create_surface(manual, f.services.components().root(manual), fragment,
                                                              std::span{&a, 1}, effect);
    check(near(f.services.surfaces().instances().instances()[f.services.surfaces().visual_range(created).first].opacity,
               .2F),
          "new surface omitted current envelope");
    check(near(f.services.surfaces().focus_effect(created).material.opacity, .175F),
          "new effect omitted current envelope");
    f.nodes.set_paint_opacity(node, 1);
    sync(f);
    check(near(f.services.surfaces().instances().instances()[f.services.surfaces().visual_range(created).first].opacity,
               .8F),
          "new surface reset before first synchronization lost source");
    check(f.services.surfaces().destroy(created), "new surface retirement failed");
    f.nodes.set_paint_opacity(node, .5F);
    auto updated = a;
    updated.opacity = .6F;
    static_cast<void>(f.services.surfaces().update_surface(fixed, std::span{&updated, 1}));
    static_cast<void>(f.services.surfaces().update_content_range(variable, changed));
    static_cast<void>(f.services.surfaces().update_effects(fixed, effect));
    static_cast<void>(f.services.surfaces().update_content_effects(variable, std::span{&fill, 1}));
    f.nodes.set_paint_opacity(node, 1);
    sync(f);
    check(near(f.services.surfaces().instances().instances()[f.services.surfaces().visual_range(fixed).first].opacity,
               .6F),
          "surface update/reset before synchronization retained painted source");
    check(near(f.services.surfaces().focus_effect(fixed).material.opacity, .7F),
          "effect update/reset before synchronization retained envelope");
    const auto content_first = f.services.surfaces().visual_range(variable).first;
    check(near(f.services.surfaces().instances().instances()[content_first].opacity, .8F),
          "content update/reset before synchronization retained envelope");
    f.nodes.set_paint_opacity(node, .5F);
    f.services.dispose();
    check(f.nodes.size() == 0 && !f.nodes.has_paint_opacity() && f.services.surfaces().size() == 0,
          "envelope ownership leaked after destroy");
}

int main() {
    try {
        run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
