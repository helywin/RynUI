#include "gallery_draft_codec.hpp"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

using namespace ryn;
using namespace rynui::example;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <typename T> void round_trip(const T& value, const T& fallback = T{}) {
    GalleryPageDraft draft;
    gallery_draft_write(draft, "field", value);
    require(gallery_draft_read(draft, "field", fallback) == value, "typed draft round trip changed a business value");
}

void test_values_and_authority() {
    round_trip(String{u8"普通草稿 / مرحبا / 🙂"});
    round_trip(std::numeric_limits<std::uint64_t>::max());
    round_trip(std::numeric_limits<std::int64_t>::min());
    round_trip(CheckboxValues{String{u8"desktop"}, 3.5, true});
    round_trip(RadioSelection{RadioValue{false}}, RadioSelection{RadioValue{String{u8"default"}}});
    round_trip(SelectSelection{SelectValue{std::int64_t{42}}});
    round_trip(SelectSelection{}, SelectSelection{SelectValue{u8"default"}});
    round_trip(DatePickerValue{CivilDate{2028, 2, 29}});
    round_trip(DatePickerValue{}, DatePickerValue{CivilDate{2026, 11, 3}});
    round_trip(TimePickerValue{TimeOfDay{23, 59, 58}});
    round_trip(StatisticValue{std::numeric_limits<std::uint64_t>::max()});
    round_trip(ResultStatus{ResultException::NotFound});
    round_trip(AvatarSizeValue{42.5F});
    round_trip(AvatarSizeValue{Responsive<float>{24}.at(Breakpoint::Sm, 32).at(Breakpoint::Lg, 64)});
    round_trip(SliderRange{25.5, 80});
    round_trip(TableSort{String{u8"quantity"}, TableSortOrder::Descend});
    round_trip(std::vector<SelectOption>{{SelectValue{u8"desktop"}, String{u8"桌面"}, false},
                                         {SelectValue{std::int64_t{7}}, String{u8"七"}, true}});
    GalleryPageDraft draft;
    const CheckboxValues original{String{u8"a"}, String{u8"b"}};
    gallery_draft_write(draft, "choices", original);
    gallery_draft_write(draft, "choices", CheckboxValues{});
    require(gallery_draft_read(draft, "choices", original).empty(), "empty selection restored stale entries");
}

void test_invalid_values_are_rejected() {
    GalleryPageDraft draft;
    draft.values["variant/index"] = std::int64_t{99};
    bool rejected{};
    try {
        static_cast<void>(gallery_draft_read(draft, "variant", ResultStatus{Status::Success}));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "unknown draft alternative silently changed authority");
    draft.values["count"] = std::string{"18446744073709551616"};
    rejected = false;
    try {
        static_cast<void>(gallery_draft_read(draft, "count", std::uint64_t{}));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "unsigned overflow wrapped a draft value");
    draft.values["date/year"] = std::int64_t{2027};
    draft.values["date/month"] = std::string{"2"};
    draft.values["date/day"] = std::string{"29"};
    rejected = false;
    try {
        static_cast<void>(gallery_draft_read(draft, "date", CivilDate{}));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "invalid civil date entered a restored component");
}

} // namespace

int main() {
    try {
        test_values_and_authority();
        test_invalid_values_are_rejected();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
