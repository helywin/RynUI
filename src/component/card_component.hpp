#pragma once
#include "component/window_component_services.hpp"
#include <ryn/card.hpp>

namespace ryn::detail {
struct CardSnapshot final {
    runtime::ComponentId header;
    runtime::ComponentId title;
    runtime::ComponentId extra;
    runtime::ComponentId cover;
    runtime::ComponentId body;
    runtime::ComponentId placeholder;
    runtime::ComponentId actions;
    runtime::Rect header_bounds;
    runtime::Rect body_bounds;
    runtime::Rect actions_bounds;
    bool loading{};
    bool hovered{};
    bool has_header{};
};

class CardComponentHost final : public WindowComponentParticipant {
public:
    explicit CardComponentHost(WindowComponentServices& services);
    ~CardComponentHost();
    void mount(const CardProps&, const CardContent&, const CardSlots&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] CardSnapshot snapshot(runtime::ComponentId id) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool active) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    void refresh(runtime::ComponentId id, bool geometry);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
