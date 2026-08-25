#include "layout/layout_engine.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_invalid_constraints_are_diagnosed_before_measure() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, ryn::layout::LeafLayout{{10.0F, 10.0F}});

    bool inverted_diagnosed = false;
    try {
        static_cast<void>(layout.measure(root, {20.0F, 10.0F, 0.0F, 10.0F}));
    } catch (const std::invalid_argument&) {
        inverted_diagnosed = true;
    }
    require(inverted_diagnosed, "inverted Constraints were accepted");
    require(nodes.require(root).measure_count == 0, "invalid Constraints mutated layout state");

    bool nan_diagnosed = false;
    try {
        static_cast<void>(layout.measure(root, {
            0.0F,
            std::numeric_limits<float>::quiet_NaN(),
            0.0F,
            10.0F,
        }));
    } catch (const std::invalid_argument&) {
        nan_diagnosed = true;
    }
    require(nan_diagnosed, "NaN Constraints were accepted");
}

void test_box_measure_and_place_with_padding() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto child = nodes.create_child(root);
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, ryn::layout::BoxLayout{{5.0F, 10.0F, 5.0F, 10.0F}});
    layout.set_layout(child, ryn::layout::LeafLayout{{30.0F, 20.0F}});

    const auto size = layout.layout(root, {0.0F, 100.0F, 0.0F, 100.0F}, {2.0F, 3.0F});

    require(size == ryn::runtime::Size{40.0F, 40.0F}, "Box measured size is incorrect");
    require(nodes.require(root).bounds == ryn::runtime::Rect{2.0F, 3.0F, 40.0F, 40.0F},
            "Box bounds are incorrect");
    require(nodes.require(child).bounds == ryn::runtime::Rect{7.0F, 13.0F, 30.0F, 20.0F},
            "Box child placement is incorrect");
}

void test_box_fill_uses_bounded_maximum() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto child = nodes.create_child(root);
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, ryn::layout::BoxLayout{
        {5.0F, 5.0F, 5.0F, 5.0F},
        true,
        false,
    });
    layout.set_layout(child, ryn::layout::LeafLayout{{20.0F, 10.0F}});

    const auto size = layout.layout(root, {0.0F, 100.0F, 0.0F, 50.0F});
    require(size == ryn::runtime::Size{100.0F, 20.0F},
            "Box fill did not use the bounded maximum width");
}

void test_horizontal_and_vertical_flex_are_deterministic() {
    ryn::runtime::NodeStore nodes;
    const auto horizontal = nodes.create_root();
    const auto first = nodes.create_child(horizontal);
    const auto second = nodes.create_child(horizontal);
    const auto vertical = nodes.create_root();
    const auto top = nodes.create_child(vertical);
    const auto bottom = nodes.create_child(vertical);
    ryn::layout::LayoutEngine layout(nodes);

    layout.set_layout(horizontal, ryn::layout::FlexLayout{
        ryn::layout::FlexDirection::horizontal,
        10.0F,
    });
    layout.set_layout(first, ryn::layout::LeafLayout{{30.0F, 20.0F}});
    layout.set_layout(second, ryn::layout::LeafLayout{{40.0F, 25.0F}});
    static_cast<void>(layout.layout(horizontal, ryn::layout::Constraints::fixed(100.0F, 40.0F)));

    require(nodes.require(first).bounds == ryn::runtime::Rect{0.0F, 0.0F, 30.0F, 20.0F},
            "horizontal Flex first child bounds are incorrect");
    require(nodes.require(second).bounds == ryn::runtime::Rect{40.0F, 0.0F, 40.0F, 25.0F},
            "horizontal Flex second child bounds are incorrect");

    layout.set_layout(vertical, ryn::layout::FlexLayout{
        ryn::layout::FlexDirection::vertical,
        5.0F,
    });
    layout.set_layout(top, ryn::layout::LeafLayout{{20.0F, 10.0F}});
    layout.set_layout(bottom, ryn::layout::LeafLayout{{25.0F, 15.0F}});
    static_cast<void>(layout.layout(vertical, ryn::layout::Constraints::fixed(40.0F, 50.0F)));

    require(nodes.require(top).bounds == ryn::runtime::Rect{0.0F, 0.0F, 20.0F, 10.0F},
            "vertical Flex first child bounds are incorrect");
    require(nodes.require(bottom).bounds == ryn::runtime::Rect{0.0F, 15.0F, 25.0F, 15.0F},
            "vertical Flex second child bounds are incorrect");
}

void test_flex_children_do_not_exceed_constraints() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto first = nodes.create_child(root);
    const auto second = nodes.create_child(root);
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, ryn::layout::FlexLayout{
        ryn::layout::FlexDirection::horizontal,
        10.0F,
    });
    layout.set_layout(first, ryn::layout::LeafLayout{{80.0F, 20.0F}});
    layout.set_layout(second, ryn::layout::LeafLayout{{80.0F, 20.0F}});

    static_cast<void>(layout.layout(root, ryn::layout::Constraints::fixed(100.0F, 20.0F)));
    const auto second_bounds = nodes.require(second).bounds;
    require(second_bounds.x + second_bounds.width <= 100.0F,
            "Flex child exceeded the horizontal Constraints");
    require(nodes.require(root).measure_count == 1 && nodes.require(root).place_count == 1,
            "layout pass counters are incorrect");
}

} // namespace

int main() {
    try {
        test_invalid_constraints_are_diagnosed_before_measure();
        test_box_measure_and_place_with_padding();
        test_box_fill_uses_bounded_maximum();
        test_horizontal_and_vertical_flex_are_deterministic();
        test_flex_children_do_not_exceed_constraints();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
