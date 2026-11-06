#pragma once
#include <ryn/file_service.hpp>

#include "animation/runtime.hpp"
#include "graphics/image_scene.hpp"
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
class FloatButtonComponentHost;
class DividerComponentHost;
class SpaceAddonHost;
class ColorPickerComponentHost;
class SliderComponentHost;
class TooltipComponentHost;
class OTPComponentHost;
class SelectComponentHost;
class AutoCompleteComponentHost;
class MentionsComponentHost;
class DatePickerComponentHost;
class CalendarComponentHost;
class CarouselComponentHost;
class TimePickerComponentHost;
class CardComponentHost;
class TabsComponentHost;
class SegmentedComponentHost;
class ModalComponentHost;
class TagBadgeComponentHost;
class PopconfirmComponentHost;
class PopoverComponentHost;
class FeedbackComponentHost;
class MenuComponentHost;
class DropdownComponentHost;
class PaginationComponentHost;
class TableComponentHost;
class EmptyResultComponentHost;
class CollapseComponentHost;
class FormComponentHost;
class DescriptionsComponentHost;
class TimelineComponentHost;
class AvatarComponentHost;
class ImageComponentHost;
class QRCodeComponentHost;
class UploadComponentHost;
class RateComponentHost;
class StatisticComponentHost;
class TreeComponentHost;
class CascaderComponentHost;
class TransferComponentHost;
class TreeSelectComponentHost;
class AnchorComponentHost;
class BreadcrumbComponentHost;
class StepsComponentHost;
class SkeletonComponentHost;
class NoticeComponentHost;
class ResponsiveLayoutHost;
class MasonryComponentHost;
class ShellLayoutComponentHost;
class SplitterComponentHost;

class WindowComponentParticipant {
public:
    virtual ~WindowComponentParticipant() = default;

    virtual void* begin_mount() noexcept {
        return nullptr;
    }

    virtual void end_mount(void*) noexcept {}

    virtual void on_destroy() noexcept {}

    virtual void on_dispose() noexcept {}

    virtual void on_clipboard_bound() {}

    virtual void on_window_active(bool) {}

    virtual void on_input_boundary_changed() {}

    virtual bool on_keyboard_input(const input::KeyboardInputEvent&) {
        return false;
    }

    virtual void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                                  std::optional<input::InteractionId>) {}

    virtual bool on_file_drop(const input::FileDropEvent&) {
        return false;
    }

    virtual bool on_scroll_input(const input::ScrollInputEvent&) {
        return false;
    }

    virtual void position_window_layers(runtime::Size, runtime::Rect) {}

    // Resolve owner-composed descendant translations after all ancestor layer writers, before glyph sync.
    virtual void synchronize_retained_translations() {}

    virtual void prepare_auxiliary_layout() {}

    virtual void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) = 0;

    virtual bool synchronize_auxiliary_fragments() {
        return false;
    }

    virtual void synchronize_auxiliary_motion() {}

    virtual std::size_t tick_auxiliary(animation::AnimationTime) {
        return 0;
    }

    virtual std::optional<animation::AnimationTime> next_auxiliary_deadline() const {
        return {};
    }
};

using AuxiliaryComponentSynchronizer = WindowComponentParticipant;

struct WindowSyncProfile final {
    std::uint64_t text_nanoseconds{};
    std::uint64_t auxiliary_geometry_nanoseconds{};
    std::array<std::uint64_t, 64> participant_geometry_nanoseconds{};
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
    WindowComponentServices(runtime::NodeStore& nodes, layout::LayoutEngine& layout, runtime::DirtyQueues& dirty,
                            TextSceneService& text_scene, std::vector<font::FontIdentity> default_font_chain,
                            runtime::FrameRequestState& frame_requests);
    WindowComponentServices(runtime::NodeStore& nodes, layout::LayoutEngine& layout, runtime::DirtyQueues& dirty,
                            TextSceneService& text_scene, ThemeFontResolver font_resolver,
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
    // Unwind cleanup: release one exact generation without delivering new boundary callbacks.
    bool retire_failed_component(runtime::ComponentId id) noexcept;
    void append_slot(runtime::ComponentId parent, const Content& content);
    // Called only by the retained-translation collection phase; applied in actual owner ancestry order.
    void enqueue_retained_translation(runtime::ComponentId owner, std::function<void()> compose);
    void dispose() noexcept;
    void set_window_active(bool active);
    bool dispatch_scroll(const input::ScrollInputEvent& event);
    bool dispatch_file_drop(const input::FileDropEvent& event);

    void bind_file_services(NativeFileServices* value, std::uint32_t window_id) noexcept {
        file_services_ = value;
        file_window_id_ = window_id;
    }

    [[nodiscard]] NativeFileServices* file_services() const noexcept {
        return file_services_;
    }

    [[nodiscard]] std::uint32_t file_window_id() const noexcept {
        return file_window_id_;
    }

    bool set_input_boundary(std::optional<runtime::ComponentId> root);
    bool set_input_blocked(runtime::ComponentId root, bool blocked);
    void request_frame() noexcept;
    [[nodiscard]] std::uint64_t frame_request_revision() const noexcept;
    [[nodiscard]] bool text_pointer_at(runtime::Point point);
    [[nodiscard]] runtime::Rect component_clip(runtime::ComponentId id, runtime::Size viewport,
                                               runtime::Rect document_clip,
                                               runtime::ClipPurpose purpose = runtime::ClipPurpose::content) const;

    void set_animation_time(animation::AnimationTime time) noexcept {
        animation_time_ = time;
    }

    [[nodiscard]] animation::AnimationTime animation_time() const noexcept {
        return animation_time_;
    }

    [[nodiscard]] animation::MotionPreference motion_preference() const noexcept {
        return motion_preference_;
    }

    void set_motion_preference(animation::MotionPreference preference);
    [[nodiscard]] std::size_t tick_animations(animation::AnimationTime frame_time);
    [[nodiscard]] std::optional<animation::AnimationTime> next_frame_deadline() const;
    [[nodiscard]] bool layout_and_synchronize(runtime::Size viewport, runtime::Rect clip, runtime::Point origin = {},
                                              float gap = 0.0F, bool unbounded_root_height = false);

    [[nodiscard]] std::uint64_t hit_test_refresh_nanoseconds() const noexcept {
        return hit_test_refresh_nanoseconds_;
    }

    void set_sync_profiling_enabled(bool enabled) noexcept;
    void reset_sync_profile() noexcept;

    [[nodiscard]] WindowSyncProfile sync_profile() const noexcept {
        return sync_profile_;
    }

    void mark_scene_structure_dirty() noexcept {
        scene_structure_dirty_ = true;
    }

    [[nodiscard]] WindowTextEditServices& bind_text_edit(input::TextInputPlatform& platform,
                                                         input::TextClipboard& clipboard);

    [[nodiscard]] WindowTextEditServices* text_edit() noexcept {
        return text_edit_.get();
    }

    void bind_clipboard(input::TextClipboard& clipboard);

    [[nodiscard]] input::TextClipboard* clipboard() const noexcept {
        return clipboard_;
    }

    void set_input_runtime(InputComponentHost* host) noexcept {
        input_runtime_ = host;
    }

    [[nodiscard]] InputComponentHost* input_runtime() const noexcept {
        return input_runtime_;
    }

    [[nodiscard]] FloatButtonComponentHost& float_button() noexcept {
        return *float_button_;
    }

    [[nodiscard]] SplitterComponentHost& splitter() noexcept {
        return *splitter_;
    }

    [[nodiscard]] ShellLayoutComponentHost& shell_layout() noexcept {
        return *shell_layout_;
    }

    [[nodiscard]] TypographyComponentHost& typography() noexcept {
        return *typography_;
    }

    [[nodiscard]] DividerComponentHost& divider() noexcept {
        return *divider_;
    }

    [[nodiscard]] SpaceAddonHost& space_addon() noexcept {
        return *space_addon_;
    }

    [[nodiscard]] ColorPickerComponentHost& color_picker() noexcept {
        return *color_picker_;
    }

    [[nodiscard]] SliderComponentHost& slider() noexcept {
        return *slider_;
    }

    [[nodiscard]] TooltipComponentHost& tooltip() noexcept {
        return *tooltip_;
    }

    [[nodiscard]] OTPComponentHost& otp() noexcept {
        return *otp_;
    }

    [[nodiscard]] SelectComponentHost& select() noexcept {
        return *select_;
    }

    [[nodiscard]] MentionsComponentHost& mentions() noexcept {
        return *mentions_;
    }

    [[nodiscard]] AutoCompleteComponentHost& auto_complete() noexcept {
        return *auto_complete_;
    }

    [[nodiscard]] TransferComponentHost& transfer() noexcept {
        return *transfer_;
    }

    [[nodiscard]] CascaderComponentHost& cascader() noexcept {
        return *cascader_;
    }

    [[nodiscard]] TreeSelectComponentHost& tree_select() noexcept {
        return *tree_select_;
    }

    [[nodiscard]] CarouselComponentHost& carousel() noexcept {
        return *carousel_;
    }

    [[nodiscard]] CalendarComponentHost& calendar() noexcept {
        return *calendar_;
    }

    [[nodiscard]] DatePickerComponentHost& date_picker() noexcept {
        return *date_picker_;
    }

    [[nodiscard]] TimePickerComponentHost& time_picker() noexcept {
        return *time_picker_;
    }

    [[nodiscard]] CardComponentHost& card() noexcept {
        return *card_;
    }

    [[nodiscard]] TabsComponentHost& tabs() noexcept {
        return *tabs_;
    }

    [[nodiscard]] CollapseComponentHost& collapse() noexcept {
        return *collapse_;
    }

    [[nodiscard]] SegmentedComponentHost& segmented() noexcept {
        return *segmented_;
    }

    [[nodiscard]] FormComponentHost& form() noexcept {
        return *form_;
    }

    [[nodiscard]] DescriptionsComponentHost& descriptions() noexcept {
        return *descriptions_;
    }

    [[nodiscard]] TimelineComponentHost& timeline() noexcept {
        return *timeline_;
    }

    [[nodiscard]] UploadComponentHost& upload() noexcept {
        return *upload_;
    }

    [[nodiscard]] QRCodeComponentHost& qrcode() noexcept {
        return *qrcode_;
    }

    [[nodiscard]] ImageComponentHost& image() noexcept {
        return *image_;
    }

    [[nodiscard]] graphics::ImageStore& images() noexcept {
        return images_;
    }

    [[nodiscard]] AvatarComponentHost& avatar() noexcept {
        return *avatar_;
    }

    [[nodiscard]] RateComponentHost& rate() noexcept {
        return *rate_;
    }

    [[nodiscard]] StatisticComponentHost& statistic() noexcept {
        return *statistic_;
    }

    [[nodiscard]] TreeComponentHost& tree() noexcept {
        return *tree_;
    }

    [[nodiscard]] AnchorComponentHost& anchor() noexcept {
        return *anchor_;
    }

    [[nodiscard]] BreadcrumbComponentHost& breadcrumb() noexcept {
        return *breadcrumb_;
    }

    [[nodiscard]] StepsComponentHost& steps() noexcept {
        return *steps_;
    }

    [[nodiscard]] SkeletonComponentHost& skeleton() noexcept {
        return *skeleton_;
    }

    [[nodiscard]] NoticeComponentHost& notices() noexcept {
        return *notices_;
    }

    [[nodiscard]] ModalComponentHost& modal() noexcept {
        return *modal_;
    }

    [[nodiscard]] TagBadgeComponentHost& tag_badge() noexcept {
        return *tag_badge_;
    }

    [[nodiscard]] PopconfirmComponentHost& popconfirm() noexcept {
        return *popconfirm_;
    }

    [[nodiscard]] PopoverComponentHost& popover() noexcept {
        return *popover_;
    }

    [[nodiscard]] FeedbackComponentHost& feedback() noexcept {
        return *feedback_;
    }

    [[nodiscard]] MenuComponentHost& menu() noexcept {
        return *menu_;
    }

    [[nodiscard]] DropdownComponentHost& dropdown() noexcept {
        return *dropdown_;
    }

    [[nodiscard]] TableComponentHost& table() noexcept {
        return *table_;
    }

    [[nodiscard]] EmptyResultComponentHost& empty_result() noexcept {
        return *empty_result_;
    }

    [[nodiscard]] PaginationComponentHost& pagination() noexcept {
        return *pagination_;
    }

    [[nodiscard]] TextComponentHost& text() noexcept {
        return text_;
    }

    [[nodiscard]] ResponsiveLayoutHost& responsive_layout() noexcept {
        return *responsive_layout_;
    }

    [[nodiscard]] MasonryComponentHost& masonry() noexcept {
        return *masonry_;
    }

    [[nodiscard]] const TextComponentHost& text() const noexcept {
        return text_;
    }

    [[nodiscard]] runtime::ComponentHost& components() noexcept {
        return text_.components();
    }

    [[nodiscard]] const runtime::ComponentHost& components() const noexcept {
        return text_.components();
    }

    [[nodiscard]] input::InteractionRegistry& interactions() noexcept {
        return interactions_;
    }

    [[nodiscard]] input::HitTestSnapshot& hit_test() noexcept {
        return hit_test_;
    }

    [[nodiscard]] component::ComponentSceneComposer& scene_composer() noexcept {
        return scene_composer_;
    }

    [[nodiscard]] component::RetainedSurfaceService& surfaces() noexcept {
        return surfaces_;
    }

    [[nodiscard]] graphics::RoundedEffectStore& rounded_effects() noexcept {
        return surfaces_.effects();
    }

    [[nodiscard]] input::FocusManager& focus() noexcept {
        return focus_;
    }

    [[nodiscard]] input::PointerRouter& pointer() noexcept {
        return pointer_;
    }

    [[nodiscard]] animation::AnimationRuntime& animations() noexcept {
        return animations_;
    }

    [[nodiscard]] const animation::AnimationRuntime& animations() const noexcept {
        return animations_;
    }

    [[nodiscard]] runtime::NodeStore& nodes() noexcept {
        return *nodes_;
    }

    [[nodiscard]] layout::LayoutEngine& layout() noexcept {
        return *layout_;
    }

    [[nodiscard]] runtime::DirtyQueues& dirty() noexcept {
        return *dirty_;
    }

private:
    bool retiring_failed_component_{};
    bool reconcile_input_boundary();
    std::uint64_t reconciled_input_boundary_revision_{};
    runtime::NodeStore* nodes_;
    layout::LayoutEngine* layout_;
    runtime::DirtyQueues* dirty_;
    runtime::FrameRequestState* frame_requests_;
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
    std::vector<std::pair<runtime::ComponentId, std::function<void()>>> retained_translation_work_;
    bool collecting_retained_translations_{};
    std::vector<WindowComponentParticipant*> pointer_participants_scratch_;
    WindowComponentParticipant* input_host_{nullptr};
    bool scene_structure_dirty_{true};
    std::optional<runtime::Rect> hit_clip_;
    std::optional<runtime::Rect> document_hit_clip_;
    std::uint64_t hit_test_refresh_nanoseconds_{};
    bool sync_profiling_enabled_{};
    WindowSyncProfile sync_profile_{};
    std::unique_ptr<WindowTextEditServices> text_edit_;
    input::TextClipboard* clipboard_{};
    InputComponentHost* input_runtime_{};
    std::optional<font::FontIdentity> clipping_density_font_;
    std::optional<float> clipping_antialias_guard_;
    std::unique_ptr<FloatButtonComponentHost> float_button_;
    std::unique_ptr<ShellLayoutComponentHost> shell_layout_;
    std::unique_ptr<SplitterComponentHost> splitter_;
    std::unique_ptr<TypographyComponentHost> typography_;
    std::unique_ptr<DividerComponentHost> divider_;
    std::unique_ptr<SpaceAddonHost> space_addon_;
    std::unique_ptr<SliderComponentHost> slider_;
    std::unique_ptr<ColorPickerComponentHost> color_picker_;
    std::unique_ptr<ModalComponentHost> modal_;
    std::unique_ptr<PopconfirmComponentHost> popconfirm_;
    std::unique_ptr<PopoverComponentHost> popover_;
    std::unique_ptr<TooltipComponentHost> tooltip_;
    std::unique_ptr<OTPComponentHost> otp_;
    std::unique_ptr<SelectComponentHost> select_;
    std::unique_ptr<AutoCompleteComponentHost> auto_complete_;
    std::unique_ptr<MentionsComponentHost> mentions_;
    std::unique_ptr<DatePickerComponentHost> date_picker_;
    std::unique_ptr<CalendarComponentHost> calendar_;
    std::unique_ptr<CarouselComponentHost> carousel_;
    std::unique_ptr<TimePickerComponentHost> time_picker_;
    std::unique_ptr<CardComponentHost> card_;
    std::unique_ptr<EmptyResultComponentHost> empty_result_;
    std::unique_ptr<TabsComponentHost> tabs_;
    std::unique_ptr<CollapseComponentHost> collapse_;
    std::unique_ptr<SegmentedComponentHost> segmented_;
    std::unique_ptr<FormComponentHost> form_;
    std::unique_ptr<DescriptionsComponentHost> descriptions_;
    std::unique_ptr<TimelineComponentHost> timeline_;
    std::unique_ptr<AvatarComponentHost> avatar_;
    graphics::ImageStore images_;
    std::unique_ptr<ImageComponentHost> image_;
    std::unique_ptr<QRCodeComponentHost> qrcode_;
    std::unique_ptr<UploadComponentHost> upload_;
    NativeFileServices* file_services_{};
    std::uint32_t file_window_id_{};

    struct InputLayoutContext final {
        runtime::Size viewport;
        runtime::Rect clip;
        runtime::Point origin;
        float gap{};
        bool unbounded{};
    };

    std::optional<InputLayoutContext> input_layout_;
    bool synchronizing_layout_{};
    std::unique_ptr<RateComponentHost> rate_;
    std::unique_ptr<StatisticComponentHost> statistic_;
    std::unique_ptr<TreeComponentHost> tree_;
    std::unique_ptr<TransferComponentHost> transfer_;
    std::unique_ptr<CascaderComponentHost> cascader_;
    std::unique_ptr<TreeSelectComponentHost> tree_select_;
    std::unique_ptr<AnchorComponentHost> anchor_;
    std::unique_ptr<BreadcrumbComponentHost> breadcrumb_;
    std::unique_ptr<StepsComponentHost> steps_;
    std::unique_ptr<SkeletonComponentHost> skeleton_;
    std::unique_ptr<NoticeComponentHost> notices_;
    std::unique_ptr<ResponsiveLayoutHost> responsive_layout_;
    std::unique_ptr<MasonryComponentHost> masonry_;
    std::unique_ptr<TagBadgeComponentHost> tag_badge_;
    std::unique_ptr<FeedbackComponentHost> feedback_;
    std::unique_ptr<MenuComponentHost> menu_;
    std::unique_ptr<DropdownComponentHost> dropdown_;
    std::unique_ptr<PaginationComponentHost> pagination_;
    std::unique_ptr<TableComponentHost> table_;
};

} // namespace ryn::detail
