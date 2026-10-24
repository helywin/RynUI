#pragma once
#include "layout/layout_engine.hpp"
#include <ryn/skeleton.hpp>
#include <ryn/skeleton_tokens.hpp>

namespace ryn::detail {
inline constexpr std::size_t skeleton_max_rows = 1024;

enum class SkeletonKind { Content, Avatar, Button, Input };

struct SkeletonModel final {
    SkeletonKind kind{SkeletonKind::Content};
    SkeletonTitle title;
    SkeletonParagraph paragraph;
    SkeletonAvatarOptions avatar;
    ControlSize size{ControlSize::Middle};
    SkeletonAvatarShape avatar_shape{SkeletonAvatarShape::Circle};
    SkeletonButtonShape button_shape{SkeletonButtonShape::Default};
    bool round{};
    bool block{};
};

struct SkeletonShape final {
    runtime::Rect bounds;
    float radius{};
};

void validate_skeleton(const SkeletonModel&);
[[nodiscard]] std::size_t skeleton_rows(const SkeletonModel&);
[[nodiscard]] runtime::Size measure_skeleton(const SkeletonModel&, const SkeletonMetricToken&, layout::Constraints);
[[nodiscard]] std::vector<SkeletonShape> arrange_skeleton(const SkeletonModel&, const SkeletonMetricToken&,
                                                          runtime::Rect);
} // namespace ryn::detail
