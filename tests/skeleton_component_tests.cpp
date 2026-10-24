#include "component/skeleton_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

auto quads(Fixture& f, runtime::ComponentId id) {
    const auto range = f.services.surfaces().visual_range(f.services.skeleton().snapshot(id).surface);
    const auto all = f.services.surfaces().instances().instances();
    return std::vector<graphics::QuadInstance>(all.begin() + range.first, all.begin() + range.first + range.count);
}

void scene_and_retained_content() {
    Fixture f;
    Signal<bool> loading{true};
    Signal<SkeletonParagraph> paragraph{SkeletonParagraph{}};
    int slots{};
    f.services.mount(Content{[&] {
        Skeleton(SkeletonProps{}.loading(loading).paragraph(paragraph).layout(LayoutStyle{}.width(dp(200))),
                 SkeletonContent{[&] {
                     ++slots;
                     Input(InputProps{}.defaultValue(u8"Draft"));
                 }});
    }});
    f.synchronize();
    const auto id = f.services.skeleton().mounted()[0];
    const auto original = f.services.skeleton().snapshot(id);
    const auto input = f.inputs.mounted_inputs()[0];
    const auto count = f.services.components().component_count();
    const auto first = quads(f, id);
    check(first.size() == 4 && first[0].bounds[2] == 76 && first[3].bounds[2] == 122 && first[0].opacity == 1,
          "Skeleton real quad title/rows did not match layout");
    check(!f.services.components().branch_active(original.content) &&
              !f.services.interactions().component_active(input.component),
          "Skeleton loading content not gated");
    loading.set(false);
    f.synchronize();
    check(quads(f, id).empty() && f.services.components().branch_active(original.content) &&
              f.inputs.mounted_inputs()[0].editor == input.editor && slots == 1,
          "Skeleton did not reveal retained content");
    loading.set(true);
    paragraph.set(SkeletonParagraph{true, 5, SkeletonWidth::fraction(.4F)});
    f.synchronize();
    check(quads(f, id).size() == 6 && quads(f, id).back().bounds[2] == 80 && slots == 1 &&
              f.services.components().component_count() == count,
          "Skeleton structural props remounted content or missed quads");
    const auto root = f.services.components().theme_scope(id);
    auto config = std::make_unique<ThemeConfig>();
    const auto measure_count = f.nodes.require(f.services.components().root(id)).measure_count;
    config->skeleton.tokens.background = Color::rgba8(10, 20, 30);
    root->update(*config);
    f.synchronize();
    check(quads(f, id)[0].color[0] == Color::rgba8(10, 20, 30).red() &&
              f.nodes.require(f.services.components().root(id)).measure_count == measure_count && slots == 1,
          "Skeleton color update measured or remounted");
    config->skeleton.tokens.title_height = dp(20);
    root->update(*config);
    f.synchronize();
    check(quads(f, id)[0].bounds[3] == 20 && f.services.skeleton().snapshot(id).content == original.content,
          "Skeleton metrics failed or lost content identity");
    static_cast<void>(f.frames.consume_request());
    const auto requests = f.frames.counters().requests;
    f.synchronize();
    check(!f.frames.pending() && !f.services.next_frame_deadline() && f.frames.counters().requests == requests,
          "static Skeleton continually requested frames");
}

void standalone_and_empty_layout() {
    Fixture f;
    Signal<bool> loading{true};
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true).gap(dp(10)), FlexContent{[&] {
                 Skeleton(SkeletonProps{}.loading(loading));
                 SkeletonAvatar(SkeletonAvatarProps{}.size(ControlSize::Large).shape(SkeletonAvatarShape::Square));
                 SkeletonButton(SkeletonButtonProps{}.shape(SkeletonButtonShape::Round));
                 SkeletonInput(SkeletonInputProps{}.block(true));
             }});
    }});
    const auto sync = [&] {
        check(f.services.layout_and_synchronize({320, 600}, {0, 0, 320, 600}), "layout failed");
    };
    sync();
    const auto ids = std::vector(f.services.skeleton().mounted().begin(), f.services.skeleton().mounted().end());
    check(f.services.interactions().size() == 0, "placeholder created an interaction");
    check(quads(f, ids[1])[0].corner_radius == 0 && quads(f, ids[1])[0].bounds[2] == 40 &&
              quads(f, ids[2])[0].bounds[2] == 64 &&
              quads(f, ids[2])[0].corner_radius == quads(f, ids[2])[0].bounds[3] / 2 && bounds(f, ids[3]).width == 320,
          "standalone size/shape/block failed");
    const auto top = bounds(f, ids[0]).y;
    loading.set(false);
    sync();
    check(bounds(f, ids[1]).y == top && f.nodes.require(f.services.components().root(ids[0])).out_of_flow,
          "empty Skeleton retained height or sibling gap");
    loading.set(true);
    sync();
    check(bounds(f, ids[1]).y > top, "empty Skeleton did not restore");
}

void three_themes_finite_and_clip() {
    Fixture f;
    Signal<float> width{200};
    Signal<float> height{160};
    f.services.mount(Content{[&] {
        Skeleton(SkeletonProps{}.avatar(true).round(true).layout(
            LayoutStyle{}.width(bind([&] { return dp(width.get()); })).height(bind([&] { return dp(height.get()); }))));
    }});
    const auto id = f.services.skeleton().mounted()[0];
    const auto scope = f.services.components().theme_scope(id);
    auto config = std::make_unique<ThemeConfig>();
    for (const auto algorithm : {ThemeAlgorithm::Default, ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
        config->algorithms = {algorithm};
        scope->update(*config);
        for (const auto size :
             {runtime::Size{200, 160}, runtime::Size{20, 9}, runtime::Size{0, 0}, runtime::Size{200, 160}}) {
            width.set(size.width);
            height.set(size.height);
            f.synchronize();
            const auto r = bounds(f, id);
            const auto painted = quads(f, id);
            check(painted.size() == 4, "finite Skeleton lost topology");
            for (const auto& q : painted) {
                check(q.bounds[0] >= r.x && q.bounds[1] >= r.y && q.bounds[0] + q.bounds[2] <= r.x + r.width &&
                          q.bounds[1] + q.bounds[3] <= r.y + r.height && q.corner_radius >= 0 &&
                          q.corner_radius <= std::min(q.bounds[2], q.bounds[3]) / 2,
                      "Skeleton crossed finite allocation");
            }
            check(painted[1].bounds[0] >= painted[0].bounds[0] + painted[0].bounds[2],
                  "Skeleton avatar crossed body allocation");
        }
    }
    f.synchronize(320, {80, 0, 80, 100});
    check(quads(f, id)[0].opacity == 0 && f.services.rounded_effects().live_count() > 0,
          "Skeleton ancestor clip did not use common logical shapes");
    f.synchronize();
    check(quads(f, id)[0].opacity == 1, "Skeleton clipped scene did not recover");
}

void empty_slot_dynamic_recovery() {
    Fixture f;
    Signal<bool> loading{false};
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true).gap(dp(10)), FlexContent{[&] {
                 Skeleton(SkeletonProps{}.loading(loading), SkeletonContent{[] {}});
                 Button(ButtonProps{}, [] { Text(u8"Following"); });
             }});
    }});
    f.synchronize();
    const auto id = f.services.skeleton().mounted()[0];
    const auto content = f.services.skeleton().snapshot(id).content;
    const auto sibling = f.buttons.mounted_buttons()[0].component;
    const auto top = bounds(f, sibling).y;
    check(f.nodes.require(f.services.components().root(id)).out_of_flow && top == 0,
          "empty typed content slot retained a Flex gap");
    f.services.append_slot(content, Content{[] { Input(InputProps{}.defaultValue(u8"Added")); }});
    f.synchronize();
    check(!f.nodes.require(f.services.components().root(id)).out_of_flow && bounds(f, sibling).y > top + 10,
          "empty retained content region did not resume after append");
    check(f.services.destroy(f.inputs.mounted_inputs()[0].component), "dynamic input removal failed");
    f.synchronize();
    check(bounds(f, sibling).y == top && f.nodes.require(f.services.components().root(id)).out_of_flow,
          "emptied retained content region kept space");
    loading.set(true);
    f.synchronize();
    check(bounds(f, sibling).y > top, "empty content cannot show Skeleton again");
    loading.set(false);
    f.synchronize();
    check(bounds(f, sibling).y == top && f.services.skeleton().snapshot(id).content == content,
          "empty content loading round trip lost region identity");
}
} // namespace

int main() {
    try {
        scene_and_retained_content();
        standalone_and_empty_layout();
        three_themes_finite_and_clip();
        empty_slot_dynamic_recovery();
        std::cout << "Skeleton retained scene contracts passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
