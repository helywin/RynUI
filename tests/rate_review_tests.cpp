#include "support/rate_fixture.hpp"
#include <string_view>

namespace {
using namespace rate_test;
using Time = animation::AnimationTime;

void move(Fixture& f, float x, float y) {
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none, x, y});
    sync(f);
}

bool popup(Fixture& f) {
    return f.services.tooltip().snapshot(f.services.tooltip().mounted()[0]).visible;
}

void geometry(bool before) {
    Fixture f;
    Signal<LogicalLength> width{dp(20)};
    Signal<LogicalLength> height{dp(20)};
    InputRef ref;
    int mounts = 0;
    f.services.mount(Content{[&] {
        Rate(RateProps{}
                 .count(1)
                 .layout(LayoutStyle{}.width(width).height(height))
                 .tooltips({RateTooltip{TooltipTitle{[&] {
                     ++mounts;
                     Input(InputProps{}.defaultValue(u8"Draft").ref(ref));
                 }}}}));
    }});
    sync(f);
    const auto id = f.services.rate().mounted()[0];
    const auto item = f.services.rate().snapshot(id).items[0].component;
    move(f, 5, 8);
    check(popup(f) && ref.focus(), "geometry fixture did not focus rich Input");
    const auto input = f.inputs.mounted_inputs()[0];
    const auto session = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"预编辑"}, {0, 1}, session})), "preedit fixture failed");
    width.set(dp(0));
    height.set(dp(0));
    sync(f);
    const bool focused = f.services.focus().state().focused == input.interaction;
    const bool composing = f.inputs.editors().require(input.editor).composition().active;
    if (before) {
        std::cout << "BEFORE geometry: popup=" << popup(f) << " focused=" << focused
                  << " session=" << f.inputs.sessions().active().valid() << " preedit=" << composing << '\n';
        check(popup(f) && focused && composing && f.inputs.sessions().active().valid(),
              "geometry P2 did not reproduce");
        return;
    }
    check(!popup(f) && !focused && !composing && !f.inputs.sessions().active().valid() && !ref.focus(),
          "zero geometry retained popup/focus/session/preedit");
    check(!bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, session})), "zero geometry accepted stale session");
    no_capture(f);
    width.set(dp(20));
    height.set(dp(20));
    sync(f);
    check(!popup(f) && f.services.rate().snapshot(id).items[0].component == item,
          "geometry restore resurrected/remounted popup");
    move(f, 5, 8);
    check(popup(f) && ref.focus() && mounts == 1 && f.inputs.mounted_inputs()[0].editor == input.editor &&
              f.inputs.editors().require(input.editor).value() == "Draft",
          "geometry restore lost editor/draft/ref identity");
    f.services.dispose();
    check(blocked(f.services.interactions()).empty(), "geometry teardown leaked input blocks");
}

void pointer_gap(bool before) {
    Fixture f;
    InputRef ref;
    Signal<bool> read_only{false};
    runtime::ComponentId parent;
    f.services.mount(Content{[&] {
        auto& build = runtime::require_component_build_context();
        parent = build.mount_component<int>(0);
        const auto node = build.root(parent);
        f.layout.set_layout(node, layout::BoxLayout{});
        build.on_resource_cleanup(parent, [&f, node] { f.layout.remove_layout(node); });
        build.mount_slot(parent, Content{[&] {
                             Rate(RateProps{}.count(1).readOnly(read_only).tooltips({RateTooltip{
                                 TooltipTitle{[&] { Input(InputProps{}.defaultValue(u8"Reachable").ref(ref)); }}}}));
                         }});
    }});
    sync(f);
    move(f, 5, 8);
    check(popup(f), "pointer fixture did not open popup");
    const auto b = f.services.tooltip().snapshot(f.services.tooltip().mounted()[0]).bounds;
    check(b.y == 32, "default popup gap fixture drifted");
    if (before) {
        move(f, 5, 21);
        std::cout << "BEFORE pointer_gap: popup at (5,21)=" << popup(f) << " popup_top=" << b.y << '\n';
        check(!popup(f), "pointer-gap P2 did not reproduce");
        return;
    }
    std::int64_t micros = 0;
    for (int y = 9; y <= static_cast<int>(b.y) + 2; ++y) {
        micros += 1000;
        static_cast<void>(f.services.tick_animations(Time::microseconds(micros)));
        move(f, 5, static_cast<float>(y));
        check(popup(f), "continuous pointer path closed popup in trigger/popup gap");
        if (y == 21) {
            read_only.set(true);
            sync(f);
            check(popup(f), "readOnly transition canceled legitimate rich Tooltip traversal");
        }
    }
    const auto input = f.inputs.mounted_inputs()[0];
    const auto target = f.inputs.layout_snapshot(input.component).viewport;
    const float x = target.x + 6;
    const float y = target.y + target.height / 2;
    const int steps = static_cast<int>(std::ceil(std::max(std::abs(x - 5), std::abs(y - (b.y + 2)))));
    for (int i = 1; i <= steps; ++i) {
        const float part = static_cast<float>(i) / steps;
        micros += 1000;
        static_cast<void>(f.services.tick_animations(Time::microseconds(micros)));
        move(f, 5 + (x - 5) * part, b.y + 2 + (y - b.y - 2) * part);
        check(popup(f), "continuous pointer path could not enter native popup child");
    }
    check(!f.services.rate().next_auxiliary_deadline(), "popup entry did not cancel Rate leave deadline");
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, x, y});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, x, y});
    check(f.services.focus().state().focused == input.interaction && ref.focus(),
          "continuous mouse path could not focus Input");
    f.services.focus().clear_focus();
    move(f, 390, 290);
    const auto deadline = f.services.rate().next_auxiliary_deadline();
    check(deadline.has_value(), "leaving popup did not arm bounded grace");
    static_cast<void>(f.services.tick_animations(*deadline));
    sync(f);
    check(!popup(f) && !f.services.rate().next_auxiliary_deadline(), "expired leave grace retained popup/deadline");
    static_cast<void>(f.frames.consume_request());
    sync(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline(), "expired grace spins when idle");
    move(f, 5, 8);
    move(f, 5, 21);
    const auto stale_deadline = f.services.rate().next_auxiliary_deadline();
    check(stale_deadline.has_value(), "teardown grace fixture failed");
    const auto old = f.services.rate().mounted()[0];
    check(f.services.destroy(old), "pending grace owner teardown failed");
    f.services.append_slot(parent, Content{[] { Rate(RateProps{}.defaultValue(4)); }});
    sync(f);
    const auto fresh = f.services.rate().mounted()[0];
    static_cast<void>(f.services.tick_animations(*stale_deadline));
    sync(f);
    check(fresh != old && f.services.rate().snapshot(fresh).value == 4 && !ref.focus() &&
              !f.services.rate().next_auxiliary_deadline(),
          "stale leave deadline affected replacement generation/ref");
    f.services.dispose();
    check(blocked(f.services.interactions()).empty(), "leave deadline teardown leaked blocks");
}

void focus_exit(bool before) {
    Fixture f;
    Signal<bool> read_only{false};
    InputRef ref;
    f.services.mount(Content{[&] {
        Rate(RateProps{}.count(1).readOnly(read_only).tooltips(
            {RateTooltip{TooltipTitle{[&] { Input(InputProps{}.ref(ref)); }}}}));
        Button(ButtonProps{}, [] { Text(u8"After"); });
    }});
    sync(f);
    const auto id = f.services.rate().mounted()[0];
    const auto root = f.services.rate().snapshot(id).focus;
    const auto input = f.inputs.mounted_inputs()[0].interaction;
    key(f, Key::tab);
    check(f.services.focus().state().focused == root && popup(f), "first Tab did not enter Rate/open Tooltip");
    key(f, Key::tab);
    check(f.services.focus().state().focused == input && popup(f), "second Tab did not enter rich Input");
    key(f, Key::tab);
    check(f.services.focus().state().focused != root && f.services.focus().state().focused != input,
          "third Tab did not leave Rate subtree");
    if (before) {
        std::cout << "BEFORE focus_exit: popup after Rate->Input->Button=" << popup(f) << '\n';
        check(popup(f), "focus-exit P2 did not reproduce");
        return;
    }
    check(!popup(f), "Tooltip stayed open after native descendant focus left");
    key(f, Key::tab);
    key(f, Key::tab);
    check(f.services.focus().state().focused == input && popup(f), "Tab could not re-enter retained popup Input");
    read_only.set(true);
    sync(f);
    check(f.services.focus().state().focused == input && popup(f) && ref.focus(),
          "readOnly swallowed legitimate native popup focus");
    key(f, Key::tab);
    check(!popup(f) && f.services.focus().state().focused != input,
          "readOnly popup did not close on descendant Tab exit");
}

float outline_coverage(Fixture& f) {
    float coverage = 0;
    for (const auto& effect : f.services.surfaces().effects().packed_instances()) {
        if (effect.geometry.kind != graphics::RoundedEffectKind::outline || !effect.material.visible ||
            effect.material.opacity <= 0) {
            continue;
        }
        const auto r = effect.geometry.shape.rect;
        for (float y = r.y - 1; y <= r.y + r.height + 1; y += .5F) {
            for (float x = r.x - 1; x <= r.x + r.width + 1; x += .5F) {
                coverage = std::max(coverage, graphics::rounded_effect_coverage({x, y}, effect));
            }
        }
    }
    return coverage;
}

void keyboard_rebind() {
    Fixture f;
    runtime::ComponentId parent;
    InputRef ref;
    int changes = 0;
    f.services.mount(Content{[&] {
        auto& build = runtime::require_component_build_context();
        parent = build.mount_component<int>(0);
        const auto node = build.root(parent);
        f.layout.set_layout(node, layout::BoxLayout{});
        build.on_resource_cleanup(parent, [&f, node] { f.layout.remove_layout(node); });
        build.mount_slot(
            parent, Content{[&] {
                Rate(RateProps{}.count(1).onChange([&](float) { ++changes; }).tooltips({RateTooltip{TooltipTitle{[&] {
                    Input(InputProps{}.defaultValue(u8"Old").ref(ref));
                }}}}));
            }});
    }});
    sync(f);
    const auto old = f.services.rate().mounted()[0];
    const auto root = f.services.rate().snapshot(old).focus;
    click(f, f.services.rate().snapshot(old).items[0]);
    changes = 0;
    auto handlers = *f.services.interactions().require(root).focus_handlers;
    const auto original = handlers.state_changed;
    bool replaced = false;
    handlers.state_changed = [&, original](FocusPresentation state) {
        if (state.focused && state.focus_visible && !replaced) {
            replaced = true;
            check(f.services.destroy(old), "keyboard focus callback could not retire owner");
            f.services.append_slot(parent, Content{[&] {
                                       Rate(RateProps{}.defaultValue(4).tooltips({RateTooltip{TooltipTitle{
                                           [&] { Input(InputProps{}.defaultValue(u8"Replacement").ref(ref)); }}}}));
                                   }});
        }
        if (original) {
            original(state);
        }
    };
    f.services.interactions().set_focus_handlers(root, std::move(handlers));
    f.services.focus().dispatch({Key::right, KeyAction::down});
    sync(f);
    const auto fresh = f.services.rate().mounted()[0];
    check(replaced && fresh != old && f.services.rate().snapshot(fresh).value == 4 && changes == 0,
          "keyboard command accessed retired/replacement model after focus callback");
    move(f, 5, 8);
    check(ref.focus() && f.inputs.editors().require(f.inputs.mounted_inputs()[0].editor).value() == "Replacement",
          "focus callback teardown unbound replacement InputRef");
    f.services.dispose();
    check(blocked(f.services.interactions()).empty(), "keyboard focus rebind leaked input blocks");
}

void keyboard(bool before) {
    Fixture f;
    std::vector<float> changes;
    f.services.mount(Content{[&] { Rate(RateProps{}.allowHalf().onChange([&](float v) { changes.push_back(v); })); }});
    sync(f);
    const auto id = f.services.rate().mounted()[0];
    click(f, f.services.rate().snapshot(id).items[1], .75F);
    check(!f.services.focus().state().focus_visible, "pointer modality fixture drifted");
    changes.clear();
    f.services.focus().dispatch({Key::right, KeyAction::down});
    sync(f);
    const auto value = f.services.rate().snapshot(id);
    const float coverage = outline_coverage(f);
    const float emphasis = bounds(f, value.items[2].background).width;
    if (before) {
        std::cout << "BEFORE keyboard: value=" << value.value
                  << " focus_visible=" << f.services.focus().state().focus_visible << " outline_coverage=" << coverage
                  << " emphasis_width=" << emphasis << '\n';
        check(value.value == 2.5F && !f.services.focus().state().focus_visible && coverage == 0 && emphasis == 20,
              "keyboard modality P2 did not reproduce");
        return;
    }
    check(value.value == 2.5F && f.services.focus().state().focus_visible && coverage > .1F &&
              std::abs(emphasis - 22) < .01F,
          "navigation did not publish keyboard focus outline coverage/emphasis");
    check(changes.size() == 1, "Right submitted multiple changes");
    f.services.focus().dispatch({Key::right, KeyAction::up});
    check(changes.size() == 1, "Right keyup submitted a change");
    f.services.focus().dispatch({Key::right, KeyAction::down, KeyModifier::none, true});
    check(changes.size() == 2 && changes.back() == 3, "Right repeat did not submit exactly one step");
    key(f, Key::home);
    key(f, Key::end);
    check(changes.size() == 4 && changes.back() == 5 && f.services.focus().state().focus_visible &&
              outline_coverage(f) > .1F,
          "Home/End lost keyboard presentation or submitted extra changes");
    for (auto navigation : {Key::home, Key::end}) {
        click(f, f.services.rate().snapshot(id).items[1], .75F);
        check(!f.services.focus().state().focus_visible, "Home/End pointer reset failed");
        const auto count = changes.size();
        key(f, navigation);
        check(f.services.focus().state().focus_visible && outline_coverage(f) > .1F && changes.size() == count + 1,
              "Home/End did not switch pointer focus to keyboard exactly once");
    }
    f.services.focus().dispatch({Key::enter, KeyAction::down});
    const auto n = changes.size();
    f.services.focus().dispatch({Key::enter, KeyAction::down, KeyModifier::none, true});
    f.services.focus().dispatch({Key::enter, KeyAction::up});
    check(changes.size() == n && changes.back() == 0, "activation repeat/keyup submitted extra changes");
    move(f, 390, 290);
    check(!f.services.rate().next_auxiliary_deadline(), "Rate without tooltips armed an unused leave deadline");
    keyboard_rebind();
}
} // namespace

int main(int argc, char** argv) {
    try {
        check(argc > 1, "Rate review case is required");
        const std::string_view name{argv[1]};
        const bool before = argc > 2 && std::string_view{argv[2]} == "--before";
        if (name == "geometry") {
            geometry(before);
        } else if (name == "pointer_gap") {
            pointer_gap(before);
        } else if (name == "focus_exit") {
            focus_exit(before);
        } else if (name == "keyboard") {
            keyboard(before);
        } else {
            throw std::runtime_error("unknown Rate review case");
        }
        std::cout << "Rate R1 " << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
