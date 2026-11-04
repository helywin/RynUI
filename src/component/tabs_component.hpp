#pragma once
#include "component/window_component_services.hpp"
#include <ryn/tabs.hpp>

namespace ryn::detail {
struct TabsSnapshot final {
    String key;
    std::optional<std::size_t> active;
    std::vector<runtime::ComponentId> headers;
    std::vector<runtime::ComponentId> panels;
    runtime::ComponentId previous;
    runtime::ComponentId next;
    runtime::Rect viewport;
    float scroll{};
    float maximum_scroll{};
    bool overflow{};
    runtime::ComponentId add;
    runtime::ComponentId more;
    std::vector<bool> rendered;
    float motion{1};
};

class TabsComponentHost final : public WindowComponentParticipant {
public:
    explicit TabsComponentHost(WindowComponentServices& services);
    ~TabsComponentHost();
    void mount(const TabsProps&, const std::vector<TabItem>&);
    void mount(const TabsProps&, const Prop<std::vector<TabItem>>&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] TabsSnapshot snapshot(runtime::ComponentId id) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    void on_window_active(bool active) override;
    void synchronize_auxiliary_motion() override;
    void on_input_boundary_changed() override;
    void prepare_auxiliary_layout() override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    void mount_header(runtime::ComponentId, std::size_t, bool initial = false);
    void mount_panel(runtime::ComponentId, std::size_t, bool initial = false);
    void bind_item(runtime::ComponentId, std::size_t);
    void update_items(runtime::ComponentId, const std::vector<TabItem>&);
    void edit(runtime::ComponentId, String, TabsEditAction);
    void rebuild_more(runtime::ComponentId, const std::vector<std::size_t>&);
    void refresh(runtime::ComponentId id, bool geometry);
    void activate(runtime::ComponentId id, std::size_t index);
    void scroll_to(runtime::ComponentId id, float value);
    [[nodiscard]] runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
