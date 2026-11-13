#pragma once

#include "gallery_demo_protocol.hpp"

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
};

void gallery_demo_validate_capture(const GalleryDemoCaptureEvidence&, std::uint64_t previous_readback,
                                   std::uint64_t scene_revision, const GalleryDemoJson& metrics,
                                   const GalleryDemoJson& fixture, const GalleryDemoJson& provenance);
void gallery_demo_validate_font_chain(const GalleryDemoScenario&);
void gallery_demo_validate_font_source(const GalleryDemoScenario&, const GalleryDemoJson& font, bool icon);
void gallery_demo_verify_source_files(const GalleryDemoJson& manifest, const std::filesystem::path& repository);

} // namespace rynui::example
