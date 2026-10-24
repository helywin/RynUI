#include "component/skeleton_model.hpp"
#include <algorithm>

namespace ryn::detail {
namespace {
void validate(ControlSize size) {
    if (size != ControlSize::Small && size != ControlSize::Middle && size != ControlSize::Large) {
        throw std::invalid_argument("Skeleton size is invalid");
    }
}

float height(ControlSize size, const SkeletonMetricToken& m) {
    return size == ControlSize::Small   ? m.control_height_small
           : size == ControlSize::Large ? m.control_height_large
                                        : m.control_height;
}

float width(SkeletonWidth value, float available, float fallback) {
    return available * value.fraction().value_or(fallback);
}
} // namespace

void validate_skeleton(const SkeletonModel& s) {
    validate(s.size);
    validate(s.avatar.size);
    if ((s.avatar.shape && *s.avatar.shape != SkeletonAvatarShape::Circle &&
         *s.avatar.shape != SkeletonAvatarShape::Square) ||
        (s.avatar_shape != SkeletonAvatarShape::Circle && s.avatar_shape != SkeletonAvatarShape::Square) ||
        s.button_shape < SkeletonButtonShape::Default || s.button_shape > SkeletonButtonShape::Square) {
        throw std::invalid_argument("Skeleton shape is invalid");
    }
    if (s.paragraph.rows.value_or(0) > skeleton_max_rows || s.paragraph.widths.size() > skeleton_max_rows) {
        throw std::invalid_argument("Skeleton supports at most 1024 paragraph rows");
    }
}

std::size_t skeleton_rows(const SkeletonModel& s) {
    return s.paragraph.enabled ? s.paragraph.rows.value_or(!s.avatar.enabled && s.title.enabled ? 3 : 2) : 0;
}

runtime::Size measure_skeleton(const SkeletonModel& s, const SkeletonMetricToken& m, layout::Constraints limits) {
    if (s.kind != SkeletonKind::Content) {
        const auto h = height(s.size, m);
        const auto w = s.kind == SkeletonKind::Avatar ||
                               (s.kind == SkeletonKind::Button && s.button_shape == SkeletonButtonShape::Circle)
                           ? h
                           : h * (s.kind == SkeletonKind::Input ? 5 : 2);
        return limits.constrain({s.block && std::isfinite(limits.max_width) ? limits.max_width : w, h});
    }
    const auto rows = skeleton_rows(s);
    float body = s.title.enabled ? m.title_height + (s.avatar.enabled ? m.avatar_title_offset : 0) : 0;
    if (rows > 0) {
        body += s.title.enabled ? (s.avatar.enabled ? m.paragraph_margin_top : m.title_paragraph_gap) : 0;
        body += static_cast<float>(rows) * m.paragraph_line_height + static_cast<float>(rows - 1) * m.row_gap;
    }
    const auto avatar = s.avatar.enabled ? height(s.avatar.size, m) : 0;
    const bool has_body = s.title.enabled || rows > 0;
    const auto natural_width = has_body ? m.intrinsic_width : avatar;
    return limits.constrain(
        {has_body && std::isfinite(limits.max_width) ? limits.max_width : natural_width, std::max(avatar, body)});
}

std::vector<SkeletonShape> arrange_skeleton(const SkeletonModel& s, const SkeletonMetricToken& m, runtime::Rect r) {
    std::vector<SkeletonShape> result;
    const auto add = [&](runtime::Rect bounds, float radius) {
        result.push_back({bounds, std::min(radius, std::min(bounds.width, bounds.height) / 2)});
    };
    if (s.kind != SkeletonKind::Content) {
        const bool capsule = (s.kind == SkeletonKind::Avatar && s.avatar_shape == SkeletonAvatarShape::Circle) ||
                             (s.kind == SkeletonKind::Button && (s.button_shape == SkeletonButtonShape::Circle ||
                                                                 s.button_shape == SkeletonButtonShape::Round));
        const auto radius = capsule                          ? std::min(r.width, r.height) / 2
                            : s.kind == SkeletonKind::Avatar ? 0
                                                             : m.control_radius;
        add(r, radius);
        return result;
    }
    const auto rows = skeleton_rows(s);
    float left = r.x;
    float available = r.width;
    if (s.avatar.enabled) {
        const auto side = std::min({height(s.avatar.size, m), r.width, r.height});
        const auto shape = s.avatar.shape.value_or(
            s.title.enabled && !s.paragraph.enabled ? SkeletonAvatarShape::Square : SkeletonAvatarShape::Circle);
        add({left, r.y, side, side}, shape == SkeletonAvatarShape::Circle ? side / 2 : 0);
        const auto reserved = side + ((s.title.enabled || rows > 0) ? std::min(m.avatar_gap, available - side) : 0);
        left += reserved;
        available -= reserved;
    }
    float y = r.y;
    const auto bottom = r.y + r.height;
    const auto gap = [&](float value) {
        y += std::min(value, std::max(0.0F, bottom - y));
    };
    const auto line = [&](float w, float h) {
        const auto actual = std::min(h, std::max(0.0F, bottom - y));
        add({left, y, w, actual}, s.round ? actual / 2 : m.block_radius);
        y += actual;
    };
    if (s.title.enabled) {
        if (s.avatar.enabled) {
            gap(m.avatar_title_offset);
        }
        line(width(s.title.width, available, s.paragraph.enabled ? (s.avatar.enabled ? .5F : .38F) : 1),
             m.title_height);
        if (rows > 0) {
            gap(s.avatar.enabled ? m.paragraph_margin_top : m.title_paragraph_gap);
        }
    }
    for (std::size_t i = 0; i < rows; ++i) {
        if (i > 0) {
            gap(m.row_gap);
        }
        float fraction = 1;
        if (!s.paragraph.widths.empty()) {
            if (i < s.paragraph.widths.size()) {
                fraction = s.paragraph.widths[i].fraction().value_or(i + 1 == rows && rows > 2 ? .61F : 1);
            } else if (i + 1 == rows && rows > 2) {
                fraction = .61F;
            }
        } else if (i + 1 == rows) {
            fraction = s.paragraph.last_line_width.fraction().value_or(
                !s.avatar.enabled || !s.title.enabled || rows > 2 ? .61F : 1);
        }
        line(available * fraction, m.paragraph_line_height);
    }
    return result;
}
} // namespace ryn::detail
