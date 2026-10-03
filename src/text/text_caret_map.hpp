#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ryn::text {
struct ShapedText;
struct TextMeasurement;

enum class TextCaretAffinity { Upstream, Downstream };

struct TextCaretStop {
    std::size_t byte{};
    std::size_t glyph_begin{};
    std::size_t glyph_count{};
    float x{};
    float baseline{};
    std::size_t line{};
    friend bool operator==(const TextCaretStop&, const TextCaretStop&) = default;
};

// Logical LTR/CJK maps. Unicode graphemes define legal
// stops; shaped clusters define geometry, never editing validity.
class TextCaretMap final {
public:
    void reserve(std::size_t stops, std::size_t glyphs);
    // Failure leaves the last published map intact. Allocation errors propagate.
    [[nodiscard]] bool assign(const ShapedText&, std::span<const std::size_t> graphemes, std::uint64_t revision,
                              float baseline);
    [[nodiscard]] bool assign(const ShapedText&, const TextMeasurement&, std::span<const std::size_t> graphemes,
                              std::uint64_t revision);

    [[nodiscard]] std::span<const TextCaretStop> stops() const noexcept {
        return stops_;
    }

    [[nodiscard]] std::uint64_t revision() const noexcept {
        return revision_;
    }

    [[nodiscard]] std::optional<TextCaretStop>
    at(std::size_t byte, std::uint64_t revision,
       TextCaretAffinity affinity = TextCaretAffinity::Downstream) const noexcept;
    // Equidistant/duplicate positions choose the earliest logical boundary.
    [[nodiscard]] std::optional<TextCaretStop> nearest(float x, std::uint64_t revision) const noexcept;
    [[nodiscard]] std::optional<TextCaretStop> nearest(float x, float y, std::uint64_t revision) const noexcept;
    [[nodiscard]] std::optional<TextCaretStop> nearest_on_line(float x, std::size_t line,
                                                               std::uint64_t revision) const noexcept;
    [[nodiscard]] std::optional<TextCaretStop> line_edge(std::size_t line, bool end,
                                                         std::uint64_t revision) const noexcept;
    [[nodiscard]] std::optional<TextCaretStop> adjacent_line(TextCaretStop current, int delta, float preferred_x,
                                                             std::uint64_t revision) const noexcept;

    [[nodiscard]] std::size_t line_count() const noexcept {
        return lines_.size();
    }

    [[nodiscard]] std::span<const TextCaretStop> line_stops(std::size_t line) const noexcept;

private:
    struct Cluster {
        std::size_t byte_begin{};
        std::size_t byte_end{};
        std::size_t glyph_begin{};
        std::size_t glyph_count{};
        float x{};
        float advance{};
    };

    struct Line {
        std::size_t stop_begin{};
        std::size_t stop_count{};
        float top{};
        float height{};
    };

    std::vector<TextCaretStop> stops_;
    std::vector<TextCaretStop> pending_;
    std::vector<Cluster> clusters_;
    std::vector<Line> lines_;
    std::vector<Line> pending_lines_;
    std::uint64_t revision_{};
};
} // namespace ryn::text
