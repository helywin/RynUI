#pragma once
#include "component/window_component_services.hpp"
#include <ryn/table.hpp>

namespace ryn::detail {
struct TableRowSnapshot final {
    String key;
    runtime::ComponentId component;
    std::vector<runtime::ComponentId> cells;
    runtime::ComponentId checkbox;
};

struct TableSnapshot final {
    std::vector<TableRowSnapshot> rows;
    std::vector<String> visible_keys;
    std::vector<runtime::ComponentId> sort_buttons;
    runtime::ComponentId select_all;
    runtime::Rect body;
    std::vector<float> widths;
    float scroll_x{};
    float scroll_y{};
    float maximum_x{};
    float maximum_y{};
    std::size_t current{1};
    TableSort sort;
    bool loading{};
};

struct TableWork final {
    std::uint64_t copied_records{};
    std::uint64_t sort_passes{};
    std::uint64_t visited_rows{};
};

class TableComponentHost final : public WindowComponentParticipant {
public:
    explicit TableComponentHost(WindowComponentServices&);
    ~TableComponentHost();
    void mount(TableOptions, const Prop<std::vector<TableRecord>>&, std::vector<TableColumnSchema>,
               const std::optional<TableEmpty>&);

    std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    TableSnapshot snapshot(runtime::ComponentId) const;
    TableWork work(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    void on_window_active(bool) override;

private:
    void reconcile(runtime::ComponentId, std::vector<TableRecord>);
    void refresh(runtime::ComponentId, bool geometry);
    runtime::ComponentId mount_row(runtime::ComponentId, const TableRecord*, bool header, bool shown = true);
    void request_sort(runtime::ComponentId, std::size_t);
    void request_selection(runtime::ComponentId, std::optional<String>, bool);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
