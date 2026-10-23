#pragma once
#include "component/window_component_services.hpp"
#include <ryn/tag.hpp>
#include <ryn/badge.hpp>

namespace ryn::detail {
struct TagSnapshot final {
    runtime::ComponentId icon;
    runtime::ComponentId content;
    runtime::ComponentId close;
    input::InteractionId interaction;
    bool checked{};
    bool visible{true};
    bool disabled{};
    bool closable{};
    TagPalette palette;
};

struct BadgeSnapshot final {
    runtime::ComponentId content;
    runtime::ComponentId indicator;
    runtime::ComponentId text;
    bool shown{};
    bool dot{};
    bool status{};
    String display_count;
    runtime::Rect indicator_bounds;
    Color color;
};

class TagBadgeComponentHost final : public WindowComponentParticipant {
public:
    explicit TagBadgeComponentHost(WindowComponentServices& services);
    ~TagBadgeComponentHost();
    void mount(const TagProps&, const TagContent&, const TagSlots&);
    void mount(const CheckableTagProps&, const TagContent&, const TagSlots&);
    void mount(const BadgeProps&, const std::optional<BadgeContent>&);

    [[nodiscard]] std::span<const runtime::ComponentId> tags() const {
        return tags_;
    }

    [[nodiscard]] std::span<const runtime::ComponentId> badges() const {
        return badges_;
    }

    [[nodiscard]] TagSnapshot tag_snapshot(runtime::ComponentId) const;
    [[nodiscard]] BadgeSnapshot badge_snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        tags_.clear();
        badges_.clear();
    }

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    runtime::ComponentId mount_tag(const TagProps&, const TagContent&, const TagSlots&, bool, bool,
                                   std::function<void(bool)>);
    void refresh_tag(runtime::ComponentId, bool);
    void refresh_badge(runtime::ComponentId, bool);
    void activate(runtime::ComponentId);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> tags_;
    std::vector<runtime::ComponentId> badges_;
};
} // namespace ryn::detail
