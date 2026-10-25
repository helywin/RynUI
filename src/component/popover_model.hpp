#pragma once

#include "animation/time.hpp"
#include <ryn/popover.hpp>

namespace ryn::detail {
[[nodiscard]] PopoverTriggers popover_triggers(PopoverTriggerMode mode);
void validate_popover_delay(Duration value);

class PopoverModel final {
public:
    PopoverModel(bool controlled, bool open) : controlled_(controlled), open_(open) {}

    void reconcile(bool open);
    [[nodiscard]] bool request(bool open);
    [[nodiscard]] bool suppress();
    void dismiss();
    void reset_observation(bool dismiss = false);
    void retime(animation::AnimationTime now, Duration enter, Duration leave);
    void observe(bool desire, bool immediate, animation::AnimationTime now, Duration enter, Duration leave);
    [[nodiscard]] std::optional<bool> due(animation::AnimationTime now);

    [[nodiscard]] bool open() const noexcept {
        return open_ && !suppressed_;
    }

    [[nodiscard]] std::optional<animation::AnimationTime> deadline() const noexcept {
        return deadline_;
    }

private:
    void schedule(bool open, bool immediate, animation::AnimationTime now, Duration enter, Duration leave);
    bool controlled_{};
    bool open_{};
    bool suppressed_{};
    bool observed_{};
    bool dismissed_{};
    bool pending_{};
    bool pending_immediate_{};
    std::optional<bool> requested_;
    std::optional<animation::AnimationTime> deadline_;
};
} // namespace ryn::detail
