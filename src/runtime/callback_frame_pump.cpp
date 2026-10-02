#include "runtime/callback_frame_pump.hpp"

namespace ryn::runtime {
struct FrameCallbackLifetime {
    CallbackFramePump* pump{};
    std::thread::id owner{std::this_thread::get_id()};
    std::uint64_t generation{};
};

FrameLoopStep FrameCallback::run(animation::AnimationTime time) const {
    const auto token = lifetime_.lock();
    if (!token || std::this_thread::get_id() != token->owner || !token->pump || token->generation != generation_) {
        return FrameLoopStep::idle;
    }
    return token->pump->dispatch(generation_, time);
}

CallbackFramePump::CallbackFramePump(FrameRequestState& requests, OnDemandFrameLoop& loop, FrameCallbackHost& host)
    : requests_(&requests), loop_(&loop), host_(&host), lifetime_(std::make_shared<FrameCallbackLifetime>()) {
    requests_->bind_wake_sink(*this);
    lifetime_->pump = this;
    reconcile();
}

CallbackFramePump::~CallbackFramePump() {
    stop();
}

void CallbackFramePump::stop() noexcept {
    if (!lifetime_->pump) {
        return;
    }
    lifetime_->pump = nullptr;
    ++lifetime_->generation;
    requests_->unbind_wake_sink(*this);
    if (scheduled_) {
        host_->cancel_callback();
    }
    scheduled_ = false;
}

void CallbackFramePump::wake() noexcept {
    if (std::this_thread::get_id() == lifetime_->owner && lifetime_->pump && !running_) {
        reconcile();
    }
}

void CallbackFramePump::reconcile() noexcept {
    if (!lifetime_->pump) {
        return;
    }
    const auto deadline = loop_->next_deadline();
    const bool wanted = requests_->pending() || deadline.has_value();
    const auto target = requests_->pending() ? std::nullopt : deadline;
    if (!wanted) {
        if (scheduled_) {
            ++lifetime_->generation;
            host_->cancel_callback();
            scheduled_ = false;
        }
        return;
    }
    if (scheduled_ && target == scheduled_deadline_) {
        return;
    }
    scheduled_ = true;
    scheduled_deadline_ = target;
    FrameCallback callback;
    callback.lifetime_ = lifetime_;
    callback.generation_ = ++lifetime_->generation;
    host_->replace_callback(target, callback);
}

FrameLoopStep CallbackFramePump::dispatch(std::uint64_t generation, animation::AnimationTime time) {
    if (!scheduled_ || running_ || generation != lifetime_->generation) {
        return FrameLoopStep::idle;
    }
    scheduled_ = false;
    running_ = true;
    const auto token = lifetime_;
    auto* const loop = loop_;
    try {
        const auto result = loop->tick(time);
        if (token->pump != this) {
            return result;
        }
        running_ = false;
        reconcile();
        return result;
    } catch (...) {
        if (token->pump != this) {
            throw;
        }
        running_ = false;
        reconcile();
        throw;
    }
}
} // namespace ryn::runtime
