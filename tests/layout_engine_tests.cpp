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

void test_external_style_constrains_and_places_without_wrapper() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, ryn::layout::LeafLayout{{80.0F, 20.0F}});
    nodes.require(root).external_layout = {
        40.0F,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        {5.0F, 3.0F, 7.0F, 4.0F},
    };

    const auto size = layout.layout(
        root,
        {0.0F, 100.0F, 0.0F, 100.0F},
        {2.0F, 6.0F});
    require(size == ryn::runtime::Size{52.0F, 27.0F},
            "external width and margin produced the wrong outer size");
    require(nodes.require(root).measured_size == ryn::runtime::Size{40.0F, 20.0F},
            "external width did not constrain leaf content");
    require(nodes.require(root).bounds == ryn::runtime::Rect{7.0F, 9.0F, 40.0F, 20.0F},
            "external margin did not inset Node bounds");
    require(nodes.size() == 1, "external LayoutStyle required a wrapper Node");
}

void test_external_style_clamps_fixed_and_intrinsic_sizes() {
    ryn::runtime::NodeStore nodes;
    const auto fixed = nodes.create_root();
    const auto intrinsic = nodes.create_root();
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(fixed, ryn::layout::LeafLayout{{80.0F, 30.0F}});
    layout.set_layout(intrinsic, ryn::layout::LeafLayout{{5.0F, 5.0F}});

    nodes.require(fixed).external_layout.width = 20.0F;
    nodes.require(fixed).external_layout.min_width = 40.0F;
    nodes.require(fixed).external_layout.max_width = 60.0F;
    require(layout.measure(fixed, {0.0F, 100.0F, 0.0F, 100.0F})
                == ryn::runtime::Size{40.0F, 30.0F},
            "fixed width did not clamp to LayoutStyle min width");

    nodes.require(intrinsic).external_layout.min_width = 30.0F;
    nodes.require(intrinsic).external_layout.max_width = 50.0F;
    layout.set_intrinsic_measure(
        intrinsic,
        1,
        [](ryn::layout::Constraints constraints) {
            require(constraints.min_width == 30.0F
                        && constraints.max_width == 50.0F,
                    "intrinsic adapter received the wrong min/max constraints");
            return ryn::runtime::Size{90.0F, 12.0F};
        });
    require(layout.measure(intrinsic, {0.0F, 100.0F, 0.0F, 100.0F})
                == ryn::runtime::Size{50.0F, 12.0F},
            "intrinsic result did not clamp to LayoutStyle max width");
}

void test_intrinsic_measure_cache_revision_and_generation() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, ryn::layout::LeafLayout{{5.0F, 5.0F}});
    int calls = 0;
    layout.set_intrinsic_measure(root, 1, [&](ryn::layout::Constraints constraints) {
        ++calls;
        return ryn::runtime::Size{80.0F, constraints.min_height + 20.0F};
    });

    const ryn::layout::Constraints wide{0.0F, 100.0F, 0.0F, 100.0F};
    require(layout.measure(root, wide) == ryn::runtime::Size{80.0F, 20.0F}
                && calls == 1,
            "intrinsic adapter did not receive constraints");
    static_cast<void>(layout.measure(root, wide));
    require(calls == 1,
            "same intrinsic revision and constraints missed the cache");
    require(layout.set_intrinsic_revision(root, 2),
            "intrinsic revision change was ignored");
    static_cast<void>(layout.measure(root, wide));
    require(calls == 2, "intrinsic revision did not invalidate the cache");
    static_cast<void>(layout.measure(root, {0.0F, 50.0F, 0.0F, 100.0F}));
    require(calls == 3 && nodes.require(root).measured_size.width == 50.0F,
            "intrinsic constraint change did not remeasure and constrain");

    const auto stale = root;
    require(nodes.destroy(stale), "intrinsic test Node could not be destroyed");
    const auto replacement = nodes.create_root();
    require(replacement.index == stale.index
                && replacement.generation != stale.generation,
            "intrinsic test did not reuse Node generation");
    layout.set_layout(replacement, ryn::layout::LeafLayout{{12.0F, 6.0F}});
    static_cast<void>(layout.measure(
        replacement,
        {0.0F, 100.0F, 0.0F, 100.0F}));
    require(calls == 3
                && nodes.require(replacement).measured_size
                    == ryn::runtime::Size{12.0F, 6.0F},
            "stale Node generation invoked an old intrinsic adapter");
    require(!layout.remove_intrinsic_measure(stale),
            "stale Node generation removed a replacement adapter");
}

void test_recursive_intrinsic_measure_fails_fast_and_recovers() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, ryn::layout::LeafLayout{{1.0F, 1.0F}});
    const ryn::layout::Constraints constraints{0.0F, 100.0F, 0.0F, 100.0F};
    layout.set_intrinsic_measure(root, 1, [&](ryn::layout::Constraints inner) {
        return layout.measure(root, inner);
    });

    bool diagnosed = false;
    try {
        static_cast<void>(layout.measure(root, constraints));
    } catch (const std::logic_error&) {
        diagnosed = true;
    }
    require(diagnosed, "recursive intrinsic measurement did not fail fast");

    layout.set_intrinsic_measure(root, 2, [](ryn::layout::Constraints) {
        return ryn::runtime::Size{20.0F, 10.0F};
    });
    require(layout.measure(root, constraints) == ryn::runtime::Size{20.0F, 10.0F},
            "intrinsic adapter did not recover after recursion failure");
    require(layout.remove_intrinsic_measure(root),
            "live intrinsic adapter could not be removed");
}

} // namespace

int main() {
    try {
        test_invalid_constraints_are_diagnosed_before_measure();
        test_box_measure_and_place_with_padding();
        test_box_fill_uses_bounded_maximum();
        test_horizontal_and_vertical_flex_are_deterministic();
        test_flex_children_do_not_exceed_constraints();
        test_external_style_constrains_and_places_without_wrapper();
        test_external_style_clamps_fixed_and_intrinsic_sizes();
        test_intrinsic_measure_cache_revision_and_generation();
        test_recursive_intrinsic_measure_fails_fast_and_recovers();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
