#include <ryn/reactive.hpp>
#include <ryn/version.hpp>

#include "internal/layer_anchors.hpp"
#include "reactive/untracked.hpp"

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
    explicit ActiveObserverGuard(ObserverNode* observer) noexcept : previous_(active_observer) {
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

ReactiveSource::~ReactiveSource() {
    // Computation closures can retain observers longer than a dynamically
    // removed source. Drop its borrowed pointer before the source is released.
    for (auto index = first_subscriber_; index != invalid_slot; index = subscribers_[index].next) {
        if (const auto observer = subscribers_[index].observer.lock()) {
            observer->forget(*this);
        }
    }
}

ReactiveSource::Subscription ReactiveSource::subscribe(const std::shared_ptr<ObserverNode>& observer) {
    std::size_t index{};
    if (free_slot_ != invalid_slot) {
        index = free_slot_;
        free_slot_ = subscribers_[index].next;
    } else {
        index = subscribers_.size();
        // Complete the only potentially throwing operation before publishing links.
        subscribers_.emplace_back();
    }
    auto& slot = subscribers_[index];
    slot.observer = observer;
    slot.identity = observer.get();
    slot.previous = last_subscriber_;
    slot.next = invalid_slot;
    if (last_subscriber_ != invalid_slot) {
        subscribers_[last_subscriber_].next = index;
    } else {
        first_subscriber_ = index;
    }
    last_subscriber_ = index;
    return {index, slot.generation};
}

void ReactiveSource::unsubscribe(Subscription subscription, const ObserverNode* observer) noexcept {
    if (subscription.index >= subscribers_.size()) {
        return;
    }
    const auto& slot = subscribers_[subscription.index];
    if (slot.generation == subscription.generation && slot.identity == observer) {
        release_slot(subscription.index);
    }
}

void ReactiveSource::release_slot(std::size_t index) noexcept {
    auto& slot = subscribers_[index];
    if (slot.previous != invalid_slot) {
        subscribers_[slot.previous].next = slot.next;
    } else {
        first_subscriber_ = slot.next;
    }
    if (slot.next != invalid_slot) {
        subscribers_[slot.next].previous = slot.previous;
    } else {
        last_subscriber_ = slot.previous;
    }
    slot.observer.reset();
    slot.identity = nullptr;
    slot.previous = invalid_slot;
    slot.next = free_slot_;
    free_slot_ = index;
    ++slot.generation;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
}

void ReactiveSource::notify_observers() {
    auto& scheduler = Scheduler::current();
    scheduler.begin_notification();
    for (auto index = first_subscriber_; index != invalid_slot;) {
        const auto next = subscribers_[index].next;
        const auto subscriber = subscribers_[index].observer.lock();
        if (!subscriber) {
            release_slot(index);
        } else {
            scheduler.schedule(subscriber);
        }
        index = next;
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
    for (const auto& dependency : dependencies_) {
        dependency.source->unsubscribe(dependency.subscription, this);
    }
    dependencies_.clear();

    auto& scheduler = Scheduler::current();
    scheduler.begin_observer();
    try {
        ActiveObserverGuard guard(this);
        callback_();
    } catch (...) {
        scheduler.end_observer();
        throw;
    }
    scheduler.end_observer();
}

void ObserverNode::deactivate() noexcept {
    if (!active_) {
        return;
    }
    active_ = false;
    queued_ = false;
    for (const auto& dependency : dependencies_) {
        dependency.source->unsubscribe(dependency.subscription, this);
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
    if (!active_ || std::ranges::any_of(dependencies_,
                                        [&source](const auto& dependency) { return dependency.source == &source; })) {
        return;
    }
    // Reserve the observer side before linking the source. A failed source growth
    // removes the placeholder, so both sides remain consistent without callbacks.
    dependencies_.push_back({&source, {}});
    try {
        dependencies_.back().subscription = source.subscribe(shared_from_this());
    } catch (...) {
        dependencies_.pop_back();
        throw;
    }
}

void ObserverNode::forget(ReactiveSource& source) noexcept {
    std::erase_if(dependencies_, [&source](const auto& dependency) { return dependency.source == &source; });
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
    if (notification_depth_ == 0 && batch_depth_ == 0 && observer_depth_ == 0 && !flushing_) {
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
    if (notification_depth_ == 0 && batch_depth_ == 0 && observer_depth_ == 0 && !flushing_) {
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
    if (batch_depth_ == 0 && notification_depth_ == 0 && observer_depth_ == 0 && !flushing_) {
        flush();
    }
}

void Scheduler::begin_observer() noexcept {
    ++observer_depth_;
}

void Scheduler::end_observer() {
    if (observer_depth_ == 0) {
        throw std::logic_error("Reactive observer depth underflow");
    }
    --observer_depth_;
    if (observer_depth_ == 0 && batch_depth_ == 0 && notification_depth_ == 0 && !flushing_) {
        flush();
    }
}

void Scheduler::process_queue(std::vector<std::shared_ptr<ObserverNode>>& queue) {
    current_queue_.swap(queue);
    std::size_t index{};
    try {
        for (; index < current_queue_.size(); ++index) {
            const auto& observer = current_queue_[index];
            observer->queued_ = false;
            observer->run();
        }
    } catch (...) {
        // Unexecuted entries are discarded, not carried into the next flush.
        // Processed entries (including the thrower) may already have queued a
        // later epoch; keep those pending entries and their queued flags intact.
        for (auto remaining = index + 1; remaining < current_queue_.size(); ++remaining) {
            current_queue_[remaining]->queued_ = false;
        }
        current_queue_.clear();
        throw;
    }
    current_queue_.clear();
}

bool Scheduler::has_pending_work() const noexcept {
    return !pending_memos_.empty() || !pending_bindings_.empty() || !pending_effects_.empty();
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

std::shared_ptr<ObserverNode> observe(ObserverPhase phase, std::function<void()> callback, bool run_immediately) {
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

void run_untracked(const std::function<void()>& callback) {
    ActiveObserverGuard guard(nullptr);
    callback();
}

} // namespace detail

struct Scope::State {
    std::vector<std::shared_ptr<detail::ObserverNode>> observers;
    std::vector<std::function<void()>> cleanups;
    bool active{true};
};

Scope::Scope() : state_(std::make_unique<State>()) {}

Scope::~Scope() {
    dispose();
}

void Scope::on_cleanup(std::function<void()> cleanup) {
    if (!state_->active) {
        cleanup();
        return;
    }
    state_->cleanups.push_back(std::move(cleanup));
}

void Scope::dispose() noexcept {
    if (!state_->active) {
        return;
    }
    state_->active = false;
    for (const auto& observer : state_->observers) {
        observer->deactivate();
    }
    state_->observers.clear();

    for (auto iterator = state_->cleanups.rbegin(); iterator != state_->cleanups.rend(); ++iterator) {
        try {
            (*iterator)();
        } catch (...) {
        }
    }
    state_->cleanups.clear();
}

bool Scope::active() const noexcept {
    return state_->active;
}

void Scope::own_observer(std::shared_ptr<detail::ObserverNode> observer) {
    if (!state_->active) {
        observer->deactivate();
        return;
    }
    state_->observers.push_back(std::move(observer));
}

} // namespace ryn
