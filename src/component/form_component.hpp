#pragma once

#include "component/form_model.hpp"
#include "component/window_component_services.hpp"

namespace ryn::detail {

struct FormItemSnapshot final {
    runtime::ComponentId label;
    runtime::ComponentId control;
    runtime::ComponentId feedback;
    String error;
};

class FormComponentHost final : public WindowComponentParticipant {
public:
    explicit FormComponentHost(WindowComponentServices&);
    ~FormComponentHost();
    void mount(const FormProps&, const FormContent&);
    void mount_item(const FormItemProps&, const FormItemContent&);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return forms_;
    }

    [[nodiscard]] std::span<const runtime::ComponentId> items() const {
        return items_;
    }

    [[nodiscard]] FormItemSnapshot snapshot(runtime::ComponentId) const;

private:
    bool interactive(runtime::ComponentId) const;
    void refresh(runtime::ComponentId, bool geometry);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> forms_;
    std::vector<runtime::ComponentId> items_;
    runtime::Size viewport_;
    runtime::Rect clip_;
};

} // namespace ryn::detail
