#pragma once

#include <cstddef>
#include <functional>
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
    virtual ~ReactiveSource() = default;

    void subscribe(const std::shared_ptr<ObserverNode>& observer);
    void unsubscribe(const ObserverNode* observer) noexcept;

protected:
    void notify_observers();

private:
    std::vector<std::weak_ptr<ObserverNode>> subscribers_;
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
    friend void record_dependency(ReactiveSource& source);

    void track(ReactiveSource& source);

    ObserverPhase phase_;
    std::function<void()> callback_;
    std::vector<ReactiveSource*> dependencies_;
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

[[nodiscard]] std::shared_ptr<ObserverNode> observe(
    ObserverPhase phase,
    std::function<void()> callback,
    bool run_immediately = true);

void record_dependency(ReactiveSource& source);

} // namespace ryn::detail
