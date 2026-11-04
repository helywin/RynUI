#pragma once

#include <ryn/component.hpp>
#include <ryn/string.hpp>
#include <ryn/text_direction.hpp>
#include <array>
#include <functional>
#include <optional>
#include <vector>

namespace ryn {

enum class PickerEndpoint { Start, End };
enum class PickerPlacement { BottomLeft, BottomRight, TopLeft, TopRight };

// Gregorian business calendar configuration; no epoch or implicit timezone conversion.
struct PickerLocale final {
    std::array<String, 12> months{String{u8"January"}, String{u8"February"}, String{u8"March"},
                                  String{u8"April"},   String{u8"May"},      String{u8"June"},
                                  String{u8"July"},    String{u8"August"},   String{u8"September"},
                                  String{u8"October"}, String{u8"November"}, String{u8"December"}};
    std::array<String, 12> short_months{String{u8"Jan"}, String{u8"Feb"}, String{u8"Mar"}, String{u8"Apr"},
                                        String{u8"May"}, String{u8"Jun"}, String{u8"Jul"}, String{u8"Aug"},
                                        String{u8"Sep"}, String{u8"Oct"}, String{u8"Nov"}, String{u8"Dec"}};
    std::array<String, 7> weekdays{String{u8"Su"}, String{u8"Mo"}, String{u8"Tu"}, String{u8"We"},
                                   String{u8"Th"}, String{u8"Fr"}, String{u8"Sa"}};
    String am{u8"AM"};
    String pm{u8"PM"};
    String now{u8"Now"};
    String today{u8"Today"};
    String confirm{u8"OK"};
    String cancel{u8"Cancel"};
    unsigned first_day{};            // Sunday = 0.
    unsigned first_week_contains{1}; // ISO: first_day=1, first_week_contains=4.
    int year_offset{};               // Display-only era, for example Buddhist +543.
    void validate() const;
    friend bool operator==(const PickerLocale&, const PickerLocale&) = default;
};

template <typename T> struct PickerRange final {
    std::optional<T> start;
    std::optional<T> end;
    friend bool operator==(const PickerRange&, const PickerRange&) = default;
};

template <typename T> struct PickerRangePreset final {
    String key;
    String label;
    std::function<PickerRange<T>()> value;
};

struct PickerRangePolicy final {
    std::array<bool, 2> allow_empty{false, false};
    std::array<bool, 2> disabled{false, false};
    bool order{true};
    friend bool operator==(const PickerRangePolicy&, const PickerRangePolicy&) = default;
};

struct PickerIconSlot final {};

using PickerIcon = SlotContent<PickerIconSlot>;

} // namespace ryn
