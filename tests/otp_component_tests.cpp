#include "support/input_fixture.hpp"
#include "component/otp_component.hpp"
#include "theme/input_tokens.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <class F> void rejects(F&& operation) {
    bool threw = false;
    try {
        operation();
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "invalid configuration accepted");
}

void key(Fixture& f, input::Key key, input::KeyModifier modifiers = input::KeyModifier::none) {
    f.services.focus().dispatch({key, input::KeyAction::down, modifiers});
    f.services.focus().dispatch({key, input::KeyAction::up, modifiers});
}

detail::MountedInputComponent cell(Fixture& f, runtime::ComponentId id) {
    for (const auto& mounted : f.inputs.mounted_inputs()) {
        if (mounted.component == id) {
            return mounted;
        }
    }
    throw std::runtime_error("missing retained cell");
}

void input(Fixture& f, const String& text) {
    check(bool(f.inputs.dispatch(input::TextCommitted{text, f.inputs.sessions().active()})), "OTP edit rejected");
}

void retained_api_and_mask() {
    Fixture f;
    Signal<std::size_t> length{4};
    Signal<String> value{String{u8"ab中dEF"}};
    Signal<OTPMask> mask{OTPMask{}};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<InputVariant> variant{InputVariant::Outlined};
    Signal<InputStatus> status{InputStatus::Default};
    Signal<OTPDirection> direction{OTPDirection::LeftToRight};
    OTPRef ref;
    int content_runs{};
    int separator_runs{};
    int notifications{};
    f.inputs.mount(Content{[&] {
        ++content_runs;
        OTP(OTPProps{}
                .value(value)
                .length(length)
                .mask(mask)
                .size(size)
                .variant(variant)
                .status(status)
                .direction(direction)
                .ref(ref)
                .onInput([&](const auto&) { ++notifications; }),
            OTPSeparator{[&](std::size_t index) -> std::optional<OTPSeparatorContent> {
                if (index % 2 == 0) {
                    return {};
                }
                return OTPSeparatorContent{[&] {
                    ++separator_runs;
                    Text(TextProps{}.content(u8"-"));
                }};
            }});
    }});
    f.synchronize(640, {0, 0, 640, 240});
    const auto group = f.services.otp().mounted().front().component;
    const auto ids = f.services.otp().cell_components(group);
    std::vector<input::TextInputOwnerId> owners;
    std::vector<detail::TextSceneId> scenes;
    for (const auto id : ids) {
        owners.push_back(cell(f, id).editor);
        scenes.push_back(f.inputs.text_scene(id));
    }
    check(ref.bound() && separator_runs == 1 && f.inputs.editors().size() == 4, "OTP initial inventory");
    const auto first = cell(f, ids[0]);
    const auto first_width = f.nodes.require(first.node).bounds.width;
    check(first_width > 20 && first_width < 40, "OTP cell did not use token width");
    mask.set({true, String{u8"*"}});
    check(ref.focus(), "OTPRef focus failed");
    f.synchronize(640, {0, 0, 640, 240});
    check(f.inputs.display_snapshot(ids[2]).text == "*" &&
              f.platform.last_properties.type == input::TextInputType::password_hidden,
          "OTP mask leaked raw text or native purpose");
    key(f, input::Key::c, input::KeyModifier::control);
    check(f.platform.writes == 0, "masked OTP copied raw value");
    const auto caret = f.inputs.layout_snapshot(ids[0]);
    check(caret.scroll_offset < 0, "OTP glyph did not center");
    mask.set({true, String{u8"界"}});
    f.synchronize(640, {0, 0, 640, 240});
    check(f.inputs.display_snapshot(ids[0]).text == String{u8"界"}.bytes(), "custom multibyte mask missing");
    check(f.inputs.editors().require(owners[2]).value() == String{u8"中"}.bytes(), "mask changed committed text");
    rejects([&] { mask.set({true, String{u8"xy"}}); });
    mask.set({});
    size.set(ControlSize::Small);
    variant.set(InputVariant::Filled);
    status.set(InputStatus::Error);
    direction.set(OTPDirection::RightToLeft);
    f.synchronize(640, {0, 0, 640, 240});
    check(f.inputs.display_snapshot(ids[2]).text == String{u8"中"}.bytes() &&
              f.inputs.size(ids[0]) == ControlSize::Small && f.inputs.status(ids[0]) == InputStatus::Error &&
              f.inputs.variant(ids[0]) == InputVariant::Filled,
          "OTP reactive props");
    check(f.nodes.require(cell(f, ids[0]).node).bounds.x > f.nodes.require(cell(f, ids[3]).node).bounds.x,
          "OTP RTL layout");
    length.set(6);
    f.synchronize(640, {0, 0, 640, 240});
    check(f.services.otp().cells(group)[4] == String{u8"E"} && separator_runs == 2,
          "OTP growth lost hidden external suffix");
    length.set(3);
    f.synchronize(640, {0, 0, 640, 240});
    check(f.inputs.editors().size() == 3 && f.services.otp().cell_components(group) ==
                                                std::vector<runtime::ComponentId>(ids.begin(), ids.begin() + 3),
          "OTP length rebuilt prefix");
    rejects([&] { length.set(0); });
    length.set(4);
    f.synchronize(640, {0, 0, 640, 240});
    for (std::size_t i = 0; i < 3; ++i) {
        check(cell(f, ids[i]).editor == owners[i] && f.inputs.text_scene(ids[i]) == scenes[i],
              "OTP ordinary props replaced retained resources");
    }
    check(content_runs == 1 && notifications == 0, "configuration ran content/user callbacks");
    f.services.dispose();
    check(!ref.bound() && f.inputs.editors().size() == 0 && f.scene.size() == 0 && f.services.otp().mounted().empty(),
          "OTP dispose leaked resources");
}

void transactions() {
    Fixture f;
    Signal<String> value{String{u8"abcd"}};
    OTPRef ref;
    int partial{};
    int complete{};
    std::vector<int> order;
    f.inputs.mount(Content{[&] {
        OTP(OTPProps{}
                .length(4)
                .value(value)
                .ref(ref)
                .onInput([&](const auto& cells) {
                    check(cells.size() == 4, "partial array omitted empty cells");
                    ++partial;
                    order.push_back(1);
                    std::string full;
                    for (const auto& cell : cells) {
                        full.append(cell.bytes());
                    }
                    value.set(String::from_utf8(full).value());
                })
                .onChange([&](String text) {
                    ++complete;
                    order.push_back(2);
                    value.set(std::move(text));
                }));
    }});
    f.synchronize();
    check(ref.focus(), "focus complete group");
    input(f, String{u8"az"});
    const auto group = f.services.otp().mounted().front().component;
    check(f.services.otp().cells(group)[0] == String{u8"a"} && f.services.otp().cells(group)[1] == String{u8"z"} &&
              f.services.otp().cells(group)[2].empty() && partial == 1 && complete == 0,
          "same local text lost changed tail");
    input(f, String{u8"12"});
    check(value.get() == String{u8"az12"} && complete == 1 && order == std::vector<int>{1, 1, 2},
          "controlled completion ordering");
    key(f, input::Key::left);
    input(f, String{u8"1"});
    check(complete == 1, "equal candidate notified completion");
}

void invalid_mounts() {
    for (int mode = 0; mode < 4; ++mode) {
        Fixture f;
        OTPRef ref;
        rejects([&] {
            f.inputs.mount(Content{[&] {
                if (mode == 0) {
                    OTP(OTPProps{}.length(0).ref(ref));
                }
                if (mode == 1) {
                    OTP(OTPProps{}.mask(u8"ab").ref(ref));
                }
                if (mode == 2) {
                    OTP(OTPProps{}.value(u8"a").defaultValue(u8"b").ref(ref));
                }
                if (mode == 3) {
                    OTP(OTPProps{}.ref(ref), OTPSeparator{[](std::size_t) -> std::optional<OTPSeparatorContent> {
                            return OTPSeparatorContent{[] {
                                Button(ButtonProps{}, ButtonContent{[] { Text(TextProps{}.content(u8"bad")); }});
                            }};
                        }});
                }
            }});
        });
        check(!ref.bound() && f.inputs.editors().size() == 0 && f.scene.size() == 0 &&
                  f.services.components().component_count() == 0,
              "failed OTP mount leaked resources");
    }
}

void visual_matrix_and_separator_rollback() {
    for (const auto algorithm : {ThemeAlgorithm::Default, ThemeAlgorithm::Dark}) {
        Fixture f;
        Signal<ControlSize> size{ControlSize::Middle};
        Signal<InputVariant> variant{InputVariant::Outlined};
        ThemeConfig config;
        config.algorithms = {algorithm, ThemeAlgorithm::Compact};
        f.inputs.mount(Content{[&] {
            Theme(ThemeProps{}.config(config),
                  ThemeContent{[&] { OTP(OTPProps{}.length(3).size(size).variant(variant).defaultValue(u8"a中c")); }});
        }});
        const auto group = f.services.otp().mounted().front().component;
        const auto ids = f.services.otp().cell_components(group);
        for (const auto control : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
            size.set(control);
            for (const auto visual :
                 {InputVariant::Outlined, InputVariant::Filled, InputVariant::Borderless, InputVariant::Underlined}) {
                variant.set(visual);
                f.synchronize();
                const auto tokens = detail::derive_input_tokens(resolve_theme(config));
                for (const auto id : ids) {
                    check(f.inputs.size(id) == control && f.inputs.variant(id) == visual &&
                              std::abs(f.nodes.require(cell(f, id).node).bounds.height -
                                       tokens.size(control).control_height) < .01F,
                          "OTP visual matrix differed from Input tokens");
                }
            }
        }
    }
    Fixture f;
    Signal<std::size_t> length{2};
    bool fail = false;
    f.inputs.mount(Content{[&] {
        OTP(OTPProps{}.length(length).defaultValue(u8"abcd"), OTPSeparator{[&](std::size_t) {
                return std::optional<OTPSeparatorContent>{OTPSeparatorContent{[&] {
                    Text(u8"-");
                    if (fail) {
                        throw std::runtime_error("separator failure");
                    }
                }}};
            }});
    }});
    f.synchronize();
    const auto group = f.services.otp().mounted().front().component;
    const auto ids = f.services.otp().cell_components(group);
    const auto components = f.services.components().component_count();
    const auto scenes = f.scene.size();
    fail = true;
    rejects([&] { length.set(4); });
    check(f.services.otp().cell_components(group) == ids && f.inputs.editors().size() == 2 &&
              f.services.components().component_count() == components && f.scene.size() == scenes,
          "throwing separator changed retained prefix or leaked resources");
    fail = false;
    length.set(3);
    f.synchronize();
    check(f.inputs.editors().size() == 3 && f.services.otp().cells(group)[2] == String{u8"c"},
          "separator recovery failed");
}
} // namespace

int main() {
    try {
        retained_api_and_mask();
        transactions();
        invalid_mounts();
        visual_matrix_and_separator_rollback();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
