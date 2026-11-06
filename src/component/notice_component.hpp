#pragma once
#include "component/window_component_services.hpp"
#include "component/notice_model.hpp"

namespace ryn::detail {
struct NoticeSnapshot final {
    runtime::ComponentId owner;
    runtime::ComponentId title;
    runtime::ComponentId description;
    runtime::ComponentId actions;
    runtime::ComponentId icon;
    runtime::ComponentId close;
    input::InteractionId interaction;
    NoticeStatus status{NoticeStatus::Info};
    NotificationPlacement placement{NotificationPlacement::TopRight};
    bool message{};
    bool closable{true};
    bool collapsed{};
    bool closing{};
    float progress{};
    float opacity{1};
};

class NoticeComponentHost final : public WindowComponentParticipant {
public:
    explicit NoticeComponentHost(WindowComponentServices&);
    ~NoticeComponentHost();
    void mount(const MessageHolderProps&);
    void mount(const NotificationHolderProps&);

    [[nodiscard]] std::span<const runtime::ComponentId> holders() const {
        return holders_;
    }

    [[nodiscard]] std::vector<NoticeEntry> entries(runtime::ComponentId) const;
    [[nodiscard]] NoticeSnapshot snapshot(runtime::ComponentId) const;
    [[nodiscard]] bool visible() const;
    bool consume_scroll_boundary(const input::ScrollInputEvent&);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    void on_window_active(bool) override;
    void prepare_auxiliary_layout() override;
    void synchronize_auxiliary_motion() override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;

private:
    void mount_holder(bool, const std::shared_ptr<NoticeRefState>&, const Prop<std::size_t>&, const Prop<NoticeStack>&);
    bool open(runtime::ComponentId, String, NoticeOptions, NoticeContent,
              std::optional<runtime::CapturedComponentContext> = {});
    bool close(runtime::ComponentId, runtime::ComponentId, bool immediate = false);
    bool finish_close(runtime::ComponentId, runtime::ComponentId);
    bool motion_allowed(runtime::ComponentId) const;
    bool clear(runtime::ComponentId);
    runtime::ComponentId create(runtime::ComponentId, String, const runtime::CapturedComponentContext&);
    runtime::ComponentId region(runtime::ComponentId, unsigned, const Content&);
    void refresh(runtime::ComponentId, bool);
    void trim(runtime::ComponentId);
    template <typename T, typename F> void connect(runtime::ComponentId, std::size_t, const std::optional<Prop<T>>&, F);
    animation::AnimationTime now();
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> holders_;
    std::vector<runtime::ComponentId> entries_;
    std::vector<runtime::ComponentId> context_sources_;
    std::shared_ptr<int> lifetime_{std::make_shared<int>(0)};
    bool pruning_{};
    animation::MonotonicTimeCursor clock_;
};
} // namespace ryn::detail
