#include "gallery_pages_host.hpp"
#include "component/selection_component.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/recording/recording_renderer.hpp"
#include "support/input_fixture.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace ryn;
using namespace rynui::example;
using Clock = std::chrono::steady_clock;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

struct Fixture final : ryn_test::input_component::Fixture {
    ryn::detail::RecordingRenderer backend;
    ryn::detail::SceneResources resources{backend};
    ReferenceSurfaceHost surfaces{buttons};
    ryn::detail::SelectionComponentHost selections{services};
    GalleryPagesHost host;
    graphics::OrderedScene visible;
    std::uint64_t upload_ns{};
    std::uint64_t submit_ns{};
    std::uint64_t syncs{};

    explicit Fixture(std::span<const GallerySampleDescriptor> registry = gallery_samples())
        : host(surfaces, inputs, frames, {}, registry) {
        buttons.text().set_sync_profiling_enabled(true);
        services.set_sync_profiling_enabled(true);
        host.mount({1440, 900});
        sync();
    }

    void sync() {
        require(host.synchronize({1440, 900}) && !host.snapshot().failed, "page synchronization failed");
        const auto started = Clock::now();
        require(resources.synchronize({&services.surfaces().instances(),
                                       scene.atlas(),
                                       scene.glyph_scene().instances(),
                                       &buttons.rounded_effects(),
                                       {1440, 900, 1},
                                       &services.images()}),
                "Recording upload failed");
        const auto uploaded = Clock::now();
        static_cast<void>(buttons.scene_composer().build_visible_scene(nodes, {0, 0, 1440, 900}, visible));
        require(backend.attach_scene(resources.attach(visible)), "Recording attach failed");
        require(backend.submit_frame(animation::AnimationTime::microseconds(++syncs * 1000)) ==
                    runtime::FrameSubmissionResult::submitted,
                "Recording scene submission failed");
        upload_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(uploaded - started).count();
        submit_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - uploaded).count();
    }

    void show(std::string_view identity) {
        require(host.request(host.model().find(identity)->route()), "benchmark route rejected");
        static_cast<void>(host.process());
        sync();
    }

    std::array<std::size_t, 6> live() {
        return {nodes.size(),
                services.components().component_count(),
                services.interactions().size(),
                inputs.editors().size(),
                services.surfaces().size(),
                backend.live_resources()};
    }
};

struct Trial final {
    std::array<std::size_t, 6> live;
    std::uint64_t mounted_visited{};
    std::uint64_t input_ns{};
    std::uint64_t layout_ns{};
    std::uint64_t sync_other_ns{};
    std::uint64_t upload_ns{};
    std::uint64_t submit_ns{};
};

Trial wheel_trial(std::span<const GallerySampleDescriptor> registry) {
    Fixture f(registry);
    const auto mount_started = Clock::now();
    f.show("ant.component.button");
    const auto mount_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - mount_started).count();
    const auto counts = f.live();
    const auto work = f.host.work();
    const auto before = f.buttons.text().sync_profile();
    const auto uploaded = f.upload_ns;
    const auto submitted = f.submit_ns;
    const auto lane = f.host.snapshot().layout.document;
    for (std::size_t i = 0; i < 32; ++i) {
        require(f.host.inject_scroll_at(lane.x + lane.width - 8, lane.y + 200, (i % 16 < 8) ? -0.5F : 0.5F),
                "real Gallery wheel injection rejected");
        f.sync();
    }
    const auto after = f.buttons.text().sync_profile();
    require(f.host.work().wheel_events - work.wheel_events == 32 &&
                f.host.work().document_wheels - work.document_wheels == 32,
            "benchmark bypassed component wheel dispatch");
    require(f.host.work().page_mounts == work.page_mounts && f.live() == counts,
            "steady wheel rebuilt page or accumulated live resources");
    for (const auto& sample : gallery_samples()) {
        require(f.host.sample_runs(sample.identity) == (sample.component_identity == "ant.component.button" ? 1 : 0),
                "an unvisited page factory ran");
    }
    const auto layout = after.layout_nanoseconds - before.layout_nanoseconds;
    const auto combined = f.host.work().layout_sync_nanoseconds - work.layout_sync_nanoseconds;
    require(combined >= layout, "layout timing escaped host synchronization interval");
    Trial result{counts,
                 after.mounted_visited - before.mounted_visited,
                 f.host.work().input_nanoseconds - work.input_nanoseconds,
                 layout,
                 combined - layout,
                 f.upload_ns - uploaded,
                 f.submit_ns - submitted};
    std::cout << "registry_entries=" << registry.size() << " wheel_events=32 mount_switch_ns=" << mount_ns
              << " input_ns=" << result.input_ns << " layout_ns=" << result.layout_ns
              << " host_sync_other_ns=" << result.sync_other_ns << " upload_ns=" << result.upload_ns
              << " recording_cull_submit_ns=" << result.submit_ns << " mounted_visits=" << result.mounted_visited
              << " live_nodes=" << counts[0] << '\n';
    return result;
}

void test_registry_independence() {
    std::vector<GallerySampleDescriptor> button_only;
    for (const auto& sample : gallery_samples()) {
        if (sample.component_identity == "ant.component.button") {
            button_only.push_back(sample);
        }
    }
    const auto isolated = wheel_trial(button_only);
    const auto full = wheel_trial(gallery_samples());
    require(isolated.live == full.live && isolated.mounted_visited == full.mounted_visited,
            "adding unvisited registry factories increased current-page work or resources");
}

void test_round_trips_and_idle() {
    Fixture f;
    for (int i = 0; i < 2; ++i) {
        f.show("ant.component.input");
        f.show("ant.component.button");
    }
    const auto baseline = f.live();
    for (int i = 0; i < 8; ++i) {
        f.show("ant.component.input");
        f.show("ant.component.button");
        require(f.live() == baseline, "page round trips increased live CPU/GPU resources");
    }
    f.show("ant.component.upload");
    f.host.dispatch(input::WindowInputEvent{input::WindowInputAction::focus_lost});
    f.sync();
    static_cast<void>(f.frames.consume_request());
    const auto uploads = f.backend.counters().uploads;
    f.sync();
    require(!f.frames.pending() && !f.services.next_frame_deadline() && f.backend.counters().uploads == uploads,
            "settled blurred page keeps requesting work or uploads");
    f.host.shutdown();
    f.resources.retire();
    require(f.nodes.size() == 0 && f.services.components().component_count() == 0 && f.inputs.editors().size() == 0 &&
                f.backend.live_resources() == 0,
            "shutdown retained page resources");
    std::cout << "round_trips=8 idle_uploads=0 shutdown_live_resources=0\n";
}
} // namespace

int main() {
    try {
        test_registry_independence();
        test_round_trips_and_idle();
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
