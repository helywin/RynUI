#pragma once

#include <ryn/component.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>

namespace rynui::example {

struct LayoutDemoTelemetry final {
    std::uint64_t content_runs{};
    std::uint64_t prop_updates{};
    std::uint64_t activations{};
};

struct LayoutDemoDefinition final {
    ryn::Content content;
    std::function<void(std::size_t)> smoke_step;
    std::function<LayoutDemoTelemetry()> telemetry;
};

int run_layout_demo(int argc, char** argv, LayoutDemoDefinition definition);

} // namespace rynui::example
