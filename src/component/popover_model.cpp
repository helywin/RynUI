#include "component/popover_model.hpp"
#include <limits>
#include <stdexcept>

namespace ryn::detail {
PopoverTriggers popover_triggers(PopoverTriggerMode mode) {
    if (mode < PopoverTriggerMode::Manual || mode > PopoverTriggerMode::Click) {
        throw std::invalid_argument("Popover trigger is invalid");
    }
    return {mode == PopoverTriggerMode::Hover || mode == PopoverTriggerMode::HoverFocus,
            mode == PopoverTriggerMode::Focus || mode == PopoverTriggerMode::HoverFocus,
            mode == PopoverTriggerMode::Click};
}

void validate_popover_delay(Duration value) {
    if (value.count_milliseconds() > 86400000) {
        throw std::invalid_argument("Popover delay cannot exceed one day");
    }
}

void PopoverModel::reconcile(bool open) {
    open_ = open;
    suppressed_ = false;
    requested_ = open;
    deadline_.reset();
    dismissed_ = false;
}

bool PopoverModel::request(bool open) {
    deadline_.reset();
    if (open) {
        suppressed_ = false;
        dismissed_ = false;
    }
    const auto previous = requested_;
    requested_ = open;
    const bool notify = previous != open && (open_ != open || previous.has_value());
    if (!controlled_) {
        open_ = open;
    }
    return notify;
}

bool PopoverModel::suppress() {
    suppressed_ = true;
    reset_observation(true);
    return request(false);
}

void PopoverModel::dismiss() {
    dismissed_ = true;
    deadline_.reset();
}

void PopoverModel::reset_observation(bool dismiss) {
    observed_ = false;
    dismissed_ = dismiss;
    deadline_.reset();
}

void PopoverModel::observe(bool desire, bool immediate, animation::AnimationTime now, Duration enter, Duration leave) {
    validate_popover_delay(enter);
    validate_popover_delay(leave);
    if (!desire) {
        dismissed_ = false;
    }
    if (desire == observed_) {
        return;
    }
    observed_ = desire;
    deadline_.reset();
    if (dismissed_) {
        return;
    }
    const auto delay = desire && immediate ? Duration{} : desire ? enter : leave;
    const auto micros = static_cast<animation::AnimationTime::rep>(delay.count_milliseconds() * 1000.0);
    if (now.count_microseconds() > std::numeric_limits<animation::AnimationTime::rep>::max() - micros) {
        throw std::overflow_error("Popover deadline overflow");
    }
    pending_ = desire;
    deadline_ = animation::AnimationTime::microseconds(now.count_microseconds() + micros);
}

std::optional<bool> PopoverModel::due(animation::AnimationTime now) {
    if (!deadline_ || now < *deadline_) {
        return {};
    }
    deadline_.reset();
    return pending_;
}
} // namespace ryn::detail
