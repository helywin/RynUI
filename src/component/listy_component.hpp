#pragma once
#include "component/window_component_services.hpp"
#include <ryn/listy.hpp>

namespace ryn::detail {
struct ListyItemSnapshot final {
    String key;
    runtime::ComponentId component;
    runtime::Rect bounds;
    bool shown{};
};

struct ListySnapshot final {
    std::vector<ListyItemSnapshot> items;
    runtime::ComponentId viewport;
    runtime::ComponentId empty;
    runtime::ComponentId pagination;
    std::size_t current{1};
    float offset{};
    float maximum{};
    std::vector<ListyItemSnapshot> groups;
    std::size_t mounted_items{};
};

class ListyComponentHost final : public WindowComponentParticipant {
public:
    explicit ListyComponentHost(WindowComponentServices&);
    ~ListyComponentHost();
    void mount(ListyOptions, Prop<std::vector<ListyRecord>>, ListyRenderer);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] ListySnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void prepare_auxiliary_layout() override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_retained_translations() override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;

private:
    void reconcile(runtime::ComponentId);
    void refresh(runtime::ComponentId);
    bool materialize(runtime::ComponentId);
    bool scroll(runtime::ComponentId, float);
    bool reveal(runtime::ComponentId, String, ListyScrollAlign, float, bool group = false);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
