#pragma once

#include "input/text_input_owner.hpp"
#include <ryn/string.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ryn::input {

struct TextInputSessionStamp {
    TextInputOwnerId owner;
    std::uint64_t epoch{};
    [[nodiscard]] bool valid() const noexcept { return owner.valid() && epoch != 0; }
    friend bool operator==(const TextInputSessionStamp&, const TextInputSessionStamp&) = default;
};

// Offsets count Unicode scalars, never bytes. An unspecified range is canonical
// {0, 0, false}; it is not a selection at the start of the composition.
struct TextScalarRange {
    std::size_t start{};
    std::size_t length{};
    bool known{true};
    friend bool operator==(const TextScalarRange&, const TextScalarRange&) = default;
};

enum class CandidateOrientation : std::uint8_t { vertical, horizontal };

inline constexpr std::size_t text_event_max_bytes = 1024 * 1024;
inline constexpr std::size_t text_event_max_candidates = 128;

struct TextCommitted {
    String text;
    TextInputSessionStamp session;
    friend bool operator==(const TextCommitted&, const TextCommitted&) = default;
};

struct CompositionChanged {
    String text;
    TextScalarRange selection;
    TextInputSessionStamp session;
    friend bool operator==(const CompositionChanged&, const CompositionChanged&) = default;
};

struct CandidatesChanged {
    std::vector<String> candidates;
    std::optional<std::size_t> selected;
    CandidateOrientation orientation{CandidateOrientation::vertical};
    TextInputSessionStamp session;
    friend bool operator==(const CandidatesChanged&, const CandidatesChanged&) = default;
};

[[nodiscard]] bool is_valid(const TextCommitted&) noexcept;
[[nodiscard]] bool is_valid(const CompositionChanged&) noexcept;
[[nodiscard]] bool is_valid(const CandidatesChanged&) noexcept;
[[nodiscard]] std::size_t payload_bytes(const CandidatesChanged&) noexcept;

} // namespace ryn::input
