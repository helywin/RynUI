#pragma once

#include "component/layout_component_context.hpp"
#include "runtime/component_host.hpp"

#include <ryn/flex.hpp>

namespace ryn::detail {

struct FlexComponentState final {
    runtime::ComponentId component;
    runtime::NodeId node;
    layout::FlexLayout model;
};

void mount_flex_component(const FlexProps& props, const FlexContent& content);

} // namespace ryn::detail
