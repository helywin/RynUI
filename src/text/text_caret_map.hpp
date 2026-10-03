#pragma once

#include "text/text_engine.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <optional>
#include <span>
#include <utility>
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
    TextCaretAffinity affinity{TextCaretAffinity::Downstream};
    friend bool operator==(const TextCaretStop&, const TextCaretStop&) = default;
};

struct TextCoverageSegment final {
    std::size_t line{};
    float x{};
    float width{};
    friend bool operator==(const TextCoverageSegment&, const TextCoverageSegment&) = default;
};

// Logical byte/affinity and visual line/x indices share measured geometry.
// Unicode graphemes define editing stops; shaper clusters define coverage.
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
    [[nodiscard]] std::optional<TextCaretStop> adjacent_visual(TextCaretStop current, int delta,
                                                               std::uint64_t revision) const noexcept;
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> line_bytes(std::size_t line,
                                                                                std::uint64_t revision) const noexcept;

    // Range endpoints must be legal logical boundaries. Visitor receives
    // merged visual pieces in line/x order; gaps remain separate. No allocation.
    template <class Visitor>
    bool visit_coverage(std::size_t begin, std::size_t end, std::uint64_t revision, Visitor&& visitor) const {
        if (end < begin || !at(begin, revision) || !at(end, revision)) {
            return false;
        }
        std::optional<TextCoverageSegment> merged;
        for (const auto& piece : coverage_) {
            if (begin == end || begin >= piece.byte_end || end <= piece.byte_begin || piece.width == 0) {
                continue;
            }
            if (merged && merged->line == piece.line && piece.x <= merged->x + merged->width + 0.0001F) {
                merged->width = std::max(merged->width, piece.x + piece.width - merged->x);
            } else {
                if (merged) {
                    visitor(*merged);
                }
                merged = TextCoverageSegment{piece.line, piece.x, piece.width};
            }
        }
        if (merged) {
            visitor(*merged);
        }
        return true;
    }

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
        std::uint8_t level{};
    };

    struct Line {
        std::size_t stop_begin{};
        std::size_t stop_count{};
        float top{};
        float height{};
        std::size_t byte_begin{};
        std::size_t byte_end{};
        std::uint8_t base_level{};
    };

    struct Coverage {
        std::size_t byte_begin{};
        std::size_t byte_end{};
        std::size_t line{};
        float x{};
        float width{};
    };

    std::vector<TextCaretStop> stops_;
    std::vector<TextCaretStop> pending_;
    std::vector<TextCaretStop> visual_;
    std::vector<TextCaretStop> pending_visual_;
    std::vector<Cluster> clusters_;
    std::vector<Coverage> coverage_;
    std::vector<Coverage> pending_coverage_;
    std::vector<bool> used_glyphs_;
    std::vector<Line> lines_;
    std::vector<Line> pending_lines_;
    std::uint64_t revision_{};
    // Allocated before fault-injected assign; preparation never constructs a
    // noexcept Debug STL container while allocation failure is enabled.
    TextMeasurement unwrapped_;
};
} // namespace ryn::text
