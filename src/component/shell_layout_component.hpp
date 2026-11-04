#pragma once

#include "component/window_component_services.hpp"
#include <ryn/layout.hpp>

namespace ryn::detail {

enum class ShellLayoutRole { Layout, Header, Footer, Content, Sider };

struct ShellLayoutSnapshot final {
    ShellLayoutRole role{};
    runtime::NodeId node;
    runtime::ComponentId body;
    runtime::ComponentId trigger;
    input::InteractionId interaction;
    bool horizontal{};
    bool collapsed{};
    bool broken{};
    bool zero_width{};
    bool trigger_visible{};
    SiderTheme theme{};
};

class ShellLayoutComponentHost final : public WindowComponentParticipant {
public:
    explicit ShellLayoutComponentHost(WindowComponentServices& services);
    ~ShellLayoutComponentHost();
    void mount(const LayoutProps&, const LayoutContent&);
    void mount(const HeaderProps&, const HeaderContent&);
    void mount(const FooterProps&, const FooterContent&);
    void mount(const ContentProps&, const ContentBody&);
    void mount(const SiderProps&, const SiderContent&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] ShellLayoutSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void prepare_auxiliary_layout() override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    runtime::ComponentId mount_shell(ShellLayoutRole, const LayoutStyle&);
    void complete_mount(runtime::ComponentId);
    void refresh(runtime::ComponentId);
    void collapse(runtime::ComponentId, bool, SiderCollapseType);
    void cancel_hidden(runtime::ComponentId);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};

} // namespace ryn::detail
