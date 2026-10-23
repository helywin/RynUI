#include "component/modal_model.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::detail;

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void controlled_visibility() {
    ModalModel model;
    check(!model.visible() && !model.settle_visibility() && !model.request_ok() &&
              !model.request_cancel(ModalCancelReason::Escape),
          "hidden Modal emitted an initial transition or request");
    auto options = model.options();
    options.open = true;
    model.reconcile(options);
    check(!model.visible() && !model.settle_visibility(false), "inactive owner opened Modal");
    check(model.settle_visibility() == std::optional<bool>{true} && !model.settle_visibility(),
          "Modal initial open transition repeated");
    for (int i = 0; i < 2; ++i) {
        check(model.request_ok() && model.request_cancel(ModalCancelReason::CancelButton) && model.visible() &&
                  model.options().open,
              "Modal request committed a refused close");
    }
    options.open = false;
    model.reconcile(options);
    check(!model.request_ok() && !model.request_cancel(ModalCancelReason::Mask),
          "pending close accepted stale activation");
    check(model.settle_visibility() == std::optional<bool>{false} && !model.visible() && !model.settle_visibility(),
          "Modal close transition was lost or repeated");
    options.open = true;
    ModalModel initial{options};
    check(!initial.visible() && initial.settle_visibility() == std::optional<bool>{true},
          "initial-open Modal became visible before content preparation");
    check(initial.settle_visibility(false) == std::optional<bool>{false} && initial.options().open &&
              initial.settle_visibility() == std::optional<bool>{true},
          "Modal owner hide did not preserve controlled open");
}

void cancellation_and_loading() {
    ModalOptions options;
    options.open = true;
    ModalModel model{options};
    static_cast<void>(model.settle_visibility());
    for (auto reason : {ModalCancelReason::CancelButton, ModalCancelReason::CloseButton, ModalCancelReason::Mask,
                        ModalCancelReason::Escape}) {
        check(model.request_cancel(reason), "default Modal rejected a cancellation path");
    }
    options.confirm_loading = true;
    model.reconcile(options);
    check(!model.request_ok() && model.request_cancel(ModalCancelReason::CancelButton) &&
              model.request_cancel(ModalCancelReason::Escape),
          "confirmLoading blocked cancellation or repeated OK");
    options.closable = false;
    options.keyboard = false;
    options.mask = {false, false};
    options.footer = ModalFooterMode::None;
    model.reconcile(options);
    check(!model.request_cancel(ModalCancelReason::CloseButton) && !model.request_cancel(ModalCancelReason::Escape) &&
              !model.request_cancel(ModalCancelReason::Mask) && !model.request_cancel(ModalCancelReason::CancelButton),
          "disabled cancellation path emitted a request");
    options.mask.closable = true;
    options.confirm_loading = false;
    model.reconcile(options);
    check(model.request_cancel(ModalCancelReason::Mask) && !model.request_ok(),
          "transparent mask lost outside cancellation or hidden default footer accepted OK");
    bool rejected{};
    try {
        static_cast<void>(model.request_cancel(static_cast<ModalCancelReason>(99)));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && model.visible(), "Modal accepted unknown cancel reason");
}

void invalid_updates_are_atomic() {
    ModalModel model{ModalOptions{.open = true}};
    static_cast<void>(model.settle_visibility());
    const auto original = model.options();
    for (float invalid :
         {0.0F, -1.0F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        auto next = original;
        next.open = false;
        next.width = invalid;
        bool rejected{};
        try {
            model.reconcile(next);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && model.options() == original && model.visible() && !model.settle_visibility(),
              "invalid Modal width partially changed visibility/options");
    }
    auto next = original;
    next.footer = static_cast<ModalFooterMode>(99);
    bool rejected{};
    try {
        model.reconcile(next);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && model.options() == original, "invalid footer changed Modal model");
}
} // namespace

int main() {
    try {
        controlled_visibility();
        cancellation_and_loading();
        invalid_updates_are_atomic();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
