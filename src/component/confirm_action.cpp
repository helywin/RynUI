#include "component/confirm_action.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ryn {
ConfirmAction::ConfirmAction(Provider provider, OwnerThreadPost post, std::chrono::milliseconds timeout)
    : provider_(std::make_shared<const Provider>(std::move(provider))),
      post_(std::make_shared<const OwnerThreadPost>(std::move(post))), timeout_(timeout) {
    if (!*provider_ || !*post_ || timeout.count() <= 0 || timeout > std::chrono::hours(24)) {
        throw std::invalid_argument("ConfirmAction requires a provider, owner dispatcher and timeout in (0, 24h]");
    }
}

ConfirmAction ConfirmAction::synchronous(std::function<ConfirmActionResult()> action) {
    if (!action) {
        throw std::invalid_argument("ConfirmAction requires a synchronous action");
    }
    return ConfirmAction{[action = std::move(action)](ConfirmActionRequest, Complete complete) {
                             complete(action());
                             return Cancel{};
                         },
                         [](std::function<void()> callback) { callback(); }};
}

ConfirmAction::operator bool() const noexcept {
    return bool(provider_);
}

const ConfirmAction::Provider& ConfirmAction::provider() const {
    return *provider_;
}

const OwnerThreadPost& ConfirmAction::post() const {
    return *post_;
}

std::chrono::milliseconds ConfirmAction::timeout() const noexcept {
    return timeout_;
}
} // namespace ryn

namespace ryn::detail {
struct ConfirmActionSession::Ticket final {
    std::shared_ptr<std::atomic_bool> cancelled{std::make_shared<std::atomic_bool>(false)};
    std::atomic_bool delivered{};
    std::atomic_bool dispatch_failed{};
    std::thread::id thread;
    std::uint64_t generation{};
};

ConfirmActionSession::ConfirmActionSession(Apply apply) : apply_(std::move(apply)) {}

ConfirmActionSession::~ConfirmActionSession() {
    cancel();
}

bool ConfirmActionSession::pending() const noexcept {
    return bool(ticket_);
}

std::optional<animation::AnimationTime> ConfirmActionSession::deadline() const noexcept {
    return deadline_;
}

void ConfirmActionSession::run_cancel(ConfirmAction::Cancel cancel) noexcept {
    if (cancel) {
        try {
            cancel();
        } catch (...) {
        }
    }
}

ConfirmAction::Cancel ConfirmActionSession::detach_cancel() noexcept {
    ++generation_;
    if (ticket_) {
        ticket_->cancelled->store(true);
        ticket_.reset();
    }
    deadline_.reset();
    return std::exchange(cancel_, {});
}

void ConfirmActionSession::cancel() noexcept {
    run_cancel(detach_cancel());
}

bool ConfirmActionSession::start(const ConfirmAction& source, animation::AnimationTime now) {
    if (thread_ != std::this_thread::get_id() || pending() || !source) {
        return false;
    }
    const auto self = shared_from_this();
    const auto ticket = std::make_shared<Ticket>();
    ticket->thread = thread_;
    ticket->generation = ++generation_;
    ticket_ = ticket;
    deadline_ = now + animation::AnimationDuration::milliseconds(static_cast<double>(source.timeout().count()));
    const auto weak = weak_from_this();
    const auto post = source.post();
    try {
        auto cancel = source.provider()({ticket->generation, ticket->cancelled},
                                        [ticket, weak, post](ConfirmActionResult result) {
                                            if (ticket->cancelled->load() || ticket->delivered.exchange(true)) {
                                                return;
                                            }
                                            try {
                                                post([ticket, weak, result = std::move(result)]() mutable {
                                                    if (ticket->thread != std::this_thread::get_id()) {
                                                        ticket->dispatch_failed.store(true);
                                                        return;
                                                    }
                                                    if (ticket->cancelled->load()) {
                                                        return;
                                                    }
                                                    if (const auto owner = weak.lock()) {
                                                        owner->complete(ticket, std::move(result));
                                                    }
                                                });
                                            } catch (...) {
                                                ticket->dispatch_failed.store(true);
                                            }
                                        });
        if (ticket_ == ticket && !ticket->cancelled->load()) {
            cancel_ = std::move(cancel);
        } else if (ticket->cancelled->load() || generation_ != ticket->generation) {
            ticket->cancelled->store(true);
            run_cancel(std::move(cancel));
        }
    } catch (...) {
        if (ticket_ == ticket) {
            complete(ticket,
                     {ConfirmActionDecision::KeepOpen, ConfirmActionError{ConfirmActionErrorCode::ProviderThrew, {}}});
        }
    }
    return true;
}

void ConfirmActionSession::complete(const std::shared_ptr<Ticket>& ticket, ConfirmActionResult result) {
    if (thread_ != std::this_thread::get_id() || ticket_ != ticket || ticket->cancelled->load() ||
        ticket->generation != generation_) {
        return;
    }
    ticket_.reset();
    deadline_.reset();
    cancel_ = {};
    const auto apply = apply_;
    apply(std::move(result));
}

bool ConfirmActionSession::tick(animation::AnimationTime now) {
    const auto ticket = ticket_;
    if (!ticket || thread_ != std::this_thread::get_id()) {
        return false;
    }
    const bool failed = ticket->dispatch_failed.load();
    if (!failed && (!deadline_ || now < *deadline_)) {
        return false;
    }
    auto cancel = detach_cancel();
    const auto apply = apply_;
    // The old operation is fully invalid before application code can reenter.
    run_cancel(std::move(cancel));
    if (generation_ == ticket->generation + 1 && !ticket_) {
        apply({ConfirmActionDecision::KeepOpen,
               ConfirmActionError{failed ? ConfirmActionErrorCode::DispatchFailed : ConfirmActionErrorCode::Timeout,
                                  {}}});
    }
    return true;
}
} // namespace ryn::detail
