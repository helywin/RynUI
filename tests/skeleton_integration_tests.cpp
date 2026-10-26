#include "component/skeleton_component.hpp"
#include "component/select_component.hpp"
#include "component/feedback_component.hpp"
#include "component/modal_component.hpp"
#include "component/collapse_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>

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
    check(f.services.layout_and_synchronize({500, 700}, {0, 0, 500, 700}), "Skeleton integration layout failed");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    for (const auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    }
    sync(f);
}

bool inside(runtime::Rect a, runtime::Rect b) {
    return a.width == 0 || a.height == 0 ||
           (a.x >= b.x && a.y >= b.y && a.x + a.width <= b.x + b.width + .01F &&
            a.y + a.height <= b.y + b.height + .01F);
}

void editor_and_native_events() {
    Fixture f;
    Signal<bool> loading{false};
    Signal<LogicalLength> width{dp(240)};
    InputRef ref;
    int mounts{};
    int clicks{};
    int blurs{};
    f.services.mount(Content{[&] {
        Skeleton(
            SkeletonProps{}.loading(loading).layout(LayoutStyle{}.width(width).height(dp(150))), SkeletonContent{[&] {
                ++mounts;
                Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[&] {
                         Input(InputProps{}
                                   .ref(ref)
                                   .defaultValue(u8"Original")
                                   .onBlur([&] { ++blurs; })
                                   .layout(LayoutStyle{}.margin_left(dp(8)).margin_right(dp(8)).margin_top(dp(8))));
                         Button(ButtonProps{}
                                    .onClick([&] { ++clicks; })
                                    .layout(LayoutStyle{}.margin_left(dp(8)).margin_right(dp(8))),
                                [] { Text(u8"Native action / 原生"); });
                     }});
            }});
    }});
    sync(f);
    const auto id = f.services.skeleton().mounted()[0];
    const auto original = f.services.skeleton().snapshot(id);
    const auto input = f.inputs.mounted_inputs()[0];
    const auto button = f.buttons.mounted_buttons()[0];
    const auto count = f.services.components().component_count();
    check(ref.focus({InputFocusCursor::All}), "Skeleton retained ref could not focus");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"普通草稿"}, f.inputs.sessions().active()})),
          "draft commit failed");
    const auto session = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"拼音"}, {1, 0}, session})), "preedit dispatch failed");
    check(f.inputs.editors().require(input.editor).composition().active, "preedit did not start");
    loading.set(true);
    sync(f);
    check(blurs == 1 && !f.inputs.sessions().active().valid() && !f.services.focus().state().focused &&
              !f.inputs.editors().require(input.editor).composition().active && !ref.focus() &&
              !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, session})),
          "loading retained preedit/session/focus or accepted stale input");
    click(f, button.component);
    check(clicks == 0, "loading accepted native Button event");
    loading.set(false);
    sync(f);
    check(f.inputs.mounted_inputs()[0].editor == input.editor &&
              f.services.skeleton().snapshot(id).content == original.content &&
              f.inputs.editors().require(input.editor).value() == String{u8"普通草稿"}.bytes() && mounts == 1 &&
              f.services.components().component_count() == count && ref.focus(),
          "Skeleton loading round trip lost editor/draft/refs/identity");
    click(f, button.component);
    check(clicks == 1 && f.services.focus().request_focus(button.interaction, FocusModality::keyboard),
          "recovered Button pointer/focus failed");
    f.services.focus().dispatch({Key::space, KeyAction::down});
    f.services.focus().dispatch({Key::space, KeyAction::up});
    sync(f);
    check(clicks == 2, "recovered native Button keyboard failed");
    const auto r = bounds(f, button.component);
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [&](const auto& effect) {
                                  return effect.material.visible && effect.material.opacity > 0 &&
                                         effect.geometry.kind == graphics::RoundedEffectKind::outline &&
                                         graphics::rounded_effect_coverage({r.x - 2, r.y + r.height / 2}, effect) > .1F;
                              }),
          "Skeleton clipped child focus decoration to its own surface");
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    check(f.services.pointer().state(PointerIdentity::mouse())->capture == button.interaction,
          "native Button capture absent");
    loading.set(true);
    sync(f);
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "loading retained pointer capture");
    loading.set(false);
    sync(f);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    check(clicks == 2, "cancelled loading gesture activated");
    // Exercise actual glyph and hit clips under a short external allocation.
    width.set(dp(32));
    sync(f);
    const auto clip = bounds(f, id);
    for (const auto& record : f.services.hit_test().records()) {
        check(inside(record.effective_clip, clip), "Skeleton child hit clip crossed allocation");
    }
    bool glyphs{};
    for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
        if (command.kind != graphics::SceneDrawKind::glyph) {
            continue;
        }
        for (auto i = command.first_instance; i < command.first_instance + command.instance_count; ++i) {
            const auto& glyph = f.scene.glyph_scene().instances().at(i);
            const runtime::Rect glyph_clip{glyph.clip_bounds[0], glyph.clip_bounds[1],
                                           glyph.clip_bounds[2] - glyph.clip_bounds[0],
                                           glyph.clip_bounds[3] - glyph.clip_bounds[1]};
            check(inside(glyph_clip, clip), "Skeleton glyph clip crossed content allocation");
            glyphs = true;
        }
    }
    check(glyphs && mounts == 1, "Skeleton clipped fixture lost native text");
}

void unfocused_and_controlled_select() {
    for (const bool controlled : {false, true}) {
        Fixture f;
        Signal<bool> loading{false};
        Signal<bool> open{true};
        Signal<String> placeholder{String{u8"Choose"}};
        int closes{};
        f.services.mount(Content{[&] {
            Skeleton(
                SkeletonProps{}.loading(loading), SkeletonContent{[&] {
                    SelectProps props;
                    props.options({{1, u8"One"}, {2, u8"Two"}}).placeholder(placeholder).onOpenChange([&](bool value) {
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
                }});
        }});
        sync(f);
        const auto id = f.services.select().mounted()[0];
        const auto original = f.services.select().snapshot(id);
        check(original.visible && !f.services.focus().state().focused, "unfocused Select fixture failed");
        loading.set(true);
        sync(f);
        check(!f.services.select().snapshot(id).visible && closes == 1, "Skeleton failed to close unfocused popup");
        loading.set(false);
        sync(f);
        placeholder.set(String{u8"Changed"});
        sync(f);
        check(!f.services.select().snapshot(id).visible && (!controlled || open.get()),
              "controlled refusal resurrected popup or changed owner value");
        if (controlled) {
            open.set(false);
            open.set(true);
            sync(f);
        } else {
            click(f, original.selector);
        }
        check(f.services.select().snapshot(id).visible &&
                  f.services.select().snapshot(id).selector == original.selector,
              "Select could not explicitly reopen with retained identity");
        click(f, f.services.select().snapshot(id).rows[0]);
        check(f.services.select().snapshot(id).value == SelectValue{1}, "native Select option event failed");
    }
}

void initial_nested_skeleton_retirement() {
    Fixture f;
    Signal<bool> loading{true};
    runtime::ComponentId retired;
    runtime::ComponentId parent;
    int closes = 0;
    int bodies = 0;
    f.services.mount(Content{[&] {
        Skeleton(SkeletonProps{}.loading(loading), SkeletonContent{[&] {
                     Select(SelectProps{}.open(true).onOpenChange([&](bool open) {
                         if (open) {
                             return;
                         }
                         ++closes;
                         const auto select = f.services.select().mounted().back();
                         parent = *f.services.components().parent(select);
                         retired = f.services.components().children(parent).back();
                         check(retired != select && f.services.destroy(retired),
                               "initial block did not retire nested Skeleton");
                     }));
                     Skeleton(SkeletonProps{}.loading(true), SkeletonContent{[&] {
                                  ++bodies;
                                  Text(u8"Retired body");
                              }});
                 }});
    }});
    sync(f);
    check(closes == 1 && retired.valid() && !f.services.components().contains(retired) && bodies == 0 &&
              f.services.skeleton().mounted().size() == 1,
          "nested Skeleton continued after initial callback retirement");
    InputRef ref;
    f.services.append_slot(parent, Content{[&] {
                               Skeleton(SkeletonProps{}.loading(false),
                                        SkeletonContent{[&] { Input(InputProps{}.ref(ref)); }});
                           }});
    sync(f);
    check(!ref.focus(), "replacement unlocked ancestor loading gate");
    loading.set(false);
    sync(f);
    check(ref.focus(), "nested Skeleton generation did not recover");
    f.services.dispose();
    check(!ref.focus() && f.services.surfaces().size() == 0, "nested Skeleton teardown leaked");
}

void synchronous_retirement_and_owner_isolation() {
    initial_nested_skeleton_retirement();
    for (const bool popup : {false, true}) {
        Fixture f;
        Signal<bool> loading{false};
        InputRef ref;
        InputRef replacement;
        InputRef other;
        runtime::ComponentId owner;
        runtime::ComponentId parent;
        int retired{};
        const auto retire = [&] {
            ++retired;
            check(f.services.destroy(owner), "callback failed to destroy old Skeleton");
            f.services.append_slot(parent, Content{[&] {
                                       Skeleton(SkeletonProps{}.loading(false),
                                                SkeletonContent{[&] { Input(InputProps{}.ref(replacement)); }});
                                   }});
        };
        f.services.mount(Content{[&] {
            Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                     Skeleton(SkeletonProps{}.loading(loading), SkeletonContent{[&] {
                                  if (popup) {
                                      Select(SelectProps{}
                                                 .options({{1, u8"One"}})
                                                 .defaultOpen(true)
                                                 .onOpenChange([&](bool value) {
                                                     if (!value) {
                                                         retire();
                                                     }
                                                 }));
                                  } else {
                                      Input(InputProps{}.ref(ref).onBlur(retire));
                                  }
                              }});
                     Skeleton(SkeletonProps{}, SkeletonContent{[&] { Input(InputProps{}.ref(other)); }});
                 }});
        }});
        sync(f);
        owner = f.services.skeleton().mounted()[0];
        parent = *f.services.components().parent(owner);
        const auto other_owner = f.services.skeleton().mounted()[1];
        const auto other_content = f.services.skeleton().snapshot(other_owner).content;
        if (!popup) {
            check(ref.focus(), "retirement fixture focus failed");
        }
        loading.set(true);
        sync(f);
        check(retired == 1 && !f.services.components().contains(owner) && !other.focus() && replacement.focus(),
              "callback retirement accessed stale state or released another owner's block");
        const auto next = f.services.skeleton().mounted().back();
        check(next != owner && f.services.components().contains(other_content) &&
                  !f.services.interactions().component_active(other_content),
              "generational owner isolation failed");
        check(f.services.destroy(next) && !replacement.focus() && !other.focus(),
              "replacement cleanup removed other owner");
    }
}

void outer_boundaries_and_motion() {
    Fixture f;
    Signal<bool> spinning{false};
    Signal<bool> loading{false};
    Signal<bool> modal{false};
    InputRef outside;
    InputRef inside;
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(spinning), SpinContent{[&] {
                 Skeleton(SkeletonProps{}.loading(loading), SkeletonContent{[&] { Input(InputProps{}.ref(outside)); }});
             }});
        Modal(ModalProps{}.open(modal).footer(ModalFooterMode::None), ModalContent{[&] {
                  Skeleton(SkeletonProps{}.loading(false), SkeletonContent{[&] { Input(InputProps{}.ref(inside)); }});
              }});
    }});
    sync(f);
    check(outside.focus(), "outside input focus failed");
    spinning.set(true);
    sync(f);
    loading.set(true);
    loading.set(false);
    sync(f);
    check(!outside.focus(), "Skeleton recovery unlocked outer Spin");
    spinning.set(false);
    modal.set(true);
    sync(f);
    check(!outside.focus() && inside.focus(), "Modal/Skeleton logical input ownership failed");
    modal.set(false);
    sync(f);
    check(outside.focus(), "Modal close failed to restore Skeleton content");
}

void hidden_animation_and_idle() {
    Fixture f;
    Signal<bool> loading{false};
    Signal<CollapseKeys> expanded{CollapseKeys{String{u8"a"}}};
    f.services.mount(Content{[&] {
        CollapsePanel panel{u8"a", CollapseTitle{[] { Text(u8"Motion"); }}, CollapseBody{[&] {
                                Skeleton(SkeletonProps{}.loading(loading), SkeletonContent{[] {
                                             Icon(IconProps{}.name(IconName::LoadingOutlined).spin(true));
                                         }});
                            }}};
        Collapse(CollapseProps{}.activeKeys(expanded), {panel});
    }});
    f.services.set_motion_preference(animation::MotionPreference::normal);
    sync(f);
    check(f.services.next_frame_deadline().has_value(), "visible retained icon did not animate");
    loading.set(true);
    sync(f);
    check(!f.services.next_frame_deadline(), "Skeleton kept hidden child animation active");
    static_cast<void>(f.frames.consume_request());
    sync(f);
    check(!f.frames.pending(), "static loading Skeleton requested continuous frame");
    loading.set(false);
    sync(f);
    check(f.services.next_frame_deadline().has_value(), "Skeleton did not resume child animation");
    f.services.set_window_active(false);
    check(!f.services.next_frame_deadline(), "inactive window retained icon animation");
    f.services.set_window_active(true);
    sync(f);
    check(f.services.next_frame_deadline().has_value(), "window activation did not restore icon");
    expanded.set({});
    sync(f);
    check(!f.services.next_frame_deadline(), "ancestor hidden boundary kept Skeleton child animation");
    expanded.set({String{u8"a"}});
    sync(f);
    check(f.services.next_frame_deadline().has_value(), "ancestor recovery lost retained icon");
}

void documented_example() {
    Fixture f;
    f.services.mount(Content{[] {
        ryn::Signal<bool> loading{true};
        ryn::Skeleton(ryn::SkeletonProps{}.loading(loading).avatar(true),
                      ryn::SkeletonContent{[] { ryn::Input(ryn::InputProps{}.defaultValue(u8"Retained draft")); }});
        ryn::SkeletonButton(ryn::SkeletonButtonProps{}.shape(ryn::SkeletonButtonShape::Round));
        ryn::SkeletonAvatar(ryn::SkeletonAvatarProps{}.size(ryn::ControlSize::Large));
        ryn::SkeletonInput(ryn::SkeletonInputProps{}.block(true));
    }});
    sync(f);
    check(f.services.skeleton().mounted().size() == 4 && f.inputs.mounted_inputs().size() == 1,
          "documented Skeleton example did not mount");
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "";
        if (name == "editor") {
            editor_and_native_events();
        } else if (name == "select") {
            unfocused_and_controlled_select();
        } else if (name == "retirement") {
            synchronous_retirement_and_owner_isolation();
        } else if (name == "boundaries") {
            outer_boundaries_and_motion();
        } else if (name == "motion") {
            hidden_animation_and_idle();
        } else if (name == "example") {
            documented_example();
        } else {
            throw std::invalid_argument("Select a Skeleton integration case");
        }
        std::cout << "Skeleton " << name << " integration passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
