#include "component/collapse_component.hpp"
#include "component/feedback_component.hpp"
#include "component/modal_component.hpp"
#include "component/select_component.hpp"
#include "support/input_fixture.hpp"
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
    check(f.services.layout_and_synchronize({500, 1200}, {0, 0, 500, 1200}), "Collapse layout failed");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    const auto& node = f.nodes.require(f.services.components().root(id));
    return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
            node.bounds.height};
}

InteractionId interaction(Fixture& f, runtime::ComponentId id) {
    for (auto hit : f.services.interactions().declaration_order()) {
        if (f.services.interactions().require(hit).component == id) {
            return hit;
        }
    }
    throw std::runtime_error("Collapse header has no interaction");
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, r.x + 3, r.y + r.height / 2});
    }
    sync(f);
}

void key(Fixture& f, Key value, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({value, KeyAction::down, modifiers});
    f.services.focus().dispatch({value, KeyAction::up, modifiers});
    sync(f);
}

CollapsePanel plain(String key) {
    return {key, CollapseTitle{[key] { Text(TextProps{}.content(key)); }}, CollapseBody{[] { Text(u8"Details"); }}};
}

IconSource arrow_source(Fixture& f, runtime::NodeId node) {
    for (const auto& text : f.services.text().mounted_texts()) {
        if (f.services.components().root(text.component) == node) {
            check(f.scene.primitive(text.scene).instances.count > 0, "Collapse arrow has no visible glyphs");
            return f.services.text().icon_snapshot(text.component).source;
        }
    }
    throw std::runtime_error("Collapse arrow is not retained");
}

void controlled_and_accordion() {
    Fixture f;
    Signal<CollapseKeys> active{CollapseKeys{String{u8"a"}}};
    Signal<bool> accordion{false};
    bool echo{};
    std::vector<CollapseKeys> changes;
    f.services.mount(Content{[&] {
        Collapse(CollapseProps{}.activeKeys(active).accordion(accordion).onChange([&](CollapseKeys keys) {
            changes.push_back(keys);
            if (echo) {
                active.set(keys);
            }
        }),
                 {plain(u8"a"), plain(u8"b"), plain(u8"locked").disabled()});
    }});
    sync(f);
    const auto id = f.services.collapse().mounted()[0];
    const auto initial = f.services.collapse().snapshot(id);
    check(arrow_source(f, initial.panels[0].arrow) == IconSource{IconName::DownOutlined} &&
              arrow_source(f, initial.panels[1].arrow) == IconSource{IconName::RightOutlined},
          "Collapse arrow state drifted");
    click(f, initial.panels[1].header);
    check(f.services.collapse().snapshot(id).active_keys == CollapseKeys{String{u8"a"}} && changes.size() == 1 &&
              changes[0] == CollapseKeys{String{u8"a"}, String{u8"b"}},
          "controlled refusal changed Collapse");
    echo = true;
    click(f, initial.panels[1].header);
    check(f.services.collapse().snapshot(id).panels[0].expanded &&
              f.services.collapse().snapshot(id).panels[1].expanded,
          "multiple Collapse failed");
    accordion.set(true);
    sync(f);
    check(f.services.collapse().snapshot(id).active_keys == CollapseKeys{String{u8"a"}},
          "accordion failed to normalize");
    click(f, initial.panels[1].header);
    check(f.services.collapse().snapshot(id).active_keys == CollapseKeys{String{u8"b"}},
          "accordion kept previous panel");
    key(f, Key::space);
    check(f.services.collapse().snapshot(id).active_keys.empty(), "accordion cannot close last panel");
    check(arrow_source(f, initial.panels[1].arrow) == IconSource{IconName::RightOutlined},
          "closing did not update retained arrow");
    active.set({String{u8"unknown"}, String{u8"locked"}, String{u8"locked"}, String{u8"a"}});
    sync(f);
    check(f.services.collapse().snapshot(id).active_keys == CollapseKeys{String{u8"locked"}} &&
              f.services.collapse().snapshot(id).panels[2].expanded,
          "explicit disabled key lost authority");
    const auto count = changes.size();
    click(f, initial.panels[2].header);
    check(changes.size() == count &&
              !f.services.focus().request_focus(interaction(f, initial.panels[2].header), FocusModality::keyboard),
          "disabled header activated or accepted focus");
}

void retained_drafts_reflow_and_cleanup() {
    Fixture f;
    Signal<CollapseKeys> active{CollapseKeys{String{u8"a"}}};
    int title_runs{};
    int body_runs{};
    int extra_runs{};
    InputRef ref;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true).gap(dp(10)), FlexContent{[&] {
                 Collapse(CollapseProps{}.activeKeys(active),
                          {CollapsePanel{u8"a", CollapseTitle{[&] {
                                             ++title_runs;
                                             Text(u8"Draft");
                                         }},
                                         CollapseBody{[&] {
                                             ++body_runs;
                                             Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                                      Input(InputProps{}.defaultValue(u8"original").ref(ref));
                                                      Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}));
                                                  }});
                                         }}}
                               .extra(CollapseExtra{[&] {
                                   ++extra_runs;
                                   Text(u8"Extra");
                               }}),
                           CollapsePanel{u8"b", CollapseTitle{[] { Text(u8"Second"); }}, CollapseBody{[] {
                                             Input(InputProps{}.autoFocus().defaultValue(u8"hidden"));
                                         }}}});
                 Button(ButtonProps{}, [] { Text(u8"Following"); });
             }});
    }});
    sync(f);
    const auto id = f.services.collapse().mounted()[0];
    const auto original = f.services.collapse().snapshot(id);
    const auto editor = f.inputs.mounted_inputs()[0];
    const auto select = f.services.select().mounted()[0];
    const auto sibling = f.buttons.mounted_buttons()[0].component;
    const auto old_y = bounds(f, sibling).y;
    const auto body_height = bounds(f, original.panels[0].body).height;
    const auto count = f.services.components().component_count();
    check(!f.inputs.sessions().active().valid(), "hidden autofocus started session");
    check(ref.focus({InputFocusCursor::All}), "retained Input ref focus failed");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"保留草稿"}, f.inputs.sessions().active()})),
          "draft commit failed");
    sync(f);
    const auto session = f.inputs.sessions().active();
    active.set({});
    sync(f);
    check(bounds(f, sibling).y == old_y - body_height && !f.services.focus().state().focused &&
              !f.inputs.sessions().active().valid() && !ref.focus() &&
              !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, session})),
          "collapsed body retained layout, focus, ref or session");
    active.set({String{u8"a"}});
    sync(f);
    check(bounds(f, sibling).y == old_y && f.inputs.mounted_inputs()[0].editor == editor.editor &&
              f.inputs.editors().require(editor.editor).value() == String{u8"保留草稿"}.bytes(),
          "collapse remounted editor or lost draft");
    click(f, f.services.select().snapshot(select).selector);
    check(f.services.select().snapshot(select).visible, "body Select did not open");
    active.set({});
    sync(f);
    active.set({String{u8"a"}});
    sync(f);
    check(!f.services.select().snapshot(select).visible, "hidden body reopened old popup");
    // Hide during an actual text-selection capture, without a release event.
    const auto r = f.inputs.layout_snapshot(editor.component).viewport;
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, r.x + 4, r.y + r.height / 2});
    check(f.services.pointer().state(PointerIdentity::mouse())->capture == editor.interaction, "Input did not capture");
    active.set({});
    sync(f);
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "hidden body retained pointer capture");
    check(title_runs == 1 && body_runs == 1 && extra_runs == 1 && f.services.components().component_count() == count &&
              f.services.collapse().snapshot(id).panels[0].body == original.panels[0].body,
          "slots were remounted");
    check(f.services.destroy(id), "Collapse destroy failed");
    sync(f);
    check(f.inputs.mounted_inputs().empty() && f.services.select().mounted().empty() &&
              f.services.collapse().mounted().empty() && f.services.interactions().size() == 1,
          "Collapse leaked descendants");
}

void keyboard_extra_and_disabled_capture() {
    Fixture f;
    Signal<bool> disabled{false};
    int extra_actions{};
    f.services.mount(Content{[&] {
        Collapse(CollapseProps{},
                 {plain(u8"a"), plain(u8"b").disabled(disabled), plain(u8"locked").disabled().extra(CollapseExtra{[&] {
                      Button(ButtonProps{}.onClick([&] { ++extra_actions; }), [] { Text(u8"Action"); });
                  }}),
                  plain(u8"c")});
    }});
    sync(f);
    const auto id = f.services.collapse().mounted()[0];
    const auto initial = f.services.collapse().snapshot(id);
    check(f.services.focus().request_focus(interaction(f, initial.panels[0].header), FocusModality::keyboard),
          "header focus failed");
    key(f, Key::down);
    check(f.services.focus().state().focused == interaction(f, initial.panels[1].header) &&
              f.services.collapse().snapshot(id).active_keys.empty(),
          "Down changed expansion or missed header");
    key(f, Key::down);
    check(f.services.focus().state().focused == interaction(f, initial.panels[3].header), "Down did not skip disabled");
    key(f, Key::home);
    key(f, Key::enter);
    key(f, Key::tab);
    check(f.services.focus().state().focused == interaction(f, initial.panels[1].header), "header Tab order failed");
    key(f, Key::end);
    key(f, Key::space);
    check(f.services.collapse().snapshot(id).active_keys.size() == 2, "keyboard multi expansion failed");
    click(f, f.buttons.mounted_buttons()[0].component);
    key(f, Key::enter);
    check(extra_actions == 2 && f.services.collapse().snapshot(id).active_keys.size() == 2,
          "disabled panel extra was disabled or toggled header");
    const auto r = bounds(f, initial.panels[1].header);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, r.x + 3, r.y + 3});
    check(f.services.pointer().state(PointerIdentity::mouse())->capture == interaction(f, initial.panels[1].header),
          "header capture failed");
    disabled.set(true);
    sync(f);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, r.x + 3, r.y + 3});
    sync(f);
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture &&
              f.services.collapse().snapshot(id).active_keys.size() == 2,
          "disabled captured header activated");
}

void logical_modal_and_spin_boundaries() {
    Fixture f;
    Signal<bool> modal{false};
    Signal<bool> spinning{false};
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(spinning), SpinContent{[] { Collapse(CollapseProps{}, {plain(u8"outside")}); }});
        Modal(ModalProps{}.open(modal).footer(ModalFooterMode::None),
              ModalContent{[] { Collapse(CollapseProps{}, {plain(u8"inside")}); }});
    }});
    sync(f);
    const auto outside = f.services.collapse().mounted()[0];
    const auto inside = f.services.collapse().mounted()[1];
    const auto out_header = f.services.collapse().snapshot(outside).panels[0].header;
    const auto in_header = f.services.collapse().snapshot(inside).panels[0].header;
    check(f.services.focus().request_focus(interaction(f, out_header), FocusModality::keyboard),
          "outside focus failed");
    spinning.set(true);
    sync(f);
    key(f, Key::enter);
    click(f, out_header);
    check(f.services.collapse().snapshot(outside).active_keys.empty() &&
              !f.services.focus().request_focus(interaction(f, out_header), FocusModality::keyboard),
          "Spin allowed Collapse activation");
    spinning.set(false);
    sync(f);
    modal.set(true);
    sync(f);
    click(f, out_header);
    check(f.services.collapse().snapshot(outside).active_keys.empty() &&
              !f.services.focus().request_focus(interaction(f, out_header), FocusModality::keyboard),
          "Modal allowed background Collapse");
    static_cast<void>(f.services.focus().request_focus(interaction(f, in_header), FocusModality::keyboard));
    check(f.services.focus().state().focused == interaction(f, in_header), "Modal inner Collapse blocked");
    key(f, Key::space);
    check(f.services.collapse().snapshot(inside).panels[0].expanded, "Modal inner Collapse keyboard failed");
    modal.set(false);
    sync(f);
    click(f, out_header);
    check(f.services.collapse().snapshot(outside).panels[0].expanded, "Collapse did not recover after boundary");
}

void theme_sizes_and_retained_material() {
    Fixture f;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<ControlSize> size{ControlSize::Middle};
    int runs{};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Collapse(CollapseProps{}.size(size).defaultActiveKeys({String{u8"a"}}),
                           {CollapsePanel{u8"a", CollapseTitle{[] { Text(u8"Title"); }}, CollapseBody{[&] {
                                              ++runs;
                                              Text(u8"Body");
                                          }}}});
              }});
    }});
    sync(f);
    const auto id = f.services.collapse().mounted()[0];
    const auto original = f.services.collapse().snapshot(id);
    for (const auto value : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
        size.set(value);
        sync(f);
        const float expected = value == ControlSize::Small ? 38.0F : value == ControlSize::Middle ? 46.0F : 56.0F;
        check(bounds(f, original.panels[0].header).height == expected, "Collapse size recipe failed");
    }
    const auto node = f.services.components().root(id);
    const auto measures = f.nodes.require(node).measure_count;
    const auto effects = f.services.rounded_effects().diagnostics();
    ThemeConfig custom;
    const auto red = Color::rgba8(120, 20, 30);
    custom.collapse.tokens.header_background = red;
    theme.set(custom);
    sync(f);
    check(f.nodes.require(node).measure_count == measures && runs == 1 &&
              f.services.rounded_effects().diagnostics().material_updates > effects.material_updates &&
              std::ranges::any_of(
                  f.services.rounded_effects().packed_instances(),
                  [&](const auto& effect) { return effect.material.color == red && effect.material.opacity > 0; }),
          "Collapse Theme material update remeasured or omitted scene");
    custom.collapse.tokens.header_block_large = dp(20);
    theme.set(custom);
    sync(f);
    check(bounds(f, original.panels[0].header).height == 64 && runs == 1 &&
              f.services.collapse().snapshot(id).panels[0].arrow == original.panels[0].arrow,
          "Collapse metrics remounted slots");
    const auto before = f.services.rounded_effects().diagnostics();
    static_cast<void>(f.frames.consume_request());
    sync(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline() &&
              f.services.rounded_effects().diagnostics().geometry_updates == before.geometry_updates &&
              f.services.rounded_effects().diagnostics().material_updates == before.material_updates,
          "Collapse idle scheduled work");
}

void callbacks_and_validation() {
    for (int mode = 0; mode < 3; ++mode) {
        Fixture f;
        runtime::ComponentId owner;
        Signal<CollapseKeys> active{CollapseKeys{String{u8"a"}}};
        bool retire{};
        f.services.mount(Content{[&] {
            CollapseProps props;
            if (mode != 0) {
                props.activeKeys(active);
            }
            props.onChange([&](CollapseKeys) { static_cast<void>(f.services.destroy(owner)); });
            Collapse(props, {CollapsePanel{u8"a", CollapseTitle{[] { Text(u8"A"); }}, CollapseBody{[&] {
                                               Input(InputProps{}.onBlur([&] {
                                                   if (retire) {
                                                       static_cast<void>(f.services.destroy(owner));
                                                   }
                                               }));
                                           }}}});
        }});
        sync(f);
        owner = f.services.collapse().mounted()[0];
        if (mode == 2) {
            check(f.services.focus().request_focus(f.inputs.mounted_inputs()[0].interaction, FocusModality::keyboard),
                  "blur fixture focus failed");
            retire = true;
            active.set({});
            sync(f);
        } else {
            click(f, f.services.collapse().snapshot(owner).panels[0].header);
        }
        check(f.services.collapse().mounted().empty() && f.inputs.mounted_inputs().empty() &&
                  f.services.interactions().size() == 0 && !f.inputs.sessions().active().valid() &&
                  f.services.rounded_effects().live_count() == 0,
              "callback disposal retained Collapse resources");
    }
    for (int mode = 0; mode < 4; ++mode) {
        Fixture f;
        int runs{};
        bool rejected{};
        try {
            f.services.mount(Content{[&] {
                CollapseProps props;
                if (mode == 0) {
                    props.size(static_cast<ControlSize>(99));
                }
                if (mode == 1) {
                    props.activeKeys(CollapseKeys{}).defaultActiveKeys({});
                }
                Collapse(props, {CollapsePanel{mode == 2 ? String{} : String{u8"a"}, CollapseTitle{[&] { ++runs; }},
                                               CollapseBody{[] {}}},
                                 plain(mode == 3 ? String{u8"a"} : String{u8"b"})});
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && runs == 0 && f.nodes.size() == 0 && f.services.collapse().mounted().empty(),
              "invalid Collapse partially mounted");
    }
    Fixture empty;
    empty.services.mount(Content{[] { Collapse(CollapseProps{}, {}); }});
    sync(empty);
    check(empty.services.interactions().size() == 0, "empty Collapse has interactions");
}

void enabled_header_refreshes_pointer_hit() {
    Fixture f;
    Signal<bool> disabled{true};
    f.services.mount(Content{[&] { Collapse(CollapseProps{}, {plain(u8"a").disabled(disabled)}); }});
    sync(f);
    const auto id = f.services.collapse().mounted()[0];
    const auto header = f.services.collapse().snapshot(id).panels[0].header;
    const auto node = f.services.components().root(header);
    click(f, header);
    check(f.services.collapse().snapshot(id).active_keys.empty(), "initial disabled header activated");
    for (int cycle = 0; cycle < 2; ++cycle) {
        const auto measures = f.nodes.require(node).measure_count;
        disabled.set(false);
        sync(f);
        check(f.nodes.require(node).measure_count == measures, "eligibility update unnecessarily measured header");
        click(f, header);
        check(f.services.collapse().snapshot(id).panels[0].expanded,
              "enabled header retained stale pointer eligibility");
        key(f, Key::space);
        check(!f.services.collapse().snapshot(id).panels[0].expanded, "enabled header keyboard failed");
        disabled.set(true);
        sync(f);
        click(f, header);
        check(!f.services.collapse().snapshot(id).panels[0].expanded, "disabled header retained pointer eligibility");
    }
}

void constrained_headers_clip_actual_scene() {
    Fixture f;
    Signal<LogicalLength> height{dp(200)};
    int mounts{};
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 std::vector<CollapsePanel> panels;
                 for (int index = 0; index < 2; ++index) {
                     panels.emplace_back(index == 0 ? String{u8"a"} : String{u8"b"}, CollapseTitle{[&] {
                                             ++mounts;
                                             Text(TypographyProps{}.content(u8"Title").mark(true));
                                         }},
                                         CollapseBody{[] { Text(u8"Body"); }});
                     panels.back().extra(CollapseExtra{[] { Text(TypographyProps{}.content(u8"Extra").mark(true)); }});
                 }
                 Collapse(CollapseProps{}.layout(LayoutStyle{}.height(height)), panels);
                 Flex(FlexProps{}.layout(LayoutStyle{}.height(dp(32))), FlexContent{[] {}});
             }});
    }});
    sync(f);
    const auto id = f.services.collapse().mounted()[0];
    const auto initial = f.services.collapse().snapshot(id);
    const auto parent = f.services.components().parent(id).value();
    const auto sibling = f.services.components().children(parent).back();
    check(std::ranges::any_of(f.buttons.button_scene().instances().instances(),
                              [](const auto& quad) {
                                  return quad.opacity > 0 && quad.color[3] > 0 && quad.bounds[2] > 0 &&
                                         quad.bounds[3] > 0;
                              }),
          "clipping fixture lacks real decoration quads");
    const auto inside = [](runtime::Rect visible, runtime::Rect clip) {
        return visible.width <= 0 || visible.height <= 0 ||
               (visible.x >= clip.x - .001F && visible.y >= clip.y - .001F &&
                visible.x + visible.width <= clip.x + clip.width + .001F &&
                visible.y + visible.height <= clip.y + clip.height + .001F);
    };
    for (float value : {48.0F, 55.0F, 32.0F, 0.0F, 48.0F}) {
        height.set(dp(value));
        sync(f);
        const auto clip = bounds(f, id);
        check(clip.height == value && bounds(f, sibling).y == clip.y + value, "constrained Collapse displaced sibling");
        for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
            for (std::uint32_t index = command.first_instance; index < command.first_instance + command.instance_count;
                 ++index) {
                if (command.kind == graphics::SceneDrawKind::glyph) {
                    const auto& glyph = f.scene.glyph_scene().instances().at(index);
                    if (glyph.translation_opacity[2] <= 0 || glyph.color[3] <= 0) {
                        continue;
                    }
                    const runtime::Rect rect{glyph.position_size[0] + glyph.translation_opacity[0],
                                             glyph.position_size[1] + glyph.translation_opacity[1],
                                             glyph.position_size[2], glyph.position_size[3]};
                    const runtime::Rect glyph_clip{glyph.clip_bounds[0], glyph.clip_bounds[1],
                                                   glyph.clip_bounds[2] - glyph.clip_bounds[0],
                                                   glyph.clip_bounds[3] - glyph.clip_bounds[1]};
                    check(inside(graphics::intersect_effect_bounds(rect, glyph_clip), clip),
                          "constrained header glyph painted into sibling");
                } else if (command.kind == graphics::SceneDrawKind::quad) {
                    const auto& quad = f.buttons.button_scene().instances().at(index);
                    if (quad.opacity > 0 && quad.color[3] > 0) {
                        check(inside({quad.bounds[0] + quad.translation[0], quad.bounds[1] + quad.translation[1],
                                      quad.bounds[2], quad.bounds[3]},
                                     clip),
                              "constrained header decoration quad painted into sibling");
                    }
                }
            }
        }
        for (const auto& panel : initial.panels) {
            const auto header = bounds(f, panel.header);
            const auto& arrow = f.nodes.require(panel.arrow).bounds;
            check(inside(arrow, header), "arrow was centered using unclamped height");
            check(inside(bounds(f, panel.title), header) && inside(bounds(f, panel.extra), header),
                  "title/extra centered outside constrained header");
        }
    }
    check(mounts == 2, "constrained resize remounted titles");
}

void unfocused_popup_does_not_resurrect() {
    for (bool controlled : {false, true}) {
        Fixture f;
        Signal<CollapseKeys> active{CollapseKeys{String{u8"a"}}};
        Signal<String> placeholder{String{u8"Choose"}};
        Signal<bool> open{true};
        std::vector<bool> intents;
        f.services.mount(Content{[&] {
            Collapse(CollapseProps{}.activeKeys(active),
                     {CollapsePanel{u8"a", CollapseTitle{[] { Text(u8"A"); }}, CollapseBody{[&] {
                                        SelectProps props;
                                        props.options({{1, u8"One"}, {2, u8"Two"}})
                                            .placeholder(placeholder)
                                            .onOpenChange([&](bool value) { intents.push_back(value); });
                                        if (controlled) {
                                            props.open(open);
                                        } else {
                                            props.defaultOpen(true);
                                        }
                                        Select(props);
                                    }}}});
        }});
        sync(f);
        const auto id = f.services.select().mounted()[0];
        const auto selector = f.services.select().snapshot(id).selector;
        check(f.services.select().snapshot(id).visible && !f.services.focus().state().focused,
              "unfocused popup fixture failed");
        for (int cycle = 0; cycle < 3; ++cycle) {
            const auto before = intents.size();
            active.set({});
            sync(f);
            check(!f.services.select().snapshot(id).visible, "hidden body popup remained visible");
            active.set({String{u8"a"}});
            sync(f);
            placeholder.set(cycle % 2 == 0 ? String{u8"Changed"} : String{u8"Again"});
            sync(f);
            check(!f.services.select().snapshot(id).visible,
                  "unfocused hidden popup resurrected on placeholder update");
            check(intents.size() > before && !intents.back(), "hidden popup did not issue a close intent");
            if (controlled) {
                check(open.get(), "cleanup changed controlled owner value");
            }
            if (controlled) {
                open.set(false);
                open.set(true);
            } else {
                click(f, selector);
            }
            sync(f);
            check(f.services.select().snapshot(id).visible, "explicit popup reopen failed");
        }
        check(f.services.destroy(f.services.collapse().mounted().front()), "popup fixture cleanup failed");
    }
}

void popup_cleanup_matches_modal_and_spin() {
    for (bool controlled : {false, true}) {
        for (bool modal_boundary : {false, true}) {
            Fixture f;
            Signal<bool> modal{false};
            Signal<bool> spinning{false};
            Signal<bool> open{true};
            Signal<String> placeholder{String{u8"Choose"}};
            int closes{};
            f.services.mount(Content{[&] {
                Spin(SpinProps{}.spinning(spinning), SpinContent{[&] {
                         Collapse(CollapseProps{}.defaultActiveKeys({String{u8"a"}}),
                                  {CollapsePanel{u8"a", CollapseTitle{[] { Text(u8"A"); }}, CollapseBody{[&] {
                                                     SelectProps props;
                                                     props.options({{1, u8"One"}})
                                                         .placeholder(placeholder)
                                                         .onOpenChange([&](bool value) {
                                                             if (!value) {
                                                                 ++closes;
                                                             }
                                                         });
                                                     if (controlled) {
                                                         props.open(open);
                                                     } else {
                                                         props.defaultOpen(true);
                                                     }
                                                     Select(props);
                                                 }}}});
                     }});
                Modal(ModalProps{}.open(modal).footer(ModalFooterMode::None), ModalContent{[] { Text(u8"Modal"); }});
            }});
            sync(f);
            const auto id = f.services.select().mounted()[0];
            check(f.services.select().snapshot(id).visible && !f.services.focus().state().focused,
                  "boundary popup fixture was already focused");
            for (int cycle = 0; cycle < 2; ++cycle) {
                const auto before = closes;
                if (modal_boundary) {
                    modal.set(true);
                } else {
                    spinning.set(true);
                }
                sync(f);
                check(!f.services.select().snapshot(id).visible && closes > before,
                      "boundary failed to suppress unfocused popup");
                if (modal_boundary) {
                    modal.set(false);
                } else {
                    spinning.set(false);
                }
                sync(f);
                placeholder.set(cycle == 0 ? String{u8"Changed"} : String{u8"Again"});
                sync(f);
                check(!f.services.select().snapshot(id).visible && (!controlled || open.get()),
                      "boundary cleanup resurrected popup or changed controlled parent");
                if (controlled) {
                    open.set(false);
                    open.set(true);
                    sync(f);
                } else {
                    click(f, f.services.select().snapshot(id).selector);
                }
                check(f.services.select().snapshot(id).visible, "boundary recovery prevented explicit open");
            }
            check(f.services.destroy(f.services.feedback().spins().front()), "boundary popup cleanup failed");
        }
    }
    for (bool controlled : {false, true}) {
        Fixture f;
        Signal<CollapseKeys> active{CollapseKeys{String{u8"a"}}};
        Signal<bool> open{true};
        runtime::ComponentId owner;
        int closes{};
        f.services.mount(Content{[&] {
            Collapse(CollapseProps{}.activeKeys(active),
                     {CollapsePanel{u8"a", CollapseTitle{[] { Text(u8"A"); }}, CollapseBody{[&] {
                                        SelectProps props;
                                        props.options({{1, u8"One"}}).onOpenChange([&](bool value) {
                                            if (!value) {
                                                ++closes;
                                                static_cast<void>(f.services.destroy(owner));
                                            }
                                        });
                                        if (controlled) {
                                            props.open(open);
                                        } else {
                                            props.defaultOpen(true);
                                        }
                                        Select(props);
                                    }}}});
        }});
        sync(f);
        owner = f.services.collapse().mounted().front();
        active.set({});
        sync(f);
        check(closes == 1 && f.services.collapse().mounted().empty() && f.services.select().mounted().empty() &&
                  f.inputs.mounted_inputs().empty() && f.services.interactions().size() == 0,
              "popup close callback disposal retained resources");
    }
}

void extra_button_focus_ring_uses_header_clip() {
    Fixture f;
    Signal<bool> disabled{false};
    Signal<ControlSize> size{ControlSize::Middle};
    int actions{};
    int mounts{};
    f.services.mount(Content{[&] {
        Collapse(CollapseProps{}.size(size), {plain(u8"a").disabled(disabled).extra(CollapseExtra{[&] {
                     ++mounts;
                     Button(ButtonProps{}.onClick([&] { ++actions; }), [] { Text(u8"Action"); });
                 }})});
    }});
    sync(f);
    const auto id = f.services.collapse().mounted().front();
    const auto header = f.services.collapse().snapshot(id).panels[0].header;
    const auto button = f.buttons.mounted_buttons().front();
    key(f, Key::tab);
    key(f, Key::tab);
    check(f.services.focus().state().focused == button.interaction &&
              f.buttons.snapshot(button.component).focus.focus_visible,
          "extra native Button did not receive visible keyboard focus");
    for (auto value : {ControlSize::Middle, ControlSize::Small, ControlSize::Large}) {
        size.set(value);
        sync(f);
        const auto effects = f.services.rounded_effects().packed_instances();
        const auto effect = std::ranges::find_if(effects, [](const auto& e) {
            return e.geometry.kind == graphics::RoundedEffectKind::outline && e.geometry.outline_width == 3 &&
                   e.geometry.outline_offset == 1 && e.material.opacity > 0 && e.material.visible;
        });
        check(effect != effects.end() && effect->geometry.ancestor_clip.has_value(),
              "extra Button focus effect missing");
        auto unclipped = *effect;
        unclipped.geometry.ancestor_clip.reset();
        const auto outer = graphics::rounded_effect_bounds(unclipped, 0);
        const auto clip = effect->geometry.ancestor_clip->bounds;
        const auto contains = [](runtime::Rect bounds, runtime::Rect rect) {
            return rect.x >= bounds.x && rect.y >= bounds.y && rect.x + rect.width <= bounds.x + bounds.width &&
                   rect.y + rect.height <= bounds.y + bounds.height;
        };
        check(contains(clip, outer) && contains(bounds(f, header), clip),
              "extra Button focus ring clipped by tight content bounds");
        const auto b = bounds(f, button.component);
        const runtime::Point point{b.x - 2.5F, b.y + b.height / 2};
        check(point.x >= clip.x && point.x <= clip.x + clip.width && point.y >= clip.y &&
                  point.y <= clip.y + clip.height && graphics::rounded_effect_coverage(point, *effect) > .5F,
              "extra Button focus ring does not paint outside control");
    }
    disabled.set(true);
    sync(f);
    click(f, button.component);
    key(f, Key::enter);
    check(actions == 2 && mounts == 1 && f.services.collapse().snapshot(id).active_keys.empty(),
          "extra viewport changed native activation or remounted slot");
    check(f.services.destroy(id), "extra focus fixture cleanup failed");
}

void pointer_header_keyboard_focus_paints() {
    Fixture f;
    int changes{};
    f.services.mount(
        Content{[&] { Collapse(CollapseProps{}.onChange([&](CollapseKeys) { ++changes; }), {plain(u8"a")}); }});
    sync(f);
    const auto id = f.services.collapse().mounted().front();
    const auto header = f.services.collapse().snapshot(id).panels[0].header;
    const auto painted = [&] {
        const auto h = bounds(f, header);
        return std::ranges::any_of(f.services.rounded_effects().packed_instances(), [&](const auto& e) {
            return e.geometry.kind == graphics::RoundedEffectKind::outline && e.geometry.outline_width == 3 &&
                   e.material.opacity > 0 && e.material.visible &&
                   graphics::rounded_effect_coverage({h.x + 1.5F, h.y + h.height / 2}, e) > .5F;
        });
    };
    for (auto value : {Key::space, Key::enter}) {
        click(f, header);
        check(f.services.focus().state().modality == FocusModality::pointer &&
                  !f.services.focus().state().focus_visible && !painted(),
              "pointer header unexpectedly has a keyboard focus ring");
        const auto before = changes;
        f.services.focus().dispatch({value, KeyAction::down});
        sync(f);
        check(changes == before + 1 && f.services.focus().state().focused == interaction(f, header) &&
                  f.services.focus().state().modality == FocusModality::keyboard &&
                  f.services.focus().state().focus_visible && painted(),
              "pointer-to-keyboard header activation did not paint focus ring");
        f.services.focus().dispatch({value, KeyAction::down, KeyModifier::none, true});
        f.services.focus().dispatch({value, KeyAction::up});
        sync(f);
        check(changes == before + 1 && !f.services.focus().state().keyboard_pressed && painted(),
              "consumed repeat/keyup activated header again or removed focus ring");
        click(f, header);
        const auto after_click = changes;
        f.services.focus().dispatch({value, KeyAction::up});
        sync(f);
        check(changes == after_click && f.services.focus().state().modality == FocusModality::keyboard && painted(),
              "consumed keyup did not update header keyboard presentation");
    }
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        if (argc > 1) {
            const std::string_view repro{argv[1]};
            if (repro == "eligibility") {
                enabled_header_refreshes_pointer_hit();
            } else if (repro == "clipping") {
                constrained_headers_clip_actual_scene();
            } else if (repro == "popup") {
                unfocused_popup_does_not_resurrect();
                popup_cleanup_matches_modal_and_spin();
            } else if (repro == "extra-focus") {
                extra_button_focus_ring_uses_header_clip();
            } else if (repro == "keyboard-focus") {
                pointer_header_keyboard_focus_paints();
            } else {
                throw std::runtime_error("unknown Collapse regression");
            }
            return 0;
        }
        enabled_header_refreshes_pointer_hit();
        constrained_headers_clip_actual_scene();
        unfocused_popup_does_not_resurrect();
        popup_cleanup_matches_modal_and_spin();
        extra_button_focus_ring_uses_header_clip();
        pointer_header_keyboard_focus_paints();
        controlled_and_accordion();
        retained_drafts_reflow_and_cleanup();
        keyboard_extra_and_disabled_capture();
        logical_modal_and_spin_boundaries();
        theme_sizes_and_retained_material();
        callbacks_and_validation();
        std::cout << "Collapse retained component contracts passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
