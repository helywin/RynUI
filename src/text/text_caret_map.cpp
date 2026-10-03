#include "text/text_caret_map.hpp"
#include "text/text_engine.hpp"
#include "input/text_boundary.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace ryn::text {
namespace {
bool logical_less(const TextCaretStop& first, const TextCaretStop& second) {
    return std::tie(first.byte, first.affinity) < std::tie(second.byte, second.affinity);
}

bool visual_less(const TextCaretStop& first, const TextCaretStop& second) {
    return std::tie(first.line, first.x, first.byte, first.affinity) <
           std::tie(second.line, second.x, second.byte, second.affinity);
}

bool same_position(const TextCaretStop& first, const TextCaretStop& second) {
    return first.line == second.line && first.x == second.x && first.baseline == second.baseline;
}
} // namespace

void TextCaretMap::reserve(std::size_t stops, std::size_t glyphs) {
    stops_.reserve(stops * 2);
    pending_.reserve(stops * 2);
    visual_.reserve(stops * 2);
    pending_visual_.reserve(stops * 2);
    clusters_.reserve(glyphs);
    coverage_.reserve(stops + glyphs);
    pending_coverage_.reserve(stops + glyphs);
    used_glyphs_.reserve(glyphs);
    lines_.reserve(stops);
    pending_lines_.reserve(stops);
}

bool TextCaretMap::assign(const ShapedText& shaped, std::span<const std::size_t> boundaries, std::uint64_t revision,
                          float baseline) {
    return unwrapped_caret_geometry(shaped, baseline, unwrapped_) && assign(shaped, unwrapped_, boundaries, revision);
}

bool TextCaretMap::assign(const ShapedText& shaped, const TextMeasurement& measurement,
                          std::span<const std::size_t> boundaries, std::uint64_t revision) {
    if (!revision || boundaries.empty() || boundaries.front() != 0 ||
        boundaries.back() != shaped.normalized_size_bytes || measurement.lines.empty() ||
        !std::isfinite(measurement.height) || measurement.height <= 0 || measurement.lines.front().byte_start != 0 ||
        measurement.lines.back().byte_end != shaped.normalized_size_bytes) {
        return false;
    }
    for (std::size_t index = 1; index < boundaries.size(); ++index) {
        if (boundaries[index - 1] >= boundaries[index]) {
            return false;
        }
    }
    if (!measurement.visual_glyphs.empty() && measurement.visual_glyphs.size() != shaped.glyphs.size()) {
        return false;
    }
    if (measurement.visual_clusters.empty() &&
        std::ranges::any_of(shaped.runs, [](const auto& run) { return run.right_to_left; })) {
        return false;
    }
    pending_.clear();
    pending_visual_.clear();
    pending_lines_.clear();
    pending_coverage_.clear();
    used_glyphs_.assign(shaped.glyphs.size(), false);
    const auto height = measurement.height / static_cast<float>(measurement.lines.size());
    std::size_t expected_glyph{};
    std::size_t expected_cluster{};
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
        const auto glyph_end = line.glyph_begin + line.glyph_count;
        if (!measurement.visual_clusters.empty()) {
            if (line.cluster_begin != expected_cluster ||
                line.cluster_count > measurement.visual_clusters.size() -
                                         std::min(expected_cluster, measurement.visual_clusters.size())) {
                return false;
            }
            float pen{};
            auto next_glyph = line.glyph_begin;
            for (auto cluster_index = line.cluster_begin; cluster_index < line.cluster_begin + line.cluster_count;
                 ++cluster_index) {
                const auto& cluster = measurement.visual_clusters[cluster_index];
                if (cluster.byte_start < line.byte_start || cluster.byte_end > line.byte_end ||
                    cluster.byte_end <= cluster.byte_start || cluster.glyph_begin != next_glyph ||
                    cluster.glyph_count > glyph_end - std::min(next_glyph, glyph_end) || cluster.glyph_count == 0 ||
                    !std::isfinite(cluster.x) || !std::isfinite(cluster.width) || cluster.width < 0 ||
                    std::abs(cluster.x - pen) > .001F) {
                    return false;
                }
                clusters_.push_back({cluster.byte_start, cluster.byte_end, cluster.glyph_begin, cluster.glyph_count,
                                     cluster.x, cluster.width, cluster.level});
                pen += cluster.width;
                next_glyph += cluster.glyph_count;
            }
            if (next_glyph != glyph_end || std::abs(pen - line.width) > .001F) {
                return false;
            }
            expected_cluster += line.cluster_count;
        } else {
            float pen{};
            for (auto visual = line.glyph_begin; visual < glyph_end; ++visual) {
                const auto glyph_index = measurement.glyph_index(visual);
                if (glyph_index >= shaped.glyphs.size()) {
                    return false;
                }
                const auto& glyph = shaped.glyphs[glyph_index];
                if (glyph.cluster < line.byte_start || glyph.cluster >= line.byte_end ||
                    !std::isfinite(glyph.advance_x) || glyph.advance_x < 0 ||
                    (!clusters_.empty() && glyph.cluster < clusters_.back().byte_begin)) {
                    return false;
                }
                if (clusters_.empty() || clusters_.back().byte_begin != glyph.cluster) {
                    if (!clusters_.empty()) {
                        clusters_.back().byte_end = glyph.cluster;
                    }
                    clusters_.push_back({glyph.cluster, line.byte_end, visual, 0, pen, 0, 0});
                }
                ++clusters_.back().glyph_count;
                clusters_.back().advance += glyph.advance_x;
                pen += glyph.advance_x;
            }
            if (!std::isfinite(pen) || std::abs(pen - line.width) > .001F) {
                return false;
            }
        }
        for (const auto& cluster : clusters_) {
            float advance{};
            for (auto visual = cluster.glyph_begin; visual < cluster.glyph_begin + cluster.glyph_count; ++visual) {
                const auto glyph_index = measurement.glyph_index(visual);
                if (glyph_index >= shaped.glyphs.size() || used_glyphs_[glyph_index]) {
                    return false;
                }
                const auto& glyph = shaped.glyphs[glyph_index];
                if (glyph.cluster != cluster.byte_begin || !std::isfinite(glyph.advance_x) ||
                    (glyph.advance_x < 0 && !shaped.bidi.assigned())) {
                    return false;
                }
                used_glyphs_[glyph_index] = true;
                advance += std::abs(glyph.advance_x);
            }
            if (!std::isfinite(advance) || std::abs(advance - cluster.advance) > .001F) {
                return false;
            }
        }
        const auto coordinate = [&](const Cluster& cluster, std::size_t byte) {
            const auto first = std::upper_bound(boundaries.begin(), boundaries.end(), cluster.byte_begin);
            const auto last = std::lower_bound(first, boundaries.end(), cluster.byte_end);
            const auto position = std::lower_bound(first, last, byte);
            const auto pieces = static_cast<float>(last - first + 1);
            const auto fraction = byte <= cluster.byte_begin ? 0.0F
                                  : byte >= cluster.byte_end ? 1.0F
                                                             : static_cast<float>(position - first + 1) / pieces;
            return cluster.x + cluster.advance * ((cluster.level & 1) ? 1 - fraction : fraction);
        };
        for (const auto& cluster : clusters_) {
            auto boundary = std::upper_bound(boundaries.begin(), boundaries.end(), cluster.byte_begin);
            if (boundary != boundaries.begin()) {
                --boundary;
            }
            for (; boundary + 1 != boundaries.end() && *boundary < cluster.byte_end; ++boundary) {
                if (*(boundary + 1) <= cluster.byte_begin) {
                    continue;
                }
                const auto first = coordinate(cluster, std::max(*boundary, cluster.byte_begin));
                const auto last = coordinate(cluster, std::min(*(boundary + 1), cluster.byte_end));
                pending_coverage_.push_back(
                    {*boundary, *(boundary + 1), index, std::min(first, last), std::abs(last - first)});
            }
        }
        const auto first_boundary = std::lower_bound(boundaries.begin(), boundaries.end(), line.byte_start);
        const auto last_boundary = std::upper_bound(first_boundary, boundaries.end(), line.byte_end);
        for (auto boundary = first_boundary; boundary != last_boundary; ++boundary) {
            const auto byte = *boundary;
            const Cluster* before{};
            const Cluster* after{};
            auto range_begin = glyph_end;
            std::size_t range_end{};
            for (const auto& cluster : clusters_) {
                if (boundary != first_boundary && cluster.byte_begin < byte && cluster.byte_end > *(boundary - 1) &&
                    (!before || cluster.byte_end > before->byte_end)) {
                    before = &cluster;
                }
                if (boundary + 1 != last_boundary && cluster.byte_end > byte && cluster.byte_begin < *(boundary + 1)) {
                    if (!after || cluster.byte_begin < after->byte_begin) {
                        after = &cluster;
                    }
                    range_begin = std::min(range_begin, cluster.glyph_begin);
                    range_end = std::max(range_end, cluster.glyph_begin + cluster.glyph_count);
                }
            }
            if (range_begin == glyph_end) {
                range_end = glyph_end;
            }
            const auto stop = [&](const Cluster& cluster, TextCaretAffinity affinity) {
                return TextCaretStop{
                    byte,  range_begin, range_end - range_begin, coordinate(cluster, byte), line.baseline,
                    index, affinity};
            };
            if (before && after) {
                const auto upstream = stop(*before, TextCaretAffinity::Upstream);
                const auto downstream = stop(*after, TextCaretAffinity::Downstream);
                if (!same_position(upstream, downstream)) {
                    pending_.push_back(upstream);
                }
                pending_.push_back(downstream);
            } else if (before) {
                pending_.push_back(stop(*before, TextCaretAffinity::Upstream));
            } else if (after) {
                pending_.push_back(stop(*after, TextCaretAffinity::Downstream));
            } else {
                pending_.push_back({byte, glyph_end, 0, 0, line.baseline, index});
            }
        }
        std::uint8_t base_level{};
        for (const auto& paragraph : shaped.bidi.paragraphs()) {
            if (paragraph.byte_begin <= line.byte_start && line.byte_end <= paragraph.byte_end) {
                base_level = paragraph.base_level;
                break;
            }
        }
        pending_lines_.push_back(
            {0, 0, static_cast<float>(index) * height, height, line.byte_start, line.byte_end, base_level});
        expected_glyph = glyph_end;
        previous_end = line.byte_end;
    }
    if (expected_glyph != shaped.glyphs.size() || expected_cluster != measurement.visual_clusters.size()) {
        return false;
    }
    std::ranges::sort(pending_, logical_less);
    for (const auto boundary : boundaries) {
        const auto found = std::lower_bound(pending_.begin(), pending_.end(), boundary,
                                            [](const auto& stop, auto byte) { return stop.byte < byte; });
        if (found == pending_.end() || found->byte != boundary) {
            return false;
        }
    }
    pending_visual_.assign(pending_.begin(), pending_.end());
    std::ranges::sort(pending_visual_, visual_less);
    for (std::size_t index = 0; index < pending_visual_.size(); ++index) {
        auto& line = pending_lines_[pending_visual_[index].line];
        if (line.stop_count == 0) {
            line.stop_begin = index;
        }
        ++line.stop_count;
    }
    std::ranges::sort(pending_coverage_, [](const auto& first, const auto& second) {
        return std::tie(first.line, first.x, first.byte_begin) < std::tie(second.line, second.x, second.byte_begin);
    });
    stops_.swap(pending_);
    visual_.swap(pending_visual_);
    coverage_.swap(pending_coverage_);
    lines_.swap(pending_lines_);
    revision_ = revision;
    return true;
}

std::optional<TextCaretStop> TextCaretMap::at(std::size_t byte, std::uint64_t revision,
                                              TextCaretAffinity affinity) const noexcept {
    if (!revision || revision != revision_) {
        return {};
    }
    const auto first = std::lower_bound(stops_.begin(), stops_.end(), byte,
                                        [](const auto& stop, auto value) { return stop.byte < value; });
    if (first == stops_.end() || first->byte != byte) {
        return {};
    }
    const auto last =
        std::upper_bound(first, stops_.end(), byte, [](auto value, const auto& stop) { return value < stop.byte; });
    const auto exact = std::find_if(first, last, [&](const auto& stop) { return stop.affinity == affinity; });
    return exact == last ? *first : *exact;
}

std::span<const TextCaretStop> TextCaretMap::line_stops(std::size_t line) const noexcept {
    return line >= lines_.size() ? std::span<const TextCaretStop>{}
                                 : std::span{visual_}.subspan(lines_[line].stop_begin, lines_[line].stop_count);
}

std::optional<TextCaretStop> TextCaretMap::nearest(float x, std::uint64_t revision) const noexcept {
    return nearest_on_line(x, 0, revision);
}

std::optional<TextCaretStop> TextCaretMap::nearest_on_line(float x, std::size_t line,
                                                           std::uint64_t revision) const noexcept {
    const auto stops = line_stops(line);
    if (!revision || revision != revision_ || stops.empty() || !std::isfinite(x)) {
        return {};
    }
    const auto right =
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

std::optional<std::pair<std::size_t, std::size_t>> TextCaretMap::line_bytes(std::size_t line,
                                                                            std::uint64_t revision) const noexcept {
    if (!revision || revision != revision_ || line >= lines_.size()) {
        return {};
    }
    return std::pair{lines_[line].byte_begin, lines_[line].byte_end};
}

std::optional<TextCaretStop> TextCaretMap::adjacent_line(TextCaretStop current, int delta, float preferred_x,
                                                         std::uint64_t revision) const noexcept {
    if (!at(current.byte, revision, current.affinity) || current.line >= lines_.size()) {
        return {};
    }
    const auto line = std::clamp(static_cast<std::int64_t>(current.line) + delta, std::int64_t{0},
                                 static_cast<std::int64_t>(lines_.size() - 1));
    return nearest_on_line(preferred_x, static_cast<std::size_t>(line), revision);
}

std::optional<TextCaretStop> TextCaretMap::adjacent_visual(TextCaretStop current, int delta,
                                                           std::uint64_t revision) const noexcept {
    const auto exact = at(current.byte, revision, current.affinity);
    if (!exact || *exact != current) {
        return {};
    }
    const auto stops = line_stops(current.line);
    const auto found = std::lower_bound(stops.begin(), stops.end(), current, visual_less);
    if (found == stops.end() || *found != current) {
        return {};
    }
    if (delta == 0) {
        return current;
    }
    if (delta < 0 && found != stops.begin()) {
        return *(found - 1);
    }
    if (delta > 0 && found + 1 != stops.end()) {
        return *(found + 1);
    }
    const auto next_line = static_cast<std::int64_t>(current.line) +
                           ((lines_[current.line].base_level & 1) ? (delta < 0 ? 1 : -1) : (delta < 0 ? -1 : 1));
    if (next_line < 0 || next_line >= static_cast<std::int64_t>(lines_.size())) {
        return current;
    }
    return line_edge(static_cast<std::size_t>(next_line), delta < 0, revision);
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
    return boundaries.assign(source.bytes()) && output.assign(shaped, boundaries.grapheme_bytes(), revision, baseline);
}

bool TextEngine::map_carets(const ShapedText& shaped, StringView source, std::uint64_t revision,
                            const TextMeasurement& measurement, TextCaretMap& output) const {
    if (source.size_bytes() != shaped.normalized_size_bytes || decode_utf8(source) != shaped.scalars) {
        return false;
    }
    input::TextBoundaryMap boundaries;
    return boundaries.assign(source.bytes()) &&
           output.assign(shaped, measurement, boundaries.grapheme_bytes(), revision);
}
} // namespace ryn::text
