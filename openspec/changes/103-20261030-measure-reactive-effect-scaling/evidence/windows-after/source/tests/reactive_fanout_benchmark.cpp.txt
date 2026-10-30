#include <ryn/reactive.hpp>

#include "support/scaling_benchmark.hpp"

#include <array>
#include <iostream>
#include <memory>
#include <vector>

namespace {

using namespace ryn_test::scaling;

class Fanout final {
public:
    Fanout(std::size_t count, bool dynamic) : values(count), calls(count), dynamic_(dynamic) {
        for (std::size_t index = 0; index < count; ++index) {
            ryn::effect(scope, [this, index] {
                bool choose_right = false;
                if (dynamic_) {
                    choose_right = selector.get();
                    ++reads;
                }
                const auto value = choose_right ? right.get() : left.get();
                ++reads;
                values[index] = static_cast<std::int64_t>(value) * 3 + static_cast<std::int64_t>(index);
                ++calls[index];
                ++runs;
            });
        }
    }

    std::uint64_t verify(std::uint64_t expected_calls) const {
        const auto expected = selected_right ? right_value : left_value;
        std::uint64_t checksum{};
        for (std::size_t index = 0; index < values.size(); ++index) {
            require(values[index] == static_cast<std::int64_t>(expected) * 3 + static_cast<std::int64_t>(index),
                    "observer result differs from the active source");
            require(calls[index] == expected_calls, "observer callback was missed or duplicated");
            checksum += static_cast<std::uint64_t>(values[index]);
        }
        return checksum;
    }

    bool switch_source() {
        selected_right = !selected_right;
        return selector.set(selected_right);
    }

    bool update(bool active) {
        const bool update_right = active ? selected_right : !selected_right;
        auto& value = update_right ? right_value : left_value;
        value += 2;
        return update_right ? right.set(value) : left.set(value);
    }

    ryn::Signal<int> left{1};
    ryn::Signal<int> right{2};
    ryn::Signal<bool> selector{false};
    std::vector<std::int64_t> values;
    std::vector<std::uint64_t> calls;
    std::uint64_t runs{};
    std::uint64_t reads{};
    int left_value{1};
    int right_value{2};
    bool selected_right{};
    // Dispose observers before any callback capture is destroyed.
    ryn::Scope scope;

private:
    bool dynamic_{};
};

struct Sample final {
    Measurement measurement;
    std::uint64_t writes{};
    std::uint64_t runs{};
    std::uint64_t reads{};
    std::size_t epochs{};
    std::uint64_t checksum{};
};

void emit(std::size_t round, const char* mode, std::size_t count, const char* phase, const char* operation,
          const char* kind, std::size_t index, std::size_t samples, const Sample& value, std::int64_t percentile) {
    const auto& measurement = value.measurement;
    std::cout << "1," << round << ',' << mode << ',' << count << ',' << phase << ',' << operation << ',' << kind << ','
              << index << ',' << samples << ',' << value.writes << ',' << value.runs << ',' << value.reads << ','
              << value.epochs << ',' << value.checksum << ',' << measurement.elapsed_ns << ','
              << static_cast<double>(measurement.elapsed_ns) / static_cast<double>(samples) << ',' << percentile << ','
              << measurement.allocations << ',' << measurement.requested_bytes << ',' << measurement.largest_request
              << ",\n";
}

void emit_samples(std::size_t round, const char* mode, std::size_t count, const char* phase, const char* operation,
                  std::span<const Sample> samples) {
    if (samples.empty()) {
        return;
    }
    Sample total;
    std::vector<std::int64_t> durations;
    durations.reserve(samples.size());
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto& sample = samples[index];
        emit(round, mode, count, phase, operation, "sample", index + 1, 1, sample, sample.measurement.elapsed_ns);
        accumulate(total.measurement, sample.measurement);
        total.writes += sample.writes;
        total.runs += sample.runs;
        total.reads += sample.reads;
        total.epochs += sample.epochs;
        total.checksum = sample.checksum;
        durations.push_back(sample.measurement.elapsed_ns);
    }
    emit(round, mode, count, phase, operation, "summary", 0, samples.size(), total, p95(std::move(durations)));
}

void run_case(const Options& config, std::size_t round, std::size_t count, bool dynamic) {
    const char* mode = dynamic ? "dynamic" : "stable";
    std::unique_ptr<Fanout> graph;
    Sample cold;
    auto& scheduler = ryn::detail::Scheduler::current();
    const auto epoch = scheduler.epoch();
    cold.measurement = measure([&] { graph = std::make_unique<Fanout>(count, dynamic); });
    cold.runs = graph->runs;
    cold.reads = graph->reads;
    cold.epochs = scheduler.epoch() - epoch;
    cold.checksum = graph->verify(1);
    emit_samples(round, mode, count, "cold", "mount", std::span(&cold, 1));

    std::uint64_t expected_calls = 1;
    for (const bool hot : {false, true}) {
        const auto iterations = hot ? config.iterations : config.warmup;
        const char* phase = hot ? "hot" : "warmup";
        std::vector<Sample> switches(dynamic ? iterations : 0);
        std::vector<Sample> updates(iterations);
        std::vector<Sample> inactive(dynamic ? iterations : 0);
        const auto sample = [&](Sample& result, const auto& action, bool notified) {
            const auto before_runs = graph->runs;
            const auto before_reads = graph->reads;
            const auto before_epoch = scheduler.epoch();
            result.measurement = measure([&] { result.writes += action() ? 1 : 0; });
            result.runs = graph->runs - before_runs;
            result.reads = graph->reads - before_reads;
            result.epochs = scheduler.epoch() - before_epoch;
            expected_calls += notified ? 1 : 0;
            result.checksum = graph->verify(expected_calls);
            require(result.writes == 1, "measurement supplied an equal-value source update");
            require(result.runs == (notified ? count : 0), "fanout callback count differs");
            require(result.reads == result.runs * (dynamic ? 2 : 1), "source read count differs");
        };
        for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
            if (dynamic) {
                sample(switches[iteration], [&] { return graph->switch_source(); }, true);
            }
            sample(updates[iteration], [&] { return graph->update(true); }, true);
            if (dynamic) {
                sample(inactive[iteration], [&] { return graph->update(false); }, false);
            }
        }
        emit_samples(round, mode, count, phase, "switch", switches);
        emit_samples(round, mode, count, phase, "active_update", updates);
        emit_samples(round, mode, count, phase, "inactive_update", inactive);
    }

    const auto before_dispose = graph->runs;
    graph->scope.dispose();
    graph->scope.dispose();
    require(graph->update(true) && graph->update(false) && graph->switch_source(), "disposal fixture did not write");
    require(graph->runs == before_dispose, "disposed fanout still received notifications");
}

void verify_failed_mount_cleanup() {
    ryn::Signal<int> source{1};
    ryn::Scope scope;
    std::size_t failed_runs{};
    bool threw{};
    try {
        ryn::effect(scope, [&] {
            static_cast<void>(source.get());
            ++failed_runs;
            throw std::runtime_error("injected initial observer failure");
        });
    } catch (const std::runtime_error&) {
        threw = true;
    }
    require(threw && failed_runs == 1, "initial callback exception was swallowed");
    int observed{};
    std::size_t healthy_runs{};
    ryn::effect(scope, [&] {
        observed = source.get();
        ++healthy_runs;
    });
    require(source.set(2) && observed == 2 && healthy_runs == 2 && failed_runs == 1,
            "failed mount leaked a subscription or corrupted later observation");
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto config = options(argc, argv);
        if (config.help) {
            std::cout << "--iterations N --warmup N --rounds N --smoke\n"
                         "Defaults: 8 hot iterations, 2 warmup iterations, 1 round. CSV on stdout.\n";
            return 0;
        }
        std::cerr << "reactive_fanout: iterations=" << config.iterations << " warmup=" << config.warmup
                  << " rounds=" << config.rounds << " smoke=" << config.smoke
                  << "; subscriber_scans unavailable; cold means fresh graph, not cold process/cache\n";
        std::cout
            << "schema,round,mode,k,phase,operation,record,sample,samples,source_updates,observer_runs,source_reads,"
               "scheduler_epochs,result_checksum,total_ns,mean_ns,p95_ns,allocations,requested_bytes,"
               "largest_request,subscriber_scans\n";
        for (std::size_t round = 1; round <= config.rounds; ++round) {
            for (const std::size_t count : std::array<std::size_t, 5>{1, 64, 256, 1024, 4096}) {
                if (config.smoke && count > 64) {
                    continue;
                }
                run_case(config, round, count, false);
                run_case(config, round, count, true);
            }
        }
        verify_failed_mount_cleanup();
        std::cerr << "reactive_fanout: result/callback/dynamic/disposal/failed-mount checks passed\n";
    } catch (const std::exception& error) {
        ryn_test::allocation::end();
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
