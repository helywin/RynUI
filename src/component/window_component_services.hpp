#pragma once

#include "animation/runtime.hpp"
#include "animation/motion_policy.hpp"
#include "component/retained_surface_service.hpp"
#include "component/text_component.hpp"
#include "component/window_text_edit_services.hpp"
#include "input/focus_manager.hpp"
#include "input/pointer_router.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace ryn::detail {

class InputComponentHost;
class TypographyComponentHost;
class DividerComponentHost;
class SliderComponentHost;

class WindowComponentParticipant {
public:
    virtual ~WindowComponentParticipant() = default;
    virtual void* begin_mount() noexcept { return nullptr; }
    virtual void end_mount(void*) noexcept {}
    virtual void on_destroy() noexcept {}
    virtual void on_dispose() noexcept {}
    virtual void on_clipboard_bound() {}
    virtual void on_window_active(bool) {}
    virtual void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) = 0;
    virtual bool synchronize_auxiliary_fragments() { return false; }
    virtual void synchronize_auxiliary_motion() {}
    virtual std::size_t tick_auxiliary(animation::AnimationTime) { return 0; }
    virtual std::optional<animation::AnimationTime> next_auxiliary_deadline() const { return {}; }
};

using AuxiliaryComponentSynchronizer = WindowComponentParticipant;

struct WindowSyncProfile final {
    std::uint64_t text_nanoseconds{};
    std::uint64_t auxiliary_geometry_nanoseconds{};
    std::array<std::uint64_t, 8> participant_geometry_nanoseconds{};
    std::uint64_t participant_count{};
    std::uint64_t effect_nanoseconds{};
    std::uint64_t text_fragments_nanoseconds{};
    std::uint64_t auxiliary_fragments_nanoseconds{};
    std::uint64_t composer_nanoseconds{};
    std::uint64_t hit_nanoseconds{};
    std::uint64_t focus_nanoseconds{};
    std::uint64_t calls{};
};

// One owner for the retained resources shared by components in a window.
// Component hosts borrow these resources and must be destroyed before this object.
class WindowComponentServices final {
public:
    WindowComponentServices(
        runtime::NodeStore& nodes,
        layout::LayoutEngine& layout,
        runtime::DirtyQueues& dirty,
        TextSceneService& text_scene,
        std::vector<font::FontIdentity> default_font_chain,
        runtime::FrameRequestState& frame_requests);
    WindowComponentServices(
        runtime::NodeStore& nodes,
        layout::LayoutEngine& layout,
        runtime::DirtyQueues& dirty,
        TextSceneService& text_scene,
        ThemeFontResolver font_resolver,
        runtime::FrameRequestState& frame_requests);
    WindowComponentServices(const WindowComponentServices&) = delete;
    WindowComponentServices& operator=(const WindowComponentServices&) = delete;
    ~WindowComponentServices();

    void attach(WindowComponentParticipant& participant);
    void detach(WindowComponentParticipant& participant) noexcept;
    void attach_input_host(WindowComponentParticipant& participant);
    void detach_input_host(WindowComponentParticipant& participant) noexcept;
    void mount(const Content& content);
    bool destroy(runtime::ComponentId id);
    void dispose() noexcept;
    void set_window_active(bool active);
    void set_animation_time(animation::AnimationTime time) noexcept { animation_time_ = time; }
    [[nodiscard]] animation::AnimationTime animation_time() const noexcept { return animation_time_; }
    [[nodiscard]] animation::MotionPreference motion_preference() const noexcept { return motion_preference_; }
    void set_motion_preference(animation::MotionPreference preference);
    [[nodiscard]] std::size_t tick_animations(animation::AnimationTime frame_time);
    [[nodiscard]] std::optional<animation::AnimationTime> next_frame_deadline() const;
    [[nodiscard]] bool layout_and_synchronize(runtime::Size viewport, runtime::Rect clip,
        runtime::Point origin = {}, float gap = 0.0F, bool unbounded_root_height = false);
    [[nodiscard]] std::uint64_t hit_test_refresh_nanoseconds() const noexcept {
        return hit_test_refresh_nanoseconds_;
    }
    void set_sync_profiling_enabled(bool enabled) noexcept;
    void reset_sync_profile() noexcept;
    [[nodiscard]] WindowSyncProfile sync_profile() const noexcept { return sync_profile_; }
    void mark_scene_structure_dirty() noexcept { scene_structure_dirty_ = true; }
    [[nodiscard]] WindowTextEditServices& bind_text_edit(
        input::TextInputPlatform& platform, input::TextClipboard& clipboard);
    [[nodiscard]] WindowTextEditServices* text_edit() noexcept { return text_edit_.get(); }
    void bind_clipboard(input::TextClipboard& clipboard);
    [[nodiscard]] input::TextClipboard* clipboard() const noexcept { return clipboard_; }
    void set_input_runtime(InputComponentHost* host) noexcept { input_runtime_ = host; }
    [[nodiscard]] InputComponentHost* input_runtime() const noexcept { return input_runtime_; }
    [[nodiscard]] TypographyComponentHost& typography() noexcept { return *typography_; }
    [[nodiscard]] DividerComponentHost& divider() noexcept { return *divider_; }
    [[nodiscard]] SliderComponentHost& slider() noexcept { return *slider_; }

    [[nodiscard]] TextComponentHost& text() noexcept { return text_; }
    [[nodiscard]] const TextComponentHost& text() const noexcept { return text_; }
    [[nodiscard]] runtime::ComponentHost& components() noexcept { return text_.components(); }
    [[nodiscard]] const runtime::ComponentHost& components() const noexcept { return text_.components(); }
    [[nodiscard]] input::InteractionRegistry& interactions() noexcept { return interactions_; }
    [[nodiscard]] input::HitTestSnapshot& hit_test() noexcept { return hit_test_; }
    [[nodiscard]] component::ComponentSceneComposer& scene_composer() noexcept { return scene_composer_; }
    [[nodiscard]] component::RetainedSurfaceService& surfaces() noexcept { return surfaces_; }
    [[nodiscard]] graphics::RoundedEffectStore& rounded_effects() noexcept { return surfaces_.effects(); }
    [[nodiscard]] input::FocusManager& focus() noexcept { return focus_; }
    [[nodiscard]] input::PointerRouter& pointer() noexcept { return pointer_; }
    [[nodiscard]] animation::AnimationRuntime& animations() noexcept { return animations_; }
    [[nodiscard]] const animation::AnimationRuntime& animations() const noexcept { return animations_; }
    [[nodiscard]] runtime::NodeStore& nodes() noexcept { return *nodes_; }
    [[nodiscard]] layout::LayoutEngine& layout() noexcept { return *layout_; }
    [[nodiscard]] runtime::DirtyQueues& dirty() noexcept { return *dirty_; }

private:
    runtime::NodeStore* nodes_;
    layout::LayoutEngine* layout_;
    runtime::DirtyQueues* dirty_;
    TextComponentHost text_;
    input::InteractionRegistry interactions_;
    input::HitTestSnapshot hit_test_;
    component::ComponentSceneComposer scene_composer_;
    component::RetainedSurfaceService surfaces_;
    input::FocusManager focus_;
    input::PointerRouter pointer_;
    animation::AnimationRuntime animations_;
    animation::AnimationTime animation_time_;
    animation::MotionPreference motion_preference_{animation::MotionPreference::normal};
    std::vector<WindowComponentParticipant*> participants_;
    WindowComponentParticipant* input_host_{nullptr};
    bool scene_structure_dirty_{true};
    std::uint64_t hit_test_refresh_nanoseconds_{};
    bool sync_profiling_enabled_{};
    WindowSyncProfile sync_profile_{};
    std::unique_ptr<WindowTextEditServices> text_edit_;
    input::TextClipboard* clipboard_{};
    InputComponentHost* input_runtime_{};
    std::unique_ptr<TypographyComponentHost> typography_;
    std::unique_ptr<DividerComponentHost> divider_;
    std::unique_ptr<SliderComponentHost> slider_;
};

} // namespace ryn::detail
