#pragma once

#include <ryn/string.hpp>

#include <array>
#include <cstdint>

namespace ryn {

// Data only: shared by the public controls and the immutable i18n catalogs.
// Picker dates are Gregorian business dates, without epoch/timezone conversion.
struct PickerLocale final {
    std::array<String, 12> months{u8"January", u8"February", u8"March",     u8"April",   u8"May",      u8"June",
                                  u8"July",    u8"August",   u8"September", u8"October", u8"November", u8"December"};
    std::array<String, 12> short_months{u8"Jan", u8"Feb", u8"Mar", u8"Apr", u8"May", u8"Jun",
                                        u8"Jul", u8"Aug", u8"Sep", u8"Oct", u8"Nov", u8"Dec"};
    std::array<String, 7> weekdays{u8"Su", u8"Mo", u8"Tu", u8"We", u8"Th", u8"Fr", u8"Sa"};
    String am{u8"AM"};
    String pm{u8"PM"};
    String now{u8"Now"};
    String today{u8"Today"};
    String confirm{u8"OK"};
    String cancel{u8"Cancel"};
    unsigned first_day{};
    unsigned first_week_contains{1};
    int year_offset{};
    void validate() const;
    // Trailing fields preserve the existing aggregate prefix.
    String date_placeholder{u8"Select date"};
    String time_placeholder{u8"Select a time"};
    String date_time_placeholder{u8"Select date"};
    std::array<String, 2> range_placeholder{u8"Start date", u8"End date"};
    std::array<String, 2> time_range_placeholder{u8"Start time", u8"End time"};
    String clear{u8"Clear"};
    String clear_range{u8"Clear range"};
    String confirm_range{u8"Confirm range"};
    String cancel_range{u8"Cancel range"};
    String month{u8"Month"};
    String year{u8"Year"};
    String date_format{u8"YYYY-MM-DD"};
    String time_format{u8"HH:mm:ss"};
    String date_time_format{u8"YYYY-MM-DD HH:mm:ss"};
    String month_panel_format{u8"MMMM YYYY"};
    String year_placeholder{u8"Select year"};
    String quarter_placeholder{u8"Select quarter"};
    String month_placeholder{u8"Select month"};
    String week_placeholder{u8"Select week"};
    std::array<String, 2> year_range_placeholder{u8"Start year", u8"End year"};
    std::array<String, 2> quarter_range_placeholder{u8"Start quarter", u8"End quarter"};
    std::array<String, 2> month_range_placeholder{u8"Start month", u8"End month"};
    std::array<String, 2> week_range_placeholder{u8"Start week", u8"End week"};
    friend bool operator==(const PickerLocale&, const PickerLocale&) = default;
};

struct PaginationLocale final {
    String items_per_page{u8" / page"};
    String page_separator{u8"/ "};
    String jump_placeholder{u8"Go to"};
    String total{u8"{total} items"};
    friend bool operator==(const PaginationLocale&, const PaginationLocale&) = default;
};

struct ImageLocale final {
    String loading{u8"Loading image"};
    String error{u8"Image unavailable"};
    String preview{u8"Preview"};
    String zoom_in{u8"Zoom in"};
    String zoom_out{u8"Zoom out"};
    String rotate_left{u8"Rotate left"};
    String rotate_right{u8"Rotate right"};
    String flip_x{u8"Flip horizontally"};
    String flip_y{u8"Flip vertically"};
    String reset{u8"Reset"};
    String previous{u8"Previous image"};
    String next{u8"Next image"};
    String close{u8"Close preview"};
    String count{u8"{current} / {total}"};
    friend bool operator==(const ImageLocale&, const ImageLocale&) = default;
};

struct UploadLocale final {
    String select{u8"Select files"};
    String drop{u8"Click or drop files here"};
    String ready{u8"Ready"};
    String uploading{u8"Uploading"};
    String done{u8"Done"};
    String error{u8"Upload failed"};
    String cancelled{u8"Cancelled"};
    String preview{u8"Preview"};
    String download{u8"Download"};
    String remove{u8"Remove"};
    String retry{u8"Retry"};
    String cancel{u8"Cancel"};
    ImageLocale image_preview;
    friend bool operator==(const UploadLocale&, const UploadLocale&) = default;
};

struct TransferLocale final {
    std::array<String, 2> titles{u8"Source", u8"Target"};
    std::array<String, 2> empty{u8"No data", u8"No data"};
    String search_placeholder{u8"Search here"};
    String item_unit{u8"item"};
    String items_unit{u8"items"};
    String loading{u8"Loading…"};
    String select_all{u8"Select all"};
    String deselect_all{u8"Deselect all"};
    String select_current{u8"Select current page"};
    String select_invert{u8"Invert selection"};
    String clear{u8"Clear selection"};
    String remove{u8"Remove"};
    String remove_all{u8"Remove all"};
    String remove_current{u8"Remove current page"};
    std::array<String, 2> actions{u8"Move to target", u8"Move to source"};
    PaginationLocale pagination;
    String count_one{u8"{selected}/{total} item"};
    String count_other{u8"{selected}/{total} items"};
    friend bool operator==(const TransferLocale&, const TransferLocale&) = default;
};

struct FormLocale final {
    String optional{u8"(optional)"};
    String validating{u8"Validating"};
    String required{u8"This field is required"};
    String invalid{u8"Invalid field value"};
    friend bool operator==(const FormLocale&, const FormLocale&) = default;
};

struct DescriptionsLocale final {
    String separator{u8":"};
    friend bool operator==(const DescriptionsLocale&, const DescriptionsLocale&) = default;
};

struct QRCodeLocale final {
    String expired{u8"QR code expired"};
    String refresh{u8"Refresh"};
    String loading{u8"Loading"};
    String scanned{u8"Scanned"};
    friend bool operator==(const QRCodeLocale&, const QRCodeLocale&) = default;
};

// Original declaration from the 107 Popup/Statistic source handoff (t3-58a06ab0).
struct StatisticLocale final {
    String decimal_separator{u8"."};
    String group_separator{u8","};
    std::uint8_t primary_group{3};
    std::uint8_t secondary_group{3};
    std::array<String, 10> digits{u8"0", u8"1", u8"2", u8"3", u8"4", u8"5", u8"6", u8"7", u8"8", u8"9"};
    friend bool operator==(const StatisticLocale&, const StatisticLocale&) = default;
};

struct CommonLocale final {
    String confirm{u8"OK"};
    String cancel{u8"Cancel"};
    String empty{u8"No data"};
    String loading{u8"Loading…"};
    String retry{u8"Retry"};
    String close{u8"Close"};
    friend bool operator==(const CommonLocale&, const CommonLocale&) = default;
};

struct TableLocale final {
    String empty{u8"No data"};
    String loading{u8"Loading…"};
    friend bool operator==(const TableLocale&, const TableLocale&) = default;
};

struct TourLocale final {
    String previous{u8"Previous"};
    String next{u8"Next"};
    String finish{u8"Finish"};
    String close{u8"Close"};
    friend bool operator==(const TourLocale&, const TourLocale&) = default;
};

struct CascaderLocale final {
    String empty{u8"No data"};
    String loading{u8"Loading…"};
    String loading_failed{u8"Loading failed"};
    String search_paths{u8"Search paths"};
    String retry{u8"Retry"};
    friend bool operator==(const CascaderLocale&, const CascaderLocale&) = default;
};

struct ColorPickerLocale final {
    String single{u8"Single"};
    String gradient{u8"Gradient"};
    String clear{u8"Clear"};
    String add_stop{u8"Add stop"};
    String remove{u8"Remove"};
    String color{u8"Color"};
    friend bool operator==(const ColorPickerLocale&, const ColorPickerLocale&) = default;
};

} // namespace ryn
