#include "component/feedback_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>
#include <limits>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    for (auto a : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), a, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    }
    f.synchronize();
}

void alert_controlled() {
    Fixture f;
    Signal<bool> visible{true};
    Signal<bool> closable{true};
    Signal<Status> status{Status::Warning};
    Signal<String> message{String{u8"Device warning"}};
    Signal<String> description{String{u8"Read details / 详情"}};
    int actions = 0;
    int action_mounts = 0;
    int closes = 0;
    f.services.mount(Content{[&] {
        Alert(AlertProps{}
                  .status(status)
                  .message(message)
                  .description(description)
                  .showIcon()
                  .visible(visible)
                  .closable(closable)
                  .onClose([&] { ++closes; }),
              AlertSlots{.action = AlertAction{[&] {
                             ++action_mounts;
                             Button(ButtonProps{}.onClick([&] { ++actions; }), ButtonContent{[] { Text(u8"Retry"); }});
                         }}});
    }});
    f.synchronize();
    const auto id = f.services.feedback().alerts()[0];
    const auto initial = f.services.feedback().alert_snapshot(id);
    check(initial.has_description && bounds(f, initial.description).y > bounds(f, initial.message).y,
          "Alert description not below message");
    click(f, initial.close);
    check(closes == 1 && f.services.feedback().alert_snapshot(id).visible, "Alert close not controlled");
    click(f, f.buttons.mounted_buttons()[0].component);
    check(actions == 1, "Alert action inaccessible");
    check(f.services.focus().request_focus(initial.close_interaction, FocusModality::keyboard),
          "Alert close focus absent");
    for (auto a : {KeyAction::down, KeyAction::up}) {
        f.services.focus().dispatch({Key::space, a});
    }
    check(closes == 2, "Alert close Space absent");
    status.set(Status::Success);
    message.set(String{u8"Ready"});
    description.set(String{});
    f.synchronize();
    check(!f.services.feedback().alert_snapshot(id).has_description && action_mounts == 1 &&
              f.services.feedback().alert_snapshot(id).close == initial.close,
          "Alert reactive update remounted slots");
    visible.set(false);
    f.synchronize();
    check(!f.services.focus().state().focused && !f.services.components().branch_active(id),
          "hidden Alert retained focus");
    visible.set(true);
    closable.set(false);
    f.synchronize();
    click(f, initial.close);
    check(closes == 2, "nonclosable Alert activated");
    check(f.services.destroy(id) && f.services.interactions().size() == 0, "Alert teardown leaked interactions");
}

void spin_retained_and_motion() {
    Fixture f;
    Signal<bool> loading{false};
    int mounts = 0;
    int clicks = 0;
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(loading).description(u8"Loading"), SpinContent{[&] {
                 ++mounts;
                 Button(ButtonProps{}.layout(LayoutStyle{}.width(dp(240)).height(dp(100))).onClick([&] { ++clicks; }),
                        ButtonContent{[] { Text(u8"Content"); }});
             }});
    }});
    f.synchronize();
    const auto id = f.services.feedback().spins()[0];
    const auto initial = f.services.feedback().spin_snapshot(id);
    const auto button = f.buttons.mounted_buttons()[0];
    click(f, button.component);
    check(clicks == 1, "idle Spin blocked content");
    check(f.services.focus().request_focus(button.interaction, FocusModality::keyboard), "Spin content focus absent");
    loading.set(true);
    f.synchronize();
    check(!f.services.focus().state().focused && !f.services.interactions().component_active(button.component),
          "Spin retained child focus/input");
    click(f, button.component);
    check(clicks == 1 && mounts == 1, "Spin shield failed or reran contents");
    check(!f.services.next_frame_deadline(), "reduced Spin retained deadline");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    f.synchronize();
    if (!f.services.next_frame_deadline()) {
        const auto a = f.services.feedback().spin_snapshot(id);
        const auto r = bounds(f, a.indicator);
        const auto root = bounds(f, id);
        std::cerr << "Spin bounds " << root.width << ',' << root.height << " indicator " << r.x << ',' << r.y << ','
                  << r.width << ',' << r.height << " visible " << a.in_view << '\n';
    }
    check(f.services.next_frame_deadline().has_value(), "visible Spin did not animate");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(250000)));
    f.synchronize();
    f.services.set_window_active(false);
    check(!f.services.next_frame_deadline(), "inactive Spin retained deadline");
    f.services.set_window_active(true);
    f.synchronize();
    check(f.services.next_frame_deadline().has_value(), "Spin did not resume");
    f.synchronize(320, {0, 180, 320, 40});
    check(!f.services.next_frame_deadline(), "offscreen Spin retained deadline");
    f.synchronize();
    check(f.services.next_frame_deadline().has_value(), "onscreen Spin did not resume");
    loading.set(false);
    f.synchronize();
    check(!f.services.next_frame_deadline() && f.services.interactions().component_active(button.component) &&
              mounts == 1,
          "Spin recovery lost state");
    click(f, button.component);
    check(clicks == 2, "Spin did not restore interaction");
    loading.set(true);
    f.synchronize();
    check(f.services.destroy(id) && !f.services.next_frame_deadline() &&
              f.services.animations().diagnostics().targets == 0,
          "Spin destroy retained animation");
    static_cast<void>(initial);
}

void progress_geometry_and_validation() {
    Fixture f;
    Signal<float> value{25};
    Signal<ProgressStatus> status{ProgressStatus::Normal};
    Signal<ProgressType> type{ProgressType::Line};
    f.services.mount(Content{[&] { Progress(ProgressProps{}.percent(value).status(status).type(type)); }});
    f.synchronize();
    const auto id = f.services.feedback().progress()[0];
    const auto count = f.services.components().component_count();
    check(f.services.feedback().progress_snapshot(id).percent == 25, "Progress percent missing");
    const auto painted = [&](runtime::Point point, Color color) {
        return std::ranges::any_of(f.services.rounded_effects().packed_instances(), [&](const auto& effect) {
            return effect.material.opacity > 0 && effect.material.color == color &&
                   graphics::rounded_effect_coverage(point, effect) > .5F;
        });
    };
    const auto line = f.services.feedback().progress_snapshot(id);
    const auto line_y = line.track.y + line.track.height / 2;
    check(painted({line.track.x + line.track.width * .1F, line_y}, line.color) &&
              !painted({line.track.x + line.track.width * .75F, line_y}, line.color),
          "line physical fill does not reflect 25 percent");
    const auto effects = f.services.rounded_effects().live_count();
    type.set(ProgressType::Circle);
    f.synchronize();
    check(std::abs(f.services.feedback().progress_snapshot(id).sweep_degrees - 90) < .01F,
          "circle quarter arc incorrect");
    auto circle = f.services.feedback().progress_snapshot(id);
    check(std::ranges::count_if(f.services.rounded_effects().packed_instances(),
                                [&](const auto& effect) {
                                    return effect.material.opacity > 0 &&
                                           effect.material.color == resolve_theme({}).progress().colors.remaining;
                                }) == 1,
          "circle rail accumulated translucent samples");
    auto radius = circle.track.width * .47F;
    auto cx = circle.track.x + circle.track.width / 2;
    auto cy = circle.track.y + circle.track.height / 2;
    check(painted({cx, cy - radius}, circle.color) && painted({cx + radius, cy}, circle.color) &&
              !painted({cx, cy + radius}, circle.color),
          "circle physical quadrant fill incorrect");
    type.set(ProgressType::Dashboard);
    value.set(100);
    f.synchronize();
    check(f.services.feedback().progress_snapshot(id).sweep_degrees == 285 &&
              f.services.feedback().progress_snapshot(id).status == ProgressStatus::Success,
          "dashboard completion incorrect");
    circle = f.services.feedback().progress_snapshot(id);
    check(!painted({cx, cy + radius}, circle.color) && painted({cx, cy - radius}, circle.color) &&
              f.services.rounded_effects().live_count() == effects,
          "dashboard gap or retained topology incorrect");
    status.set(ProgressStatus::Exception);
    f.synchronize();
    check(f.services.feedback().progress_snapshot(id).status == ProgressStatus::Exception,
          "Exception lost to 100 percent");
    value.set(-50);
    f.synchronize();
    check(f.services.feedback().progress_snapshot(id).percent == 0, "Progress low clamp missing");
    value.set(500);
    f.synchronize();
    check(f.services.feedback().progress_snapshot(id).percent == 100 &&
              f.services.components().component_count() == count,
          "Progress clamp/remount incorrect");
    bool rejected = false;
    try {
        value.set(std::numeric_limits<float>::infinity());
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "Progress accepted nonfinite percent");
    f.services.destroy(id);
    check(f.scene.size() == 0 && !f.services.next_frame_deadline(), "Progress teardown retained scene");
}

void spin_input_lifetime_and_nesting() {
    Fixture f;
    Signal<bool> outer{false};
    Signal<bool> inner{false};
    int clicks = 0;
    int blurs = 0;
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(outer), SpinContent{[&] {
                 Flex(FlexProps{}.vertical(true), [&] {
                     Input(InputProps{}.defaultValue(u8"retained").onBlur([&] { ++blurs; }));
                     Spin(SpinProps{}.spinning(inner), SpinContent{[&] {
                              Button(ButtonProps{}.onClick([&] { ++clicks; }), ButtonContent{[] { Text(u8"Nested"); }});
                          }});
                 });
             }});
    }});
    f.synchronize();
    const auto outer_id = f.services.feedback().spins()[1];
    const auto content = f.services.feedback().spin_snapshot(outer_id).content;
    const auto editor = f.inputs.mounted_inputs()[0];
    const auto button = f.buttons.mounted_buttons()[0];
    check(f.services.focus().request_focus(editor.interaction, FocusModality::keyboard) &&
              f.inputs.sessions().active().valid(),
          "wrapped Input did not start session");
    outer.set(true);
    f.synchronize();
    check(blurs == 1 && !f.inputs.sessions().active().valid() &&
              !f.services.focus().request_focus(editor.interaction, FocusModality::keyboard),
          "Spin retained editor session or allowed focus");
    f.services.append_slot(content, Content{[&] {
                               Button(ButtonProps{}.onClick([&] { ++clicks; }),
                                      ButtonContent{[] { Text(u8"Late child"); }});
                           }});
    f.synchronize();
    const auto late = f.buttons.mounted_buttons().back();
    check(!f.services.interactions().component_active(late.component) &&
              !f.services.focus().request_focus(late.interaction, FocusModality::keyboard),
          "Spin did not block dynamically appended child");
    inner.set(true);
    outer.set(false);
    f.synchronize();
    check(f.services.interactions().component_active(editor.component) &&
              !f.services.interactions().component_active(button.component) &&
              f.services.interactions().component_active(late.component),
          "nested Spin blocking was overwritten");
    inner.set(false);
    f.synchronize();
    const auto r = bounds(f, button.component);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    check(f.services.pointer().state(PointerIdentity::mouse())->capture.has_value(), "wrapped Button did not capture");
    outer.set(true);
    f.synchronize();
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "Spin retained pointer capture");
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    check(clicks == 0, "cancelled loading gesture clicked");
    check(f.services.dispatch_scroll({0, 10, r.x + r.width / 2, r.y + r.height / 2}),
          "loading shield did not consume wheel");
    outer.set(false);
    f.synchronize();
    check(f.inputs.editors().require(editor.editor).value() == "retained", "Spin changed retained editor text");
    f.services.dispose();
    check(f.services.interactions().size() == 0 && f.services.animations().diagnostics().targets == 0,
          "nested Spin leaked input or animation");
}

void themes_slots_and_narrow_layout() {
    Fixture f;
    Signal<ThemeConfig> config{ThemeConfig{}};
    int slots = 0;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  Alert(AlertProps{}.message(u8"Custom").closable().showIcon(), AlertSlots{.icon = AlertIcon{[&] {
                                                                                               ++slots;
                                                                                               Text(u8"i");
                                                                                           }},
                                                                                           .close = AlertClose{[&] {
                                                                                               ++slots;
                                                                                               Text(u8"Close");
                                                                                           }}});
                  Spin(SpinProps{}.spinning(false), SpinContent{[&] {
                           ++slots;
                           Text(u8"Unchanged");
                       }});
                  Progress(ProgressProps{}.percent(42).type(ProgressType::Circle).size(ControlSize::Small));
              }});
    }});
    f.synchronize();
    const auto alert = f.services.feedback().alerts()[0];
    const auto spin = f.services.feedback().spins()[0];
    const auto progress = f.services.feedback().progress()[0];
    const std::array ids{alert, spin, progress};
    std::array<std::uint64_t, 3> measures;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        measures[i] = f.nodes.require(f.services.components().root(ids[i])).measure_count;
    }
    auto custom = ThemeConfig{};
    custom.alert.tokens.message_color = Color::rgba8(11, 22, 33);
    custom.spin.tokens.indicator_color = Color::rgba8(33, 22, 11);
    custom.progress.tokens.default_color = Color::rgba8(44, 55, 66);
    config.set(custom);
    f.synchronize();
    for (std::size_t i = 0; i < ids.size(); ++i) {
        check(measures[i] == f.nodes.require(f.services.components().root(ids[i])).measure_count,
              "feedback color update remeasured");
    }
    check(slots == 3 &&
              f.services.feedback().progress_snapshot(progress).color == *custom.progress.tokens.default_color,
          "feedback theme update reran slots or lost color");
    custom.progress.tokens.circle_size_small = dp(80);
    config.set(custom);
    f.synchronize();
    check(bounds(f, progress).width == 80 && slots == 3, "Progress size token did not update retained layout");
    Fixture narrow;
    narrow.services.mount(Content{[] {
        Alert(AlertProps{}.message(u8"Narrow").showIcon().closable().layout(LayoutStyle{}.width(dp(2))));
        Spin(SpinProps{}.layout(LayoutStyle{}.width(dp(0)).height(dp(0))));
        Progress(ProgressProps{}.type(ProgressType::Circle).layout(LayoutStyle{}.width(dp(0)).height(dp(0))));
    }});
    narrow.synchronize();
    check(!narrow.services.next_frame_deadline(), "zero-area Spin scheduled motion");
}

void invalid_mounts_and_close_retirement() {
    for (int mode = 0; mode < 5; ++mode) {
        Fixture f;
        bool rejected = false;
        try {
            f.services.mount(Content{[&] {
                if (mode == 0) {
                    Progress(ProgressProps{}.percent(std::numeric_limits<float>::quiet_NaN()));
                } else if (mode == 1) {
                    Progress(ProgressProps{}.type(static_cast<ProgressType>(9)));
                } else if (mode == 2) {
                    Progress(ProgressProps{}.status(static_cast<ProgressStatus>(9)));
                } else if (mode == 3) {
                    Spin(SpinProps{}.size(static_cast<ControlSize>(9)));
                } else {
                    Alert(AlertProps{}.status(static_cast<Status>(9)));
                }
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && f.nodes.size() == 0 && f.services.feedback().alerts().empty() &&
                  f.services.feedback().spins().empty() && f.services.feedback().progress().empty(),
              "invalid feedback mount leaked");
    }
    Fixture f;
    runtime::ComponentId owner;
    f.services.mount(Content{
        [&] { Alert(AlertProps{}.message(u8"Close").closable().onClose([&] { f.services.destroy(owner); })); }});
    f.synchronize();
    owner = f.services.feedback().alerts()[0];
    click(f, f.services.feedback().alert_snapshot(owner).close);
    check(f.nodes.size() == 0 && f.scene.size() == 0 && f.services.surfaces().size() == 0 &&
              f.services.rounded_effects().live_count() == 0,
          "Alert close callback teardown leaked");
    Fixture throwing;
    bool rejected = false;
    try {
        throwing.services.mount(Content{[] {
            Spin(SpinProps{}, SpinContent{[] {}},
                 SpinSlots{.indicator = SpinIndicator{[] { throw std::invalid_argument("slot failure"); }}});
        }});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && throwing.nodes.size() == 0 && throwing.services.interactions().size() == 0 &&
              throwing.services.surfaces().size() == 0,
          "throwing Spin indicator leaked partial mount");

    Fixture reentrant;
    Signal<bool> loading{false};
    runtime::ComponentId spin;
    reentrant.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(loading),
             SpinContent{[&] { Input(InputProps{}.onBlur([&] { reentrant.services.destroy(spin); })); }});
    }});
    reentrant.synchronize();
    spin = reentrant.services.feedback().spins()[0];
    check(reentrant.services.focus().request_focus(reentrant.inputs.mounted_inputs()[0].interaction,
                                                   FocusModality::keyboard),
          "reentrant Spin focus failed");
    loading.set(true);
    reentrant.synchronize();
    check(reentrant.nodes.size() == 0 && reentrant.services.interactions().size() == 0 &&
              !reentrant.inputs.sessions().active().valid(),
          "loading blur teardown leaked subtree/session");
}
} // namespace

int main() {
    try {
        alert_controlled();
        spin_retained_and_motion();
        progress_geometry_and_validation();
        spin_input_lifetime_and_nesting();
        themes_slots_and_narrow_layout();
        invalid_mounts_and_close_retirement();
        std::cout << "feedback controlled close, retained shield/input lifetime, finite motion, physical progress "
                     "geometry and precise themes passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
