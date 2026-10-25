#include "component/notice_model.hpp"
#include <algorithm>
#include <cmath>

namespace ryn::detail {
runtime::ComponentId NoticeModel::find(const String& key) const {
    const auto it = std::ranges::find(entries, key, &NoticeEntry::key);
    return it == entries.end() ? runtime::ComponentId{} : it->id;
}

void NoticeModel::add(String key, runtime::ComponentId id) {
    if (key.empty() || find(key).valid() || !id.valid()) {
        throw std::invalid_argument("Notice key or identity is invalid");
    }
    entries.push_back({std::move(key), id});
}

void NoticeModel::erase(runtime::ComponentId id) {
    std::erase_if(entries, [id](const auto& item) { return item.id == id; });
}

void NoticeTimer::reset(animation::AnimationTime now, double value) {
    if (!std::isfinite(value) || value < 0 || value > 315360000) {
        throw std::invalid_argument("Notice duration must be finite, nonnegative, and at most ten years");
    }
    seconds = value;
    deadline = value == 0 ? std::optional<animation::AnimationTime>{}
                          : now + animation::AnimationDuration::milliseconds(value * 1000);
}

void validate_notice_status(NoticeStatus value) {
    if (value < NoticeStatus::Success || value > NoticeStatus::Loading) {
        throw std::invalid_argument("Invalid Notice status");
    }
}

void validate_notice_placement(NotificationPlacement value) {
    if (value < NotificationPlacement::TopLeft || value > NotificationPlacement::BottomRight) {
        throw std::invalid_argument("Invalid Notification placement");
    }
}
} // namespace ryn::detail

namespace ryn {
namespace {
std::shared_ptr<detail::NoticeBinding> binding(const std::shared_ptr<detail::NoticeRefState>& state) {
    auto result = state->binding.lock();
    return result && result->alive ? result : nullptr;
}

template <typename T> std::optional<Content> content(const std::optional<T>& slot) {
    if (!slot) {
        return {};
    }
    return Content{detail::SlotContentAccess::function(*slot)};
}
} // namespace

MessageRef::MessageRef() : state_(std::make_shared<detail::NoticeRefState>()) {}

NotificationRef::NotificationRef() : state_(std::make_shared<detail::NoticeRefState>()) {}

bool MessageRef::bound() const {
    return bool(binding(state_));
}

bool NotificationRef::bound() const {
    return bool(binding(state_));
}

bool MessageRef::open(String key, MessageProps props, MessageSlots slots) const {
    const auto target = binding(state_);
    return target && target->open(std::move(key), std::move(props.options_), {content(slots.content), {}, {}});
}

bool NotificationRef::open(String key, NotificationProps props, NotificationSlots slots) const {
    const auto target = binding(state_);
    return target && target->open(std::move(key), std::move(props.options_),
                                  {content(slots.title), content(slots.description), content(slots.actions)});
}

bool MessageRef::close(String key) const {
    const auto target = binding(state_);
    return target && target->close(std::move(key));
}

bool NotificationRef::close(String key) const {
    const auto target = binding(state_);
    return target && target->close(std::move(key));
}

bool MessageRef::clear() const {
    const auto target = binding(state_);
    return target && target->clear();
}

bool NotificationRef::clear() const {
    const auto target = binding(state_);
    return target && target->clear();
}
} // namespace ryn
