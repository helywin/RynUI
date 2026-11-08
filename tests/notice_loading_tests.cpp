#include "component/notice_component.hpp"
#include "support/input_fixture.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <type_traits>

namespace {
using namespace ryn;
using namespace ryn::input;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <bool Message> struct LoadingFixture final : ryn_test::input_component::Fixture {
    std::conditional_t<Message, MessageRef, NotificationRef> ref;
    Signal<NoticeStatus> status{NoticeStatus::Loading};
    Signal<NoticeStack> stack{NoticeStack{}};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<bool> custom_spin{false};
    InputRef input;
    runtime::ComponentId custom_icon;
    int mounts{};

    LoadingFixture() {
        services.set_motion_preference(animation::MotionPreference::normal);
        services.mount(Content{[&] {
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      if constexpr (Message) {
                          MessageHolder(MessageHolderProps{}.ref(ref).stack(stack));
                      } else {
                          NotificationHolder(NotificationHolderProps{}.ref(ref).stack(stack));
                      }
                  }});
        }});
    }

    NoticeHandle open(double duration = 0, bool motion = false, bool progress = false) {
        const auto rich = [&] {
            ++mounts;
            Input(InputProps{}.defaultValue(u8"original").ref(input).layout(LayoutStyle{}.width(dp(160))));
            Icon(IconProps{}.name(IconName::LoadingOutlined).rotate(17).spin(custom_spin));
            custom_icon = services.text().mounted_texts().back().component;
        };
        if constexpr (Message) {
            return ref.openAsync(u8"loading",
                                 MessageProps{}.status(status).duration(duration).motion(motion).showProgress(progress),
                                 {.content = MessageContent{rich}});
        } else {
            return ref.openAsync(u8"loading",
                                 NotificationProps{}
                                     .title(u8"Loading")
                                     .status(status)
                                     .duration(duration)
                                     .motion(motion)
                                     .showProgress(progress),
                                 {.description = NotificationDescription{rich}});
        }
    }

    void sync() {
        for (int frame = 0; frame < 3; ++frame) {
            check(services.layout_and_synchronize({720, 600}, {0, 0, 720, 600}), "Loading layout failed");
        }
    }

    void tick(std::int64_t microseconds) {
        static_cast<void>(services.tick_animations(animation::AnimationTime::microseconds(microseconds)));
        sync();
    }

    runtime::ComponentId holder() {
        return services.notices().holders().front();
    }

    runtime::ComponentId entry() {
        return services.notices().entries(holder()).front().id;
    }

    runtime::ComponentId icon() {
        const auto owner = services.notices().snapshot(entry()).icon;
        for (const auto& text : services.text().mounted_texts()) {
            if (services.components().parent(text.component) == owner) {
                return text.component;
            }
        }
        throw std::runtime_error("default notice Icon missing");
    }

    float angle() {
        return services.text().icon_snapshot(icon()).angle_degrees;
    }

    void hover(bool value) {
        const auto r = nodes.require(services.components().root(entry())).bounds;
        services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none,
                                     value ? r.x + 2 : 0, value ? r.y + 2 : 599});
    }
};

bool pending(const NoticeHandle& handle) {
    return handle.closed().wait_for(std::chrono::seconds{0}) == std::future_status::timeout;
}

NoticeCloseResult result(const NoticeHandle& handle) {
    check(!pending(handle), "notice future is still pending");
    return handle.closed().get();
}

template <bool Message> void status_and_retention() {
    LoadingFixture<Message> f;
    const auto handle = f.open(); // duration=0 and default motion=false isolate Icon's own deadline.
    f.sync();
    const auto entry = f.entry();
    const auto original = f.services.notices().snapshot(entry);
    const auto icon = f.icon();
    const auto layer = f.services.text().icon_snapshot(icon).layers.front();
    const auto transform = [&] {
        return f.scene.glyph_scene().instances().at(f.scene.primitive(layer).instances.first).transform;
    };
    const auto initial_transform = transform();
    const auto editor = f.inputs.mounted_inputs().front();
    check(f.input.focus({InputFocusCursor::All}), "Loading editor could not focus");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"retained draft"}, f.inputs.sessions().active()})),
          "Loading editor could not edit");
    const auto session = f.inputs.sessions().active();
    const auto count = f.services.components().component_count();
    check(!f.services.notices().next_auxiliary_deadline() && f.services.animations().next_deadline().has_value(),
          "duration=0 Loading did not schedule its Icon independently of expiry/motion");
    f.tick(250000);
    check(std::abs(f.angle() - 90) < .01F && transform() != initial_transform,
          "Loading did not rotate the retained glyph by a quarter turn");
    for (auto status : {NoticeStatus::Info, NoticeStatus::Success, NoticeStatus::Warning, NoticeStatus::Error}) {
        f.status.set(status);
        f.sync();
        check(!f.services.animations().next_deadline() && !f.services.notices().next_auxiliary_deadline() &&
                  !f.services.text().icon_snapshot(icon).spin && f.angle() == 0,
              "ordinary notice status retained spinner phase or deadline");
    }
    f.tick(500000);
    check(f.angle() == 0 && pending(handle), "ordinary status moved or settled a persistent notice");
    f.status.set(NoticeStatus::Loading);
    f.sync();
    f.tick(750000);
    const auto current = f.services.notices().snapshot(entry);
    check(std::abs(f.angle() - 90) < .01F && f.icon() == icon && current.title == original.title &&
              current.description == original.description && f.entry() == entry &&
              f.services.components().component_count() == count && f.mounts == 1 && f.input.bound() &&
              f.inputs.editors().find(editor.editor) && f.inputs.sessions().active() == session &&
              f.inputs.display_snapshot(editor.component).text == "retained draft" && pending(handle),
          "Loading restart remounted retained content/editor or replaced its future");
    check(!f.services.text().icon_snapshot(f.custom_icon).spin &&
              f.services.text().icon_snapshot(f.custom_icon).angle_degrees == 17,
          "notice status took ownership of a rich-slot Icon");
    f.status.set(NoticeStatus::Info);
    f.custom_spin.set(true);
    f.sync();
    f.tick(875000);
    check(f.services.text().icon_snapshot(f.custom_icon).angle_degrees > 17 && f.angle() == 0,
          "application-controlled rich-slot Icon could not spin independently");
    check(handle.close() && result(handle) == NoticeCloseResult::Closed && !f.input.bound() &&
              !f.services.next_frame_deadline() && f.services.animations().diagnostics().targets == 0,
          "close leaked retained editors or Icon animation targets");
}

template <bool Message> void visibility_and_retirement() {
    LoadingFixture<Message> f;
    const auto handle = f.open();
    f.sync();
    const auto entry = f.entry();
    const auto icon = f.icon();
    f.tick(125000);
    check(f.angle() > 0, "visible Loading did not move");
    // Hidden entry and inactive holder exercise the same retained branch gate used by off-page owners.
    for (auto owner : {entry, f.holder()}) {
        check(f.services.components().set_branch_active(owner, false), "notice branch did not hide");
        f.sync();
        check(!f.services.next_frame_deadline() && f.angle() == 0, "hidden/off-page Loading retained motion");
        f.tick(250000);
        check(f.angle() == 0 && pending(handle), "hidden persistent Loading advanced or closed");
        check(f.services.components().set_branch_active(owner, true), "notice branch did not restore");
        f.sync();
        check(f.services.next_frame_deadline().has_value(), "restored Loading did not resume");
    }
    f.services.set_window_active(false);
    check(!f.services.next_frame_deadline() && f.angle() == 0, "inactive window retained duration=0 spin");
    f.services.set_window_active(true);
    check(f.services.next_frame_deadline().has_value(), "reactivated window did not resume spin");
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    check(!f.services.next_frame_deadline() && f.angle() == 0, "reduced motion retained spinner work");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    f.tick(375000);
    check(f.angle() > 0, "normal motion did not resume spinner");
    auto no_motion = ThemeConfig{};
    no_motion.seed.motion = false;
    f.theme.set(no_motion);
    f.sync();
    check(!f.services.next_frame_deadline() && f.angle() == 0, "Theme motion=false retained spin");
    f.theme.set(ThemeConfig{});
    f.sync();
    f.stack.set(NoticeStack{true, 1});
    if constexpr (Message) {
        check(f.ref.open(u8"top", MessageProps{}.content(u8"top").duration(0)), "Message stack open failed");
    } else {
        check(f.ref.open(u8"top", NotificationProps{}.title(u8"top").duration(0)), "Notification stack open failed");
    }
    f.sync();
    check(f.services.notices().snapshot(entry).collapsed && !f.services.components().branch_active(icon) &&
              !f.services.next_frame_deadline() && f.angle() == 0,
          "collapsed Loading retained spinner work");
    f.stack.set(NoticeStack{});
    f.sync();
    f.tick(500000);
    check(!f.services.notices().snapshot(entry).collapsed && f.angle() > 0 && f.mounts == 1,
          "expanded Loading failed to resume its retained Icon");
    check(f.services.destroy(f.holder()), "holder retirement failed");
    check(result(handle) == NoticeCloseResult::Cancelled && !f.services.components().contains(icon) &&
              !f.input.bound() && !f.services.next_frame_deadline() &&
              f.services.animations().diagnostics().targets == 0 && f.services.animations().diagnostics().scopes == 0,
          "holder retirement retained spinner resources or pending future");
}

template <bool Message> void expiry_hover_and_exit() {
    LoadingFixture<Message> f;
    const auto handle = f.open(4, true, true);
    f.sync();
    const auto entry = f.entry();
    f.tick(1000000);
    check(std::abs(f.services.notices().snapshot(entry).progress - .75F) < .001F, "Loading changed expiry progress");
    f.hover(true);
    check(!f.services.notices().next_auxiliary_deadline() && f.services.next_frame_deadline().has_value(),
          "hover paused the spinner along with expiry, or failed to pause expiry");
    f.tick(1250000);
    const auto angle = f.angle();
    f.tick(1500000);
    check(f.angle() != angle && pending(handle) &&
              std::abs(f.services.notices().snapshot(entry).progress - .75F) < .001F,
          "hover did not preserve both paused expiry and moving Loading Icon");
    f.tick(5000000);
    check(pending(handle), "hovered Loading expired");
    f.hover(false);
    // Preserve the original inactive-window expiry contract; only the Icon's frame work stops.
    f.services.set_window_active(false);
    f.sync();
    const auto expiry = animation::AnimationTime::microseconds(8000000);
    check(f.angle() == 0 && f.services.notices().next_auxiliary_deadline() == expiry &&
              f.services.next_frame_deadline() == expiry,
          "inactive timed Loading lost expiry or retained spinner frames");
    f.tick(6000000);
    check(pending(handle) && std::abs(f.services.notices().snapshot(entry).progress - .5F) < .001F,
          "inactive Loading stopped its original expiry clock");
    f.services.set_window_active(true);
    f.tick(7999999);
    check(pending(handle) && !f.services.notices().snapshot(entry).closing, "Loading expired early");
    f.tick(8000000);
    check(pending(handle) && f.services.notices().snapshot(entry).closing && !f.input.focus(),
          "expiry settled before exit motion or retained input authority");
    f.tick(9000000);
    check(result(handle) == NoticeCloseResult::Closed && !f.services.components().contains(entry) &&
              !f.services.next_frame_deadline(),
          "Loading exit failed to settle future and clean all deadlines");
}
} // namespace

int main(int argc, char** argv) {
    try {
        const auto test = argc > 1 ? std::string_view{argv[1]} : std::string_view{};
        if (test == "message_status") {
            status_and_retention<true>();
        } else if (test == "notification_status") {
            status_and_retention<false>();
        } else if (test == "message_visibility") {
            visibility_and_retirement<true>();
        } else if (test == "notification_visibility") {
            visibility_and_retirement<false>();
        } else if (test == "message_expiry") {
            expiry_hover_and_exit<true>();
        } else if (test == "notification_expiry") {
            expiry_hover_and_exit<false>();
        } else {
            throw std::invalid_argument("unknown notice Loading case");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
