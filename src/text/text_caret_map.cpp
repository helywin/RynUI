#include "text/text_caret_map.hpp"
#include "text/text_engine.hpp"
#include "input/text_boundary.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::text {
void TextCaretMap::reserve(std::size_t stops, std::size_t glyphs) {
    stops_.reserve(stops); pending_.reserve(stops); clusters_.reserve(glyphs);
}
bool TextCaretMap::assign(const ShapedText& shaped, std::span<const std::size_t> boundaries,
    std::uint64_t revision, float baseline) {
    if(!revision || !std::isfinite(baseline) || boundaries.empty() || boundaries.front() != 0
        || boundaries.back() != shaped.normalized_size_bytes || shaped.paragraphs.size() > 1) return false;
    for(std::size_t i = 1; i < boundaries.size(); ++i)
        if(boundaries[i - 1] >= boundaries[i]) return false;
    for(const auto& run : shaped.runs) if(run.right_to_left) return false;
    clusters_.clear();
    float advance{};
    for(std::size_t i = 0; i < shaped.glyphs.size(); ++i) {
        const auto& glyph = shaped.glyphs[i];
        if(glyph.cluster >= shaped.normalized_size_bytes || !std::isfinite(glyph.advance_x)
            || glyph.advance_x < 0 || (!clusters_.empty() && glyph.cluster < clusters_.back().byte_begin)) return false;
        if(clusters_.empty() || clusters_.back().byte_begin != glyph.cluster) {
            if(!clusters_.empty()) clusters_.back().byte_end = glyph.cluster;
            clusters_.push_back({glyph.cluster, shaped.normalized_size_bytes, i, 0, advance, 0});
        }
        auto& cluster = clusters_.back();
        ++cluster.glyph_count; cluster.advance += glyph.advance_x; advance += glyph.advance_x;
        if(!std::isfinite(advance)) return false;
    }
    pending_.clear(); pending_.reserve(boundaries.size());
    std::size_t cluster_index{}, range_index{};
    for(std::size_t i = 0; i < boundaries.size(); ++i) {
        const auto byte = boundaries[i];
        TextCaretStop stop{byte, shaped.glyphs.size(), 0, 0, baseline};
        if(byte == shaped.normalized_size_bytes) stop.x = advance;
        else if(!clusters_.empty() && byte >= clusters_.front().byte_begin) {
            while(cluster_index + 1 < clusters_.size() && clusters_[cluster_index + 1].byte_begin <= byte) ++cluster_index;
            const auto& cluster = clusters_[cluster_index];
            const auto first = std::upper_bound(boundaries.begin(), boundaries.end(), cluster.byte_begin);
            const auto last = std::lower_bound(first, boundaries.end(), cluster.byte_end);
            const auto position = std::lower_bound(first, last, byte);
            const auto pieces = static_cast<float>(last - first + 1);
            const auto fraction = byte == cluster.byte_begin ? 0.0F
                : static_cast<float>(position - first + 1) / pieces;
            stop.x = cluster.x + cluster.advance * fraction;
        }
        if(i + 1 < boundaries.size()) {
            while(range_index < clusters_.size() && clusters_[range_index].byte_end <= byte) ++range_index;
            auto last = range_index;
            while(last < clusters_.size() && clusters_[last].byte_begin < boundaries[i + 1]) ++last;
            if(last > range_index) {
                stop.glyph_begin = clusters_[range_index].glyph_begin;
                const auto& end = clusters_[last - 1];
                stop.glyph_count = end.glyph_begin + end.glyph_count - stop.glyph_begin;
            }
        }
        if(!pending_.empty() && stop.x < pending_.back().x) return false;
        pending_.push_back(stop);
    }
    stops_.swap(pending_); revision_ = revision;
    return true;
}
std::optional<TextCaretStop> TextCaretMap::at(std::size_t byte, std::uint64_t revision) const noexcept {
    if(!revision || revision != revision_) return {};
    const auto found = std::lower_bound(stops_.begin(), stops_.end(), byte,
        [](const auto& stop, auto value) { return stop.byte < value; });
    if(found == stops_.end() || found->byte != byte) return {};
    return *found;
}
std::optional<TextCaretStop> TextCaretMap::nearest(float x, std::uint64_t revision) const noexcept {
    if(!revision || revision != revision_ || stops_.empty() || !std::isfinite(x)) return {};
    auto right = std::lower_bound(stops_.begin(), stops_.end(), x,
        [](const auto& stop, auto value) { return stop.x < value; });
    if(right == stops_.begin()) return *right;
    auto chosen = right;
    if(right == stops_.end() || x - (right - 1)->x <= right->x - x) chosen = right - 1;
    chosen = std::lower_bound(stops_.begin(), chosen + 1, chosen->x,
        [](const auto& stop, auto value) { return stop.x < value; });
    return *chosen;
}
bool TextEngine::map_carets(const ShapedText& shaped, StringView source,
    std::uint64_t revision, float baseline, TextCaretMap& output) const {
    if(source.size_bytes() != shaped.normalized_size_bytes) return false;
    input::Utf8ScalarIterator scalars{source.bytes()};
    std::size_t index{};
    while(const auto scalar = scalars.next()) {
        if(index >= shaped.scalars.size()) return false;
        const auto& expected = shaped.scalars[index++];
        if(scalar->value != expected.value || scalar->byte_begin != expected.byte_start
            || scalar->byte_end != expected.byte_end) return false;
    }
    if(!scalars.valid() || index != shaped.scalars.size()) return false;
    input::TextBoundaryMap boundaries;
    if(!boundaries.assign(source.bytes())) return false;
    return output.assign(shaped, boundaries.grapheme_bytes(), revision, baseline);
}
} // namespace ryn::text
