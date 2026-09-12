#include "support/input_fixture.hpp"
#include "support/input_counting_gpu.hpp"
#include "support/allocation_probe.hpp"
#include <chrono>
#include <iostream>

namespace {
using namespace ryn;
using namespace ryn::input;
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
void run(std::size_t iterations, bool composing) {
    ryn_test::input_component::Fixture f;
    f.inputs.mount(Content{[&] {
        for(int index = 0; index < 256; ++index) Input(InputProps{}.defaultValue(u8"abcdef 中文"));
        Text(u8"unrelated retained Text");
    }});
    const auto sync = [&] {
        require(f.buttons.layout_and_synchronize({320, 9000}, {0, 0, 320, 9000}), "benchmark sync failed");
    };
    sync();
    const auto target = f.inputs.mounted_inputs().front();
    require(f.buttons.focus().request_focus(target.interaction, FocusModality::keyboard), "benchmark focus failed");
    const auto stamp = f.inputs.sessions().active();
    CompositionChanged preedit{String{u8"compose"}, {0, 1}, stamp};
    if(composing) require(bool(f.inputs.dispatch(preedit)), "benchmark preedit failed");
    ryn_test::input_component::CountingGpu api;
    graphics::QuadGpuBuffer quads{api, f.buttons.button_scene().instances()};
    detail::GlyphGpuResources glyphs{api};
    detail::RoundedEffectGpuResources effects{api};
    std::size_t dispatch_allocations{}, sync_allocations{};
    const auto cycle = [&](std::size_t index) {
        preedit.selection = {index % 6, 1};
        const auto before_dispatch = ryn_test::allocation::count;
        if(composing) require(bool(f.inputs.dispatch(preedit)), "benchmark range change failed");
        else {
            require(bool(f.inputs.editors().require(target.editor).select({0, index % 6 + 1})), "benchmark selection failed");
            f.dirty.invalidate(target.node, runtime::DirtyFlags::Geometry);
        }
        const auto after_dispatch = ryn_test::allocation::count;
        sync();
        quads.synchronize(f.buttons.button_scene().instances());
        glyphs.synchronize(f.scene.atlas(), f.scene.glyph_scene().instances());
        effects.synchronize(f.buttons.rounded_effects(), {320, 9000, 1});
        if(ryn_test::allocation::tracking) {
            dispatch_allocations += after_dispatch - before_dispatch;
            sync_allocations += ryn_test::allocation::count - after_dispatch;
        }
        static_cast<void>(f.frames.consume_request());
    };
    for(std::size_t index = 0; index < 20; ++index) cycle(index);
    api.selected = f.scene.primitive(f.inputs.text_layers(target.component).selected).instances;
    api.check_ranges = true;
    const auto textures = api.texture_uploads, effect_uploads = api.effect_uploads;
    const auto glyph_count = f.scene.glyph_scene().instances().size();
    const auto quad_capacity = f.buttons.button_scene().instances().capacity();
    const auto effect_capacity = f.buttons.rounded_effects().slot_capacity();
    const auto composer_rebuilds = f.buttons.scene_composer().diagnostics().rebuilds;
    const auto hit = f.buttons.hit_test().diagnostics();
    const auto measures = f.nodes.require(target.node).measure_count;
    const auto components = f.buttons.components().component_count();
    const auto mount_runs = f.buttons.components().mount_runs();
    struct WorkCounts { std::uint64_t shapes, measures, placements; };
    std::array<WorkCounts, 256> work;
    for(std::size_t index = 0; index < work.size(); ++index) {
        const auto& input = f.inputs.mounted_inputs()[index];
        const auto& node = f.nodes.require(input.node);
        work[index] = {f.scene.text_state(f.inputs.text_scene(input.component)).counters().shape_count,
            node.measure_count, node.place_count};
    }
    const auto sibling = f.buttons.text().mounted_texts().front().scene;
    const auto sibling_shapes = f.scene.text_state(sibling).counters().shape_count;
    const auto begin = std::chrono::steady_clock::now();
    ryn_test::allocation::begin();
    for(std::size_t index = 0; index < iterations; ++index) cycle(index);
    const auto allocations = ryn_test::allocation::end();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
    std::cout << "inputs=256 mode=" << (composing ? "composition-selection" : "selection")
        << " updates=" << iterations << " allocations=" << allocations << " elapsed_ms=" << elapsed << '\n';
    std::cout << "dispatch_allocations=" << dispatch_allocations << " sync_allocations=" << sync_allocations << '\n';
    require(allocations == 0, "Input retained scene allocates after warmup");
    require(api.texture_uploads == textures && api.effect_uploads == effect_uploads,
        "Input range update uploaded atlas/effects");
    require(f.scene.glyph_scene().instances().size() == glyph_count
        && f.buttons.button_scene().instances().capacity() == quad_capacity
        && f.buttons.rounded_effects().slot_capacity() == effect_capacity
        && f.buttons.components().component_count() == components
        && f.buttons.components().mount_runs() == mount_runs
        && f.buttons.scene_composer().diagnostics().rebuilds == composer_rebuilds,
        "Input retained scene capacity/topology grew");
    for(std::size_t index = 0; index < work.size(); ++index) {
        const auto& input = f.inputs.mounted_inputs()[index];
        const auto& node = f.nodes.require(input.node);
        require(node.measure_count == work[index].measures && node.place_count == work[index].placements
            && f.scene.text_state(f.inputs.text_scene(input.component)).counters().shape_count == work[index].shapes,
            "Input update reshaped, measured or laid out a retained Input");
    }
    require(f.nodes.require(target.node).measure_count == measures
        && f.scene.text_state(sibling).counters().shape_count == sibling_shapes
        && f.buttons.hit_test().diagnostics().records_refreshed == hit.records_refreshed,
        "Input range updates touched Measure, sibling Text or HitTest");
}
}
int main(int argc, char** argv) {
    try {
        const auto iterations = argc > 1 ? std::stoull(argv[1]) : 20000;
        run(iterations, false); run(iterations, true);
    }
    catch(const std::exception& error) { ryn_test::allocation::end(); std::cerr << error.what() << '\n'; return 1; }
}
