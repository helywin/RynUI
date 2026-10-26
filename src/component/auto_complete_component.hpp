#pragma once
#include "component/window_component_services.hpp"
#include <ryn/auto_complete.hpp>

namespace ryn::detail {
struct AutoCompleteSnapshot final {
    runtime::ComponentId input;
    runtime::ComponentId popup;
    std::vector<runtime::ComponentId> rows;
    String value;
    String query;
    String display;
    std::optional<std::size_t> active;
    runtime::Rect bounds;
    runtime::Rect clip;
    float scroll{};
    bool visible{};
    bool backfilled{};
};

class AutoCompleteComponentHost final : public WindowComponentParticipant {
public:
    explicit AutoCompleteComponentHost(WindowComponentServices&);
    ~AutoCompleteComponentHost();
    void mount(const AutoCompleteProps&, const std::optional<AutoCompletePrefix>&,
               const std::optional<AutoCompleteSuffix>&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] AutoCompleteSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    void prepare_auxiliary_layout() override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void position_window_layer(runtime::ComponentId, runtime::Size, runtime::Rect);
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    [[nodiscard]] bool editable(runtime::ComponentId, bool check_composition = true) const;
    void refresh(runtime::ComponentId, bool geometry = true);
    void update_theme(runtime::ComponentId, bool geometry);
    void request_open(runtime::ComponentId, bool, bool restore_focus = false);
    void suppress(runtime::ComponentId);
    void publish(runtime::ComponentId, String, std::optional<AutoCompleteOption>, bool clear = false);
    void choose(runtime::ComponentId, std::size_t);
    void reconcile_rows(runtime::ComponentId);
    void mount_row(runtime::ComponentId, std::size_t);
    void mount_label(runtime::ComponentId);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    std::vector<runtime::ComponentId> positioning_;
    bool window_active_{true};
};
} // namespace ryn::detail
