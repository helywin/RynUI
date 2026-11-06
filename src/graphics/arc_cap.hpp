#pragma once

#include "graphics/rounded_effect.hpp"

namespace ryn::graphics {

// Exact exposed boundary of the non-round circular stroke union. Kept apart
// from the legacy round centerline-distance path.
[[nodiscard]] float nonround_arc_signed_distance(runtime::Point point, LogicalRoundedRect centerline, float width,
                                                 RoundedArcPath arc) noexcept;

} // namespace ryn::graphics
