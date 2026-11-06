#pragma once
#include "animation/time.hpp"
#include "runtime/component_host.hpp"
#include <ryn/notice.hpp>
#include <thread>

namespace ryn::detail {
struct NoticeContent;

struct NoticeEntry final {
    String key;
    runtime::ComponentId id;
};

class NoticeModel final {
public:
    std::vector<NoticeEntry> entries;
    [[nodiscard]] runtime::ComponentId find(const String&) const;
    void add(String, runtime::ComponentId);
    void erase(runtime::ComponentId);
};

struct NoticeTimer final {
    double seconds{};
    std::optional<animation::AnimationTime> deadline;
    animation::AnimationDuration remaining;
    bool paused{};
    void reset(animation::AnimationTime, double);
    void pause(animation::AnimationTime);
    void resume(animation::AnimationTime);
    [[nodiscard]] float progress(animation::AnimationTime) const;
};

struct NoticeCompletion final {
    std::promise<NoticeCloseResult> promise;
    std::shared_future<NoticeCloseResult> future{promise.get_future().share()};
    std::function<bool()> close;
    std::function<bool(NoticeOptions, NoticeContent)> update;
    std::thread::id thread{std::this_thread::get_id()};
    bool message{};
    bool settled{};
    void settle(NoticeCloseResult value) noexcept;
};

void validate_notice_status(NoticeStatus);
void validate_notice_placement(NotificationPlacement);

struct NoticeContent final {
    std::optional<Content> title;
    std::optional<Content> description;
    std::optional<Content> actions;
};

struct NoticeBinding final {
    bool alive{true};
    std::thread::id thread{std::this_thread::get_id()};
    std::function<bool(String, NoticeOptions, NoticeContent, std::optional<runtime::CapturedComponentContext>)> open;
    std::function<bool(String)> close;
    std::function<bool()> clear;
};

struct NoticeRefState final {
    std::weak_ptr<NoticeBinding> binding;
    std::optional<runtime::CapturedComponentContext> context;
    std::function<bool()> live;
};
} // namespace ryn::detail
