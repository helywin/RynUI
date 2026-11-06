#pragma once
#include "component/window_component_services.hpp"
#include <ryn/affix.hpp>
#include <ryn/border_beam.hpp>
#include <ryn/watermark.hpp>

namespace ryn::detail {
enum class DecorationKind { Watermark, Affix, BorderBeam };

struct DecorationSnapshot final {
    DecorationKind kind{};
    runtime::ComponentId content;
    runtime::ComponentId layer;
    runtime::Rect bounds;
    bool affixed{};
    bool animating{};
    double phase{};
    ImageStatus image_status{};
    std::vector<TextSceneId> texts;
    std::vector<graphics::ImageId> images;
};

class DecorationComponentHost final : public WindowComponentParticipant {
public:
    explicit DecorationComponentHost(WindowComponentServices&);
    ~DecorationComponentHost();
    void mount(const WatermarkProps&, const WatermarkContent&);
    void mount(const AffixProps&, const AffixContent&);
    void mount(const BorderBeamProps&, const BorderBeamContent&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] DecorationSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    void synchronize_auxiliary_motion() override;
    void prepare_auxiliary_layout() override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_retained_translations() override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    bool synchronize_auxiliary_fragments() override;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;

private:
    struct Lifetime;
    runtime::ComponentId create(DecorationKind, const LayoutStyle&, const Content&);
    void refresh(runtime::ComponentId);
    void cancel(runtime::ComponentId);
    void start(runtime::ComponentId);
    void compose_affix(runtime::ComponentId);
    void watermark(runtime::ComponentId, runtime::Size, runtime::Rect);
    void beam(runtime::ComponentId, runtime::Size, runtime::Rect);
    void gate(runtime::ComponentId);
    WindowComponentServices* services_;
    std::shared_ptr<Lifetime> lifetime_;
    std::vector<runtime::ComponentId> mounted_;
    runtime::Size viewport_;
    runtime::Rect window_clip_;
};
} // namespace ryn::detail
