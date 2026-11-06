#pragma once
#include "component/window_component_services.hpp"
#include <ryn/qrcode.hpp>

namespace ryn::detail {
struct QRCodeSnapshot final {
    runtime::Rect bounds;
    runtime::Rect symbol;
    runtime::Rect logo;
    std::optional<QRCodeData> data;
    std::optional<QRCodeError> error;
    QRCodePaint paint;
    std::optional<graphics::ImageId> logo_image;
    runtime::ComponentId status_content;
    runtime::ComponentId refresh;
    QRCodeStatus status{};
    ImageStatus logo_status{};
    std::vector<graphics::QuadInstance> modules;
};

class QRCodeComponentHost final : public WindowComponentParticipant {
public:
    explicit QRCodeComponentHost(WindowComponentServices&);
    ~QRCodeComponentHost();
    void mount(const QRCodeProps&);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    void prepare_auxiliary_layout() override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] QRCodeSnapshot snapshot(runtime::ComponentId) const;

private:
    struct Lifetime;
    void refresh(runtime::ComponentId, bool = true);
    void encode(runtime::ComponentId);
    void cancel_logo(runtime::ComponentId) noexcept;
    void load_logo(runtime::ComponentId);
    void status_content(runtime::ComponentId);
    void publish(runtime::ComponentId, runtime::Size, runtime::Rect);
    bool request_refresh(runtime::ComponentId);
    void fail(runtime::ComponentId, QRCodeError);
    QRCodePaint paint(runtime::ComponentId) const;
    WindowComponentServices* services_;
    std::shared_ptr<Lifetime> lifetime_;
    std::vector<runtime::ComponentId> mounted_;
    bool active_{true};
};
} // namespace ryn::detail
