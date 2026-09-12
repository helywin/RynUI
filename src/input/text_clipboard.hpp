#pragma once

#include <ryn/string.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace ryn::input {

inline constexpr std::size_t clipboard_max_bytes = 1024 * 1024;
enum class ClipboardError {
    none, no_text, platform_failure, invalid_utf8, embedded_null,
    capacity_exceeded, allocation_failure, wrong_thread,
};
struct ClipboardReadResult {
    ClipboardError error{ClipboardError::none};
    std::optional<String> text;
    [[nodiscard]] explicit operator bool() const noexcept {
        return error == ClipboardError::none && text.has_value();
    }
};
struct ClipboardAvailability {
    ClipboardError error{ClipboardError::none};
    bool available{};
};
class TextClipboard {
public:
    virtual ~TextClipboard() = default;
    [[nodiscard]] virtual ClipboardReadResult read_text() = 0;
    [[nodiscard]] virtual ClipboardError write_text(StringView) = 0;
    [[nodiscard]] virtual ClipboardAvailability has_text() const noexcept = 0;
};

// Metadata is a snapshot, not a promise that clipboard contents remain the
// same when the next command runs. Text is always fetched at command time.
struct ClipboardChanged {
    bool owned{};
    std::uint32_t format_count{};
    friend bool operator==(const ClipboardChanged&, const ClipboardChanged&) = default;
};
[[nodiscard]] inline bool is_valid(const ClipboardChanged& event) noexcept {
    return event.format_count <= 256;
}

} // namespace ryn::input
