#pragma once
#include <ryn/date_picker.hpp>
#include <array>

namespace ryn::detail {
struct DateValueSemantics final {
    DatePickerMode mode{DatePickerMode::Date};
    unsigned first_day{};
    friend bool operator==(const DateValueSemantics&, const DateValueSemantics&) = default;
};

struct PickerCandidateSemantics final {
    std::optional<DateValueSemantics> date;
    std::optional<TimePickerPrecision> time;
    friend bool operator==(const PickerCandidateSemantics&, const PickerCandidateSemantics&) = default;
};

enum class PickerRequestDisposition { Rejected, Consumed, Superseded };

// A synchronous request owns this check; it is never stored with accepted values.
struct PickerCandidate final {
    PickerCandidateSemantics semantics;
    std::function<bool()> current;

    [[nodiscard]] bool active() const {
        return !current || current();
    }
};
} // namespace ryn::detail
