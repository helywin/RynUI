#include "component/feedback_component.hpp"
#include "component/modal_component.hpp"
#include "component/notice_component.hpp"
#include "support/input_fixture.hpp"
#include <ryn/alert.hpp>
#include <ryn/drawer.hpp>
#include <ryn/notification.hpp>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void sync(Fixture& f) {
    for (int frame = 0; frame < 3; ++frame) {
        check(f.services.layout_and_synchronize({720, 600}, {0, 0, 720, 600}), "feedback completion layout failed");
    }
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

void pointer(Fixture& f, PointerAction action, float x, float y) {
    f.services.pointer().dispatch({PointerIdentity::mouse(), action,
                                   action == PointerAction::move ? PointerButton::none : PointerButton::primary, x, y});
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    pointer(f, PointerAction::down, r.x + r.width / 2, r.y + r.height / 2);
    pointer(f, PointerAction::up, r.x + r.width / 2, r.y + r.height / 2);
}

void tick(Fixture& f, std::int64_t microseconds) {
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(microseconds)));
    sync(f);
}

NoticeCloseResult result(const NoticeHandle& handle) {
    check(handle.closed().wait_for(std::chrono::seconds{0}) == std::future_status::ready,
          "close result unexpectedly remained pending");
    return handle.closed().get();
}

bool painted(Fixture& f, Color color, runtime::Point point) {
    for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
        for (auto index = command.first_instance; index < command.first_instance + command.instance_count; ++index) {
            if (command.kind == graphics::SceneDrawKind::quad) {
                const auto& q = f.services.surfaces().instances().at(index);
                const graphics::LogicalRoundedRect shape{
                    {q.bounds[0] + q.translation[0], q.bounds[1] + q.translation[1], q.bounds[2], q.bounds[3]},
                    q.corner_radius};
                if (q.color == std::array{color.red(), color.green(), color.blue(), color.alpha()} && q.opacity > .5F &&
                    graphics::rounded_rect_signed_distance(point, shape) < 0) {
                    return true;
                }
            } else if (command.kind == graphics::SceneDrawKind::rounded_effect) {
                const auto& effect = f.services.rounded_effects().packed_instances()[index];
                if (effect.geometry.ancestor_clip) {
                    const auto clip = effect.geometry.ancestor_clip->bounds;
                    if (point.x < clip.x || point.x >= clip.x + clip.width || point.y < clip.y ||
                        point.y >= clip.y + clip.height) {
                        continue;
                    }
                }
                if (effect.material.color == color && effect.material.visible && effect.material.opacity > .5F &&
                    graphics::rounded_effect_coverage(point, effect) > .5F) {
                    return true;
                }
            }
        }
    }
    return false;
}

runtime::ComponentId notice(Fixture& f, std::size_t index = 0) {
    return f.services.notices().entries(f.services.notices().holders().front()).at(index).id;
}

void alert() {
    Fixture f;
    f.buttons.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> visible{true};
    Signal<bool> banner{true};
    Signal<AlertMarquee> marquee{AlertMarquee{}};
    int requests{};
    int finished{};
    int mounts{};
    InputRef retained;
    f.services.mount(Content{[&] {
        Alert(
            AlertProps{}
                .banner(banner)
                .visible(visible)
                .motion()
                .closable()
                .marquee(marquee)
                .message(u8"This long banner scrolls within its allocated body without remounting the retained action")
                .onClose([&] { ++requests; })
                .afterClose([&] { ++finished; })
                .layout(LayoutStyle{}.width(dp(280))),
            {.action = AlertAction{[&] {
                 ++mounts;
                 Input(InputProps{}.ref(retained).layout(LayoutStyle{}.width(dp(40))));
             }}});
        Button(ButtonProps{}, [] { Text(u8"Following sibling"); });
    }});
    sync(f);
    const auto id = f.services.feedback().alerts().front();
    auto snapshot = f.services.feedback().alert_snapshot(id);
    check(snapshot.banner && snapshot.status == Status::Warning && f.services.components().branch_active(snapshot.icon),
          "banner defaults were not applied");
    const auto count = f.services.components().component_count();
    const auto following = f.buttons.mounted_buttons().back().component;
    const auto original_y = bounds(f, following).y;
    tick(f, 100000);
    tick(f, 200000);
    check(f.services.feedback().alert_snapshot(id).marquee_offset > 0 && mounts == 1,
          "banner did not scroll retained content");
    pointer(f, PointerAction::move, 5, 5);
    const auto paused = f.services.feedback().alert_snapshot(id).marquee_offset;
    tick(f, 300000);
    check(f.services.feedback().alert_snapshot(id).marquee_offset == paused, "banner hover did not pause");
    click(f, snapshot.close);
    check(requests == 1 && f.services.feedback().alert_snapshot(id).visible && !finished,
          "controlled Alert committed a rejected request");
    visible.set(false);
    check(f.services.feedback().alert_snapshot(id).closing && !retained.focus(), "exit left retained controls active");
    tick(f, 400000);
    check(bounds(f, following).y < original_y, "exit did not reflow following content");
    tick(f, 1000000);
    check(!f.services.feedback().alert_snapshot(id).visible && finished == 1, "exit completion missing");
    visible.set(true);
    sync(f);
    banner.set(false);
    marquee.set(AlertMarquee{false});
    sync(f);
    check(f.services.components().component_count() == count && mounts == 1 && retained.bound(),
          "Alert update rebuilt retained content");
    check(!f.services.feedback().next_auxiliary_deadline(), "static Alert scheduled polling");
    Fixture automatic;
    automatic.services.mount(Content{[] { Alert(AlertProps{}.message(u8"Uncontrolled").closable()); }});
    sync(automatic);
    const auto automatic_id = automatic.services.feedback().alerts().front();
    click(automatic, automatic.services.feedback().alert_snapshot(automatic_id).close);
    sync(automatic);
    check(!automatic.services.feedback().alert_snapshot(automatic_id).visible,
          "uncontrolled Alert did not close itself");
}

void notice_timer() {
    Fixture f;
    NotificationRef notifications;
    f.services.mount(Content{[&] { NotificationHolder(NotificationHolderProps{}.ref(notifications)); }});
    auto handle = notifications.openAsync(u8"timed", NotificationProps{}.title(u8"Timed").duration(4).showProgress());
    sync(f);
    const auto id = notice(f);
    tick(f, 1000000);
    const auto r = bounds(f, id);
    const auto fraction = f.services.notices().snapshot(id).progress;
    check(std::abs(fraction - .75F) < .001F, "notice progress did not use remaining duration");
    const auto color = f.services.components().theme_scope(id)->snapshot().notification().colors.info;
    check(painted(f, color, {r.x + r.width * .2F, r.y + r.height - 1}) &&
              !painted(f, color, {r.x + r.width * .9F, r.y + r.height - 1}),
          "published notice progress fill does not match remaining duration");
    pointer(f, PointerAction::move, r.x + 2, r.y + 2);
    check(!f.services.notices().next_auxiliary_deadline(), "paused progress still scheduled frames");
    tick(f, 5000000);
    check(handle.closed().wait_for(std::chrono::seconds{0}) == std::future_status::timeout,
          "hover failed to pause expiry");
    pointer(f, PointerAction::move, 0, 599);
    tick(f, 7999999);
    check(f.services.components().contains(id), "resume lost remaining duration");
    tick(f, 8000000);
    check(result(handle) == NoticeCloseResult::Closed && !handle.close() && !f.services.next_frame_deadline(),
          "expiry did not settle its future or idle deadline");
}

void notice_lifecycle() {
    Fixture f;
    Fixture other;
    NotificationRef notifications;
    NotificationRef isolated;
    int closed{};
    f.services.mount(Content{[&] { NotificationHolder(NotificationHolderProps{}.ref(notifications)); }});
    other.services.mount(Content{[&] { NotificationHolder(NotificationHolderProps{}.ref(isolated)); }});
    auto old = notifications.openAsync(u8"same", NotificationProps{}.title(u8"Old").duration(0).onClose([&] {
        ++closed;
        check(notifications.open(u8"same", NotificationProps{}.title(u8"Replacement").duration(0)),
              "callback reopen failed");
    }));
    check(isolated.open(u8"same", NotificationProps{}.title(u8"Other window").duration(0)), "isolated open failed");
    check(old.close() && result(old) == NoticeCloseResult::Closed && !old.close() && closed == 1,
          "old close handle reached a replacement generation");
    check(other.services.notices().entries(other.services.notices().holders().front()).size() == 1,
          "notice crossed window boundary");
    auto cancelled = notifications.openAsync(u8"pending", NotificationProps{}.duration(0).onClose([&] { ++closed; }));
    f.services.destroy(f.services.notices().holders().front());
    check(result(cancelled) == NoticeCloseResult::Cancelled && !cancelled.close() && closed == 1,
          "holder destruction failed to cancel pending result");
    auto unbound = notifications.openAsync(u8"no-holder", NotificationProps{});
    check(result(unbound) == NoticeCloseResult::Cancelled, "unbound service did not cancel immediately");
    Fixture rebound;
    rebound.services.mount(Content{[&] { NotificationHolder(NotificationHolderProps{}.ref(notifications)); }});
    auto failed = notifications.openAsync(u8"valid", NotificationProps{}.duration(0));
    try {
        static_cast<void>(notifications.open(u8"valid", NotificationProps{}.duration(-1)));
        check(false, "invalid update did not throw");
    } catch (const std::invalid_argument&) {
    }
    check(result(failed) == NoticeCloseResult::Failed && !failed.close(), "failed update left a live partial entry");
}

void notice_stack() {
    Fixture f;
    f.buttons.set_motion_preference(animation::MotionPreference::normal);
    NotificationRef notifications;
    Signal<NoticeStack> stack{NoticeStack{true, 3}};
    f.services.mount(Content{[&] { NotificationHolder(NotificationHolderProps{}.ref(notifications).stack(stack)); }});
    for (int i = 0; i < 5; ++i) {
        check(notifications.open(String::from_utf8(std::to_string(i)).value(),
                                 NotificationProps{}.title(u8"Stack entry").duration(0)),
              "stack open failed");
    }
    sync(f);
    const auto first = notice(f);
    const auto last = notice(f, 4);
    auto first_snapshot = f.services.notices().snapshot(first);
    check(first_snapshot.collapsed && !f.services.components().branch_active(first_snapshot.title) &&
              !f.services.notices().snapshot(last).collapsed,
          "collapsed stack left old content interactive");
    const auto last_bounds = bounds(f, last);
    pointer(f, PointerAction::move, last_bounds.x + 2, last_bounds.y + 2);
    sync(f);
    check(!f.services.notices().snapshot(first).collapsed &&
              f.services.components().branch_active(first_snapshot.title),
          "hover did not expand retained entries");
    pointer(f, PointerAction::move, 0, 599);
    sync(f);
    check(f.services.notices().snapshot(first).collapsed, "stack did not collapse after leave");
    stack.set(NoticeStack{false});
    sync(f);
    check(!f.services.notices().snapshot(first).collapsed && !f.services.notices().next_auxiliary_deadline(),
          "static expanded stack scheduled work");
    auto animated = notifications.openAsync(u8"animated", NotificationProps{}.title(u8"Animated").duration(0).motion());
    sync(f);
    tick(f, 1000000);
    check(animated.close(), "animated close rejected");
    check(animated.closed().wait_for(std::chrono::seconds{0}) == std::future_status::timeout,
          "close future settled before exit");
    tick(f, 2000000);
    check(result(animated) == NoticeCloseResult::Closed, "animated close did not settle");
    auto reduced = notifications.openAsync(u8"reduced", NotificationProps{}.duration(0).motion());
    check(reduced.close(), "reduced-motion close rejected");
    f.buttons.set_motion_preference(animation::MotionPreference::reduced);
    sync(f);
    check(result(reduced) == NoticeCloseResult::Closed, "reduced motion left close pending");
    check(notifications.clear() && !f.services.notices().next_auxiliary_deadline(), "clear leaked animation deadline");
}

void drawer() {
    Fixture f;
    Signal<bool> open{false};
    Signal<bool> loading{false};
    Signal<bool> rtl{false};
    Signal<float> width{360};
    InputRef editor;
    int mounts{};
    int extra_mounts{};
    std::vector<float> requests;
    f.services.mount(Content{[&] {
        Drawer(DrawerProps{}
                   .open(open)
                   .title(u8"Drawer")
                   .width(width)
                   .lazy()
                   .destroyOnHidden()
                   .loading(loading)
                   .rtl(rtl)
                   .resizable(DrawerResize{true, 200, 500})
                   .onResize([&](float value) { requests.push_back(value); }),
               DrawerContent{[&] {
                   ++mounts;
                   Input(InputProps{}.ref(editor).defaultValue(u8"Draft"));
               }},
               {.extra = DrawerExtra{[&] {
                    ++extra_mounts;
                    Button(ButtonProps{}, [] { Text(u8"Extra"); });
                }}});
    }});
    sync(f);
    const auto id = f.services.modal().mounted().front();
    check(mounts == 0 && !editor.bound(), "lazy Drawer mounted closed content");
    open.set(true);
    sync(f);
    auto panel = f.services.modal().snapshot(id);
    check(mounts == 1 && extra_mounts == 1 && editor.focus(), "Drawer did not mount or focus body");
    const auto body_id = panel.body;
    const auto session = f.inputs.sessions().active();
    loading.set(true);
    sync(f);
    panel = f.services.modal().snapshot(id);
    check(panel.loading && !editor.focus() && f.services.components().branch_active(panel.placeholder) &&
              !f.services.components().branch_active(panel.body) && !f.inputs.sessions().active().valid(),
          "Drawer loading failed to replace body with a noninteractive Skeleton");
    check(!bool(f.inputs.dispatch(TextCommitted{String{u8"stale"}, session})),
          "Drawer loading accepted a stale editor session");
    loading.set(false);
    rtl.set(true);
    sync(f);
    check(mounts == 1 && f.services.modal().snapshot(id).body == body_id && editor.bound(),
          "loading or RTL remounted body");
    check(bounds(f, panel.extra).x < bounds(f, panel.title).x && bounds(f, panel.close).x > bounds(f, panel.title).x,
          "Drawer RTL did not mirror header regions");
    const auto handle = bounds(f, panel.resize_handle);
    pointer(f, PointerAction::down, handle.x + 2, handle.y + 20);
    pointer(f, PointerAction::move, handle.x - 50, handle.y + 20);
    pointer(f, PointerAction::up, handle.x - 50, handle.y + 20);
    sync(f);
    check(!requests.empty() && requests.back() > 360 && f.services.modal().snapshot(id).bounds.width == 360,
          "Drawer resize committed a controlled request");
    width.set(requests.back());
    sync(f);
    check(f.services.modal().snapshot(id).bounds.width == requests.back(), "Drawer rejected accepted width authority");
    open.set(false);
    sync(f);
    check(!editor.bound() && !f.services.modal().snapshot(id).body_mounted, "destroyOnHidden retained body resources");
    open.set(true);
    sync(f);
    check(mounts == 2 && extra_mounts == 1 && editor.bound(), "Drawer did not recreate only destroyed content");
    Fixture failing;
    Signal<bool> fail_open{false};
    failing.services.mount(Content{[&] {
        Button(ButtonProps{}, [] { Text(u8"Surviving sibling"); });
        Drawer(DrawerProps{}.open(fail_open).lazy(), DrawerContent{[] {
                   Input(InputProps{}.defaultValue(u8"Partially mounted"));
                   throw std::runtime_error("application body factory failed");
               }});
    }});
    sync(failing);
    const auto failed_owner = failing.services.modal().mounted().front();
    bool threw{};
    try {
        fail_open.set(true);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && !failing.services.components().contains(failed_owner) && failing.inputs.mounted_inputs().empty() &&
              !failing.buttons.mounted_buttons().empty(),
          "failed lazy Drawer left a partial editor subtree or retired an unrelated sibling");
}

void drawer_push() {
    Fixture f;
    Signal<bool> nested{false};
    f.services.mount(Content{[&] {
        Drawer(DrawerProps{}.open().title(u8"Parent").width(400), DrawerContent{[&] {
                   Drawer(DrawerProps{}.open(nested).width(240).push(DrawerPush{true, 80}),
                          DrawerContent{[] { Text(u8"Child"); }});
               }});
    }});
    sync(f);
    const auto parent = f.services.modal().mounted()[0];
    const auto original = f.services.modal().snapshot(parent).bounds;
    nested.set(true);
    sync(f);
    check(f.services.modal().snapshot(parent).bounds.x == original.x - 80, "nested Drawer did not push parent");
    nested.set(false);
    sync(f);
    check(f.services.modal().snapshot(parent).bounds == original, "nested close did not restore parent geometry");
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "alert";
        if (name == "alert") {
            alert();
        } else if (name == "notice_timer") {
            notice_timer();
        } else if (name == "notice_lifecycle") {
            notice_lifecycle();
        } else if (name == "notice_stack") {
            notice_stack();
        } else if (name == "drawer") {
            drawer();
        } else if (name == "drawer_push") {
            drawer_push();
        } else {
            throw std::runtime_error("unknown feedback completion case");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
