#include "gallery_existing_data_entry_acceptance.hpp"
#include "component/selection_component.hpp"
#include "component/slider_component.hpp"
#include "component/rate_component.hpp"
#include "component/otp_component.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace rynui::example {
void accept_existing_data_entry(const GalleryPageRoute& route, ExistingDataEntryActions a) {
    using namespace ryn;
    const auto owns = [&](runtime::ComponentId child, runtime::ComponentId owner) {
        for (auto current = std::optional{child}; current; current = a.app.components().parent(*current)) {
            if (*current == owner) {
                return true;
            }
        }
        return false;
    };
    const auto point = [&](runtime::NodeId node, float fraction = .5F) {
        const auto& n = a.app.nodes().require(node);
        return runtime::Point{n.bounds.x + n.translation.x + n.bounds.width * fraction,
                              n.bounds.y + n.translation.y + n.bounds.height / 2};
    };
    const auto arm_retirement = [&](runtime::Point point) {
        a.pointer(input::PointerAction::down, point);
        a.render();
        const auto state = a.app.pointer().state(input::PointerIdentity::mouse());
        a.check(state && state->capture.has_value(), "native owned DataEntry retirement capture was not armed");
    };
    const auto focus = [&](input::InteractionId interaction) {
        a.check(a.app.focus().state().focused == interaction ||
                    a.app.focus().request_focus(interaction, input::FocusModality::keyboard),
                "owned DataEntry native focus failed");
    };
    const auto has_text = [&](std::string_view value) {
        return std::ranges::any_of(a.app.services().text().mounted_texts(), [&](const auto& text) {
            return a.app.services().text().scene_service().text_state(text.scene).content().bytes().find(value) !=
                   std::string_view::npos;
        });
    };
    auto& selection = a.selection;
    const auto members = [&](runtime::ComponentId owner) {
        std::vector<ryn::detail::MountedSelectionComponent> result;
        for (const auto row : selection.mounted()) {
            if (owns(row.component, owner)) {
                result.push_back(row);
            }
        }
        return result;
    };
    if (route.component_identity == "ant.component.checkbox") {
        const auto group = a.pages.target("gallery.checkbox.controlled");
        a.reveal(group);
        const auto rows = members(group);
        a.check(rows.size() == 3 && selection.snapshot(rows[2].component).disabled,
                "native typed Checkbox group missing");
        a.click(rows[1].component);
        a.check(selection.checkbox_group_value(group) == CheckboxValues{String{u8"desktop"}, 1.0},
                "native Checkbox controlled callback did not accept typed number");
        focus(rows[1].interaction);
        a.press(input::Key::space);
        a.check(selection.checkbox_group_value(group) == CheckboxValues{String{u8"desktop"}},
                "native Checkbox Space did not update authority");
        a.click(rows[2].component);
        a.check(selection.checkbox_group_value(group).size() == 1, "disabled Checkbox emitted candidate");
        const auto dynamic = a.pages.target("gallery.checkbox.dynamic");
        const auto before = members(dynamic);
        a.reveal(a.pages.target("gallery.checkbox.options-update"));
        a.activate(a.pages.target("gallery.checkbox.options-update"));
        a.render();
        const auto after = members(dynamic);
        a.check(before.size() == 2 && after.size() == 3 &&
                    std::ranges::any_of(after, [&](auto row) { return row.component == before[0].component; }) &&
                    std::ranges::any_of(after, [&](auto row) { return row.component == before[1].component; }),
                "native Checkbox option update remounted retained members");
        const auto rich = a.pages.target("gallery.checkbox.rich-rtl-ref");
        a.reveal(rich);
        a.activate(a.pages.target("gallery.checkbox.focus"));
        a.render();
        a.check(has_text("Checkbox focus"), "native Checkbox focus callback missing");
        arm_retirement(point(a.app.components().root(rich)));
        std::cout << "checkbox_native=passed typed_callback=true disabled=true dynamic_identity=true focus_event=true "
                     "capture_retirement=armed\n";
    }
    if (route.component_identity == "ant.component.radio") {
        const auto controlled = a.pages.target("gallery.radio.controlled");
        a.reveal(controlled);
        const auto controlled_rows = members(controlled);
        a.check(controlled_rows.size() == 3, "native controlled Radio sample missing");
        focus(controlled_rows[0].interaction);
        a.press(input::Key::right);
        a.check(selection.radio_group_value(controlled) == RadioSelection{String{u8"b"}},
                "native Radio controlled keyboard callback did not update authority");
        a.click(controlled_rows[2].component);
        a.check(selection.radio_group_value(controlled) == RadioSelection{String{u8"b"}},
                "native controlled Radio disabled item changed authority");
        const auto group = a.pages.target("gallery.radio.dynamic-outline");
        a.reveal(group);
        const auto rows = members(group);
        a.check(rows.size() == 2, "native Radio dynamic group missing");
        focus(rows[0].interaction);
        a.press(input::Key::right);
        a.check(selection.radio_group_value(group) == RadioSelection{String{u8"b"}} &&
                    a.app.focus().state().focused == rows[1].interaction,
                "native Radio roving keyboard callback failed");
        a.reveal(a.pages.target("gallery.radio.options-update"));
        a.activate(a.pages.target("gallery.radio.options-update"));
        a.render();
        a.check(members(group).size() == 3 && a.app.components().contains(rows[0].component) &&
                    a.app.components().contains(rows[1].component),
                "native Radio dynamic identity failed");
        const auto solid = a.pages.target("gallery.radio.solid-large-block");
        a.reveal(solid);
        const auto solid_rows = members(solid);
        a.click(solid_rows[0].component);
        a.check(selection.radio_group_value(solid) == RadioSelection{String{u8"desktop"}},
                "native Radio solid typed value callback failed");
        a.click(solid_rows[2].component);
        a.check(selection.radio_group_value(solid) == RadioSelection{String{u8"desktop"}},
                "native disabled Radio changed selection");
        const auto rich = a.pages.target("gallery.radio.ref");
        a.reveal(rich);
        a.activate(a.pages.target("gallery.radio.focus"));
        a.render();
        a.check(has_text("Radio focus"), "native Radio ref/focus event failed");
        arm_retirement(point(a.app.components().root(rich)));
        std::cout << "radio_native=passed roving_callback=true dynamic_identity=true solid_disabled=true "
                     "ref_event=true capture_retirement=armed\n";
    }
    if (route.component_identity == "ant.component.switch") {
        const auto owner = a.pages.target("gallery.switch.controlled");
        a.reveal(owner);
        const auto row = members(owner).front();
        const bool initial = selection.snapshot(row.component).checked;
        a.click(row.component);
        a.check(selection.snapshot(row.component).checked != initial,
                "native Switch controlled pointer callback failed");
        focus(row.interaction);
        a.press(input::Key::space);
        a.check(selection.snapshot(row.component).checked == initial && has_text("Switch focus"),
                "native Switch keyboard/focus callback failed");
        for (const auto name : {"gallery.switch.basic-disabled", "gallery.switch.basic-loading"}) {
            const auto gate = a.pages.target(name);
            a.reveal(gate);
            const bool before = selection.snapshot(gate).checked;
            a.click(gate);
            a.check(selection.snapshot(gate).checked == before, "native Switch input gate failed");
        }
        a.reveal(owner);
        arm_retirement(point(row.node));
        std::cout << "switch_native=passed pointer_keyboard_callbacks=true focus_event=true disabled_loading=true "
                     "capture_retirement=armed\n";
    }
    if (route.component_identity == "ant.component.slider") {
        auto& sliders = a.app.services().slider();
        const auto mounted = [&](runtime::ComponentId owner) {
            const auto found =
                std::ranges::find(sliders.mounted(), owner, &ryn::detail::MountedSliderComponent::component);
            a.check(found != sliders.mounted().end(), "native Slider target missing");
            return *found;
        };
        const auto controlled = a.pages.target("gallery.slider.controlled");
        a.reveal(controlled);
        const auto single = mounted(controlled);
        const auto accepted = sliders.snapshot(controlled).values.front();
        focus(single.thumbs.front());
        a.press(input::Key::right);
        a.check(sliders.snapshot(controlled).values.front() > accepted && has_text("Slider focus"),
                "native Slider controlled callback/focus event failed");
        const auto range = a.pages.target("gallery.slider.range-track");
        a.reveal(range);
        const auto initial = sliders.snapshot(range);
        const auto translation = a.app.nodes().require(a.app.components().root(range)).translation;
        const runtime::Point middle{(initial.centers[0].x + initial.centers[1].x) / 2 + translation.x,
                                    initial.centers[0].y + translation.y};
        a.pointer(input::PointerAction::down, middle);
        a.render();
        const auto capture = a.app.pointer().state(input::PointerIdentity::mouse());
        a.check(capture && capture->capture.has_value() && sliders.snapshot(range).dragging,
                "native range track did not capture at its translated window coordinates");
        a.pointer(input::PointerAction::move, {middle.x + 20, middle.y});
        a.pointer(input::PointerAction::up, {middle.x + 20, middle.y});
        a.render();
        const auto moved = sliders.snapshot(range);
        a.check(moved.values[0] > initial.values[0] &&
                    std::abs((moved.values[1] - moved.values[0]) - (initial.values[1] - initial.values[0])) < .001,
                "native draggable range callback failed");
        const auto multi = a.pages.target("gallery.slider.multi-edit");
        a.reveal(multi);
        const auto edit = mounted(multi);
        focus(edit.thumbs[1]);
        a.press(input::Key::delete_forward);
        a.check(sliders.snapshot(multi).values.size() == 2, "native editable MultiSlider keyboard removal failed");
        const auto disabled = a.pages.target("gallery.slider.multi-disabled");
        a.reveal(disabled);
        const auto gated = mounted(disabled);
        a.check(!a.app.focus().request_focus(gated.thumbs[1], input::FocusModality::keyboard),
                "native per-handle disabled gate admitted focus");
        focus(gated.thumbs[0]);
        const auto before = sliders.snapshot(disabled).values;
        a.press(input::Key::right);
        a.check(sliders.snapshot(disabled).values[0] > before[0] && sliders.snapshot(disabled).values[1] == before[1],
                "native MultiSlider keyboard callback changed disabled handle");
        const auto handle = sliders.snapshot(disabled).centers[0];
        const auto handle_translation = a.app.nodes().require(a.app.components().root(disabled)).translation;
        arm_retirement({handle.x + handle_translation.x, handle.y + handle_translation.y});
        std::cout << "slider_native=passed draggable_range_callback=true editable_multi=true handle_gate=true "
                     "capture_retirement=armed\n";
    }
    if (route.component_identity == "ant.component.rate") {
        auto& rate = a.app.services().rate();
        const auto owner = a.pages.target("gallery.rate.control");
        a.reveal(owner);
        const auto initial = rate.snapshot(owner);
        a.pointer(input::PointerAction::down, point(initial.items[0].node, .25F));
        a.pointer(input::PointerAction::up, point(initial.items[0].node, .25F));
        a.render();
        a.check(rate.snapshot(owner).value == .5F && has_text("已接受候选="), "native Rate half-star callback failed");
        a.activate(a.pages.target("gallery.rate.authority"));
        a.render();
        a.pointer(input::PointerAction::down, point(initial.items[1].node, .75F));
        a.pointer(input::PointerAction::up, point(initial.items[1].node, .75F));
        a.render();
        a.check(rate.snapshot(owner).value == .5F && has_text("已拒绝候选="), "native Rate controlled refusal failed");
        a.activate(a.pages.target("gallery.rate.authority"));
        a.activate(a.pages.target("gallery.rate.direction"));
        a.activate(a.pages.target("gallery.rate.character"));
        a.activate(a.pages.target("gallery.rate.focus"));
        a.render();
        a.check(rate.snapshot(owner).items[0].component == initial.items[0].component && has_text("Rate focus"),
                "native Rate character/direction/ref remounted or lost focus event");
        a.press(input::Key::left);
        a.check(rate.snapshot(owner).value == 1 && has_text("Rate keydown"), "native Rate RTL key event failed");
        for (const auto name : {"gallery.rate.disabled", "gallery.rate.readonly"}) {
            a.activate(a.pages.target(name));
            a.render();
            a.pointer(input::PointerAction::down, point(initial.items[0].node));
            a.pointer(input::PointerAction::up, point(initial.items[0].node));
            a.render();
            a.check(rate.snapshot(owner).value == 1, "native Rate gate changed value");
            a.activate(a.pages.target(name));
            a.render();
        }
        a.reveal(owner);
        a.pointer(input::PointerAction::move, point(initial.items[4].node));
        a.render();
        const auto note = a.pages.target("gallery.rate.note");
        const auto rich =
            std::ranges::find(a.inputs.mounted_inputs(), note, &ryn::detail::MountedInputComponent::component);
        a.check(rich != a.inputs.mounted_inputs().end(), "native Rate retained rich Tooltip editor missing");
        const auto saved = *rich;
        focus(saved.interaction);
        a.pages.dispatch(input::TextCommitted{String{u8" native retained draft"}, a.inputs.sessions().active()});
        a.render();
        const auto draft = std::string{a.inputs.editors().require(saved.editor).value()};
        a.activate(a.pages.target("gallery.rate.visible"));
        a.render();
        a.check(!a.inputs.sessions().active().valid(), "hidden Rate rich Tooltip retained editor session");
        a.activate(a.pages.target("gallery.rate.restore"));
        a.render();
        a.pointer(input::PointerAction::move, point(initial.items[4].node));
        a.render();
        a.check(a.app.components().contains(saved.component) &&
                    a.inputs.editors().require(saved.editor).value() == draft,
                "native Rate rich Tooltip hide/restore lost draft identity");
        arm_retirement(point(initial.items[1].node));
        std::cout << "rate_native=passed half_callback=true controlled_refusal=true rtl=true reactive_character=true "
                     "ref_key_events=true gates=true capture_retirement=armed\n";
    }
    if (route.component_identity == "ant.component.input") {
        // The common page harness starts a synthetic composition. End that transaction
        // before testing committed text; this does not exercise an OS candidate window.
        if (a.inputs.sessions().active().valid()) {
            a.pages.dispatch(input::CompositionChanged{String{}, {}, a.inputs.sessions().active()});
            a.render();
        }
        const auto owner = a.pages.target(route.family == GalleryInputFamily::search      ? "gallery.search.controlled"
                                          : route.family == GalleryInputFamily::password  ? "gallery.password.basic"
                                          : route.family == GalleryInputFamily::text_area ? "gallery.text-area.autosize"
                                          : route.family == GalleryInputFamily::otp       ? "gallery.otp.formatter-mask"
                                                                                          : "gallery.input.controlled");
        a.reveal(owner);
        std::optional<ryn::detail::MountedInputComponent> editor;
        for (const auto input : a.inputs.mounted_inputs()) {
            if (owns(input.component, owner) && a.app.interactions().branch_active(input.interaction)) {
                editor = input;
                break;
            }
        }
        a.check(editor.has_value(), "native canonical Input family editor missing");
        focus(editor->interaction);
        a.pages.dispatch(input::KeyboardInputEvent{input::Key::a, input::KeyAction::down, input::KeyModifier::control});
        a.pages.dispatch(input::KeyboardInputEvent{input::Key::a, input::KeyAction::up, input::KeyModifier::control});
        const String text = route.family == GalleryInputFamily::text_area ? String{u8"中文 é\nאבג\nthird\nfourth"}
                            : route.family == GalleryInputFamily::otp     ? String{u8"ab12cd"}
                                                                          : String{u8"中文 é אבג"};
        a.pages.dispatch(input::TextCommitted{text, a.inputs.sessions().active()});
        a.render();
        if (route.family == GalleryInputFamily::otp) {
            const auto cells = a.app.services().otp().cells(owner);
            a.check(cells.size() == 6 && cells.front() == String{u8"A"} && cells.back() == String{u8"D"},
                    "native OTP formatter/paste distribution failed");
            a.check(a.inputs.display_snapshot(editor->component).text.find('A') == std::string::npos,
                    "native OTP mask exposed formatted source");
        } else {
            a.check(a.inputs.editors().require(editor->editor).value() == text.bytes(),
                    "native Input family committed Unicode changed");
            if (route.family == GalleryInputFamily::password) {
                a.check(a.inputs.display_snapshot(editor->component).text != text.bytes(),
                        "native Password exposed source");
            }
            if (route.family == GalleryInputFamily::search || route.family == GalleryInputFamily::input) {
                a.press(input::Key::enter);
                a.check(has_text(text.bytes()), "native Input/Search submit callback missing");
            }
            if (route.family == GalleryInputFamily::text_area) {
                a.check(a.app.nodes().require(editor->node).bounds.height > 50,
                        "native TextArea autoSize did not grow");
            }
        }
        // Leave the live editor session/preedit (and ordinary text drag capture) for the next route to retire.
        focus(editor->interaction);
        a.render();
        const auto area = a.inputs.layout_snapshot(editor->component);
        const auto target = point(editor->node);
        const auto hit = a.app.services().hit_test().hit_test(target);
        std::cout << "input_retirement_geometry family=" << static_cast<int>(route.family) << " target=" << target.x
                  << ',' << target.y << " viewport=" << area.viewport.x << ',' << area.viewport.y << ','
                  << area.viewport.width << ',' << area.viewport.height << " clip=" << area.clip.x << ',' << area.clip.y
                  << ',' << area.clip.width << ',' << area.clip.height << " editor_hit=" << (hit == editor->interaction)
                  << '\n';
        a.check(hit == editor->interaction, "native Input retirement point did not hit its actual editor bounds");
        if (route.family == GalleryInputFamily::otp) {
            // OTP deliberately selects a whole grapheme cell instead of starting text drag capture.
            a.pointer(input::PointerAction::down, target);
            a.pointer(input::PointerAction::up, target);
            a.render();
            const auto pointer = a.app.pointer().state(input::PointerIdentity::mouse());
            a.check((!pointer || !pointer->capture) && a.app.focus().state().focused == editor->interaction &&
                        a.inputs.editors().require(editor->editor).selection() == input::TextSelection{0, 1},
                    "native OTP click did not select the whole cell without drag capture");
            a.press(input::Key::right);
            a.check(a.app.focus().state().focused != editor->interaction, "native OTP keyboard did not advance cell");
            a.press(input::Key::left);
            a.check(a.app.focus().state().focused == editor->interaction, "native OTP keyboard did not return to cell");
        } else {
            arm_retirement(target);
        }
        a.pages.dispatch(input::CompositionChanged{String{u8"组合"}, {}, a.inputs.sessions().active()});
        a.render();
        std::cout
            << "input_family_native=passed family=" << static_cast<int>(route.family)
            << " committed_unicode=true feature_callback=true "
            << (route.family == GalleryInputFamily::otp
                    ? "whole_cell_click=true keyboard_navigation=true preedit_retirement=armed drag_capture=false\n"
                    : "capture_preedit_retirement=armed\n");
    }
}
} // namespace rynui::example
