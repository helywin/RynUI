#pragma once

#include "gallery_page_layout.hpp"
#include "gallery_page_model.hpp"
#include "gallery_scroll_region.hpp"
#include "gallery_sample_registry.hpp"
#include "reference_surface.hpp"

#include "input/platform_input.hpp"

#include <ryn/theme.hpp>
#include <ryn/i18n.hpp>
#include <ryn/text_direction.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

namespace ryn::detail {
class InputComponentHost;
}

namespace rynui::example {

struct GalleryPagesWork final {
    std::uint64_t input_events{};
    std::uint64_t wheel_events{};
    std::uint64_t component_wheels{};
    std::uint64_t document_wheels{};
    std::uint64_t navigation_wheels{};
    std::uint64_t page_mounts{};
    std::uint64_t layout_calls{};
    std::uint64_t input_nanoseconds{};
    std::uint64_t layout_sync_nanoseconds{};
};

struct GalleryPagesSnapshot final {
    GalleryPageRoute route;
    GalleryPageLayout layout;
    GalleryScrollRangeSnapshot navigation;
    GalleryScrollRangeSnapshot document;
    std::uint64_t generation{};
    bool directory_open{};
    bool failed{};
    ryn::runtime::ComponentId shell;
    ryn::runtime::ComponentId page;
};

// Internal CPU host. Native platform/GPU adapters supply input and submit its
// ordinary logical scene; no SDL or renderer object is stored in this class.
class GalleryPagesHost final {
public:
    using Copy = std::function<void(const ryn::String&)>;

    GalleryPagesHost(ReferenceSurfaceHost& surfaces, ryn::detail::InputComponentHost& inputs,
                     ryn::runtime::FrameRequestState& frames, Copy copy = {},
                     std::span<const GallerySampleDescriptor> registry = gallery_samples());
    ~GalleryPagesHost();
    GalleryPagesHost(const GalleryPagesHost&) = delete;
    GalleryPagesHost& operator=(const GalleryPagesHost&) = delete;

    void mount(ryn::runtime::Size viewport);
    bool request(GalleryPageRoute route);
    // Process only between individual input dispatches, never inside a callback.
    bool process();
    bool dispatch(const ryn::input::PlatformInputEvent& event);
    bool inject_scroll_at(float x, float y, float delta_y);
    bool synchronize(ryn::runtime::Size viewport);
    void set_theme(const ryn::ThemeConfig& theme);
    [[nodiscard]] ryn::ThemeConfig theme() const;
    void set_locale(const ryn::LocaleId& locale);
    void set_format_locale(const ryn::LocaleId& locale);
    void set_direction(ryn::TextDirection direction);
    void set_messages(const ryn::MessageCatalog& messages);
    [[nodiscard]] ryn::LocaleId locale() const;
    [[nodiscard]] GalleryPagesSnapshot snapshot() const;
    [[nodiscard]] std::string last_error() const;
    [[nodiscard]] const GalleryPagesWork& work() const noexcept;
    [[nodiscard]] const GalleryPageModel& model() const noexcept;
    [[nodiscard]] ryn::runtime::ComponentId target(std::string_view identity) const;
    [[nodiscard]] std::uint64_t sample_runs(std::string_view identity) const;
    [[nodiscard]] bool pending() const noexcept;
    void shutdown() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rynui::example
