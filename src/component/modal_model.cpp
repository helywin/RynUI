#include "component/modal_model.hpp"
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
namespace {
void validate(const ModalOptions& options) {
    if (!std::isfinite(options.width) || options.width <= 0) {
        throw std::invalid_argument("Modal width must be positive and finite");
    }
    if (options.footer != ModalFooterMode::Default && options.footer != ModalFooterMode::None) {
        throw std::invalid_argument("Modal footer mode is invalid");
    }
}
} // namespace

ModalModel::ModalModel(ModalOptions options) {
    reconcile(options);
}

void ModalModel::reconcile(ModalOptions options) {
    validate(options);
    options_ = options;
}

std::optional<bool> ModalModel::settle_visibility(bool owner_active) {
    const bool next = options_.open && owner_active;
    if (visible_ == next) {
        return {};
    }
    visible_ = next;
    return visible_;
}

bool ModalModel::request_ok() const noexcept {
    return visible_ && options_.open && options_.footer == ModalFooterMode::Default && !options_.confirm_loading;
}

bool ModalModel::request_cancel(ModalCancelReason reason) const {
    bool allowed{};
    switch (reason) {
    case ModalCancelReason::CancelButton:
        allowed = options_.footer == ModalFooterMode::Default;
        break;
    case ModalCancelReason::CloseButton:
        allowed = options_.closable;
        break;
    case ModalCancelReason::Mask:
        allowed = options_.mask.closable;
        break;
    case ModalCancelReason::Escape:
        allowed = options_.keyboard;
        break;
    default:
        throw std::invalid_argument("Modal cancel reason is invalid");
    }
    return visible_ && options_.open && allowed;
}
} // namespace ryn::detail
