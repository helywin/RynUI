#include "input/text_history.hpp"

#include <cstring>
#include <stdexcept>

namespace ryn::input {

void TextHistory::copy_bytes(std::size_t offset, std::size_t bytes, char* output) const noexcept {
    if(bytes == 0) return;
    offset %= arena_.size();
    const auto first = std::min(bytes, arena_.size() - offset);
    std::memcpy(output, arena_.data() + offset, first);
    if(first < bytes) std::memcpy(output + first, arena_.data(), bytes - first);
}
void TextHistory::read_bytes(std::size_t offset, std::size_t bytes, std::string& output) const {
    output.resize(bytes);
    copy_bytes(offset, bytes, output.data());
}
void TextHistory::ensure_capacity(std::size_t bytes) {
    if(bytes <= arena_.size()) return;
    if(bytes > max_payload_bytes) throw std::length_error("Text history payload budget exceeded");
    const auto capacity = std::min(max_payload_bytes, std::max(bytes, std::max<std::size_t>(256, arena_.size() * 2)));
    std::vector<char> next(capacity);
    std::size_t offset = 0;
    for(std::size_t index = 0; index < count_; ++index) {
        const auto& record = entry(index);
        copy_bytes(record.offset, record.bytes(), next.data() + offset);
        offset += record.bytes();
    }
    arena_.swap(next);
    offset = 0;
    for(std::size_t index = 0; index < count_; ++index) {
        auto& record = entry(index);
        record.offset = offset;
        offset += record.bytes();
    }
    tail_ = offset % arena_.size();
}
void TextHistory::reserve(std::size_t bytes) {
    const auto budget = bytes >= max_payload_bytes / (2 * max_transactions)
        ? max_payload_bytes : bytes * 2 * max_transactions;
    ensure_capacity(budget);
    pending_before_.reserve(std::min(bytes, max_payload_bytes));
    pending_after_.reserve(std::min(bytes, max_payload_bytes));
}
void TextHistory::prepare(std::string_view before, TextSelection before_selection,
    std::string_view after, TextSelection after_selection, std::uint64_t epoch, bool merge) {
    prepared_ = false;
    const auto* last = cursor_ != 0 ? &entry(cursor_ - 1) : nullptr;
    pending_merge_ = merge && cursor_ == count_ && last && last->epoch == epoch
        && last->after_selection == before_selection;
    const auto before_bytes = pending_merge_ ? last->before_bytes : before.size();
    if(before_bytes > max_payload_bytes || after.size() > max_payload_bytes - before_bytes)
        throw std::length_error("Single text history transaction exceeds budget");
    if(pending_merge_) {
        read_bytes(last->offset, last->before_bytes, pending_before_);
        before_selection = last->before_selection;
    } else pending_before_.assign(before);
    pending_after_.assign(after);
    const auto kept = cursor_ - (pending_merge_ ? 1 : 0);
    std::size_t kept_bytes = 0;
    for(std::size_t index = 0; index < kept; ++index) kept_bytes += entry(index).bytes();
    ensure_capacity(std::min(max_payload_bytes, kept_bytes + before_bytes + after.size()));
    pending_ = {0, before_bytes, after.size(), before_selection, after_selection, epoch};
    prepared_ = true;
}
void TextHistory::discard_back() noexcept {
    if(count_ == 0) return;
    const auto& record = entry(count_ - 1);
    tail_ = record.offset;
    used_ -= record.bytes();
    --count_;
    cursor_ = std::min(cursor_, count_);
}
void TextHistory::discard_front() noexcept {
    if(count_ == 0) return;
    used_ -= entry(0).bytes();
    begin_ = (begin_ + 1) % max_transactions;
    --count_;
    if(cursor_ != 0) --cursor_;
    ++evictions_;
}
void TextHistory::write_bytes(std::string_view text) noexcept {
    if(text.empty()) return;
    const auto first = std::min(text.size(), arena_.size() - tail_);
    std::memcpy(arena_.data() + tail_, text.data(), first);
    if(first < text.size()) std::memcpy(arena_.data(), text.data() + first, text.size() - first);
    tail_ = (tail_ + text.size()) % arena_.size();
}
void TextHistory::commit() noexcept {
    if(!prepared_) return;
    while(count_ > cursor_) discard_back();
    if(pending_merge_) { discard_back(); ++merges_; }
    while(count_ >= max_transactions || pending_.bytes() > arena_.size() - used_) discard_front();
    pending_.offset = tail_;
    write_bytes(pending_before_);
    write_bytes(pending_after_);
    entry(count_) = pending_;
    used_ += pending_.bytes();
    ++count_;
    cursor_ = count_;
    prepared_ = false;
}
bool TextHistory::read_undo(std::string& value, TextSelection& selection) const {
    if(cursor_ == 0) return false;
    const auto& record = entry(cursor_ - 1);
    read_bytes(record.offset, record.before_bytes, value);
    selection = record.before_selection;
    return true;
}
bool TextHistory::read_redo(std::string& value, TextSelection& selection) const {
    if(cursor_ == count_) return false;
    const auto& record = entry(cursor_);
    read_bytes(record.offset + record.before_bytes, record.after_bytes, value);
    selection = record.after_selection;
    return true;
}
void TextHistory::commit_undo() noexcept { if(cursor_ != 0) --cursor_; prepared_ = false; }
void TextHistory::commit_redo() noexcept { if(cursor_ < count_) ++cursor_; prepared_ = false; }
void TextHistory::clear() noexcept {
    begin_ = count_ = cursor_ = tail_ = used_ = 0;
    prepared_ = false;
    pending_before_.clear(); pending_after_.clear();
}
TextHistorySnapshot TextHistory::snapshot() const noexcept {
    return {cursor_, count_ - cursor_, used_, arena_.size(), evictions_, merges_};
}
std::size_t TextHistory::retained_capacity() const noexcept {
    return arena_.capacity() + pending_before_.capacity() + pending_after_.capacity();
}

} // namespace ryn::input
