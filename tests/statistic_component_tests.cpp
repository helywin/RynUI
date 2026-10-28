#include "component/statistic_component.hpp"
#include "component/skeleton_component.hpp"
#include "component/select_component.hpp"
#include "component/modal_component.hpp"
#include "component/feedback_component.hpp"
#include "support/timeline_fixture.hpp"
#include <limits>
#include <string_view>

namespace {
using namespace timeline_test;

bool drawn(Fixture& f, detail::TextSceneId scene) {
    for (const auto& range : f.scene.primitive(scene).draw_ranges) {
        for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
            if (command.kind == graphics::SceneDrawKind::glyph &&
                command.first_instance < range.instances.first + range.instances.count &&
                range.instances.first < command.first_instance + command.instance_count) {
                return true;
            }
        }
    }
    return false;
}

void retained() {
    Fixture f;
    Signal<StatisticValue> value{StatisticValue{1234.123456789}};
    Signal<String> title{String{u8"Active users"}};
    Signal<String> prefix{String{u8"$"}};
    Signal<String> suffix{String{u8"users"}};
    Signal<bool> visible{true};
    Signal<bool> loading{false};
    Signal<LogicalLength> width{dp(480)};
    Signal<LogicalLength> height{dp(100)};
    auto config = std::make_shared<ThemeConfig>();
    Signal<ThemeConfig> theme{*config};
    InputRef ref;
    int mounts = 0;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Statistic(StatisticProps{}
                                .value(value)
                                .title(title)
                                .prefix(prefix)
                                .suffix(suffix)
                                .loading(loading)
                                .visible(visible)
                                .richValue(StatisticValueContent{[&] {
                                    ++mounts;
                                    Input(InputProps{}.ref(ref).defaultValue(u8"Original"));
                                }})
                                .layout(LayoutStyle{}.width(width).height(height)));
              }});
    }});
    sync(f);
    const auto id = f.services.statistic().mounted()[0];
    const auto original = f.services.statistic().snapshot(id);
    const auto input = f.inputs.mounted_inputs()[0];
    const auto count = f.services.components().component_count();
    check(ref.focus({InputFocusCursor::All}), "Statistic rich ref failed to focus");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"Saved draft"}, f.inputs.sessions().active()})),
          "draft commit failed");
    const auto session = f.inputs.sessions().active();
    value.set(StatisticValue{987654321.123456789});
    title.set(u8"Changed title");
    prefix.set(u8"USD");
    suffix.set(u8"accounts");
    config->statistic.tokens.content = Color::rgba8(12, 34, 56);
    theme.set(*config);
    sync(f);
    check(f.inputs.sessions().active() == session && f.inputs.editors().require(input.editor).value() == "Saved draft",
          "ordinary properties or Theme reset draft/session");
    loading.set(true);
    sync(f);
    check(!ref.focus() && !f.inputs.sessions().active().valid(), "loading left hidden input active");
    check(f.services.skeleton().snapshot(original.skeleton).loading, "loading did not reuse Skeleton");
    const auto layers = f.inputs.text_layers(input.component);
    check(!drawn(f, layers.base) && !drawn(f, layers.selected) && !drawn(f, layers.placeholder),
          "loading still drew hidden rich editor glyphs");
    check(!f.services.surfaces().instances().instances().empty(), "loading emitted no real Skeleton quads");
    check(!f.services.next_frame_deadline(), "Statistic loading introduced idle deadline");
    loading.set(false);
    visible.set(false);
    sync(f);
    check(!ref.focus(), "hidden Statistic accepted focus");
    check(f.services.scene_composer().ordered_scene().commands().empty(), "hidden Statistic still emitted draws");
    visible.set(true);
    sync(f);
    width.set(dp(0));
    height.set(dp(0));
    sync(f);
    width.set(dp(480));
    height.set(dp(100));
    sync(f);
    check(f.services.statistic().snapshot(id).value == original.value &&
              f.inputs.mounted_inputs()[0].editor == input.editor &&
              f.inputs.editors().require(input.editor).value() == "Saved draft" && mounts == 1 &&
              f.services.components().component_count() == count && ref.focus(),
          "Statistic recovery remounted content");
    const auto measures = f.nodes.require(f.services.components().root(id)).measure_count;
    config->statistic.tokens.title = Color::rgba8(89, 67, 45);
    theme.set(*config);
    sync(f);
    check(f.nodes.require(f.services.components().root(id)).measure_count == measures,
          "color-only Statistic Theme update dirtied layout");
    const auto color = *config->statistic.tokens.title;
    check(std::ranges::any_of(f.scene.glyph_scene().instances().instances(),
                              [&](const auto& glyph) {
                                  return glyph.color ==
                                             std::array{color.red(), color.green(), color.blue(), color.alpha()} &&
                                         glyph.translation_opacity[2] > 0;
                              }),
          "Statistic Theme color did not reach real glyphs");
    config->statistic.tokens.content_font_size = dp(28);
    config->statistic.tokens.title_font_size = dp(16);
    theme.set(*config);
    sync(f);
    check(f.inputs.editors().require(input.editor).value() == "Saved draft" && ref.focus(),
          "metric Theme update reset rich draft/ref");
    check(f.services.destroy(id), "Statistic destruction failed");
    sync(f);
    check(!ref.focus() && f.services.statistic().mounted().empty() && f.services.surfaces().size() == 0 &&
              f.services.interactions().declaration_order().empty() && blocked_roots(f.services.interactions()).empty(),
          "Statistic teardown leaked resources");
}

void font_limit_recovery() {
    unsigned failures = 0;
    for (bool title : {true, false}) {
        Fixture f;
        f.services.mount(Content{[] { Statistic(StatisticProps{}.title(u8"Title").value(42)); }});
        sync(f);
        const auto id = f.services.statistic().mounted()[0];
        const auto scope = f.services.components().theme_scope(id);
        const auto* before = &scope->snapshot();
        const auto generation = scope->generation();
        int notifications = 0;
        auto subscription =
            scope->capture([&](auto) { ++notifications; }, [&] { static_cast<void>(scope->statistic_metrics()); });
        auto config = std::make_unique<ThemeConfig>();
        auto& font = title ? config->statistic.tokens.title_font_size : config->statistic.tokens.content_font_size;
        font = dp(100000000);
        bool rejected = false;
        try {
            scope->update(*config);
            sync(f);
        } catch (const std::invalid_argument&) {
            rejected = true;
        } catch (const std::exception& e) {
            std::cerr << (title ? "title" : "content") << " post-publication exception: " << e.what()
                      << "; snapshot changed=" << (&scope->snapshot() != before) << '\n';
        }
        if (!rejected || &scope->snapshot() != before || scope->generation() != generation || notifications != 0) {
            ++failures;
            std::cerr << (title ? "title" : "content") << " oversized font was not atomically rejected\n";
        }
        notifications = 0;
        font = dp(title ? 16.0F : 28.0F);
        scope->update(*config);
        sync(f);
        check(notifications == 1 && f.services.components().contains(id) &&
                  f.scene.glyph_scene().instances().size() > 0,
              "valid font did not recover retained scene");
    }
    check(failures == 0, "Statistic font limits failed before publication");
}

void first_mount_updates() {
    unsigned failures = 0;
    for (int changed = 0; changed < 6; ++changed) {
        Fixture f;
        Signal<StatisticValue> value{StatisticValue{1234.1234}};
        Signal<std::optional<int>> precision{4};
        Signal<String> decimal{String{u8"."}};
        Signal<String> group{String{u8","}};
        Signal<int> formatter_version{0};
        InputRef ref;
        int formats = 0;
        try {
            const auto prop = [changed]<typename T>(Signal<T> signal) -> Prop<T> {
                return changed == 5 ? Prop<T>{bind([signal] { return signal.get(); })} : Prop<T>{signal};
            };
            f.services.mount(Content{[&] {
                Statistic(StatisticProps{}
                              .value(prop(value))
                              .precision(prop(precision))
                              .decimalSeparator(prop(decimal))
                              .groupSeparator(prop(group))
                              .formatter(bind([&]() -> StatisticFormatter {
                                  if (formatter_version.get() == 0) {
                                      return {};
                                  }
                                  return [&](const StatisticValue&) {
                                      ++formats;
                                      return String{u8"Updated"};
                                  };
                              }))
                              .richTitle(StatisticTitle{[&] {
                                  if (changed == 0 || changed == 5) {
                                      value.set(StatisticValue{9876.54321});
                                  }
                                  if (changed == 1 || changed == 5) {
                                      precision.set(2);
                                  }
                                  if (changed == 2 || changed == 5) {
                                      decimal.set(u8";");
                                  }
                                  if (changed == 3 || changed == 5) {
                                      group.set(u8"_");
                                  }
                                  if (changed == 4) {
                                      formatter_version.set(1);
                                  }
                                  Text(u8"Updates during composition");
                              }})
                              .richValue(StatisticValueContent{
                                  [&] { Input(InputProps{}.ref(ref).defaultValue(u8"Retained")); }}));
            }});
            sync(f);
            const auto id = f.services.statistic().mounted()[0];
            constexpr std::array expected{"9,876.5432", "1,234.12", "1,234;1234", "1_234.1234", "Updated", "9_876;54"};
            check(f.services.statistic().snapshot(id).formatted_value.bytes() == expected[changed],
                  "first frame lost a property update made in richTitle");
            check(formats == (changed == 4 ? 1 : 0), "first mount called formatter unnecessarily");
            check(ref.focus({InputFocusCursor::All}) &&
                      bool(f.inputs.dispatch(TextCommitted{String{u8"Saved"}, f.inputs.sessions().active()})),
                  "first-mount update lost rich input");
            formatter_version.set(0);
            precision.set(3);
            decimal.set(u8".");
            group.set(u8",");
            value.set(StatisticValue{12.375});
            sync(f);
            check(f.services.statistic().snapshot(id).formatted_value.bytes() == "12.375",
                  "valid update after first mount failed");
            bool rejected = false;
            try {
                value.set(StatisticValue{std::numeric_limits<double>::infinity()});
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            check(rejected && f.services.statistic().snapshot(id).formatted_value.bytes() == "12.375",
                  "invalid update published after first-mount reconciliation");
            value.set(StatisticValue{24.625});
            sync(f);
            check(f.services.statistic().snapshot(id).formatted_value.bytes() == "24.625" && ref.focus() &&
                      f.inputs.editors().require(f.inputs.mounted_inputs()[0].editor).value() == "Saved",
                  "rejected update did not recover retained draft/ref");
        } catch (const std::exception& e) {
            ++failures;
            std::cerr << "first-mount changed=" << changed << ": " << e.what() << '\n';
        }
    }
    check(failures == 0, "Statistic first-mount property reconciliation failed");

    Fixture stable;
    Signal<StatisticValue> stable_value{StatisticValue{1}};
    int formats = 0;
    stable.services.mount(Content{[&] {
        Statistic(StatisticProps{}.value(stable_value).formatter([&](const StatisticValue&) {
            ++formats;
            return String{u8"Stable"};
        }));
    }});
    sync(stable);
    check(formats == 1, "unchanged first mount formatted more than once");

    Fixture revised;
    Signal<StatisticValue> revised_value{StatisticValue{1}};
    int revisions = 0;
    revised.services.mount(Content{[&] {
        Statistic(StatisticProps{}.value(revised_value).formatter([&](const StatisticValue& v) {
            ++revisions;
            if (std::get<std::int64_t>(v.storage()) == 1) {
                revised_value.set(StatisticValue{2});
                return String{u8"Obsolete"};
            }
            return String{u8"Latest"};
        }));
    }});
    sync(revised);
    check(
        revisions == 2 &&
            revised.services.statistic().snapshot(revised.services.statistic().mounted()[0]).formatted_value.bytes() ==
                "Latest",
        "formatter revision published the obsolete first model");
}

void formatting() {
    first_mount_updates();
    Fixture f;
    Signal<StatisticValue> value{StatisticValue{u8"-1234.123456789"}};
    Signal<std::optional<int>> precision{8};
    Signal<String> separator{String{u8","}};
    Signal<int> format{0};
    f.services.mount(Content{[&] {
        Statistic(StatisticProps{}.value(value).precision(precision).groupSeparator(separator).formatter(bind([&] {
            return format.get() ? StatisticFormatter{[](const StatisticValue&) { return String{u8"Custom"}; }}
                                : StatisticFormatter{};
        })));
    }});
    sync(f);
    const auto id = f.services.statistic().mounted()[0];
    check(f.services.statistic().snapshot(id).formatted_value.bytes() == "-1,234.12345678",
          "initial displayed precision incorrect");
    precision.set(-1);
    separator.set(u8"__");
    sync(f);
    check(f.services.statistic().snapshot(id).formatted_value.bytes() == "-1__234",
          "negative precision mapping incorrect");
    format.set(1);
    sync(f);
    check(f.services.statistic().snapshot(id).formatted_value.bytes() == "Custom",
          "formatter did not reach retained text");
    const auto before = f.services.statistic().snapshot(id).formatted_value;
    bool rejected = false;
    try {
        value.set(StatisticValue{std::numeric_limits<double>::infinity()});
        sync(f);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && f.services.statistic().snapshot(id).formatted_value == before,
          "invalid number partially published");
    value.set(StatisticValue{std::numeric_limits<std::uint64_t>::max()});
    format.set(0);
    precision.set(std::nullopt);
    separator.set(u8",");
    sync(f);
    check(f.services.statistic().snapshot(id).formatted_value.bytes() == "18,446,744,073,709,551,615",
          "invalid update did not recover");
    check(f.scene.glyph_scene().instances().size() > 0, "formatted value emitted no glyphs");
    font_limit_recovery();
}

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
    for (const auto& text : f.services.text().mounted_texts()) {
        if (!descendant(f, text.component, region)) {
            continue;
        }
        for (const auto& range : f.scene.primitive(text.scene).draw_ranges) {
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

void clips() {
    Fixture f;
    Signal<LogicalLength> width{dp(60)};
    Signal<LogicalLength> height{dp(65)};
    Signal<bool> loading{false};
    int mounts = 0;
    f.services.mount(Content{[&] {
        Statistic(StatisticProps{}
                      .loading(loading)
                      .layout(LayoutStyle{}.width(width).height(height))
                      .richTitle(StatisticTitle{[&] {
                          ++mounts;
                          Text(TypographyProps{}.content(u8"汉汉汉汉汉汉").mark(true));
                      }})
                      .richPrefix(StatisticPrefix{[&] {
                          ++mounts;
                          Text(TypographyProps{}.content(u8"前").mark(true));
                      }})
                      .richValue(StatisticValueContent{[&] {
                          ++mounts;
                          Text(TypographyProps{}.content(u8"汉汉汉汉汉汉").mark(true));
                      }})
                      .richSuffix(StatisticSuffix{[&] {
                          ++mounts;
                          Button(ButtonProps{}.wave(false), [] { Text(u8"Suffix"); });
                      }}));
    }});
    sync(f);
    const auto id = f.services.statistic().mounted()[0];
    const auto original = f.services.statistic().snapshot(id);
    check(ink(f, original.title).raw_outside > 0 && ink(f, original.title).visible_inside > 0,
          "clip fixture has no actual overflowing R8 ink");
    bool glyphs = false;
    bool quads = false;
    bool fills = false;
    for (auto extent : {runtime::Size{60, 65}, runtime::Size{24, 6}, runtime::Size{5, 0}, runtime::Size{0, 0},
                        runtime::Size{480, 100}}) {
        width.set(dp(extent.width));
        height.set(dp(extent.height));
        sync(f);
        const auto root = bounds(f, id);
        quads |= !f.services.surfaces().instances().instances().empty();
        const auto parts = f.services.statistic().snapshot(id);
        for (auto region : {parts.title, parts.prefix, parts.value, parts.suffix}) {
            check(inside(bounds(f, region), root), "region escaped finite Statistic");
            const auto sample = ink(f, region);
            check(sample.visible_outside == 0, "actual R8 ink survived outside region");
            glyphs |= sample.visible_inside > 0;
            for (const auto& hit : f.services.hit_test().records()) {
                if (const auto* record = f.services.interactions().find(hit.interaction);
                    record && descendant(f, record->component, region)) {
                    check(inside(hit.effective_clip, bounds(f, region)), "hit escaped region");
                }
            }
        }
        for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
            if (command.kind != graphics::SceneDrawKind::quad) {
                continue;
            }
            for (auto index = command.first_instance; index < command.first_instance + command.instance_count;
                 ++index) {
                const auto& q = f.services.surfaces().instances().instances()[index];
                if (q.opacity <= 0 || q.color[3] <= 0) {
                    continue;
                }
                quads = true;
                check(inside({q.bounds[0] + q.translation[0], q.bounds[1] + q.translation[1], q.bounds[2], q.bounds[3]},
                             root),
                      "real quad escaped Statistic");
            }
        }
        for (const auto& effect : f.services.rounded_effects().packed_instances()) {
            if (!effect.material.visible || effect.material.opacity <= 0 || effect.material.color.alpha() <= 0) {
                continue;
            }
            check(effect.geometry.ancestor_clip && inside(effect.geometry.ancestor_clip->bounds, root),
                  "clipped fill/effect escaped root");
            for (auto region : {parts.title, parts.prefix, parts.value, parts.suffix}) {
                const auto r = bounds(f, region);
                const runtime::Point center{r.x + r.width / 2, r.y + r.height / 2};
                fills |= graphics::rounded_effect_coverage(center, effect) > .1F;
            }
            check(graphics::rounded_effect_coverage({root.x + root.width + 1, root.y + 1}, effect) == 0,
                  "actual effect coverage escaped root");
        }
        check(!f.services.hit_test().hit_test({root.x + root.width + 1, root.y + 1}), "hit escaped root");
    }
    check(glyphs && quads && fills, "clip fixture emitted no real glyph/quad/fill coverage");
    check(mounts == 4 && f.services.statistic().snapshot(id).value == original.value, "clip recovery remounted slots");
    const auto root = bounds(f, id);
    const runtime::Rect document{root.x, root.y, root.width / 2, root.height / 2};
    check(f.services.layout_and_synchronize({600, 800}, document), "document clip sync failed");
    for (const auto& hit : f.services.hit_test().records()) {
        check(inside(hit.effective_clip, document), "document interaction clip ignored");
    }
    for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
        if (command.kind != graphics::SceneDrawKind::glyph) {
            continue;
        }
        for (auto index = command.first_instance; index < command.first_instance + command.instance_count; ++index) {
            const auto& g = f.scene.glyph_scene().instances().at(index);
            check(inside({g.clip_bounds[0], g.clip_bounds[1], g.clip_bounds[2] - g.clip_bounds[0],
                          g.clip_bounds[3] - g.clip_bounds[1]},
                         document),
                  "glyph document clip ignored");
        }
    }
    for (const auto& effect : f.services.rounded_effects().packed_instances()) {
        if (effect.material.visible && effect.material.opacity > 0) {
            check(effect.geometry.ancestor_clip && inside(effect.geometry.ancestor_clip->bounds, document),
                  "fill/effect document clip ignored");
        }
    }
    loading.set(true);
    sync(f);
    check(f.services.skeleton().snapshot(original.skeleton).loading, "loading skeleton not live");
}

void initial_mount_retirement() {
    unsigned failures = 0;
    for (bool loading : {false, true}) {
        for (bool replace : {false, true}) {
            Fixture f;
            InputRef ref;
            Signal<StatisticValue> value{StatisticValue{1}};
            Signal<String> prefix{String{u8"Old"}};
            runtime::ComponentId retired;
            runtime::ComponentId replacement;
            runtime::ComponentId parent;
            bool premature = false;
            int closes = 0;
            try {
                f.services.mount(Content{[&] {
                    Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                             Statistic(StatisticProps{}
                                           .visible(loading)
                                           .loading(loading)
                                           .value(value)
                                           .prefix(prefix)
                                           .richValue(StatisticValueContent{[&] {
                                               Input(InputProps{}.ref(ref).defaultValue(u8"Old"));
                                               Select(SelectProps{}.open(true).onOpenChange([&](bool open) {
                                                   if (open) {
                                                       return;
                                                   }
                                                   ++closes;
                                                   if (f.services.statistic().mounted().empty()) {
                                                       premature = true;
                                                       return;
                                                   }
                                                   retired = f.services.statistic().mounted().back();
                                                   parent = *f.services.components().parent(retired);
                                                   check(f.services.destroy(retired),
                                                         "first close could not destroy Statistic");
                                               }));
                                           }}));
                         }});
                }});
                sync(f);
                check(!premature && closes == 1 && retired.valid() && !f.services.components().contains(retired),
                      "first close ran before owner was fully registered or retained the owner");
                if (replace) {
                    // The host intentionally forbids dynamic slots while its initial mount is active.
                    f.services.append_slot(parent, Content{[&] {
                                               Statistic(StatisticProps{}.richValue(StatisticValueContent{[&] {
                                                   Input(InputProps{}.ref(ref).defaultValue(u8"New generation"));
                                               }}));
                                           }});
                    replacement = f.services.statistic().mounted().back();
                }
                value.set(StatisticValue{2});
                prefix.set(u8"Stale update");
                sync(f);
                if (replace) {
                    check(replacement != retired && f.services.components().contains(replacement) && ref.focus() &&
                              f.inputs.editors().require(f.inputs.mounted_inputs()[0].editor).value() ==
                                  "New generation" &&
                              f.services.statistic().snapshot(replacement).formatted_value.bytes() == "0",
                          "initial retirement corrupted rebound ref or replacement generation");
                } else {
                    check(f.services.statistic().mounted().empty() && !ref.focus(),
                          "initial retirement left live owner/ref");
                }
                check(blocked_roots(f.services.interactions()).empty(), "initial retirement leaked input blocks");
                f.services.dispose();
                check(!ref.focus() && f.services.surfaces().size() == 0 &&
                          blocked_roots(f.services.interactions()).empty(),
                      "initial retirement cleanup leaked resources");
            } catch (const std::exception& e) {
                ++failures;
                std::cerr << "initial " << (loading ? "loading" : "hidden") << " replace=" << replace << ": "
                          << e.what() << '\n';
            }
        }
    }
    check(failures == 0, "Statistic initial-mount retirement failed");
}

void blocked_ancestor_mount() {
    unsigned failures = 0;
    for (bool modal : {false, true}) {
        for (int retirement : {0, 1, 2}) {
            if (modal && retirement == 2) {
                continue;
            }
            const bool retire = retirement != 0;
            Fixture f;
            Signal<bool> ancestor{!modal};
            Signal<bool> loading{true};
            Signal<bool> open{true};
            Signal<StatisticValue> value{StatisticValue{1}};
            InputRef title_ref;
            InputRef value_ref;
            runtime::ComponentId owner;
            runtime::ComponentId parent;
            int closes = 0;
            int bodies = 0;
            int bodies_at_close = 0;
            try {
                const auto statistic = [&] {
                    Statistic(
                        StatisticProps{}
                            .loading(loading)
                            .value(value)
                            .richTitle(StatisticTitle{[&] {
                                Input(InputProps{}.ref(title_ref).defaultValue(u8"Original title"));
                                Select(SelectProps{}.options({{1, u8"One"}}).open(open).onOpenChange([&](bool shown) {
                                    if (shown) {
                                        return;
                                    }
                                    ++closes;
                                    const auto select = f.services.select().mounted().back();
                                    const auto title = *f.services.components().parent(select);
                                    owner = *f.services.components().parent(title);
                                    parent = *f.services.components().parent(owner);
                                    bodies_at_close = bodies;
                                    if (retirement == 1) {
                                        check(f.services.destroy(owner), "blocked first close failed to retire owner");
                                    } else if (retirement == 2) {
                                        const auto child = f.services.components().children(owner).back();
                                        check(child != title && f.services.destroy(child),
                                              "blocked first close failed to retire private Skeleton");
                                    }
                                }));
                            }})
                            .richValue(StatisticValueContent{[&] {
                                ++bodies;
                                Input(InputProps{}.ref(value_ref).defaultValue(u8"Original value"));
                            }}));
                };
                f.services.mount(Content{[&] {
                    if (modal) {
                        Modal(ModalProps{}.open(ancestor).footer(ModalFooterMode::None), ModalContent{statistic});
                    } else {
                        Skeleton(SkeletonProps{}.loading(ancestor), SkeletonContent{statistic});
                    }
                }});
                sync(f);
                check(closes == 1 && owner.valid(), "blocked ancestor did not close controlled title Select once");
                check(!title_ref.focus() && !value_ref.focus(), "blocked ancestor admitted input");
                if (retire) {
                    check(!f.services.components().contains(owner) && f.services.statistic().mounted().empty() &&
                              bodies == bodies_at_close,
                          "mount continued old body after owner retirement");
                    const auto old = owner;
                    f.services.append_slot(
                        parent, Content{[&] {
                            Statistic(StatisticProps{}
                                          .richTitle(StatisticTitle{[&] {
                                              Input(InputProps{}.ref(title_ref).defaultValue(u8"Replacement title"));
                                          }})
                                          .richValue(StatisticValueContent{[&] {
                                              Input(InputProps{}.ref(value_ref).defaultValue(u8"Replacement value"));
                                          }}));
                        }});
                    owner = f.services.statistic().mounted().back();
                    check(owner != old && !f.services.components().contains(old), "retired generation remained active");
                } else {
                    check(f.services.components().contains(owner) && bodies == 1, "surviving mount lost rich content");
                }
                for (auto blocked : blocked_roots(f.services.interactions())) {
                    check(f.services.components().contains(blocked), "mount leaked a stale generational input block");
                }
                ancestor.set(modal);
                loading.set(false);
                value.set(StatisticValue{2});
                sync(f);
                check(title_ref.focus() && value_ref.focus(), "blocked ancestor recovery lost retained/rebound refs");
                const auto expected =
                    retire ? std::string_view{"Replacement value"} : std::string_view{"Original value"};
                check(f.inputs.editors().require(f.inputs.mounted_inputs().back().editor).value() == expected,
                      "ancestor recovery reset rich draft/ref generation");
                if (retire) {
                    check(f.services.statistic().snapshot(owner).formatted_value.bytes() == "0",
                          "old value binding reached replacement generation");
                } else {
                    const auto select = f.services.select().mounted().back();
                    check(!f.services.select().snapshot(select).visible,
                          "blocked controlled refusal resurrected popup");
                    open.set(false);
                    open.set(true);
                    sync(f);
                    check(f.services.select().snapshot(select).visible, "surviving controlled Select could not reopen");
                }
                f.services.dispose();
                check(!title_ref.focus() && !value_ref.focus() && f.services.surfaces().size() == 0 &&
                          blocked_roots(f.services.interactions()).empty(),
                      "blocked composition cleanup leaked resources");
            } catch (const std::exception& e) {
                ++failures;
                std::cerr << "blocked " << (modal ? "Modal" : "Skeleton") << " retirement=" << retirement << ": "
                          << e.what() << '\n';
            }
        }
    }
    check(failures == 0, "Statistic blocked-ancestor first mount failed");
}

void rich_region_retirement() {
    enum class Target { Survive, Region, Skeleton, Wrapper, Content, Owner };

    struct Case final {
        std::size_t region;
        Target target;
        bool rebind{};
    };

    const std::array cases{Case{0, Target::Region},   Case{1, Target::Region},      Case{2, Target::Region},
                           Case{3, Target::Region},   Case{1, Target::Skeleton},    Case{2, Target::Skeleton},
                           Case{3, Target::Skeleton}, Case{1, Target::Wrapper},     Case{2, Target::Content},
                           Case{1, Target::Owner},    Case{2, Target::Owner, true}, Case{1, Target::Skeleton, true},
                           Case{0, Target::Survive},  Case{1, Target::Survive},     Case{2, Target::Survive},
                           Case{3, Target::Survive}};
    unsigned failures = 0;
    for (const auto test : cases) {
        Fixture f;
        Signal<bool> ancestor{test.region == 0};
        Signal<bool> loading{true};
        Signal<StatisticValue> value{StatisticValue{1}};
        InputRef ref;
        std::array<int, 4> mounts{};
        runtime::ComponentId original;
        runtime::ComponentId replacement;
        runtime::ComponentId retired_target;
        runtime::ComponentId outside;
        int closes = 0;
        const auto replace = [&] {
            Statistic(StatisticProps{}.value(99).richValue(
                StatisticValueContent{[&] { Input(InputProps{}.ref(ref).defaultValue(u8"Replacement")); }}));
            replacement = f.services.statistic().mounted().back();
        };
        try {
            const auto body = [&](std::size_t index) {
                ++mounts[index];
                if (index != test.region) {
                    Text(u8"Stable");
                    return;
                }
                Input(InputProps{}.ref(ref).defaultValue(u8"Original"));
                Select(SelectProps{}.open(true).onOpenChange([&](bool open) {
                    if (open) {
                        return;
                    }
                    ++closes;
                    const auto select = f.services.select().mounted().back();
                    const auto region = *f.services.components().parent(select);
                    runtime::ComponentId content;
                    runtime::ComponentId wrapper;
                    runtime::ComponentId skeleton;
                    if (test.region == 0) {
                        original = *f.services.components().parent(region);
                    } else {
                        content = *f.services.components().parent(region);
                        wrapper = *f.services.components().parent(content);
                        skeleton = *f.services.components().parent(wrapper);
                        original = *f.services.components().parent(skeleton);
                    }
                    outside = *f.services.components().parent(original);
                    runtime::ComponentId victim;
                    switch (test.target) {
                    case Target::Survive:
                        break;
                    case Target::Region:
                        victim = region;
                        break;
                    case Target::Skeleton:
                        victim = skeleton;
                        break;
                    case Target::Wrapper:
                        victim = wrapper;
                        break;
                    case Target::Content:
                        victim = content;
                        break;
                    case Target::Owner:
                        victim = original;
                        break;
                    }
                    if (victim.valid()) {
                        retired_target = victim;
                        check(f.services.destroy(victim), "rich-region callback failed to retire target");
                    }
                    if (test.rebind) {
                        // Explicit mount_slot is valid during the initial host mount; append_slot is not.
                        runtime::require_component_build_context().mount_slot(outside, Content{replace});
                    }
                }));
                Skeleton(SkeletonProps{}.loading(true), SkeletonContent{[] { Text(u8"Trigger"); }});
            };
            f.services.mount(Content{[&] {
                Skeleton(SkeletonProps{}.loading(ancestor), SkeletonContent{[&] {
                             Statistic(StatisticProps{}
                                           .loading(loading)
                                           .value(value)
                                           .richTitle(StatisticTitle{[&] { body(0); }})
                                           .richPrefix(StatisticPrefix{[&] { body(1); }})
                                           .richValue(StatisticValueContent{[&] { body(2); }})
                                           .richSuffix(StatisticSuffix{[&] { body(3); }}));
                         }});
            }});
            sync(f);
            check(closes == 1 && original.valid(), "rich-region retirement callback did not run once");
            const bool survives = test.target == Target::Survive;
            if (survives) {
                check(f.services.components().contains(original) && mounts == std::array{1, 1, 1, 1},
                      "surviving rich composition was not completed once");
            } else {
                check(!f.services.components().contains(original),
                      "invalid private structure left original owner alive");
                for (std::size_t i = test.region + 1; i < mounts.size(); ++i) {
                    check(mounts[i] == 0, "mount continued a later region after retirement");
                }
                if (!test.rebind) {
                    f.services.append_slot(outside, Content{replace});
                }
                check(replacement != original && f.services.components().contains(replacement),
                      "old generation cleanup destroyed callback replacement");
                if (test.rebind) {
                    check(replacement.index == retired_target.index &&
                              replacement.generation != retired_target.generation,
                          "callback replacement did not exercise same-index generation reuse");
                }
            }
            for (auto blocked : blocked_roots(f.services.interactions())) {
                check(f.services.components().contains(blocked), "rich retirement leaked old generation block");
            }
            ancestor.set(false);
            loading.set(false);
            value.set(StatisticValue{2});
            sync(f);
            check(ref.focus({InputFocusCursor::All}), "rich retirement recovery lost retained/rebound ref");
            const auto input = f.inputs.mounted_inputs()[0];
            check(f.inputs.editors().require(input.editor).value() == (survives ? "Original" : "Replacement"),
                  "rich retirement recovery reset draft or ref generation");
            check(bool(f.inputs.dispatch(TextCommitted{String{u8"Recovered draft"}, f.inputs.sessions().active()})),
                  "rich retirement recovery rejected real input");
            if (!survives) {
                check(f.services.statistic().snapshot(replacement).formatted_value.bytes() == "99",
                      "old value binding mutated callback replacement");
            }
            f.services.dispose();
            check(!ref.focus() && blocked_roots(f.services.interactions()).empty() && f.services.surfaces().size() == 0,
                  "rich retirement teardown leaked resources");
        } catch (const std::exception& e) {
            ++failures;
            std::cerr << "rich region=" << test.region << " target=" << static_cast<int>(test.target)
                      << " rebind=" << test.rebind << ": " << e.what() << '\n';
        }
    }
    check(failures == 0, "Statistic rich-region generation validation failed");
}

void final_mount_boundary() {
    unsigned failures = 0;
    // -1: Skeleton; 0..3: region; 4: owner; 5/6: Skeleton/owner rebind; 7: survive; 8: staged loading.
    for (int target = -1; target < 9; ++target) {
        Fixture f;
        Signal<bool> visible{target == 8};
        Signal<bool> loading{false};
        Signal<StatisticValue> value{StatisticValue{1}};
        InputRef ref;
        runtime::ComponentId original;
        runtime::ComponentId replacement;
        runtime::ComponentId retired;
        int closes = 0;
        try {
            f.services.mount(Content{[&] {
                Flex(FlexProps{}, FlexContent{[&] {
                         Statistic(StatisticProps{}.value(value).visible(visible).loading(loading).richValue(
                             StatisticValueContent{[&] {
                                 Input(InputProps{}.ref(ref).defaultValue(u8"Original"));
                                 Select(SelectProps{}.open(true).onOpenChange([&](bool open) {
                                     if (open) {
                                         return;
                                     }
                                     ++closes;
                                     original = f.services.statistic().mounted().back();
                                     const auto parent = *f.services.components().parent(original);
                                     const auto snapshot = f.services.statistic().snapshot(original);
                                     const std::array regions{snapshot.title, snapshot.prefix, snapshot.value,
                                                              snapshot.suffix};
                                     retired = target == -1 || target == 5 || target == 8 ? snapshot.skeleton
                                               : target < 4                               ? regions[target]
                                               : target == 7                              ? runtime::ComponentId{}
                                                                                          : original;
                                     if (retired.valid()) {
                                         check(f.services.destroy(retired), "final boundary failed to retire target");
                                     }
                                     if (target == 5 || target == 6) {
                                         runtime::require_component_build_context().mount_slot(
                                             parent, Content{[&] {
                                                 Statistic(
                                                     StatisticProps{}.value(99).richValue(StatisticValueContent{[&] {
                                                         Input(InputProps{}.ref(ref).defaultValue(u8"Replacement"));
                                                     }}));
                                                 replacement = f.services.statistic().mounted().back();
                                             }});
                                     }
                                 }));
                                 if (target == 8) {
                                     loading.set(true);
                                 }
                             }}));
                     }});
            }});
            // This was the reported post-mount crash when only private content was retired.
            visible.set(true);
            value.set(StatisticValue{2});
            sync(f);
            check(closes == 1 && original.valid(), "final boundary did not close controlled Select once");
            check(f.services.components().contains(original) == (target == 7),
                  "final refresh left an incomplete owner alive");
            if (replacement.valid()) {
                check(replacement.index == retired.index && replacement.generation != retired.generation &&
                          f.services.components().contains(replacement) && ref.focus() &&
                          f.services.statistic().snapshot(replacement).formatted_value.bytes() == "99",
                      "final boundary cleanup harmed callback replacement generation");
            } else if (target == 7) {
                check(ref.focus(), "surviving final boundary could not recover input");
            } else {
                check(f.services.statistic().mounted().empty() && !ref.focus(), "final boundary leaked owner/ref");
            }
            f.services.dispose();
            check(blocked_roots(f.services.interactions()).empty() && f.services.surfaces().size() == 0 && !ref.focus(),
                  "final boundary teardown leaked resources");
        } catch (const std::exception& e) {
            ++failures;
            std::cerr << "final boundary target=" << target << ": " << e.what() << '\n';
        }
    }
    check(failures == 0, "Statistic final first-mount boundary failed");
}

void first_mount_callbacks() {
    enum class Trigger { Formatter, ValueGetter, VisibleGetter, LayoutGetter, TextGetter, LoadingGetter };
    enum class Target { Owner, Skeleton, Region };

    struct Case final {
        Trigger trigger;
        Target target;
        bool later{};
    };

    const std::array cases{
        Case{Trigger::Formatter, Target::Owner},         Case{Trigger::Formatter, Target::Skeleton},
        Case{Trigger::Formatter, Target::Region},        Case{Trigger::ValueGetter, Target::Owner},
        Case{Trigger::ValueGetter, Target::Skeleton},    Case{Trigger::VisibleGetter, Target::Owner},
        Case{Trigger::LayoutGetter, Target::Owner},      Case{Trigger::TextGetter, Target::Region},
        Case{Trigger::LoadingGetter, Target::Skeleton},  Case{Trigger::Formatter, Target::Skeleton, true},
        Case{Trigger::ValueGetter, Target::Region, true}};
    unsigned failures = 0;
    for (const auto test : cases) {
        Fixture f;
        Signal<StatisticValue> value{StatisticValue{1}};
        Signal<int> loading_tick{0};
        InputRef ref;
        runtime::ComponentId owner;
        runtime::ComponentId outside;
        runtime::ComponentId replacement;
        runtime::ComponentId retired;
        bool armed = false;
        bool fired = false;
        try {
            const auto retire = [&] {
                if (!armed || fired) {
                    return;
                }
                fired = true;
                if (!owner.valid()) {
                    owner = f.services.components().children(outside).back();
                }
                if (test.target == Target::Owner) {
                    retired = owner;
                } else {
                    const auto snapshot = f.services.statistic().snapshot(owner);
                    retired = test.target == Target::Skeleton ? snapshot.skeleton : snapshot.value;
                }
                check(f.services.destroy(retired), "getter/formatter could not retire target");
                runtime::require_component_build_context().mount_slot(
                    outside, Content{[&] {
                        Statistic(StatisticProps{}.value(99).richValue(StatisticValueContent{
                            [&] { Input(InputProps{}.ref(ref).defaultValue(u8"Replacement")); }}));
                        replacement = f.services.statistic().mounted().back();
                    }});
            };
            f.services.mount(Content{[&] {
                Flex(FlexProps{}, FlexContent{[&] {
                         outside = f.services.components().root_components().back();
                         armed = test.trigger == Trigger::VisibleGetter || test.trigger == Trigger::LayoutGetter;
                         Statistic(StatisticProps{}
                                       .value(bind([&] {
                                           const auto current = value.get();
                                           if (test.trigger == Trigger::ValueGetter) {
                                               retire();
                                           }
                                           return current;
                                       }))
                                       .formatter([&](const StatisticValue&) {
                                           if (test.trigger == Trigger::Formatter) {
                                               retire();
                                           }
                                           return String{u8"Formatted"};
                                       })
                                       .visible(bind([&] {
                                           if (test.trigger == Trigger::VisibleGetter) {
                                               retire();
                                           }
                                           return true;
                                       }))
                                       .loading(bind([&] {
                                           static_cast<void>(loading_tick.get());
                                           if (test.trigger == Trigger::LoadingGetter) {
                                               retire();
                                           }
                                           return false;
                                       }))
                                       .title(bind([&] {
                                           if (test.trigger == Trigger::TextGetter) {
                                               retire();
                                           }
                                           return String{u8"Title"};
                                       }))
                                       .layout(LayoutStyle{}.width(bind([&] {
                                           if (test.trigger == Trigger::LayoutGetter) {
                                               retire();
                                           }
                                           return dp(480);
                                       })))
                                       .richTitle(StatisticTitle{[&] {
                                           Text(u8"Title");
                                           const auto title = *f.services.components().parent(
                                               f.services.text().mounted_texts().back().component);
                                           owner = *f.services.components().parent(title);
                                           armed = !test.later && test.trigger != Trigger::LoadingGetter;
                                       }})
                                       .richValue(StatisticValueContent{[&] {
                                           Input(InputProps{}.ref(ref).defaultValue(u8"Original"));
                                           if (test.trigger == Trigger::LoadingGetter) {
                                               armed = true;
                                               loading_tick.set(1);
                                           }
                                       }}));
                         if (test.later) {
                             // Keep a real build context for callback rebind while exercising a ready owner.
                             armed = true;
                             value.set(StatisticValue{2});
                         }
                     }});
            }});
            sync(f);
            value.set(StatisticValue{3});
            loading_tick.set(2);
            sync(f);
            check(fired && !f.services.components().contains(owner) && f.services.statistic().mounted().size() == 1 &&
                      replacement.index == retired.index && replacement.generation != retired.generation &&
                      f.services.components().contains(replacement),
                  "getter/formatter cleanup harmed replacement generation or kept incomplete owner");
            check(f.services.statistic().snapshot(replacement).formatted_value.bytes() == "99" && ref.focus() &&
                      f.inputs.editors().require(f.inputs.mounted_inputs()[0].editor).value() == "Replacement",
                  "getter/formatter callback lost rebound draft/ref");
            f.services.dispose();
            check(!ref.focus() && blocked_roots(f.services.interactions()).empty() && f.services.surfaces().size() == 0,
                  "getter/formatter retirement leaked resources");
        } catch (const std::exception& e) {
            ++failures;
            std::cerr << "mount callback=" << static_cast<int>(test.trigger)
                      << " target=" << static_cast<int>(test.target) << " later=" << test.later << ": " << e.what()
                      << '\n';
        }
    }
    check(failures == 0, "Statistic guarded first-mount callback failed");

    Fixture loading;
    Signal<bool> show_loading{false};
    InputRef ref;
    loading.services.mount(Content{[&] {
        Statistic(StatisticProps{}.loading(show_loading).richValue(StatisticValueContent{[&] {
            Input(InputProps{}.ref(ref).defaultValue(u8"Retained"));
            show_loading.set(true);
        }}));
    }});
    sync(loading);
    const auto owner = loading.services.statistic().mounted()[0];
    check(loading.services.skeleton().snapshot(loading.services.statistic().snapshot(owner).skeleton).loading &&
              !ref.focus(),
          "loading changed during private Skeleton mount was lost");
    show_loading.set(false);
    sync(loading);
    check(ref.focus(), "loading changed during mount could not recover retained input");
}

void lifecycle() {
    first_mount_callbacks();
    final_mount_boundary();
    rich_region_retirement();
    blocked_ancestor_mount();
    initial_mount_retirement();
    Fixture f;
    Signal<bool> loading{false};
    InputRef ref;
    runtime::ComponentId owner;
    runtime::ComponentId parent;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 Statistic(StatisticProps{}.loading(loading).richValue(StatisticValueContent{[&] {
                     Input(InputProps{}.ref(ref).onBlur([&] {
                         check(f.services.destroy(owner), "synchronous disposal failed");
                         InputRef replacement = ref;
                         f.services.append_slot(parent, Content{[replacement] {
                                                    Input(
                                                        InputProps{}.ref(replacement).defaultValue(u8"New generation"));
                                                }});
                     }));
                 }}));
             }});
    }});
    sync(f);
    owner = f.services.statistic().mounted()[0];
    parent = *f.services.components().parent(owner);
    check(ref.focus(), "retirement fixture did not focus");
    loading.set(true);
    sync(f);
    check(f.services.statistic().mounted().empty() && f.inputs.mounted_inputs().size() == 1 &&
              blocked_roots(f.services.interactions()).empty() && ref.focus(),
          "retired generation blocked rebound ref");
    f.services.dispose();
    check(blocked_roots(f.services.interactions()).empty(), "owned blocks leaked on dispose");

    Fixture formatter_fixture;
    Signal<StatisticValue> value{StatisticValue{1}};
    runtime::ComponentId formatted_owner;
    formatter_fixture.services.mount(Content{[&] {
        Statistic(StatisticProps{}.value(value).formatter([&](const StatisticValue& v) {
            if (std::get<std::int64_t>(v.storage()) == 2) {
                check(formatter_fixture.services.destroy(formatted_owner), "formatter disposal failed");
            }
            return String{u8"formatted"};
        }));
    }});
    sync(formatter_fixture);
    formatted_owner = formatter_fixture.services.statistic().mounted()[0];
    value.set(StatisticValue{2});
    sync(formatter_fixture);
    check(formatter_fixture.services.statistic().mounted().empty(), "formatter used retired generation");
}

void mount_rollback() {
    struct MountError final : std::runtime_error {
        MountError() : std::runtime_error("R6 mount marker") {}
    };
    enum class Failure {
        Infinity,
        NaN,
        Formatter,
        Getter,
        Slot,
        EarlyGetter,
        FormatterOwnerRebind,
        FormatterPrivateRebind,
        Refresh,
        GetterOwnerRebind,
        SlotOwnerRebind,
        FormatterBlurThrow,
        SlotBlurThrow
    };
    unsigned failures = 0;
    for (const auto failure :
         {Failure::Infinity, Failure::NaN, Failure::Formatter, Failure::Getter, Failure::Slot, Failure::EarlyGetter,
          Failure::FormatterOwnerRebind, Failure::FormatterPrivateRebind, Failure::Refresh, Failure::GetterOwnerRebind,
          Failure::SlotOwnerRebind, Failure::FormatterBlurThrow, Failure::SlotBlurThrow}) {
        Fixture f;
        Signal<StatisticValue> value{
            failure == Failure::Infinity ? StatisticValue{std::numeric_limits<double>::infinity()}
            : failure == Failure::NaN    ? StatisticValue{std::numeric_limits<double>::quiet_NaN()}
                                         : StatisticValue{1}};
        InputRef failed_ref;
        InputRef fallback_ref;
        runtime::ComponentId owner;
        runtime::ComponentId outside;
        runtime::ComponentId replacement;
        runtime::ComponentId retired;
        runtime::ComponentId fallback;
        std::vector<runtime::NodeId> failed_nodes;
        std::vector<runtime::NodeId> text_nodes;
        std::function<void(runtime::ComponentId)> capture_nodes;
        std::shared_ptr<theme_runtime::ThemeScope> theme;
        TextInputSessionStamp failed_session;
        int reads = 0;
        int formats = 0;
        int failure_blurs = 0;
        bool cleanup = false;
        bool captured = false;
        bool caught = false;
        bool original_error = false;
        bool clean_at_catch = false;
        bool child_resources_clean = true;
        const auto counts = [&] {
            return std::array{f.services.components().component_count(),
                              f.nodes.size(),
                              f.services.surfaces().size(),
                              f.services.interactions().declaration_order().size(),
                              blocked_roots(f.services.interactions()).size(),
                              f.inputs.mounted_inputs().size(),
                              f.services.text().mounted_texts().size(),
                              theme->diagnostics().subscriber_count};
        };
        capture_nodes = [&](runtime::ComponentId component) {
            const auto node = f.services.components().root(component);
            failed_nodes.push_back(node);
            if (std::ranges::any_of(f.services.text().mounted_texts(),
                                    [&](const auto& text) { return text.component == component; })) {
                text_nodes.push_back(node);
            }
            for (auto child : f.services.components().children(component)) {
                capture_nodes(child);
            }
        };
        try {
            const auto capture_owner = [&] {
                if (captured) {
                    return;
                }
                captured = true;
                owner = f.services.components().children(outside).back();
                runtime::require_component_build_context().scope(owner).on_cleanup([&] {
                    cleanup = true;
                    capture_nodes(owner);
                });
                runtime::require_component_build_context().on_resource_cleanup(owner, [&] {
                    // Child cleanups have run, but nodes are still live: intrinsic removal is observable here.
                    for (auto node : failed_nodes) {
                        child_resources_clean &= !f.layout.remove_intrinsic_measure(node);
                        if (node != f.services.components().root(owner) &&
                            std::ranges::find(text_nodes, node) == text_nodes.end()) {
                            child_resources_clean &= !f.layout.remove_layout(node);
                        }
                    }
                    // Text retains an inert LeafLayout generation cache, with no owned callback/resources.
                });
            };
            const auto rebind = [&](bool private_only) {
                retired = private_only ? f.services.statistic().snapshot(owner).skeleton : owner;
                check(f.services.destroy(retired), "throwing callback could not retire owner/private Skeleton");
                runtime::require_component_build_context().mount_slot(
                    outside, Content{[&] {
                        Statistic(StatisticProps{}.value(99).richValue(StatisticValueContent{
                            [&] { Input(InputProps{}.ref(failed_ref).defaultValue(u8"Replacement")); }}));
                        replacement = f.services.statistic().mounted().back();
                    }});
            };
            f.services.mount(Content{[&] {
                Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                         outside = f.services.components().root_components().back();
                         theme = runtime::require_component_build_context().theme_scope();
                         const auto baseline = counts();
                         try {
                             Statistic(
                                 StatisticProps{}
                                     .value(bind([&] {
                                         ++reads;
                                         const auto current = value.get();
                                         if (failure == Failure::GetterOwnerRebind) {
                                             rebind(false);
                                             throw MountError{};
                                         }
                                         if (failure == Failure::Getter) {
                                             throw MountError{};
                                         }
                                         return current;
                                     }))
                                     .loading(failure == Failure::NaN)
                                     .visible(bind([&] {
                                         if (failure == Failure::EarlyGetter) {
                                             capture_owner();
                                             throw MountError{};
                                         }
                                         return failure != Failure::Refresh;
                                     }))
                                     .formatter([&](const StatisticValue&) {
                                         ++formats;
                                         if (failure == Failure::FormatterOwnerRebind ||
                                             failure == Failure::FormatterPrivateRebind) {
                                             rebind(failure == Failure::FormatterPrivateRebind);
                                         }
                                         if (failure == Failure::Refresh) {
                                             return String{u8"Formatted"};
                                         }
                                         throw MountError{};
                                     })
                                     .richTitle(StatisticTitle{[&] {
                                         capture_owner();
                                         Text(u8"Incomplete");
                                     }})
                                     .richValue(StatisticValueContent{[&] {
                                         Input(InputProps{}.ref(failed_ref).defaultValue(u8"Incomplete").onBlur([&] {
                                             if (failure == Failure::FormatterBlurThrow ||
                                                 failure == Failure::SlotBlurThrow) {
                                                 ++failure_blurs;
                                                 throw std::runtime_error(
                                                     "Secondary blur must not run during rollback");
                                             }
                                         }));
                                         if (failure != Failure::NaN) {
                                             check(failed_ref.focus(), "failed mount did not activate rich input");
                                             failed_session = f.inputs.sessions().active();
                                             check(failed_session.valid(), "failed mount has no real input session");
                                         }
                                         if (failure == Failure::Refresh) {
                                             Select(SelectProps{}.open(true).onOpenChange([](bool open) {
                                                 if (!open) {
                                                     throw MountError{};
                                                 }
                                             }));
                                         }
                                         if (failure == Failure::SlotOwnerRebind) {
                                             rebind(false);
                                             throw MountError{};
                                         }
                                         if (failure == Failure::Slot || failure == Failure::SlotBlurThrow) {
                                             throw MountError{};
                                         }
                                     }}));
                         } catch (const std::exception& error) {
                             caught = true;
                             original_error =
                                 failure == Failure::Infinity || failure == Failure::NaN
                                     ? dynamic_cast<const std::invalid_argument*>(&error) &&
                                           std::string_view{error.what()} == "Statistic numeric value must be finite"
                                     : dynamic_cast<const MountError*>(&error) &&
                                           std::string_view{error.what()} == "R6 mount marker";
                             clean_at_catch =
                                 !f.services.components().contains(owner) && cleanup &&
                                 (!failed_session.valid() || f.inputs.sessions().active() != failed_session);
                             if (replacement.valid()) {
                                 clean_at_catch &= f.services.components().contains(replacement) &&
                                                   f.services.statistic().mounted().size() == 1;
                             } else {
                                 clean_at_catch &= counts() == baseline && !failed_ref.focus() &&
                                                   !f.inputs.sessions().active().valid();
                             }
                             if (!clean_at_catch) {
                                 std::cerr << "local catch=" << static_cast<int>(failure)
                                           << " owner_alive=" << f.services.components().contains(owner)
                                           << " statistic=" << f.services.statistic().mounted().size()
                                           << " inputs=" << f.inputs.mounted_inputs().size()
                                           << " blocks=" << blocked_roots(f.services.interactions()).size()
                                           << " cleanup=" << cleanup << '\n';
                             }
                         }
                         // Keep the host Content successful: it must not perform the failed child's rollback for us.
                         Input(InputProps{}.ref(fallback_ref).defaultValue(u8"Fallback"));
                         fallback = f.inputs.mounted_inputs().back().component;
                     }});
            }});
            sync(f);
            check(fallback_ref.focus({InputFocusCursor::All}) &&
                      bool(f.inputs.dispatch(TextCommitted{String{u8"Fallback saved"}, f.inputs.sessions().active()})),
                  "local catch fallback could not accept real input");
            sync(f);
            check(drawn(f, f.inputs.text_layers(fallback).base), "local catch fallback emitted no glyphs");
            const auto prior_reads = reads;
            const auto prior_formats = formats;
            value.set(StatisticValue{7});
            sync(f);
            check(caught && original_error && clean_at_catch && child_resources_clean && reads == prior_reads &&
                      formats == prior_formats && failure_blurs == 0,
                  "local catch retained failed owner/resources or its subscriptions");
            for (auto node : failed_nodes) {
                check(!f.nodes.find(node), "failed mount retained a node generation");
            }
            check(!f.layout.remove_layout(failed_nodes[0]), "failed mount retained the owner layout callback");
            if (replacement.valid()) {
                check(replacement.index == retired.index && replacement.generation != retired.generation &&
                          f.services.statistic().snapshot(replacement).formatted_value.bytes() == "99" &&
                          failed_ref.focus() &&
                          f.inputs.editors().require(f.inputs.mounted_inputs()[0].editor).value() == "Replacement",
                      "throw rollback damaged callback replacement generation/ref/draft");
            }
            f.services.dispose();
            check(f.services.components().component_count() == 0 && f.nodes.size() == 0 &&
                      f.services.surfaces().size() == 0 && f.services.interactions().declaration_order().empty() &&
                      blocked_roots(f.services.interactions()).empty() && f.inputs.mounted_inputs().empty() &&
                      !f.inputs.sessions().active().valid() && !failed_ref.focus() && !fallback_ref.focus() &&
                      theme->diagnostics().subscriber_count == 0,
                  "local catch teardown did not return resources to baseline");
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "rollback failure=" << static_cast<int>(failure) << ": " << error.what() << '\n';
        }
        f.services.dispose();
    }
    check(failures == 0, "Statistic locally caught first-mount rollback failed");
}

void gates() {
    Fixture f;
    Signal<bool> visible{true};
    Signal<bool> loading{false};
    Signal<bool> open{true};
    int closes = 0;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true).gap(dp(11)), FlexContent{[&] {
                 Button(ButtonProps{}.wave(false), [] { Text(u8"Before"); });
                 Statistic(StatisticProps{}.visible(visible).loading(loading).richValue(StatisticValueContent{[&] {
                     Select(SelectProps{}.options({{1, u8"One"}}).open(open).onOpenChange([&](bool value) {
                         if (!value) {
                             ++closes;
                         }
                     }));
                 }}));
                 Button(ButtonProps{}.wave(false), [] { Text(u8"After"); });
             }});
    }});
    sync(f);
    const auto select = f.services.select().mounted()[0];
    const auto original = f.services.select().snapshot(select);
    check(original.visible, "controlled popup missing");
    loading.set(true);
    sync(f);
    check(!f.services.select().snapshot(select).visible && closes == 1 && open.get(),
          "loading did not close controlled popup");
    loading.set(false);
    sync(f);
    check(!f.services.select().snapshot(select).visible, "controlled rejection resurrected popup");
    open.set(false);
    open.set(true);
    sync(f);
    check(f.services.select().snapshot(select).visible, "explicit popup reopening failed");
    visible.set(false);
    sync(f);
    check(!f.services.select().snapshot(select).visible, "hidden root left popup open");
    const auto before = bounds(f, f.buttons.mounted_buttons()[0].component);
    const auto after = bounds(f, f.buttons.mounted_buttons()[1].component);
    check(std::abs(after.y - before.y - before.height - 11) < .01F, "hidden Statistic retained flow/gap");
}

void boundaries() {
    Fixture f;
    Signal<bool> spinning{false};
    Signal<bool> loading{false};
    Signal<bool> modal{false};
    InputRef outside;
    InputRef inside;
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(spinning), SpinContent{[&] {
                 Statistic(StatisticProps{}.loading(loading).richValue(
                     StatisticValueContent{[&] { Input(InputProps{}.ref(outside)); }}));
             }});
        Modal(ModalProps{}.open(modal).footer(ModalFooterMode::None), ModalContent{[&] {
                  Statistic(
                      StatisticProps{}.richValue(StatisticValueContent{[&] { Input(InputProps{}.ref(inside)); }}));
              }});
    }});
    sync(f);
    check(outside.focus(), "outside ref failed to focus");
    spinning.set(true);
    sync(f);
    loading.set(true);
    sync(f);
    loading.set(false);
    sync(f);
    check(!outside.focus(), "Statistic loading recovery unlocked outer Spin");
    spinning.set(false);
    modal.set(true);
    sync(f);
    check(!outside.focus() && inside.focus(), "Statistic broke Modal window-layer input ownership");
    modal.set(false);
    sync(f);
    check(outside.focus(), "Modal close did not restore retained Statistic");
}
} // namespace

int main(int argc, char** argv) {
    try {
        const auto name = argc > 1 ? std::string_view{argv[1]} : std::string_view{};
        if (name == "retained") {
            retained();
        } else if (name == "formatting") {
            formatting();
        } else if (name == "clips") {
            clips();
        } else if (name == "lifecycle") {
            lifecycle();
        } else if (name == "gates") {
            gates();
        } else if (name == "boundaries") {
            boundaries();
        } else if (name == "rollback") {
            mount_rollback();
        } else {
            throw std::runtime_error("unknown Statistic case");
        }
        std::cout << "Statistic " << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
