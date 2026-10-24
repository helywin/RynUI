#pragma once
#include "component/skeleton_model.hpp"
#include "component/window_component_services.hpp"

namespace ryn::detail {
struct SkeletonSnapshot final {
    runtime::ComponentId content;
    component::RetainedSurfaceId surface;
    SkeletonKind kind{SkeletonKind::Content};
    bool loading{true};
};

class SkeletonComponentHost final : public WindowComponentParticipant {
public:
    explicit SkeletonComponentHost(WindowComponentServices&);
    ~SkeletonComponentHost();
    void mount(const SkeletonProps&, const std::optional<SkeletonContent>&);
    void mount(const SkeletonAvatarProps&);
    void mount(const SkeletonButtonProps&);
    void mount(const SkeletonInputProps&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] SkeletonSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    runtime::ComponentId create(SkeletonModel, const LayoutStyle&, const std::optional<SkeletonContent>&, bool);
    void refresh(runtime::ComponentId, bool);
    template <typename T> void connect(runtime::ComponentId, const Prop<T>&, T SkeletonModel::*);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
