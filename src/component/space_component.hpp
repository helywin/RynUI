#pragma once

#include "component/layout_component_context.hpp"
#include "runtime/component_host.hpp"

#include <ryn/space.hpp>

namespace ryn::detail {

struct SpaceComponentState final {
    struct Item final {
        runtime::ComponentId component;
        std::optional<runtime::ComponentId> separator;
    };

    runtime::ComponentId component;
    runtime::NodeId node;
    layout::FlexLayout model;
    LayoutGap gap;
    SpaceAlign requested_align{SpaceAlign::Auto};
    theme_runtime::Subscription theme_subscription;
    std::vector<Item> items;
};

void mount_space_component(const SpaceProps& props, const SpaceContent& content);

} // namespace ryn::detail
