#include "support/data_display_fixture.hpp"

namespace {
using namespace data_display_test;

CollapsePanel plain(String key) {
    return {key, CollapseTitle{[key] { Text(key); }}, CollapseBody{[] { Text(u8"Body"); }}};
}

void keyed_drafts_and_revision() {
    Fixture f;
    Signal<int> topology{0};
    Signal<int> revision{0};
    InputRef ref;
    int title_runs{};
    int body_runs{};
    int extra_actions{};
    const auto source = bind([&] {
        std::vector<CollapsePanel> panels;
        auto panel = CollapsePanel{u8"draft", CollapseTitle{[&] {
                                       ++title_runs;
                                       Text(u8"Draft");
                                   }},
                                   CollapseBody{[&] {
                                       ++body_runs;
                                       Input(InputProps{}.defaultValue(u8"initial").ref(ref));
                                   }}}
                         .extra(CollapseExtra{[&] {
                             Button(ButtonProps{}.onClick([&] { ++extra_actions; }), [] { Text(u8"Action"); });
                         }})
                         .titleRevision(static_cast<std::uint64_t>(revision.get()));
        if (topology.get() == 1) {
            panels.push_back(plain(u8"new"));
        }
        panels.push_back(panel);
        if (topology.get() == 0) {
            panels.push_back(plain(u8"new"));
        }
        if (topology.get() == 3) {
            panels.push_back(panel);
        }
        return panels;
    });
    f.services.mount(Content{[&] { Collapse(CollapseProps{}.defaultActiveKeys({u8"draft"}), source); }});
    sync(f);
    const auto id = f.services.collapse().mounted()[0];
    const auto initial = f.services.collapse().snapshot(id).panels[0];
    draft(f, ref, u8"retained native draft");
    const auto editor = f.inputs.mounted_inputs()[0].editor;
    check(ref.focus({InputFocusCursor::All}), "retained editor selection focus failed");
    const auto selection = f.inputs.editors().require(editor).selection();
    topology.set(1);
    sync(f);
    check(f.inputs.editors().require(editor).selection() == selection, "keyed move changed native Input selection");
    check(f.services.collapse().snapshot(id).panels[1].body == initial.body &&
              f.inputs.mounted_inputs()[0].editor == editor,
          "reorder remounted body");
    revision.set(1);
    sync(f);
    check(title_runs == 2 && body_runs == 1, "slot revision rebuilt an unrelated slot");
    click(f, f.buttons.mounted_buttons()[0].component);
    check(extra_actions == 1 && f.services.collapse().snapshot(id).panels[1].expanded,
          "independent extra toggled panel");
    bool rejected{};
    try {
        topology.set(3);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && f.services.collapse().snapshot(id).panels.size() == 2 &&
              f.inputs.mounted_inputs()[0].editor == editor,
          "invalid topology did not roll back atomically");
    topology.set(2);
    sync(f);
    check(f.services.collapse().snapshot(id).panels.size() == 1 && body_runs == 1, "key deletion lost retained draft");
}

void finite_motion_lazy_retirement() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<CollapseKeys> active{CollapseKeys{}};
    int runs{};
    InputRef ref;
    f.services.mount(Content{[&] {
        Collapse(CollapseProps{}.activeKeys(active).lazy().destroyOnHidden(),
                 {CollapsePanel{u8"a", CollapseTitle{[] { Text(u8"A"); }}, CollapseBody{[&] {
                                    ++runs;
                                    Input(InputProps{}.ref(ref));
                                }}}});
    }});
    sync(f);
    check(runs == 0, "lazy body executed before activation");
    const auto id = f.services.collapse().mounted()[0];
    active.set({u8"a"});
    sync(f);
    check(runs == 1 && f.services.animations().size() > 0, "opening has no finite animation");
    tick(f, 100000);
    const auto body = f.services.collapse().snapshot(id).panels[0].body;
    const float midpoint = bounds(f, body).height;
    check(midpoint > 0, "opening did not paint intermediate body height");
    tick(f, 400000);
    const float full = bounds(f, body).height;
    check(full > midpoint && f.services.animations().size() == 0, "opening did not settle at full height");
    draft(f, ref, u8"draft");
    const auto session = f.inputs.sessions().active();
    active.set({});
    check(!f.inputs.sessions().active().valid() && !ref.focus() &&
              !bool(f.inputs.dispatch(TextCommitted{u8"late", session})),
          "closing kept input session alive");
    sync(f);
    check(!f.inputs.mounted_inputs().empty() && bounds(f, body).height == full,
          "closing destroyed or hid paint before transition");
    tick(f, 500000);
    check(bounds(f, body).height > 0 && bounds(f, body).height < full, "closing has no intermediate geometry");
    active.set({u8"a"});
    sync(f);
    tick(f, 800000);
    check(runs == 1 && f.services.animations().size() == 0, "reverse remounted content or stayed active");
    active.set({});
    sync(f);
    tick(f, 1200000);
    check(f.inputs.mounted_inputs().empty() && !ref.focus(), "destroyOnHidden did not retire at settled boundary");
    active.set({u8"a"});
    sync(f);
    check(runs == 2, "destroyed lazy body did not get a new generation");
    f.services.set_window_active(false);
    sync(f);
    check(f.services.animations().size() == 0, "inactive window retained animation");
    f.services.set_window_active(true);
    sync(f);
    active.set({});
    sync(f);
    tick(f, 1600000);
    const auto count = f.services.frame_request_revision();
    sync(f);
    sync(f);
    check(f.services.frame_request_revision() == count && f.services.animations().size() == 0,
          "idle Collapse requested endless frames");
}

void failure_cleanup() {
    Fixture f;
    Signal<bool> fail{false};
    f.services.mount(Content{[&] {
        Collapse(CollapseProps{}, bind([&] {
                     std::vector<CollapsePanel> p{plain(u8"safe")};
                     if (fail.get()) {
                         p.push_back({u8"bad", CollapseTitle{[] { throw std::runtime_error("factory"); }},
                                      CollapseBody{[] {}}});
                     }
                     return p;
                 }));
        Button(ButtonProps{}, [] { Text(u8"Sibling"); });
    }});
    sync(f);
    bool threw{};
    try {
        fail.set(true);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && f.services.collapse().mounted().empty() && f.buttons.mounted_buttons().size() == 1,
          "failed append orphaned Collapse regions");
}

void custom_icon_gates_rtl_and_force_render() {
    Fixture f;
    Signal<CollapseKeys> active{CollapseKeys{}};
    Signal<CollapseCollapsible> gate{CollapseCollapsible::Icon};
    Signal<CollapseIconPlacement> placement{CollapseIconPlacement::Start};
    Signal<bool> arrow{true};
    int bodies{};
    int icons{};
    int extras{};
    int changes{};
    f.services.mount(Content{[&] {
        Collapse(CollapseProps{}
                     .activeKeys(active)
                     .lazy()
                     .destroyOnHidden()
                     .rtl()
                     .bordered(false)
                     .collapsible(gate)
                     .expandIconPlacement(placement)
                     .expandIcon([&](const CollapseExpandInfo& info) {
                         ++icons;
                         return SlotContent<CollapseExpandIconSlot>{[state = info.active] {
                             Icon(IconProps{}.name(bind(
                                 [state] { return state.get() ? IconName::MinusOutlined : IconName::PlusOutlined; })));
                         }};
                     })
                     .onChange([&](CollapseKeys value) {
                         ++changes;
                         active.set(std::move(value));
                     }),
                 {CollapsePanel{u8"a", CollapseTitle{[] { Text(u8"Title"); }}, CollapseBody{[&] {
                                    ++bodies;
                                    Input(InputProps{}.defaultValue(u8"retained"));
                                }}}
                      .forceRender()
                      .showArrow(arrow)
                      .extra(CollapseExtra{
                          [&] { Button(ButtonProps{}.onClick([&] { ++extras; }), [] { Text(u8"Extra"); }); }})});
    }});
    sync(f);
    const auto id = f.services.collapse().mounted()[0];
    auto panel = f.services.collapse().snapshot(id).panels[0];
    check(bodies == 1 && icons == 1 && f.nodes.require(panel.arrow).bounds.x > bounds(f, panel.title).x,
          "forceRender or custom RTL icon was not mounted correctly");
    const auto editor = f.inputs.mounted_inputs()[0].editor;
    const auto header = bounds(f, panel.header);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                       header.x + header.width / 2, header.y + header.height / 2});
    }
    check(changes == 0, "icon-only gate accepted title/header activation");
    const auto icon = f.nodes.require(panel.arrow).bounds;
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                       icon.x + icon.width / 2, icon.y + icon.height / 2});
    }
    sync(f);
    check(changes == 1 && f.services.collapse().snapshot(id).panels[0].expanded,
          "custom icon-only trigger did not expand");
    placement.set(CollapseIconPlacement::End);
    sync(f);
    check(f.nodes.require(panel.arrow).bounds.x < bounds(f, panel.title).x && icons == 1,
          "end placement remounted or failed to mirror icon");
    gate.set(CollapseCollapsible::Disabled);
    sync(f);
    click(f, f.buttons.mounted_buttons()[0].component);
    check(extras == 1 && changes == 1, "disabled header swallowed independent extra action");
    active.set({});
    sync(f);
    check(f.inputs.mounted_inputs()[0].editor == editor && bodies == 1, "forceRender did not override destroyOnHidden");
    gate.set(CollapseCollapsible::Icon);
    arrow.set(false);
    sync(f);
    click(f, panel.header);
    check(changes == 1, "hidden icon remained an icon-only trigger");
}

void lazy_and_replaced_body_theme() {
    Fixture f;
    Signal<CollapseKeys> active{CollapseKeys{}};
    Signal<int> revision{0};
    ThemeConfig config;
    const auto green = Color::rgba8(20, 130, 60);
    config.collapse.tokens.body_text = green;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  Collapse(CollapseProps{}.activeKeys(active).lazy(), bind([&] {
                               return std::vector<CollapsePanel>{CollapsePanel{
                                   u8"a", CollapseTitle{[] { Text(u8"Header"); }}, CollapseBody{[] {
                                       Text(u8"styled body");
                                   }}}.bodyRevision(static_cast<std::uint64_t>(revision.get()))};
                           }));
              }});
    }});
    sync(f);
    active.set({u8"a"});
    sync(f);
    check(text_color(f, "styled body") == channels(green), "lazy body lost its Collapse Theme foreground");
    revision.set(1);
    sync(f);
    check(text_color(f, "styled body") == channels(green), "replacement body lost its semantic style");
}

void reactive_icon_retirement() {
    Fixture f;
    Signal<CollapseKeys> active{CollapseKeys{}};
    runtime::ComponentId owner;
    f.services.mount(Content{[&] {
        Collapse(CollapseProps{}.activeKeys(active).expandIcon([&](const CollapseExpandInfo& info) {
            return SlotContent<CollapseExpandIconSlot>{[&, source = info.active] {
                Icon(IconProps{}.name(bind([&, source] {
                    if (source.get() && owner.valid()) {
                        static_cast<void>(f.services.destroy(owner));
                    }
                    return IconName::PlusOutlined;
                })));
            }};
        }),
                 {plain(u8"a")});
        Button(ButtonProps{}, [] { Text(u8"Sibling"); });
    }});
    owner = f.services.collapse().mounted()[0];
    sync(f);
    active.set({u8"a"});
    sync(f);
    check(!f.services.components().contains(owner) && f.buttons.mounted_buttons().size() == 1 &&
              f.services.animations().size() == 0,
          "reactive expand icon accessed or retained its retired owner");
}

void self_destroying_factory() {
    Fixture f;
    Signal<bool> remove{false};
    runtime::ComponentId owner;
    const auto body = [&] {
        Input(InputProps{});
        if (remove.get()) {
            check(f.services.destroy(owner), "factory could not destroy owner");
        }
    };
    f.services.mount(Content{[&] {
        Collapse(CollapseProps{}, bind([&] {
                     std::vector<CollapsePanel> rows{
                         {u8"safe", CollapseTitle{[] { Text(u8"Safe"); }}, CollapseBody{[] {}}}};
                     if (remove.get()) {
                         rows.push_back({u8"retire", CollapseTitle{[] {}}, CollapseBody{body}});
                     }
                     return rows;
                 }));
        Button(ButtonProps{}, [] { Text(u8"Sibling"); });
    }});
    owner = f.services.collapse().mounted()[0];
    sync(f);
    try {
        remove.set(true);
    } catch (const std::exception&) {
    }
    sync(f);
    check(!f.services.components().contains(owner) && f.inputs.mounted_inputs().empty() &&
              f.buttons.mounted_buttons().size() == 1,
          "self-destroying factory left orphan input or removed sibling");
}

} // namespace

int main() {
    try {
        form_composition(0);
        self_destroying_factory();
        reactive_icon_retirement();
        keyed_drafts_and_revision();
        finite_motion_lazy_retirement();
        custom_icon_gates_rtl_and_force_render();
        lazy_and_replaced_body_theme();
        failure_cleanup();
        std::cout << "Collapse: 3 new retained/motion/failure scenarios passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
