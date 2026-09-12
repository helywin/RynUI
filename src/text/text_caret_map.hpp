#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ryn::text {
struct ShapedText;

struct TextCaretStop {
    std::size_t byte{}, glyph_begin{}, glyph_count{};
    float x{}, baseline{};
    friend bool operator==(const TextCaretStop&, const TextCaretStop&) = default;
};

// Single-line logical LTR/CJK foundation. Unicode graphemes define legal
// stops; shaped clusters define geometry, never editing validity.
class TextCaretMap final {
public:
    void reserve(std::size_t stops, std::size_t glyphs);
    // Failure leaves the last published map intact. Allocation errors propagate.
    [[nodiscard]] bool assign(const ShapedText&, std::span<const std::size_t> graphemes,
        std::uint64_t revision, float baseline);
    [[nodiscard]] std::span<const TextCaretStop> stops() const noexcept { return stops_; }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] std::optional<TextCaretStop> at(std::size_t byte, std::uint64_t revision) const noexcept;
    // Equidistant/duplicate positions choose the earliest logical boundary.
    [[nodiscard]] std::optional<TextCaretStop> nearest(float x, std::uint64_t revision) const noexcept;
private:
    struct Cluster {
        std::size_t byte_begin{}, byte_end{}, glyph_begin{}, glyph_count{};
        float x{}, advance{};
    };
    std::vector<TextCaretStop> stops_, pending_;
    std::vector<Cluster> clusters_;
    std::uint64_t revision_{};
};
} // namespace ryn::text
