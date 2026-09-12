#include "component/input_display.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
void InputDisplayState::reserve(std::size_t bytes) {
    text_.reserve(bytes); pending_.reserve(bytes);
    boundaries_.reserve(bytes); pending_boundaries_.reserve(bytes);
    composition_boundaries_.reserve(bytes); pending_composition_boundaries_.reserve(bytes);
}
InputDisplayUpdate InputDisplayState::update(const input::TextEditorState& editor, StringView placeholder) {
    const auto value = editor.value();
    const auto composition = editor.composition();
    const bool is_placeholder = value.empty() && !composition.active;
    const auto replacement = composition.active ? composition.replacement : input::TextSelection{};
    const auto inserted_size = composition.active ? composition.text.size() : 0;
    bool preedit_changed = composition.active && (!composing_ || replacement_ != replacement
        || inserted_size_ != inserted_size
        || std::string_view{text_}.substr(replacement_.begin(), inserted_size_) != composition.text);
    const auto size_without_selection = value.size() - (composition.active ? replacement.end() - replacement.begin() : 0);
    if(inserted_size > std::numeric_limits<std::size_t>::max() - size_without_selection)
        throw std::length_error("Input display size overflow");
    pending_.clear();
    if(is_placeholder) pending_.append(placeholder.bytes());
    else if(composition.active) {
        pending_.reserve(size_without_selection + inserted_size);
        pending_.append(value.substr(0, replacement.begin()));
        pending_.append(composition.text);
        pending_.append(value.substr(replacement.end()));
    } else pending_.append(value);
    const bool text_changed = !revision_ || pending_ != text_;
    if(text_changed && revision_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Input display revision exhausted");
    if(text_changed && !pending_boundaries_.assign(pending_)) throw std::logic_error("Invalid Input display UTF-8");
    if(preedit_changed && !pending_composition_boundaries_.assign(composition.text)) throw std::logic_error("Invalid preedit UTF-8");
    const auto& boundaries = text_changed ? pending_boundaries_ : boundaries_;
    const auto& preedit = preedit_changed ? pending_composition_boundaries_ : composition_boundaries_;
    auto selection = is_placeholder ? input::TextSelection{} : editor.selection();
    input::TextSelection underline;
    if(composition.active) {
        const auto start = replacement.begin();
        underline = {boundaries.floor(start), boundaries.ceil(start + inserted_size)};
        if(composition.selection.known) {
            const auto begin = start + preedit.scalar_to_byte(composition.selection.start).value();
            const auto end = start + preedit.scalar_to_byte(composition.selection.start + composition.selection.length).value();
            selection = composition.selection.length == 0
                ? input::TextSelection{boundaries.ceil(begin), boundaries.ceil(begin)}
                : input::TextSelection{boundaries.floor(begin), boundaries.ceil(end)};
        } else selection = {boundaries.ceil(start + inserted_size), boundaries.ceil(start + inserted_size)};
    }
    const bool geometry_changed = text_changed || selection != selection_ || underline != composition_
        || placeholder_ != is_placeholder || composing_ != composition.active;
    if(text_changed) { text_.swap(pending_); boundaries_.swap(pending_boundaries_); ++revision_; }
    if(preedit_changed) composition_boundaries_.swap(pending_composition_boundaries_);
    selection_ = selection; caret_ = selection.caret; composition_ = underline;
    replacement_ = replacement; committed_size_ = value.size(); inserted_size_ = inserted_size;
    composing_ = composition.active; placeholder_ = is_placeholder;
    return {text_changed, geometry_changed};
}
InputDisplaySnapshot InputDisplayState::snapshot() const noexcept {
    return {text_, selection_, composition_, caret_, placeholder_, composing_};
}
std::size_t InputDisplayState::committed_to_display(std::size_t byte, bool trailing) const noexcept {
    byte = std::min(byte, committed_size_);
    if(placeholder_) return 0;
    if(!composing_ || byte < replacement_.begin()) return byte;
    if(replacement_.empty() && byte == replacement_.begin()) return byte + (trailing ? inserted_size_ : 0);
    if(byte == replacement_.begin()) return byte;
    if(byte >= replacement_.end()) return replacement_.begin() + inserted_size_ + (byte - replacement_.end());
    return replacement_.begin() + (trailing ? inserted_size_ : 0);
}
std::size_t InputDisplayState::display_to_committed(std::size_t byte, bool trailing) const noexcept {
    byte = std::min(byte, text_.size());
    if(placeholder_) return 0;
    if(!composing_ || byte <= replacement_.begin()) return std::min(byte, committed_size_);
    if(byte >= replacement_.begin() + inserted_size_) return replacement_.end() + (byte - replacement_.begin() - inserted_size_);
    return trailing ? replacement_.end() : replacement_.begin();
}
std::optional<float> InputDisplayState::scroll_for_caret(const text::TextCaretMap& map, std::uint64_t revision,
    float viewport_width, float previous_offset, float caret_width) const noexcept {
    if(!std::isfinite(viewport_width) || viewport_width < 0 || !std::isfinite(previous_offset)
        || !std::isfinite(caret_width) || caret_width < 0) return {};
    const auto caret = map.at(caret_, revision);
    if(!caret || map.stops().empty()) return {};
    if(placeholder_ || viewport_width == 0) return 0;
    const auto maximum = std::max(0.0F, map.stops().back().x + caret_width - viewport_width);
    auto offset = std::clamp(previous_offset, 0.0F, maximum);
    if(caret->x < offset) offset = caret->x;
    if(caret->x + caret_width > offset + viewport_width)
        offset = std::max(0.0F, caret->x + std::min(caret_width, viewport_width) - viewport_width);
    return std::clamp(offset, 0.0F, maximum);
}
} // namespace ryn::detail
