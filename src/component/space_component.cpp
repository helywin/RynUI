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

[[nodiscard]] layout::FlexWrap space_wrap(bool wrap) noexcept {
    return wrap ? layout::FlexWrap::wrap : layout::FlexWrap::no_wrap;
}

[[nodiscard]] layout::FlexAlign space_align(SpaceAlign align) {
    switch (align) {
    case SpaceAlign::Start:
        return layout::FlexAlign::start;
    case SpaceAlign::Center:
        return layout::FlexAlign::center;
    case SpaceAlign::End:
        return layout::FlexAlign::end;
    }
    throw std::invalid_argument("Space align value is invalid");
}

void apply_measure_model(
    SpaceComponentState& state,
    layout::FlexLayout candidate,
    layout::LayoutEngine& layout,
    runtime::DirtyQueues& dirty) {
    if (candidate == state.model) {
        return;
    }
    layout.set_layout(state.node, candidate);
    state.model = candidate;
    dirty.invalidate_subtree(
        state.node,
        runtime::DirtyFlags::Measure |
            runtime::DirtyFlags::Layout |
            runtime::DirtyFlags::Geometry);
}

void apply_placement_model(
    SpaceComponentState& state,
    layout::FlexLayout candidate,
    layout::LayoutEngine& layout,
    runtime::DirtyQueues& dirty) {
    if (candidate == state.model) {
        return;
    }
    layout.set_layout(state.node, candidate);
    state.model = candidate;
    dirty.invalidate_subtree(
        state.node,
        runtime::DirtyFlags::Placement | runtime::DirtyFlags::Geometry);
}

} // namespace

void mount_space_component(const SpaceProps& props, const SpaceContent& content) {
    auto& services = require_layout_component_services();
    auto& build = runtime::require_component_build_context();

    const auto gap = resolve_layout_gap(read_prop(SpacePropsAccess::size(props)), services.theme);
    layout::FlexLayout initial{
        .direction = space_direction(read_prop(SpacePropsAccess::vertical(props))),
        .main_gap = gap.main,
        .padding = {},
        .fill_width = false,
        .fill_height = false,
        .wrap = space_wrap(read_prop(SpacePropsAccess::wrap(props))),
        .justify = layout::FlexJustify::start,
        .align = space_align(read_prop(SpacePropsAccess::align(props))),
        .cross_gap = gap.cross,
        .item_policy = layout::FlexItemPolicy::sequential,
    };

    const auto component = build.mount_component<SpaceComponentState>();
    auto& state = build.state<SpaceComponentState>(component);
    state.component = component;
    state.node = build.root(component);
    state.model = initial;
    services.layout.set_layout(state.node, initial);
    build.on_resource_cleanup(component, [layout = &services.layout, node = state.node] {
        static_cast<void>(layout->remove_layout(node));
    });
    runtime::connect_layout_style(
        build.scope(component),
        SpacePropsAccess::layout(props),
        state.node,
        services.nodes,
        services.dirty);

    auto& scope = build.scope(component);
    auto* layout = &services.layout;
    auto* dirty = &services.dirty;
    const auto* theme = &services.theme;
    static_cast<void>(connect_prop(
        scope,
        SpacePropsAccess::vertical(props),
        [&state, layout, dirty](bool vertical) {
            auto candidate = state.model;
            candidate.direction = space_direction(vertical);
            apply_measure_model(state, candidate, *layout, *dirty);
        }));
    static_cast<void>(connect_prop(
        scope,
        SpacePropsAccess::wrap(props),
        [&state, layout, dirty](bool wrap) {
            auto candidate = state.model;
            candidate.wrap = space_wrap(wrap);
            apply_measure_model(state, candidate, *layout, *dirty);
        }));
    static_cast<void>(connect_prop(
        scope,
        SpacePropsAccess::align(props),
        [&state, layout, dirty](SpaceAlign align) {
            auto candidate = state.model;
            candidate.align = space_align(align);
            apply_placement_model(state, candidate, *layout, *dirty);
        }));
    static_cast<void>(connect_prop(
        scope,
        SpacePropsAccess::size(props),
        [&state, layout, dirty, theme](const LayoutGap& value) {
            const auto resolved = resolve_layout_gap(value, *theme);
            auto candidate = state.model;
            candidate.main_gap = resolved.main;
            candidate.cross_gap = resolved.cross;
            apply_measure_model(state, candidate, *layout, *dirty);
        }));

    build.mount_slot(component, content);
    services.dirty.invalidate_subtree(
        state.node,
        runtime::DirtyFlags::Measure |
            runtime::DirtyFlags::Layout |
            runtime::DirtyFlags::Geometry);
}

} // namespace ryn::detail

namespace ryn {

void Space(SpaceProps props, SpaceContent content) {
    detail::mount_space_component(props, content);
}

} // namespace ryn
