#include "renderer/common/scene_resources.hpp"
#include "renderer/recording/recording_renderer.hpp"

#include <algorithm>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string_view>

namespace {
using namespace ryn;
using namespace ryn::graphics;
using namespace ryn::detail;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <class F> void rejects(F&& action, std::string_view reason) {
    try {
        action();
    } catch (const std::invalid_argument& error) {
        require(std::string_view(error.what()).find(reason) != std::string_view::npos,
                "effect rejection omitted its specific reason");
        return;
    }
    throw std::runtime_error("unsupported effect semantics were accepted");
}

EffectPaint paint() {
    return {{EffectGradientRamp{{0, Color{1, 0, 0}}, {1, Color{0, 0, 1}}}, EffectLinearDomain{{8, 8}, {24, 8}}},
            std::nullopt};
}

RoundedEffectInstance solid() {
    return make_arc_effect({16, 16}, 8, 4, 0, 90, Color{0.25F, 0.5F, 0.75F, 0.5F}, 0.5F);
}

void capabilities_are_independent_and_default_off() {
    auto caps = baseline_scene_capabilities();
    require(!caps.effect_paints && !caps.effect_arc_caps && !caps.effect_paint_abi_version &&
                RecordingRenderer{}.capabilities().effect_paints && RecordingRenderer{}.capabilities().effect_arc_caps,
            "baseline capabilities changed or Recording did not opt in independently");
    validate_scene_effect_requirements(caps, {});
    rejects([&] { validate_scene_effect_requirements(caps, {true, false}); }, "paint");
    rejects([&] { validate_scene_effect_requirements(caps, {false, true}); }, "cap");
    caps.effect_paints = true;
    caps.effect_paint_abi_version = effect_paint_abi_version;
    validate_scene_effect_requirements(caps, {true, false});
    rejects([&] { validate_scene_effect_requirements(caps, {false, true}); }, "cap");
    caps.effect_paints = false;
    caps.effect_arc_caps = true;
    rejects([&] { validate_scene_effect_requirements(caps, {false, true}); }, "Cap Geometry ABI");
    caps.cap_geometry_abi_version = cap_geometry_abi_version;
    validate_scene_effect_requirements(caps, {false, true});
    rejects([&] { validate_scene_effect_requirements(caps, {true, false}); }, "paint");
}

void legacy_solid_bytes_and_refusal() {
    static_assert(sizeof(RoundedEffectGpuInstance) == 112);
    static_assert(logical_scene_version == 3 && packed_scene_abi_version == 2);
    const RoundedEffectGpuInstance expected{
        {-0.84375F, 0.84375F, 0.6875F, -0.6875F}, {8, 8, 16, 16}, {0, 0, 64, 64}, {0.25F, 0.5F, 0.75F, 0.5F},
        {8, 0, std::numbers::pi_v<float> / 2, 0}, {0, 4, 0, 3},   {0.5F, 1, 0, 0}};
    const auto packed = pack_rounded_effect_instance(solid(), {64, 64, 1});
    require(std::ranges::equal(std::as_bytes(std::span{&packed, 1}), std::as_bytes(std::span{&expected, 1})),
            "old solid/default Round 112-byte record changed");
    const auto explicit_round =
        make_arc_effect({16, 16}, 8, 4, 0, 90, ArcCap::Round, Color{0.25F, 0.5F, 0.75F, 0.5F}, 0.5F);
    require(pack_rounded_effect_instance(explicit_round, {64, 64, 1}) == packed,
            "explicit Round changed the legacy record");
    for (int semantic = 0; semantic < 3; ++semantic) {
        for (int visibility = 0; visibility < 4; ++visibility) {
            auto value = solid();
            if (semantic == 0) {
                value.material.paint = paint();
            } else {
                std::get<RoundedArcPath>(value.geometry.path).cap = semantic == 1 ? ArcCap::Butt : ArcCap::Square;
            }
            if (visibility == 1) {
                value.material.visible = false;
            } else if (visibility == 2) {
                value.geometry.translation = {1000, 1000};
            } else if (visibility == 3) {
                value.geometry.ancestor_clip = EffectClip{1, {1000, 1000, 2, 2}};
            }
            rejects([&] { static_cast<void>(pack_rounded_effect_instance(value, {64, 64, 1})); }, "Legacy");
        }
    }
}

void live_scene_preflight_precedes_begin_and_retries() {
    for (int semantic = 0; semantic < 4; ++semantic) {
        for (int visibility = 0; visibility < 7; ++visibility) {
            RecordingRenderer backend{baseline_scene_capabilities()};
            {
                SceneResources resources{backend};
                RoundedEffectStore effects;
                GlyphAtlas atlas;
                GlyphInstanceStore glyphs;
                OrderedScene order;
                order.append_command({SceneDrawKind::rounded_effect, 0, 1, invalid_glyph_atlas_page});
                const SceneDeviceMetrics metrics{64, 64, 1};
                const auto sync = [&] {
                    return resources.synchronize({nullptr, atlas, glyphs, &effects, metrics});
                };
                auto id = effects.add(solid());
                require(sync(), "solid scene fixture failed");
                const auto old = resources.attach(order);
                require(backend.attach_scene(old), "initial solid attachment was invalid");
                const auto uploads = backend.counters().uploads;
                const auto commits = backend.counters().commits;
                const auto cancels = backend.counters().cancels;
                const auto live_resources = backend.live_resources();
                auto unsupported = solid();
                if (semantic == 0 || semantic == 3) {
                    unsupported.material.paint = paint();
                }
                if (semantic != 0) {
                    std::get<RoundedArcPath>(unsupported.geometry.path).cap =
                        semantic == 2 ? ArcCap::Square : ArcCap::Butt;
                }
                if (visibility == 1) {
                    unsupported.material.visible = false;
                } else if (visibility == 2) {
                    unsupported.geometry.translation = {1000, 1000};
                } else if (visibility == 3) {
                    unsupported.geometry.ancestor_clip = EffectClip{2, {1000, 1000, 10, 10}};
                } else if (visibility == 4) {
                    std::get<RoundedArcPath>(unsupported.geometry.path).sweep_radians = 0;
                } else if (visibility == 5) {
                    unsupported.material.opacity = 0;
                }
                if (visibility == 6) {
                    // Ordinary retained updates must be checked even when
                    // packed topology was already current before the update.
                    static_cast<void>(effects.update_material(id, unsupported.material));
                    static_cast<void>(effects.update_geometry(id, unsupported.geometry));
                } else {
                    require(effects.remove(id), "solid identity did not retire");
                    id = effects.add(unsupported);
                }
                if (visibility != 0 && visibility != 6) {
                    static_cast<void>(effects.compact({0, 0, 64, 64}));
                }
                require(effects.live_requirements() ==
                            RoundedEffectRequirements{semantic == 0 || semantic == 3, semantic != 0},
                        "live requirements depended on packing/visibility or stale removed slots");
                backend.fail_next(RecordingFailure::begin);
                rejects([&] { static_cast<void>(sync()); }, semantic == 0 || semantic == 3 ? "paint" : "cap");
                require(backend.counters().uploads == uploads && backend.counters().commits == commits &&
                            backend.counters().cancels == cancels && backend.live_resources() == live_resources &&
                            !backend.valid_attachment(old) && !backend.valid_attachment(resources.attach(order)),
                        "unsupported preflight began a batch, changed resources, or retained an attachment");
                require(effects.at(id) == unsupported,
                        "unsupported preflight mutated logical retry data/paint ownership");
                require(effects.remove(id), "unsupported effect did not retire");
                id = effects.add(solid());
                require(effects.live_requirements() == RoundedEffectRequirements{}, "removed semantics stayed live");
                require(!sync(), "unsupported preflight consumed the pending begin failure");
                require(sync() && backend.attach_scene(resources.attach(order)),
                        "corrected solid scene could not atomically retry");
                const auto expected = pack_rounded_effect_instance(effects.at(id), metrics);
                require(std::ranges::equal(backend.buffer_bytes(resources.effects().buffer()).first(sizeof(expected)),
                                           std::as_bytes(std::span{&expected, 1})),
                        "solid retry did not upload the corrected bytes");
                const auto committed_uploads = backend.counters().uploads;
                require(sync() && backend.counters().uploads == committed_uploads,
                        "idle corrected scene re-uploaded resources");
            }
            require(backend.live_resources() == 0, "preflight/retry leaked backend resources");
        }
    }
}
} // namespace

int main() {
    try {
        capabilities_are_independent_and_default_off();
        legacy_solid_bytes_and_refusal();
        live_scene_preflight_precedes_begin_and_retries();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
