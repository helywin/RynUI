#include "component/descriptions_component.hpp"
#include "component/feedback_component.hpp"
#include "component/modal_component.hpp"
#include "component/select_component.hpp"
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
    check(f.services.layout_and_synchronize({600, 800}, {0, 0, 600, 800}), "integration layout failed");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    }
    sync(f);
}

DescriptionsLabel label() {
    return DescriptionsLabel{[] { Text(u8"Field"); }};
}

// Same read-only registry observation used by the native navigation lifetime fixture.
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

const auto& blocked_roots(const InteractionRegistry& registry) {
    return registry.*blocked_roots_member(BlockedRootsMember{});
}

void popup_and_reentrant_cleanup() {
    Fixture f;
    Signal<bool> shown{true};
    Signal<String> placeholder{String{u8"Choose"}};
    int changes = 0;
    f.services.mount(Content{[&] {
        Descriptions(DescriptionsProps{}.column(1).bordered(),
                     {DescriptionsItem{u8"select", label(), DescriptionsContent{[&] {
                                           Select(SelectProps{}
                                                      .options({{1, u8"One"}, {2, u8"Two"}})
                                                      .defaultOpen(true)
                                                      .placeholder(placeholder)
                                                      .onChange([&](SelectSelection) { ++changes; }));
                                       }}}
                          .visible(shown)});
    }});
    sync(f);
    const auto owner = f.services.descriptions().mounted()[0];
    const auto select = f.services.select().mounted()[0];
    check(f.services.select().snapshot(select).visible && !f.services.focus().state().focused,
          "unfocused default-open Select fixture failed");
    shown.set(false);
    sync(f);
    check(!f.services.select().snapshot(select).visible, "hidden unfocused popup remained visible");
    shown.set(true);
    placeholder.set(u8"Changed");
    sync(f);
    check(!f.services.select().snapshot(select).visible, "ordinary update resurrected suppressed popup");
    click(f, f.services.select().snapshot(select).selector);
    const auto popup = f.services.select().snapshot(select);
    check(popup.visible && popup.popup.height > 0 && !popup.rows.empty(), "rich Select did not reopen");
    click(f, popup.rows[1]);
    check(changes == 1 && f.services.select().snapshot(select).value == SelectSelection{2},
          "rich Select event/value failed");
    check(f.services.destroy(owner), "popup teardown failed");
    check(blocked_roots(f.services.interactions()).empty(), "popup teardown leaked a block");

    runtime::ComponentId disposable;
    Fixture g;
    Signal<bool> visible{true};
    int closes = 0;
    g.services.mount(Content{[&] {
        Descriptions(DescriptionsProps{},
                     {DescriptionsItem{
                         u8"callback", label(), DescriptionsContent{[&] {
                             Select(
                                 SelectProps{}.options({{1, u8"One"}}).defaultOpen(true).onOpenChange([&](bool open) {
                                     if (!open) {
                                         ++closes;
                                         check(g.services.destroy(disposable), "callback destruction failed");
                                     }
                                 }));
                         }}}
                          .visible(visible)});
    }});
    sync(g);
    disposable = g.services.descriptions().mounted()[0];
    visible.set(false);
    sync(g);
    check(closes == 1 && g.services.descriptions().mounted().empty() && g.services.select().mounted().empty() &&
              g.services.components().component_count() == 0 && blocked_roots(g.services.interactions()).empty(),
          "close callback unmount retained state or block");
}

void owned_blocks_and_focus_callbacks() {
    Fixture f;
    int clicks = 0;
    runtime::ComponentId append_parent;
    f.services.mount(Content{[&] {
        Button(ButtonProps{}.onClick([&] { ++clicks; }), [] { Text(u8"Protected"); });
        auto& build = runtime::require_component_build_context();
        append_parent = build.mount_component<int>(0);
        const auto node = build.root(append_parent);
        f.layout.set_layout(node, layout::BoxLayout{{}, true});
        build.on_resource_cleanup(append_parent, [&f, node] { f.layout.remove_layout(node); });
    }});
    sync(f);
    const auto protected_button = f.buttons.mounted_buttons()[0].component;
    f.services.set_input_blocked(protected_button, true);
    const auto baseline = blocked_roots(f.services.interactions());
    const auto count = f.services.components().component_count();
    for (int cycle = 0; cycle < 4; ++cycle) {
        Signal<bool> shown{cycle % 2 != 0};
        InputRef ref;
        runtime::ComponentId owner;
        bool destroy_on_blur = false;
        f.services.append_slot(
            append_parent, Content{[&] {
                Descriptions(DescriptionsProps{}.column(1).bordered(),
                             {DescriptionsItem{u8"input", label(), DescriptionsContent{[&] {
                                                   Input(InputProps{}.ref(ref).defaultValue(u8"Draft").onBlur([&] {
                                                       if (destroy_on_blur) {
                                                           check(f.services.destroy(owner),
                                                                 "blur callback unmount failed");
                                                       }
                                                   }));
                                               }}}
                                  .visible(shown)});
            }});
        sync(f);
        owner = f.services.descriptions().mounted()[0];
        if (cycle % 2 != 0) {
            check(ref.focus(), "visible Input focus failed");
            const auto input = f.inputs.mounted_inputs()[0];
            const auto r = f.inputs.layout_snapshot(input.component).viewport;
            f.services.pointer().dispatch(
                {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, r.x + 4, r.y + r.height / 2});
            check(f.services.pointer().state(PointerIdentity::mouse())->capture == input.interaction,
                  "Input capture fixture failed");
            destroy_on_blur = true;
            shown.set(false);
            sync(f);
            check(!f.services.pointer().state(PointerIdentity::mouse())->capture,
                  "hidden/unmounted input retained capture");
        } else {
            check(blocked_roots(f.services.interactions()).size() == baseline.size() + 1 && !ref.focus(),
                  "initial hidden branch did not register owned block");
            check(f.services.destroy(owner), "hidden owner teardown failed");
            sync(f);
        }
        check(blocked_roots(f.services.interactions()) == baseline &&
                  f.services.components().component_count() == count && f.inputs.mounted_inputs().empty() &&
                  !f.services.interactions().component_active(protected_button),
              "owned cleanup leaked registrations or removed another owner's block");
    }
    f.services.set_input_blocked(protected_button, false);
    sync(f);
    click(f, protected_button);
    check(clicks == 1 && blocked_roots(f.services.interactions()).empty(), "unrelated owner did not recover");
}

void modal_spin_and_drafts() {
    Fixture f;
    Signal<bool> spinning{false};
    Signal<bool> modal{false};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    InputRef ref;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Spin(SpinProps{}.spinning(spinning), SpinContent{[&] {
                           Descriptions(DescriptionsProps{}.column(1).bordered(),
                                        {DescriptionsItem{u8"edit", label(), DescriptionsContent{[&] {
                                                              Input(InputProps{}.ref(ref).defaultValue(u8"Initial"));
                                                          }}},
                                         DescriptionsItem{u8"choice", label(), DescriptionsContent{[] {
                                                              Select(SelectProps{}.options({{1, u8"One"}}));
                                                          }}}});
                       }});
                  Modal(ModalProps{}.open(modal).title(u8"Boundary").restoreFocusAfterClose(false),
                        ModalContent{[] { Text(u8"Modal"); }});
              }});
    }});
    sync(f);
    const auto input = f.inputs.mounted_inputs()[0];
    const auto select = f.services.select().mounted()[0];
    check(ref.focus({InputFocusCursor::All}), "draft focus failed");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"Saved draft"}, f.inputs.sessions().active()})),
          "draft commit failed");
    const auto session = f.inputs.sessions().active();
    auto config = std::make_shared<ThemeConfig>();
    config->algorithms = {ThemeAlgorithm::Dark};
    theme.set(*config);
    sync(f);
    check(f.inputs.sessions().active() == session && f.inputs.editors().require(input.editor).value() == "Saved draft",
          "ordinary Theme reset draft/session");
    modal.set(true);
    sync(f);
    check(!ref.focus() && !f.inputs.sessions().active().valid() &&
              !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, session})),
          "Modal failed to retire input session");
    modal.set(false);
    sync(f);
    check(ref.focus(), "Modal close failed to restore eligibility");
    spinning.set(true);
    sync(f);
    check(!ref.focus() && !f.inputs.sessions().active().valid(), "Spin did not block rich input");
    spinning.set(false);
    sync(f);
    click(f, f.services.select().snapshot(select).selector);
    check(f.services.select().snapshot(select).visible, "post-Spin Select cannot open");
    spinning.set(true);
    sync(f);
    check(!f.services.select().snapshot(select).visible, "Spin retained rich popup");
    spinning.set(false);
    sync(f);
    check(!f.services.select().snapshot(select).visible && f.inputs.mounted_inputs()[0].editor == input.editor &&
              f.inputs.editors().require(input.editor).value() == "Saved draft",
          "boundary recovery reopened popup/remounted editor");
    f.services.dispose();
    check(blocked_roots(f.services.interactions()).empty(), "Spin/Descriptions disposal retained block");
}

bool inside(runtime::Rect r, runtime::Rect clip) {
    return r.width <= 0 || r.height <= 0 ||
           (r.x >= clip.x - .01F && r.y >= clip.y - .01F && r.x + r.width <= clip.x + clip.width + .01F &&
            r.y + r.height <= clip.y + clip.height + .01F);
}

void finite_scene_theme_and_idle() {
    Fixture f;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<LogicalLength> width{dp(500)};
    Signal<LogicalLength> height{dp(220)};
    int mounts = 0;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Descriptions(
                      DescriptionsProps{}.column(2).bordered().layout(LayoutStyle{}.width(width).height(height)),
                      {DescriptionsItem{u8"rich", label(), DescriptionsContent{[&] {
                                            ++mounts;
                                            Button(ButtonProps{}, [] { Text(u8"Action"); });
                                        }}},
                       DescriptionsItem{
                           u8"wrap", DescriptionsLabel{[] {
                               Text(TypographyProps{}.content(u8"Long label wraps within narrow space").mark(true));
                           }},
                           DescriptionsContent{[] {
                               Text(TypographyProps{}
                                        .content(u8"Long value must wrap and clip in a finite cell")
                                        .mark(true));
                           }}}});
              }});
    }});
    sync(f);
    const auto owner = f.services.descriptions().mounted()[0];
    const auto button = f.buttons.mounted_buttons()[0];
    check(f.services.focus().request_focus(button.interaction, FocusModality::keyboard),
          "rich Button keyboard focus failed");
    sync(f);
    const auto effect = std::ranges::find_if(f.services.rounded_effects().packed_instances(), [](const auto& e) {
        return e.geometry.kind == graphics::RoundedEffectKind::outline && e.geometry.outline_width == 3 &&
               e.material.opacity > 0 && e.material.visible;
    });
    check(effect != f.services.rounded_effects().packed_instances().end() && effect->geometry.ancestor_clip,
          "rich focus outline missing");
    const auto b = bounds(f, button.component);
    check(graphics::rounded_effect_coverage({b.x - 2.5F, b.y + b.height / 2}, *effect) > .5F,
          "rich child tight root clip removed outward focus decoration");
    bool glyph_seen = false;
    bool quad_seen = false;
    for (auto algorithm : {ThemeAlgorithm::Default, ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
        auto config = std::make_shared<ThemeConfig>();
        config->algorithms = {algorithm};
        theme.set(*config);
        bool label_color_seen = false;
        for (const auto size :
             {runtime::Size{500, 220}, runtime::Size{140, 55}, runtime::Size{0, 0}, runtime::Size{500, 220}}) {
            width.set(dp(size.width));
            height.set(dp(size.height));
            sync(f);
            const auto clip = bounds(f, owner);
            check(clip.width == size.width && clip.height == size.height, "finite root ignored constraints");
            const auto parts = f.services.descriptions().snapshot(owner);
            for (const auto& item : parts.items) {
                check(inside(bounds(f, item.viewport), clip) && inside(bounds(f, item.label), clip) &&
                          inside(bounds(f, item.content), clip),
                      "actual constrained regions escaped root");
            }
            for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
                for (auto i = command.first_instance; i < command.first_instance + command.instance_count; ++i) {
                    if (command.kind == graphics::SceneDrawKind::glyph) {
                        const auto& g = f.scene.glyph_scene().instances().at(i);
                        const auto color =
                            f.services.components().theme_scope(owner)->snapshot().descriptions().colors.bordered_label;
                        label_color_seen |=
                            g.color == std::array{color.red(), color.green(), color.blue(), color.alpha()};
                        const runtime::Rect r{g.position_size[0] + g.translation_opacity[0],
                                              g.position_size[1] + g.translation_opacity[1], g.position_size[2],
                                              g.position_size[3]};
                        const runtime::Rect gc{g.clip_bounds[0], g.clip_bounds[1], g.clip_bounds[2] - g.clip_bounds[0],
                                               g.clip_bounds[3] - g.clip_bounds[1]};
                        glyph_seen |= g.translation_opacity[2] > 0 && g.color[3] > 0;
                        check(inside(graphics::intersect_effect_bounds(r, gc), clip),
                              "actual glyph escaped panel clip");
                    } else if (command.kind == graphics::SceneDrawKind::quad) {
                        const auto& q = f.buttons.button_scene().instances().at(i);
                        if (q.opacity > 0 && q.color[3] > 0) {
                            quad_seen = true;
                            check(inside({q.bounds[0] + q.translation[0], q.bounds[1] + q.translation[1], q.bounds[2],
                                          q.bounds[3]},
                                         clip),
                                  "actual quad escaped panel clip");
                        }
                    }
                }
            }
            check(!f.services.hit_test().hit_test({clip.x + clip.width + 1, clip.y + 2}), "hit escaped finite clip");
        }
        check(label_color_seen, "Theme label foreground never reached actual glyph material");
    }
    check(glyph_seen && quad_seen && mounts == 1, "scene fixture lacked actual glyph/quad or remounted content");
    const auto before = f.services.surfaces().diagnostics();
    static_cast<void>(f.frames.consume_request());
    sync(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline() &&
              f.services.surfaces().diagnostics().geometry_updates == before.geometry_updates &&
              f.services.surfaces().diagnostics().material_updates == before.material_updates,
          "idle Descriptions scheduled frame or republished geometry");
    height.set(dp(180));
    check(f.frames.pending(), "Prop update did not wake normal DirtyQueues");
    sync(f);
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        const auto which = argc > 1 ? std::string_view{argv[1]} : std::string_view{};
        if (which.empty() || which == "popup") {
            popup_and_reentrant_cleanup();
        }
        if (which.empty() || which == "cleanup") {
            owned_blocks_and_focus_callbacks();
        }
        if (which.empty() || which == "boundary") {
            modal_spin_and_drafts();
        }
        if (which.empty() || which == "scene") {
            finite_scene_theme_and_idle();
        }
        std::cout << "Descriptions integration contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
