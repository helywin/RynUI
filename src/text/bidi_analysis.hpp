#pragma once

#include <ryn/string.hpp>
#include <ryn/text_direction.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace ryn::text {

struct BidiParagraph final {
    std::size_t byte_begin{};
    std::size_t content_end{};
    std::size_t byte_end{};
    std::uint8_t base_level{};
    friend bool operator==(const BidiParagraph&, const BidiParagraph&) = default;
};

struct BidiRun final {
    std::size_t byte_begin{};
    std::size_t byte_end{};
    std::uint8_t level{};

    [[nodiscard]] bool right_to_left() const noexcept {
        return (level & 1) != 0;
    }

    friend bool operator==(const BidiRun&, const BidiRun&) = default;
};

struct ScriptRun final {
    std::size_t byte_begin{};
    std::size_t byte_end{};
    std::uint32_t unicode_tag{};
    friend bool operator==(const ScriptRun&, const ScriptRun&) = default;
};

// Immutable shared Unicode analysis owns its source and library resources.
// Construction/line reordering may allocate; scalar and level queries do not.
class BidiAnalysis final {
public:
    BidiAnalysis() = default;
    [[nodiscard]] bool assign(String source, TextDirection direction = TextDirection::Auto);

    [[nodiscard]] bool assigned() const noexcept {
        return bool(data_);
    }

    [[nodiscard]] StringView source() const noexcept;
    [[nodiscard]] TextDirection direction() const noexcept;
    [[nodiscard]] std::span<const BidiParagraph> paragraphs() const noexcept;
    [[nodiscard]] std::span<const ScriptRun> scripts() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> levels() const noexcept;
    [[nodiscard]] std::optional<std::uint8_t> level_at(std::size_t byte) const noexcept;
    [[nodiscard]] bool scalar_boundary(std::size_t byte) const noexcept;
    // End is exclusive. A nonempty range must stay in one paragraph.
    [[nodiscard]] std::optional<std::vector<BidiRun>> line_runs(std::size_t begin, std::size_t end) const;
    friend bool operator==(const BidiAnalysis&, const BidiAnalysis&) noexcept;

private:
    struct Data;
    std::shared_ptr<const Data> data_;
};

[[nodiscard]] bool valid_text_direction(TextDirection) noexcept;

} // namespace ryn::text
