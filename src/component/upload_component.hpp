#pragma once
#include "component/window_component_services.hpp"
#include "input/file_drop.hpp"
#include <ryn/upload.hpp>

namespace ryn::detail {
struct UploadRowSnapshot final {
    UploadUid uid;
    runtime::ComponentId component;
    runtime::ComponentId content;
    runtime::ComponentId image;
    std::array<runtime::ComponentId, 5> actions;
    runtime::Rect bounds;
};

struct UploadSnapshot final {
    UploadFiles files;
    std::vector<UploadRowSnapshot> rows;
    runtime::ComponentId trigger;
    runtime::ComponentId preview_group;
    runtime::Rect bounds;
    runtime::Rect clip;
    bool selecting{};
    bool dragging{};
    std::size_t pending{};
    String selection_error;
};

class UploadComponentHost final : public WindowComponentParticipant {
public:
    explicit UploadComponentHost(WindowComponentServices&);
    ~UploadComponentHost();
    void mount(const UploadProps&, const UploadSlots&);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    bool on_file_drop(const input::FileDropEvent&) override;
    void prepare_auxiliary_layout() override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const noexcept {
        return mounted_;
    }

    [[nodiscard]] UploadSnapshot snapshot(runtime::ComponentId) const;
    bool choose(runtime::ComponentId);
    bool select(runtime::ComponentId, NativeFiles);
    bool retry(runtime::ComponentId, const UploadUid&);
    bool abort(runtime::ComponentId, const UploadUid&);
    bool remove(runtime::ComponentId, const UploadUid&);
    bool preview(runtime::ComponentId, const UploadUid&);
    bool download(runtime::ComponentId, const UploadUid&);

private:
    struct Lifetime;
    void cancel_selection(runtime::ComponentId) noexcept;
    void resolve_drop(runtime::ComponentId, std::vector<String>);
    void refresh(runtime::ComponentId, bool geometry = true);
    void update_locale(runtime::ComponentId, UploadLocale);
    void reconcile(runtime::ComponentId);
    void build_row(runtime::ComponentId, const UploadFile&);
    void replace_content(runtime::ComponentId, const UploadUid&);
    bool editable(runtime::ComponentId) const;
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::shared_ptr<Lifetime> lifetime_;
    std::vector<runtime::ComponentId> mounted_;
    bool active_{true};
    std::optional<runtime::ComponentId> drop_target_;
    std::vector<String> drop_paths_;
    runtime::Point drop_position_;
};
} // namespace ryn::detail
