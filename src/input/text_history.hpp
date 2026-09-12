#pragma once

#include "input/text_selection.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ryn::input {

struct TextHistorySnapshot {
    std::size_t undo_count{}, redo_count{}, payload_bytes{}, storage_bytes{};
    std::uint64_t evictions{}, merges{};
};

// Ring descriptors and a bounded byte arena retain full before/after values.
// No individual transaction owns a heap allocation. Preparation may throw;
// publication and cursor movement cannot allocate or fail.
class TextHistory final {
public:
    static constexpr std::size_t max_transactions = 128;
    static constexpr std::size_t max_payload_bytes = 1024 * 1024;
    void reserve(std::size_t value_bytes);
    void prepare(std::string_view before, TextSelection before_selection,
        std::string_view after, TextSelection after_selection, std::uint64_t epoch, bool merge);
    void commit() noexcept;
    [[nodiscard]] bool read_undo(std::string& value, TextSelection& selection) const;
    [[nodiscard]] bool read_redo(std::string& value, TextSelection& selection) const;
    void commit_undo() noexcept;
    void commit_redo() noexcept;
    void clear() noexcept;
    [[nodiscard]] TextHistorySnapshot snapshot() const noexcept;
    [[nodiscard]] std::size_t retained_capacity() const noexcept;
private:
    struct Entry {
        std::size_t offset{}, before_bytes{}, after_bytes{};
        TextSelection before_selection, after_selection;
        std::uint64_t epoch{};
        [[nodiscard]] std::size_t bytes() const noexcept { return before_bytes + after_bytes; }
    };
    Entry& entry(std::size_t index) noexcept { return entries_[(begin_ + index) % max_transactions]; }
    const Entry& entry(std::size_t index) const noexcept { return entries_[(begin_ + index) % max_transactions]; }
    void ensure_capacity(std::size_t bytes);
    void read_bytes(std::size_t offset, std::size_t bytes, std::string& output) const;
    void copy_bytes(std::size_t offset, std::size_t bytes, char* output) const noexcept;
    void write_bytes(std::string_view text) noexcept;
    void discard_back() noexcept;
    void discard_front() noexcept;
    std::array<Entry, max_transactions> entries_{};
    std::vector<char> arena_;
    std::string pending_before_, pending_after_;
    Entry pending_;
    std::size_t begin_{}, count_{}, cursor_{}, tail_{}, used_{};
    std::uint64_t evictions_{}, merges_{};
    bool pending_merge_{}, prepared_{};
};

} // namespace ryn::input
