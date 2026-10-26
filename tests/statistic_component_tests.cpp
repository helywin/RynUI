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

void formatting() {
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

void lifecycle() {
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
