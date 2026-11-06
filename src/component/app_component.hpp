#pragma once

#include "component/window_component_services.hpp"

#include <ryn/app.hpp>

namespace ryn::detail {
struct AppBinding final {
    bool alive{true};
    std::thread::id thread{std::this_thread::get_id()};
    runtime::CapturedComponentContext context;
    runtime::ComponentId owner;
    WindowComponentServices* services{};
    MessageRef message;
    NotificationRef notification;
};

struct AppRefState final {
    std::weak_ptr<AppBinding> binding;
    std::optional<runtime::CapturedComponentContext> context;
    bool assigned{};
};

struct AppState final {
    std::shared_ptr<AppBinding> binding{std::make_shared<AppBinding>()};
};

[[nodiscard]] bool app_context_live(const AppBinding&, const runtime::CapturedComponentContext&);

class AppComponentHost final : public WindowComponentParticipant {
public:
    explicit AppComponentHost(WindowComponentServices&);
    ~AppComponentHost();
    void mount(const AppProps&, const AppContent&);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override {}

private:
    WindowComponentServices* services_;
};
} // namespace ryn::detail
