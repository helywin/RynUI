#pragma once

#include "component/form_model.hpp"
#include "component/window_component_services.hpp"

namespace ryn::detail {
struct FormHostLifetime;

struct FormItemSnapshot final {
    runtime::ComponentId label;
    runtime::ComponentId control;
    runtime::ComponentId feedback;
    runtime::ComponentId extra;
    runtime::ComponentId icon;
    String error;
};

class FormComponentHost final : public WindowComponentParticipant {
public:
    explicit FormComponentHost(WindowComponentServices&);
    ~FormComponentHost();
    void mount_errors(const FormErrorListProps&);
    void mount_provider(const FormProviderProps&, const FormProviderContent&);
    bool scroll_field(runtime::ComponentId, FormNamePath, FormScrollOptions);
    std::optional<FormControlTarget> field_control(runtime::ComponentId, FormNamePath);
    void mount(const FormProps&, const FormContent&);
    void mount_list(const FormListProps&, const FormListItem&);
    void mount_list_row(const std::shared_ptr<FormListState>&, const std::shared_ptr<FormListRow>&, bool initial);
    bool update_list(const std::shared_ptr<FormListState>&, std::vector<std::shared_ptr<FormListRow>>);
    void mount_item(const FormItemProps&, const FormItemContent&);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;
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
    std::shared_ptr<FormHostLifetime> lifetime_;
    std::vector<runtime::ComponentId> forms_;
    std::vector<runtime::ComponentId> items_;
    runtime::Size viewport_;
    runtime::Rect clip_;
};

} // namespace ryn::detail
