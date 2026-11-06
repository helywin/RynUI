#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/notice.hpp>

namespace ryn {
namespace detail {
class AppComponentHost;
struct AppRefState;
} // namespace detail

// Capture useApp() while mounting, then keep the ref in event callbacks.
// The ref defines its target window; UI requests run on that window's owner thread.
class AppRef final {
public:
    AppRef();
    AppRef(const AppRef&);
    AppRef& operator=(const AppRef&);
    AppRef(AppRef&&) noexcept = default;
    AppRef& operator=(AppRef&&) noexcept = default;
    [[nodiscard]] bool bound() const;
    [[nodiscard]] MessageRef message() const;
    [[nodiscard]] NotificationRef notification() const;

private:
    friend class AppProps;
    friend class detail::AppComponentHost;
    friend AppRef useApp();
    std::shared_ptr<detail::AppRefState> state_;
};

class AppProps final {
public:
    AppProps& ref(const AppRef& value) {
        reference_ = value.state_;
        return *this;
    }

    AppProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

    AppProps& messageMaxCount(Prop<std::size_t> value) {
        message_max_count_ = std::move(value);
        return *this;
    }

    AppProps& notificationMaxCount(Prop<std::size_t> value) {
        notification_max_count_ = std::move(value);
        return *this;
    }

    AppProps& notificationStack(Prop<NoticeStack> value) {
        notification_stack_ = std::move(value);
        return *this;
    }

private:
    friend class detail::AppComponentHost;
    std::shared_ptr<detail::AppRefState> reference_;
    LayoutStyle layout_;
    Prop<std::size_t> message_max_count_{std::size_t{8}};
    Prop<std::size_t> notification_max_count_{std::size_t{8}};
    Prop<NoticeStack> notification_stack_{NoticeStack{}};
};

struct AppContentSlot final {};

using AppContent = SlotContent<AppContentSlot>;

void App(AppProps props, AppContent content);
[[nodiscard]] AppRef useApp();
} // namespace ryn
