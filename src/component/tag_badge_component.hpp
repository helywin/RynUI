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
    bool closing{};
    float motion{1};
    runtime::Rect visual_bounds;
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
    float number_motion{1};
    float processing_phase{};
    bool ribbon{};
    runtime::ComponentId previous_count;
};

class TagBadgeComponentHost final : public WindowComponentParticipant {
public:
    explicit TagBadgeComponentHost(WindowComponentServices& services);
    ~TagBadgeComponentHost();
    void mount(const TagProps&, const TagContent&, const TagSlots&);
    void mount(const CheckableTagProps&, const TagContent&, const TagSlots&);
    void mount(const BadgeProps&, const std::optional<BadgeContent>&);
    void mount(const RibbonProps&, const BadgeContent&);
    void mount(const TagGroupProps&);
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    void synchronize_auxiliary_motion() override;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;

    struct GroupSnapshot final {
        std::vector<std::pair<String, runtime::ComponentId>> items;
        TagValues value;
        bool dragging{};
    };

    GroupSnapshot group_snapshot(runtime::ComponentId) const;

    std::span<const runtime::ComponentId> groups() const {
        return groups_;
    }

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
        groups_.clear();
    }

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    runtime::ComponentId mount_tag(const TagProps&, const TagContent&, const TagSlots&, bool, bool,
                                   std::function<void(bool)>);
    void refresh_tag(runtime::ComponentId, bool);
    void refresh_badge(runtime::ComponentId, bool);
    void activate(runtime::ComponentId);
    void reconcile_group(runtime::ComponentId, const std::vector<TagGroupItem>&);
    void refresh_group(runtime::ComponentId);
    void group_pointer(runtime::ComponentId, String, input::PointerDispatchContext&);
    void request_reorder(runtime::ComponentId, String, std::size_t);
    void replace_badge_slot(runtime::ComponentId, bool, std::shared_ptr<const BadgeRenderer>);
    void update_count(runtime::ComponentId, BadgeCountValue);
    bool active_{true};
    animation::AnimationTime observed_time_;
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> tags_;
    std::vector<runtime::ComponentId> badges_;
    std::vector<runtime::ComponentId> groups_;
};
} // namespace ryn::detail
