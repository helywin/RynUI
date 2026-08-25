#include <ryn/detail/reactive_runtime.hpp>
#include <ryn/version.hpp>

#include "internal/layer_anchors.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ryn {

Version version() noexcept {
    return {
        static_cast<std::uint16_t>(RYNUI_VERSION_MAJOR),
        static_cast<std::uint16_t>(RYNUI_VERSION_MINOR),
        static_cast<std::uint16_t>(RYNUI_VERSION_PATCH),
    };
}

namespace detail {
namespace {

thread_local ObserverNode* active_observer = nullptr;

class ActiveObserverGuard final {
public:
    explicit ActiveObserverGuard(ObserverNode* observer) noexcept
        : previous_(active_observer) {
        active_observer = observer;
    }

    ~ActiveObserverGuard() {
        active_observer = previous_;
    }

private:
    ObserverNode* previous_;
};

} // namespace

void reactive_layer_anchor() noexcept {}

void ReactiveSource::subscribe(const std::shared_ptr<ObserverNode>& observer) {
    for (auto iterator = subscribers_.begin(); iterator != subscribers_.end();) {
        const auto subscriber = iterator->lock();
        if (!subscriber) {
            iterator = subscribers_.erase(iterator);
        } else {
            if (subscriber.get() == observer.get()) {
                return;
            }
            ++iterator;
        }
    }
    subscribers_.emplace_back(observer);
}

void ReactiveSource::unsubscribe(const ObserverNode* observer) noexcept {
    std::erase_if(subscribers_, [observer](const auto& subscriber) {
        const auto locked = subscriber.lock();
        return !locked || locked.get() == observer;
    });
}

void ReactiveSource::notify_observers() {
    auto& scheduler = Scheduler::current();
    scheduler.begin_notification();
    for (auto iterator = subscribers_.begin(); iterator != subscribers_.end();) {
        const auto subscriber = iterator->lock();
        if (!subscriber) {
            iterator = subscribers_.erase(iterator);
        } else {
            scheduler.schedule(subscriber);
            ++iterator;
        }
    }
    scheduler.end_notification();
}

ObserverNode::ObserverNode(ObserverPhase phase, std::function<void()> callback)
    : phase_(phase), callback_(std::move(callback)) {}

ObserverNode::~ObserverNode() {
    deactivate();
}

void ObserverNode::run() {
    if (!active_) {
        return;
    }
    for (auto* dependency : dependencies_) {
        dependency->unsubscribe(this);
    }
    dependencies_.clear();

    ActiveObserverGuard guard(this);
    callback_();
}

void ObserverNode::deactivate() noexcept {
    if (!active_) {
        return;
    }
    active_ = false;
    queued_ = false;
    for (auto* dependency : dependencies_) {
        dependency->unsubscribe(this);
    }
    dependencies_.clear();
}

bool ObserverNode::active() const noexcept {
    return active_;
}

ObserverPhase ObserverNode::phase() const noexcept {
    return phase_;
}

void ObserverNode::track(ReactiveSource& source) {
    if (std::find(dependencies_.begin(), dependencies_.end(), &source)
            != dependencies_.end()) {
        return;
    }
    dependencies_.push_back(&source);
    source.subscribe(shared_from_this());
}

Scheduler& Scheduler::current() noexcept {
    thread_local Scheduler scheduler;
    return scheduler;
}

void Scheduler::schedule(const std::shared_ptr<ObserverNode>& observer) {
    if (!observer->active_ || observer->queued_) {
        return;
    }
    observer->queued_ = true;
    switch (observer->phase_) {
    case ObserverPhase::memo:
        pending_memos_.push_back(observer);
        break;
    case ObserverPhase::binding:
        pending_bindings_.push_back(observer);
        break;
    case ObserverPhase::effect:
        pending_effects_.push_back(observer);
        break;
    }
    if (notification_depth_ == 0 && batch_depth_ == 0 && !flushing_) {
        flush();
    }
}

void Scheduler::begin_notification() noexcept {
    ++notification_depth_;
}

void Scheduler::end_notification() {
    if (notification_depth_ == 0) {
        throw std::logic_error("Reactive notification depth underflow");
    }
    --notification_depth_;
    if (notification_depth_ == 0 && batch_depth_ == 0 && !flushing_) {
        flush();
    }
}

void Scheduler::begin_batch() noexcept {
    ++batch_depth_;
}

void Scheduler::end_batch() {
    if (batch_depth_ == 0) {
        throw std::logic_error("Reactive batch depth underflow");
    }
    --batch_depth_;
    if (batch_depth_ == 0 && notification_depth_ == 0 && !flushing_) {
        flush();
    }
}

void Scheduler::process_queue(std::vector<std::shared_ptr<ObserverNode>>& queue) {
    current_queue_.swap(queue);
    for (const auto& observer : current_queue_) {
        observer->queued_ = false;
        observer->run();
    }
    current_queue_.clear();
}

bool Scheduler::has_pending_work() const noexcept {
    return !pending_memos_.empty()
        || !pending_bindings_.empty()
        || !pending_effects_.empty();
}

void Scheduler::flush() {
    if (flushing_) {
        return;
    }
    flushing_ = true;
    std::size_t rounds = 0;
    try {
        while (has_pending_work()) {
            if (++rounds > 100) {
                throw std::runtime_error("Reactive flush exceeded 100 epochs");
            }
            ++epoch_;
            process_queue(pending_memos_);
            process_queue(pending_bindings_);
            process_queue(pending_effects_);
        }
    } catch (...) {
        flushing_ = false;
        throw;
    }
    flushing_ = false;
}

std::size_t Scheduler::epoch() const noexcept {
    return epoch_;
}

std::shared_ptr<ObserverNode> observe(
    ObserverPhase phase,
    std::function<void()> callback,
    bool run_immediately) {
    auto observer = std::make_shared<ObserverNode>(phase, std::move(callback));
    if (run_immediately) {
        observer->run();
    }
    return observer;
}

void record_dependency(ReactiveSource& source) {
    if (active_observer != nullptr) {
        active_observer->track(source);
    }
}

} // namespace detail
} // namespace ryn
