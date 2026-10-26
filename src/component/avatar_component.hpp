#pragma once
#include "component/window_component_services.hpp"
#include <ryn/avatar.hpp>

namespace ryn::detail {
struct AvatarSnapshot final {
    runtime::ComponentId icon;
    runtime::ComponentId content;
    component::RetainedSurfaceId surface;
    runtime::Rect viewport;
    float font_size{};
    float natural_text_width{};
    bool visible{};
    bool icon_visible{};
};

struct AvatarGroupItemSnapshot final {
    String key;
    runtime::ComponentId inline_avatar;
    runtime::ComponentId overflow_avatar;
    bool inline_visible{};
    bool overflow_visible{};
};

struct AvatarGroupSnapshot final {
    std::vector<AvatarGroupItemSnapshot> items;
    runtime::ComponentId popover;
    runtime::ComponentId overflow;
    std::size_t overflow_count{};
};

class AvatarComponentHost final : public WindowComponentParticipant {
public:
    explicit AvatarComponentHost(WindowComponentServices&);
    ~AvatarComponentHost();
    void mount(const AvatarProps&, const AvatarSlots&);
    void mount(const AvatarGroupProps&, const std::vector<AvatarGroupItem>&);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    [[nodiscard]] AvatarSnapshot snapshot(runtime::ComponentId) const;
    [[nodiscard]] AvatarGroupSnapshot group_snapshot(runtime::ComponentId) const;

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] std::span<const runtime::ComponentId> groups() const {
        return groups_;
    }

private:
    runtime::ComponentId mount_avatar(const AvatarProps&, const AvatarSlots&, bool grouped,
                                      std::optional<Prop<bool>> icon_visible = {});
    void refresh(runtime::ComponentId, bool);
    void refresh_group(runtime::ComponentId);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    runtime::Size measure_group(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints, bool popup);
    void place_group(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect, bool popup);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    std::vector<runtime::ComponentId> groups_;
};
} // namespace ryn::detail
