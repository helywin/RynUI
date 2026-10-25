#include "component/notice_model.hpp"
#include <ryn/rynui.hpp>
#include <iostream>
#include <limits>
#include <type_traits>

namespace {
using namespace ryn;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <typename F> void rejects(F f) {
    bool threw = false;
    try {
        f();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "invalid notice model value accepted");
}
} // namespace

int main() {
    try {
        static_assert(!std::is_convertible_v<MessageContent, NotificationTitle>);
        static_assert(!std::is_convertible_v<MessageProps, NotificationProps>);
        MessageRef message;
        NotificationRef notification;
        check(!message.bound() && !message.open(u8"a", MessageProps{}.content(u8"hello")) && !message.close(u8"a") &&
                  !message.clear(),
              "unbound MessageRef ran");
        check(!notification.bound() && !notification.open(u8"a", NotificationProps{}.title(u8"title")) &&
                  !notification.clear(),
              "unbound NotificationRef ran");
        detail::NoticeModel model;
        const runtime::ComponentId old{1, 1};
        const runtime::ComponentId fresh{1, 2};
        model.add(u8"key", old);
        check(model.find(u8"key") == old, "key lookup failed");
        rejects([&] { model.add(u8"key", fresh); });
        rejects([&] { model.add(u8"", fresh); });
        model.erase(fresh);
        check(model.find(u8"key") == old, "generation erase used index only");
        model.erase(old);
        model.add(u8"key", fresh);
        check(model.find(u8"key") == fresh, "key reuse failed");
        detail::NoticeTimer timer;
        timer.reset(animation::AnimationTime::microseconds(123), 0);
        check(!timer.deadline, "persistent notice has deadline");
        timer.reset(animation::AnimationTime::microseconds(123), 1.5);
        check(timer.deadline == animation::AnimationTime::microseconds(1500123), "deadline drifted");
        for (double value :
             {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), 315360001.0}) {
            rejects([&] { timer.reset({}, value); });
        }
        check(timer.deadline == animation::AnimationTime::microseconds(1500123), "invalid duration changed timer");
        rejects([] { detail::validate_notice_status(static_cast<NoticeStatus>(99)); });
        rejects([] { detail::validate_notice_placement(static_cast<NotificationPlacement>(99)); });
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
