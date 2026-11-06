#pragma once
#include "component/window_component_services.hpp"
#include <ryn/empty.hpp>
#include <ryn/result.hpp>

namespace ryn::detail {
struct EmptyResultSnapshot final {
    runtime::ComponentId image;
    runtime::ComponentId label;
    runtime::ComponentId subtitle;
    runtime::ComponentId extra;
    runtime::ComponentId content;
    runtime::Rect image_bounds;
    runtime::Rect label_bounds;
    runtime::Rect subtitle_bounds;
    runtime::Rect extra_bounds;
    runtime::Rect content_bounds;
    EmptyImage empty_image{EmptyImage::Default};
    ResultStatus status{Status::Default};
    bool is_result{};
    bool illustration{};
    float visual_motion{1};
};

class EmptyResultComponentHost final : public WindowComponentParticipant {
public:
    explicit EmptyResultComponentHost(WindowComponentServices&);
    ~EmptyResultComponentHost();
    void mount(const EmptyProps&, const EmptySlots&);
    void mount(const ResultProps&, const ResultSlots&);

    [[nodiscard]] std::span<const runtime::ComponentId> empties() const {
        return empties_;
    }

    [[nodiscard]] std::span<const runtime::ComponentId> results() const {
        return results_;
    }

    [[nodiscard]] EmptyResultSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void prepare_auxiliary_layout() override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_motion() override;
    void on_window_active(bool) override;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;

private:
    runtime::ComponentId mount(bool result, String label, String subtitle, EmptyImage image, ResultStatus status,
                               const LayoutStyle&, std::optional<Content> visual, std::optional<Content> extra,
                               std::optional<Content> content, std::optional<Content> description = {});
    void refresh(runtime::ComponentId, bool geometry);
    void replace_result_region(runtime::ComponentId, std::size_t, std::optional<Content>);
    template <typename Slot>
    void connect_result_slot(runtime::ComponentId, std::size_t,
                             const std::optional<Prop<std::shared_ptr<const Slot>>>&);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> empties_;
    std::vector<runtime::ComponentId> results_;
};
} // namespace ryn::detail
