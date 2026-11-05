#pragma once
#include "component/window_component_services.hpp"
#include <ryn/tree.hpp>

namespace ryn::detail {
struct TreeRowSnapshot final {
    runtime::ComponentId component;
    runtime::ComponentId switcher;
    runtime::ComponentId checkbox;
    runtime::ComponentId title;
    runtime::ComponentId icon;
    input::InteractionId interaction;
    bool shown{};
    bool expanded{};
    bool selected{};
    bool checked{};
    bool indeterminate{};
    String key;
    bool realized{};
    bool loading{};
    String error;
};

struct TreeSnapshot final {
    TreeKeys expanded;
    TreeKeys selected;
    TreeKeys checked;
    TreeKeys half_checked;
    std::vector<TreeRowSnapshot> rows;
    float scroll{};
    float content_height{};
};

// Private host counters: projection work cannot be inferred from unchanged scene bytes.
struct TreeWork final {
    std::uint64_t projected_rows{};
    std::uint64_t presented_rows{};
};

class TreeComponentHost final : public WindowComponentParticipant {
public:
    explicit TreeComponentHost(WindowComponentServices&);
    ~TreeComponentHost();
    void mount(const TreeProps&, const std::vector<TreeItem>&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] TreeSnapshot snapshot(runtime::ComponentId) const;
    void check(runtime::ComponentId, const String&);
    [[nodiscard]] bool empty(runtime::ComponentId) const;
    [[nodiscard]] std::vector<TreeItem> data(runtime::ComponentId) const;
    [[nodiscard]] TreeWork work(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    void prepare_auxiliary_layout() override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_motion() override;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    bool intact(runtime::ComponentId);
    void refresh(runtime::ComponentId, bool);
    void present(runtime::ComponentId, std::size_t);
    void entry(runtime::ComponentId, std::size_t);
    void activate(runtime::ComponentId, std::size_t, int, bool additive = false, bool range = false);
    void reconcile(runtime::ComponentId, const std::vector<TreeItem>&);
    void mount_rows(runtime::ComponentId, const std::vector<std::size_t>&);
    void realize(runtime::ComponentId, float height);
    void load(runtime::ComponentId, String);
    void cancel_loads(runtime::ComponentId);
    void drag(runtime::ComponentId, const input::PointerInputEvent&);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    runtime::Size measure_row(runtime::ComponentId, std::size_t, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    void place_row(runtime::ComponentId, std::size_t, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
