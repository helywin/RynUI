#pragma once
#include "runtime/component_host.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace ryn::input {
class InteractionRegistry;
struct InputConstraintState;

// A window capability, never just a window-local ComponentId.
class InputRootRef final {
public:
    InputRootRef() = default;
    [[nodiscard]] bool valid() const;

    [[nodiscard]] runtime::ComponentId component() const noexcept {
        return component_;
    }

    friend bool operator==(const InputRootRef&, const InputRootRef&) noexcept;

private:
    friend class InteractionRegistry;
    friend class InputConstraintBatch;
    friend class InputConstraintLease;
    std::weak_ptr<InputConstraintState> window_;
    runtime::ComponentId component_;
};

enum class InputConstraintKind { BlockSubtrees, RestrictToSubtrees };

struct InputConstraintSpec final {
    std::vector<InputRootRef> roots;
    bool consume_unhandled_scroll{};
    friend bool operator==(const InputConstraintSpec&, const InputConstraintSpec&) = default;
};

// Copyable exact-generation handle. The component scope installs quiet release;
// retaining component storage or another copy never extends its authority.
class InputConstraintLease final {
public:
    InputConstraintLease() = default;
    [[nodiscard]] bool valid() const;
    [[nodiscard]] InputRootRef owner() const;
    bool release() noexcept;
    friend bool operator==(const InputConstraintLease&, const InputConstraintLease&) noexcept;

private:
    friend class InteractionRegistry;
    friend class InputConstraintBatch;
    friend class InputConstraintAuthority;
    std::weak_ptr<InputConstraintState> window_;
    std::uint32_t index_{};
    std::uint64_t generation_{};
};

class InputConstraintAuthority final {
public:
    InputConstraintAuthority() = default;
    [[nodiscard]] bool valid() const;
    friend bool operator==(const InputConstraintAuthority&, const InputConstraintAuthority&) noexcept;

private:
    friend class InteractionRegistry;
    friend class InputConstraintBatch;
    InputConstraintLease lease_;
};

class InputCompanionLease final {
public:
    InputCompanionLease() = default;
    [[nodiscard]] bool valid() const;
    bool release() noexcept;

private:
    friend class InteractionRegistry;
    friend class InputConstraintBatch;
    std::weak_ptr<InputConstraintState> window_;
    std::uint32_t index_{};
    std::uint64_t generation_{};
};

struct InputConstraintDiagnostics final {
    std::size_t leases{};
    std::size_t active_leases{};
    std::size_t companions{};
    std::size_t active_companions{};
    std::uint64_t publications{};
};

struct InputConstraintUpdate final {
    InputConstraintLease lease;
    std::optional<InputConstraintSpec> spec; // absent deactivates this lease
};

class InputConstraintBatch final {
public:
    InputConstraintBatch(InputConstraintBatch&&) noexcept;
    InputConstraintBatch& operator=(InputConstraintBatch&&) noexcept;
    ~InputConstraintBatch();
    InputConstraintBatch(const InputConstraintBatch&) = delete;
    InputConstraintBatch& operator=(const InputConstraintBatch&) = delete;
    void replace(const InputConstraintLease&, InputConstraintSpec);
    void deactivate(const InputConstraintLease&);
    void enable(const InputCompanionLease&, bool = true);
    void boundary_view(std::optional<InputConstraintAuthority>);
    // No application callbacks. Failed validation/allocation keeps the old policy.
    bool commit();

private:
    friend class InteractionRegistry;
    struct Impl;
    explicit InputConstraintBatch(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
} // namespace ryn::input
