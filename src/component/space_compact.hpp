#pragma once

#include "component/layout_component_context.hpp"
#include "component/retained_surface_service.hpp"

#include <ryn/space.hpp>

#include <memory>

namespace ryn::detail {

struct CompactMetadata final {
    ControlSize size{ControlSize::Middle};
    SpaceOrientation orientation{SpaceOrientation::Horizontal};
    bool right_to_left{};
    bool first{true};
    bool last{true};
    std::array<bool, 4> corners{true, true, true, true};

    friend bool operator==(const CompactMetadata&, const CompactMetadata&) = default;
};

struct CompactBorder final {
    graphics::LogicalRoundedRect shape;
    std::array<bool, 4> corners{true, true, true, true};
    Color color;
    float width{1};
    int priority{2};
    bool visible{true};
    runtime::Point translation;
    std::optional<graphics::EffectClip> clip;
    bool dashed{};
    std::span<const graphics::RoundedEffectInstance> decorations;
};

class CompactContext final : public std::enable_shared_from_this<CompactContext> {
public:
    CompactContext(runtime::ComponentHost& host, LayoutComponentServices services, runtime::ComponentId component);
    void attach(runtime::ComponentId component, std::function<void(const CompactMetadata&)> apply,
                std::function<CompactBorder()> border, component::RetainedSurfaceService* surfaces = nullptr);
    void attach_many(runtime::ComponentId component, std::function<void(const CompactMetadata&)> apply,
                     std::function<std::vector<CompactBorder>()> borders,
                     component::RetainedSurfaceService* surfaces = nullptr);

    [[nodiscard]] component::RetainedSurfaceService* surfaces() const noexcept {
        return surfaces_;
    }

    [[nodiscard]] std::vector<CompactBorder> borders() const;
    [[nodiscard]] bool has_items() const;
    void detach(runtime::ComponentId component);
    void refresh(bool measure = true);
    void publish_seams();
    void dispose() noexcept;

    [[nodiscard]] std::span<const graphics::RoundedEffectInstance> seams() const noexcept {
        return seams_;
    }

    [[nodiscard]] runtime::Size measure(layout::LayoutEngine& engine, layout::Constraints constraints);
    void place(layout::LayoutEngine& engine, runtime::Rect bounds);

    CompactMetadata metadata;
    bool block{};
    bool explicit_size{};
    std::array<bool, 4> outer_corners{true, true, true, true};
    std::weak_ptr<CompactContext> parent_context;

private:
    struct Member final {
        runtime::ComponentId component;
        std::function<void(const CompactMetadata&)> apply;
        std::function<std::vector<CompactBorder>()> borders;
    };

    [[nodiscard]] runtime::ComponentId direct_child(runtime::ComponentId component) const;
    [[nodiscard]] std::vector<runtime::ComponentId> flow_children() const;
    [[nodiscard]] float overlap(runtime::NodeId left, runtime::NodeId right) const;
    runtime::ComponentHost* host_;
    LayoutComponentServices services_;
    runtime::ComponentId component_;
    runtime::NodeId node_;
    std::vector<Member> members_;
    component::RetainedSurfaceService* surfaces_{};
    runtime::SceneFragmentId seam_fragment_;
    component::RetainedSurfaceId seam_range_;
    std::vector<graphics::RoundedEffectInstance> seams_;
    bool active_{true};
    bool refreshing_{};
};

struct SpaceCompactState final {
    std::shared_ptr<CompactContext> context;
};

[[nodiscard]] std::shared_ptr<CompactContext> nearest_compact(runtime::ComponentBuildContext& build);

} // namespace ryn::detail
