#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>

namespace ryn::graphics {

// Range is an internal {first, count} instance range with uint32_t fields.
template <typename Range>
class DirtyRangeAccumulator final {
public:
    void reserve(std::size_t capacity) {
        ranges_.reserve(capacity);
    }

    void append(Range range) {
        if (range.count == 0) {
            return;
        }
        if (!ranges_.empty()) {
            Range& last = ranges_.back();
            const auto last_end =
                static_cast<std::uint64_t>(last.first) + last.count;
            const auto range_end =
                static_cast<std::uint64_t>(range.first) + range.count;
            if (range.first <= last_end && last.first <= range_end) {
                const auto first = std::min(last.first, range.first);
                last = {
                    first,
                    static_cast<std::uint32_t>(std::max(last_end, range_end) - first),
                };
                needs_normalization_ = true;
                return;
            }
        }
        ranges_.push_back(range);
        needs_normalization_ = true;
    }

    void discard_shifted(std::uint32_t first) noexcept {
        std::size_t retained = 0;
        for (Range range : ranges_) {
            if (range.first >= first) {
                continue;
            }
            const auto end = static_cast<std::uint64_t>(range.first) + range.count;
            if (end > first) {
                range.count = first - range.first;
            }
            if (range.count != 0) {
                ranges_[retained++] = range;
            }
        }
        ranges_.resize(retained);
    }

    [[nodiscard]] std::span<const Range> ranges() const noexcept {
        if (needs_normalization_) {
            std::ranges::sort(ranges_, {}, &Range::first);
            std::size_t merged = 0;
            for (const Range candidate : ranges_) {
                if (merged != 0) {
                    Range& prior = ranges_[merged - 1];
                    const auto prior_end =
                        static_cast<std::uint64_t>(prior.first) + prior.count;
                    const auto candidate_end =
                        static_cast<std::uint64_t>(candidate.first) + candidate.count;
                    if (candidate.first <= prior_end) {
                        prior.count = static_cast<std::uint32_t>(
                            std::max(prior_end, candidate_end) - prior.first);
                        continue;
                    }
                }
                ranges_[merged++] = candidate;
            }
            ranges_.resize(merged);
            needs_normalization_ = false;
        }
        return ranges_;
    }

    void clear() noexcept {
        ranges_.clear();
        needs_normalization_ = false;
    }

private:
    mutable std::vector<Range> ranges_;
    mutable bool needs_normalization_{false};
};

} // namespace ryn::graphics
