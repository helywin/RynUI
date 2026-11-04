#pragma once

#include <ryn/string.hpp>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace ryn {

// The application owns transport and the owner-thread event-loop adapter.
using OwnerThreadPost = std::function<void(std::function<void()>)>;
using AsyncOptionsCancel = std::function<void()>;

struct AsyncOptionsRequest final {
    String query;
    std::uint64_t generation{};
    std::shared_ptr<const std::atomic_bool> cancelled;
};

template <class Option> struct AsyncOptionsResult final {
    std::vector<Option> options;
    String error;
};

template <class Option> class AsyncOptionsSource final {
public:
    using Complete = std::function<void(AsyncOptionsResult<Option>)>;
    using Provider = std::function<AsyncOptionsCancel(AsyncOptionsRequest, Complete)>;
    AsyncOptionsSource() = default;

    AsyncOptionsSource(Provider provider, OwnerThreadPost post)
        : provider_(std::make_shared<const Provider>(std::move(provider))),
          post_(std::make_shared<const OwnerThreadPost>(std::move(post))) {}

    [[nodiscard]] explicit operator bool() const noexcept {
        return provider_ && *provider_ && post_ && *post_;
    }

    [[nodiscard]] const Provider& provider() const {
        return *provider_;
    }

    [[nodiscard]] const OwnerThreadPost& post() const {
        return *post_;
    }

    friend bool operator==(const AsyncOptionsSource&, const AsyncOptionsSource&) = default;

private:
    std::shared_ptr<const Provider> provider_;
    std::shared_ptr<const OwnerThreadPost> post_;
};

} // namespace ryn
