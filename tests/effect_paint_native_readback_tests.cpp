#include "graphics/color_field.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "effect_linear_source_oracle.hpp"
#if defined(RYNUI_LINEAR_S2_VARIANT_DIRECTORY)
#include "effect_linear_s2_tests.hpp"
#include "renderer/common/composition_packing.hpp"
#endif

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdlib>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif
#endif

#include <SDL3/SDL.h>

#include <algorithm>
#include <bit>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
using namespace ryn;
using namespace ryn::graphics;
using namespace ryn::detail;

std::size_t checks{};
std::size_t cases{};
std::filesystem::path output_directory;

void require(bool value, const std::string& message) {
    ++checks;
    if (!value) {
        throw std::runtime_error(message);
    }
}

void no_dialogs() {
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#if defined(_MSC_VER)
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    for (const int kind : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT}) {
        _CrtSetReportMode(kind, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(kind, _CRTDBG_FILE_STDERR);
    }
#endif
#endif
    SDL_SetAssertionHandler(
        [](const SDL_AssertData* data, void*) -> SDL_AssertState {
            std::cerr << "SDL assertion: " << data->condition << " at " << data->filename << ':' << data->linenum
                      << '\n';
            return SDL_ASSERTION_ABORT;
        },
        nullptr);
}

PlatformCreateResult hidden_host(int width = 128, int height = 96) {
    PlatformConfig config;
    config.title = "RynUI hidden effect paint contract";
    config.width = width;
    config.height = height;
    config.high_pixel_density = false;
    config.hidden = true;
    return PlatformState::create(config);
}

void check_hidden(PlatformState& platform) {
    const auto flags = SDL_GetWindowFlags(static_cast<SDL_Window*>(platform.window()));
    require((flags & SDL_WINDOW_HIDDEN) != 0 && (flags & SDL_WINDOW_INPUT_FOCUS) == 0,
            "Readback window must stay hidden without input focus");
}

void pixel(const SceneRgbaReadback& frame, int x, int y, std::array<float, 4> expected, const std::string& label,
           int tolerance = 2) {
    require(x >= 0 && y >= 0 && std::uint32_t(x) < frame.width && std::uint32_t(y) < frame.height,
            label + ": pixel bounds");
    const auto offset = std::size_t(y) * frame.row_pitch + std::size_t(x) * 4;
    for (std::size_t channel = 0; channel < 4; ++channel) {
        const int actual = std::to_integer<unsigned char>(frame.pixels[offset + channel]);
        const int target = static_cast<int>(std::lround(std::clamp(expected[channel], 0.0F, 1.0F) * 255));
        require(std::abs(actual - target) <= tolerance,
                label + ": RGBA[" + std::to_string(channel) + "] at " + std::to_string(x) + "," + std::to_string(y) +
                    " actual=" + std::to_string(actual) + " expected=" + std::to_string(target));
    }
}

struct Fixture final {
    SdlSceneRenderer& backend;
    GlyphAtlas atlas;
    GlyphInstanceStore glyphs;
    RoundedEffectStore effects;
    SceneResources resources;
    OrderedScene order;
    SceneDeviceMetrics metrics;

    Fixture(SdlSceneRenderer& renderer, PlatformState& platform, float scale = 1)
        : backend(renderer), resources(renderer) {
        const auto window = platform.window_metrics();
        metrics = {static_cast<std::uint32_t>(window.pixel_width), static_cast<std::uint32_t>(window.pixel_height),
                   scale};
    }

    bool sync() {
        return resources.synchronize({nullptr, atlas, glyphs, &effects, metrics});
    }

    void all() {
        order.clear();
        const auto count = resources.effects().instance_count();
        if (count) {
            order.append_command({SceneDrawKind::rounded_effect, 0, count, invalid_glyph_atlas_page});
        }
    }

    SceneRgbaReadback read(const std::string& label, bool rebuild_order = true) {
        const bool synchronized = sync();
        require(synchronized, label + ": synchronize: " + backend.last_error());
        if (rebuild_order) {
            all();
        }
        require(backend.attach_scene(resources.attach(order)), label + ": attach");
        SceneRgbaReadback result;
        require(backend.read_frame_rgba(result), label + ": GPU readback: " + backend.last_error());
        require(result.row_pitch == result.width * 4 &&
                    result.pixels.size() == std::size_t(result.row_pitch) * result.height,
                label + ": owned tight RGBA rows");
        std::ofstream raw(output_directory / (label + ".rgba"), std::ios::binary);
        raw.write(reinterpret_cast<const char*>(result.pixels.data()),
                  static_cast<std::streamsize>(result.pixels.size()));
        require(bool(raw), label + ": raw output");
        ++cases;
        return result;
    }

    void reference(const SceneRgbaReadback& frame, int x, int y, const std::string& label, int tolerance = 2) {
        std::array<float, 4> framebuffer{};
        const auto& projection = resources.effects();
        const auto bases = projection.instances();
        const auto& tables = projection.paint_tables();
        const auto caps = projection.cap_geometry();
        for (const auto& command : order.commands()) {
            for (std::uint32_t index = command.first_instance; index < command.first_instance + command.instance_count;
                 ++index) {
                const auto color =
                    tables.links.empty()
                        ? rounded_effect_gpu_fragment_reference({float(x) + 0.5F, float(y) + 0.5F}, bases[index])
                        : effect_paint_gpu_fragment_reference({float(x) + 0.5F, float(y) + 0.5F}, bases[index],
                                                              tables.links[index], tables.fields, tables.stops,
                                                              caps.empty() ? nullptr : &caps[index]);
                for (std::size_t channel = 0; channel < 3; ++channel) {
                    framebuffer[channel] = color[channel] * color[3] + framebuffer[channel] * (1 - color[3]);
                }
                framebuffer[3] = color[3] + framebuffer[3] * (1 - color[3]);
            }
        }
        pixel(frame, x, y, framebuffer, label, tolerance);
    }

    void encloses(std::uint32_t index, int x, int y) {
        const auto& rect = resources.effects().instances()[index].clip_rect;
        const double left = (double(rect[0]) + 1) * metrics.pixel_width / 2;
        const double top = (1 - double(rect[1])) * metrics.pixel_height / 2;
        const double right = left + double(rect[2]) * metrics.pixel_width / 2;
        const double bottom = top - double(rect[3]) * metrics.pixel_height / 2;
        require(x + 0.5 >= left && x + 0.5 < right && y + 0.5 >= top && y + 0.5 < bottom,
                "independent expected sample must lie in actual packed draw domain");
    }
};

EffectPaintField constant(Color color) {
    return {EffectGradientRamp{{0, color}, {1, color}}, EffectLinearDomain{{0, 0}, {1, 0}}};
}

RoundedEffectInstance fill(runtime::Rect bounds = {8, 8, 40, 32}) {
    return make_shadow_effect({bounds, 0}, {ShadowKind::outer, {}, 0, 0, Color{1, 1, 1}});
}

std::array<float, 4> source_linear_pixel(const RoundedEffectInstance& effect, float scale, runtime::Point point,
                                         float coverage = 1) {
    const auto& paint = *effect.material.paint;
    auto color = effect_source_oracle::sample(paint.foreground, effect.geometry.translation, scale, point);
    if (paint.underlay) {
        const auto underlay = effect_source_oracle::sample(*paint.underlay, effect.geometry.translation, scale, point);
        const auto remaining = 1 - color[3];
        for (std::size_t channel = 0; channel < 4; ++channel) {
            color[channel] += underlay[channel] * remaining;
        }
    }
    std::array<float, 4> result;
    for (std::size_t channel = 0; channel < 4; ++channel) {
        result[channel] = static_cast<float>(color[channel] * coverage * effect.material.opacity);
    }
    return result;
}

using CostClock = std::chrono::steady_clock;

struct LinearCostOptions final {
    int width{128};
    int height{96};
    std::string field{"all"};
    std::string layers{"both"};
};

constexpr const char* native_usage =
    "Expected --expect-driver direct3d12|vulkan --suite transport|caps|all|linear-cost --output <ignored out directory>"
    " [--linear-size 128x96|1920x1080] [--linear-case all|horizontal|diagonal|l1|l2|adjacent|extreme]"
    " [--linear-layers single|double|both]. Linear options require --suite linear-cost."
    " 1920x1080 requires one explicit --linear-case and --linear-layers single or double; run 128x96 first.";

double cost_ms(CostClock::time_point from, CostClock::time_point to = CostClock::now()) {
    return std::chrono::duration<double, std::milli>(to - from).count();
}

struct LinearCostLog final {
    CostClock::time_point suite_started;
    std::ofstream csv{output_directory / "linear-cost.csv"};

    explicit LinearCostLog(CostClock::time_point started) : suite_started(started) {
        require(bool(csv), "linear-cost CSV open failed");
        csv << "case,phase,elapsed_ms,suite_elapsed_ms,pixels,oracle_samples,uploaded_bytes,upload_regions,"
               "pipeline_creations\n";
        csv.flush();
        std::cout << "linear_cost_timing=CPU_wall_clock GPU_timestamps=unavailable "
                     "cold_pipeline_includes_buffer_create=true lazy_driver_JIT_in_first_readback=true "
                     "pack_preflight_and_upload_share_sync_interval=true total_includes_oracle_and_io=true "
                     "driver_cache_state=uncontrolled\n"
                  << std::flush;
    }

    void budget() const {
        require(CostClock::now() - suite_started < std::chrono::seconds(55),
                "linear-cost exhausted the existing 55-second workload; external guard must bound blocking GPU calls");
    }

    CostClock::time_point begin(const std::string& name, const char* phase) {
        budget();
        std::cout << "linear_cost_begin case=" << name << " phase=" << phase
                  << " suite_elapsed_ms=" << cost_ms(suite_started) << '\n'
                  << std::flush;
        return CostClock::now();
    }

    void record(const std::string& name, const char* phase, CostClock::time_point started, std::size_t pixels = 0,
                std::size_t samples = 0, std::uint64_t bytes = 0, std::uint64_t regions = 0,
                std::uint64_t pipelines = 0) {
        const auto ended = CostClock::now();
        csv << name << ',' << phase << ',' << cost_ms(started, ended) << ',' << cost_ms(suite_started, ended) << ','
            << pixels << ',' << samples << ',' << bytes << ',' << regions << ',' << pipelines << '\n';
        csv.flush();
        require(bool(csv), "linear-cost CSV write failed");
        budget();
    }
};

RoundedEffectInstance linear_cost_source(const std::string& name, const LinearCostOptions& options, bool dual,
                                         float scale) {
    // Oversize only the shape, so every viewport sample has coverage=1. The
    // field remains independent of the coverage box, including cancellation.
    auto effect = fill({-4 / scale, -4 / scale, (options.width + 8) / scale, (options.height + 8) / scale});
    const float alpha = dual ? 0.5F : 1.0F;
    EffectLinearDomain domain{{0, 0}, {float(options.width) / scale, 0}};
    EffectGradientRamp ramp{{0, Color{1, 0, 0, alpha}}, {1, Color{0, 0, 1, alpha}}};
    if (name == "diagonal") {
        domain = {{-2, -3}, {float(options.width) / scale, float(options.height) / scale}};
    } else if (name == "l1") {
        domain = {{-16777211, -16777210}, {-16777203, -16777218}};
        ramp = EffectGradientRamp{{0, Color{1, 0, 0, alpha}},
                                  {1.0F / 32, Color{1, 0, 0, alpha}},
                                  {1.0F / 32, Color{0, 0, 1, alpha}},
                                  {1, Color{0, 0, 1, alpha}}};
    } else if (name == "l2") {
        effect = fill({33554428, 33554428, float(options.width + 8), float(options.height + 8)});
        effect.geometry.translation = {-33554432, -33554432};
        domain = {{33554432, 33554432}, {33554440, 33554424}};
        ramp = EffectGradientRamp{{-0.125F, Color{1, 0, 0, alpha}}, {0, Color{0, 0, 1, alpha}}};
    } else if (name == "adjacent") {
        domain = {{-100000000, -100000000}, {-99999992, -100000008}};
        ramp = EffectGradientRamp{{std::nextafter(-0.0625F, -1.0F), Color{1, 0, 0, alpha}},
                                  {std::nextafter(-0.0625F, 0.0F), Color{0, 0, 1, alpha}}};
    } else if (name == "extreme") {
        const auto maximum = std::numeric_limits<float>::max();
        domain = {{16.5F, -maximum / 4}, {17.5F, -maximum / 4}};
        ramp = EffectGradientRamp{{-maximum, Color{1, 0, 0, alpha}}, {maximum, Color{0, 0, 1, alpha}}};
    }
    effect.material.paint = EffectPaint{{ramp, domain}, std::nullopt};
    if (dual) {
        // Always translucent, distinct, and Linear: an opaque-foreground skip
        // cannot make this double-field measurement look like a single field.
        if (name == "l1") {
            domain = {{-16777210, -16777210}, {-16777202, -16777218}};
        } else {
            std::swap(domain.start, domain.end);
        }
        effect.material.paint->underlay =
            EffectPaintField{EffectGradientRamp{{0, Color{0, 1, 0, 0.5F}}, {1, Color{1, 1, 0, 0.5F}}}, domain};
    }
    return effect;
}

void linear_cost_frame(Fixture& fixture, const RoundedEffectInstance& effect, const std::string& name,
                       LinearCostLog& log, bool full_oracle, bool idle = false, bool full_upload = false) {
    const auto total_started = log.begin(name, "frame_total");
    const auto before = fixture.backend.counters();
    const auto full_before = fixture.resources.effects().counters().full_uploads;
    auto stage = log.begin(name, "cpu_preflight_pack_upload");
    require(fixture.sync(), name + ": synchronize: " + fixture.backend.last_error());
    const auto after = fixture.backend.counters();
    log.record(name, "cpu_preflight_pack_upload", stage, 0, 0, after.uploaded_bytes - before.uploaded_bytes,
               after.buffer_upload_regions - before.buffer_upload_regions,
               after.effect_paint_pipeline_creations - before.effect_paint_pipeline_creations);
    if (idle) {
        require(after.uploaded_bytes == before.uploaded_bytes &&
                    after.upload_submissions == before.upload_submissions &&
                    after.buffer_upload_regions == before.buffer_upload_regions,
                "linear-cost static idle uploaded data");
    } else {
        require(after.uploaded_bytes > before.uploaded_bytes, "linear-cost dirty scene did not upload");
    }
    if (full_upload) {
        require(fixture.resources.effects().counters().full_uploads > full_before,
                "linear-cost metrics projection not full");
    }
    stage = log.begin(name, "cpu_order_attach");
    fixture.all();
    require(fixture.resources.effects().instance_count() == 1, "linear-cost must draw exactly one viewport effect");
    const auto& tables = fixture.resources.effects().paint_tables();
    require(tables.fields.size() == (effect.material.paint->underlay ? 2u : 1u),
            "linear-cost actual field count mismatch");
    for (const auto& field : tables.fields) {
        require(field.kind == static_cast<std::uint32_t>(EffectPaintFieldKind::linear),
                "linear-cost accidentally selected a non-Linear field");
    }
    require(fixture.backend.attach_scene(fixture.resources.attach(fixture.order)), name + ": attach");
    log.record(name, "cpu_order_attach", stage);
    stage = log.begin(name, "submit_fence_readback");
    SceneRgbaReadback frame;
    require(fixture.backend.read_frame_rgba(frame), name + ": readback: " + fixture.backend.last_error());
    const auto pixels = std::size_t(frame.width) * frame.height;
    log.record(name, "submit_fence_readback", stage, pixels);
    require(frame.width == fixture.metrics.pixel_width && frame.height == fixture.metrics.pixel_height &&
                frame.row_pitch == frame.width * 4 && frame.pixels.size() == pixels * 4,
            "linear-cost actual readback extent or tight owned RGBA differs");
    stage = log.begin(name, "independent_source_oracle");
    std::size_t samples{};
    const auto probe = [&](int x, int y) {
        fixture.encloses(0, x, y);
        pixel(frame, x, y,
              source_linear_pixel(effect, fixture.metrics.display_scale, {float(x) + 0.5F, float(y) + 0.5F}), name);
        ++samples;
    };
    if (full_oracle) {
        require(pixels == 128 * 96, "linear-cost full oracle is bounded to the small matrix");
        for (int y = 0; y < int(frame.height); ++y) {
            log.budget();
            for (int x = 0; x < int(frame.width); ++x) {
                probe(x, y);
            }
        }
    } else {
        // A finite 5x5 lattice plus both semantic counterexample points. Never
        // run a two-million-pixel CPU oracle inside the large GPU cost probe.
        for (int row = 0; row < 5; ++row) {
            for (int column = 0; column < 5; ++column) {
                probe(int((frame.width - 1) * column / 4), int((frame.height - 1) * row / 4));
            }
        }
        probe(16, 16);
        probe(16, 17);
        require(samples == 27, "linear-cost large oracle must remain strictly bounded");
    }
    log.record(name, "independent_source_oracle", stage, pixels, samples);
    stage = log.begin(name, "raw_output");
    std::ofstream raw(output_directory / (name + ".rgba"), std::ios::binary);
    raw.write(reinterpret_cast<const char*>(frame.pixels.data()), static_cast<std::streamsize>(frame.pixels.size()));
    raw.close();
    require(bool(raw), name + ": raw output");
    log.record(name, "raw_output", stage, pixels);
    ++cases;
    log.record(name, "frame_total_including_oracle_io", total_started, pixels, samples);
}

void linear_cost(SdlSceneRenderer& renderer, PlatformState& platform, const LinearCostOptions& options,
                 CostClock::time_point suite_started) {
    LinearCostLog log(suite_started);
    require(!renderer.capabilities().effect_paints && !renderer.capabilities().effect_linear_fields &&
                !renderer.capabilities().effect_conic_fields && !renderer.capabilities().effect_underlays &&
                !renderer.capabilities().effect_arc_caps,
            "linear-cost production flags must remain OFF");
    require(renderer.begin_pending_paint_validation_for_test(), "linear-cost candidate validation scope failed");

    struct PaintScope final {
        SdlSceneRenderer& renderer;

        ~PaintScope() {
            renderer.end_pending_paint_validation_for_test();
        }
    } scope{renderer};

    const auto cold = log.begin("suite", "cold_pipeline_jit_buffer_create");
    require(renderer.counters().effect_paint_pipeline_creations == 0, "linear-cost requires a cold paint pipeline");
    auto* buffer = renderer.create_effect_paint_buffer(EffectPaintBufferKind::link, effect_paint_link_stride);
    require(buffer != nullptr, "linear-cost cold pipeline/buffer creation: " + std::string(renderer.last_error()));
    renderer.release_effect_paint_buffer(EffectPaintBufferKind::link, buffer);
    require(renderer.counters().effect_paint_pipeline_creations == 1, "linear-cost cold pipeline was not created");
    log.record("suite", "cold_pipeline_jit_buffer_create", cold, 0, 0, 0, 0, 1);
    const bool small = options.width == 128;
    bool updates_done{};
    for (const std::string field : {"horizontal", "diagonal", "l1", "l2", "adjacent", "extreme"}) {
        if (options.field != "all" && options.field != field) {
            continue;
        }
        for (const bool dual : {false, true}) {
            if ((options.layers == "single" && dual) || (options.layers == "double" && !dual)) {
                continue;
            }
            const auto name = "linear-cost-" + field + (dual ? "-double" : "-single");
            const auto scenario = log.begin(name, "scenario_total");
            const auto prepare = log.begin(name, "cpu_source_prepare");
            Fixture fixture(renderer, platform, field == "l1" ? 1.5F : 1.0F);
            auto effect = linear_cost_source(field, options, dual, fixture.metrics.display_scale);
            const auto id = fixture.effects.add(effect);
            log.record(name, "cpu_source_prepare", prepare);
            linear_cost_frame(fixture, effect, name, log, small);
            if (small && !updates_done) {
                updates_done = true;
                linear_cost_frame(fixture, effect, name + "-idle", log, true, true);
                auto changed = log.begin(name, "cpu_paint_update_prepare");
                effect.material.opacity = 0.75F;
                require(fixture.effects.update_material(id, effect.material), "linear-cost paint-only update");
                log.record(name, "cpu_paint_update_prepare", changed);
                linear_cost_frame(fixture, effect, name + "-paint-update", log, true);
                changed = log.begin(name, "cpu_metrics_update_prepare");
                fixture.metrics.display_scale *= 1.25F;
                log.record(name, "cpu_metrics_update_prepare", changed);
                linear_cost_frame(fixture, effect, name + "-metrics-update", log, true, false, true);
            }
            log.record(name, "scenario_total_including_prepare_oracle_io", scenario);
        }
    }
    log.record("suite", "suite_total_including_host_cold_prepare_oracle_io", suite_started);
}

void raw_affine_linear(SdlSceneRenderer& renderer, PlatformState& platform) {
    const EffectGradientRamp ramp{
        {0, Color{1, 0, 0}}, {1.0F / 32, Color{1, 0, 0}}, {1.0F / 32, Color{0, 0, 1}}, {1, Color{0, 0, 1}}};
    const EffectLinearDomain a{{-16777211, -16777210}, {-16777203, -16777218}};
    const EffectLinearDomain b{{-16777210, -16777210}, {-16777202, -16777218}};
    {
        Fixture f(renderer, platform, 1.5F);
        auto effect = fill();
        effect.material.paint = EffectPaint{{ramp, a}, std::nullopt};
        const auto id = f.effects.add(effect);
        const auto frame_a = f.read("raw-affine-dpi-A-blue");
        f.encloses(0, 16, 16);
        const auto expected_a = source_linear_pixel(effect, f.metrics.display_scale, {16.5F, 16.5F});
        require(expected_a == std::array<float, 4>{0, 0, 1, 1}, "independent exact source A must be blue");
        pixel(frame_a, 16, 16, expected_a, "source logical A blue", 0);
        const auto base_a = f.resources.effects().instances()[0];
        const auto tables_a = f.resources.effects().paint_tables();
        effect.material.paint = EffectPaint{{ramp, b}, std::nullopt};
        require(f.effects.update_material(id, effect.material), "source B material update");
        const auto frame_b = f.read("raw-affine-dpi-B-red");
        f.encloses(0, 16, 16);
        const auto expected_b = source_linear_pixel(effect, f.metrics.display_scale, {16.5F, 16.5F});
        require(expected_b == std::array<float, 4>{1, 0, 0, 1}, "independent exact source B must be red");
        pixel(frame_b, 16, 16, expected_b, "source logical B red", 0);
        const auto& tables_b = f.resources.effects().paint_tables();
        require(base_a == f.resources.effects().instances()[0] && tables_a.links == tables_b.links &&
                    tables_a.stops == tables_b.stops && tables_a.fields[0] != tables_b.fields[0] &&
                    tables_a.fields[0].affine_bits == tables_b.fields[0].affine_bits,
                "actual projection must distinguish only the raw source domain in the DPI collision pair");
        require(sizeof(tables_b.fields[0]) == 48 && tables_b.fields[0].affine_bits[2] == 0x3fc00000,
                "actual raw affine field48 or scale bits differ");
        // Change only actual GPU field bytes back to A. The CPU source remains B;
        // overwrite the borrowed upload input before finish to check ownership.
        const auto binding = f.resources.effects().draw_binding(0, 1);
        auto raw = tables_a.fields[0];
        require(renderer.begin_upload_batch() &&
                    renderer.upload_effect_paint_buffer(EffectPaintBufferKind::field, binding.field, 0,
                                                        std::as_bytes(std::span{&raw, 1})),
                "raw field-only source ownership upload");
        std::ranges::fill(std::as_writable_bytes(std::span{&raw, 1}), std::byte{0xcd});
        require(renderer.finish_upload_batch(), "raw field-only ownership finish");
        SceneRgbaReadback owned;
        require(renderer.read_frame_rgba(owned), "raw field-only owned GPU readback");
        pixel(owned, 16, 16, expected_a, "shader consumed owned raw field48 bits", 0);
    }
    {
        Fixture f(renderer, platform);
        auto effect = fill({33554432, 33554432, 48, 48});
        effect.geometry.translation = {-33554432, -33554432};
        effect.material.paint = EffectPaint{{EffectGradientRamp{{-0.125F, Color{1, 0, 0}}, {0, Color{0, 0, 1}}},
                                             EffectLinearDomain{{33554432, 33554432}, {33554440, 33554424}}},
                                            std::nullopt};
        static_cast<void>(f.effects.add(effect));
        const auto frame = f.read("raw-affine-translation-rounding");
        f.encloses(0, 16, 17);
        const auto expected = source_linear_pixel(effect, 1, {16.5F, 17.5F});
        require(expected == std::array<float, 4>{0.5F, 0, 0.5F, 1}, "exact affine translation must yield t=-1/16");
        pixel(frame, 16, 17, expected, "raw translation exact affine purple");
    }
    for (const float scale : {0.25F, std::ldexp(1.0F, -60)}) {
        Fixture f(renderer, platform, scale);
        const auto tiny = std::numeric_limits<float>::denorm_min();
        auto effect = fill({8 / scale, 8 / scale, 40 / scale, 32 / scale});
        effect.material.paint = EffectPaint{{EffectGradientRamp{{0, Color{1, 0, 0, 0}}, {1, Color{0, 0, 1}}},
                                             EffectLinearDomain{{-0.0F, tiny}, {32 / scale, tiny}}},
                                            std::nullopt};
        static_cast<void>(f.effects.add(effect));
        const auto frame = f.read(scale == 0.25F ? "raw-affine-scale-quarter" : "raw-affine-scale-small");
        f.encloses(0, 16, 16);
        require(f.resources.effects().paint_tables().fields[0].raw_domain_bits[0] == 0x80000000 &&
                    f.resources.effects().paint_tables().fields[0].raw_domain_bits[1] == 1,
                "raw signed zero/subnormal domain bits lost before GPU");
        pixel(frame, 16, 16, source_linear_pixel(effect, scale, {16.5F, 16.5F}), "small scale exact affine");
        pixel(frame, 16, 16, {0, 0, 33.0F / 64, 33.0F / 64}, "independent small scale fraction 33/64");
    }
    {
        Fixture f(renderer, platform, 1.5F);
        auto effect = fill();
        const EffectGradientRamp foreground{{0, Color{1, 0, 0, 0}},
                                            {1.0F / 32, Color{1, 0, 0, 0}},
                                            {1.0F / 32, Color{0, 0, 1, 0.5F}},
                                            {1, Color{0, 0, 1, 0.5F}}};
        const EffectGradientRamp underlay{{0, Color{0, 1, 0, 0.5F}},
                                          {1.0F / 32, Color{0, 1, 0, 0.5F}},
                                          {1.0F / 32, Color{1, 1, 0, 0.5F}},
                                          {1, Color{1, 1, 0, 0.5F}}};
        effect.material.paint = EffectPaint{{foreground, a}, EffectPaintField{underlay, b}};
        effect.material.opacity = 0.5F;
        const auto id = f.effects.add(effect);
        const auto frame = f.read("raw-affine-dual-linear");
        const auto expected = source_linear_pixel(effect, 1.5F, {16.5F, 16.5F});
        require(expected == std::array<float, 4>{0, 0.125F, 0.25F, 0.375F}, "independent dual source-over expected");
        pixel(frame, 16, 16, expected, "raw distinct dual fields opacity once");
        effect.geometry.shape.rect.x = 11;
        effect.geometry.ancestor_clip = EffectClip{1, {11, 8, 10, 32}};
        require(f.effects.update_geometry(id, effect.geometry), "raw dual coverage/hard clip update");
        const auto edge = f.read("raw-affine-dual-coverage-clip");
        f.encloses(0, 16, 16);
        pixel(edge, 16, 16, source_linear_pixel(effect, 1.5F, {16.5F, 16.5F}, 0.5F),
              "raw dual edge coverage/opacity once");
        pixel(edge, 15, 16, {}, "raw dual hard clip exact zero", 0);
    }
    for (const float axis : {1e-30F, 1e30F}) {
        Fixture f(renderer, platform);
        auto effect = fill();
        effect.material.paint = EffectPaint{{ramp, EffectLinearDomain{{0, 0}, {axis, 0}}}, std::nullopt};
        effect.material.visible = false;
        static_cast<void>(f.effects.add(effect));
        const auto before = renderer.counters();
        bool rejected{};
        try {
            static_cast<void>(f.sync());
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        const auto after = renderer.counters();
        require(rejected && after.upload_submissions == before.upload_submissions &&
                    after.buffer_transfer_creations == before.buffer_transfer_creations &&
                    after.buffer_upload_regions == before.buffer_upload_regions,
                "raw affine retained admission must reject under/overflow even for a hidden field");
    }
}

void old_solid_cold(SdlSceneRenderer& renderer, PlatformState& platform) {
    Fixture f(renderer, platform);
    const auto id = f.effects.add(fill());
    const auto frame = f.read("old-solid-cold");
    pixel(frame, 16, 16, {1, 1, 1, 1}, "old solid");
    require(renderer.counters().effect_paint_pipeline_creations == 0, "Cold solid created a paint pipeline");
    const auto binding = f.resources.effects().draw_binding(0, 1);
    require(!binding.link && !binding.field && !binding.stop &&
                f.resources.effects().paint_capacities() == std::array<std::uint32_t, 3>{},
            "Cold solid allocated paint tables");
    require(f.resources.effects().instances()[0] == pack_rounded_effect_instance(f.effects.at(id), f.metrics),
            "Cold legacy seven float4 bytes changed");
    auto material = f.effects.at(id).material;
    material.color = Color{0, 1, 0};
    require(f.effects.update_material(id, material), "legacy material update");
    static_cast<void>(f.read("old-solid-partial"));
    require(f.resources.effects().counters().partial_uploads > 0, "Legacy partial upload path disappeared");
}

void paint_samples(SdlSceneRenderer& renderer, PlatformState& platform) {
    Fixture f(renderer, platform);
    auto effect = fill();
    effect.material.paint = EffectPaint{{EffectGradientRamp{{0, Color{1, 0, 0, 0}}, {1, Color{0, 0, 1, 1}}},
                                         EffectLinearDomain{{8.5F, 16.5F}, {24.5F, 16.5F}}},
                                        std::nullopt};
    const auto id = f.effects.add(effect);
    auto frame = f.read("transparent-linear");
    pixel(frame, 16, 16, {0, 0, 0.5F, 0.5F}, "premultiplied ramp midpoint");
    effect.material.paint = EffectPaint{constant(Color{1, 0, 0, 0.5F}), constant(Color{0, 0, 1, 0.5F})};
    effect.material.opacity = 0.5F;
    require(f.effects.update_material(id, effect.material), "dual update");
    frame = f.read("dual-alpha");
    pixel(frame, 16, 16, {0.25F, 0, 0.125F, 0.375F}, "dual sourceover alpha must be .375, not .4375");
    effect.geometry.shape.rect.x = 8.5F;
    require(f.effects.update_geometry(id, effect.geometry), "coverage edge update");
    frame = f.read("dual-coverage");
    pixel(frame, 8, 16, {0.125F, 0, 0.0625F, 0.1875F}, "coverage/material/ancestor applied once");
    effect.material.opacity = 0;
    require(f.effects.update_material(id, effect.material), "zero opacity update");
    pixel(f.read("opacity-zero"), 16, 16, {}, "opacity zero");
    effect.material.opacity = 0.5F;
    require(f.effects.update_material(id, effect.material), "opacity restore");
    pixel(f.read("opacity-restored"), 16, 16, {0.25F, 0, 0.125F, 0.375F}, "opacity restore");
    effect.geometry.shape.rect = {8, 8, 40, 32};
    effect.material.opacity = 1;
    effect.material.paint = EffectPaint{
        {EffectGradientRamp{{-1, Color{1, 0, 0}}, {0.5F, Color{0, 1, 0}}, {0.5F, Color{0, 0, 1}}, {2, Color{1, 1, 1}}},
         EffectLinearDomain{{8.5F, 16.5F}, {24.5F, 16.5F}}},
        std::nullopt};
    require(f.effects.update_geometry(id, effect.geometry), "reset bounds");
    require(f.effects.update_material(id, effect.material), "duplicate ramp update");
    frame = f.read("duplicate-right-continuous");
    pixel(frame, 16, 16, {0, 0, 1, 1}, "duplicate right continuity");
    f.reference(frame, 9, 16, "domain outside [0,1]");
    effect.material.paint =
        EffectPaint{{EffectGradientRamp{{0.5F, Color{1, 0, 0}}, {0.5F, Color{0, 1, 0}}, {0.5F, Color{0, 0, 1}}},
                     EffectLinearDomain{{8.5F, 16.5F}, {24.5F, 16.5F}}},
                    std::nullopt};
    require(f.effects.update_material(id, effect.material), "equal ramp");
    frame = f.read("equal-stops");
    pixel(frame, 8, 16, {1, 0, 0, 1}, "first endpoint hold");
    pixel(frame, 16, 16, {0, 0, 1, 1}, "all equal last duplicate");
    pixel(frame, 30, 16, {0, 0, 1, 1}, "last endpoint hold");
    std::vector<EffectGradientStop> many;
    for (int index = 0; index <= 256; ++index) {
        many.push_back({float(index) / 256, index < 192 ? Color{1, 0, 0} : Color{0, 1, 0}});
    }
    effect.material.paint =
        EffectPaint{{EffectGradientRamp{many}, EffectLinearDomain{{8.5F, 16.5F}, {24.5F, 16.5F}}}, std::nullopt};
    require(f.effects.update_material(id, effect.material), "257 stops");
    pixel(f.read("many-stops"), 22, 16, {0, 1, 0, 1}, "257 stop late span");
    effect.material.paint = EffectPaint{{EffectGradientRamp{{0, Color{1, 0, 0, 0.5F}}, {1, Color{0, 1, 0, 0.5F}}},
                                         EffectConicDomain{{24.5F, 24.5F}, 0, 2 * std::numbers::pi_v<float>}},
                                        constant(Color{0, 0, 1, 0.5F})};
    require(f.effects.update_material(id, effect.material), "conic over linear");
    frame = f.read("conic-over-linear");
    pixel(frame, 24, 24, {0.5F, 0, 0.25F, 0.75F}, "conic center t0");
    for (const auto point :
         std::array{std::array{32, 24}, std::array{24, 32}, std::array{16, 24}, std::array{24, 16}}) {
        f.reference(frame, point[0], point[1], "conic direction");
    }
    effect.material.paint->foreground = {effect.material.paint->foreground.ramp(),
                                         EffectConicDomain{{24.5F, 24.5F}, 0.7F, 1.3F}};
    require(f.effects.update_material(id, effect.material), "conic rotated partial");
    frame = f.read("conic-start-sweep");
    for (const auto point :
         std::array{std::array{32, 24}, std::array{24, 32}, std::array{16, 24}, std::array{24, 16}}) {
        f.reference(frame, point[0], point[1], "conic nonzero start/sweep");
    }
}

void cancellation_and_stop_boundaries(SdlSceneRenderer& renderer, PlatformState& platform) {
    Fixture f(renderer, platform);
    auto effect = fill();
    effect.material.paint = EffectPaint{{EffectGradientRamp{{-0.125F, Color{1, 0, 0}}, {0, Color{0, 0, 1}}},
                                         EffectLinearDomain{{-100000000, -100000000}, {-99999992, -100000008}}},
                                        std::nullopt};
    const auto id = f.effects.add(effect);
    // Independently: axis=(8,-8), N=-8, D=128, t=-1/16.
    // The framebuffer is premultiplied; this opaque midpoint is unchanged.
    const auto linear_frame = f.read("linear-cancel-original-factors");
    f.encloses(0, 16, 17);
    pixel(linear_frame, 16, 17, {0.5F, 0, 0.5F, 1}, "exact Linear cancellation");
    effect.material.paint = EffectPaint{
        {EffectGradientRamp{
             {-0.125F, Color{1, 0, 0}}, {-0.0625F, Color{1, 0, 0}}, {-0.0625F, Color{0, 0, 1}}, {0, Color{0, 0, 1}}},
         EffectLinearDomain{{-100000000, -100000000}, {-99999992, -100000008}}},
        std::nullopt};
    require(f.effects.update_material(id, effect.material), "Linear duplicate cancellation update");
    pixel(f.read("linear-exact-duplicate"), 16, 17, {0, 0, 1, 1}, "exact duplicate remains right continuous");
    const float before = std::nextafter(-0.0625F, -std::numeric_limits<float>::infinity());
    const float after = std::nextafter(-0.0625F, std::numeric_limits<float>::infinity());
    effect.material.paint = EffectPaint{{EffectGradientRamp{{before, Color{1, 0, 0}}, {after, Color{0, 0, 1}}},
                                         EffectLinearDomain{{-100000000, -100000000}, {-99999992, -100000008}}},
                                        std::nullopt};
    require(f.effects.update_material(id, effect.material), "Linear adjacent stops update");
    // At this power-of-two boundary the lower spacing is twice the upper.
    pixel(f.read("linear-adjacent-fraction"), 16, 17, {1.0F / 3, 0, 2.0F / 3, 1},
          "adjacent-stop fraction uses the same exact N-stop*D");
    const float boundary = std::nextafter(0.25F, 0.0F);
    effect.material.paint = EffectPaint{
        {EffectGradientRamp{
             {0, Color{1, 0, 0}}, {boundary, Color{1, 0, 0}}, {boundary, Color{0, 0, 1}}, {1, Color{0, 0, 1}}},
         EffectConicDomain{{16.5F, 16.5F}, 1e-7F, 2 * std::numbers::pi_v<float>}},
        std::nullopt};
    require(f.effects.update_material(id, effect.material), "Conic crossing fixture update");
    // (pi/2-float(1e-7))/(2*pi) < nextdown(.25): red, independent
    // of the CPU packed sampler and of shader atan2 implementation accuracy.
    pixel(f.read("conic-duplicate-crossing"), 16, 17, {1, 0, 0, 1}, "Conic duplicate crossing");
    pixel(f.read("conic-center-zero"), 16, 16, {1, 0, 0, 1}, "Conic center maps to zero");
    effect.geometry.shape.rect = {0, 0, 40, 32};
    effect.material.paint =
        EffectPaint{{EffectGradientRamp{{0, Color{1, 0, 0}}, {1, Color{0, 0, 1}}},
                     EffectConicDomain{
                         {1e8F, std::nextafter(0.5F, 0.0F)}, std::numbers::pi_v<float>, 2 * std::numbers::pi_v<float>}},
                    std::nullopt};
    require(std::get<EffectConicDomain>(effect.material.paint->foreground.domain()).start_radians ==
                -std::numbers::pi_v<float>,
            "true tau fixture requires the canonical negative-pi start");
    require(f.effects.update_geometry(id, effect.geometry) && f.effects.update_material(id, effect.material),
            "true tau wrap update");
    // Positive dy puts atan2 just below true pi. The canonical start is -pi_f,
    // so angle-start exceeds true tau by pi_f-pi (less than 1e-6).
    // True-tau wrapping therefore yields a coordinate near zero: red.
    pixel(f.read("conic-true-tau-wrap"), 0, 0, {1, 0, 0, 1}, "Conic true tau full-sentinel wrap");
}

void extreme_fields(SdlSceneRenderer& renderer, PlatformState& platform) {
    Fixture f(renderer, platform);
    auto effect = fill();
    const float maximum = std::numeric_limits<float>::max();
    const float tiny = std::numeric_limits<float>::denorm_min();
    const auto ramp = [](float first, float last) {
        return EffectGradientRamp{{first, Color{1, 0, 0}}, {last, Color{0, 0, 1}}};
    };
    const std::array fields{
        EffectPaintField{ramp(-maximum, maximum), EffectLinearDomain{{16.5F, 16.5F}, {17.5F, 16.5F}}},
        EffectPaintField{ramp(-tiny, tiny), EffectLinearDomain{{16.5F, 16.5F}, {17.5F, 16.5F}}},
        EffectPaintField{
            ramp(0, std::ldexp(1.0F, 24)),
            EffectLinearDomain{{-std::ldexp(1.0F, 80), 0}, {-std::ldexp(1.0F, 80) + std::ldexp(1.0F, 57), 0}}},
        EffectPaintField{ramp(0, 33), EffectLinearDomain{{0, -3.0e38F}, {1, -3.0e38F}}},
        EffectPaintField{ramp(0, 3.3e21F), EffectLinearDomain{{0, 0}, {1.0e-20F, 0}}},
        EffectPaintField{ramp(0, 2.0F), EffectConicDomain{{16.5F, 16.5F}, 0, tiny}},
        EffectPaintField{ramp(0, 1.0e26F), EffectConicDomain{{-1.0e20F, 15.5F}, 0, tiny}},
        EffectPaintField{ramp(0, 2.0F), EffectConicDomain{{-1.0e38F, 15.5F}, 0, 1.0e-38F}}};
    effect.material.paint = EffectPaint{fields[0], std::nullopt};
    const auto id = f.effects.add(effect);
    for (std::size_t index = 0; index < fields.size(); ++index) {
        effect.material.paint = EffectPaint{fields[index], std::nullopt};
        static_cast<void>(f.effects.update_material(id, effect.material));
        const auto frame = f.read("extreme-" + std::to_string(index));
        f.reference(frame, 16, 16, "stable extreme field " + std::to_string(index));
        if (index < 5) {
            pixel(frame, 16, 16, {0.5F, 0, 0.5F, 1}, "independent extreme midpoint");
        }
    }
}

double independent_butt_alpha(runtime::Point point, const RoundedEffectInstance& value) {
    const auto& geometry = value.geometry;
    auto rect = geometry.shape.rect;
    rect.x += geometry.translation.x;
    rect.y += geometry.translation.y;
    const double radius = double(rect.width) / 2;
    const double half_width = double(geometry.outline_width) / 2;
    const double qx = double(point.x) - rect.x - radius;
    const double qy = double(point.y) - rect.y - radius;
    const auto& path = std::get<RoundedArcPath>(geometry.path);
    const double start = path.start_radians;
    const double end = start + double(path.sweep_radians);
    const double ax = std::cos(start);
    const double ay = std::sin(start);
    const double bx = std::cos(end);
    const double by = std::sin(end);
    const bool angular = ax * qy - ay * qx >= 0 && qx * by - qy * bx >= 0;
    const double rho = std::hypot(qx, qy);
    const double dominant = std::max(std::abs(qx), std::abs(qy));
    const double minor = std::min(std::abs(qx), std::abs(qy));
    const double delta = ((dominant - radius) * (dominant + radius) + minor * minor) / (rho + radius);
    const auto radial_edge = [&](double x, double y) {
        const double along = qx * x + qy * y;
        const double tangent = qy * x - qx * y;
        return std::hypot(along - std::clamp(along, radius - half_width, radius + half_width), tangent);
    };
    double nearest = std::min(radial_edge(ax, ay), radial_edge(bx, by));
    if (angular) {
        nearest = std::min(nearest, std::min(std::abs(delta - half_width), std::abs(delta + half_width)));
    }
    const bool inside = angular && std::abs(delta) <= half_width;
    const double distance = inside ? -nearest : nearest;
    const double t = std::clamp(distance + 0.5, 0.0, 1.0);
    return 1 - t * t * (3 - 2 * t);
}

void caps(SdlSceneRenderer& renderer, PlatformState& platform) {
    // Independent endpoint expectations first, then the wider packed-reference
    // matrix. The CPU reference shares A's cap math and is not independent proof.
    for (const auto cap : {ArcCap::Butt, ArcCap::Square}) {
        for (const float offset : {-0.25F, 0.0F, 0.25F}) {
            Fixture f(renderer, platform);
            const auto arc = make_arc_effect({30.5F, 30.5F - offset}, 10, 4, 0, 90, cap, Color{1, 0, 0});
            static_cast<void>(f.effects.add(arc));
            static_cast<void>(f.effects.add(fill({90, 8, 12, 12})));
            const auto frame = f.read("cap-endpoint-" + std::to_string(int(cap)) + "-" + std::to_string(offset));
            const float alpha = cap == ArcCap::Square ? 1.0F : (offset < 0 ? 0.15625F : offset > 0 ? 0.84375F : 0.5F);
            pixel(frame, 40, 30, {alpha, 0, 0, alpha}, "independent cap endpoint");
            pixel(frame, 94, 12, {1, 1, 1, 1}, "cap-only pipeline mixed with legacy solid");
            const auto binding = f.resources.effects().draw_binding(0, 1);
            require(binding.field_count == 1 && binding.stop_count == 1 && binding.field && binding.stop,
                    "cap-only must bind two unread dummy storage records");
            require(binding.cap_geometry && f.resources.effects().cap_geometry().size() == 2,
                    "cap-only draw omitted its integer instance stream");
        }
    }
    for (const bool full : {false, true}) {
        Fixture f(renderer, platform);
        auto arc = make_arc_effect({30.5F, 30.5F}, 10, 4, 0, 360, ArcCap::Butt, Color{1, 0, 0});
        if (!full) {
            std::get<RoundedArcPath>(arc.geometry.path).sweep_radians =
                std::nextafter(2 * std::numbers::pi_v<float>, 0.0F);
        }
        static_cast<void>(f.effects.add(arc));
        const auto frame = f.read(full ? "adjacent-full" : "adjacent-near-full");
        const float alpha = full ? 1.0F : 0.5F;
        pixel(frame, 40, 30, {alpha, 0, 0, alpha}, "adjacent float sweep must not become full-turn");
    }
    for (const float sweep : {90.0F, 360.0F}) {
        Fixture f(renderer, platform);
        static_cast<void>(
            f.effects.add(make_arc_effect({30.5F, 30.5F}, 1, 20, 0, sweep, ArcCap::Square, Color{1, 0, 0, 0.5F})));
        const auto frame = f.read("thick-center-" + std::to_string(sweep));
        pixel(frame, 30, 30, {0.5F, 0, 0, 0.5F}, "thick strip crosses center once");
        f.reference(frame, 24, 24, "negative radial sector");
    }
    for (const auto cap : {ArcCap::Butt, ArcCap::Square}) {
        Fixture f(renderer, platform);
        static_cast<void>(f.effects.add(make_arc_effect({30.5F, 30.5F}, 10, 4, 5, 350, cap, Color{1, 0, 0, 0.5F})));
        const auto frame = f.read("near-full-gap-" + std::to_string(int(cap)));
        pixel(frame, 40, 30, cap == ArcCap::Square ? std::array{0.5F, 0.0F, 0.0F, 0.5F} : std::array<float, 4>{},
              "near-full cap intersection/gap");
    }
    std::size_t sample_index{};
    for (const auto cap : {ArcCap::Butt, ArcCap::Square}) {
        for (const float width : {0.001F, 4.0F, 19.999998F, 20.0F, 20.000002F, 32.0F}) {
            for (const float sweep :
                 {0.01F, 90.0F, 179.99998F, 270.0F, 350.0F, 359.0F, std::nextafter(360.0F, 0.0F), 360.0F}) {
                Fixture f(renderer, platform);
                auto arc = make_arc_effect({34.5F, 34.5F}, 10, width, 5, sweep, cap, Color{0, 1, 0, 0.5F});
                if (sweep == std::nextafter(360.0F, 0.0F)) {
                    auto& path = std::get<RoundedArcPath>(arc.geometry.path);
                    path.sweep_radians = std::nextafter(2 * std::numbers::pi_v<float>, 0.0F);
                }
                static_cast<void>(f.effects.add(arc));
                const auto label = "cap-matrix-" + std::to_string(sample_index++);
                const auto frame = f.read(label);
                for (const auto point :
                     std::array{std::array{34, 34}, std::array{44, 34}, std::array{44, 33}, std::array{34, 44},
                                std::array{24, 34}, std::array{34, 24}, std::array{41, 41}, std::array{48, 35}}) {
                    f.reference(frame, point[0], point[1], label);
                }
            }
        }
    }
    Fixture corner(renderer, platform);
    static_cast<void>(corner.effects.add(
        make_arc_effect({30.5F, 30.5F - 0.70710678F}, 1, 20, 45, 90, ArcCap::Square, Color{1, 0, 0, 0.5F})));
    const auto frame = corner.read("square-outer-bounds");
    pixel(frame, 44, 30, {0.5F, 0, 0, 0.5F}, "square corner beyond legacy radius+halfwidth bounds");
    for (int x = 41; x <= 46; ++x) {
        corner.reference(frame, x, 30, "square outer corner AA");
    }
    std::size_t large_index{};
    for (const float radius :
         {std::nextafter(1.0e8F, 0.0F), 1.0e8F, std::nextafter(1.0e8F, std::numeric_limits<float>::infinity())}) {
        for (const float width :
             {std::nextafter(4.0F, 0.0F), 4.0F, std::nextafter(4.0F, std::numeric_limits<float>::infinity())}) {
            Fixture large(renderer, platform);
            const auto large_arc =
                make_arc_effect({32.0F - radius, 0}, radius, width, 0, 90, ArcCap::Butt, Color{1, 0, 0});
            static_cast<void>(large.effects.add(large_arc));
            const auto large_frame = large.read("large-radius-thin-stroke-" + std::to_string(large_index++));
            const auto base = large.resources.effects().instances()[0];
            const auto right = (double(base.clip_rect[0]) + base.clip_rect[2] + 1) * large_frame.width / 2;
            require(right > 33.5, "large-radius draw envelope omitted outer visible columns");
            for (int y = 30; y <= 32; ++y) {
                pixel(large_frame, 31, y, {1, 0, 0, 1}, "large-radius thin stroke must not collapse");
                pixel(large_frame, 30, y, {1, 0, 0, 1}, "adjacent inside pixel");
                pixel(large_frame, 29, y, {}, "adjacent outside AA endpoint");
                pixel(large_frame, 32, y, {1, 0, 0, 1}, "outer neighbor previously lost by float LTWH");
                pixel(large_frame, 33, y, {1, 0, 0, 1}, "outer AA interior previously culled");
                pixel(large_frame, 34, y, {}, "outer AA endpoint");
                large.reference(large_frame, 31, y, "A double large-radius reference");
                large.reference(large_frame, 30, y, "A double adjacent inside reference");
                large.reference(large_frame, 29, y, "A double adjacent outside reference");
            }
        }
    }

    for (const auto exponent : {80, 126}) {
        Fixture f(renderer, platform);
        const auto radius = std::ldexp(1.0F, exponent);
        static_cast<void>(f.effects.add(make_arc_effect({-radius, 0}, radius, 4, 0, 90, ArcCap::Butt, Color{1, 0, 0})));
        const auto frame = f.read("cap-exponent-" + std::to_string(exponent));
        // Independent quarter-annulus result; A's normalized double r+/-h
        // also collapses here, so the shared scalar reference is not an oracle.
        pixel(frame, 0, 31, {1, 0, 0, 1}, "large-exponent stable radial difference");
        pixel(frame, 1, 31, {1, 0, 0, 1}, "large-exponent outer AA interior");
        pixel(frame, 2, 31, {}, "large-exponent outer AA endpoint");
    }
    CapGeometryGpuV1 collision_first;
    for (int index = 0; index < 2; ++index) {
        Fixture f(renderer, platform, 1.5F);
        const auto center = index == 0 ? -8388603.0F : -8388602.0F;
        auto value = make_arc_effect({center, 0}, 8388608, 4, 0, 90, ArcCap::Butt, Color{1, 0, 0});
        value.geometry.ancestor_clip = EffectClip{1, {0, 0, 8 / 1.5F, 8 / 1.5F}};
        static_cast<void>(f.effects.add(value));
        const auto frame = f.read("cap-logical-collision-" + std::to_string(index));
        const auto cap = f.resources.effects().cap_geometry()[0];
        if (index == 0) {
            collision_first = cap;
        } else {
            require(collision_first != cap, "integer stream retained the physical bytes collision");
        }
        pixel(frame, 5, 0, index == 0 ? std::array{1.0F, 0.0F, 0.0F, 1.0F} : std::array<float, 4>{},
              "independent distinct logical inputs after DPI collision");
        pixel(frame, 8, 0, {}, "cap hard clip remains half open");
    }
    for (const auto cap : {ArcCap::Butt, ArcCap::Square}) {
        Fixture f(renderer, platform);
        auto value =
            make_arc_effect({30.5F, 30.5F}, 10, std::numeric_limits<float>::denorm_min(), 0, 90, cap, Color{1, 0, 0});
        static_cast<void>(f.effects.add(value));
        const auto frame = f.read("cap-subnormal-stroke-" + std::to_string(int(cap)));
        require(f.resources.effects().cap_geometry()[0].stroke_scale_bits[0] == 1u, "subnormal input bits were lost");
        pixel(frame, 40, 30, {0.5F, 0, 0, 0.5F}, "positive subnormal stroke is not empty before bit decode");
    }
    {
        Fixture f(renderer, platform, 0.25F);
        auto value = make_arc_effect({-0.1F, 18}, 4, 1, 0, 90, ArcCap::Butt, Color{1, 0, 0});
        value.geometry.ancestor_clip = EffectClip{1, {6, 0, 26, 32}};
        static_cast<void>(f.effects.add(value));
        const auto frame = f.read("cap-sub-unit-dpi-clip-aa");
        pixel(frame, 1, 4, {0.028F, 0, 0, 0.028F}, "one physical pixel AA survives early cull at fractional clip");
        pixel(frame, 0, 4, {}, "fractional hard clip is not enlarged by AA bounds");
    }
    const float pi = std::numbers::pi_v<float>;
    std::size_t angle_index{};
    for (const float start : {0.0F, 1.0F, pi / 4, -pi / 4, pi / 2, -pi / 2, pi, -pi}) {
        Fixture f(renderer, platform);
        const float radius = 1.0e8F;
        auto value = make_arc_effect({32 - radius * std::cos(start), 32 - radius * std::sin(start)}, radius, 4, 0, 90,
                                     ArcCap::Butt, Color{1, 0, 0});
        auto& path = std::get<RoundedArcPath>(value.geometry.path);
        path.start_radians = start;
        path.sweep_radians = std::ldexp(1.0F, -25);
        static_cast<void>(f.effects.add(value));
        const auto frame = f.read("cap-small-sweep-angle-" + std::to_string(angle_index++));
        bool visible{};
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                const auto alpha =
                    static_cast<float>(independent_butt_alpha({float(x) + 0.5F, float(y) + 0.5F}, value));
                visible = visible || alpha > 0.5F;
                pixel(frame, x, y, {alpha, 0, 0, alpha}, "independent small-sweep endpoint frame");
            }
        }
        require(visible, "small-sweep fixture lost every independently visible pixel");
    }
}

void cap_stream_transport(SdlSceneRenderer& renderer, PlatformState& platform) {
    Fixture f(renderer, platform);
    auto painted = fill({60, 8, 12, 12});
    painted.material.paint = EffectPaint{constant(Color{0, 1, 0, 0.5F}), {}};
    const auto ids = f.effects.add_batch(
        std::array{fill({90, 8, 12, 12}), make_arc_effect({30.5F, 30.5F}, 10, 4, 0, 90, ArcCap::Butt, Color{1, 0, 0}),
                   fill({90, 30, 12, 12}), painted});
    f.order.append_command({SceneDrawKind::rounded_effect, 1, 3, invalid_glyph_atlas_page});
    f.order.append_command({SceneDrawKind::rounded_effect, 0, 1, invalid_glyph_atlas_page});
    const auto initial = f.read("cap-stream-nonzero-first", false);
    pixel(initial, 40, 30, {0.5F, 0, 0, 0.5F}, "three-stream nonzero-first cap");
    pixel(initial, 64, 12, {0, 0.5F, 0, 0.5F}, "Round paint zero cap placeholder");
    const auto binding = f.resources.effects().draw_binding(1, 1);
    require(binding.base_byte_offset == 112 && binding.link_byte_offset == 16 && binding.cap_byte_offset == 32,
            "actual three-stream binding first offsets disagree");
    auto raw = f.resources.effects().cap_geometry()[1];
    raw.stroke_scale_bits[0] = std::bit_cast<std::uint32_t>(8.0F);
    require(renderer.begin_upload_batch() &&
                renderer.upload_cap_geometry_buffer(binding.cap_geometry, 32, std::as_bytes(std::span{&raw, 1})),
            "cap raw ownership upload failed");
    std::ranges::fill(std::as_writable_bytes(std::span{&raw, 1}), std::byte{0xcd});
    require(renderer.finish_upload_batch(), "cap raw ownership finish failed");
    SceneRgbaReadback owned;
    require(renderer.read_frame_rgba(owned), "cap raw ownership readback failed");
    pixel(owned, 42, 32, {1, 0, 0, 1}, "shader consumed owned cap source bytes after overwrite");
    const auto old = f.resources.attach(f.order);
    auto geometry = f.effects.at(ids[1]).geometry;
    geometry.outline_width = 12;
    require(f.effects.update_geometry(ids[1], geometry) && renderer.fail_next_upload_finish_for_test(),
            "cap retry fixture failed");
    const auto staged_before = renderer.counters().buffer_upload_regions;
    require(!f.sync() && !renderer.attach_scene(old), "five-buffer finish failure kept attachment");
    require(renderer.counters().buffer_upload_regions >= staged_before + 5,
            "finish failure must follow five actual staged buffer regions");
    SceneRgbaReadback rejected{1, 1, 4, std::vector<std::byte>(4)};
    require(!renderer.read_frame_rgba(rejected) && rejected.pixels.empty(), "cap failure returned stale RGBA");
    f.effects.clear_dirty_ranges();
    const auto before = renderer.counters();
    const auto retry = f.read("cap-five-buffer-retry");
    require(renderer.counters().buffer_upload_regions >= before.buffer_upload_regions + 5,
            "cap retry skipped one of five buffers");
    pixel(retry, 44, 32, {1, 0, 0, 1}, "cap recovery used new CPU geometry");
    const auto retry_attachment = f.resources.attach(f.order);
    geometry.outline_width = 8;
    require(f.effects.update_geometry(ids[1], geometry) && renderer.fail_next_cap_geometry_upload_for_test(),
            "arm last cap upload failure");
    const auto last_before = renderer.counters().buffer_upload_regions;
    bool last_failed{};
    try {
        last_failed = !f.sync();
    } catch (const std::exception&) {
        last_failed = true;
    }
    require(last_failed && renderer.counters().buffer_upload_regions >= last_before + 4 &&
                !renderer.attach_scene(retry_attachment),
            "last cap upload failure did not invalidate attachment");
    require(!renderer.read_frame_rgba(rejected) && rejected.pixels.empty(), "last upload failure retained RGBA");
    f.effects.clear_dirty_ranges();
    const auto last_retry_before = renderer.counters().buffer_upload_regions;
    pixel(f.read("cap-last-upload-no-dirty-retry"), 42, 32, {1, 0, 0, 1}, "last upload recovery source bytes");
    require(renderer.counters().buffer_upload_regions >= last_retry_before + 5, "last failure retry omitted buffer");
    std::array<std::byte, 32> arbitrary{};
    require(!renderer.upload_cap_geometry_buffer(binding.link, 0, arbitrary) &&
                !renderer.upload_cap_geometry_buffer(binding.cap_geometry, std::numeric_limits<std::size_t>::max(),
                                                     arbitrary),
            "cap wrong-kind/range accepted");
    auto other_host = hidden_host();
    require(bool(other_host), "cap foreign host");
    SdlSceneRenderer foreign(*other_host.state);
    require(!foreign.upload_cap_geometry_buffer(binding.cap_geometry, 0, arbitrary), "foreign cap handle accepted");
    bool off_owner{};
    std::thread other([&] { off_owner = renderer.upload_cap_geometry_buffer(binding.cap_geometry, 0, arbitrary); });
    other.join();
    require(!off_owner, "non-owner cap upload accepted");
    const auto idle = renderer.counters();
    require(f.sync() && renderer.counters().uploaded_bytes == idle.uploaded_bytes &&
                renderer.counters().buffer_transfer_maps == idle.buffer_transfer_maps &&
                renderer.counters().upload_submissions == idle.upload_submissions,
            "cap idle performed upload work");
    require(f.effects.remove(ids[1]), "cap removal failed");
    const auto restored = f.read("cap-restore-round-paint-two-streams");
    require(!f.resources.effects().draw_binding(0, 1).cap_geometry, "Round paint retained third stream");
    pixel(restored, 64, 12, {0, 0.5F, 0, 0.5F}, "Round paint after cap stream retirement");
    require(!renderer.upload_cap_geometry_buffer(binding.cap_geometry, 0, std::as_bytes(std::span{&raw, 1})),
            "retired cap tombstone accepted upload");
}

void cap_dual_alpha_and_hard_clip(SdlSceneRenderer& renderer, PlatformState& platform) {
    Fixture f(renderer, platform);
    auto cap = make_arc_effect({30.5F, 30.5F}, 10, 4, 0, 90, ArcCap::Butt, Color{1, 1, 1});
    cap.material.paint = EffectPaint{constant(Color{1, 0, 0, 0.5F}), constant(Color{0, 0, 1, 0.5F})};
    cap.material.opacity = 0.5F;
    const auto id = f.effects.add(cap);
    const auto frame = f.read("cap-dual-alpha");
    f.encloses(0, 37, 36);
    f.encloses(0, 40, 30);
    pixel(frame, 37, 36, {0.25F, 0, 0.125F, 0.375F}, "cap same-fragment dual alpha .375");
    pixel(frame, 40, 30, {0.125F, 0, 0.0625F, 0.1875F}, "cap half coverage alpha .1875");
    cap.geometry.ancestor_clip = EffectClip{1, {40.5F, 30.5F, 1, 1}};
    require(f.effects.update_geometry(id, cap.geometry), "cap exact half-open clip update");
    const auto clipped = f.read("cap-dual-hard-clip");
    pixel(clipped, 40, 30, {0.125F, 0, 0.0625F, 0.1875F}, "hard clip inclusive left/top");
    for (const auto p : std::array{std::array{39, 30}, std::array{41, 30}, std::array{40, 31}}) {
        pixel(clipped, p[0], p[1], {}, "cap hard clip outside strictly zero", 0);
    }
}

void cap_positive_subnormal_sweep(SdlSceneRenderer& renderer, PlatformState& platform) {
    Fixture f(renderer, platform);
    auto cap = make_arc_effect({30.5F, 30.5F}, 10, 4, 0, 90, ArcCap::Butt, Color{1, 0, 0});
    std::get<RoundedArcPath>(cap.geometry.path).sweep_radians = std::numeric_limits<float>::denorm_min();
    static_cast<void>(f.effects.add(cap));
    const auto frame = f.read("cap-positive-subnormal-sweep");
    f.encloses(0, 40, 30);
    require(std::bit_cast<std::uint32_t>(f.resources.effects().instances()[0].shadow_params[2]) == 1,
            "packed angle bits lost positive subnormal sweep");
    // Exactly on the included start radial edge: signed distance is zero,
    // positive sweep is nonempty, so one-pixel smooth coverage is one half.
    pixel(frame, 40, 30, {0.5F, 0, 0, 0.5F}, "GPU float-angle transport preserves denorm_min sweep");
}

void fractional_clip_and_cells(SdlSceneRenderer& renderer, PlatformState& platform) {
    std::size_t index{};
    for (const float scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
        for (const float phase : {0.0F, 0.25F, 0.5F, 0.75F}) {
            Fixture f(renderer, platform, scale);
            auto effect = make_arc_effect({24, 24}, 10, 5, 13, 315, Color{1, 1, 1});
            effect.geometry.translation = {phase / scale, (phase + 0.25F) / scale};
            effect.material.paint = EffectPaint{{EffectGradientRamp{{0, Color{1, 0, 0, 0.5F}}, {1, Color{0, 1, 0}}},
                                                 EffectConicDomain{{24, 24}, 0.3F, 5.2F}},
                                                constant(Color{0, 0, 1, 0.5F})};
            static_cast<void>(f.effects.add(effect));
            auto clipped = fill({0, 0, 20, 20});
            clipped.material.paint = EffectPaint{constant(Color{1, 0, 0, 0.5F}), std::nullopt};
            clipped.geometry.ancestor_clip = EffectClip{7, {2.5F / scale, 2.5F / scale, 4 / scale, 4 / scale}};
            static_cast<void>(f.effects.add(clipped));
            const runtime::Rect cells{50.5F / scale, 4.5F / scale, 32 / scale, 16 / scale};
            const auto source =
                sampled_color_field(cells, {}, 8, 4, [](float, float) { return Color{0.2F, 0.4F, 0.6F, 0.5F}; },
                                    {8, scene_logical_viewport(f.metrics)});
            static_cast<void>(f.effects.add_batch(source));
            // Solid ColorPicker cells and paint share one upload transaction,
            // with global storage indices surviving nonzero-first draw splits.
            f.order.append_command({SceneDrawKind::rounded_effect, 2, 16, invalid_glyph_atlas_page});
            f.order.append_command({SceneDrawKind::rounded_effect, 0, 2, invalid_glyph_atlas_page});
            f.order.append_command({SceneDrawKind::rounded_effect, 18, 16, invalid_glyph_atlas_page});
            const auto frame = f.read("fractional-" + std::to_string(index++), false);
            pixel(frame, 2, 2, {0.5F, 0, 0, 0.5F}, "left/top included");
            pixel(frame, 6, 4, {}, "right excluded", 0);
            pixel(frame, 4, 6, {}, "bottom excluded", 0);
            for (int y = 8; y <= 16; y += 4) {
                for (int x = 54; x <= 78; x += 4) {
                    pixel(frame, x, y, {0.1F, 0.2F, 0.3F, 0.5F}, "ColorPicker half-open shared cell", 2);
                }
            }
            for (int y = int(16 * scale); y <= int(33 * scale); y += 5) {
                for (int x = int(14 * scale); x <= int(36 * scale); x += 5) {
                    f.reference(frame, x, y, "fractional translation/domain/one-pixel AA");
                }
            }
        }
    }
}

void nonzero_draws_and_remap(SdlSceneRenderer& renderer, PlatformState& platform) {
    Fixture f(renderer, platform);
    std::array<RoundedEffectInstance, 4> values;
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index] = fill({8 + float(index) * 24, 8, 16, 24});
    }
    values[1].material.paint = EffectPaint{constant(Color{1, 0, 0, 0.5F}), constant(Color{0, 0, 1, 0.5F})};
    values[1].material.opacity = 0.5F;
    values[3].material.paint =
        EffectPaint{{EffectGradientRamp{{-2, Color{0, 1, 0}}, {4, Color{0, 1, 0}}}, EffectLinearDomain{{0, 0}, {1, 0}}},
                    std::nullopt};
    const auto ids = f.effects.add_batch(values);
    f.order.append_command({SceneDrawKind::rounded_effect, 1, 2, invalid_glyph_atlas_page});
    f.order.append_command({SceneDrawKind::rounded_effect, 0, 1, invalid_glyph_atlas_page});
    f.order.append_command({SceneDrawKind::rounded_effect, 3, 1, invalid_glyph_atlas_page});
    auto frame = f.read("nonzero-multiple-draws", false);
    pixel(frame, 36, 16, {0.25F, 0, 0.125F, 0.375F}, "nonzero first");
    pixel(frame, 84, 16, {0, 1, 0, 1}, "global late stop span");
    const auto ranges = f.resources.effects().draw_binding(1, 2);
    require(ranges.base_byte_offset == 112 && ranges.link_byte_offset == 16, "two stream byte offsets");
    auto material = values[1].material;
    material.visible = false;
    require(f.effects.update_material(ids[1], material), "hide remap");
    frame = f.read("hidden-remap");
    pixel(frame, 36, 16, {}, "hidden no old field");
    pixel(frame, 84, 16, {0, 1, 0, 1}, "remapped late field");
    material.visible = true;
    require(f.effects.update_material(ids[1], material), "restore remap");
    require(f.effects.remove(ids[0]), "remove first");
    const auto reused = f.effects.add(fill({8, 8, 16, 24}));
    require(reused.index == ids[0].index && reused.generation != ids[0].generation, "slot reuse identity");
    frame = f.read("slot-reuse-remap");
    pixel(frame, 36, 16, {0.25F, 0, 0.125F, 0.375F}, "restored dual field after deletion");
    auto geometry = values[3].geometry;
    geometry.translation = {1000, 0};
    require(f.effects.update_geometry(ids[3], geometry), "viewport cull");
    frame = f.read("viewport-cull");
    pixel(frame, 84, 16, {}, "viewport culled paint");
    geometry.translation = {};
    geometry.ancestor_clip = EffectClip{12, {0, 70, 1, 1}};
    require(f.effects.update_geometry(ids[3], geometry), "ancestor cull");
    frame = f.read("ancestor-cull");
    pixel(frame, 84, 16, {}, "ancestor culled paint");
    require(f.effects.update_geometry(ids[3], values[3].geometry), "geometry restore");
    pixel(f.read("culled-restore"), 84, 16, {0, 1, 0, 1}, "restored global field index");
}

void legacy_same_scene(SdlSceneRenderer& renderer, PlatformState& platform, bool active_cap = false) {
    Fixture f(renderer, platform);
    const std::array legacy{
        make_arc_effect({20.5F, 20.5F}, 10, 4, 10, 280, Color{1, 0, 0, 0.5F}),
        make_arc_effect({20.5F, 50.5F}, 0, 6, 0, 90, Color{1, 1, 0, 0.5F}),
        make_dashed_outline_effect({{40, 8, 24, 28}, 4}, 2, {4, 3, 1}, Color{0, 1, 0, 0.5F}),
        make_tooltip_fill_effect({{40, 44, 24, 20}, 3}, {TooltipArrowSide::bottom, 12, 4, 5}, Color{0, 0, 1, 0.5F}),
        make_outline_effect({{8, 70, 24, 16}, 3}, 2, 1, Color{1, 0, 1, 0.5F}),
        make_shadow_effect({{44, 74, 20, 12}, 3}, {ShadowKind::outer, {1, 1}, 4, 1, Color{0, 1, 1, 0.5F}})};
    static_cast<void>(f.effects.add_batch(legacy));
    const auto before = f.read("legacy-before");
    const std::vector<RoundedEffectGpuInstance> bytes(f.resources.effects().instances().begin(),
                                                      f.resources.effects().instances().end());
    auto painted = fill({96, 8, 20, 20});
    painted.material.paint = EffectPaint{constant(Color{1, 0, 0}), std::nullopt};
    if (active_cap) {
        painted = make_arc_effect({100.5F, 18.5F}, 8, 3, 0, 90, ArcCap::Butt, Color{1, 0, 0});
    }
    const auto extra = f.effects.add(painted);
    const auto during = f.read("legacy-during-paint");
    require(bool(f.resources.effects().draw_binding(0, 1).cap_geometry) == active_cap,
            "legacy mix did not select intended two/three-stream pipeline");
    require(std::equal(bytes.begin(), bytes.end(), f.resources.effects().instances().begin()),
            "Legacy base bytes in paint scene");
    require(f.effects.remove(extra), "remove paint for legacy restore");
    const auto after = f.read("legacy-after");
    for (int y = 0; y < 96; ++y) {
        for (int x = 0; x < 80; ++x) {
            const auto offset = std::size_t(y) * before.row_pitch + std::size_t(x) * 4;
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const int a = std::to_integer<unsigned char>(before.pixels[offset + channel]);
                const int b = std::to_integer<unsigned char>(during.pixels[offset + channel]);
                const int c = std::to_integer<unsigned char>(after.pixels[offset + channel]);
                require(std::abs(a - b) <= 1 && a == c, "Legacy ROI changed under paint pipeline");
            }
        }
    }
    pixel(before, 20, 50, {0.5F, 0.5F, 0, 0.5F}, "old Round zero-radius footprint");
    const auto binding = f.resources.effects().draw_binding(0, std::uint32_t(legacy.size()));
    require(!binding.link && !binding.field && !binding.stop, "Restore solid retained tables");
}

void failure_ownership_retry_idle(SdlSceneRenderer& renderer, PlatformState& platform) {
    Fixture f(renderer, platform);
    auto effect = fill();
    effect.material.paint = EffectPaint{constant(Color{1, 0, 0}), std::nullopt};
    const auto id = f.effects.add(effect);
    const auto cold_before = renderer.counters();
    static_cast<void>(f.read("failure-initial"));
    const auto cold_after = renderer.counters();
    std::cout << "same-scene cold upload_bytes=" << cold_after.uploaded_bytes - cold_before.uploaded_bytes
              << " regions=" << cold_after.buffer_upload_regions - cold_before.buffer_upload_regions
              << " submissions=" << cold_after.upload_submissions - cold_before.upload_submissions << '\n';
    auto old_attachment = f.resources.attach(f.order);
    for (int mode = 0; mode < 3; ++mode) {
        effect.material.paint = EffectPaint{constant(mode == 1 ? Color{0, 0, 1} : Color{0, 1, 0}), std::nullopt};
        require(f.effects.update_material(id, effect.material), "failure color update");
        const auto counters = renderer.counters();
        if (mode == 0) {
            require(renderer.fail_next_effect_field_upload_for_test(), "arm mid-upload failure");
        } else if (mode == 1) {
            require(renderer.fail_next_upload_finish_for_test(), "arm finish failure");
        } else {
            require(renderer.begin_upload_batch(), "force begin failure");
        }
        bool failed{};
        try {
            failed = !f.sync();
        } catch (const std::exception&) {
            failed = true;
        }
        require(failed, "Injected SDL failure was accepted");
        require(std::string(renderer.last_error())
                        .find(mode == 0   ? "field upload failure"
                              : mode == 1 ? "finish failure"
                                          : "owner or state") != std::string::npos,
                "wrong SDL failure stage");
        require(renderer.counters().upload_submissions == counters.upload_submissions, "Failed upload was submitted");
        if (mode == 0) {
            require(renderer.counters().buffer_transfer_maps > counters.buffer_transfer_maps,
                    "Mid-upload seam fired before base/link source was accepted");
        }
        if (mode == 1) {
            require(renderer.counters().buffer_upload_regions >= counters.buffer_upload_regions + 4,
                    "Finish seam did not follow four real staged buffer copies");
        }
        renderer.cancel_upload_batch();
        require(!renderer.attach_scene(old_attachment), "Failed transaction left attachment ready");
        SceneRgbaReadback rejected{1, 1, 4, std::vector<std::byte>(4)};
        require(!renderer.read_frame_rgba(rejected) && rejected.pixels.empty(), "Failure returned stale RGBA output");
        // No new CPU mutation/dirty input may be required for a full retry.
        f.effects.clear_dirty_ranges();
        const auto before = renderer.counters();
        const auto recovered = f.read("failure-recovered-" + std::to_string(mode));
        pixel(recovered, 16, 16, mode == 1 ? std::array{0.0F, 0.0F, 1.0F, 1.0F} : std::array{0.0F, 1.0F, 0.0F, 1.0F},
              "real readback after no-dirty retry");
        require(renderer.counters().buffer_upload_regions >= before.buffer_upload_regions + 4, "Retry omitted a table");
        std::cout << "same-scene updated-retry mode=" << mode
                  << " upload_bytes=" << renderer.counters().uploaded_bytes - before.uploaded_bytes
                  << " regions=" << renderer.counters().buffer_upload_regions - before.buffer_upload_regions
                  << " submissions=" << renderer.counters().upload_submissions - before.upload_submissions << '\n';
        old_attachment = f.resources.attach(f.order);
    }
    const auto binding = f.resources.effects().draw_binding(0, 1);
    require(!renderer.upload_effect_paint_buffer(EffectPaintBufferKind::link, binding.field, 0,
                                                 std::as_bytes(std::span(f.resources.effects().paint_tables().links))),
            "Wrong-kind upload accepted");
    renderer.release_effect_paint_buffer(EffectPaintBufferKind::link, binding.field);
    require(!renderer.create_effect_paint_buffer(EffectPaintBufferKind::link, 0) &&
                !renderer.create_effect_paint_buffer(EffectPaintBufferKind::field, 31),
            "Invalid paint buffer stride/size accepted");
    std::array<std::byte, 16> arbitrary{};
    require(!renderer.upload_effect_paint_buffer(EffectPaintBufferKind::link, binding.link,
                                                 std::numeric_limits<std::size_t>::max(), arbitrary),
            "Offset overflow upload accepted");
    require(!renderer.upload_effect_paint_buffer(
                EffectPaintBufferKind::field, binding.field,
                std::size_t(f.resources.effects().paint_capacities()[1]) * effect_paint_field_stride - 1, arbitrary),
            "Length overflow accepted");
    auto second = hidden_host();
    require(bool(second), "foreign hidden host");
    SdlSceneRenderer foreign(*second.state);
    require(!foreign.upload_effect_paint_buffer(EffectPaintBufferKind::link, binding.link, 0, arbitrary),
            "Foreign owner handle accepted");
    bool off_thread{};
    std::thread other([&] {
        off_thread = renderer.upload_effect_paint_buffer(EffectPaintBufferKind::link, binding.link, 0, arbitrary);
    });
    other.join();
    require(!off_thread, "Off-owner thread upload accepted");
    // Upload actual link/field/stop bytes, overwrite every caller buffer before
    // finish, then consume them through the real shader/readback.
    auto tables = f.resources.effects().paint_tables();
    for (auto& stop : tables.stops) {
        stop.premultiplied_rgba = {0, 0, 1, 1};
    }
    require(renderer.begin_upload_batch(), "ownership begin");
    require(renderer.upload_effect_paint_buffer(EffectPaintBufferKind::link, binding.link, 0,
                                                std::as_bytes(std::span(tables.links))),
            "link source ownership");
    require(renderer.upload_effect_paint_buffer(EffectPaintBufferKind::field, binding.field, 0,
                                                std::as_bytes(std::span(tables.fields))),
            "field source ownership");
    require(renderer.upload_effect_paint_buffer(EffectPaintBufferKind::stop, binding.stop, 0,
                                                std::as_bytes(std::span(tables.stops))),
            "stop source ownership");
    std::ranges::fill(std::as_writable_bytes(std::span(tables.links)), std::byte{0xcd});
    std::ranges::fill(std::as_writable_bytes(std::span(tables.fields)), std::byte{0xcd});
    std::ranges::fill(std::as_writable_bytes(std::span(tables.stops)), std::byte{0xcd});
    tables = {};
    require(renderer.finish_upload_batch(), "ownership finish");
    SceneRgbaReadback owned;
    require(renderer.read_frame_rgba(owned), "ownership GPU readback");
    pixel(owned, 16, 16, {0, 0, 1, 1}, "owned upload bytes consumed");
    const auto idle_before = renderer.counters();
    require(f.sync(), "idle synchronize");
    const auto idle_after = renderer.counters();
    require(idle_before.uploaded_bytes == idle_after.uploaded_bytes &&
                idle_before.buffer_upload_regions == idle_after.buffer_upload_regions &&
                idle_before.buffer_transfer_creations == idle_after.buffer_transfer_creations &&
                idle_before.buffer_transfer_maps == idle_after.buffer_transfer_maps &&
                idle_before.upload_submissions == idle_after.upload_submissions,
            "Idle scene uploaded GPU data");
    std::cout << "upload counters cold/updated/idle: bytes=" << idle_after.uploaded_bytes
              << " regions=" << idle_after.buffer_upload_regions << " submissions=" << idle_after.upload_submissions
              << " idle_delta=0\n";
    f.resources.retire();
    require(!renderer.attach_scene(old_attachment), "retired attachment accepted");
    require(!renderer.upload_effect_paint_buffer(EffectPaintBufferKind::link, binding.link, 0, arbitrary),
            "Released tombstone accepted");
    pixel(f.read("retire-rebuild"), 16, 16, {0, 1, 0, 1}, "retire rebuild from CPU source");
    check_hidden(*second.state);
}

std::string closure_identity(const std::string& shader = "rounded_effect_paint") {
    std::ifstream input(RYNUI_EFFECT_SHADER_LOCK);
    const std::string lock{std::istreambuf_iterator<char>{input}, {}};
    const auto pipeline = lock.find("\"" + shader + "\":");
    require(pipeline != std::string::npos, "Native test needs the generated schema2 shader lock");
    const auto key = lock.find("\"closure_sha256\":", pipeline);
    const auto first = lock.find('"', key + std::strlen("\"closure_sha256\":"));
    const auto last = lock.find('"', first + 1);
    require(key != std::string::npos && first != std::string::npos && last - first == 65, "Missing source closure ID");
    return lock.substr(first + 1, 64);
}

void pending_caps_reject(SdlSceneRenderer& renderer, PlatformState& platform) {
    for (int mode = 0; mode < 4; ++mode) {
        Fixture f(renderer, platform);
        auto arc = make_arc_effect({24, 24}, 10, 4, 0, 90, ArcCap::Butt, Color{1, 0, 0});
        if (mode == 1) {
            arc.material.visible = false;
        } else if (mode == 2) {
            arc.material.opacity = 0;
        } else if (mode == 3) {
            arc.geometry.translation = {1000, 0};
        }
        static_cast<void>(f.effects.add(arc));
        const auto before = renderer.counters();
        bool rejected{};
        std::string rejection;
        try {
            static_cast<void>(f.sync());
        } catch (const std::invalid_argument& error) {
            rejection = error.what();
            rejected = rejection.find("arc cap capability") != std::string::npos;
        }
        require(rejected, "Pending non-Round solver must be rejected by live preflight, mode=" + std::to_string(mode) +
                              ", rejection=" + rejection);
        require(renderer.counters().buffer_transfer_creations == before.buffer_transfer_creations &&
                    renderer.counters().upload_submissions == before.upload_submissions,
                "Unsupported cap reached GPU upload work");
    }
}

void validation_scope_exit_reject(SdlSceneRenderer& renderer, PlatformState& platform, bool cap) {
    Fixture f(renderer, platform);
    auto effect = cap ? make_arc_effect({30.5F, 30.5F}, 10, 4, 0, 90, ArcCap::Butt, Color{1, 0, 0}) : fill();
    if (!cap) {
        effect.material.paint = EffectPaint{constant(Color{1, 0, 0}), {}};
    }
    static_cast<void>(f.effects.add(effect));
    require(f.sync(), "prime live scene before validation scope exit");
    f.all();
    const auto old = f.resources.attach(f.order);
    if (cap) {
        renderer.end_pending_cap_validation_for_test();
    } else {
        renderer.end_pending_paint_validation_for_test();
    }
    const auto before = renderer.counters();
    bool rejected{};
    try {
        static_cast<void>(f.sync());
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && !renderer.attach_scene(old) &&
                renderer.counters().upload_submissions == before.upload_submissions &&
                renderer.counters().buffer_transfer_creations == before.buffer_transfer_creations,
            "scope exit did not reject the same live source before begin");
}
#if defined(RYNUI_LINEAR_S2_VARIANT_DIRECTORY)
#include "effect_linear_s2_native.hpp"
#endif
} // namespace

int main(int argc, char** argv) {
    no_dialogs();
    try {
        const auto started = std::chrono::steady_clock::now();
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << native_usage << '\n';
            return 0;
        }
        require(argc >= 7 && argc <= 13 && argc % 2 == 1 && std::string(argv[1]) == "--expect-driver" &&
                    std::string(argv[3]) == "--suite" && std::string(argv[5]) == "--output",
                native_usage);
        const std::string expected_driver = argv[2];
        const std::string suite = argv[4];
        require((expected_driver == "direct3d12" || expected_driver == "vulkan") &&
                    (suite == "transport" || suite == "caps" || suite == "all" || suite == "linear-cost"
#if defined(RYNUI_LINEAR_S2_VARIANT_DIRECTORY)
                     || suite == "linear-s2"
#endif
                     ),
                "Unknown driver or suite");
        LinearCostOptions cost_options;
        bool size_set{};
        bool case_set{};
        bool layers_set{};
        for (int argument = 7; argument < argc; argument += 2) {
            require(suite == "linear-cost", "Linear options require --suite linear-cost");
            const std::string option = argv[argument];
            const std::string value = argv[argument + 1];
            if (option == "--linear-size") {
                require(!size_set && (value == "128x96" || value == "1920x1080"), native_usage);
                size_set = true;
                if (value == "1920x1080") {
                    cost_options.width = 1920;
                    cost_options.height = 1080;
                }
            } else if (option == "--linear-case") {
                require(!case_set && (value == "all" || value == "horizontal" || value == "diagonal" || value == "l1" ||
                                      value == "l2" || value == "adjacent" || value == "extreme"),
                        native_usage);
                case_set = true;
                cost_options.field = value;
            } else if (option == "--linear-layers") {
                require(!layers_set && (value == "single" || value == "double" || value == "both"), native_usage);
                layers_set = true;
                cost_options.layers = value;
            } else {
                throw std::invalid_argument(native_usage);
            }
        }
        require(cost_options.width == 128 ||
                    (case_set && cost_options.field != "all" && layers_set && cost_options.layers != "both"),
                "1920x1080 requires exactly one explicit case/layer selection; run 128x96 first");
        output_directory = argv[6];
        std::filesystem::create_directories(output_directory);
#if defined(RYNUI_LINEAR_S2_VARIANT_DIRECTORY)
        if (suite == "linear-s2") {
            require(argc == 7, "S2 uses only the fixed 32x24 matrix");
            cost_options.width = 32;
            cost_options.height = 24;
            std::ifstream identity(std::filesystem::path(RYNUI_LINEAR_S2_VARIANT_DIRECTORY) / "manifest.json");
            require(bool(identity), "S2 requires separate variant closure/defines/artifact identities");
            std::ofstream identity_copy(output_directory / "shader-variants.json", std::ios::binary);
            identity_copy << identity.rdbuf();
            require(bool(identity_copy), "S2 variant identity copy");
        }
#endif
        const auto closure_id = closure_identity(suite == "caps" ? "rounded_effect_cap" : "rounded_effect_paint");
        const auto host_started = CostClock::now();
        if (suite == "linear-cost") {
            std::cout << "linear_cost_begin phase=host_renderer_init source_closure=" << closure_id
                      << " config=" << RYNUI_TEST_CONFIG << " width=" << cost_options.width
                      << " height=" << cost_options.height << " case=" << cost_options.field
                      << " layers=" << cost_options.layers << '\n'
                      << std::flush;
        }
        auto host = hidden_host(cost_options.width, cost_options.height);
        require(bool(host), "Hidden platform creation failed");
        check_hidden(*host.state);
        const auto host_finished = CostClock::now();
#if defined(RYNUI_LINEAR_S2_VARIANT_DIRECTORY)
        if (suite == "linear-s2") {
            linear_s2_native(*host.state, expected_driver, started);
            linear_s2_composition_crop(*host.state, expected_driver, started);
            check_hidden(*host.state);
            require(CostClock::now() - started < std::chrono::seconds(55), "S2 exceeded unchanged 55-second workload");
            std::cout << "linear_s2 driver=" << expected_driver << " config=" << RYNUI_TEST_CONFIG << " cases=" << cases
                      << " checks=" << checks << " elapsed_ms=" << cost_ms(started) << '\n';
            return 0;
        }
#endif
        SdlSceneRenderer renderer(*host.state);
        if (suite == "linear-cost") {
            require(host.state->window_metrics().pixel_width == cost_options.width &&
                        host.state->window_metrics().pixel_height == cost_options.height,
                    "linear-cost actual hidden viewport differs from the explicitly bounded size");
            std::cout << "linear_cost_host_ms=" << cost_ms(host_started, host_finished)
                      << " renderer_init_ms=" << cost_ms(host_finished) << " suite_elapsed_ms=" << cost_ms(started)
                      << " driver=" << renderer.gpu_driver() << " format=" << renderer.shader_format() << '\n'
                      << std::flush;
        }
        require(std::string(renderer.gpu_driver()) == expected_driver,
                "Actual GPU driver differs from --expect-driver");
        require(std::string(renderer.shader_format()) == (expected_driver == "direct3d12" ? "DXIL" : "SPIR-V"),
                "Actual shader format differs from the required backend format");
        renderer.set_clear_color(Color{0, 0, 0, 0});
        if (suite == "linear-cost") {
            linear_cost(renderer, *host.state, cost_options, started);
        }
        if (suite == "transport" || suite == "all") {
            old_solid_cold(renderer, *host.state);
            require(!renderer.capabilities().effect_paints, "production paint capability prematurely enabled");
            require(renderer.begin_pending_paint_validation_for_test(), "paint candidate validation scope failed");

            struct PaintValidationScope final {
                SdlSceneRenderer& renderer;

                ~PaintValidationScope() {
                    renderer.end_pending_paint_validation_for_test();
                }
            } paint_validation{renderer};

            paint_samples(renderer, *host.state);
            raw_affine_linear(renderer, *host.state);
            cancellation_and_stop_boundaries(renderer, *host.state);
            extreme_fields(renderer, *host.state);
            fractional_clip_and_cells(renderer, *host.state);
            nonzero_draws_and_remap(renderer, *host.state);
            legacy_same_scene(renderer, *host.state);
            failure_ownership_retry_idle(renderer, *host.state);
            check_hidden(*host.state);
            require(host.state->window_metrics().pixel_width > 0 && host.state->window_metrics().pixel_height > 0,
                    "Retiring a second platform invalidated the surviving hidden window");
            if (!renderer.capabilities().effect_arc_caps) {
                pending_caps_reject(renderer, *host.state);
            }
            validation_scope_exit_reject(renderer, *host.state, false);
        }
        if (suite == "caps" || suite == "all") {
            require(!renderer.capabilities().effect_arc_caps,
                    "production non-Round capability was prematurely enabled");
            require(renderer.begin_pending_cap_validation_for_test(), "candidate-only cap validation scope failed");

            struct CapValidationScope final {
                SdlSceneRenderer& renderer;

                ~CapValidationScope() {
                    renderer.end_pending_cap_validation_for_test();
                }
            } cap_validation{renderer};

            std::cout << "pending_cap_candidate=true production_capability=false cap_source_closure="
                      << closure_identity("rounded_effect_cap") << '\n';
            caps(renderer, *host.state);
            cap_stream_transport(renderer, *host.state);
            cap_dual_alpha_and_hard_clip(renderer, *host.state);
            cap_positive_subnormal_sweep(renderer, *host.state);
            legacy_same_scene(renderer, *host.state, true);
            validation_scope_exit_reject(renderer, *host.state, true);
        }
        check_hidden(*host.state);
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
        require(elapsed < std::chrono::seconds(55), "Hidden readback matrix exceeded its bounded 55-second workload");
        std::cout << "driver=" << renderer.gpu_driver() << " format=" << renderer.shader_format()
                  << " source_closure=" << closure_id << " config=" << RYNUI_TEST_CONFIG << " suite=" << suite
                  << " cases=" << cases << " checks=" << checks << " elapsed_ms=" << elapsed.count() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "effect_paint_native failure after cases=" << cases << " checks=" << checks << ": " << error.what()
                  << '\n';
        return 1;
    }
}
