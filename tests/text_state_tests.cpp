#include "runtime/component_mount.hpp"
#include "runtime/frame_scheduler.hpp"
#include "text/text_engine.hpp"

#include <ryn/reactive.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using ryn::String;
using ryn::font::FontIdentity;
using ryn::font::FontRuntime;
using ryn::text::TextEngine;
using ryn::text::TextLayoutConfig;
using ryn::text::TextState;
using ryn::text::TextStateCounters;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::unique_ptr<FontRuntime> create_runtime() {
    auto created = FontRuntime::create();
    require(static_cast<bool>(created), "Font Runtime initialization failed");
    return std::move(created.runtime);
}

std::vector<FontIdentity> load_chain(FontRuntime& fonts, std::uint32_t pixel_size) {
    const auto latin = fonts.load_font_file(
        RYNUI_VALIDATION_LATIN_FONT, 0, pixel_size);
    const auto cjk = fonts.load_font_file(
        RYNUI_VALIDATION_CJK_FONT, 0, pixel_size);
    require(latin && cjk, "Text state validation fonts failed to load");
    return {latin.font, cjk.font};
}

void test_layered_text_invalidation_with_signals() {
    auto fonts = create_runtime();
    TextEngine engine{*fonts};
    const auto chain_14 = load_chain(*fonts, 14);
    const auto chain_18 = load_chain(*fonts, 18);
    ryn::runtime::FrameRequestState frames;

    TextState target{
        engine,
        String{u8"Hello"},
        chain_14,
        14,
        TextLayoutConfig{20.0F, std::numeric_limits<float>::infinity()},
        [&] { frames.request_frame(); }};
    TextState unrelated{
        engine,
        String{u8"Unrelated"},
        chain_14,
        14,
        TextLayoutConfig{20.0F, std::numeric_limits<float>::infinity()}};
    require(target.synchronize() && unrelated.synchronize(),
            "initial Text state synchronization failed");
    require(!frames.pending(), "initial Text synchronization requested a frame");

    ryn::Signal<String> content{String{u8"Hello"}};
    ryn::Signal<std::array<float, 4>> color{
        std::array<float, 4>{0.0F, 0.0F, 0.0F, 1.0F}};
    ryn::Signal<float> width{std::numeric_limits<float>::infinity()};

    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentInstance target_component{
        nodes,
        [&](ryn::runtime::MountContext& context) {
            static_cast<void>(context.create_root());
            static_cast<void>(ryn::connect_binding(
                context.scope(),
                ryn::bind([&] { return content.get(); }),
                [&](String value) {
                    if (target.set_content(std::move(value))) {
                        require(target.synchronize(), "content binding failed");
                    }
                }));
            static_cast<void>(ryn::connect_binding(
                context.scope(),
                ryn::bind([&] { return color.get(); }),
                [&](std::array<float, 4> value) {
                    if (target.set_color(value)) {
                        require(target.synchronize(), "color binding failed");
                    }
                }));
            static_cast<void>(ryn::connect_binding(
                context.scope(),
                ryn::bind([&] { return width.get(); }),
                [&](float value) {
                    if (target.set_width_constraint(value)) {
                        require(target.synchronize(), "width binding failed");
                    }
                }));
        }};
    ryn::runtime::ComponentInstance unrelated_component{
        nodes,
        [&](ryn::runtime::MountContext& context) {
            static_cast<void>(context.create_root());
        }};
    require(target_component.mount_runs() == 1
                && unrelated_component.mount_runs() == 1,
            "Text test components did not mount exactly once");
    require(!frames.pending(), "equal initial bindings requested a frame");

    const TextStateCounters initial = target.counters();
    const TextStateCounters unrelated_initial = unrelated.counters();
    color.set({0.2F, 0.4F, 0.6F, 1.0F});
    require(frames.pending() && frames.counters().requests == 1,
            "Material change did not request the next frame");
    require(target.counters().shape_count == initial.shape_count
                && target.counters().measure_count == initial.measure_count
                && target.counters().layout_count == initial.layout_count
                && target.counters().material_range_updates
                    == initial.material_range_updates + 1,
            "color change invalidated text geometry");
    require(unrelated.counters().shape_count == unrelated_initial.shape_count
                && unrelated.counters().measure_count == unrelated_initial.measure_count,
            "color change invalidated unrelated Text state");
    require(target_component.mount_runs() == 1
                && unrelated_component.mount_runs() == 1,
            "color binding reran a Component");
    require(frames.consume_request(), "Material frame request could not be consumed");

    const TextStateCounters after_color = target.counters();
    content.set(String{u8"Hello 中文"});
    require(frames.pending(), "content change did not request the next frame");
    require(target.counters().shape_count == after_color.shape_count + 1
                && target.counters().measure_count == after_color.measure_count + 1
                && target.counters().layout_count == after_color.layout_count + 1
                && target.counters().material_range_updates
                    == after_color.material_range_updates,
            "content change did not reprocess exactly the target Text geometry");
    require(unrelated.counters().shape_count == unrelated_initial.shape_count
                && unrelated.counters().measure_count == unrelated_initial.measure_count,
            "content change reprocessed unrelated Text state");
    require(target_component.mount_runs() == 1
                && unrelated_component.mount_runs() == 1,
            "content binding reran a Component");
    require(frames.consume_request(), "content frame request could not be consumed");

    const TextStateCounters before_width = target.counters();
    width.set(target.measurement().width * 0.55F);
    require(target.counters().shape_count == before_width.shape_count
                && target.counters().measure_count == before_width.measure_count + 1
                && target.counters().layout_count == before_width.layout_count + 1,
            "width constraint did not preserve the shaped run");
    require(frames.consume_request(), "constraint frame request could not be consumed");

    const TextStateCounters before_line_height = target.counters();
    require(target.set_line_height(28.0F), "line-height change was ignored");
    require(target.synchronize(), "line-height synchronization failed");
    require(target.counters().shape_count == before_line_height.shape_count
                && target.counters().measure_count == before_line_height.measure_count + 1
                && target.counters().layout_count == before_line_height.layout_count + 1,
            "line-height change unnecessarily reshaped text");
    require(frames.consume_request(), "line-height frame request could not be consumed");

    const TextStateCounters before_font = target.counters();
    require(target.set_font_chain(chain_18), "font-chain revision was ignored");
    require(target.set_pixel_size(18), "pixel-size revision was ignored");
    require(target.synchronize(), "font revision synchronization failed");
    require(target.counters().shape_count == before_font.shape_count + 1
                && target.counters().measure_count == before_font.measure_count + 1
                && target.counters().layout_count == before_font.layout_count + 1,
            "font revisions did not coalesce into one Text reprocess");
    require(frames.consume_request(), "font frame request could not be consumed");

    const TextStateCounters before_opacity = target.counters();
    require(target.set_opacity(0.5F), "opacity change was ignored");
    require(target.synchronize(), "opacity synchronization failed");
    require(target.counters().shape_count == before_opacity.shape_count
                && target.counters().measure_count == before_opacity.measure_count
                && target.counters().layout_count == before_opacity.layout_count
                && target.counters().material_range_updates
                    == before_opacity.material_range_updates + 1,
            "opacity change invalidated text geometry");
    require(frames.consume_request(), "opacity frame request could not be consumed");

    bool rejected_invalid_opacity = false;
    try {
        static_cast<void>(target.set_opacity(std::numeric_limits<float>::quiet_NaN()));
    } catch (const std::invalid_argument&) {
        rejected_invalid_opacity = true;
    }
    require(rejected_invalid_opacity, "invalid opacity entered Text Material state");
    require(target_component.mount_runs() == 1
                && unrelated_component.mount_runs() == 1,
            "Text property updates reran a Component");
}

} // namespace

int main() {
    try {
        test_layered_text_invalidation_with_signals();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
