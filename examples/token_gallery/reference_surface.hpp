#pragma once

#include "ant_design_reference_catalog.hpp"
#include "component/button_component.hpp"

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rynui::example {

namespace detail {
struct ReferenceSurfacePropsAccess;
struct ReferenceSurfaceComponentState;
}

inline constexpr std::size_t reference_surface_visual_layer_count = 4;

enum class ReferenceSurfaceVisualLayer : std::uint8_t {
    border,
    background,
    swatch,
    status_badge,
};

class ReferenceSurfaceProps final {
public:
    ReferenceSurfaceProps& status(ryn::Prop<GallerySupportStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    ReferenceSurfaceProps& swatch(
        ryn::Prop<std::optional<ryn::Color>> value) {
        swatch_ = std::move(value);
        return *this;
    }

    ReferenceSurfaceProps& swatch(ryn::Color value) {
        return swatch(std::optional<ryn::Color>{value});
    }

    ReferenceSurfaceProps& elevated(ryn::Prop<bool> value) {
        elevated_ = std::move(value);
        return *this;
    }

    ReferenceSurfaceProps& visible(ryn::Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    ReferenceSurfaceProps& layout(ryn::LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::ReferenceSurfacePropsAccess;

    ryn::Prop<GallerySupportStatus> status_{GallerySupportStatus::planned};
    ryn::Prop<std::optional<ryn::Color>> swatch_{std::optional<ryn::Color>{}};
    ryn::Prop<bool> elevated_{false};
    ryn::Prop<bool> visible_{true};
    ryn::LayoutStyle layout_;
};

struct ReferenceSurfaceContentSlot final {};
using ReferenceSurfaceContent = ryn::SlotContent<ReferenceSurfaceContentSlot>;

using ReferenceSurfaceVisualData = std::array<
    ryn::graphics::QuadInstance,
    reference_surface_visual_layer_count>;

struct MountedReferenceSurface final {
    ryn::runtime::ComponentId component;
    ryn::runtime::NodeId node;
    ryn::component::ButtonSceneId scene;
    ryn::runtime::SceneFragmentId fragment;
};

struct ReferenceSurfaceSnapshot final {
    GallerySupportStatus status{GallerySupportStatus::planned};
    std::optional<ryn::Color> swatch;
    bool elevated{};
    bool visible{true};
    ryn::component::ButtonSceneId scene;
    ryn::graphics::QuadInstanceRange visual_range;
};

class ReferenceSurfaceHost final
    : private ryn::detail::AuxiliaryComponentSynchronizer {
public:
    explicit ReferenceSurfaceHost(
        ryn::detail::ButtonComponentHost& application);
    ReferenceSurfaceHost(const ReferenceSurfaceHost&) = delete;
    ReferenceSurfaceHost& operator=(const ReferenceSurfaceHost&) = delete;
    ~ReferenceSurfaceHost() override;

    void mount(const ryn::Content& content);
    bool destroy(ryn::runtime::ComponentId component);
    [[nodiscard]] bool layout_and_synchronize(
        ryn::runtime::Size viewport,
        ryn::runtime::Rect clip,
        ryn::runtime::Point origin = {},
        float gap = 0.0F);
    [[nodiscard]] std::span<const MountedReferenceSurface>
        mounted_surfaces() const noexcept;
    [[nodiscard]] ReferenceSurfaceSnapshot snapshot(
        ryn::runtime::ComponentId component) const;
    [[nodiscard]] ryn::detail::ButtonComponentHost& application() noexcept;
    [[nodiscard]] detail::ReferenceSurfaceComponentState* find_state(
        ryn::runtime::ComponentId component) noexcept;
    [[nodiscard]] const detail::ReferenceSurfaceComponentState* find_state(
        ryn::runtime::ComponentId component) const noexcept;

private:
    friend void ReferenceSurface(
        ReferenceSurfaceProps props,
        ReferenceSurfaceContent content);

    void synchronize_auxiliary_geometry(
        ryn::runtime::Size viewport,
        ryn::runtime::Rect clip) override;
    void record_mounted(MountedReferenceSurface mounted);

    ryn::detail::ButtonComponentHost* application_;
    std::vector<MountedReferenceSurface> mounted_;
};

void ReferenceSurface(
    ReferenceSurfaceProps props,
    ReferenceSurfaceContent content);

} // namespace rynui::example
