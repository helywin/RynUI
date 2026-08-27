#pragma once

#include "layout/layout_engine.hpp"
#include "runtime/invalidation.hpp"
#include "runtime/node_store.hpp"

namespace ryn::detail {

struct LayoutComponentServices final {
    runtime::NodeStore& nodes;
    layout::LayoutEngine& layout;
    runtime::DirtyQueues& dirty;
};

class ActiveLayoutComponentServices final {
public:
    explicit ActiveLayoutComponentServices(LayoutComponentServices& services) noexcept;
    ActiveLayoutComponentServices(const ActiveLayoutComponentServices&) = delete;
    ActiveLayoutComponentServices& operator=(const ActiveLayoutComponentServices&) = delete;
    ~ActiveLayoutComponentServices();

private:
    LayoutComponentServices* previous_;
};

[[nodiscard]] LayoutComponentServices& require_layout_component_services();

} // namespace ryn::detail
