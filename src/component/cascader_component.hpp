#pragma once
#include "component/window_component_services.hpp"
#include <ryn/cascader.hpp>

namespace ryn::detail {
struct CascaderRowSnapshot final {
    String key;
    CascaderPath path;
    runtime::ComponentId component;
    bool shown{};
    bool checked{};
    bool disabled{};
    runtime::ComponentId viewport;
};

struct CascaderTagSnapshot final {
    CascaderPath path;
    runtime::ComponentId component;
    runtime::ComponentId remove;
    String label;
    bool shown{};
};

struct CascaderSnapshot final {
    runtime::ComponentId selector;
    runtime::ComponentId search;
    runtime::ComponentId panel;
    CascaderPaths value;
    CascaderPath expanded;
    std::vector<CascaderRowSnapshot> rows;
    bool open{};
    runtime::ComponentId clear;
    runtime::ComponentId overflow;
    std::vector<CascaderTagSnapshot> tags;
    std::vector<float> scroll;
    runtime::ComponentId status;
    runtime::ComponentId retry;
    String error;
    bool loading{};
    runtime::ComponentId arrow;
};

class CascaderComponentHost final : public WindowComponentParticipant {
public:
    explicit CascaderComponentHost(WindowComponentServices&);
    ~CascaderComponentHost();
    void mount(const CascaderProps&, bool panel = false);

    std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    CascaderSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool) override;
    void on_input_boundary_changed() override;

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    void on_focus_changed();
    void prepare_auxiliary_layout() override;

    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;

private:
    runtime::Size measure_panel(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place_panel(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    void mount_tags(runtime::ComponentId);
    void reconcile_tags(runtime::ComponentId);
    void remove_tag(runtime::ComponentId, CascaderPath);
    void set_query(runtime::ComponentId, String, bool);
    void clear(runtime::ComponentId);
    void emit_focus(runtime::ComponentId, bool);
    void reconcile(runtime::ComponentId, const std::vector<CascaderOption>&);
    void refresh(runtime::ComponentId, bool geometry = true);
    void activate(runtime::ComponentId, String, bool select);
    void request_open(runtime::ComponentId, bool);
    void change(runtime::ComponentId, CascaderPaths);
    void load(runtime::ComponentId, String);
    bool editable(runtime::ComponentId) const;
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    bool active_{true};
};
} // namespace ryn::detail
