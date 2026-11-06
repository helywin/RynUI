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
    remaining = animation::AnimationDuration::milliseconds(value * 1000);
    deadline = value == 0 ? std::optional<animation::AnimationTime>{}
               : paused   ? std::optional<animation::AnimationTime>{}
                          : now + remaining;
}

void NoticeTimer::pause(animation::AnimationTime now) {
    if (paused) {
        return;
    }
    if (deadline) {
        remaining = *deadline > now ? *deadline - now : animation::AnimationDuration{};
    }
    deadline.reset();
    paused = true;
}

void NoticeTimer::resume(animation::AnimationTime now) {
    if (!paused) {
        return;
    }
    paused = false;
    if (seconds > 0) {
        deadline = now + remaining;
    }
}

float NoticeTimer::progress(animation::AnimationTime now) const {
    if (seconds == 0) {
        return 0;
    }
    const auto left = paused                        ? remaining
                      : deadline && *deadline > now ? *deadline - now
                                                    : animation::AnimationDuration{};
    return std::clamp(static_cast<float>(left.count_microseconds() / (seconds * 1000000)), 0.0F, 1.0F);
}

void NoticeCompletion::settle(NoticeCloseResult value) noexcept {
    if (!settled) {
        settled = true;
        close = {};
        update = {};
        promise.set_value(value);
    }
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
    if (!state) {
        return {};
    }
    auto result = state->binding.lock();
    return result && result->thread == std::this_thread::get_id() && result->alive && (!state->live || state->live())
               ? result
               : nullptr;
}

template <typename T> std::optional<Content> content(const std::optional<T>& slot) {
    if (!slot) {
        return {};
    }
    return Content{detail::SlotContentAccess::function(*slot)};
}
} // namespace

MessageRef::MessageRef() : state_(std::make_shared<detail::NoticeRefState>()) {}

NoticeHandle::NoticeHandle() : state_(std::make_shared<detail::NoticeCompletion>()) {}

std::shared_future<NoticeCloseResult> NoticeHandle::closed() const {
    return state_->future;
}

bool NoticeHandle::close() const {
    if (state_->thread != std::this_thread::get_id()) {
        return false;
    }
    const auto request = state_->close;
    return request && request();
}

bool NoticeHandle::update(MessageProps props, MessageSlots slots) const {
    if (state_->thread != std::this_thread::get_id() || !state_->message) {
        return false;
    }
    const auto request = state_->update;
    return request && request(std::move(props.options_), {content(slots.content), {}, {}});
}

bool NoticeHandle::update(MessageProps props) const {
    return update(std::move(props), MessageSlots{});
}

bool NoticeHandle::update(NotificationProps props) const {
    return update(std::move(props), NotificationSlots{});
}

bool NoticeHandle::update(NotificationProps props, NotificationSlots slots) const {
    if (state_->thread != std::this_thread::get_id() || state_->message) {
        return false;
    }
    const auto request = state_->update;
    return request && request(std::move(props.options_),
                              {content(slots.title), content(slots.description), content(slots.actions)});
}

NotificationRef::NotificationRef() : state_(std::make_shared<detail::NoticeRefState>()) {}

bool MessageRef::bound() const {
    return bool(binding(state_));
}

bool NotificationRef::bound() const {
    return bool(binding(state_));
}

bool MessageRef::open(String key, MessageProps props, MessageSlots slots) const {
    const auto target = binding(state_);
    return target &&
           target->open(std::move(key), std::move(props.options_), {content(slots.content), {}, {}}, state_->context);
}

bool NotificationRef::open(String key, NotificationProps props, NotificationSlots slots) const {
    const auto target = binding(state_);
    return target &&
           target->open(std::move(key), std::move(props.options_),
                        {content(slots.title), content(slots.description), content(slots.actions)}, state_->context);
}

NoticeHandle MessageRef::openAsync(String key, MessageProps props, MessageSlots slots) const {
    NoticeHandle handle;
    props.options_.completion = handle.state_;
    try {
        if (!open(std::move(key), std::move(props), std::move(slots))) {
            handle.state_->settle(NoticeCloseResult::Cancelled);
        }
    } catch (...) {
        handle.state_->settle(NoticeCloseResult::Failed);
        throw;
    }
    return handle;
}

NoticeHandle NotificationRef::openAsync(String key, NotificationProps props, NotificationSlots slots) const {
    NoticeHandle handle;
    props.options_.completion = handle.state_;
    try {
        if (!open(std::move(key), std::move(props), std::move(slots))) {
            handle.state_->settle(NoticeCloseResult::Cancelled);
        }
    } catch (...) {
        handle.state_->settle(NoticeCloseResult::Failed);
        throw;
    }
    return handle;
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
