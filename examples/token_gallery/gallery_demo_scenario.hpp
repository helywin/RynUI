#pragma once

#include "gallery_demo_sample.hpp"
#include "gallery_pages_host.hpp"
#include "component/button_component.hpp"
#include "component/input_component.hpp"

#include <filesystem>
#include <functional>

namespace rynui::example {

struct GalleryDemoNativeActions final {
    ryn::detail::ButtonComponentHost& app;
    ryn::detail::InputComponentHost& inputs;
    GalleryPagesHost& pages;
    std::shared_ptr<GalleryDemoSampleContext> context;
    std::function<void()> render;
    std::function<void(std::int64_t)> set_time;
    std::function<std::uint64_t()> scene_revision;
    std::function<GalleryDemoJson()> metrics;
    std::function<bool(const std::filesystem::path&)> capture;
    std::function<GalleryDemoJson(ryn::font::FontIdentity)> font_info;
    std::function<std::uint32_t(ryn::font::FontIdentity, char32_t)> nominal_glyph;
};

// Uses the const scene query so editor selection/placeholder views remain read-only.
[[nodiscard]] GalleryDemoJson
gallery_demo_text_snapshot(const ryn::detail::TextSceneService&, ryn::detail::TextSceneId, ryn::runtime::Rect,
                           const std::function<GalleryDemoJson(ryn::font::FontIdentity)>&,
                           const std::function<std::uint32_t(ryn::font::FontIdentity, char32_t)>& = {});

// The production finite adapter and CPU routing tests share this exact bridge.
// It dispatches through GalleryPagesHost; it has no callback/value mutation API.
class GalleryDemoInputBridge {
public:
    GalleryDemoInputBridge(const GalleryDemoScenario& scenario, GalleryDemoNativeActions actions);
    [[nodiscard]] GalleryDemoJson dispatch(const GalleryDemoAction&);
    [[nodiscard]] ryn::runtime::ComponentId target(std::string_view name);
    [[nodiscard]] std::string semantic(std::optional<ryn::input::InteractionId>);

    const GalleryDemoScenario& scenario;
    GalleryDemoNativeActions actions;
    std::map<std::string, ryn::input::TextInputSessionStamp> sessions;
};

void run_gallery_demo_scenario(const GalleryDemoScenario&, const std::filesystem::path& observations,
                               GalleryDemoJson provenance, GalleryDemoNativeActions);

} // namespace rynui::example
