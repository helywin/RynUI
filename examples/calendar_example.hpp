#pragma once
#include <ryn/rynui.hpp>

inline void CalendarExample() {
    using namespace ryn;
    Calendar(
        CalendarProps{}
            .defaultValue({2024, 2, 29})
            .fullscreen(false)
            .showWeek()
            .clock([] { return CivilDate{2024, 2, 29}; })
            .showToday()
            .cellRender(CalendarCellRenderer{[](Binding<CalendarCellInfo> info) {
                return CalendarCell{[info] {
                    Text(TextProps{}.content(bind([info] { return info.get().today ? String{u8"Today"} : String{}; })));
                }};
            }}));
}
