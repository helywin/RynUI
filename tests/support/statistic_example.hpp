#pragma once
#include <ryn/rynui.hpp>

inline void StatisticExample() {
    ryn::Statistic(
        ryn::StatisticProps{}.title(u8"用户数").value(std::uint64_t{18446744073709551615ULL}).suffix(u8"人"));

    ryn::Signal<ryn::StatisticValue> balance{ryn::StatisticValue{u8"-1234.123456789"}};
    ryn::Statistic(ryn::StatisticProps{}
                       .title(u8"账户余额")
                       .value(balance)
                       .precision(8)
                       .decimalSeparator(u8".")
                       .groupSeparator(u8",")
                       .richPrefix(ryn::StatisticPrefix{[] { ryn::Text(u8"CNY"); }}));

    ryn::Statistic(ryn::StatisticProps{}.value(17).formatter(
        [](const ryn::StatisticValue&) -> ryn::String { return u8"已完成"; }));
}
