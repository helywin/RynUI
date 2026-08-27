#include "layout/layout_engine.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float left, float right, float epsilon = 0.001F) {
    return std::abs(left - right) <= epsilon;
}

ryn::layout::FlexLayout flex_layout(
    ryn::layout::FlexDirection direction = ryn::layout::FlexDirection::horizontal,
    float main_gap = 0.0F,
    ryn::layout::FlexWrap wrap = ryn::layout::FlexWrap::no_wrap,
    ryn::layout::FlexJustify justify = ryn::layout::FlexJustify::start,
    ryn::layout::FlexAlign align = ryn::layout::FlexAlign::start,
    float cross_gap = 0.0F) {
    ryn::layout::FlexLayout result;
    result.direction = direction;
    result.main_gap = main_gap;
    result.wrap = wrap;
    result.justify = justify;
    result.align = align;
    result.cross_gap = cross_gap;
    return result;
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

    layout.set_layout(horizontal, flex_layout(
        ryn::layout::FlexDirection::horizontal,
        10.0F));
    layout.set_layout(first, ryn::layout::LeafLayout{{30.0F, 20.0F}});
    layout.set_layout(second, ryn::layout::LeafLayout{{40.0F, 25.0F}});
    static_cast<void>(layout.layout(horizontal, ryn::layout::Constraints::fixed(100.0F, 40.0F)));

    require(nodes.require(first).bounds == ryn::runtime::Rect{0.0F, 0.0F, 30.0F, 20.0F},
            "horizontal Flex first child bounds are incorrect");
    require(nodes.require(second).bounds == ryn::runtime::Rect{40.0F, 0.0F, 40.0F, 25.0F},
            "horizontal Flex second child bounds are incorrect");

    layout.set_layout(vertical, flex_layout(
        ryn::layout::FlexDirection::vertical,
        5.0F));
    layout.set_layout(top, ryn::layout::LeafLayout{{20.0F, 10.0F}});
    layout.set_layout(bottom, ryn::layout::LeafLayout{{25.0F, 15.0F}});
    static_cast<void>(layout.layout(vertical, ryn::layout::Constraints::fixed(40.0F, 50.0F)));

    require(nodes.require(top).bounds == ryn::runtime::Rect{0.0F, 0.0F, 20.0F, 10.0F},
            "vertical Flex first child bounds are incorrect");
    require(nodes.require(bottom).bounds == ryn::runtime::Rect{0.0F, 15.0F, 25.0F, 15.0F},
            "vertical Flex second child bounds are incorrect");
}

void test_flex_model_validation_equality_and_empty_layout() {
    const ryn::layout::FlexLayout defaults;
    require(defaults == ryn::layout::FlexLayout{},
            "equal Flex layout values compared unequal");
    auto changed = defaults;
    changed.cross_gap = 4.0F;
    require(changed != defaults,
            "different Flex layout values compared equal");

    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    ryn::layout::LayoutEngine layout(nodes);
    auto expect_invalid = [&](ryn::layout::FlexLayout value) {
        bool diagnosed = false;
        try {
            layout.set_layout(root, value);
        } catch (const std::invalid_argument&) {
            diagnosed = true;
        }
        require(diagnosed, "invalid Flex layout value was accepted");
    };

    auto invalid = defaults;
    invalid.main_gap = -1.0F;
    expect_invalid(invalid);
    invalid = defaults;
    invalid.cross_gap = std::numeric_limits<float>::infinity();
    expect_invalid(invalid);
    invalid = defaults;
    invalid.padding.left = std::numeric_limits<float>::infinity();
    expect_invalid(invalid);
    invalid = defaults;
    invalid.wrap = static_cast<ryn::layout::FlexWrap>(99);
    expect_invalid(invalid);
    invalid = defaults;
    invalid.justify = static_cast<ryn::layout::FlexJustify>(99);
    expect_invalid(invalid);
    invalid = defaults;
    invalid.align = static_cast<ryn::layout::FlexAlign>(99);
    expect_invalid(invalid);

    auto empty = defaults;
    empty.padding = {2.0F, 3.0F, 5.0F, 7.0F};
    layout.set_layout(root, empty);
    require(layout.layout(root, {
                    0.0F,
                    std::numeric_limits<float>::infinity(),
                    0.0F,
                    std::numeric_limits<float>::infinity(),
                }) == ryn::runtime::Size{7.0F, 10.0F}
                && layout.flex_layout_diagnostics(root).line_count == 0,
            "empty Flex did not resolve to padding-only logical size");
}

void test_flex_wrap_is_greedy_and_reuses_line_scratch() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto first = nodes.create_child(root);
    const auto second = nodes.create_child(root);
    const auto third = nodes.create_child(root);
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, flex_layout(
        ryn::layout::FlexDirection::horizontal,
        10.0F,
        ryn::layout::FlexWrap::wrap,
        ryn::layout::FlexJustify::start,
        ryn::layout::FlexAlign::start,
        5.0F));
    layout.set_layout(first, ryn::layout::LeafLayout{{40.0F, 10.0F}});
    layout.set_layout(second, ryn::layout::LeafLayout{{40.0F, 20.0F}});
    layout.set_layout(third, ryn::layout::LeafLayout{{40.0F, 15.0F}});

    static_cast<void>(layout.layout(
        root,
        ryn::layout::Constraints::fixed(100.0F, 60.0F)));
    const auto initial = layout.flex_layout_diagnostics(root);
    require(initial.item_count == 3 && initial.line_count == 2,
            "greedy Flex wrap formed the wrong lines");
    require(nodes.require(first).bounds == ryn::runtime::Rect{0.0F, 0.0F, 40.0F, 10.0F}
                && nodes.require(second).bounds
                    == ryn::runtime::Rect{50.0F, 0.0F, 40.0F, 20.0F}
                && nodes.require(third).bounds
                    == ryn::runtime::Rect{0.0F, 25.0F, 40.0F, 15.0F},
            "wrapped Flex bounds or cross gap are incorrect");

    for (int iteration = 0; iteration < 64; ++iteration) {
        static_cast<void>(layout.layout(
            root,
            ryn::layout::Constraints::fixed(100.0F, 60.0F)));
    }
    const auto stable = layout.flex_layout_diagnostics(root);
    require(stable.item_capacity == initial.item_capacity
                && stable.line_capacity == initial.line_capacity,
            "stable Flex child count reallocated item or line scratch");

    const auto unconstrained = layout.layout(root, {
        0.0F,
        std::numeric_limits<float>::infinity(),
        0.0F,
        std::numeric_limits<float>::infinity(),
    });
    require(unconstrained == ryn::runtime::Size{140.0F, 20.0F}
                && layout.flex_layout_diagnostics(root).line_count == 1,
            "infinite Flex main constraint incorrectly wrapped children");

    ryn::runtime::NodeStore oversized_nodes;
    const auto oversized_root = oversized_nodes.create_root();
    const auto oversized = oversized_nodes.create_child(oversized_root);
    const auto follower = oversized_nodes.create_child(oversized_root);
    ryn::layout::LayoutEngine oversized_layout(oversized_nodes);
    oversized_layout.set_layout(oversized_root, flex_layout(
        ryn::layout::FlexDirection::horizontal,
        8.0F,
        ryn::layout::FlexWrap::wrap));
    oversized_layout.set_layout(oversized, ryn::layout::LeafLayout{{10.0F, 10.0F}});
    oversized_layout.set_layout(follower, ryn::layout::LeafLayout{{10.0F, 10.0F}});
    oversized_nodes.require(oversized).external_layout.margin = {
        60.0F, 0.0F, 60.0F, 0.0F};
    static_cast<void>(oversized_layout.layout(
        oversized_root,
        ryn::layout::Constraints::fixed(100.0F, 30.0F)));
    require(oversized_layout.flex_layout_diagnostics(oversized_root).line_count == 2
                && oversized_nodes.require(follower).bounds.y == 10.0F,
            "single oversized Flex child did not occupy a deterministic line");
}

void test_flex_justify_modes_and_fractional_stability() {
    struct JustifyCase final {
        ryn::layout::FlexJustify justify;
        std::array<float, 3> x;
    };
    const std::array cases{
        JustifyCase{ryn::layout::FlexJustify::start, {0.0F, 20.0F, 40.0F}},
        JustifyCase{ryn::layout::FlexJustify::center, {25.0F, 45.0F, 65.0F}},
        JustifyCase{ryn::layout::FlexJustify::end, {50.0F, 70.0F, 90.0F}},
        JustifyCase{ryn::layout::FlexJustify::space_between, {0.0F, 45.0F, 90.0F}},
        JustifyCase{ryn::layout::FlexJustify::space_around,
            {8.333333F, 45.0F, 81.666664F}},
        JustifyCase{ryn::layout::FlexJustify::space_evenly,
            {12.5F, 45.0F, 77.5F}},
    };

    for (const auto& test : cases) {
        ryn::runtime::NodeStore nodes;
        const auto root = nodes.create_root();
        const std::array children{
            nodes.create_child(root),
            nodes.create_child(root),
            nodes.create_child(root),
        };
        ryn::layout::LayoutEngine layout(nodes);
        layout.set_layout(root, flex_layout(
            ryn::layout::FlexDirection::horizontal,
            10.0F,
            ryn::layout::FlexWrap::no_wrap,
            test.justify));
        for (const auto child : children) {
            layout.set_layout(child, ryn::layout::LeafLayout{{10.0F, 10.0F}});
        }
        static_cast<void>(layout.layout(
            root,
            ryn::layout::Constraints::fixed(100.0F, 20.0F)));
        for (std::size_t index = 0; index < children.size(); ++index) {
            require(near(nodes.require(children[index]).bounds.x, test.x[index]),
                    "Flex justify produced the wrong logical x coordinate");
        }

        const auto first_run = nodes.require(children[0]).bounds;
        for (int iteration = 0; iteration < 32; ++iteration) {
            static_cast<void>(layout.layout(
                root,
                ryn::layout::Constraints::fixed(100.0F, 20.0F)));
        }
        require(nodes.require(children[0]).bounds == first_run,
                "fractional Flex justify bounds changed across repeated layouts");
    }

    for (const auto justify : {
            ryn::layout::FlexJustify::space_between,
            ryn::layout::FlexJustify::space_around,
            ryn::layout::FlexJustify::space_evenly}) {
        ryn::runtime::NodeStore nodes;
        const auto root = nodes.create_root();
        const auto child = nodes.create_child(root);
        ryn::layout::LayoutEngine layout(nodes);
        layout.set_layout(root, flex_layout(
            ryn::layout::FlexDirection::horizontal,
            0.0F,
            ryn::layout::FlexWrap::no_wrap,
            justify));
        layout.set_layout(child, ryn::layout::LeafLayout{{10.0F, 10.0F}});
        static_cast<void>(layout.layout(
            root,
            ryn::layout::Constraints::fixed(100.0F, 20.0F)));
        const float expected = justify == ryn::layout::FlexJustify::space_between
            ? 0.0F
            : 45.0F;
        require(near(nodes.require(child).bounds.x, expected),
                "single-item distributed justify is incorrect");
    }
}

void test_flex_align_modes_and_stretch_constraints() {
    struct AlignCase final {
        ryn::layout::FlexAlign align;
        float first_y;
        float first_height;
        float second_y;
    };
    const std::array cases{
        AlignCase{ryn::layout::FlexAlign::start, 0.0F, 10.0F, 0.0F},
        AlignCase{ryn::layout::FlexAlign::center, 15.0F, 10.0F, 10.0F},
        AlignCase{ryn::layout::FlexAlign::end, 30.0F, 10.0F, 20.0F},
        AlignCase{ryn::layout::FlexAlign::stretch, 0.0F, 25.0F, 0.0F},
    };

    for (const auto& test : cases) {
        ryn::runtime::NodeStore nodes;
        const auto root = nodes.create_root();
        const auto first = nodes.create_child(root);
        const auto second = nodes.create_child(root);
        ryn::layout::LayoutEngine layout(nodes);
        layout.set_layout(root, flex_layout(
            ryn::layout::FlexDirection::horizontal,
            5.0F,
            ryn::layout::FlexWrap::no_wrap,
            ryn::layout::FlexJustify::start,
            test.align));
        layout.set_layout(first, ryn::layout::LeafLayout{{10.0F, 10.0F}});
        layout.set_layout(second, ryn::layout::LeafLayout{{10.0F, 20.0F}});
        nodes.require(first).external_layout.max_height = 25.0F;
        nodes.require(second).external_layout.height = 20.0F;
        static_cast<void>(layout.layout(
            root,
            ryn::layout::Constraints::fixed(100.0F, 40.0F)));
        require(near(nodes.require(first).bounds.y, test.first_y)
                    && near(nodes.require(first).bounds.height, test.first_height)
                    && near(nodes.require(second).bounds.y, test.second_y)
                    && near(nodes.require(second).bounds.height, 20.0F),
                "Flex cross-axis align or constrained stretch is incorrect");
    }

    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto first = nodes.create_child(root);
    const auto second = nodes.create_child(root);
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, flex_layout(
        ryn::layout::FlexDirection::vertical,
        5.0F,
        ryn::layout::FlexWrap::no_wrap,
        ryn::layout::FlexJustify::start,
        ryn::layout::FlexAlign::center));
    layout.set_layout(first, ryn::layout::LeafLayout{{10.0F, 10.0F}});
    layout.set_layout(second, ryn::layout::LeafLayout{{20.0F, 10.0F}});
    static_cast<void>(layout.layout(
        root,
        ryn::layout::Constraints::fixed(40.0F, 50.0F)));
    require(nodes.require(first).bounds.x == 15.0F
                && nodes.require(second).bounds.x == 10.0F,
            "vertical Flex did not center children on its cross axis");
}

void test_flex_dual_axis_gaps_and_nested_logical_bounds() {
    for (const float gap : {8.0F, 16.0F, 24.0F, 13.5F}) {
        ryn::runtime::NodeStore nodes;
        const auto root = nodes.create_root();
        const auto first = nodes.create_child(root);
        const auto second = nodes.create_child(root);
        ryn::layout::LayoutEngine layout(nodes);
        layout.set_layout(root, flex_layout(
            ryn::layout::FlexDirection::horizontal,
            2.0F,
            ryn::layout::FlexWrap::wrap,
            ryn::layout::FlexJustify::start,
            ryn::layout::FlexAlign::start,
            gap));
        layout.set_layout(first, ryn::layout::LeafLayout{{10.0F, 10.0F}});
        layout.set_layout(second, ryn::layout::LeafLayout{{10.0F, 10.0F}});
        static_cast<void>(layout.layout(
            root,
            ryn::layout::Constraints::fixed(15.0F, 60.0F)));
        require(nodes.require(first).bounds.x == 0.0F
                    && nodes.require(first).bounds.y == 0.0F
                    && nodes.require(second).bounds.x == 0.0F
                    && near(nodes.require(second).bounds.y, 10.0F + gap),
                "Flex cross gap added an edge gap or used the wrong value");
    }

    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto nested = nodes.create_child(root);
    const auto sibling = nodes.create_child(root);
    const auto top = nodes.create_child(nested);
    const auto bottom = nodes.create_child(nested);
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, flex_layout(
        ryn::layout::FlexDirection::horizontal,
        8.0F));
    layout.set_layout(nested, flex_layout(
        ryn::layout::FlexDirection::vertical,
        8.0F));
    layout.set_layout(sibling, ryn::layout::LeafLayout{{40.0F, 10.0F}});
    layout.set_layout(top, ryn::layout::LeafLayout{{10.0F, 10.0F}});
    layout.set_layout(bottom, ryn::layout::LeafLayout{{10.0F, 10.0F}});
    static_cast<void>(layout.layout(
        root,
        ryn::layout::Constraints::fixed(60.0F, 40.0F),
        {3.0F, 4.0F}));
    require(nodes.require(nested).bounds
                    == ryn::runtime::Rect{3.0F, 4.0F, 10.0F, 28.0F}
                && nodes.require(top).bounds
                    == ryn::runtime::Rect{3.0F, 4.0F, 10.0F, 10.0F}
                && nodes.require(bottom).bounds
                    == ryn::runtime::Rect{3.0F, 22.0F, 10.0F, 10.0F}
                && nodes.require(sibling).bounds
                    == ryn::runtime::Rect{21.0F, 4.0F, 40.0F, 10.0F},
            "nested Flex logical bounds or clipping are incorrect");
}

void test_flex_children_do_not_exceed_constraints() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto first = nodes.create_child(root);
    const auto second = nodes.create_child(root);
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, flex_layout(
        ryn::layout::FlexDirection::horizontal,
        10.0F));
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

void test_horizontal_content_centers_children_with_token_metrics() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto first = nodes.create_child(root);
    const auto second = nodes.create_child(root);
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, ryn::layout::HorizontalContentLayout{
        32.0F,
        15.0F,
        1.0F,
        8.0F,
        false,
        14.0F,
    });
    layout.set_layout(first, ryn::layout::LeafLayout{{20.0F, 10.0F}});
    layout.set_layout(second, ryn::layout::LeafLayout{{30.0F, 14.0F}});
    nodes.require(root).external_layout.width = 120.0F;
    nodes.require(root).external_layout.margin = {10.0F, 5.0F, 6.0F, 7.0F};

    const auto outer = layout.layout(
        root,
        {0.0F, 200.0F, 0.0F, 100.0F},
        {3.0F, 4.0F});
    require(outer == ryn::runtime::Size{136.0F, 44.0F}
                && nodes.require(root).bounds
                    == ryn::runtime::Rect{13.0F, 9.0F, 120.0F, 32.0F},
            "horizontal content root ignored fixed width or external margin");
    require(nodes.require(first).bounds
                    == ryn::runtime::Rect{44.0F, 20.0F, 20.0F, 10.0F}
                && nodes.require(second).bounds
                    == ryn::runtime::Rect{72.0F, 18.0F, 30.0F, 14.0F},
            "horizontal content did not center the child group on both axes");
    require(layout.horizontal_content_geometry(root)
                    == ryn::layout::HorizontalContentGeometry{
                        {29.0F, 10.0F, 88.0F, 30.0F},
                        std::nullopt,
                    },
            "horizontal content retained the wrong local geometry");
}

void test_horizontal_content_sizes_empty_and_constrained_content() {
    const std::array models{
        ryn::layout::HorizontalContentLayout{24.0F, 7.0F, 1.0F, 8.0F, false, 14.0F},
        ryn::layout::HorizontalContentLayout{32.0F, 15.0F, 1.0F, 8.0F, false, 14.0F},
        ryn::layout::HorizontalContentLayout{40.0F, 15.0F, 1.0F, 8.0F, false, 14.0F},
    };
    const std::array expected{
        ryn::runtime::Size{16.0F, 24.0F},
        ryn::runtime::Size{32.0F, 32.0F},
        ryn::runtime::Size{32.0F, 40.0F},
    };
    for (std::size_t index = 0; index < models.size(); ++index) {
        ryn::runtime::NodeStore nodes;
        const auto root = nodes.create_root();
        ryn::layout::LayoutEngine layout(nodes);
        layout.set_layout(root, models[index]);
        require(layout.layout(root, {0.0F, 200.0F, 0.0F, 100.0F})
                    == expected[index],
                "empty horizontal content lost its size token contract");
    }

    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto cjk = nodes.create_child(root);
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, models[1]);
    layout.set_layout(cjk, ryn::layout::LeafLayout{});
    int intrinsic_calls = 0;
    float received_max = 0.0F;
    layout.set_intrinsic_measure(cjk, 1, [&](ryn::layout::Constraints constraints) {
        ++intrinsic_calls;
        received_max = constraints.max_width;
        return ryn::runtime::Size{80.0F, 22.0F};
    });
    nodes.require(root).external_layout.min_width = 54.0F;
    nodes.require(root).external_layout.max_width = 60.0F;
    require(layout.layout(root, {0.0F, 200.0F, 0.0F, 100.0F})
                    == ryn::runtime::Size{60.0F, 32.0F}
                && intrinsic_calls == 1
                && received_max == 28.0F
                && nodes.require(cjk).bounds.width == 28.0F,
            "constrained CJK intrinsic content exceeded token padding or max width");
}

void test_static_loading_geometry_preserves_child_identity_and_cache() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto child = nodes.create_child(root);
    const auto unrelated = nodes.create_root();
    ryn::layout::LayoutEngine layout(nodes);
    const ryn::layout::HorizontalContentLayout idle{
        32.0F,
        15.0F,
        1.0F,
        8.0F,
        false,
        14.0F,
    };
    auto loading = idle;
    loading.loading = true;
    layout.set_layout(root, idle);
    layout.set_layout(child, ryn::layout::LeafLayout{});
    layout.set_layout(unrelated, ryn::layout::LeafLayout{{12.0F, 8.0F}});
    int intrinsic_calls = 0;
    layout.set_intrinsic_measure(child, 7, [&](ryn::layout::Constraints) {
        ++intrinsic_calls;
        return ryn::runtime::Size{42.0F, 22.0F};
    });

    const auto idle_size = layout.layout(
        root,
        {0.0F, std::numeric_limits<float>::infinity(), 0.0F, 100.0F});
    const auto child_identity = child;
    const auto unrelated_measure_count = nodes.require(unrelated).measure_count;
    require(idle_size == ryn::runtime::Size{74.0F, 32.0F}
                && intrinsic_calls == 1
                && !layout.horizontal_content_geometry(root)
                    .loading_indicator_bounds.has_value(),
            "idle horizontal content measurement was incorrect");

    layout.set_layout(root, loading);
    const auto loading_size = layout.layout(
        root,
        {0.0F, std::numeric_limits<float>::infinity(), 0.0F, 100.0F});
    const auto indicator = layout.horizontal_content_geometry(root)
        .loading_indicator_bounds;
    require(loading_size == ryn::runtime::Size{96.0F, 32.0F}
                && indicator.has_value()
                && indicator->width == 14.0F
                && indicator->height == 14.0F
                && child == child_identity
                && nodes.find(child_identity) != nullptr
                && intrinsic_calls == 1
                && nodes.require(unrelated).measure_count == unrelated_measure_count,
            "static loading indicator remounted, remeasured, or touched a sibling");

    layout.set_layout(root, idle);
    require(layout.layout(
                    root,
                    {0.0F, std::numeric_limits<float>::infinity(), 0.0F, 100.0F})
                    == idle_size
                && child == child_identity
                && intrinsic_calls == 1
                && !layout.horizontal_content_geometry(root)
                    .loading_indicator_bounds.has_value(),
            "static loading removal changed child identity or intrinsic cache");
}

} // namespace

int main() {
    try {
        test_invalid_constraints_are_diagnosed_before_measure();
        test_box_measure_and_place_with_padding();
        test_box_fill_uses_bounded_maximum();
        test_horizontal_and_vertical_flex_are_deterministic();
        test_flex_model_validation_equality_and_empty_layout();
        test_flex_wrap_is_greedy_and_reuses_line_scratch();
        test_flex_justify_modes_and_fractional_stability();
        test_flex_align_modes_and_stretch_constraints();
        test_flex_dual_axis_gaps_and_nested_logical_bounds();
        test_flex_children_do_not_exceed_constraints();
        test_external_style_constrains_and_places_without_wrapper();
        test_external_style_clamps_fixed_and_intrinsic_sizes();
        test_intrinsic_measure_cache_revision_and_generation();
        test_recursive_intrinsic_measure_fails_fast_and_recovers();
        test_horizontal_content_centers_children_with_token_metrics();
        test_horizontal_content_sizes_empty_and_constrained_content();
        test_static_loading_geometry_preserves_child_identity_and_cache();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
