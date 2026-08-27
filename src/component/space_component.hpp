#pragma once

#include "component/layout_component_context.hpp"
#include "runtime/component_host.hpp"

#include <ryn/space.hpp>

namespace ryn::detail {

struct SpaceComponentState final {
    runtime::ComponentId component;
    runtime::NodeId node;
    layout::FlexLayout model;
};

void mount_space_component(const SpaceProps& props, const SpaceContent& content);

} // namespace ryn::detail
