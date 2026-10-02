#include "component/space_component.hpp"

#include "component/layout_container_values.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <stdexcept>
#include <utility>

namespace ryn::detail {

struct SpacePropsAccess final {
    [[nodiscard]] static const Prop<bool>& vertical(const SpaceProps& props) noexcept {
        return props.vertical_;
    }

    [[nodiscard]] static const std::optional<Prop<SpaceOrientation>>& orientation(const SpaceProps& props) noexcept {
        return props.orientation_;
    }

    [[nodiscard]] static const Prop<FlexDirection>& direction(const SpaceProps& props) noexcept {
        return props.direction_;
    }

    [[nodiscard]] static const Prop<bool>& wrap(const SpaceProps& props) noexcept {
        return props.wrap_;
    }

    [[nodiscard]] static const Prop<SpaceAlign>& align(const SpaceProps& props) noexcept {
        return props.align_;
    }

    [[nodiscard]] static const Prop<LayoutGap>& size(const SpaceProps& props) noexcept {
        return props.size_;
    }

    [[nodiscard]] static const LayoutStyle& layout(const SpaceProps& props) noexcept {
        return props.layout_;
    }
};

namespace {

[[nodiscard]] layout::FlexDirection space_direction(bool vertical) noexcept {
    return vertical ? layout::FlexDirection::vertical : layout::FlexDirection::horizontal;
}

[[nodiscard]] layout::FlexDirection space_direction(SpaceOrientation value) {
    switch (value) {
    case SpaceOrientation::Horizontal:
        return layout::FlexDirection::horizontal;
    case SpaceOrientation::Vertical:
        return layout::FlexDirection::vertical;
    }
    throw std::invalid_argument("Space orientation is invalid");
}

[[nodiscard]] bool space_rtl(FlexDirection value) {
    switch (value) {
    case FlexDirection::LeftToRight:
        return false;
    case FlexDirection::RightToLeft:
        return true;
    }
    throw std::invalid_argument("Space direction is invalid");
}

[[nodiscard]] layout::FlexWrap space_wrap(bool wrap) noexcept {
    return wrap ? layout::FlexWrap::wrap : layout::FlexWrap::no_wrap;
}

[[nodiscard]] layout::FlexAlign space_align(SpaceAlign align, layout::FlexDirection direction) {
    switch (align) {
    case SpaceAlign::Start:
        return layout::FlexAlign::start;
    case SpaceAlign::Center:
        return layout::FlexAlign::center;
    case SpaceAlign::End:
        return layout::FlexAlign::end;
    case SpaceAlign::Auto:
        return direction == layout::FlexDirection::horizontal ? layout::FlexAlign::center : layout::FlexAlign::stretch;
    case SpaceAlign::Baseline:
        return layout::FlexAlign::baseline;
    }
    throw std::invalid_argument("Space align value is invalid");
}

void apply_measure_model(SpaceComponentState& state, layout::FlexLayout candidate, layout::LayoutEngine& layout,
                         runtime::DirtyQueues& dirty) {
    if (candidate == state.model) {
        return;
    }
    layout.set_layout(state.node, candidate);
    state.model = candidate;
    dirty.invalidate_subtree(state.node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                             runtime::DirtyFlags::Geometry);
}

void apply_placement_model(SpaceComponentState& state, layout::FlexLayout candidate, layout::LayoutEngine& layout,
                           runtime::DirtyQueues& dirty, const runtime::NodeStore& nodes) {
    bool measure = candidate.align == layout::FlexAlign::baseline || state.model.align == layout::FlexAlign::baseline;
    // Baseline-preserving direction changes do not alter the baseline of this
    // container. Internal alignment changes can affect a participating ancestor.
    if (candidate.align == state.model.align) {
        measure = false;
    }
    if (candidate.align != state.model.align) {
        for (auto* node = nodes.find(state.node); node != nullptr;
             node = node->parent ? nodes.find(*node->parent) : nullptr) {
            measure = measure || node->baseline_participant;
        }
    }
    if (measure) {
        apply_measure_model(state, candidate, layout, dirty);
        return;
    }
    if (candidate == state.model) {
        return;
    }
    layout.set_layout(state.node, candidate);
    state.model = candidate;
    dirty.invalidate_subtree(state.node, runtime::DirtyFlags::Placement | runtime::DirtyFlags::Geometry);
}

void subscribe_theme_gap(SpaceComponentState& state, const std::shared_ptr<theme_runtime::ThemeScope>& theme,
                         layout::LayoutEngine& layout, runtime::DirtyQueues& dirty) {
    state.theme_subscription.reset();
    if (!LayoutGapAccess::preset(state.gap).has_value()) {
        return;
    }
    state.theme_subscription = theme->capture(
        [&state, theme, &layout, &dirty](theme_runtime::DirtyPhase) {
            const auto resolved = resolve_layout_gap(state.gap, *theme);
            auto candidate = state.model;
            candidate.main_gap = resolved.main;
            candidate.cross_gap = resolved.cross;
            apply_measure_model(state, candidate, layout, dirty);
        },
        [&state, theme] { static_cast<void>(resolve_layout_gap(state.gap, *theme)); });
}

} // namespace

void mount_space_component(const SpaceProps& props, const SpaceContent& content) {
    auto& services = require_layout_component_services();
    auto& build = runtime::require_component_build_context();

    const auto theme = build.theme_scope();
    const auto initial_gap = read_prop(SpacePropsAccess::size(props));
    const auto gap = resolve_layout_gap(initial_gap, *theme);
    const auto& orientation = SpacePropsAccess::orientation(props);
    const auto initial_direction = orientation ? space_direction(read_prop(*orientation))
                                               : space_direction(read_prop(SpacePropsAccess::vertical(props)));
    const auto initial_align = read_prop(SpacePropsAccess::align(props));
    layout::FlexLayout initial{
        .direction = initial_direction,
        .main_gap = gap.main,
        .padding = {},
        .fill_width = false,
        .fill_height = false,
        .wrap = space_wrap(read_prop(SpacePropsAccess::wrap(props))),
        .justify = layout::FlexJustify::start,
        .align = space_align(initial_align, initial_direction),
        .cross_gap = gap.cross,
        .item_policy = layout::FlexItemPolicy::sequential,
        .right_to_left = space_rtl(read_prop(SpacePropsAccess::direction(props))),
    };

    const auto component = build.mount_component<SpaceComponentState>();
    auto& state = build.state<SpaceComponentState>(component);
    state.component = component;
    state.node = build.root(component);
    state.model = initial;
    state.gap = initial_gap;
    state.requested_align = initial_align;
    services.layout.set_layout(state.node, initial);
    build.on_resource_cleanup(
        component, [layout = &services.layout, node = state.node] { static_cast<void>(layout->remove_layout(node)); });
    runtime::connect_layout_style(build.scope(component), SpacePropsAccess::layout(props), state.node, services.nodes,
                                  services.dirty);

    auto& scope = build.scope(component);
    auto* layout = &services.layout;
    auto* dirty = &services.dirty;
    auto* nodes = &services.nodes;
    subscribe_theme_gap(state, theme, *layout, *dirty);
    const auto change_orientation = [&state, layout, dirty](layout::FlexDirection value) {
        auto candidate = state.model;
        candidate.direction = value;
        candidate.align = space_align(state.requested_align, value);
        apply_measure_model(state, candidate, *layout, *dirty);
    };
    if (orientation) {
        static_cast<void>(connect_prop(scope, *orientation, [change_orientation](SpaceOrientation value) {
            change_orientation(space_direction(value));
        }));
    } else {
        static_cast<void>(connect_prop(scope, SpacePropsAccess::vertical(props), [change_orientation](bool value) {
            change_orientation(space_direction(value));
        }));
    }
    static_cast<void>(
        connect_prop(scope, SpacePropsAccess::direction(props), [&state, layout, dirty, nodes](FlexDirection value) {
            auto candidate = state.model;
            candidate.right_to_left = space_rtl(value);
            apply_placement_model(state, candidate, *layout, *dirty, *nodes);
        }));
    static_cast<void>(connect_prop(scope, SpacePropsAccess::wrap(props), [&state, layout, dirty](bool wrap) {
        auto candidate = state.model;
        candidate.wrap = space_wrap(wrap);
        apply_measure_model(state, candidate, *layout, *dirty);
    }));
    static_cast<void>(
        connect_prop(scope, SpacePropsAccess::align(props), [&state, layout, dirty, nodes](SpaceAlign align) {
            auto candidate = state.model;
            candidate.align = space_align(align, candidate.direction);
            state.requested_align = align;
            apply_placement_model(state, candidate, *layout, *dirty, *nodes);
        }));
    static_cast<void>(
        connect_prop(scope, SpacePropsAccess::size(props), [&state, layout, dirty, theme](const LayoutGap& value) {
            state.gap = value;
            const auto resolved = resolve_layout_gap(value, *theme);
            auto candidate = state.model;
            candidate.main_gap = resolved.main;
            candidate.cross_gap = resolved.cross;
            apply_measure_model(state, candidate, *layout, *dirty);
            subscribe_theme_gap(state, theme, *layout, *dirty);
        }));

    build.mount_slot(component, content);
    services.dirty.invalidate_subtree(state.node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                      runtime::DirtyFlags::Geometry);
}

} // namespace ryn::detail

namespace ryn {

void Space(SpaceProps props, SpaceContent content) {
    detail::mount_space_component(props, content);
}

} // namespace ryn
