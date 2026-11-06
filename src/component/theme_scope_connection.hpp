#pragma once

#include "runtime/component_host.hpp"

namespace ryn::detail {

// The large ThemeConfig temporaries finish in this function before any nested
// content mounts. ConfigProvider shares the already validated Theme boundary.
[[nodiscard]] std::shared_ptr<theme_runtime::ThemeScope> initialize_theme_scope(runtime::ComponentBuildContext& build,
                                                                                const Prop<ThemeConfig>& config);

} // namespace ryn::detail
