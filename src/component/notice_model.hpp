#pragma once
#include "animation/time.hpp"
#include "runtime/component_host.hpp"
#include <ryn/notice.hpp>

namespace ryn::detail {
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
    void reset(animation::AnimationTime, double);
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
    std::function<bool(String, NoticeOptions, NoticeContent)> open;
    std::function<bool(String)> close;
    std::function<bool()> clear;
};

struct NoticeRefState final {
    std::weak_ptr<NoticeBinding> binding;
};
} // namespace ryn::detail
