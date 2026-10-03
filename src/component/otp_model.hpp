#pragma once

#include <ryn/string.hpp>

#include <functional>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace ryn::detail {

using OTPCells = std::vector<String>;
using OTPFormatter = std::function<String(String)>;

struct OTPCandidate final {
    OTPCells cells;
    std::size_t next_index{};
    bool complete_changed{};
    std::uint64_t source_revision{};
    std::uint64_t owner_key{};
};

class OTPModel final {
public:
    explicit OTPModel(std::size_t length = 6, std::string_view initial = {}, OTPFormatter formatter = {});
    OTPModel(const OTPModel&) = delete;
    OTPModel& operator=(const OTPModel&) = delete;

    [[nodiscard]] std::size_t length() const noexcept {
        return length_;
    }

    [[nodiscard]] std::uint64_t revision() const noexcept {
        return revision_;
    }

    [[nodiscard]] OTPCells projection() const;
    [[nodiscard]] String value() const;
    [[nodiscard]] std::size_t first_empty() const noexcept;
    bool reconcile(std::string_view);
    bool set_length(std::size_t);
    [[nodiscard]] std::optional<OTPCandidate> prepare(std::size_t index, std::string_view cell_candidate);
    bool commit(OTPCandidate&);

    bool commit(OTPCandidate&& candidate) {
        return commit(candidate);
    }

    void retire() noexcept;
    static void validate_length(std::size_t);

private:
    void require_owner() const;
    OTPCells source_;
    OTPFormatter formatter_;
    std::size_t length_{};
    std::uint64_t revision_{1};
    std::uint64_t owner_key_{};
    std::thread::id thread_{std::this_thread::get_id()};
    bool retired_{};
};

} // namespace ryn::detail
