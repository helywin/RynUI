#include "component/selection_component.hpp"
#include "support/input_fixture.hpp"

#include <ryn/rynui.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

bool near(float a, float b) {
    return std::abs(a - b) < 0.02F;
}

void activate(Fixture& f, input::InteractionId id) {
    if (f.services.focus().state().focused != id) {
        require(f.services.focus().request_focus(id, input::FocusModality::keyboard), "Switch focus failed");
    }
    f.services.focus().dispatch({input::Key::space, input::KeyAction::down});
    f.services.focus().dispatch({input::Key::space, input::KeyAction::up});
}

void retained_content_and_direction() {
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    Signal<bool> checked{false};
    Signal<SwitchDirection> direction{SwitchDirection::LeftToRight};
    Signal<String> caption{u8"关闭 Disabled"};
    int on_runs{};
    int off_runs{};
    f.buttons.mount(Content{[&] {
        Switch(SwitchProps{}.checked(checked).direction(direction), SwitchSlots{SwitchCheckedContent{[&] {
                                                                                    ++on_runs;
                                                                                    Text(u8"开");
                                                                                }},
                                                                                SwitchUncheckedContent{[&] {
                                                                                    ++off_runs;
                                                                                    Text(TextProps{}.content(caption));
                                                                                }}});
        Checkbox(CheckboxProps{});
        Radio(RadioProps{});
    }});
    f.synchronize();
    const auto sw = host.mounted().front();
    const auto texts = f.services.text().mounted_texts();
    require(texts.size() == 2 && on_runs == 1 && off_runs == 1, "Switch content was not retained once");
    const auto width = f.nodes.require(sw.node).bounds.width;
    require(width > 44 && f.services.interactions().size() == 3, "Switch content did not contribute stable width");
    const auto on = texts[0];
    const auto off = texts[1];
    require(!f.services.components().branch_active(on.component) &&
                f.services.components().branch_active(off.component),
            "Switch painted the wrong initial branch");
    const auto shapes = f.scene.text_state(off.scene).counters().shape_count;
    const auto mounts = f.services.components().mount_runs();
    const auto before = f.nodes.require(sw.node).measure_count;
    f.dirty.clear();
    checked.set(true);
    f.synchronize();
    require(near(f.nodes.require(sw.node).bounds.width, width) && f.nodes.require(sw.node).measure_count == before &&
                f.services.components().mount_runs() == mounts && on_runs == 1 && off_runs == 1 &&
                f.scene.text_state(off.scene).counters().shape_count == shapes &&
                f.services.components().branch_active(on.component) &&
                !f.services.components().branch_active(off.component),
            "Switch toggle changed width, measure, shaping or retained content");
    const auto range = f.services.surfaces().visual_range(sw.surface);
    const auto ltr = f.services.surfaces().instances().at(range.first + 1).bounds;
    f.dirty.clear();
    direction.set(SwitchDirection::RightToLeft);
    f.synchronize();
    const auto rtl = f.services.surfaces().instances().at(range.first + 1).bounds;
    require(rtl[0] < ltr[0] && near(rtl[0], 2) && f.nodes.require(sw.node).measure_count == before,
            "Switch RTL did not mirror handle without measure");
    const auto on_node = f.services.components().root(on.component);
    const auto clip = f.nodes.content_clip(on_node, {0, 0, 320, 240});
    require(clip.x >= 24 && clip.x + clip.width <= width - 9 + 0.02F, "Switch RTL content budget is wrong");
    f.nodes.require(sw.node).translation = {13, 7};
    f.synchronize();
    require(f.nodes.require(on_node).translation == runtime::Point{13, 7} &&
                near(f.nodes.content_clip(on_node, {0, 0, 320, 240}).x, clip.x + 13),
            "Switch translation did not follow content and clip");
    f.nodes.require(sw.node).translation = {};
    f.synchronize();
    caption.set(u8"很长的关闭内容 Long content changes width");
    f.synchronize();
    require(f.nodes.require(sw.node).bounds.width > width && on_runs == 1 && off_runs == 1,
            "Inactive Switch content update did not resize retained layout");
    f.synchronize(40, {0, 0, 40, 240});
    const auto primitive = f.scene.primitive(on.scene).instances;
    require(primitive.count > 0, "Switch visible text has no glyphs");
    const auto glyph_clip = f.scene.glyph_scene().instances().at(primitive.first).clip_bounds;
    require(glyph_clip[0] >= 24 && glyph_clip[2] <= 31.02F, "Switch glyph escaped narrow content clip");
    f.synchronize(10, {0, 0, 10, 240});
    require(!f.services.components().branch_active(on.component), "Zero content viewport retained visible branch");
    f.synchronize();
    require(f.services.components().branch_active(on.component), "Switch did not recover content from zero width");
}

void reference_and_callback_lifecycle() {
    SwitchRef reference;
    require(!reference.bound() && !reference.focus() && !reference.blur(), "Unbound SwitchRef was usable");
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    Signal<bool> loading{false};
    Signal<bool> disabled{false};
    std::vector<int> calls;
    f.buttons.mount(Content{[&] {
        Switch(SwitchProps{}
                   .checked(false)
                   .loading(loading)
                   .disabled(disabled)
                   .ref(reference)
                   .autoFocus(true)
                   .onChange([&](bool value) { calls.push_back(value ? 1 : -1); })
                   .onClick([&](bool value) { calls.push_back(value ? 2 : -2); }));
    }});
    const auto sw = host.mounted().front();
    require(reference.bound() && f.services.focus().state().focused == sw.interaction, "Switch autoFocus/ref failed");
    f.synchronize();
    activate(f, sw.interaction);
    require(calls == std::vector<int>{1, 2} && !host.snapshot(sw.component).checked,
            "Switch callbacks changed controlled value or callback order");
    loading.set(true);
    require(reference.focus() && f.services.focus().state().focused == sw.interaction,
            "Loading changed focus contract");
    activate(f, sw.interaction);
    require(calls.size() == 2, "Loading Switch called activation callbacks");
    loading.set(false);
    require(reference.blur() && !f.services.focus().state().focused && !reference.blur(), "SwitchRef blur failed");
    disabled.set(true);
    require(!reference.focus(), "Disabled SwitchRef focused");
    bool cross_thread_rejected{};
    std::thread thread{[&] {
        try {
            static_cast<void>(reference.bound());
        } catch (const std::logic_error&) {
            cross_thread_rejected = true;
        }
    }};
    thread.join();
    require(cross_thread_rejected, "SwitchRef allowed wrong thread");
    require(f.services.destroy(sw.component) && !reference.bound() && !reference.focus(),
            "Destroyed SwitchRef stayed bound");
    Fixture rebound;
    detail::SelectionComponentHost rebound_host{rebound.services};
    rebound.buttons.mount(Content{[&] {
        Switch(SwitchProps{}
                   .ref(reference)
                   .onChange([&](bool) {
                       calls.push_back(3);
                       require(rebound.services.destroy(rebound_host.mounted().front().component),
                               "Callback destroy failed");
                   })
                   .onClick([&](bool value) {
                       require(value && !reference.bound(), "Click after destroy retained invalid ref/value");
                       calls.push_back(4);
                   }));
    }});
    rebound.synchronize();
    activate(rebound, rebound_host.mounted().front().interaction);
    require(calls == std::vector<int>{1, 2, 3, 4} && rebound_host.mounted().empty() &&
                rebound.services.interactions().size() == 0 && rebound.services.surfaces().size() == 0,
            "Switch callback destruction leaked resources");
}

void invalid_content_and_reference() {
    for (const bool duplicate : {false, true}) {
        Fixture f;
        detail::SelectionComponentHost host{f.services};
        SwitchRef reference;
        bool rejected{};
        try {
            f.buttons.mount(Content{[&] {
                if (duplicate) {
                    Switch(SwitchProps{}.ref(reference));
                    Switch(SwitchProps{}.ref(reference));
                } else {
                    Switch(SwitchProps{}.ref(reference),
                           SwitchSlots{SwitchCheckedContent{
                                           [] { Button(ButtonProps{}, ButtonContent{[] { Text(u8"invalid"); }}); }},
                                       {}});
                }
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected && !reference.bound() && host.mounted().empty() &&
                    f.services.components().component_count() == 0 && f.services.interactions().size() == 0 &&
                    f.services.surfaces().size() == 0,
                "Switch invalid mount did not roll back");
    }
}
} // namespace

int main() {
    try {
        retained_content_and_direction();
        reference_and_callback_lifecycle();
        invalid_content_and_reference();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
