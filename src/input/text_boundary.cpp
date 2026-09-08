#include "input/text_boundary.hpp"

#include <utf8proc.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ryn::input {
TextWordClass text_word_class(char32_t scalar) noexcept {
    const auto category = utf8proc_category(static_cast<utf8proc_int32_t>(scalar));
    if((scalar >= U'\t' && scalar <= U'\r') || scalar == U'\u0085'
            || category == UTF8PROC_CATEGORY_ZS || category == UTF8PROC_CATEGORY_ZL
            || category == UTF8PROC_CATEGORY_ZP) {
        return TextWordClass::whitespace;
    }
    if(category >= UTF8PROC_CATEGORY_LU && category <= UTF8PROC_CATEGORY_PC) {
        return TextWordClass::word;
    }
    if(category >= UTF8PROC_CATEGORY_PD && category <= UTF8PROC_CATEGORY_PO) {
        return TextWordClass::punctuation;
    }
    return TextWordClass::symbol;
}
namespace {
constexpr std::size_t empty_boundary = 0;
std::span<const std::size_t> offsets(const std::vector<std::size_t>& values) noexcept {
    return values.empty() ? std::span<const std::size_t>(&empty_boundary, 1)
                          : std::span<const std::size_t>(values);
}
}

std::optional<TextScalar> Utf8ScalarIterator::next() noexcept {
    if(!valid_ || offset_ == bytes_.size()) {
        return std::nullopt;
    }
    utf8proc_int32_t scalar{};
    // Decode at most four bytes, avoiding size_t -> signed-length overflow.
    const auto count = utf8proc_iterate(
        reinterpret_cast<const utf8proc_uint8_t*>(bytes_.data() + offset_),
        static_cast<utf8proc_ssize_t>(std::min<std::size_t>(bytes_.size() - offset_, 4)),
        &scalar);
    if(count <= 0) {
        valid_ = false;
        return std::nullopt;
    }
    const auto begin = offset_;
    offset_ += static_cast<std::size_t>(count);
    return TextScalar{static_cast<char32_t>(scalar), begin, offset_};
}

bool TextBoundaryMap::assign(std::string_view bytes) {
    pending_scalars_.clear();
    pending_graphemes_.clear();
    pending_graphemes_.push_back(0);
    Utf8ScalarIterator iterator(bytes);
    utf8proc_int32_t state = 0;
    utf8proc_int32_t previous_scalar = 0;
    while(const auto scalar = iterator.next()) {
        if(scalar->byte_begin != 0 && utf8proc_grapheme_break_stateful(
               previous_scalar, static_cast<utf8proc_int32_t>(scalar->value), &state)) {
            pending_graphemes_.push_back(scalar->byte_begin);
        }
        pending_scalars_.push_back(scalar->byte_begin);
        previous_scalar = static_cast<utf8proc_int32_t>(scalar->value);
    }
    if(!iterator.valid()) {
        return false;
    }
    pending_scalars_.push_back(bytes.size());
    if(!bytes.empty()) {
        pending_graphemes_.push_back(bytes.size());
    }
    scalars_.swap(pending_scalars_);
    graphemes_.swap(pending_graphemes_);
    return true;
}

void TextBoundaryMap::reserve(std::size_t scalar_capacity) {
    if(scalar_capacity == std::numeric_limits<std::size_t>::max()) {
        throw std::length_error("Text boundary capacity overflow");
    }
    const auto size = scalar_capacity + 1;
    scalars_.reserve(size);
    graphemes_.reserve(size);
    pending_scalars_.reserve(size);
    pending_graphemes_.reserve(size);
}

void TextBoundaryMap::swap(TextBoundaryMap& other) noexcept {
    // Member swaps also avoid allocating MSVC Debug iterator proxies, which
    // generic std::swap(TextBoundaryMap) would create through temporary vectors.
    scalars_.swap(other.scalars_);
    graphemes_.swap(other.graphemes_);
    pending_scalars_.swap(other.pending_scalars_);
    pending_graphemes_.swap(other.pending_graphemes_);
}

std::span<const std::size_t> TextBoundaryMap::grapheme_bytes() const noexcept {
    return offsets(graphemes_);
}
std::span<const std::size_t> TextBoundaryMap::scalar_bytes() const noexcept {
    return offsets(scalars_);
}
std::size_t TextBoundaryMap::size_bytes() const noexcept { return scalar_bytes().back(); }
std::size_t TextBoundaryMap::scalar_count() const noexcept { return scalar_bytes().size() - 1; }
std::size_t TextBoundaryMap::grapheme_count() const noexcept { return grapheme_bytes().size() - 1; }
std::size_t TextBoundaryMap::retained_capacity() const noexcept {
    return scalars_.capacity() + graphemes_.capacity()
        + pending_scalars_.capacity() + pending_graphemes_.capacity();
}

std::optional<std::size_t> TextBoundaryMap::byte_to_scalar(std::size_t byte) const noexcept {
    const auto values = scalar_bytes();
    const auto found = std::lower_bound(values.begin(), values.end(), byte);
    if(found == values.end() || *found != byte) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - values.begin());
}
std::optional<std::size_t> TextBoundaryMap::scalar_to_byte(std::size_t scalar) const noexcept {
    const auto values = scalar_bytes();
    return scalar < values.size() ? std::optional(values[scalar]) : std::nullopt;
}
bool TextBoundaryMap::is_boundary(std::size_t byte) const noexcept {
    const auto values = grapheme_bytes();
    return std::binary_search(values.begin(), values.end(), byte);
}
std::size_t TextBoundaryMap::floor(std::size_t byte) const noexcept {
    const auto values = grapheme_bytes();
    return *std::prev(std::upper_bound(values.begin(), values.end(), byte));
}
std::size_t TextBoundaryMap::ceil(std::size_t byte) const noexcept {
    const auto values = grapheme_bytes();
    const auto found = std::lower_bound(values.begin(), values.end(), byte);
    return found == values.end() ? values.back() : *found;
}
std::size_t TextBoundaryMap::previous(std::size_t byte) const noexcept {
    const auto values = grapheme_bytes();
    const auto found = std::lower_bound(values.begin(), values.end(), byte);
    return found == values.begin() ? 0 : *std::prev(found);
}
std::size_t TextBoundaryMap::next(std::size_t byte) const noexcept {
    const auto values = grapheme_bytes();
    const auto found = std::upper_bound(values.begin(), values.end(), byte);
    return found == values.end() ? values.back() : *found;
}
std::string_view TextBoundaryMap::unicode_version() noexcept { return utf8proc_unicode_version(); }
std::string_view TextBoundaryMap::dependency_version() noexcept { return utf8proc_version(); }

} // namespace ryn::input
