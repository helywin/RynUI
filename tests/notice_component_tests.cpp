#include "component/notice_component.hpp"
#include "component/select_component.hpp"
#include "component/feedback_component.hpp"
#include "component/modal_component.hpp"
#include "support/input_fixture.hpp"
#include "support/notice_example.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
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

void sync(Fixture& f, float w = 720, float h = 600) {
    check(f.services.layout_and_synchronize({w, h}, {0, 0, w, h}), "notice layout failed");
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
}

runtime::ComponentId entry(Fixture& f, std::size_t holder = 0, std::size_t index = 0) {
    return f.services.notices().entries(f.services.notices().holders()[holder])[index].id;
}

void event() {
    Fixture f;
    MessageRef messages;
    NotificationRef notices;
    int closed = 0;
    f.services.mount(Content{[&] {
        MessageHolder(MessageHolderProps{}.ref(messages));
        NotificationHolder(NotificationHolderProps{}.ref(notices));
        Button(ButtonProps{}.onClick([&] {
            check(messages.open(u8"save", MessageProps{}.content(u8"Saved").status(NoticeStatus::Success).duration(0)),
                  "event Message open failed");
        }),
               [] { Text(u8"Save"); });
    }});
    sync(f);
    click(f, f.buttons.mounted_buttons()[0].component);
    sync(f);
    const auto message = entry(f);
    check(f.services.notices().snapshot(message).status == NoticeStatus::Success, "event notice status missing");
    const auto r = bounds(f, message);
    check(std::abs(r.x + r.width / 2 - 360) < .01F && r.y == 8, "Message is not top centered");
    check(f.scene.glyph_scene().instances().size() > 0 && f.services.surfaces().instances().size() > 0,
          "notice has no real scene");
    check(messages.open(u8"save", MessageProps{}.content(u8"Updated").status(NoticeStatus::Loading)) &&
              entry(f) == message,
          "same key replaced entry");
    for (auto status : {NoticeStatus::Success, NoticeStatus::Info, NoticeStatus::Warning, NoticeStatus::Error,
                        NoticeStatus::Loading}) {
        check(notices.open(u8"notify", NotificationProps{}
                                           .title(u8"Title")
                                           .description(u8"Description")
                                           .status(status)
                                           .duration(0)
                                           .onClose([&] { ++closed; })),
              "Notification open failed");
        sync(f);
        check(f.services.notices().snapshot(entry(f, 1)).status == status, "Notification status drifted");
    }
    const auto notification = entry(f, 1);
    check(f.services.focus().state().focused.has_value(), "trigger focus unexpectedly lost");
    click(f, f.services.notices().snapshot(notification).close);
    sync(f);
    check(closed == 1 && f.services.notices().entries(f.services.notices().holders()[1]).empty(),
          "real close Button did not dismiss exactly once");
    check(messages.clear() && !messages.close(u8"save"), "Message clear failed");
}

void retention() {
    Fixture f;
    NotificationRef notices;
    InputRef input_ref;
    Signal<String> title{String{u8"Title"}};
    Signal<NotificationPlacement> placement{NotificationPlacement::TopRight};
    auto initial = std::make_shared<ThemeConfig>();
    Signal<ThemeConfig> theme{*initial};
    int mounts = 0;
    int actions = 0;
    int action_clicks = 0;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme),
              ThemeContent{[&] { NotificationHolder(NotificationHolderProps{}.ref(notices)); }});
        Input(InputProps{}.defaultValue(u8"Outside"));
    }});
    sync(f);
    const auto outside = f.inputs.mounted_inputs()[0];
    check(f.services.focus().request_focus(outside.interaction, FocusModality::keyboard), "outside focus failed");
    check(notices.open(u8"editor", NotificationProps{}.title(title).placement(placement).duration(0),
                       {.description = NotificationDescription{[&] {
                            ++mounts;
                            Input(InputProps{}.defaultValue(u8"Original").ref(input_ref));
                        }},
                        .actions = NotificationActions{[&] {
                            ++actions;
                            Button(ButtonProps{}.onClick([&] { ++action_clicks; }), [] { Text(u8"Apply"); });
                        }}}),
          "rich notice open failed");
    sync(f);
    check(f.services.focus().state().focused == outside.interaction, "Notification open stole focus");
    const auto id = entry(f);
    const auto original = f.services.notices().snapshot(id);
    const auto input = f.inputs.mounted_inputs()[1];
    check(input_ref.focus({InputFocusCursor::All}), "notice editor cannot focus");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"retained draft"}, f.inputs.sessions().active()})),
          "notice editor cannot edit");
    const auto session = f.inputs.sessions().active();
    title.set(u8"Reactive title");
    placement.set(NotificationPlacement::BottomLeft);
    auto next = std::make_shared<ThemeConfig>(*initial);
    next->notification.tokens.text = Color::rgba8(90, 40, 10);
    next->notification.tokens.padding_inline = dp(16);
    theme.set(*next);
    check(notices.open(u8"other", NotificationProps{}.title(u8"Other").duration(0)), "second notice failed");
    check(notices.open(u8"editor", NotificationProps{}.description(u8"String does not replace rich content")),
          "partial update failed");
    sync(f);
    check(f.services.notices().snapshot(id).description == original.description &&
              f.inputs.mounted_inputs()[1].component == input.component && mounts == 1 && actions == 1 &&
              input_ref.bound() && f.inputs.sessions().active() == session &&
              f.inputs.display_snapshot(input.component).text == "retained draft",
          "ordinary updates remounted editor or lost draft/ref/session");
    check(notices.open(u8"editor", NotificationProps{},
                       {.actions = NotificationActions{[] { Text(u8"Replaced actions"); }}}),
          "explicit action replacement failed");
    sync(f);
    check(mounts == 1 && f.services.notices().snapshot(id).description == original.description &&
              f.services.notices().snapshot(id).actions != original.actions,
          "slot replacement affected unrelated region");
    check(notices.close(u8"editor"), "focused notification close failed");
    sync(f);
    check(!input_ref.bound() && !f.inputs.sessions().active().valid() &&
              f.services.focus().state().focused != input.interaction,
          "focused close left editor/ref/session");
    check(notices.clear(), "clear failed");
    check(f.services.notices().entries(f.services.notices().holders()[0]).empty(), "entries remained after clear");
}

void clock_test() {
    Fixture f;
    MessageRef messages;
    NotificationRef notices;
    Signal<std::size_t> maximum{std::size_t{2}};
    int closes = 0;
    f.services.mount(Content{[&] {
        MessageHolder(MessageHolderProps{}.ref(messages).maxCount(maximum));
        NotificationHolder(NotificationHolderProps{}.ref(notices));
    }});
    sync(f);
    check(!f.services.next_frame_deadline(), "empty holders scheduled polling");
    check(messages.open(u8"a", MessageProps{}.content(u8"A").duration(0).onClose([&] { ++closes; })),
          "persistent open failed");
    check(!f.services.next_frame_deadline(), "persistent static notice scheduled polling");
    check(messages.open(u8"b", MessageProps{}.content(u8"B").duration(0)), "B failed");
    const auto b = entry(f, 0, 1);
    check(messages.open(u8"a", MessageProps{}.content(u8"same order")), "A update failed");
    check(messages.open(u8"c", MessageProps{}.content(u8"C").duration(0)), "C failed");
    check(closes == 1 && entry(f) == b, "maxCount did not evict oldest insertion");
    maximum.set(1);
    check(f.services.notices().entries(f.services.notices().holders()[0]).size() == 1,
          "reactive maxCount did not trim");
    check(messages.clear(), "persistent clear failed");
    f.services.set_animation_time(animation::AnimationTime::microseconds(1000000));
    check(messages.open(u8"default", MessageProps{}.content(u8"default")), "default duration failed");
    check(f.services.next_frame_deadline() == animation::AnimationTime::microseconds(4000000),
          "Message default duration drifted");
    check(notices.open(u8"default", NotificationProps{}.title(u8"default")), "Notification default failed");
    check(f.services.notices().next_auxiliary_deadline() == animation::AnimationTime::microseconds(4000000),
          "earliest deadline wrong");
    f.services.set_window_active(false);
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(4000000)));
    check(f.services.notices().entries(f.services.notices().holders()[0]).empty(), "inactive notice did not expire");
    check(f.services.next_frame_deadline() == animation::AnimationTime::microseconds(5500000),
          "remaining Notification deadline wrong");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(2000000)));
    check(notices.open(u8"default", NotificationProps{}.duration(1)), "duration update failed");
    check(f.services.next_frame_deadline() == animation::AnimationTime::microseconds(5000000),
          "clock regressed during keyed update");
    f.services.set_window_active(true);
    sync(f);
    const auto r = bounds(f, entry(f, 1));
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::move, PointerButton::none, r.x + 10, r.y + 10});
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(5000000)));
    check(f.services.notices().entries(f.services.notices().holders()[1]).empty() && !f.services.next_frame_deadline(),
          "hover paused expiry or idle polling remained");
}

void lifecycle() {
    Fixture f;
    NotificationRef notices;
    runtime::ComponentId parent;
    f.services.mount(Content{[&] {
        auto& build = runtime::require_component_build_context();
        parent = build.mount_component<int>(0);
        f.layout.set_layout(build.root(parent), layout::BoxLayout{});
        build.mount_slot(parent, Content{[&] { NotificationHolder(NotificationHolderProps{}.ref(notices)); }});
    }});
    sync(f);
    const auto holder = f.services.notices().holders()[0];
    int closed = 0;
    check(notices.open(u8"a", NotificationProps{}.title(u8"A").duration(.01).onClose([&] {
        ++closed;
        check(notices.open(u8"a", NotificationProps{}.title(u8"New A").duration(0)), "callback reopen failed");
        check(notices.open(u8"b", NotificationProps{}.duration(0)), "callback update failed");
    })),
          "A failed");
    check(notices.open(u8"b", NotificationProps{}.title(u8"B").duration(.01)), "B failed");
    const auto old_a = entry(f);
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(10000)));
    check(closed == 1 && f.services.notices().entries(holder).size() == 2 &&
              f.services.notices().entries(holder)[1].id != old_a,
          "expiry batch retired reopened/updated key");
    check(notices.clear(), "clear failed");
    check(f.services.notices().entries(holder).empty(), "clear callback result wrong");
    check(notices.open(u8"destroy", NotificationProps{}.title(bind([&] {
        f.services.destroy(holder);
        return String{u8"stale"};
    }))) == false,
          "getter continued after synchronous holder destruction");
    check(!notices.bound() && !notices.clear(), "unmounted Ref stayed live");
    f.services.append_slot(parent, Content{[&] { NotificationHolder(NotificationHolderProps{}.ref(notices)); }});
    check(notices.bound(), "Ref rebind failed");
    const auto rebound = f.services.notices().holders()[0];
    check(!notices.open(u8"getter-rebind", NotificationProps{}.title(bind([&] {
        f.services.destroy(rebound);
        f.services.append_slot(parent, Content{[&] { NotificationHolder(NotificationHolderProps{}.ref(notices)); }});
        return String{u8"old generation"};
    }))),
          "old getter continued against rebound Ref");
    check(notices.bound() && f.services.notices().entries(f.services.notices().holders()[0]).empty(),
          "getter rebind mutated the new holder");
    check(notices.open(u8"slot", NotificationProps{}.duration(0), {.description = NotificationDescription{[&] {
                                                                       f.services.destroy(
                                                                           f.services.notices().holders()[0]);
                                                                   }}}) == false,
          "slot continued after holder destruction");
    f.services.append_slot(parent, Content{[&] { NotificationHolder(NotificationHolderProps{}.ref(notices)); }});
    check(notices.open(u8"exception", NotificationProps{}.title(u8"kept").duration(0)),
          "initial slot exception entry failed");
    const auto before = f.services.notices().snapshot(entry(f)).description;
    bool threw = false;
    try {
        static_cast<void>(
            notices.open(u8"exception", NotificationProps{},
                         {.description = NotificationDescription{[] { throw std::runtime_error("slot failure"); }}}));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && f.services.notices().snapshot(entry(f)).description == before, "slot exception lost old region");
    check(notices.open(u8"dispose", NotificationProps{}.duration(0).onClose([&] { f.services.destroy(parent); })),
          "dispose entry failed");
    check(notices.close(u8"dispose") && !notices.bound(), "close callback unload failed");
}

void boundaries() {
    Fixture f;
    NotificationRef normal;
    NotificationRef under_spin;
    NotificationRef under_hidden;
    InputRef normal_input;
    InputRef spin_input;
    InputRef hidden_input;
    InputRef foreign;
    Signal<bool> modal_open{false};
    Signal<bool> spinning{false};
    runtime::ComponentId hidden_owner;
    runtime::ComponentId foreign_owner;
    f.services.mount(Content{[&] {
        NotificationHolder(NotificationHolderProps{}.ref(normal));
        Spin(SpinProps{}.spinning(spinning),
             SpinContent{[&] { NotificationHolder(NotificationHolderProps{}.ref(under_spin)); }});
        auto& build = runtime::require_component_build_context();
        hidden_owner = build.mount_component<int>(0);
        f.layout.set_layout(build.root(hidden_owner), layout::BoxLayout{});
        build.mount_slot(hidden_owner,
                         Content{[&] { NotificationHolder(NotificationHolderProps{}.ref(under_hidden)); }});
        foreign_owner = build.mount_component<int>(0);
        f.layout.set_layout(build.root(foreign_owner), layout::BoxLayout{});
        build.mount_slot(foreign_owner, Content{[&] { Input(InputProps{}.ref(foreign)); }});
        Modal(ModalProps{}.open(modal_open), ModalContent{[] { Text(u8"Modal"); }});
    }});
    sync(f);
    check(normal.open(u8"n", NotificationProps{}.duration(0),
                      {.description = NotificationDescription{[&] { Input(InputProps{}.ref(normal_input)); }}}),
          "normal notice failed");
    check(under_spin.open(u8"s", NotificationProps{}.duration(0),
                          {.description = NotificationDescription{[&] { Input(InputProps{}.ref(spin_input)); }}}),
          "Spin notice failed");
    check(under_hidden.open(u8"h", NotificationProps{}.duration(0),
                            {.description = NotificationDescription{[&] { Input(InputProps{}.ref(hidden_input)); }}}),
          "hidden notice failed");
    sync(f);
    check(normal_input.focus(), "notice outside boundary cannot focus initially");
    modal_open.set(true);
    sync(f);
    check(!normal_input.focus() && !spin_input.focus() && !hidden_input.focus(), "notice crossed Modal boundary");
    modal_open.set(false);
    sync(f);
    check(spin_input.focus(), "Spin content cannot focus before suppression");
    spinning.set(true);
    sync(f);
    check(!spin_input.focus() && !f.inputs.sessions().active().valid(), "Spin did not suppress notice descendant");
    f.services.components().set_branch_active(hidden_owner, false);
    sync(f);
    check(!hidden_input.focus(), "hidden ancestor notice accepted focus");
    f.services.set_input_blocked(foreign_owner, true);
    check(!foreign.focus(), "foreign block setup failed");
    check(under_spin.clear() && under_hidden.clear(), "suppressed clear failed");
    check(!foreign.focus(), "notice clear removed foreign owner block");
    check(normal.clear(), "normal clear failed");
    f.services.set_input_blocked(foreign_owner, false);
    sync(f);
    check(foreign.focus(), "notice cleanup left unrelated blocking state");
}

void popup() {
    Fixture f;
    NotificationRef notices;
    SelectRef select_ref;
    std::vector<SelectOption> options;
    for (int i = 0; i < 20; ++i) {
        options.push_back({String::from_utf8(std::to_string(i)).value(), String{u8"Option"}});
    }
    f.services.mount(Content{[&] { NotificationHolder(NotificationHolderProps{}.ref(notices)); }});
    sync(f);
    check(notices.open(u8"select", NotificationProps{}.duration(0),
                       {.description =
                            NotificationDescription{[&] { Select(SelectProps{}.options(options).ref(select_ref)); }}}),
          "popup notice failed");
    sync(f);
    const auto select = f.services.select().mounted()[0];
    click(f, f.services.select().snapshot(select).selector);
    sync(f);
    auto popup = f.services.select().snapshot(select);
    check(popup.visible, "child Select popup did not open");
    const ScrollInputEvent wheel{0, -1, popup.popup.x + 10, popup.popup.y + popup.popup.height / 2};
    check(f.services.dispatch_scroll(wheel), "child popup wheel fell through");
    sync(f);
    check(f.services.select().snapshot(select).scroll > 0, "notice swallowed child popup wheel");
    f.services.focus().dispatch({Key::escape, KeyAction::down});
    sync(f);
    check(!f.services.select().snapshot(select).visible &&
              !f.services.notices().entries(f.services.notices().holders()[0]).empty(),
          "notice swallowed child popup Escape");
    const auto id = entry(f);
    const auto r = bounds(f, id);
    check(f.services.dispatch_scroll({0, -1, r.x + 2, r.y + 2}), "notice background wheel leaked");
    check(!f.services.dispatch_scroll({0, -1, 1, 500}), "notice swallowed unrelated background wheel");
    // A later topmost sibling should receive the hit; notice's fallback is ownership based.
    runtime::ComponentId cover;
    input::InteractionId cover_hit;
    f.services.append_slot(
        f.services.notices().holders()[0], Content{[&] {
            auto& build = runtime::require_component_build_context();
            cover = build.mount_component<int>(0);
            const auto node = build.root(cover);
            f.layout.set_layout(node, layout::LeafLayout{{r.width, r.height}});
            f.services.components().set_window_layer(cover, 3000);
            cover_hit = f.services.interactions().create({cover, node, {}, true, false, {}, false, false});
            const auto fragment =
                build.register_scene_fragment(cover, runtime::SceneFragmentPlacement::before_children);
            graphics::QuadInstance q;
            q.opacity = 0;
            const auto surface = f.services.surfaces().create(cover, node, fragment, cover_hit, std::span{&q, 1});
            build.on_resource_cleanup(cover, [&, surface, node] {
                f.services.interactions().remove(cover_hit);
                f.services.surfaces().destroy(surface);
                f.layout.remove_layout(node);
            });
        }});
    static_cast<void>(
        f.layout.measure_child(f.services.components().root(cover), layout::Constraints::fixed(r.width, r.height)));
    f.layout.place_child(f.services.components().root(cover), r);
    sync(f);
    check(!f.services.notices().consume_scroll_boundary({0, -1, r.x + 2, r.y + 2}),
          "covered notice swallowed sibling wheel");
}

void clipping() {
    Fixture f;
    MessageRef messages;
    NotificationRef notices;
    f.services.mount(Content{[&] {
        MessageHolder(MessageHolderProps{}.ref(messages));
        NotificationHolder(NotificationHolderProps{}.ref(notices));
    }});
    sync(f);
    check(messages.open(u8"message", MessageProps{}.content(u8"Long message inside finite viewport").duration(0)),
          "message failed");
    const std::array<NotificationPlacement, 4> placements{
        NotificationPlacement::TopLeft, NotificationPlacement::TopRight, NotificationPlacement::BottomLeft,
        NotificationPlacement::BottomRight};
    for (unsigned i = 0; i < 4; ++i) {
        check(
            notices.open(
                String::from_utf8(std::to_string(i)).value(),
                NotificationProps{}.title(u8"Title").description(u8"Description").placement(placements[i]).duration(0)),
            "corner failed");
    }
    check(notices.open(u8"overflow", NotificationProps{}.title(u8"Overflow stack").duration(0)),
          "overflow entry failed");
    sync(f, 900, 700);
    for (unsigned i = 0; i < 4; ++i) {
        const auto r = bounds(f, entry(f, 1, i));
        check((i % 2 == 0 ? r.x == 24 : r.x + r.width == 876) && (i < 2 ? r.y == 24 : r.y + r.height == 676),
              "Notification corner placement wrong");
    }
    for (const auto size : {runtime::Size{180, 90}, runtime::Size{20, 10}, runtime::Size{1, 1}}) {
        sync(f, size.width, size.height);
        for (std::size_t holder = 0; holder < 2; ++holder) {
            for (const auto& item : f.services.notices().entries(f.services.notices().holders()[holder])) {
                const auto r = bounds(f, item.id);
                check(std::isfinite(r.x) && std::isfinite(r.y) && r.width >= 0 && r.height >= 0 && r.x >= 0 &&
                          r.y >= 0 && r.x + r.width <= size.width + .01F && r.y + r.height <= size.height + .01F,
                      "notice escaped finite viewport");
            }
        }
        for (const auto& hit : f.services.hit_test().records()) {
            check(hit.effective_clip.x >= 0 && hit.effective_clip.y >= 0 &&
                      hit.effective_clip.x + hit.effective_clip.width <= size.width + .01F &&
                      hit.effective_clip.y + hit.effective_clip.height <= size.height + .01F,
                  "notice child hit escaped clipping");
        }
    }
}

// docs/message-notification.md compiles this same minimal event-driven flow.
void example() {
    Fixture f;
    f.services.mount(Content{[] { FeedbackControls(); }});
    sync(f);
    click(f, f.buttons.mounted_buttons()[0].component);
    sync(f);
    check(f.services.notices().entries(f.services.notices().holders()[0]).size() == 1 &&
              f.services.notices().entries(f.services.notices().holders()[1]).size() == 1,
          "example event did not open notices");
    const auto buttons = f.buttons.mounted_buttons();
    click(f, buttons.back().component);
    sync(f);
    check(f.services.notices().entries(f.services.notices().holders()[1]).empty(),
          "example action did not close notice");
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        const std::string_view name = argc > 1 ? argv[1] : "event";
        if (name == "event") {
            event();
        } else if (name == "retention") {
            retention();
        } else if (name == "clock") {
            clock_test();
        } else if (name == "lifecycle") {
            lifecycle();
        } else if (name == "boundaries") {
            boundaries();
        } else if (name == "popup") {
            popup();
        } else if (name == "clipping") {
            clipping();
        } else if (name == "example") {
            example();
        } else {
            throw std::runtime_error("unknown notice test");
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
