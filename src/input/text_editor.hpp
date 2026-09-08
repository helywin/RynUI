#pragma once

#include "input/text_boundary.hpp"
#include "input/text_input_owner.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace ryn::input {

struct TextSelection final {
    std::size_t anchor{};
    std::size_t caret{};
    [[nodiscard]] std::size_t begin() const noexcept { return std::min(anchor, caret); }
    [[nodiscard]] std::size_t end() const noexcept { return std::max(anchor, caret); }
    [[nodiscard]] bool empty() const noexcept { return anchor == caret; }
    friend bool operator==(TextSelection, TextSelection) = default;
};

enum class TextEditError {
    none, invalid_utf8, invalid_range, disabled, read_only,
    capacity_exceeded, allocation_failure, revision_exhausted,
};

struct TextEditResult final {
    TextEditError error{TextEditError::none};
    bool value_changed{};
    bool selection_changed{};
    bool truncated{};
    [[nodiscard]] explicit operator bool() const noexcept { return error == TextEditError::none; }
};

enum class TextCaretMove { left, right, home, end };

struct TextEditorLimits final {
    std::size_t max_scalars{std::numeric_limits<std::size_t>::max()};
    std::size_t max_bytes{std::numeric_limits<std::size_t>::max()};
};

struct TextEditorDiagnostics final {
    std::uint64_t mutations{};
    std::uint64_t selections{};
    std::uint64_t rejected{};
    std::uint64_t truncated{};
};

// Owned by TextEditorStore. Never retain a reference across destroy; delayed
// work resolves TextInputOwnerId again through the store before dispatching.
class TextEditorState final {
public:
    TextEditorState(const TextEditorState&) = delete;
    TextEditorState& operator=(const TextEditorState&) = delete;

    [[nodiscard]] TextInputOwnerId id() const noexcept { return id_; }
    [[nodiscard]] std::string_view value() const;
    [[nodiscard]] TextSelection selection() const;
    [[nodiscard]] const TextBoundaryMap& boundaries() const;
    [[nodiscard]] std::uint64_t revision() const;
    [[nodiscard]] const TextEditorDiagnostics& diagnostics() const;
    [[nodiscard]] std::size_t retained_capacity() const;
    [[nodiscard]] TextEditorLimits limits() const;
    [[nodiscard]] bool disabled() const;
    [[nodiscard]] bool read_only() const;
    void set_eligibility(bool disabled, bool read_only);
    void reserve(std::size_t bytes);

    // Authoritative value replacement bypasses edit eligibility, but uses the
    // same normalization/limits/atomic publication as a committed edit.
    [[nodiscard]] TextEditResult set_value(std::string_view text);
    [[nodiscard]] TextEditResult set_limits(TextEditorLimits limits);
    [[nodiscard]] TextEditResult replace_selection(std::string_view text);
    [[nodiscard]] TextEditResult erase_backward();
    [[nodiscard]] TextEditResult erase_forward();
    [[nodiscard]] TextEditResult select(TextSelection selection);
    [[nodiscard]] TextEditResult place(std::size_t byte, bool extend = false);
    [[nodiscard]] TextEditResult move(TextCaretMove direction, bool extend = false);
    [[nodiscard]] TextEditResult select_all();
    // Same-class runs for letters/marks/numbers/connectors, whitespace and
    // punctuation; symbols (including emoji) select one whole grapheme.
    [[nodiscard]] TextEditResult select_word(std::size_t byte);

private:
    friend class TextEditorStore;
    TextEditorState(TextInputOwnerId id, std::string_view initial, TextEditorLimits limits);
    void ensure_owner_thread() const;
    [[nodiscard]] TextEditResult reject(TextEditError error);
    [[nodiscard]] TextEditResult replace(TextSelection range, std::string_view text, bool authoritative);
    [[nodiscard]] TextEditResult publish_selection(TextSelection selection);
    [[nodiscard]] TextWordClass word_class_at(std::size_t byte) const noexcept;

    TextInputOwnerId id_;
    std::thread::id owner_thread_{std::this_thread::get_id()};
    std::string value_;
    std::string pending_value_;
    std::string normalized_;
    TextBoundaryMap boundaries_;
    TextBoundaryMap pending_boundaries_;
    TextBoundaryMap inserted_boundaries_;
    TextSelection selection_;
    TextEditorLimits limits_;
    TextEditorDiagnostics diagnostics_;
    std::uint64_t revision_{};
    bool disabled_{};
    bool read_only_{};
};

class TextEditorStore final {
public:
    void reserve(std::size_t owners);
    [[nodiscard]] TextInputOwnerId create(std::string_view initial = {}, TextEditorLimits limits = {});
    bool destroy(TextInputOwnerId id);
    [[nodiscard]] TextEditorState* find(TextInputOwnerId id);
    [[nodiscard]] const TextEditorState* find(TextInputOwnerId id) const;
    [[nodiscard]] TextEditorState& require(TextInputOwnerId id);
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t capacity() const;
    [[nodiscard]] bool is_owner_thread() const noexcept;
private:
    void ensure_owner_thread() const;
    struct Slot final {
        std::unique_ptr<TextEditorState> state;
        std::uint32_t generation{1};
    };
    std::thread::id owner_thread_{std::this_thread::get_id()};
    std::vector<Slot> slots_;
    std::size_t size_{};
};

} // namespace ryn::input
