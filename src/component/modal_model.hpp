#pragma once
#include <optional>
#include <ryn/modal.hpp>

namespace ryn::detail {
struct ModalOptions final {
    bool open{};
    bool closable{true};
    bool keyboard{true};
    ModalMask mask;
    bool confirm_loading{};
    ModalFooterMode footer{ModalFooterMode::Default};
    float width{520};
    friend bool operator==(const ModalOptions&, const ModalOptions&) = default;
};

class ModalModel final {
public:
    explicit ModalModel(ModalOptions options = {});
    void reconcile(ModalOptions options);
    [[nodiscard]] std::optional<bool> settle_visibility(bool owner_active = true);
    [[nodiscard]] bool request_ok() const noexcept;
    [[nodiscard]] bool request_cancel(ModalCancelReason reason) const;

    [[nodiscard]] const ModalOptions& options() const noexcept {
        return options_;
    }

    [[nodiscard]] bool visible() const noexcept {
        return visible_;
    }

private:
    ModalOptions options_;
    bool visible_{};
};
} // namespace ryn::detail
