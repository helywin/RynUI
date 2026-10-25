#include <ryn/rynui.hpp>

void FeedbackControls() {
    ryn::MessageRef messages;
    ryn::NotificationRef notifications;
    ryn::MessageHolder(ryn::MessageHolderProps{}.ref(messages));
    ryn::NotificationHolder(ryn::NotificationHolderProps{}.ref(notifications));
    ryn::Button(ryn::ButtonProps{}.onClick([messages, notifications] {
        static_cast<void>(
            messages.open(u8"save", ryn::MessageProps{}.content(u8"Saved").status(ryn::NoticeStatus::Success)));
        static_cast<void>(notifications.open(
            u8"job", ryn::NotificationProps{}.title(u8"Export").description(u8"Ready").duration(0),
            {.actions = ryn::NotificationActions{[notifications] {
                 ryn::Button(
                     ryn::ButtonProps{}.onClick([notifications] { static_cast<void>(notifications.close(u8"job")); }),
                     [] { ryn::Text(u8"Dismiss"); });
             }}}));
    }),
                [] { ryn::Text(u8"Save"); });
}
