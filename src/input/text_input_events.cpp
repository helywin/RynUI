#include "input/text_input_events.hpp"

#include <limits>

namespace ryn::input {

bool is_valid(const TextCommitted& event) noexcept {
    return event.text.size_bytes() <= text_event_max_bytes;
}

bool is_valid(const CompositionChanged& event) noexcept {
    if (event.text.size_bytes() > text_event_max_bytes) return false;
    const auto range = event.selection;
    if (!range.known) return range.start == 0 && range.length == 0;
    std::size_t scalars = 0;
    // String has already validated UTF-8; only leading bytes count as scalars.
    for (const auto byte : event.text.utf8()) {
        if ((static_cast<unsigned char>(byte) & 0xC0U) != 0x80U) ++scalars;
    }
    return range.start <= scalars && range.length <= scalars - range.start;
}

std::size_t payload_bytes(const CandidatesChanged& event) noexcept {
    std::size_t total = 0;
    for (const auto& candidate : event.candidates) {
        if (candidate.size_bytes() > std::numeric_limits<std::size_t>::max() - total)
            return std::numeric_limits<std::size_t>::max();
        total += candidate.size_bytes();
    }
    return total;
}

bool is_valid(const CandidatesChanged& event) noexcept {
    return event.candidates.size() <= text_event_max_candidates
        && (!event.selected || *event.selected < event.candidates.size())
        && (event.orientation == CandidateOrientation::vertical
            || event.orientation == CandidateOrientation::horizontal)
        && payload_bytes(event) <= text_event_max_bytes;
}

} // namespace ryn::input
