#include "support/state_projection_fixture.hpp"
#include "support/allocation_probe.hpp"
#include <chrono>
#include <string_view>

namespace {
using namespace projection_test;
using Clock = std::chrono::steady_clock;

struct Sample final {
    std::size_t dispatch_allocations{};
    std::size_t sync_allocations{};
    std::int64_t dispatch_ns{};
    std::int64_t sync_ns{};
};

template <class Dispatch> Sample measure(Fixture& f, Dispatch dispatch) {
    const auto start = Clock::now();
    ryn_test::allocation::begin();
    dispatch();
    const auto dispatch_allocations = ryn_test::allocation::end();
    const auto dispatched = Clock::now();
    ryn_test::allocation::begin();
    sync(f);
    const auto sync_allocations = ryn_test::allocation::end();
    const auto end = Clock::now();
    return {dispatch_allocations, sync_allocations,
            std::chrono::duration_cast<std::chrono::nanoseconds>(dispatched - start).count(),
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - dispatched).count()};
}

void print(std::string_view component, std::size_t count, std::string_view operation, std::size_t iteration,
           Sample sample, std::uint64_t comparator, std::int64_t projected, std::int64_t presented,
           std::int64_t visited, std::int64_t copied, std::int64_t sorts, std::uint64_t rebuilds) {
    std::cout << component << ',' << count << ',' << operation << ',' << iteration << ',' << sample.dispatch_allocations
              << ',' << sample.sync_allocations << ',' << sample.dispatch_ns << ',' << sample.sync_ns << ','
              << comparator << ',' << projected << ',' << presented << ',' << visited << ',' << copied << ',' << sorts
              << ',' << rebuilds << '\n';
}

void summary(std::string_view component, std::size_t count, std::string_view operation,
             std::vector<std::int64_t> elapsed) {
    std::ranges::sort(elapsed);
    const auto index = (elapsed.size() * 95 + 99) / 100 - 1;
    std::cout << "# p95_ns," << component << ',' << count << ',' << operation << ',' << elapsed[index] << '\n';
}

void tree(std::size_t count, std::size_t iterations, bool baseline) {
    Fixture f;
    const auto id = mount_tree(f, count);
    const auto rows = f.services.tree().snapshot(id).rows;
    for (std::size_t i = 0; i < 8; ++i) {
        hover(f, rows[i % 2]);
        sync(f);
    }
    std::vector<std::int64_t> elapsed;
    elapsed.reserve(iterations);
    for (std::size_t i = 0; i < iterations; ++i) {
        const auto before = tree_counts(f.services.tree(), id);
        const auto rebuilds = f.services.scene_composer().diagnostics().rebuilds;
        const auto sample = measure(f, [&] { hover(f, rows[i % 2]); });
        const auto after = tree_counts(f.services.tree(), id);
        const auto rebuilt = f.services.scene_composer().diagnostics().rebuilds - rebuilds;
        print("tree", count, "hover", i, sample, 0,
              before.available ? static_cast<std::int64_t>(after.projected - before.projected) : -1,
              before.available ? static_cast<std::int64_t>(after.presented - before.presented) : -1, -1, -1, -1,
              rebuilt);
        elapsed.push_back(sample.dispatch_ns + sample.sync_ns);
        if (!baseline) {
            check(rebuilt == 0, "Tree hover benchmark rebuilt composer");
            check(!before.available || (after.projected == before.projected && after.presented - before.presented == 2),
                  "Tree hover benchmark touched unrelated rows");
        }
    }
    summary("tree", count, "hover", std::move(elapsed));
}

void table(std::size_t count, std::size_t iterations, bool baseline) {
    TableFixture fixture(count);
    fixture.mount();
    auto& f = fixture.f;
    for (const auto operation :
         {std::string_view{"selection"}, std::string_view{"bordered"}, std::string_view{"page"}}) {
        const auto dispatch = [&](std::size_t i) {
            if (operation == "selection") {
                fixture.selected.set({key(count - 1 - i % 2)});
            } else if (operation == "bordered") {
                fixture.bordered.set(i % 2 != 0);
            } else {
                fixture.page.set(i % 2 + 1);
            }
        };
        for (std::size_t i = 0; i < 8; ++i) {
            dispatch(i);
            sync(f);
        }
        std::vector<std::int64_t> elapsed;
        elapsed.reserve(iterations);
        for (std::size_t i = 0; i < iterations; ++i) {
            const auto before = table_counts(f.services.table(), fixture.id);
            const auto comparisons = fixture.comparisons;
            const auto rebuilds = f.services.scene_composer().diagnostics().rebuilds;
            const auto sample = measure(f, [&] { dispatch(i); });
            const auto after = table_counts(f.services.table(), fixture.id);
            const auto compared = fixture.comparisons - comparisons;
            print("table", count, operation, i, sample, compared, -1, -1,
                  before.available ? static_cast<std::int64_t>(after.visited - before.visited) : -1,
                  before.available ? static_cast<std::int64_t>(after.copied - before.copied) : -1,
                  before.available ? static_cast<std::int64_t>(after.sorts - before.sorts) : -1,
                  f.services.scene_composer().diagnostics().rebuilds - rebuilds);
            elapsed.push_back(sample.dispatch_ns + sample.sync_ns);
            if (!baseline) {
                check(compared == 0, "Table selection/border/page benchmark called comparator");
                check(!before.available || (after.sorts == before.sorts && after.copied == before.copied),
                      "Table hot update benchmark copied/sorted source records");
            }
        }
        summary("table", count, operation, std::move(elapsed));
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        std::cout << std::unitbuf;
        if (argc < 3) {
            throw std::invalid_argument(
                "usage: state_projection_benchmark tree|table count [iterations=64] [--baseline]");
        }
        const std::string_view component = argv[1];
        const auto count = std::stoull(argv[2]);
        const auto iterations = argc > 3 ? std::stoull(argv[3]) : 64;
        const bool baseline = argc > 4 && std::string_view{argv[4]} == "--baseline";
        check(iterations > 0 && count >= 32 && (component == "table" || count <= 1024), "invalid benchmark dimensions");
        std::cout
            << "component,count,operation,iteration,dispatch_allocations,sync_allocations,dispatch_ns,sync_ns,"
               "comparator,projected_rows,presented_rows,visited_rows,copied_records,sort_passes,composer_rebuilds\n";
        if (component == "tree") {
            tree(count, iterations, baseline);
        } else if (component == "table") {
            table(count, iterations, baseline);
        } else {
            throw std::invalid_argument("unknown benchmark component");
        }
    } catch (const std::exception& error) {
        ryn_test::allocation::end();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
