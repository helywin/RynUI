#include "graphics/color_field.hpp"
#include "renderer/common/effect_paint_packing.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::graphics;
using namespace ryn::detail;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <class F> void rejects(F&& action) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("invalid packed paint accepted");
}

EffectPaintField linear(Color first, Color last) {
    return {EffectGradientRamp{{0, first}, {1, last}}, EffectLinearDomain{{10, 10}, {30, 10}}};
}

RoundedEffectInstance fill() {
    return make_shadow_effect({{10, 10, 20, 20}, 0}, {ShadowKind::outer, {}, 0, 0, Color{1, 1, 1}});
}

void compare(const RoundedEffectInstance& effect, runtime::Point point, float scale) {
    EffectPaintPackedTables tables;
    const std::array source{effect};
    const SceneDeviceMetrics metrics{200, 200, scale};
    pack_effect_paint_tables(source, metrics, tables);
    const auto base = pack_effect_paint_base_instance(effect, metrics);
    const auto packed = effect_paint_gpu_fragment_reference({point.x * scale, point.y * scale}, base,
                                                            tables.links.front(), tables.fields, tables.stops);
    const auto expected = rounded_effect_fragment_reference(point, effect, 1 / scale);
    const std::array color{expected.red(), expected.green(), expected.blue(), expected.alpha()};
    for (std::size_t channel = 0; channel < color.size(); ++channel) {
        // A fully transparent fragment has no observable straight RGB.
        if (channel == 3 || (color[3] > 0 && packed[3] > 0)) {
            require(std::abs(packed[channel] - color[channel]) < 0.00005F,
                    "packed table reference differs from the logical reference");
        }
    }
}

void layout_bytes_and_domains() {
    auto value = fill();
    const EffectGradientRamp ramp{
        {-1, Color{1, 0, 0, 0}}, {0.5F, Color{0, 1, 0, 0.5F}}, {0.5F, Color{0, 0, 1, 1}}, {2, Color{1, 1, 1}}};
    value.material.paint =
        EffectPaint{{ramp, EffectLinearDomain{{10, 10}, {30, 10}}}, linear(Color{1, 0, 0, 0.5F}, Color{1, 0, 0, 0.5F})};
    value.geometry.translation = {3, 7};
    EffectPaintPackedTables tables;
    const std::array source{value};
    pack_effect_paint_tables(source, {200, 200, 2}, tables);
    require(tables.links == std::vector<EffectPaintGpuLink>{{0, 1, 0, 0}} && tables.fields.size() == 2 &&
                tables.stops.size() == 6 && tables.fields[0].domain == std::array<float, 4>{26, 34, 66, 34} &&
                tables.fields[1].first_stop == 4 && tables.fields[0].stop_count == 4 &&
                tables.stops[0].position == std::array<float, 4>{-1, 0, 0, 0} &&
                tables.stops[0].premultiplied_rgba == std::array<float, 4>{0, 0, 0, 0} &&
                tables.stops[1].premultiplied_rgba == std::array<float, 4>{0, 0.5F, 0, 0.5F},
            "paint ABI field offsets, span, translation, scale or premultiplied bytes differ");
    for (const auto scale : {1.0F, 1.25F, 2.0F}) {
        for (const auto x : {13.0F, 22.9F, 23.0F, 23.1F, 32.75F}) {
            compare(value, {x, 20}, scale);
        }
    }
    // No arbitrary small stop limit, including duplicate/out-of-domain positions.
    std::vector<EffectGradientStop> many;
    for (int index = 0; index < 257; ++index) {
        many.push_back({float(index) / 256, Color{0, 0, 1, 0.5F}});
    }
    value.material.paint =
        EffectPaint{{EffectGradientRamp{many}, EffectLinearDomain{{10, 10}, {30, 10}}}, std::nullopt};
    pack_effect_paint_tables(std::array{value}, {200, 200, 1}, tables);
    require(tables.fields.front().stop_count == 257 && tables.stops.size() == 257,
            "paint pack imposed a small stop limit");
}

void composition_caps_and_legacy_parameters() {
    auto value = fill();
    value.material.opacity = 0.5F;
    value.material.paint = EffectPaint{linear(Color{1, 0, 0, 0.5F}, Color{1, 0, 0, 0.5F}),
                                       linear(Color{0, 0, 1, 0.5F}, Color{0, 0, 1, 0.5F})};
    EffectPaintPackedTables tables;
    pack_effect_paint_tables(std::array{value}, {100, 100, 1}, tables);
    const auto sample = effect_paint_gpu_fragment_reference(
        {20, 20}, pack_effect_paint_base_instance(value, {100, 100, 1}), tables.links[0], tables.fields, tables.stops);
    require(sample[3] == 0.375F && std::abs(sample[0] - 2.0F / 3) < 0.00001F &&
                std::abs(sample[2] - 1.0F / 3) < 0.00001F,
            "dual fields applied opacity before source-over");
    const EffectGradientRamp ramp{{0, Color{1, 0, 0, 0}}, {1, Color{0, 0, 1, 1}}};
    value.material.paint->foreground =
        EffectPaintField{ramp, EffectConicDomain{{20, 20}, 0, 2 * std::numbers::pi_v<float>}};
    for (const auto point : {runtime::Point{20, 20}, {21, 20}, {20, 21}, {19, 20}, {20, 19}}) {
        compare(value, point, 1.25F);
    }
    for (const auto cap : {ArcCap::Round, ArcCap::Butt, ArcCap::Square}) {
        for (const auto width : {4.0F, 20.0F}) {
            for (const auto sweep : {90.0F, 350.0F, 360.0F}) {
                auto arc = make_arc_effect({25, 25}, 3, width, 5, sweep, cap, Color{1, 0, 0, 0.5F});
                if (cap == ArcCap::Round) {
                    arc.material.paint = value.material.paint;
                }
                for (const auto point : {runtime::Point{25, 25}, {28, 25}, {28, 24.75F}, {35, 20}, {40, 25}}) {
                    compare(arc, point, 2);
                }
            }
        }
    }
    const std::array legacy{
        make_dashed_outline_effect({{10, 10, 30, 20}, 3}, 2, {3, 2, 1}, Color{1, 0, 0}),
        make_tooltip_fill_effect({{10, 10, 30, 20}, 3}, {TooltipArrowSide::top, 15, 3, 4}, Color{0, 1, 0})};
    for (auto effect : legacy) {
        const auto base = pack_rounded_effect_instance(effect, {100, 100, 1});
        effect.material.paint = value.material.paint;
        const auto painted_base = pack_effect_paint_base_instance(effect, {100, 100, 1});
        require(base == painted_base, "paint reused base dash/Tooltip parameter slots");
        compare(effect, {25, 10}, 1.25F);
    }
}

void half_open_solid_field_with_paint() {
    for (const auto scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
        for (const auto phase : {0.0F, 0.25F, 0.5F, 0.75F}) {
            const runtime::Rect bounds{12 + phase / scale, 16 + phase / scale, 36, 14};
            auto source = sampled_color_field(
                bounds, {}, 8, 4, [](float, float) { return Color{0.2F, 0.4F, 0.6F, 0.5F}; }, {1, {0, 0, 100, 80}});
            auto painted = fill();
            painted.geometry.translation = {60, 0};
            painted.material.paint = EffectPaint{linear(Color{1, 0, 0}, Color{0, 0, 1}), std::nullopt};
            source.push_back(painted);
            EffectPaintPackedTables tables;
            pack_effect_paint_tables(source, {200, 160, scale}, tables);
            require(tables.links.size() == source.size() && tables.links.back().foreground_field == 0,
                    "mixed ColorPicker solid field/paint link indices differ");
            // Every cell boundary has one owner. A new paint sharing this scene
            // must leave old solidEffects' half-open coverage and bytes intact.
            const runtime::Point boundary{bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
            float alpha{};
            for (std::size_t index = 0; index + 1 < source.size(); ++index) {
                const auto old = pack_rounded_effect_instance(source[index], {200, 160, scale});
                const auto base = pack_effect_paint_base_instance(source[index], {200, 160, scale});
                require(old == base && tables.links[index].foreground_field == effect_paint_no_field,
                        "paint altered solid field base bytes or assigned it a field");
                alpha += effect_paint_gpu_fragment_reference({boundary.x * scale, boundary.y * scale}, base,
                                                             tables.links[index], tables.fields, tables.stops)[3];
            }
            require(std::abs(alpha - 0.5F) < 0.00001F, "solid field half-open boundary blended twice or left a seam");
        }
    }
}

void malformed_tables_and_capacity() {
    auto value = fill();
    value.material.paint = EffectPaint{linear(Color{1, 0, 0}, Color{0, 0, 1}), std::nullopt};
    EffectPaintPackedTables tables;
    pack_effect_paint_tables(std::array{value}, {100, 100, 1}, tables);
    for (int mode = 0; mode < 8; ++mode) {
        auto bad = tables;
        if (mode == 0) {
            bad.links[0].foreground_field = 1;
        } else if (mode == 1) {
            bad.links[0].arc_cap = 3;
        } else if (mode == 2) {
            bad.fields[0].first_stop = effect_paint_no_field;
        } else if (mode == 3) {
            bad.fields[0].stop_count = effect_paint_no_field;
        } else if (mode == 4) {
            bad.fields[0].domain[2] = bad.fields[0].domain[0];
        } else if (mode == 5) {
            bad.stops[0].position[0] = 2;
        } else if (mode == 6) {
            bad.stops[0].premultiplied_rgba[3] = 0;
        } else {
            bad.links[0].reserved = 1;
        }
        rejects([&] { validate_effect_paint_link(bad.links[0], bad.fields, bad.stops); });
    }
    require(effect_buffer_capacity(3, 0, 32, 128) == 4 && effect_buffer_capacity(1, 8, 32, 256) == 8,
            "capacity calculation differs from actual allocation growth");
    rejects([] { static_cast<void>(effect_buffer_capacity(3, 0, 32, 127)); });
    rejects([] { static_cast<void>(effect_buffer_capacity(1, 8, 32, 255)); });
    rejects([] {
        static_cast<void>(effect_buffer_capacity(2, 0, std::numeric_limits<std::size_t>::max(),
                                                 std::numeric_limits<std::uint64_t>::max()));
    });
    if constexpr (sizeof(std::size_t) > sizeof(std::uint32_t)) {
        rejects([] {
            static_cast<void>(effect_buffer_capacity(std::size_t(effect_paint_no_field) + 1, 0, 16,
                                                     std::numeric_limits<std::uint64_t>::max()));
        });
    }
}
} // namespace

int main() {
    try {
        layout_bytes_and_domains();
        composition_caps_and_legacy_parameters();
        half_open_solid_field_with_paint();
        malformed_tables_and_capacity();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
