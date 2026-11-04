#pragma once
#include "component/window_component_services.hpp"
#include <ryn/transfer.hpp>

namespace ryn::detail {
struct TransferRowSnapshot final {
    TransferKey key;
    runtime::ComponentId component;
    runtime::ComponentId checkbox;
    runtime::ComponentId remove;
    bool target{};
    bool selected{};
    bool shown{};
};

struct TransferSnapshot final {
    TransferKeys target_keys;
    TransferKeys selected_keys;
    std::vector<TransferRowSnapshot> rows;
    std::array<runtime::ComponentId, 2> search;
    std::array<runtime::ComponentId, 2> select_all;
    std::array<runtime::ComponentId, 2> move;
    std::array<std::size_t, 2> pages;
    std::array<float, 2> scroll;
};

class TransferComponentHost final : public WindowComponentParticipant {
public:
    explicit TransferComponentHost(WindowComponentServices&);
    ~TransferComponentHost();
    void mount(const TransferProps&);

    std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    TransferSnapshot snapshot(runtime::ComponentId) const;
    void select(runtime::ComponentId, TransferDirection, TransferSelection);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override {}

private:
    void reconcile(runtime::ComponentId, const std::vector<TransferItem>&);
    void refresh(runtime::ComponentId);
    void select_keys(runtime::ComponentId, TransferKeys, bool);
    void move(runtime::ComponentId, TransferDirection, std::optional<TransferKey> key = {});
    bool editable(runtime::ComponentId) const;
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    bool active_{true};
};
} // namespace ryn::detail
