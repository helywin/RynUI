#include "component/descriptions_component.hpp"
#include "component/modal_component.hpp"
#include "component/selection_component.hpp"
#include "support/input_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string_view>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void sync(Fixture& f) {
    check(f.services.layout_and_synchronize({500, 600}, {0, 0, 500, 600}), "region clip layout failed");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    const auto& n = f.nodes.require(f.services.components().root(id));
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

bool contains(runtime::Rect r, runtime::Point p) {
    return r.width > 0 && r.height > 0 && p.x >= r.x && p.y >= r.y && p.x <= r.x + r.width && p.y <= r.y + r.height;
}

float effect_coverage(runtime::Point point, const graphics::RoundedEffectInstance& effect) {
    if (effect.geometry.ancestor_clip && !contains(effect.geometry.ancestor_clip->bounds, point)) {
        return 0;
    }
    return graphics::rounded_effect_coverage(point, effect) * effect.material.opacity * effect.material.color.alpha();
}

bool descendant(Fixture& f, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto current = std::optional{child}; current; current = f.services.components().parent(*current)) {
        if (*current == parent) {
            return true;
        }
    }
    return false;
}

struct Ink final {
    std::size_t raw_outside{};
    std::size_t visible_outside{};
    std::size_t visible_inside{};
};

// Sample actual R8 texel centers through each retained glyph's UV/vertex and shader clip.
// shaders/glyph.hlsl PSMain clips first, then multiplies atlas coverage by color alpha/opacity.
Ink ink(Fixture& f, runtime::ComponentId region) {
    Ink result;
    const auto allocated = bounds(f, region);
    const auto& atlas = f.scene.atlas();
    const auto aw = atlas.config().page_width;
    const auto ah = atlas.config().page_height;
    std::vector<detail::TextSceneId> scenes;
    for (const auto& text : f.services.text().mounted_texts()) {
        if (!descendant(f, text.component, region)) {
            continue;
        }
        scenes.push_back(text.scene);
    }
    for (const auto& input : f.inputs.mounted_inputs()) {
        if (descendant(f, input.component, region)) {
            const auto layers = f.inputs.text_layers(input.component);
            scenes.insert(scenes.end(), {layers.base, layers.selected, layers.placeholder});
        }
    }
    for (const auto scene : scenes) {
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

void text_regions(bool bordered, bool vertical) {
    Fixture f;
    Signal<LogicalLength> width{dp(bordered ? 24.0F : vertical ? 40.0F : 20.0F)};
    Signal<LogicalLength> height{dp(100)};
    Signal<String> text{String{u8"汉"}};
    Signal<bool> mark{false};
    auto theme = std::make_shared<ThemeConfig>();
    theme->descriptions.tokens.padding_inline = dp(1);
    theme->descriptions.tokens.padding_inline_small = dp(1);
    theme->descriptions.tokens.item_padding_end = dp(16);
    int mounts = 0;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(*theme), ThemeContent{[&] {
                  auto item = [&](String key) {
                      return DescriptionsItem{
                          key, DescriptionsLabel{[&] {
                              ++mounts;
                              Text(TypographyProps{}.content(text).mark(mark));
                          }},
                          DescriptionsContent{[&] { Text(TypographyProps{}.content(text).mark(mark)); }}};
                  };
                  auto items = std::vector{item(u8"a")};
                  if (vertical) {
                      items.push_back(item(u8"b"));
                  }
                  Descriptions(DescriptionsProps{}
                                   .column(vertical ? 2 : 1)
                                   .colon(false)
                                   .bordered(bordered)
                                   .labelPlacement(vertical ? DescriptionsLabelPlacement::Vertical
                                                            : DescriptionsLabelPlacement::Horizontal)
                                   .layout(LayoutStyle{}.width(width).height(height)),
                               items);
              }});
    }});
    sync(f);
    const auto owner = f.services.descriptions().mounted()[0];
    const auto original = f.services.descriptions().snapshot(owner);
    const auto label_bounds = bounds(f, original.items[0].label);
    const auto value_bounds = bounds(f, original.items[0].content);
    check(label_bounds.width > 0 && value_bounds.width > 0, "repro did not allocate label and value regions");
    const auto first = ink(f, original.items[0].label);
    std::cout << "bordered=" << bordered << " vertical=" << vertical << " label_width=" << label_bounds.width
              << " value_width=" << value_bounds.width << " raw_outside=" << first.raw_outside
              << " visible_outside=" << first.visible_outside << '\n';
    check(first.raw_outside > 0 && first.visible_inside > 0, "repro did not sample real oversized glyph ink");
    check(first.visible_outside == 0, "label R8 ink survived shader clip outside its allocated region");
    mark.set(true);
    sync(f);
    const auto marker = f.services.components().theme_scope(owner)->snapshot().typography().colors.mark_background;
    bool mark_seen = false;
    for (const auto& q : f.services.surfaces().instances().instances()) {
        if (q.color != std::array{marker.red(), marker.green(), marker.blue(), marker.alpha()} || q.opacity <= 0) {
            continue;
        }
        mark_seen = true;
        const runtime::Rect rect{q.bounds[0] + q.translation[0], q.bounds[1] + q.translation[1], q.bounds[2],
                                 q.bounds[3]};
        bool allocated = false;
        for (const auto& item : original.items) {
            for (const auto region : {item.label, item.content}) {
                const auto clip = bounds(f, region);
                allocated |= rect.width <= 0 || rect.height <= 0 ||
                             (rect.x >= clip.x - .01F && rect.y >= clip.y - .01F &&
                              rect.x + rect.width <= clip.x + clip.width + .01F &&
                              rect.y + rect.height <= clip.y + clip.height + .01F);
            }
        }
        check(allocated, "actual Text mark quad escaped all label/value allocations");
    }
    check(mark_seen, "narrow content surface fixture did not publish actual quads");
    for (const auto extent : {runtime::Size{20, 60}, runtime::Size{vertical ? 40.0F : 20.0F, 7}, runtime::Size{0, 0},
                              runtime::Size{80, 100}}) {
        width.set(dp(extent.width));
        height.set(dp(extent.height));
        text.set(extent.height == 60 || extent.width == 80 ? String{u8"W\u2060W"} : String{u8"汉"});
        sync(f);
        const auto now = f.services.descriptions().snapshot(owner);
        if (!vertical && extent.height == 60) {
            check(ink(f, now.items[0].label).raw_outside > 0, "unbreakable content fixture lacked oversized ink");
        }
        for (std::size_t i = 0; i < now.items.size(); ++i) {
            check(now.items[i].label == original.items[i].label && now.items[i].content == original.items[i].content,
                  "region resize remounted slots");
            check(ink(f, now.items[i].label).visible_outside == 0 && ink(f, now.items[i].content).visible_outside == 0,
                  "retained glyph escaped label/value allocation after short/zero/recovery");
        }
    }
    check(mounts == (vertical ? 2 : 1), "ordinary text updates reran label slots");
}

bool inside(runtime::Rect rect, runtime::Rect clip) {
    return rect.width <= 0 || rect.height <= 0 ||
           (rect.x >= clip.x - .01F && rect.y >= clip.y - .01F && rect.x + rect.width <= clip.x + clip.width + .01F &&
            rect.y + rect.height <= clip.y + clip.height + .01F);
}

void rich_regions_and_decorations() {
    Fixture f;
    Signal<bool> open{true};
    Signal<bool> shown{true};
    Signal<LogicalLength> width{dp(280)};
    Signal<LogicalLength> height{dp(170)};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<bool> bordered{true};
    Signal<DescriptionsLabelPlacement> placement{DescriptionsLabelPlacement::Horizontal};
    InputRef ref;
    int clicks = 0;
    int mounts = 0;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Modal(ModalProps{}.open(open).width(400).title(u8"Rich regions"), ModalContent{[&] {
                            Descriptions(DescriptionsProps{}
                                             .column(1)
                                             .colon(false)
                                             .bordered(bordered)
                                             .labelPlacement(placement)
                                             .layout(LayoutStyle{}.width(width).height(height)),
                                         {DescriptionsItem{u8"rich", DescriptionsLabel{[] {
                                                               Text(TypographyProps{}.content(u8"汉").mark(true));
                                                           }},
                                                           DescriptionsContent{[&] {
                                                               ++mounts;
                                                               Flex(FlexProps{}.vertical(true).gap(dp(8)),
                                                                    FlexContent{[&] {
                                                                        Button(ButtonProps{}.onClick([&] { ++clicks; }),
                                                                               [] { Text(u8"汉 Button"); });
                                                                        Input(InputProps{}
                                                                                  .ref(ref)
                                                                                  .defaultValue(u8"汉 Draft")
                                                                                  .variant(InputVariant::Borderless));
                                                                    }});
                                                           }}}
                                              .visible(shown)});
                        }});
              }});
    }});
    sync(f);
    const auto owner = f.services.descriptions().mounted()[0];
    const auto original = f.services.descriptions().snapshot(owner);
    runtime::ComponentId body_button;
    input::InteractionId body_interaction;
    for (const auto& candidate : f.buttons.mounted_buttons()) {
        if (descendant(f, candidate.component, owner)) {
            body_button = candidate.component;
            body_interaction = candidate.interaction;
        }
    }
    check(body_button.valid(), "rich body Button missing");
    const auto input = f.inputs.mounted_inputs()[0];
    const auto original_input_bounds = bounds(f, input.component);
    const auto original_button_bounds = bounds(f, body_button);
    const auto focus_ink = [&](runtime::Rect b, float outset) {
        const runtime::Point point{b.x - outset, b.y + b.height / 2};
        return std::ranges::any_of(f.services.rounded_effects().packed_instances(), [&](const auto& effect) {
            return effect.geometry.kind == graphics::RoundedEffectKind::outline &&
                   effect.geometry.outline_width > 1.5F && effect.material.opacity > 0 && effect.material.visible &&
                   effect_coverage(point, effect) > .05F;
        });
    };
    static_cast<void>(f.services.focus().request_focus(body_interaction, FocusModality::keyboard));
    check(f.services.focus().state().focused == body_interaction, "body Button focus failed");
    sync(f);
    check(focus_ink(original_button_bounds, 2.5F), "partition clip removed actual outward Button focus coverage");
    check(ink(f, original.items[0].content).visible_outside == 0, "Button text escaped value allocation");
    const runtime::Point center{original_button_bounds.x + original_button_bounds.width / 2,
                                original_button_bounds.y + original_button_bounds.height / 2};
    check(f.services.hit_test().hit_test(center) == body_interaction, "body Button lost actual hit geometry");
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary, center.x, center.y});
    }
    check(clicks == 1, "rich body Button event changed");
    check(f.services.focus().request_focus(input.interaction, FocusModality::keyboard), "body Input focus failed");
    sync(f);
    check(focus_ink(original_input_bounds, .75F), "partition clip removed actual outward Input focus coverage");
    check(ref.focus({InputFocusCursor::All}), "Input ref focus failed");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"汉 retained draft"}, f.inputs.sessions().active()})),
          "Input text delivery failed");
    sync(f);
    const auto session = f.inputs.sessions().active();
    auto config = std::make_shared<ThemeConfig>();
    config->algorithms = {ThemeAlgorithm::Dark};
    theme.set(*config);
    sync(f);
    check(f.inputs.sessions().active() == session &&
              f.inputs.editors().require(input.editor).value() == "汉 retained draft",
          "Theme update changed rich Input session/draft");
    check(bounds(f, input.component) == original_input_bounds && bounds(f, body_button) == original_button_bounds,
          "clip fix shrank or repositioned native controls");
    for (const auto extent : {runtime::Size{80, 25}, runtime::Size{0, 0}, runtime::Size{280, 170}}) {
        width.set(dp(extent.width));
        height.set(dp(extent.height));
        bordered.set(false);
        placement.set(DescriptionsLabelPlacement::Vertical);
        sync(f);
        const auto now = f.services.descriptions().snapshot(owner);
        check(now.items[0].content == original.items[0].content && ink(f, now.items[0].content).visible_outside == 0,
              "rich content escaped allocation or remounted after constraint changes");
        const auto allocated = bounds(f, now.items[0].content);
        const auto geometry = f.inputs.layout_snapshot(input.component);
        check(inside(geometry.clip, allocated), "Input effective text clip escaped allocated value region");
        check(!f.services.hit_test().hit_test({allocated.x - 1, allocated.y + allocated.height / 2}) ||
                  f.services.hit_test().hit_test({allocated.x - 1, allocated.y + allocated.height / 2}) !=
                      input.interaction,
              "Input hit escaped allocated value region");
    }
    const auto marker = f.services.components().theme_scope(owner)->snapshot().typography().colors.mark_background;
    bool surface_seen = false;
    const auto label_area = bounds(f, f.services.descriptions().snapshot(owner).items[0].label);
    for (const auto& q : f.services.surfaces().instances().instances()) {
        if (q.color == std::array{marker.red(), marker.green(), marker.blue(), marker.alpha()} && q.opacity > 0) {
            surface_seen = true;
            check(inside({q.bounds[0] + q.translation[0], q.bounds[1] + q.translation[1], q.bounds[2], q.bounds[3]},
                         label_area),
                  "Text mark surface escaped label allocation");
        }
    }
    check(surface_seen, "allocated label did not publish a real mark surface");
    bool fill_seen = false;
    for (const auto& effect : f.services.rounded_effects().packed_instances()) {
        if (effect.material.color != marker || effect.material.opacity <= 0 || !effect.material.visible) {
            continue;
        }
        fill_seen = true;
        for (const auto point :
             {runtime::Point{label_area.x - .25F, label_area.y + label_area.height / 2},
              runtime::Point{label_area.x + label_area.width + .25F, label_area.y + label_area.height / 2},
              runtime::Point{label_area.x + label_area.width / 2, label_area.y - .25F},
              runtime::Point{label_area.x + label_area.width / 2, label_area.y + label_area.height + .25F}}) {
            check(effect_coverage(point, effect) == 0, "Modal clipped fill painted outside label region");
        }
    }
    check(fill_seen, "Modal did not publish actual clipped label fill effects");
    shown.set(false);
    sync(f);
    check(!ref.focus() && !f.inputs.sessions().active().valid(), "hidden rich region retained input");
    shown.set(true);
    width.set(dp(280));
    height.set(dp(170));
    sync(f);
    check(mounts == 1 && f.inputs.mounted_inputs()[0].editor == input.editor &&
              f.inputs.editors().require(input.editor).value() == "汉 retained draft",
          "rich region recovery lost identity/draft");
}

void selection_decorations(bool modal) {
    bool passed = true;
    for (int kind = 0; kind < 4; ++kind) {
        Fixture f;
        detail::SelectionComponentHost selection{f.services};
        f.services.set_motion_preference(animation::MotionPreference::reduced);
        int mounts = 0;
        int changes = 0;
        const auto body = [&] {
            Flex(FlexProps{}
                     .vertical(true)
                     .align(FlexAlign::Center)
                     .justify(FlexJustify::Center)
                     .layout(LayoutStyle{}.width(dp(240)).height(dp(120))),
                 FlexContent{[&] {
                     Flex(
                         FlexProps{}.vertical(true).layout(LayoutStyle{}.width(dp(180)).height(dp(80))),
                         FlexContent{[&] {
                             Descriptions(
                                 DescriptionsProps{}.column(1).colon(false).layout(LayoutStyle{}.width(dp(180))),
                                 {DescriptionsItem{u8"selection", DescriptionsLabel{[] { Text(u8"Label"); }},
                                                   DescriptionsContent{[&] {
                                                       ++mounts;
                                                       if (kind == 0) {
                                                           Switch(SwitchProps{}.onChange([&](bool) { ++changes; }));
                                                       } else if (kind == 1) {
                                                           Checkbox(CheckboxProps{}.onChange([&](bool) { ++changes; }));
                                                       } else if (kind == 2) {
                                                           Radio(RadioProps{}.onChange([&](bool) { ++changes; }));
                                                       } else {
                                                           RadioButton(RadioProps{}.onChange([&](bool) { ++changes; }));
                                                       }
                                                   }}}});
                         }});
                 }});
        };
        f.services.mount(Content{[&] {
            if (modal) {
                Modal(ModalProps{}.open(true).width(400).title(u8"Selection clip"), ModalContent{body});
            } else {
                body();
            }
        }});
        const auto synchronize = [&] {
            check(f.services.layout_and_synchronize({500, 600}, {0, 0, 500, 600}, {40, 40}),
                  "selection region layout failed");
        };
        synchronize();
        const auto owner = f.services.descriptions().mounted()[0];
        const auto original = f.services.descriptions().snapshot(owner);
        const auto control = selection.mounted()[0];
        const auto panel = f.services.components().root(owner);
        const auto external = *f.nodes.require(panel).parent;
        const auto value = bounds(f, original.items[0].content);
        const float offset = value.x - f.nodes.require(external).bounds.x;
        runtime::Size panel_size;
        // Place an overflowing child at a real ancestor's left edge without resizing the control.
        f.layout.set_layout(
            external, layout::ComponentLayout{
                          [&](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                              panel_size = engine.measure_child(panel, {0, 180, 0, 80});
                              return limits.constrain({180, 80});
                          },
                          [&](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect b) {
                              engine.place_child(panel, {b.x - offset, b.y + 20, panel_size.width, panel_size.height});
                          }});
        f.dirty.invalidate(external, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Geometry);
        synchronize();
        static_cast<void>(f.services.focus().request_focus(control.interaction, FocusModality::keyboard));
        synchronize();
        check(f.services.focus().state().focused == control.interaction &&
                  selection.snapshot(control.component).focus.focus_visible,
              "selection did not acquire real keyboard focus");
        const auto before = bounds(f, control.component);
        const auto focus = f.services.surfaces().focus_effect(control.surface);
        const auto shape = focus.geometry.shape.rect;
        const float outset = focus.geometry.outline_offset + focus.geometry.outline_width / 2;
        const runtime::Point point{shape.x - outset + focus.geometry.translation.x,
                                   shape.y + shape.height * .4F + focus.geometry.translation.y};
        const auto coverage = [&] {
            float result = 0;
            for (const auto& effect : f.services.rounded_effects().packed_instances()) {
                if (effect.geometry.kind == graphics::RoundedEffectKind::outline &&
                    effect.geometry.outline_width == focus.geometry.outline_width && effect.material.visible) {
                    result = std::max(result, effect_coverage(point, effect));
                }
            }
            return result;
        };
        const auto allocated = bounds(f, original.items[0].content);
        check(!contains(allocated, point) && point.x < f.nodes.require(external).bounds.x,
              "selection regression did not sample outside the tight slot and ancestor");
        auto raw = focus;
        raw.geometry.ancestor_clip.reset();
        const float raw_coverage = effect_coverage(point, raw);
        const float available = coverage();
        f.nodes.require(external).clip_content = true;
        f.dirty.invalidate(external, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
        synchronize();
        const float hard_clipped = coverage();
        f.nodes.require(external).clip_content = false;
        f.dirty.invalidate(external, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
        synchronize();
        const float restored = coverage();
        std::cout << "selection=" << kind << " modal=" << modal << " raw=" << raw_coverage << " available=" << available
                  << " hard=" << hard_clipped << " restored=" << restored << '\n';
        passed = passed && raw_coverage > .1F && available > .1F && hard_clipped == 0 && restored > .1F;
        check(bounds(f, control.component) == before && mounts == 1 &&
                  selection.mounted()[0].component == control.component,
              "selection clip update resized or remounted the control");
        const runtime::Point center{before.x + before.width / 2, before.y + before.height / 2};
        check(f.services.hit_test().hit_test(center) == control.interaction, "selection hit did not recover");
        for (const auto& hit : f.services.hit_test().records()) {
            if (hit.interaction == control.interaction) {
                check(hit.effective_clip == f.services.component_clip(control.component, {500, 600}, {0, 0, 500, 600}),
                      "selection decoration query changed the content/hit clip");
            }
        }
        f.services.focus().dispatch({Key::space, KeyAction::down});
        f.services.focus().dispatch({Key::space, KeyAction::up});
        check(changes == 1, "selection keyboard event changed");
    }
    check(passed, "selection outward ring coverage or external hard clip failed");
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        const auto which = argc > 1 ? std::string_view{argv[1]} : std::string_view{"horizontal"};
        if (which == "rich") {
            rich_regions_and_decorations();
        } else if (which == "selection" || which == "selection_modal") {
            selection_decorations(which == "selection_modal");
        } else {
            text_regions(which == "bordered" || which == "vertical_bordered",
                         which == "vertical" || which == "vertical_bordered");
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
