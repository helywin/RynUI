#include "component/skeleton_model.hpp"
#include <iostream>
#include <limits>

using namespace ryn;
using namespace ryn::detail;

void check(bool value) {
    if (!value) {
        throw std::runtime_error("Skeleton model contract failed");
    }
}

int main() {
    SkeletonModel s;
    SkeletonMetricToken m;
    validate_skeleton(s);
    auto size = measure_skeleton(s, m, {0, 200, 0, 500});
    check(size == runtime::Size{200, 120});
    auto shapes = arrange_skeleton(s, m, {0, 0, size.width, size.height});
    check(shapes.size() == 4 && shapes[0].bounds.width == 76 && shapes[3].bounds.width == 122);
    s.avatar.enabled = true;
    check(skeleton_rows(s) == 2);
    size = measure_skeleton(s, m, {0, 200, 0, 500});
    shapes = arrange_skeleton(s, m, {0, 0, size.width, size.height});
    check(shapes.size() == 4 && shapes[0].bounds.width == 40 && shapes[1].bounds.x == 56);
    check(shapes[1].bounds.width == 72 && shapes[3].bounds.width == 144);
    s.paragraph.rows = 4;
    s.paragraph.widths = {SkeletonWidth::fraction(.2F), SkeletonWidth::fraction(.4F)};
    s.round = true;
    shapes = arrange_skeleton(s, m, {0, 0, 200, 500});
    check(shapes[2].bounds.width == 144 * .2F && shapes[3].bounds.width == 144 * .4F);
    check(shapes[4].bounds.width == 144 && shapes[5].bounds.width == 144 * .61F);
    for (const auto rect : {runtime::Rect{4, 8, 15, 9}, runtime::Rect{4, 8, 0, 0}, runtime::Rect{4, 8, 200, 500}}) {
        for (const auto& shape : arrange_skeleton(s, m, rect)) {
            check(shape.bounds.x >= rect.x && shape.bounds.y >= rect.y && shape.bounds.width >= 0 &&
                  shape.bounds.height >= 0 && shape.bounds.x + shape.bounds.width <= rect.x + rect.width &&
                  shape.bounds.y + shape.bounds.height <= rect.y + rect.height && shape.radius >= 0 &&
                  shape.radius <= std::min(shape.bounds.width, shape.bounds.height) / 2);
        }
    }
    check(measure_skeleton(s, m, {0, std::numeric_limits<float>::infinity(), 0, 500}).width == m.intrinsic_width);
    s.kind = SkeletonKind::Button;
    check(measure_skeleton(s, m, {0, 200, 0, 100}) == runtime::Size{64, 32});
    s.button_shape = SkeletonButtonShape::Circle;
    check(measure_skeleton(s, m, {0, 200, 0, 100}) == runtime::Size{32, 32});
    s.block = true;
    check(measure_skeleton(s, m, {0, 200, 0, 100}) == runtime::Size{200, 32});
    s.block = false;
    s.kind = SkeletonKind::Input;
    check(measure_skeleton(s, m, {0, 200, 0, 100}) == runtime::Size{160, 32});
    s.kind = SkeletonKind::Avatar;
    s.size = ControlSize::Large;
    check(measure_skeleton(s, m, {0, 200, 0, 100}) == runtime::Size{40, 40});
    s.paragraph.rows = 0;
    s.kind = SkeletonKind::Content;
    s.title.enabled = false;
    s.avatar.enabled = false;
    check(measure_skeleton(s, m, {0, 200, 0, 100}) == runtime::Size{});
    std::cout << "Skeleton finite layout and shape contracts passed\n";
}
