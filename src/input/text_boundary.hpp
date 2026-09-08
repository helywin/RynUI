#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ryn::input {

enum class TextWordClass { word, whitespace, punctuation, symbol };
[[nodiscard]] TextWordClass text_word_class(char32_t scalar) noexcept;

struct TextScalar {
    char32_t value{};
    std::size_t byte_begin{};
    std::size_t byte_end{};
};

// Internal byte input deliberately admits invalid UTF-8 for platform validation.
// No upstream types or headers cross this boundary.
class Utf8ScalarIterator final {
public:
    explicit Utf8ScalarIterator(std::string_view bytes) noexcept : bytes_(bytes) {}
    [[nodiscard]] std::optional<TextScalar> next() noexcept;
    [[nodiscard]] bool valid() const noexcept { return valid_; }
    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }
private:
    std::string_view bytes_;
    std::size_t offset_{};
    bool valid_{true};
};

class TextBoundaryMap final {
public:
    // Failed validation leaves the preceding map intact. Allocation failure
    // propagates without publishing a partial map.
    [[nodiscard]] bool assign(std::string_view bytes);
    void reserve(std::size_t scalar_capacity);
    void swap(TextBoundaryMap& other) noexcept;
    [[nodiscard]] std::span<const std::size_t> grapheme_bytes() const noexcept;
    [[nodiscard]] std::span<const std::size_t> scalar_bytes() const noexcept;
    [[nodiscard]] std::size_t size_bytes() const noexcept;
    [[nodiscard]] std::size_t scalar_count() const noexcept;
    [[nodiscard]] std::size_t grapheme_count() const noexcept;
    [[nodiscard]] std::size_t retained_capacity() const noexcept;
    [[nodiscard]] std::optional<std::size_t> byte_to_scalar(std::size_t byte) const noexcept;
    [[nodiscard]] std::optional<std::size_t> scalar_to_byte(std::size_t scalar) const noexcept;
    [[nodiscard]] bool is_boundary(std::size_t byte) const noexcept;
    [[nodiscard]] std::size_t floor(std::size_t byte) const noexcept;
    [[nodiscard]] std::size_t ceil(std::size_t byte) const noexcept;
    [[nodiscard]] std::size_t previous(std::size_t byte) const noexcept;
    [[nodiscard]] std::size_t next(std::size_t byte) const noexcept;
    [[nodiscard]] static std::string_view unicode_version() noexcept;
    [[nodiscard]] static std::string_view dependency_version() noexcept;
private:
    // Sentinel-only default map represents empty text without allocating.
    std::vector<std::size_t> scalars_;
    std::vector<std::size_t> graphemes_;
    std::vector<std::size_t> pending_scalars_;
    std::vector<std::size_t> pending_graphemes_;
};

} // namespace ryn::input
