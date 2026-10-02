#pragma once

#include "runtime/frame_scheduler.hpp"

#include <memory>
#include <thread>

namespace ryn::runtime {
class CallbackFramePump;
struct FrameCallbackLifetime;

class FrameCallback final {
public:
    FrameCallback() = default;
    [[nodiscard]] FrameLoopStep run(animation::AnimationTime time) const;

private:
    friend class CallbackFramePump;
    std::weak_ptr<FrameCallbackLifetime> lifetime_;
    std::uint64_t generation_{};
};

class FrameCallbackHost {
public:
    virtual ~FrameCallbackHost() = default;
    // Replace the one outstanding callback. nullopt means the next event turn;
    // never invoke inline. Host and callbacks run on the pump's owner thread.
    virtual void replace_callback(std::optional<animation::AnimationTime> deadline,
                                  FrameCallback callback) noexcept = 0;
    virtual void cancel_callback() noexcept = 0;
};

class CallbackFramePump final : private FrameWakeSink {
public:
    CallbackFramePump(FrameRequestState& requests, OnDemandFrameLoop& loop, FrameCallbackHost& host);
    ~CallbackFramePump();
    CallbackFramePump(const CallbackFramePump&) = delete;
    CallbackFramePump& operator=(const CallbackFramePump&) = delete;
    void stop() noexcept;

private:
    friend class FrameCallback;
    void wake() noexcept override;
    void reconcile() noexcept;
    FrameLoopStep dispatch(std::uint64_t generation, animation::AnimationTime time);
    FrameRequestState* requests_;
    OnDemandFrameLoop* loop_;
    FrameCallbackHost* host_;
    std::shared_ptr<FrameCallbackLifetime> lifetime_;
    std::optional<animation::AnimationTime> scheduled_deadline_;
    bool scheduled_{};
    bool running_{};
};
} // namespace ryn::runtime
