#include "component/flex_component.hpp"

#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <stdexcept>
#include <utility>

namespace ryn::detail {

struct LayoutGapAccess final {
    [[nodiscard]] static const std::optional<SpaceSize>& preset(const LayoutGap& gap) noexcept {
        return gap.preset_;
    }

    [[nodiscard]] static float main(const LayoutGap& gap) noexcept {
        return gap.main_;
    }

    [[nodiscard]] static float cross(const LayoutGap& gap) noexcept {
        return gap.cross_;
    }
};

struct FlexPropsAccess final {
    [[nodiscard]] static const Prop<bool>& vertical(const FlexProps& props) noexcept {
        return props.vertical_;
    }

    [[nodiscard]] static const Prop<bool>& wrap(const FlexProps& props) noexcept {
        return props.wrap_;
    }

    [[nodiscard]] static const Prop<FlexJustify>& justify(const FlexProps& props) noexcept {
        return props.justify_;
    }

    [[nodiscard]] static const Prop<FlexAlign>& align(const FlexProps& props) noexcept {
        return props.align_;
    }

    [[nodiscard]] static const Prop<LayoutGap>& gap(const FlexProps& props) noexcept {
        return props.gap_;
    }

    [[nodiscard]] static const LayoutStyle& layout(const FlexProps& props) noexcept {
        return props.layout_;
    }
};

namespace {

[[nodiscard]] layout::FlexDirection flex_direction(bool vertical) noexcept {
    return vertical ? layout::FlexDirection::vertical : layout::FlexDirection::horizontal;
}

[[nodiscard]] layout::FlexWrap flex_wrap(bool wrap) noexcept {
    return wrap ? layout::FlexWrap::wrap : layout::FlexWrap::no_wrap;
}

[[nodiscard]] layout::FlexJustify flex_justify(FlexJustify justify) {
    switch (justify) {
    case FlexJustify::Start:
        return layout::FlexJustify::start;
    case FlexJustify::Center:
        return layout::FlexJustify::center;
    case FlexJustify::End:
        return layout::FlexJustify::end;
    case FlexJustify::SpaceBetween:
        return layout::FlexJustify::space_between;
    case FlexJustify::SpaceAround:
        return layout::FlexJustify::space_around;
    case FlexJustify::SpaceEvenly:
        return layout::FlexJustify::space_evenly;
    }
    throw std::invalid_argument("Flex justify value is invalid");
}

[[nodiscard]] layout::FlexAlign flex_align(FlexAlign align) {
    switch (align) {
    case FlexAlign::Start:
        return layout::FlexAlign::start;
    case FlexAlign::Center:
        return layout::FlexAlign::center;
    case FlexAlign::End:
        return layout::FlexAlign::end;
    case FlexAlign::Stretch:
        return layout::FlexAlign::stretch;
    }
    throw std::invalid_argument("Flex align value is invalid");
}

struct ResolvedGap final {
    float main{0.0F};
    float cross{0.0F};
};

[[nodiscard]] ResolvedGap resolve_gap(const LayoutGap& gap, const DefaultThemeSnapshot& theme) {
    if (!LayoutGapAccess::preset(gap).has_value()) {
        return {LayoutGapAccess::main(gap), LayoutGapAccess::cross(gap)};
    }
    switch (*LayoutGapAccess::preset(gap)) {
    case SpaceSize::Small:
        return {theme.layout_spacing.small, theme.layout_spacing.small};
    case SpaceSize::Middle:
        return {theme.layout_spacing.middle, theme.layout_spacing.middle};
    case SpaceSize::Large:
        return {theme.layout_spacing.large, theme.layout_spacing.large};
    }
    throw std::invalid_argument("LayoutGap preset is invalid");
}

void apply_measure_model(FlexComponentState& state, layout::FlexLayout candidate,
                         layout::LayoutEngine& layout, runtime::DirtyQueues& dirty) {
    if (candidate == state.model) {
        return;
    }
    layout.set_layout(state.node, candidate);
    state.model = candidate;
    dirty.invalidate_subtree(state.node, runtime::DirtyFlags::Measure |
                                             runtime::DirtyFlags::Layout |
                                             runtime::DirtyFlags::Geometry);
}

void apply_placement_model(FlexComponentState& state, layout::FlexLayout candidate,
                           layout::LayoutEngine& layout, runtime::DirtyQueues& dirty) {
    if (candidate == state.model) {
        return;
    }
    layout.set_layout(state.node, candidate);
    state.model = candidate;
    dirty.invalidate_subtree(state.node,
                             runtime::DirtyFlags::Placement | runtime::DirtyFlags::Geometry);
}

} // namespace

void mount_flex_component(const FlexProps& props, const FlexContent& content) {
    auto& services = require_layout_component_services();
    auto& build = runtime::require_component_build_context();

    const auto gap = resolve_gap(read_prop(FlexPropsAccess::gap(props)), services.theme);
    layout::FlexLayout initial{
        .direction = flex_direction(read_prop(FlexPropsAccess::vertical(props))),
        .main_gap = gap.main,
        .padding = {},
        .fill_width = false,
        .fill_height = false,
        .wrap = flex_wrap(read_prop(FlexPropsAccess::wrap(props))),
        .justify = flex_justify(read_prop(FlexPropsAccess::justify(props))),
        .align = flex_align(read_prop(FlexPropsAccess::align(props))),
        .cross_gap = gap.cross,
    };

    const auto component = build.mount_component<FlexComponentState>();
    auto& state = build.state<FlexComponentState>(component);
    state.component = component;
    state.node = build.root(component);
    state.model = initial;
    services.layout.set_layout(state.node, initial);
    build.on_resource_cleanup(component, [layout = &services.layout, node = state.node] {
        static_cast<void>(layout->remove_layout(node));
    });
    runtime::connect_layout_style(build.scope(component), FlexPropsAccess::layout(props),
                                  state.node, services.nodes, services.dirty);

    auto& scope = build.scope(component);
    auto* layout = &services.layout;
    auto* dirty = &services.dirty;
    const auto* theme = &services.theme;
    static_cast<void>(connect_prop(scope, FlexPropsAccess::vertical(props),
                                   [&state, layout, dirty](bool vertical) {
                                       auto candidate = state.model;
                                       candidate.direction = flex_direction(vertical);
                                       apply_measure_model(state, candidate, *layout, *dirty);
                                   }));
    static_cast<void>(
        connect_prop(scope, FlexPropsAccess::wrap(props), [&state, layout, dirty](bool wrap) {
            auto candidate = state.model;
            candidate.wrap = flex_wrap(wrap);
            apply_measure_model(state, candidate, *layout, *dirty);
        }));
    static_cast<void>(connect_prop(scope, FlexPropsAccess::justify(props),
                                   [&state, layout, dirty](FlexJustify justify) {
                                       auto candidate = state.model;
                                       candidate.justify = flex_justify(justify);
                                       apply_placement_model(state, candidate, *layout, *dirty);
                                   }));
    static_cast<void>(connect_prop(scope, FlexPropsAccess::align(props),
                                   [&state, layout, dirty](FlexAlign align) {
                                       auto candidate = state.model;
                                       candidate.align = flex_align(align);
                                       apply_placement_model(state, candidate, *layout, *dirty);
                                   }));
    static_cast<void>(connect_prop(scope, FlexPropsAccess::gap(props),
                                   [&state, layout, dirty, theme](const LayoutGap& value) {
                                       const auto resolved = resolve_gap(value, *theme);
                                       auto candidate = state.model;
                                       candidate.main_gap = resolved.main;
                                       candidate.cross_gap = resolved.cross;
                                       apply_measure_model(state, candidate, *layout, *dirty);
                                   }));

    build.mount_slot(component, content);
    services.dirty.invalidate_subtree(state.node, runtime::DirtyFlags::Measure |
                                                      runtime::DirtyFlags::Layout |
                                                      runtime::DirtyFlags::Geometry);
}

} // namespace ryn::detail

namespace ryn {

void Flex(FlexProps props, FlexContent content) {
    detail::mount_flex_component(props, content);
}

} // namespace ryn
