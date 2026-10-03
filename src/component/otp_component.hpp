#pragma once
#include "component/window_component_services.hpp"
#include "component/otp_model.hpp"
#include <ryn/otp.hpp>

namespace ryn::detail {
struct OTPState;

struct MountedOTPComponent final {
    runtime::ComponentId component;
    runtime::NodeId node;
};

class OTPComponentHost final : private WindowComponentParticipant {
public:
    explicit OTPComponentHost(WindowComponentServices&);
    ~OTPComponentHost();

    [[nodiscard]] std::span<const MountedOTPComponent> mounted() const noexcept {
        return mounted_;
    }

    [[nodiscard]] OTPCells cells(runtime::ComponentId) const;
    [[nodiscard]] std::vector<runtime::ComponentId> cell_components(runtime::ComponentId) const;

private:
    friend struct OTPPropsAccess;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override {}

    OTPState* find(runtime::ComponentId) const;
    void mount_cells(runtime::ComponentId, std::size_t begin, std::size_t end);
    void set_length(runtime::ComponentId, std::size_t);
    void project(runtime::ComponentId);
    void update_layout(runtime::ComponentId);
    std::string prepare(runtime::ComponentId, std::size_t, std::string_view);
    void committed(runtime::ComponentId, std::size_t);
    bool focus(runtime::ComponentId, std::size_t);
    bool blur(runtime::ComponentId);
    void focused(runtime::ComponentId, std::size_t);
    bool keyboard(runtime::ComponentId, std::size_t, const input::KeyboardInputEvent&);
    WindowComponentServices* services_;
    std::vector<MountedOTPComponent> mounted_;
};
} // namespace ryn::detail
