#pragma once
#include "component/window_component_services.hpp"
#include <ryn/collapse.hpp>

namespace ryn::detail {
struct CollapsePanelSnapshot final {
    runtime::ComponentId header;
    runtime::ComponentId title;
    runtime::ComponentId extra;
    runtime::ComponentId body;
    runtime::NodeId arrow;
    bool expanded{};
    bool disabled{};
};

struct CollapseSnapshot final {
    CollapseKeys active_keys;
    std::vector<CollapsePanelSnapshot> panels;
};

class CollapseComponentHost final : public WindowComponentParticipant {
public:
    explicit CollapseComponentHost(WindowComponentServices& services);
    ~CollapseComponentHost();
    void mount(const CollapseProps&, const std::vector<CollapsePanel>&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] CollapseSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    void refresh(runtime::ComponentId, bool geometry);
    void activate(runtime::ComponentId, std::size_t);
    [[nodiscard]] runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
