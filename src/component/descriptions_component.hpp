#pragma once
#include "component/descriptions_model.hpp"
#include "component/window_component_services.hpp"
#include <ryn/descriptions.hpp>

namespace ryn::detail {
struct DescriptionsItemSnapshot final {
    runtime::ComponentId viewport;
    runtime::ComponentId label;
    runtime::ComponentId content;
    runtime::ComponentId colon;
    bool visible{};
    runtime::Rect label_cell;
    runtime::Rect content_cell;
};

struct DescriptionsSnapshot final {
    runtime::ComponentId header;
    runtime::ComponentId title;
    runtime::ComponentId extra;
    DescriptionsRows rows;
    std::vector<DescriptionsItemSnapshot> items;
};

class DescriptionsComponentHost final : public WindowComponentParticipant {
public:
    explicit DescriptionsComponentHost(WindowComponentServices&);
    ~DescriptionsComponentHost();
    void mount(const DescriptionsProps&, const std::vector<DescriptionsItem>&);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] DescriptionsSnapshot snapshot(runtime::ComponentId) const;

private:
    void refresh(runtime::ComponentId, bool);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
