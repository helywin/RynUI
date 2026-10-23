#include "component/popconfirm_model.hpp"

namespace ryn::detail {
void PopconfirmModel::reconcile(bool open) {
    open_ = open;
    suppressed_ = false;
    requested_.reset();
}

bool PopconfirmModel::request(bool open) {
    const bool changed = requested_.value_or(this->open()) != open;
    requested_ = open;
    if (open) {
        suppressed_ = false;
    }
    if (!controlled_) {
        open_ = open;
    }
    return changed;
}

bool PopconfirmModel::suppress() {
    // Suppression must happen even when an earlier controlled close request was
    // ignored. Request deduplication cannot bypass lifecycle cleanup.
    suppressed_ = true;
    const bool notify = open_ && requested_ != false;
    requested_ = false;
    if (!controlled_) {
        open_ = false;
    }
    return notify;
}
} // namespace ryn::detail
