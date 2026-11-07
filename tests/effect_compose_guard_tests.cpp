#include "component/window_component_services.hpp"
#include "font/font_runtime.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/recording/recording_renderer.hpp"
#include "runtime/component_host.hpp"
#include "text/text_engine.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::detail;
using namespace ryn::graphics;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void compose_upload_draw_without_text() {
    runtime::NodeStore nodes;
    layout::LayoutEngine layout{nodes};
    runtime::FrameRequestState frames;
    runtime::DirtyQueues dirty{nodes, &frames};
    auto fonts = font::FontRuntime::create();
    require(bool(fonts), "font runtime creation");
    const auto default_font =
        fonts.runtime->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, font::FontRasterConfig{16, 1});
    require(bool(default_font), "default font loading");
    text::TextEngine engine{*fonts.runtime};
    TextSceneService text_scene{*fonts.runtime, engine, frames};
    WindowComponentServices services{nodes, layout, dirty, text_scene, std::vector{default_font.font}, frames};
    RecordingRenderer backend;
    SceneResources resources{backend};
    component::RetainedSurfaceId content;
    services.mount(Content{[&] {
        auto& build = runtime::require_component_build_context();
        const auto owner = build.mount_component<int>(0);
        const auto node = build.root(owner);
        nodes.require(node).requested_size = {8, 8};
        layout.set_layout(node, layout::LeafLayout{});
        const auto fragment = build.register_scene_fragment(owner, runtime::SceneFragmentPlacement::before_children);
        content = services.surfaces().create_content_range(fragment, {});
        build.on_resource_cleanup(owner, [&, node] {
            services.surfaces().destroy_content_range(content);
            layout.remove_layout(node);
        });
    }});
    auto cap = make_arc_effect({-0.1F, 18}, 4, 1, 0, 90, ArcCap::Butt, Color{1, 0, 0});
    cap.geometry.ancestor_clip = EffectClip{1, {6, 0, 26, 32}};
    const auto solid = make_shadow_effect({{6, 1, 1, 2}, 0}, {ShadowKind::outer, {}, 0, 0, Color{0, 1, 0}});
    const std::array effects{cap, solid};
    services.surfaces().update_content_effects(content, effects);
    auto& store = services.surfaces().effects();
    const std::vector<RoundedEffectId> ids{store.live_ids().begin(), store.live_ids().end()};
    require(ids.size() == 2 && services.text().mounted_texts().empty(), "fixture must contain two effects, no text");
    unsigned iteration{};
    for (const float scale : {0.25F, 1.0F, 0.25F, 0.25F, 0.25F}) {
        if (iteration == 3) {
            auto geometry = store.at(ids[0]).geometry;
            geometry.translation.y = 0.25F;
            require(store.update_geometry(ids[0], geometry), "geometry update");
        }
        services.set_display_scale(scale);
        const float extent = 8 / scale;
        require(services.layout_and_synchronize({extent, extent}, {0, 0, extent, extent}), "retained compose");
        require(services.display_scale() == scale, "host display scale accessor");
        const auto& composed = services.scene_composer().ordered_scene();
        const std::vector<SceneDrawCommand> original_commands{composed.commands().begin(), composed.commands().end()};
        const std::vector<RoundedEffectInstance> original_instances{store.packed_instances().begin(),
                                                                    store.packed_instances().end()};
        const auto cap_index = store.packed_index(ids[0]);
        const auto solid_index = store.packed_index(ids[1]);
        require(solid_index == (scale == 0.25F ? 1U : 0U) && cap_index.has_value() == (scale == 0.25F),
                "compose AA guard culled cap or shifted solid sentinel");
        const SceneDeviceMetrics metrics{8, 8, scale};
        EffectPaintPackedTables original_tables;
        if (cap_index) {
            pack_effect_paint_tables(original_instances, metrics, original_tables);
        }
        const auto uploads = backend.counters().uploads;
        require(
            resources.synchronize({nullptr, text_scene.atlas(), text_scene.glyph_scene().instances(), &store, metrics}),
            "resource sync");
        require(store.packed_index(ids[0]) == cap_index && store.packed_index(ids[1]) == solid_index &&
                    std::ranges::equal(composed.commands(), original_commands),
                "upload remapped original compose");
        require(backend.attach_scene(resources.attach(composed)) &&
                    backend.submit_frame(animation::AnimationTime{}) == runtime::FrameSubmissionResult::submitted,
                "Recording draw of original composed commands");
        require(backend.draws().size() == original_commands.size(), "draw command count/order changed");
        std::size_t consumed{};
        for (std::size_t draw_index = 0; draw_index < backend.draws().size(); ++draw_index) {
            const auto& draw = backend.draws()[draw_index];
            require(draw.command == original_commands[draw_index] && draw.command.kind == SceneDrawKind::rounded_effect,
                    "Recording first/count/order changed");
            require(draw.instance_bytes.size() == draw.command.instance_count * sizeof(RoundedEffectGpuInstance),
                    "draw byte count");
            for (std::uint32_t row = 0; row < draw.command.instance_count; ++row) {
                const auto index = draw.command.first_instance + row;
                const auto expected = pack_effect_paint_base_instance(original_instances[index], metrics);
                require(std::memcmp(draw.instance_bytes.data() + row * sizeof(expected), &expected, sizeof(expected)) ==
                            0,
                        "draw consumed a different effect ID or bytes after upload");
                if (cap_index) {
                    const auto expected_cap = pack_cap_geometry(original_instances[index], metrics);
                    require(draw.cap_geometry_bytes.size() == draw.command.instance_count * sizeof(expected_cap) &&
                                std::memcmp(draw.cap_geometry_bytes.data() + row * sizeof(expected_cap), &expected_cap,
                                            sizeof(expected_cap)) == 0,
                            "original compose cap row/bytes changed");
                    require(draw.effect_link_bytes.size() == draw.command.instance_count * sizeof(EffectPaintGpuLink) &&
                                std::memcmp(draw.effect_link_bytes.data() + row * sizeof(EffectPaintGpuLink),
                                            &original_tables.links[index], sizeof(EffectPaintGpuLink)) == 0,
                            "original compose link row/bytes changed");
                } else {
                    require(draw.cap_geometry_bytes.empty() && draw.effect_link_bytes.empty(),
                            "solid-only projection retained extra streams");
                }
                ++consumed;
            }
        }
        require(consumed == original_instances.size(), "composed count omitted or duplicated effect");
        require(std::ranges::equal(store.live_ids(), ids), "DPI/geometry update replaced retained IDs");
        if (iteration == 4) {
            require(backend.counters().uploads == uploads, "idle guard synchronization uploaded bytes");
        }
        ++iteration;
    }
    services.dispose();
}
} // namespace

int main() {
    try {
        compose_upload_draw_without_text();
        std::cout << "effect_compose_guard: no-text DPI/remap/geometry/idle source contract\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
