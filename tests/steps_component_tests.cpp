#include "component/steps_component.hpp"
#include "support/navigation_fixture.hpp"
#include <iostream>
#include <limits>

namespace {
using namespace ryn_test::navigation;

void controlled_focus_and_retry() {
    Fixture f;
    Signal<std::size_t> current{0};
    Signal<bool> disabled{true};
    Signal<bool> hidden{false};
    Signal<bool> vertical{false};
    std::vector<std::size_t> requests;
    f.services.mount(Content{[&] {
        Steps(StepsProps{}.current(current).vertical(vertical).onChange([&](std::size_t i) { requests.push_back(i); }),
              {{u8"a", u8"First"},
               StepsItem{u8"skip", u8"Hidden"}.visible(hidden),
               StepsItem{u8"b", u8"Second"}.disabled(disabled),
               {u8"c", u8"Third"}});
    }});
    sync(f);
    const auto id = f.services.steps().mounted()[0];
    const auto original = f.services.steps().snapshot(id);
    check(std::ranges::count_if(f.services.interactions().declaration_order(),
                                [&](auto hit) {
                                    const auto& record = f.services.interactions().require(hit);
                                    return record.eligible && record.focusable && record.tab_stop &&
                                           f.services.interactions().branch_active(hit);
                                }) == 1,
          "Steps did not expose exactly one roving Tab stop");
    key(f, Key::tab);
    check(f.services.focus().state().focused == interaction(f, original.items[0]), "Steps roving entry wrong");
    key(f, Key::right);
    check(f.services.focus().state().focused == interaction(f, original.items[3]) && requests.empty() &&
              f.services.steps().snapshot(id).current == 0,
          "Steps arrow submitted or failed to skip disabled/hidden");
    key(f, Key::enter);
    check(requests == std::vector<std::size_t>{3} && f.services.steps().snapshot(id).current == 0,
          "controlled Steps request changed authority");
    key(f, Key::enter);
    check(requests.size() == 2, "controlled rejected Steps request could not retry");
    f.services.focus().dispatch({Key::space, KeyAction::down, KeyModifier::none, true});
    f.services.focus().dispatch({Key::space, KeyAction::up});
    check(requests.size() == 2, "Steps repeat/keyup activated");
    disabled.set(false);
    sync(f);
    click(f, original.items[2]);
    check(requests.size() == 3 && requests.back() == 2 && f.services.steps().snapshot(id).current == 0,
          "Steps reenabled real pointer hit/original index failed");
    check(!f.services.focus().state().focus_visible, "Steps pointer focus incorrectly keyboard-visible");
    key(f, Key::space);
    check(requests.size() == 4 && f.services.focus().state().focus_visible, "Steps keyboard modality not promoted");
    key(f, Key::home);
    check(f.services.focus().state().focused == interaction(f, original.items[0]) && requests.size() == 4,
          "Steps Home activated");
    key(f, Key::end);
    check(f.services.focus().state().focused == interaction(f, original.items[3]) && requests.size() == 4,
          "Steps End activated");
    vertical.set(true);
    sync(f);
    key(f, Key::up);
    check(f.services.focus().state().focused == interaction(f, original.items[2]) && requests.size() == 4,
          "Steps vertical arrow failed");
    current.set(2);
    sync(f);
    check(f.services.steps().snapshot(id).current == 2 && f.services.steps().snapshot(id).items == original.items,
          "controlled current did not apply or remounted");
    current.set(std::numeric_limits<std::size_t>::max());
    sync(f);
    const auto all = f.services.steps().snapshot(id);
    check(std::ranges::all_of(all.statuses, [](auto v) { return v == StepsStatus::Finish; }) && requests.size() == 4,
          "Steps extreme current overflow/implicit callback");
}

runtime::ComponentId builtin_status_icon(Fixture& f, runtime::ComponentId item) {
    const auto icon_owner = f.services.components().children(item).front();
    const auto wrapper = f.services.components().children(icon_owner)[1];
    return f.services.components().children(wrapper).front();
}

void states_and_native_scene() {
    Fixture f;
    Signal<std::size_t> current{1};
    Signal<StepsStatus> status{StepsStatus::Error};
    Signal<std::optional<StepsStatus>> override_status{std::nullopt};
    Signal<StepsSize> size{StepsSize::Default};
    f.services.mount(Content{[&] {
        Steps(StepsProps{}.current(current).status(status).size(size),
              {StepsItem{u8"a", u8"First"}.description(u8"Complete"),
               StepsItem{u8"b", u8"Second"}.subtitle(u8"Optional").description(u8"Current detail"),
               StepsItem{u8"c", u8"Third"}.status(override_status)});
    }});
    sync(f);
    const auto id = f.services.steps().mounted()[0];
    const auto original = f.services.steps().snapshot(id);
    check(original.statuses == std::vector{StepsStatus::Finish, StepsStatus::Error, StepsStatus::Wait},
          "Steps status projection wrong");
    check(f.services.text().icon_snapshot(builtin_status_icon(f, original.items[0])).source.bundled_name() ==
                  IconName::CheckOutlined &&
              f.services.text().icon_snapshot(builtin_status_icon(f, original.items[1])).source.bundled_name() ==
                  IconName::CloseOutlined,
          "native Steps check/error icon wrong");
    check(glyphs_inside(f, id, bounds(f, id)) > 0, "Steps has no actual text glyphs");
    const auto& third = find_text(f, String{u8"3"}.view());
    check(f.services.components().branch_active(third.component) && f.scene.primitive(third.scene).instances.count > 0,
          "Steps Wait number missing");
    const auto icon_bounds = f.nodes.require(original.icons[2]).bounds;
    const auto number = bounds(f, third.component);
    check(std::abs(number.x + number.width / 2 - icon_bounds.x - icon_bounds.width / 2) < .01F &&
              std::abs(number.y + number.height / 2 - icon_bounds.y - icon_bounds.height / 2) < .01F,
          "Steps number not centered in native circle");
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [](const auto& e) { return e.material.opacity > 0 && e.geometry.shape.radius == 16; }),
          "native Steps circle missing");
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [](const auto& e) {
                                  return e.material.opacity > 0 && e.geometry.shape.rect.height == 1 &&
                                         e.geometry.shape.rect.width == 16;
                              }),
          "native Steps connector missing");
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [&](const auto& e) {
                                  return e.material.opacity > 0 && e.geometry.shape.rect.height == 1 &&
                                         e.geometry.shape.rect.width == 16 &&
                                         e.material.color ==
                                             f.services.components().theme_scope(id)->snapshot().steps().colors.error;
                              }),
          "Steps error connector did not follow status");
    override_status.set(StepsStatus::Process);
    sync(f);
    check(f.services.steps().snapshot(id).statuses[2] == StepsStatus::Process, "Steps item status override ignored");
    size.set(StepsSize::Small);
    sync(f);
    check(f.nodes.require(original.icons[2]).bounds.width == 24 &&
              f.services.steps().snapshot(id).items == original.items,
          "Small Steps icon size/remount wrong");
    for (auto hit : f.services.interactions().declaration_order()) {
        check(!f.services.interactions().require(hit).eligible, "Steps without onChange became interactive");
    }
}

void rich_geometry_flow_and_theme() {
    Fixture f;
    Signal<LogicalLength> width{dp(420)};
    Signal<LogicalLength> height{dp(110)};
    Signal<bool> vertical{false};
    Signal<bool> shown{true};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    int runs = 0;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Flex(FlexProps{}.vertical(true).gap(dp(19)), FlexContent{[&] {
                           Text(u8"Before");
                           Steps(StepsProps{}
                                     .vertical(vertical)
                                     .layout(LayoutStyle{}.width(width).height(height))
                                     .onChange([](std::size_t) {}),
                                 {StepsItem{u8"a", u8"First"}
                                      .visible(shown)
                                      .richTitle(StepsTitle{[&] {
                                          ++runs;
                                          Text(u8"Rich title");
                                      }})
                                      .richSubtitle(StepsSubtitle{[&] {
                                          ++runs;
                                          Text(u8"Sub");
                                      }})
                                      .richDescription(StepsDescription{[&] {
                                          ++runs;
                                          Text(u8"Line one\nLine two\nLine three");
                                      }})
                                      .icon(StepsIcon{[&] {
                                          ++runs;
                                          Icon(IconProps{}.name(IconName::SearchOutlined));
                                      }}),
                                  StepsItem{u8"b", u8"Second"}.visible(shown).description(u8"Description\nDetail")});
                           Text(u8"After");
                       }});
              }});
    }});
    sync(f);
    const auto id = f.services.steps().mounted()[0];
    const auto original = f.services.steps().snapshot(id);
    const auto root = f.services.components().root(id);
    const auto measures = f.nodes.require(root).measure_count;
    const auto sibling = f.services.components().root(find_text(f, String{u8"After"}.view()).component);
    const auto sibling_measures = f.nodes.require(sibling).measure_count;
    ThemeConfig config;
    config.steps.tokens.title = Color::rgba8(20, 50, 90);
    config.steps.tokens.process_background = Color::rgba8(10, 40, 80);
    theme.set(config);
    sync(f);
    check(f.nodes.require(root).measure_count == measures &&
              f.nodes.require(sibling).measure_count == sibling_measures && runs == 4,
          "Steps material update remeasured/remounted unrelated content");
    for (bool orientation : {false, true}) {
        vertical.set(orientation);
        for (float w : {420.0F, 48.0F, 4.0F, 0.0F}) {
            width.set(dp(w));
            for (float h : {110.0F, 24.0F, 4.0F, 0.0F}) {
                height.set(dp(h));
                sync(f);
                const auto visible = glyphs_inside(f, id, bounds(f, id));
                surfaces_and_hits_inside(f, bounds(f, id));
                if (w == 0 || h == 0) {
                    check(visible == 0, "zero Steps geometry paints glyphs");
                }
                for (auto item : original.items) {
                    glyphs_inside(f, item, bounds(f, item));
                }
                for (const auto& hit : f.services.hit_test().records()) {
                    if (!descendant(f, f.services.interactions().require(hit.interaction).component, id) ||
                        !hit.effective_eligible) {
                        continue;
                    }
                    const auto r = graphics::intersect_effect_bounds(hit.translated_bounds, hit.effective_clip);
                    const auto allowed = bounds(f, id);
                    check(r.width <= 0 || r.height <= 0 ||
                              (r.x >= allowed.x && r.y >= allowed.y &&
                               r.x + r.width <= allowed.x + allowed.width + .01F &&
                               r.y + r.height <= allowed.y + allowed.height + .01F),
                          "Steps hit escaped allocation");
                }
            }
        }
    }
    width.set(dp(420));
    height.set(dp(110));
    sync(f);
    const auto first_bounds = bounds(f, original.items[0]);
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                   first_bounds.x + first_bounds.width / 2, first_bounds.y + first_bounds.height / 2});
    check(f.services.pointer().state(PointerIdentity::mouse())->capture == interaction(f, original.items[0]) &&
              f.services.focus().state().focused == interaction(f, original.items[0]),
          "Steps hide fixture did not own capture/focus");
    shown.set(false);
    sync(f);
    const auto before = bounds(f, find_text(f, String{u8"Before"}.view()).component);
    const auto after = bounds(f, find_text(f, String{u8"After"}.view()).component);
    check(f.nodes.require(root).out_of_flow && std::abs(after.y - before.y - before.height - 19) < .01F,
          "all-hidden Steps retained Flex gap");
    check(!f.services.focus().state().focused && !f.services.pointer().state(PointerIdentity::mouse())->capture &&
              !f.services.pointer().state(PointerIdentity::mouse())->press_origin,
          "hidden Steps retained owned focus/capture");
    shown.set(true);
    width.set(dp(420));
    height.set(dp(110));
    sync(f);
    check(f.services.steps().snapshot(id).items == original.items && runs == 4,
          "Steps visibility/geometry remounted rich slots");
    static_cast<void>(f.frames.consume_request());
    sync(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline(), "Steps idle requests frames");
}

void child_input_and_button() {
    Fixture f;
    int requests = 0;
    int clicks = 0;
    f.services.mount(Content{[&] {
        Steps(StepsProps{}.onChange([&](std::size_t) { ++requests; }),
              {StepsItem{u8"a", u8"A"}.richTitle(
                   StepsTitle{[&] { Input(InputProps{}.layout(LayoutStyle{}.width(dp(90)))); }}),
               StepsItem{u8"b", u8"B"}.richDescription(StepsDescription{[&] {
                   Button(ButtonProps{}.onClick([&] { ++clicks; }), ButtonContent{[] { Text(u8"Child"); }});
               }})});
    }});
    sync(f);
    const auto input = f.inputs.mounted_inputs()[0];
    click(f, input.component);
    key(f, Key::space);
    check(f.services.focus().state().focused == input.interaction && f.inputs.sessions().active().valid() &&
              requests == 0,
          "Steps stole Input focus/key/session");
    const auto stamp = f.inputs.sessions().active();
    check(static_cast<bool>(f.inputs.dispatch(CompositionChanged{String{u8"ni"}, {2, 0}, stamp})),
          "Steps child preedit rejected");
    key(f, Key::enter);
    check(f.inputs.editors().require(input.editor).composition().active && requests == 0,
          "Steps stole composing Input Enter");
    check(static_cast<bool>(f.inputs.dispatch(TextCommitted{String{u8"你"}, stamp})),
          "Steps child composition commit rejected");
    check(!f.inputs.editors().require(input.editor).composition().active && requests == 0,
          "Steps child composition changed navigation");
    const auto button = f.buttons.mounted_buttons()[0];
    click(f, button.component);
    key(f, Key::enter);
    check(clicks == 2 && requests == 0, "Steps wrapper stole native child Button activation");
}

void boundaries_lifetime_and_validation() {
    Fixture f;
    Signal<bool> spinning{false};
    Signal<bool> open{false};
    int requests = 0;
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(spinning), SpinContent{[&] {
                 Steps(StepsProps{}.onChange([&](std::size_t) { ++requests; }), {{u8"a", u8"A"}, {u8"b", u8"B"}});
             }});
        Modal(ModalProps{}.open(open).footer(ModalFooterMode::None), ModalContent{[] { Text(u8"Modal"); }});
    }});
    sync(f);
    const auto id = f.services.steps().mounted()[0];
    const auto original = f.services.steps().snapshot(id);
    click(f, original.items[1]);
    check(requests == 1 && f.services.steps().snapshot(id).current == 1, "default Steps activation failed");
    spinning.set(true);
    sync(f);
    click(f, original.items[0]);
    check(requests == 1 && !f.services.focus().state().focused, "Spin did not block Steps/clear focus");
    spinning.set(false);
    sync(f);
    open.set(true);
    sync(f);
    click(f, original.items[0]);
    check(requests == 1, "Modal leaked Steps activation");
    open.set(false);
    sync(f);
    click(f, original.items[0]);
    check(requests == 2 && f.services.steps().snapshot(id).items == original.items, "Steps boundary restore failed");
    for (bool keyboard : {false, true}) {
        Fixture life;
        runtime::ComponentId owner;
        life.services.mount(Content{[&] {
            Steps(StepsProps{}.onChange([&](std::size_t) { static_cast<void>(life.services.destroy(owner)); }),
                  {{u8"a", u8"A"}, {u8"b", u8"B"}});
        }});
        sync(life);
        owner = life.services.steps().mounted()[0];
        const auto target = life.services.steps().snapshot(owner).items[1];
        if (keyboard) {
            static_cast<void>(life.services.focus().request_focus(interaction(life, target), FocusModality::keyboard));
            key(life, Key::enter);
        } else {
            click(life, target);
        }
        check(life.services.steps().mounted().empty() && life.services.interactions().size() == 0 &&
                  life.services.rounded_effects().live_count() == 0,
              "Steps callback destroy leaked");
    }
    for (int mode = 0; mode < 5; ++mode) {
        Fixture invalid;
        bool rejected = false;
        try {
            invalid.services.mount(Content{[&] {
                StepsProps props;
                if (mode == 0) {
                    props.current(std::size_t{0}).defaultCurrent(0);
                }
                if (mode == 1) {
                    props.size(static_cast<StepsSize>(99));
                }
                if (mode == 2) {
                    props.status(static_cast<StepsStatus>(99));
                }
                Steps(props, {{u8"a", u8"A"}, {mode == 3 ? String{} : String{u8"a"}, u8"B"}});
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && invalid.nodes.size() == 0 && invalid.services.interactions().size() == 0,
              "invalid Steps partially mounted");
    }
}
} // namespace

int main() {
    try {
        controlled_focus_and_retry();
        states_and_native_scene();
        rich_geometry_flow_and_theme();
        child_input_and_button();
        boundaries_lifetime_and_validation();
        std::cout << "Steps component contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
