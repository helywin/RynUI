#pragma once

#include "gallery_pages_host.hpp"
#include "gallery_sample_registry.hpp"
#include "component/button_component.hpp"
#include "renderer/sdl/scene_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <thread>
#include <chrono>

namespace rynui::example {

// Finite test helper: the same retained scene is drawn by the native GPU into
// a readback target. The hidden window never acquires desktop focus.
inline void capture_gallery_visuals(const GalleryPageRoute& route, std::size_t case_index,
                                    ryn::detail::ButtonComponentHost& app, GalleryPagesHost& pages,
                                    ryn::runtime::Size viewport, float scale, const std::filesystem::path& output,
                                    const std::function<void()>& render, const std::function<void(float)>& wheel,
                                    const std::function<std::vector<ryn::runtime::ComponentId>()>& triggers,
                                    const std::function<void(ryn::runtime::ComponentId)>& activate,
                                    const std::function<void()>& dismiss,
                                    const std::function<void()>& advance_animation = {}) {
    using namespace ryn;
    const auto bounds = [&](runtime::ComponentId id) {
        const auto& node = app.nodes().require(app.components().root(id));
        auto rect = node.bounds;
        rect.x += node.translation.x;
        rect.y += node.translation.y;
        return rect;
    };
    std::ofstream manifest(output / "captures.tsv", std::ios::app);
    manifest.exceptions(std::ios::failbit | std::ios::badbit);
    const auto lane = pages.snapshot().layout.document;
    const auto root = pages.snapshot().page;
    const auto save = [&](const std::string& identity, std::string_view state, std::size_t tile, runtime::Rect rect) {
        const auto clip = state == "activation" ? runtime::Rect{0, 0, viewport.width, viewport.height} : lane;
        const auto left = std::max(clip.x, rect.x - 8);
        const auto top = std::max(clip.y, rect.y - 8);
        const auto right = std::min(clip.x + clip.width - 8, rect.x + rect.width + 8);
        const auto bottom = std::min(clip.y + clip.height, rect.y + rect.height + 8);
        if (right <= left || bottom <= top) {
            throw std::runtime_error("Visual sample has no visible capture rectangle: " + identity);
        }
        const auto name = std::to_string(case_index) + "-" + identity + "-" + std::string{state} + "-" +
                          std::to_string(tile) + ".bmp";
        std::filesystem::copy_file(output / "current.bmp", output / name,
                                   std::filesystem::copy_options::overwrite_existing);
        manifest << case_index << '\t' << route.component_identity << '\t' << identity << '\t' << state << '\t' << tile
                 << '\t' << scale << '\t' << std::floor(left * scale) << '\t' << std::floor(top * scale) << '\t'
                 << std::ceil(right * scale) << '\t' << std::ceil(bottom * scale) << '\t' << name << '\n';
    };
    std::size_t samples{};
    for (const auto& sample : gallery_samples()) {
        if (sample.kind != route.kind || sample.component_identity != route.component_identity ||
            sample.family != route.family) {
            continue;
        }
        const auto id = pages.target(sample.identity);
        if (!id.valid()) {
            throw std::runtime_error("Registered visual sample is not mounted: " + std::string{sample.identity});
        }
        const auto stride = lane.height - 40;
        const auto tiles = static_cast<std::size_t>(std::max(1.0F, std::ceil(bounds(id).height / stride)));
        for (std::size_t tile = 0; tile < tiles; ++tile) {
            auto rect = bounds(id);
            const auto target = lane.y + 12 - tile * stride;
            wheel(-(rect.y - target) / 48);
            render();
            save(std::string{sample.identity}, "normal", tile, bounds(id));
        }
        // Capture the same first tile after a real document wheel roundtrip.
        wheel((pages.snapshot().document.maximum_offset + 48) / 48);
        render();
        const auto rect = bounds(id);
        wheel(-(rect.y - lane.y - 12) / 48);
        render();
        if (pages.snapshot().page != root || !app.components().contains(id)) {
            throw std::runtime_error("Visual scroll roundtrip remounted its sample");
        }
        save(std::string{sample.identity}, "roundtrip", 0, bounds(id));
        ++samples;
    }
    if (!samples) {
        throw std::runtime_error("Requested visual page has no registered samples");
    }
    std::size_t state_index{};
    for (const auto id : triggers()) {
        if (state_index == 8) {
            break;
        }
        const auto rect = bounds(id);
        wheel(-(rect.y - lane.y - 80) / 48);
        render();
        activate(id);
        render();
        save(route.component_identity, "activation", state_index++, {0, 0, viewport.width, viewport.height});
        if (route.component_identity == "ant.component.switch") {
            for (std::size_t phase = 1; phase <= 4; ++phase) {
                std::this_thread::sleep_for(std::chrono::milliseconds(40));
                render();
                save(route.component_identity, "animation", state_index * 10 + phase,
                     {0, 0, viewport.width, viewport.height});
            }
        }
        if (route.component_identity == "ant.component.skeleton" || route.component_identity == "ant.component.spin" ||
            route.component_identity == "ant.component.result" || route.component_identity == "ant.component.popover" ||
            route.component_identity == "ant.component.statistic" ||
            route.component_identity == "ant.component.tooltip" || route.component_identity == "ant.component.alert" ||
            route.component_identity == "ant.component.drawer" || route.component_identity == "ant.component.message" ||
            route.component_identity == "ant.component.notification" ||
            route.component_identity == "ant.component.app") {
            const std::size_t phases = route.component_identity == "ant.component.tooltip" ? 6 : 3;
            for (std::size_t phase = 1; phase <= phases; ++phase) {
                std::this_thread::sleep_for(std::chrono::milliseconds(60));
                render();
                save(route.component_identity, "animation", state_index * 10 + phase,
                     {0, 0, viewport.width, viewport.height});
            }
        }
        if (route.component_identity == "ant.component.border-beam" && advance_animation) {
            const auto beam = pages.target("gallery.border-beam.main");
            for (std::size_t phase = 1; phase <= 4; ++phase) {
                advance_animation();
                render();
                save(route.component_identity, "animation", state_index * 10 + phase, bounds(beam));
            }
        }
        dismiss();
        render();
    }
}

} // namespace rynui::example
