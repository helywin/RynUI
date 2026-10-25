#include "support/timeline_fixture.hpp"
#include "component/select_component.hpp"
#include <string_view>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace timeline_test;

struct Ink final {
    std::size_t raw_outside{};
    std::size_t visible_outside{};
    std::size_t visible_inside{};
};

Ink ink(Fixture& f, runtime::ComponentId region) {
    Ink result;
    const auto allocated = bounds(f, region);
    const auto& atlas = f.scene.atlas();
    const auto aw = atlas.config().page_width;
    const auto ah = atlas.config().page_height;
    std::vector<detail::TextSceneId> scenes;
    for (const auto& text : f.services.text().mounted_texts()) {
        if (descendant(f, text.component, region)) {
            scenes.push_back(text.scene);
        }
    }
    for (const auto& input : f.inputs.mounted_inputs()) {
        if (descendant(f, input.component, region)) {
            const auto layers = f.inputs.text_layers(input.component);
            scenes.insert(scenes.end(), {layers.base, layers.selected, layers.placeholder});
        }
    }
    for (auto scene : scenes) {
        for (const auto& range : f.scene.primitive(scene).draw_ranges) {
            const auto bytes = atlas.page_bytes(range.atlas_page);
            for (auto i = range.instances.first; i < range.instances.first + range.instances.count; ++i) {
                const auto& g = f.scene.glyph_scene().instances().at(i);
                if (g.translation_opacity[2] <= 0 || g.color[3] <= 0) {
                    continue;
                }
                const int x0 = static_cast<int>(std::lround(g.uv_rect[0] * aw));
                const int y0 = static_cast<int>(std::lround(g.uv_rect[1] * ah));
                const int x1 = static_cast<int>(std::lround(g.uv_rect[2] * aw));
                const int y1 = static_cast<int>(std::lround(g.uv_rect[3] * ah));
                for (int y = y0; y < y1; ++y) {
                    for (int x = x0; x < x1; ++x) {
                        if (bytes[static_cast<std::size_t>(y) * aw + x] < 32) {
                            continue;
                        }
                        const auto point = graphics::glyph_vertex(
                            g, {(static_cast<float>(x - x0) + .5F) / static_cast<float>(x1 - x0),
                                (static_cast<float>(y - y0) + .5F) / static_cast<float>(y1 - y0)});
                        const bool outside = !contains(allocated, point);
                        const bool survives = point.x >= g.clip_bounds[0] && point.x <= g.clip_bounds[2] &&
                                              point.y >= g.clip_bounds[1] && point.y <= g.clip_bounds[3];
                        result.raw_outside += outside;
                        result.visible_outside += outside && survives;
                        result.visible_inside += !outside && survives;
                    }
                }
            }
        }
    }
    return result;
}

void region_ink_and_surfaces() {
    Fixture f;
    Signal<LogicalLength> width{dp(64)};
    Signal<LogicalLength> height{dp(70)};
    Signal<bool> reverse{false};
    Signal<String> text{String{u8"汉汉"}};
    Signal<ThemeConfig> config{ThemeConfig{}};
    int mounts = 0;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  Timeline(TimelineProps{}.reverse(reverse).layout(LayoutStyle{}.width(width).height(height)),
                           {TimelineItem{u8"first", u8""}
                                .richTitle(TimelineTitle{[&] {
                                    ++mounts;
                                    Text(TypographyProps{}.content(text).mark(true));
                                }})
                                .richContent(TimelineContent{[&] {
                                    ++mounts;
                                    Text(TypographyProps{}.content(text).mark(true));
                                }})
                                .icon(TimelineIcon{[&] {
                                    ++mounts;
                                    Text(TypographyProps{}.content(u8"汉").mark(true));
                                }}),
                            TimelineItem{u8"second", u8"Tail"}.title(u8"Title").tone(TimelineTone::Blue)});
              }});
    }});
    sync(f);
    const auto owner = f.services.timeline().mounted()[0];
    const auto original = f.services.timeline().snapshot(owner);
    const auto first = ink(f, original.items[0].title);
    check(first.raw_outside > 0 && first.visible_inside > 0 && first.visible_outside == 0,
          "actual title R8 ink did not respect allocated region");
    bool glyphs = false;
    bool quads = false;
    for (auto algorithm : {ThemeAlgorithm::Default, ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
        auto theme = std::make_shared<ThemeConfig>();
        theme->algorithms = {algorithm};
        config.set(*theme);
        for (auto extent : {runtime::Size{64, 70}, runtime::Size{25, 6}, runtime::Size{5, 0}, runtime::Size{0, 0},
                            runtime::Size{320, 100}}) {
            width.set(dp(extent.width));
            height.set(dp(extent.height));
            reverse.set(!reverse.get());
            sync(f);
            const auto root = bounds(f, owner);
            const auto parts = f.services.timeline().snapshot(owner);
            for (const auto& item : parts.items) {
                for (auto region : {item.title, item.content, item.icon}) {
                    check(inside(bounds(f, region), root), "allocated region escaped root");
                    const auto sample = ink(f, region);
                    check(sample.visible_outside == 0, "real R8 ink survived outside its region");
                    glyphs |= sample.visible_inside > 0;
                    const auto r = bounds(f, region);
                    for (const auto& hit : f.services.hit_test().records()) {
                        if (const auto* record = f.services.interactions().find(hit.interaction);
                            record && descendant(f, record->component, region)) {
                            check(inside(hit.effective_clip, r), "hit escaped allocated region");
                        }
                    }
                }
            }
            const auto marker =
                f.services.components().theme_scope(owner)->snapshot().typography().colors.mark_background;
            for (const auto& q : f.services.surfaces().instances().instances()) {
                if (q.opacity <= 0 || q.color[3] <= 0) {
                    continue;
                }
                quads = true;
                const runtime::Rect r{q.bounds[0] + q.translation[0], q.bounds[1] + q.translation[1], q.bounds[2],
                                      q.bounds[3]};
                check(inside(r, root), "actual Timeline surface escaped root");
                if (q.color == std::array{marker.red(), marker.green(), marker.blue(), marker.alpha()}) {
                    bool allocated = false;
                    for (const auto& item : parts.items) {
                        for (auto region : {item.title, item.content, item.icon}) {
                            allocated |= inside(r, bounds(f, region));
                        }
                    }
                    check(allocated, "actual text marker escaped all regions");
                }
            }
            check(!f.services.hit_test().hit_test({root.x + root.width + 1, root.y + 1}),
                  "hit escaped finite Timeline");
        }
    }
    check(glyphs && quads && mounts == 3 &&
              f.services.timeline().snapshot(owner).items[0].title == original.items[0].title,
          "clip recovery remounted slots or had no real scene");
}

void rich_regions_focus_and_hardclip() {
    Fixture f;
    Signal<LogicalLength> width{dp(580)};
    Signal<LogicalLength> height{dp(100)};
    InputRef ref;
    int mounts = 0;
    f.services.mount(Content{[&] {
        Timeline(TimelineProps{}.layout(LayoutStyle{}.width(width).height(height)),
                 {TimelineItem{u8"rich", u8""}
                      .richTitle(TimelineTitle{[&] {
                          ++mounts;
                          Button(ButtonProps{}.wave(false), [] { Text(u8"Title action"); });
                      }})
                      .richContent(TimelineContent{[&] {
                          ++mounts;
                          Input(InputProps{}.ref(ref).defaultValue(u8"A retained input draft"));
                      }})
                      .icon(TimelineIcon{[&] {
                          ++mounts;
                          Select(SelectProps{}.options({{1, u8"One"}}).layout(LayoutStyle{}.width(dp(100))));
                      }})});
    }});
    sync(f);
    const auto owner = f.services.timeline().mounted()[0];
    const auto input = f.inputs.mounted_inputs()[0];
    const auto button = f.buttons.mounted_buttons()[0];
    const auto natural_button = bounds(f, button.component);
    check(f.services.focus().request_focus(button.interaction, FocusModality::keyboard), "rich Button focus failed");
    sync(f);
    auto effect = std::ranges::find_if(f.services.rounded_effects().packed_instances(), [](const auto& e) {
        return e.geometry.kind == graphics::RoundedEffectKind::outline && e.geometry.outline_width == 3 &&
               e.material.opacity > 0 && e.material.visible;
    });
    check(effect != f.services.rounded_effects().packed_instances().end(), "focus outline missing");
    const runtime::Point outward{natural_button.x - 2.5F, natural_button.y + natural_button.height / 2};
    check(graphics::rounded_effect_coverage(outward, *effect) > .5F &&
              (!effect->geometry.ancestor_clip || contains(effect->geometry.ancestor_clip->bounds, outward)),
          "passive region cropped outward focus decoration");
    const runtime::Rect hard{natural_button.x, 0, 600 - natural_button.x, 800};
    check(f.services.layout_and_synchronize({600, 800}, hard), "hardclip layout failed");
    effect = std::ranges::find_if(f.services.rounded_effects().packed_instances(), [](const auto& e) {
        return e.geometry.kind == graphics::RoundedEffectKind::outline && e.geometry.outline_width == 3 &&
               e.material.opacity > 0 && e.material.visible;
    });
    check(effect != f.services.rounded_effects().packed_instances().end() && effect->geometry.ancestor_clip &&
              !contains(effect->geometry.ancestor_clip->bounds, outward),
          "outward focus ignored external hardclip");
    check(ref.focus({InputFocusCursor::All}), "rich Input focus failed");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"Retained"}, f.inputs.sessions().active()})),
          "rich draft failed");
    for (auto extent : {runtime::Size{25, 10}, runtime::Size{0, 0}, runtime::Size{580, 100}}) {
        width.set(dp(extent.width));
        height.set(dp(extent.height));
        sync(f);
        const auto parts = f.services.timeline().snapshot(owner);
        check(bounds(f, button.component).width == natural_button.width &&
                  bounds(f, button.component).height == natural_button.height,
              "Timeline shrank actual Button to hide overflow");
        for (auto region : {parts.items[0].title, parts.items[0].content, parts.items[0].icon}) {
            check(ink(f, region).visible_outside == 0, "rich control ink escaped region");
            for (const auto& hit : f.services.hit_test().records()) {
                const auto* record = f.services.interactions().find(hit.interaction);
                if (record && descendant(f, record->component, region)) {
                    check(inside(hit.effective_clip, bounds(f, region)), "rich control hit escaped region");
                }
            }
        }
    }
    check(mounts == 3 && f.inputs.mounted_inputs()[0].editor == input.editor &&
              f.inputs.editors().require(input.editor).value() == "Retained",
          "rich narrow recovery lost draft/identity");
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        region_ink_and_surfaces();
        rich_regions_focus_and_hardclip();
        std::cout << "Timeline region clips passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
