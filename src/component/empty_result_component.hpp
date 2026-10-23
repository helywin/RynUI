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
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    runtime::ComponentId mount(bool result, String label, String subtitle, EmptyImage image, ResultStatus status,
                               const LayoutStyle&, std::optional<Content> visual, std::optional<Content> extra,
                               std::optional<Content> content);
    void refresh(runtime::ComponentId, bool geometry);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> empties_;
    std::vector<runtime::ComponentId> results_;
};
} // namespace ryn::detail
