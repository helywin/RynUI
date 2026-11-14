#pragma once

#include "gallery_demo_protocol.hpp"
#include "runtime/component_host.hpp"
#include "graphics/glyph_scene.hpp"

namespace ryn::detail {
class ButtonComponentHost;
struct InputPaintSnapshot;
} // namespace ryn::detail

namespace rynui::example {

// Evidence returned by the existing renderer readback, never inferred from a saved file.
struct GalleryDemoCaptureEvidence final {
    std::uint64_t readback_id{};
    std::uint64_t fence_id{};
    std::uint64_t scene_revision{};
    std::uint64_t renderer_owner{};
    std::uint64_t device_epoch{};
    GalleryDemoJson metrics;
    std::string framebuffer_format;
    std::string source_channel_order;
    std::string channel_order;
    std::string alpha;
    std::string color_profile;
    bool fence_completed{};
    std::string source_hash;
    std::string binary_sha256;
    bool raw_alpha_available{};
    std::uint8_t raw_alpha_min{};
    std::uint8_t raw_alpha_max{};
    std::uint64_t raw_opaque_pixels{};
    std::uint64_t raw_total_pixels{};
    std::string raw_rgba_fnv1a64;
    GalleryDemoJson clear_rgba;
};

void gallery_demo_validate_capture(const GalleryDemoCaptureEvidence&, std::uint64_t previous_readback,
                                   std::uint64_t scene_revision, const GalleryDemoJson& metrics,
                                   const GalleryDemoJson& fixture, const GalleryDemoJson& provenance);
void gallery_demo_validate_font_chain(const GalleryDemoScenario&);
void gallery_demo_validate_font_source(const GalleryDemoScenario&, const GalleryDemoJson& font, bool icon);
void gallery_demo_verify_source_files(const GalleryDemoJson& manifest, const std::filesystem::path& repository);

// Independent reads of retained authoring state. No fixture or Prop getters.
GalleryDemoJson gallery_demo_retained_content(ryn::detail::ButtonComponentHost&, ryn::runtime::ComponentId,
                                              std::optional<ryn::runtime::ComponentId> exclude = {});
GalleryDemoJson
gallery_demo_retained_tree(ryn::detail::ButtonComponentHost&, std::span<const ryn::runtime::ComponentId> roots,
                           const std::vector<std::pair<std::string, ryn::runtime::ComponentId>>& targets,
                           std::optional<ryn::runtime::ComponentId> origin = {},
                           std::span<const std::pair<std::string, ryn::runtime::ComponentId>> feedback_targets = {});
GalleryDemoJson gallery_demo_applied_theme(ryn::detail::ButtonComponentHost&, ryn::runtime::ComponentId);
GalleryDemoJson gallery_demo_input_paint_snapshot(const ryn::detail::InputPaintSnapshot&);
GalleryDemoJson gallery_demo_paint_stack(ryn::detail::ButtonComponentHost&,
                                         std::span<const ryn::runtime::ComponentId> roots,
                                         const ryn::graphics::OrderedScene& submitted);

} // namespace rynui::example
