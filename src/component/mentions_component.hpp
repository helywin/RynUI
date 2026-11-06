#pragma once
#include "component/window_component_services.hpp"
#include <ryn/mentions.hpp>

namespace ryn::detail {
struct MentionsSnapshot final {
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
    bool open{};
    bool suppressed{};
    std::vector<std::size_t> row_indices;
    std::size_t total_rows{};
    bool pending{};
    String prefix;
    std::size_t query_begin{};
    std::size_t caret{};
    runtime::ComponentId empty;
};

class MentionsComponentHost final : public WindowComponentParticipant {
public:
    explicit MentionsComponentHost(WindowComponentServices&);
    ~MentionsComponentHost();
    void mount(const MentionsProps&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] MentionsSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    void prepare_auxiliary_layout() override;
    void validate_private_regions();
    void on_focus_changed();
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void position_window_layer(runtime::ComponentId, runtime::Size, runtime::Rect);
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    [[nodiscard]] bool editable(runtime::ComponentId, bool check_composition = true) const;
    void update_locale(runtime::ComponentId, String empty, String loading);
    void refresh(runtime::ComponentId, bool geometry = true);
    void update_theme(runtime::ComponentId, bool geometry);
    void request_open(runtime::ComponentId, bool, bool restore_focus = false);
    void suppress(runtime::ComponentId);
    void publish(runtime::ComponentId, String, std::optional<MentionsOption>, bool clear = false);
    void choose(runtime::ComponentId, std::size_t);
    void reconcile_rows(runtime::ComponentId);
    void load(runtime::ComponentId, String);
    void refresh_query(runtime::ComponentId, bool force = false);
    void rebuild_popup(runtime::ComponentId);
    void mount_popup(runtime::ComponentId);
    void mount_empty_content(runtime::ComponentId);
    void rebuild_empty_content(runtime::ComponentId);
    void mount_editor(runtime::ComponentId);
    void rebuild_editor(runtime::ComponentId);
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;
    void mount_row(runtime::ComponentId, std::size_t);
    void mount_label(runtime::ComponentId, runtime::ComponentId);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    std::vector<runtime::ComponentId> positioning_;
    std::size_t mount_depth_{};
    bool window_active_{true};
};
} // namespace ryn::detail
