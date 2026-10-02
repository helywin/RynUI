#include "input/interaction_registry.hpp"

#include <ryn/component.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

struct BenchmarkState final {};

ryn::runtime::ComponentId mount_root() {
    return ryn::runtime::require_component_build_context().mount_component<BenchmarkState>();
}

struct Case final {
    std::size_t nodes;
    std::size_t interactions;
    std::size_t dirty;
    std::size_t iterations;
};

void run_case(const Case& test_case) {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId component;
    components.mount(ryn::Content{[&] { component = mount_root(); }});
    const auto root = components.root(component);

    std::vector<ryn::runtime::NodeId> node_ids;
    node_ids.reserve(test_case.nodes);
    for (std::size_t index = 0; index < test_case.nodes; ++index) {
        const auto id = nodes.create_child(root);
        auto& node = nodes.require(id);
        node.bounds = {static_cast<float>(index), 0.0F, 1.0F, 1.0F};
        node.place_generation = 1;
        node_ids.push_back(id);
    }

    ryn::input::InteractionRegistry registry(components, nodes);
    registry.reserve(test_case.interactions);
    std::vector<ryn::input::HitTestPaintEntry> entries;
    entries.reserve(test_case.interactions);
    const auto stride = test_case.nodes / test_case.interactions;
    for (std::size_t index = 0; index < test_case.interactions; ++index) {
        const auto interaction = registry.create({component, node_ids[index * stride], std::nullopt, true, false, {}});
        entries.push_back({interaction, std::nullopt});
    }

    ryn::input::HitTestSnapshot snapshot(registry, nodes);
    snapshot.rebuild(entries, {0.0F, 0.0F, static_cast<float>(test_case.nodes), 2.0F});

    // 4051 is odd, so this visits distinct slots for each power-of-two case.
    std::vector<ryn::runtime::NodeId> dirty;
    dirty.reserve(test_case.dirty);
    std::vector<bool> selected(test_case.nodes);
    for (std::size_t index = 0; index < test_case.dirty; ++index) {
        const auto selected_index = (index * 4051U) & (test_case.nodes - 1);
        dirty.push_back(node_ids[selected_index]);
        selected[selected_index] = true;
    }
    std::size_t expected = 0;
    for (std::size_t index = 0; index < test_case.interactions; ++index) {
        expected += selected[index * stride] ? 1U : 0U;
    }

    for (int warmup = 0; warmup < 5; ++warmup) {
        if (snapshot.refresh(dirty) != expected) {
            throw std::runtime_error("HitTest dirty benchmark warmup mismatch");
        }
    }
    const auto started = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < test_case.iterations; ++iteration) {
        if (snapshot.refresh(dirty) != expected) {
            throw std::runtime_error("HitTest dirty benchmark refresh mismatch");
        }
    }
    const auto elapsed = std::chrono::steady_clock::now() - started;
    const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
    std::cout << test_case.nodes << ',' << test_case.interactions << ',' << test_case.dirty << ','
              << test_case.iterations << ',' << expected << ','
              << nanos / static_cast<std::int64_t>(test_case.iterations) << '\n';
}

} // namespace

int main() {
    try {
        constexpr std::array cases{
            Case{4096, 256, 1, 2000},
            Case{4096, 256, 64, 100},
            Case{4096, 256, 512, 30},
            Case{16384, 1024, 4096, 10},
        };
        std::cout << "nodes,interactions,dirty,iterations,refreshed,cpu_ns_per_refresh\n";
        for (const auto& test_case : cases) {
            run_case(test_case);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
