#pragma once

#include <ryn/icon.hpp>

namespace rynui::example {
inline ryn::IconSource icon_vector_sample() {
    using namespace ryn;
    static const IconVector vector{
        {0, 0, 100, 100},
        {{IconColorRole::Primary,
          {IconMove{{10, 10}}, IconLine{{90, 10}}, IconLine{{90, 90}}, IconLine{{10, 90}}, IconClose{},
           IconMove{{35, 35}}, IconLine{{35, 65}}, IconLine{{65, 65}}, IconLine{{65, 35}}, IconClose{}}},
         {IconColorRole::Secondary,
          {IconMove{{10, 75}}, IconQuadratic{{50, 20}, {90, 75}}, IconLine{{90, 90}}, IconLine{{10, 90}}, IconClose{}},
          .7F},
         {IconColorRole::Primary,
          {IconMove{{20, 20}}, IconCubic{{20, 0}, {80, 0}, {80, 20}}, IconLine{{80, 25}}, IconLine{{20, 25}},
           IconClose{}}}}};
    return IconSource{vector};
}

inline ryn::IconSource icon_wide_sample() {
    using namespace ryn;
    static const IconVector vector{
        {10, -10, 200, 100},
        {{IconColorRole::Primary,
          {IconMove{{10, -10}}, IconLine{{210, -10}}, IconLine{{210, 90}}, IconLine{{10, 90}}, IconClose{},
           IconMove{{75, 15}}, IconLine{{75, 65}}, IconLine{{145, 65}}, IconLine{{145, 15}}, IconClose{}}}}};
    return IconSource{vector};
}
} // namespace rynui::example
