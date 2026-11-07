#pragma once

#include "animation/time.hpp"
#include <ryn/confirm_action.hpp>
#include <optional>
#include <thread>

namespace ryn::detail {
// One actual Modal/Popconfirm action. The ticket is the only object accessed by
// workers. Its callback captures a weak session, never a component or a host.
class ConfirmActionSession final : public std::enable_shared_from_this<ConfirmActionSession> {
public:
    using Apply = std::function<void(ConfirmActionResult)>;
    explicit ConfirmActionSession(Apply);
    ~ConfirmActionSession();
    bool start(const ConfirmAction&, animation::AnimationTime);
    [[nodiscard]] ConfirmAction::Cancel detach_cancel() noexcept;
    void cancel() noexcept;
    bool tick(animation::AnimationTime);
    [[nodiscard]] bool pending() const noexcept;

    [[nodiscard]] std::uint64_t generation() const noexcept {
        return generation_;
    }

    [[nodiscard]] std::optional<animation::AnimationTime> deadline() const noexcept;
    static void run_cancel(ConfirmAction::Cancel) noexcept;

private:
    struct Ticket;
    void complete(const std::shared_ptr<Ticket>&, ConfirmActionResult);
    Apply apply_;
    std::thread::id thread_{std::this_thread::get_id()};
    std::shared_ptr<Ticket> ticket_;
    ConfirmAction::Cancel cancel_;
    std::optional<animation::AnimationTime> deadline_;
    std::uint64_t generation_{};
};
} // namespace ryn::detail
