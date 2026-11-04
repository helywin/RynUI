#pragma once

#include "component/window_component_services.hpp"

#include <ryn/masonry.hpp>

namespace ryn::detail {

struct MasonryItemSnapshot final {
    MasonryKey key;
    runtime::ComponentId component;
    int column{};
    runtime::Rect bounds;
};

class MasonryComponentHost final : public WindowComponentParticipant {
public:
    explicit MasonryComponentHost(WindowComponentServices& services);
    ~MasonryComponentHost() override;
    void* begin_mount() noexcept override;
    void end_mount(void* previous) noexcept override;
    void on_destroy() noexcept override;
    void prepare_auxiliary_layout() override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    void mount(MasonryOptions options, Prop<std::vector<MasonryRecord>> items, MasonryRenderer render);
    [[nodiscard]] std::vector<MasonryItemSnapshot> snapshot(runtime::ComponentId id) const;

private:
    void reconcile(runtime::ComponentId id);
    [[nodiscard]] runtime::Size measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                        layout::Constraints limits);
    void place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};

} // namespace ryn::detail
