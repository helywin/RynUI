#include "text/text_caret_map.hpp"
#include "text/text_engine.hpp"
#include "input/text_boundary.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::text {
void TextCaretMap::reserve(std::size_t stops, std::size_t glyphs) {
    stops_.reserve(stops);
    pending_.reserve(stops);
    clusters_.reserve(glyphs);
    lines_.reserve(stops);
    pending_lines_.reserve(stops);
}

bool TextCaretMap::assign(const ShapedText& shaped, std::span<const std::size_t> boundaries, std::uint64_t revision,
                          float baseline) {
    if (!revision || !std::isfinite(baseline) || boundaries.empty() || boundaries.front() != 0 ||
        boundaries.back() != shaped.normalized_size_bytes || shaped.paragraphs.size() > 1) {
        return false;
    }
    for (std::size_t i = 1; i < boundaries.size(); ++i) {
        if (boundaries[i - 1] >= boundaries[i]) {
            return false;
        }
    }
    for (const auto& run : shaped.runs) {
        if (run.right_to_left) {
            return false;
        }
    }
    clusters_.clear();
    float advance{};
    for (std::size_t i = 0; i < shaped.glyphs.size(); ++i) {
        const auto& glyph = shaped.glyphs[i];
        if (glyph.cluster >= shaped.normalized_size_bytes || !std::isfinite(glyph.advance_x) || glyph.advance_x < 0 ||
            (!clusters_.empty() && glyph.cluster < clusters_.back().byte_begin)) {
            return false;
        }
        if (clusters_.empty() || clusters_.back().byte_begin != glyph.cluster) {
            if (!clusters_.empty()) {
                clusters_.back().byte_end = glyph.cluster;
            }
            clusters_.push_back({glyph.cluster, shaped.normalized_size_bytes, i, 0, advance, 0});
        }
        auto& cluster = clusters_.back();
        ++cluster.glyph_count;
        cluster.advance += glyph.advance_x;
        advance += glyph.advance_x;
        if (!std::isfinite(advance)) {
            return false;
        }
    }
    pending_.clear();
    pending_.reserve(boundaries.size());
    std::size_t cluster_index{};
    std::size_t range_index{};
    for (std::size_t i = 0; i < boundaries.size(); ++i) {
        const auto byte = boundaries[i];
        TextCaretStop stop{byte, shaped.glyphs.size(), 0, 0, baseline};
        if (byte == shaped.normalized_size_bytes) {
            stop.x = advance;
        } else if (!clusters_.empty() && byte >= clusters_.front().byte_begin) {
            while (cluster_index + 1 < clusters_.size() && clusters_[cluster_index + 1].byte_begin <= byte) {
                ++cluster_index;
            }
            const auto& cluster = clusters_[cluster_index];
            const auto first = std::upper_bound(boundaries.begin(), boundaries.end(), cluster.byte_begin);
            const auto last = std::lower_bound(first, boundaries.end(), cluster.byte_end);
            const auto position = std::lower_bound(first, last, byte);
            const auto pieces = static_cast<float>(last - first + 1);
            const auto fraction = byte == cluster.byte_begin ? 0.0F : static_cast<float>(position - first + 1) / pieces;
            stop.x = cluster.x + cluster.advance * fraction;
        }
        if (i + 1 < boundaries.size()) {
            while (range_index < clusters_.size() && clusters_[range_index].byte_end <= byte) {
                ++range_index;
            }
            auto last = range_index;
            while (last < clusters_.size() && clusters_[last].byte_begin < boundaries[i + 1]) {
                ++last;
            }
            if (last > range_index) {
                stop.glyph_begin = clusters_[range_index].glyph_begin;
                const auto& end = clusters_[last - 1];
                stop.glyph_count = end.glyph_begin + end.glyph_count - stop.glyph_begin;
            }
        }
        if (!pending_.empty() && stop.x < pending_.back().x) {
            return false;
        }
        pending_.push_back(stop);
    }
    pending_lines_.clear();
    pending_lines_.push_back({0, pending_.size(), 0, 0});
    stops_.swap(pending_);
    lines_.swap(pending_lines_);
    revision_ = revision;
    return true;
}

bool TextCaretMap::assign(const ShapedText& shaped, const TextMeasurement& measurement,
                          std::span<const std::size_t> boundaries, std::uint64_t revision) {
    if (!revision || boundaries.empty() || boundaries.front() != 0 ||
        boundaries.back() != shaped.normalized_size_bytes || measurement.lines.empty() ||
        !std::isfinite(measurement.height) || measurement.height <= 0 || measurement.lines.front().byte_start != 0 ||
        measurement.lines.back().byte_end != shaped.normalized_size_bytes) {
        return false;
    }
    for (std::size_t i = 1; i < boundaries.size(); ++i) {
        if (boundaries[i - 1] >= boundaries[i]) {
            return false;
        }
    }
    for (const auto& run : shaped.runs) {
        if (run.right_to_left) {
            return false;
        }
    }
    pending_.clear();
    pending_lines_.clear();
    const auto height = measurement.height / static_cast<float>(measurement.lines.size());
    std::size_t expected_glyph{};
    std::size_t previous_end{};
    for (std::size_t index = 0; index < measurement.lines.size(); ++index) {
        const auto& line = measurement.lines[index];
        if (!std::isfinite(line.baseline) || !std::isfinite(line.width) || line.width < 0 ||
            line.byte_start < previous_end || line.byte_end < line.byte_start ||
            line.byte_end > shaped.normalized_size_bytes || line.glyph_begin != expected_glyph ||
            line.glyph_count > shaped.glyphs.size() - std::min(expected_glyph, shaped.glyphs.size()) ||
            !std::binary_search(boundaries.begin(), boundaries.end(), line.byte_start) ||
            !std::binary_search(boundaries.begin(), boundaries.end(), line.byte_end)) {
            return false;
        }
        clusters_.clear();
        float advance{};
        const auto glyph_end = line.glyph_begin + line.glyph_count;
        for (std::size_t i = line.glyph_begin; i < glyph_end; ++i) {
            const auto& glyph = shaped.glyphs[i];
            if (glyph.cluster < line.byte_start || glyph.cluster >= line.byte_end || !std::isfinite(glyph.advance_x) ||
                glyph.advance_x < 0 || (!clusters_.empty() && glyph.cluster < clusters_.back().byte_begin)) {
                return false;
            }
            if (clusters_.empty() || clusters_.back().byte_begin != glyph.cluster) {
                if (!clusters_.empty()) {
                    clusters_.back().byte_end = glyph.cluster;
                }
                clusters_.push_back({glyph.cluster, line.byte_end, i, 0, advance, 0});
            }
            ++clusters_.back().glyph_count;
            clusters_.back().advance += glyph.advance_x;
            advance += glyph.advance_x;
            if (!std::isfinite(advance)) {
                return false;
            }
        }
        const auto first_boundary = std::lower_bound(boundaries.begin(), boundaries.end(), line.byte_start);
        const auto last_boundary = std::upper_bound(first_boundary, boundaries.end(), line.byte_end);
        const auto stop_begin = pending_.size();
        std::size_t cluster_index{};
        std::size_t range_index{};
        for (auto boundary = first_boundary; boundary != last_boundary; ++boundary) {
            const auto byte = *boundary;
            TextCaretStop stop{byte, glyph_end, 0, 0, line.baseline, index};
            if (byte == line.byte_end) {
                stop.x = advance;
            } else if (!clusters_.empty() && byte >= clusters_.front().byte_begin) {
                while (cluster_index + 1 < clusters_.size() && clusters_[cluster_index + 1].byte_begin <= byte) {
                    ++cluster_index;
                }
                const auto& cluster = clusters_[cluster_index];
                const auto first = std::upper_bound(boundaries.begin(), boundaries.end(), cluster.byte_begin);
                const auto last = std::lower_bound(first, boundaries.end(), cluster.byte_end);
                const auto position = std::lower_bound(first, last, byte);
                const auto pieces = static_cast<float>(last - first + 1);
                const auto fraction =
                    byte == cluster.byte_begin ? 0.0F : static_cast<float>(position - first + 1) / pieces;
                stop.x = cluster.x + cluster.advance * fraction;
            }
            if (boundary + 1 != last_boundary) {
                while (range_index < clusters_.size() && clusters_[range_index].byte_end <= byte) {
                    ++range_index;
                }
                auto last = range_index;
                while (last < clusters_.size() && clusters_[last].byte_begin < *(boundary + 1)) {
                    ++last;
                }
                if (last > range_index) {
                    stop.glyph_begin = clusters_[range_index].glyph_begin;
                    const auto& end = clusters_[last - 1];
                    stop.glyph_count = end.glyph_begin + end.glyph_count - stop.glyph_begin;
                }
            }
            if (pending_.size() > stop_begin && stop.x < pending_.back().x) {
                return false;
            }
            pending_.push_back(stop);
        }
        pending_lines_.push_back(
            {stop_begin, pending_.size() - stop_begin, static_cast<float>(index) * height, height});
        expected_glyph = glyph_end;
        previous_end = line.byte_end;
    }
    if (expected_glyph != shaped.glyphs.size()) {
        return false;
    }
    std::size_t covered{};
    for (const auto& stop : pending_) {
        if (covered < boundaries.size() && stop.byte == boundaries[covered]) {
            ++covered;
        }
    }
    if (covered != boundaries.size()) {
        return false;
    }
    stops_.swap(pending_);
    lines_.swap(pending_lines_);
    revision_ = revision;
    return true;
}

std::optional<TextCaretStop> TextCaretMap::at(std::size_t byte, std::uint64_t revision,
                                              TextCaretAffinity affinity) const noexcept {
    if (!revision || revision != revision_) {
        return {};
    }
    const auto found = std::lower_bound(stops_.begin(), stops_.end(), byte,
                                        [](const auto& stop, auto value) { return stop.byte < value; });
    if (found == stops_.end() || found->byte != byte) {
        return {};
    }
    if (affinity == TextCaretAffinity::Upstream) {
        return *found;
    }
    const auto end =
        std::upper_bound(found, stops_.end(), byte, [](auto value, const auto& stop) { return value < stop.byte; });
    return *(end - 1);
}

std::optional<TextCaretStop> TextCaretMap::nearest(float x, std::uint64_t revision) const noexcept {
    return nearest_on_line(x, 0, revision);
}

std::span<const TextCaretStop> TextCaretMap::line_stops(std::size_t line) const noexcept {
    if (line >= lines_.size()) {
        return {};
    }
    return std::span{stops_}.subspan(lines_[line].stop_begin, lines_[line].stop_count);
}

std::optional<TextCaretStop> TextCaretMap::nearest_on_line(float x, std::size_t line,
                                                           std::uint64_t revision) const noexcept {
    const auto stops = line_stops(line);
    if (!revision || revision != revision_ || stops.empty() || !std::isfinite(x)) {
        return {};
    }
    auto right =
        std::lower_bound(stops.begin(), stops.end(), x, [](const auto& stop, auto value) { return stop.x < value; });
    if (right == stops.begin()) {
        return *right;
    }
    auto chosen = right;
    if (right == stops.end() || x - (right - 1)->x <= right->x - x) {
        chosen = right - 1;
    }
    chosen = std::lower_bound(stops.begin(), chosen + 1, chosen->x,
                              [](const auto& stop, auto value) { return stop.x < value; });
    return *chosen;
}

std::optional<TextCaretStop> TextCaretMap::nearest(float x, float y, std::uint64_t revision) const noexcept {
    if (lines_.empty() || !std::isfinite(y)) {
        return {};
    }
    const auto next = std::upper_bound(lines_.begin(), lines_.end(), y,
                                       [](float value, const auto& line) { return value < line.top; });
    const auto line = next == lines_.begin() ? 0 : static_cast<std::size_t>(next - lines_.begin() - 1);
    return nearest_on_line(x, line, revision);
}

std::optional<TextCaretStop> TextCaretMap::line_edge(std::size_t line, bool end,
                                                     std::uint64_t revision) const noexcept {
    const auto stops = line_stops(line);
    if (!revision || revision != revision_ || stops.empty()) {
        return {};
    }
    return end ? stops.back() : stops.front();
}

std::optional<TextCaretStop> TextCaretMap::adjacent_line(TextCaretStop current, int delta, float preferred_x,
                                                         std::uint64_t revision) const noexcept {
    if (current.line >= lines_.size()) {
        return {};
    }
    const auto line = std::clamp(static_cast<std::int64_t>(current.line) + delta, std::int64_t{0},
                                 static_cast<std::int64_t>(lines_.size() - 1));
    return nearest_on_line(preferred_x, static_cast<std::size_t>(line), revision);
}

bool TextEngine::map_carets(const ShapedText& shaped, StringView source, std::uint64_t revision, float baseline,
                            TextCaretMap& output) const {
    if (source.size_bytes() != shaped.normalized_size_bytes) {
        return false;
    }
    input::Utf8ScalarIterator scalars{source.bytes()};
    std::size_t index{};
    while (const auto scalar = scalars.next()) {
        if (index >= shaped.scalars.size()) {
            return false;
        }
        const auto& expected = shaped.scalars[index++];
        if (scalar->value != expected.value || scalar->byte_begin != expected.byte_start ||
            scalar->byte_end != expected.byte_end) {
            return false;
        }
    }
    if (!scalars.valid() || index != shaped.scalars.size()) {
        return false;
    }
    input::TextBoundaryMap boundaries;
    if (!boundaries.assign(source.bytes())) {
        return false;
    }
    return output.assign(shaped, boundaries.grapheme_bytes(), revision, baseline);
}

bool TextEngine::map_carets(const ShapedText& shaped, StringView source, std::uint64_t revision,
                            const TextMeasurement& measurement, TextCaretMap& output) const {
    if (source.size_bytes() != shaped.normalized_size_bytes || decode_utf8(source) != shaped.scalars) {
        return false;
    }
    input::TextBoundaryMap boundaries;
    if (!boundaries.assign(source.bytes())) {
        return false;
    }
    return output.assign(shaped, measurement, boundaries.grapheme_bytes(), revision);
}
} // namespace ryn::text
