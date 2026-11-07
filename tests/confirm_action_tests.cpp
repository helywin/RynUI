#include "component/confirm_action.hpp"
#include "support/i18n_test.hpp"
#include <thread>
#include <vector>

namespace {
using namespace ryn;
using namespace ryn::animation;
using detail::ConfirmActionSession;
using ryn_test::i18n::require;

void completion() {
    int calls{};
    ConfirmActionResult last;
    auto session = std::make_shared<ConfirmActionSession>([&](ConfirmActionResult value) {
        ++calls;
        last = std::move(value);
    });
    require(session->start(ConfirmAction::synchronous([] { return ConfirmActionResult{}; }), {}), "sync start failed");
    require(calls == 1 && !session->pending() && !session->deadline(), "sync action did not finish once");
    ConfirmAction::Complete complete;
    std::vector<std::function<void()>> posted;
    ConfirmAction source{[&](ConfirmActionRequest, ConfirmAction::Complete done) {
                             complete = std::move(done);
                             return ConfirmAction::Cancel{};
                         },
                         [&](std::function<void()> fn) { posted.push_back(std::move(fn)); }};
    session->start(source, {});
    require(!session->start(source, {}), "duplicate pending action accepted");
    std::thread worker([&] {
        complete({ConfirmActionDecision::KeepOpen});
        complete({});
    });
    worker.join();
    require(calls == 1 && session->pending() && posted.size() == 1,
            "worker touched owner state or duplicated delivery");
    const auto delivery = posted.back();
    delivery();
    delivery();
    require(calls == 2 && last.decision == ConfirmActionDecision::KeepOpen && !session->pending() &&
                !session->deadline(),
            "owner completion/duplicate/KeepOpen contract failed");
}

void failures() {
    for (const int mode : {0, 1, 2, 3}) {
        int calls{};
        int cancels{};
        ConfirmActionErrorCode error{};
        auto session = std::make_shared<ConfirmActionSession>([&](ConfirmActionResult value) {
            ++calls;
            require(value.error.has_value() && value.decision == ConfirmActionDecision::KeepOpen,
                    "failure closed action");
            error = value.error->code;
        });
        ConfirmAction::Complete complete;
        ConfirmAction source{[&](ConfirmActionRequest, ConfirmAction::Complete done) -> ConfirmAction::Cancel {
                                 if (mode == 0) {
                                     throw std::runtime_error("provider");
                                 }
                                 complete = std::move(done);
                                 return [&] {
                                     ++cancels;
                                 };
                             },
                             [mode](std::function<void()> fn) {
                                 if (mode == 1) {
                                     throw std::runtime_error("post");
                                 }
                                 fn();
                             },
                             std::chrono::milliseconds(10)};
        session->start(source, {});
        if (mode == 1 || mode == 2) {
            std::thread worker([&] { complete({}); });
            worker.join();
            require(calls == 0, "worker executed failure callback");
        }
        if (mode != 0) {
            session->tick(AnimationTime::microseconds(10000));
        }
        require(calls == 1 && !session->pending() && !session->deadline(), "failure retained pending/deadline");
        require(error == (mode == 0   ? ConfirmActionErrorCode::ProviderThrew
                          : mode == 3 ? ConfirmActionErrorCode::Timeout
                                      : ConfirmActionErrorCode::DispatchFailed),
                "failure classification changed");
        require(cancels == (mode == 0 ? 0 : 1), "failed operation cancel count changed");
    }
}

void retirement() {
    int applies{};
    int cancels{};
    ConfirmAction::Complete late;
    std::shared_ptr<const std::atomic_bool> ticket;
    auto session = std::make_shared<ConfirmActionSession>([&](ConfirmActionResult) { ++applies; });
    ConfirmAction source{[&](ConfirmActionRequest request, ConfirmAction::Complete done) {
                             ticket = request.cancelled;
                             late = std::move(done);
                             return [&] {
                                 ++cancels;
                                 require(ticket->load() && !session->pending() && !session->deadline(),
                                         "cancel ran before invalidation");
                                 late({});
                                 throw std::runtime_error("cancel");
                             };
                         },
                         [](std::function<void()> fn) { fn(); }};
    session->start(source, {});
    auto cancel = session->detach_cancel();
    require(ticket->load() && cancels == 0, "detach executed user cancel");
    ConfirmActionSession::run_cancel(std::move(cancel));
    late({});
    require(cancels == 1 && applies == 0, "late completion survived cancellation");

    auto sync = std::make_shared<ConfirmActionSession>([&](ConfirmActionResult) { session->cancel(); });
    // Synchronous completion can retire its own session before provider returns a cancel.
    sync = std::make_shared<ConfirmActionSession>([&](ConfirmActionResult) { sync->cancel(); });
    sync->start(ConfirmAction{[&](ConfirmActionRequest, ConfirmAction::Complete done) {
                                  done({});
                                  return [&] {
                                      ++cancels;
                                  };
                              },
                              [](std::function<void()> fn) { fn(); }},
                {});
    require(cancels == 2 && !sync->pending(), "returned cancel escaped synchronous retirement");
}
} // namespace

int main() {
    ryn_test::i18n::suppress_dialogs();
    try {
        completion();
        failures();
        retirement();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
