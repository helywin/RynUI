#include "gallery_notice_acceptance.hpp"
#include "component/notice_component.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <type_traits>

namespace rynui::example {
namespace {
template <bool Message>
void loading(ExistingDataEntryActions a, const std::function<void(ryn::animation::AnimationDuration)>& advance,
             const std::function<void(std::string_view)>& capture) {
    using namespace ryn;
    std::conditional_t<Message, MessageRef, NotificationRef> ref;
    Signal<NoticeStatus> status{NoticeStatus::Loading};
    InputRef input;
    int mounts{};
    auto& services = a.app.services();
    const auto preference = services.motion_preference();
    services.set_motion_preference(animation::MotionPreference::normal);
    // An isolated real holder keeps this check independent of the sample's props, locale and status cycle.
    services.append_slot(a.pages.snapshot().page, Content{[&] {
                             if constexpr (Message) {
                                 MessageHolder(MessageHolderProps{}.ref(ref));
                             } else {
                                 NotificationHolder(NotificationHolderProps{}.ref(ref));
                             }
                         }});
    const auto holder = services.notices().holders().back();
    const auto rich = [&] {
        ++mounts;
        Input(InputProps{}.ref(input).defaultValue(u8"native retained notice draft"));
    };
    const auto handle = [&] {
        if constexpr (Message) {
            return ref.openAsync(u8"native-loading", MessageProps{}.status(status).duration(0),
                                 {.content = MessageContent{rich}});
        } else {
            return ref.openAsync(u8"native-loading", NotificationProps{}.title(u8"Loading").status(status).duration(0),
                                 {.description = NotificationDescription{rich}});
        }
    }();
    a.render();
    const auto entry = services.notices().entries(holder).front().id;
    const auto original = services.notices().snapshot(entry);
    const auto icon = [&] {
        for (const auto& text : services.text().mounted_texts()) {
            if (a.app.components().parent(text.component) == original.icon) {
                return text.component;
            }
        }
        throw std::runtime_error("hidden notice default Icon missing");
    }();
    const auto editor = a.inputs.mounted_inputs().back();
    a.check(input.focus({InputFocusCursor::All}), "hidden notice editor could not focus");
    const auto session = a.inputs.sessions().active();
    a.render();
    const auto& icon_node = a.app.nodes().require(a.app.components().root(icon));
    const auto scale = services.display_scale();
    const auto left =
        std::max(0, static_cast<int>(std::floor((icon_node.bounds.x + icon_node.translation.x) * scale)) - 2);
    const auto top =
        std::max(0, static_cast<int>(std::floor((icon_node.bounds.y + icon_node.translation.y) * scale)) - 2);
    const auto right =
        static_cast<int>(std::ceil((icon_node.bounds.x + icon_node.translation.x + icon_node.bounds.width) * scale)) +
        2;
    const auto bottom =
        static_cast<int>(std::ceil((icon_node.bounds.y + icon_node.translation.y + icon_node.bounds.height) * scale)) +
        2;
    std::cout << "notice_loading_icon_rect=" << left << ',' << top << ',' << right << ',' << bottom << '\n';
    const auto angle = [&] {
        return services.text().icon_snapshot(icon).angle_degrees;
    };
    const auto first = angle();
    capture("loading-first");
    advance(animation::AnimationDuration::milliseconds(250));
    a.check(std::abs(angle() - first) > 1, "hidden Loading Icon did not rotate between GPU frames");
    capture("loading-second");
    status.set(NoticeStatus::Info);
    a.render();
    advance(animation::AnimationDuration::milliseconds(250));
    a.check(angle() == 0 && !services.text().icon_snapshot(icon).spin, "hidden Info status retained Loading motion");
    capture("info-stopped");
    status.set(NoticeStatus::Loading);
    a.render();
    advance(animation::AnimationDuration::milliseconds(250));
    const auto current = services.notices().snapshot(entry);
    a.check(angle() > 0 && current.icon == original.icon && current.title == original.title &&
                current.description == original.description && mounts == 1 && input.bound() &&
                a.inputs.editors().find(editor.editor) && a.inputs.sessions().active() == session &&
                a.inputs.display_snapshot(editor.component).text == "native retained notice draft" &&
                handle.closed().wait_for(std::chrono::seconds{0}) == std::future_status::timeout,
            "hidden Loading restart lost retained entry/editor/future");
    capture("loading-restarted");
    a.check(handle.close() && handle.closed().wait_for(std::chrono::seconds{0}) == std::future_status::ready &&
                handle.closed().get() == NoticeCloseResult::Closed && !input.bound() &&
                !a.app.components().contains(icon),
            "hidden Loading close retained its Icon/editor/future");
    a.check(services.destroy(holder), "hidden notice holder retirement failed");
    services.set_motion_preference(preference);
    a.render();
    std::cout << (Message ? "message" : "notification")
              << "_loading_native=passed rotated_gpu_frames=true ordinary_stopped=true retained_editor=true "
                 "closed_future=true\n";
}
} // namespace

void accept_notice_loading(const GalleryPageRoute& route, ExistingDataEntryActions a,
                           const std::function<void(ryn::animation::AnimationDuration)>& advance,
                           const std::function<void(std::string_view)>& capture) {
    if (route.component_identity == "ant.component.message") {
        loading<true>(a, advance, capture);
    } else if (route.component_identity == "ant.component.notification") {
        loading<false>(a, advance, capture);
    }
}
} // namespace rynui::example
