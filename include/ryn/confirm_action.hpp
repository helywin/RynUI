#pragma once

#include <ryn/async_options.hpp>
#include <chrono>
#include <optional>

namespace ryn {
enum class ConfirmActionDecision { Close, KeepOpen };

enum class ConfirmActionErrorCode { Business, ProviderThrew, DispatchFailed, Timeout };

struct ConfirmActionError final {
    ConfirmActionErrorCode code{ConfirmActionErrorCode::Business};
    String message;
};

struct ConfirmActionResult final {
    ConfirmActionDecision decision{ConfirmActionDecision::Close};
    std::optional<ConfirmActionError> error;
};

struct ConfirmActionRequest final {
    std::uint64_t generation{};
    std::shared_ptr<const std::atomic_bool> cancelled;
};

// UI mutation belongs to the owner thread. A completion may be synchronous,
// repeated, or called by a worker; post must arrange owner-thread delivery.
class ConfirmAction final {
public:
    using Cancel = std::function<void()>;
    using Complete = std::function<void(ConfirmActionResult)>;
    using Provider = std::function<Cancel(ConfirmActionRequest, Complete)>;
    ConfirmAction() = default;
    ConfirmAction(Provider, OwnerThreadPost, std::chrono::milliseconds timeout = std::chrono::seconds(30));
    [[nodiscard]] static ConfirmAction synchronous(std::function<ConfirmActionResult()>);
    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] const Provider& provider() const;
    [[nodiscard]] const OwnerThreadPost& post() const;
    [[nodiscard]] std::chrono::milliseconds timeout() const noexcept;

private:
    std::shared_ptr<const Provider> provider_;
    std::shared_ptr<const OwnerThreadPost> post_;
    std::chrono::milliseconds timeout_{30000};
};
} // namespace ryn
