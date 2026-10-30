#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

namespace ryn::detail {

class ObserverNode;

enum class ObserverPhase {
    memo,
    binding,
    effect,
};

class ReactiveSource {
public:
    ReactiveSource() = default;
    ReactiveSource(const ReactiveSource&) = delete;
    ReactiveSource& operator=(const ReactiveSource&) = delete;
    virtual ~ReactiveSource();

protected:
    void notify_observers();

private:
    friend class ObserverNode;

    static constexpr std::size_t invalid_slot = std::numeric_limits<std::size_t>::max();

    struct Subscription final {
        std::size_t index{invalid_slot};
        std::uint64_t generation{};
    };

    struct SubscriberSlot final {
        std::weak_ptr<ObserverNode> observer;
        const ObserverNode* identity{};
        std::size_t previous{invalid_slot};
        std::size_t next{invalid_slot};
        std::uint64_t generation{1};
    };

    [[nodiscard]] Subscription subscribe(const std::shared_ptr<ObserverNode>& observer);
    void unsubscribe(Subscription subscription, const ObserverNode* observer) noexcept;
    void release_slot(std::size_t index) noexcept;

    std::vector<SubscriberSlot> subscribers_;
    std::size_t first_subscriber_{invalid_slot};
    std::size_t last_subscriber_{invalid_slot};
    std::size_t free_slot_{invalid_slot};
};

class ObserverNode final : public std::enable_shared_from_this<ObserverNode> {
public:
    ObserverNode(ObserverPhase phase, std::function<void()> callback);
    ObserverNode(const ObserverNode&) = delete;
    ObserverNode& operator=(const ObserverNode&) = delete;
    ~ObserverNode();

    void run();
    void deactivate() noexcept;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] ObserverPhase phase() const noexcept;

private:
    friend class Scheduler;
    friend class ReactiveSource;
    friend void record_dependency(ReactiveSource& source);

    void track(ReactiveSource& source);
    void forget(ReactiveSource& source) noexcept;

    struct Dependency final {
        ReactiveSource* source{};
        ReactiveSource::Subscription subscription;
    };

    ObserverPhase phase_;
    std::function<void()> callback_;
    std::vector<Dependency> dependencies_;
    bool active_{true};
    bool queued_{false};
};

class Scheduler final {
public:
    static Scheduler& current() noexcept;

    void schedule(const std::shared_ptr<ObserverNode>& observer);
    void begin_notification() noexcept;
    void end_notification();
    void begin_batch() noexcept;
    void end_batch();
    void begin_observer() noexcept;
    void end_observer();
    void flush();

    [[nodiscard]] std::size_t epoch() const noexcept;

private:
    void process_queue(std::vector<std::shared_ptr<ObserverNode>>& queue);
    [[nodiscard]] bool has_pending_work() const noexcept;

    std::vector<std::shared_ptr<ObserverNode>> pending_memos_;
    std::vector<std::shared_ptr<ObserverNode>> pending_bindings_;
    std::vector<std::shared_ptr<ObserverNode>> pending_effects_;
    std::vector<std::shared_ptr<ObserverNode>> current_queue_;
    std::size_t epoch_{0};
    std::size_t notification_depth_{0};
    std::size_t batch_depth_{0};
    std::size_t observer_depth_{0};
    bool flushing_{false};
};

[[nodiscard]] std::shared_ptr<ObserverNode> observe(ObserverPhase phase, std::function<void()> callback,
                                                    bool run_immediately = true);

void record_dependency(ReactiveSource& source);

} // namespace ryn::detail
