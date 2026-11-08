#include "component/window_component_services.hpp"
#include "component/decoration_component.hpp"
#include "component/shell_layout_component.hpp"
#include "component/splitter_component.hpp"
#include "component/typography_component.hpp"
#include "component/float_button_component.hpp"
#include "component/divider_component.hpp"
#include "component/space_addon.hpp"
#include "component/slider_component.hpp"
#include "component/color_picker_component.hpp"
#include "component/tooltip_component.hpp"
#include "component/otp_component.hpp"
#include "component/select_component.hpp"
#include "component/auto_complete_component.hpp"
#include "component/mentions_component.hpp"
#include "component/date_picker_component.hpp"
#include "component/time_picker_component.hpp"
#include "component/card_component.hpp"
#include "component/listy_component.hpp"
#include "component/tabs_component.hpp"
#include "component/calendar_component.hpp"
#include "component/carousel_component.hpp"
#include "component/collapse_component.hpp"
#include "component/segmented_component.hpp"
#include "component/form_component.hpp"
#include "component/descriptions_component.hpp"
#include "component/timeline_component.hpp"
#include "component/avatar_component.hpp"
#include "component/image_component.hpp"
#include "component/qrcode_component.hpp"
#include "component/tour_component.hpp"
#include "component/upload_component.hpp"
#include "component/rate_component.hpp"
#include "component/statistic_component.hpp"
#include "component/tree_component.hpp"
#include "component/tree_select_component.hpp"
#include "component/cascader_component.hpp"
#include "component/transfer_component.hpp"
#include "component/breadcrumb_component.hpp"
#include "component/anchor_component.hpp"
#include "component/steps_component.hpp"
#include "component/skeleton_component.hpp"
#include "component/notice_component.hpp"
#include "component/app_component.hpp"
#include "component/modal_component.hpp"
#include "component/window_overlay_stack.hpp"
#include "component/tag_badge_component.hpp"
#include "component/popconfirm_component.hpp"
#include "component/popover_component.hpp"
#include "component/feedback_component.hpp"
#include "component/menu_component.hpp"
#include "component/dropdown_component.hpp"
#include "component/pagination_component.hpp"
#include "component/table_component.hpp"
#include "component/empty_result_component.hpp"
#include "component/input_component.hpp"
#include "component/responsive_layout.hpp"
#include "component/masonry_component.hpp"

#include <chrono>
#include <cmath>

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ryn::detail {
namespace {

class SyncPhaseTimer final {
public:
    SyncPhaseTimer(bool enabled, std::uint64_t& total) noexcept
        : enabled_(enabled), total_(total),
          started_(enabled ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{}) {}

    ~SyncPhaseTimer() {
        if (enabled_) {
            total_ += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started_)
                    .count());
        }
    }

private:
    bool enabled_;
    std::uint64_t& total_;
    std::chrono::steady_clock::time_point started_;
};

} // namespace

void WindowComponentServices::set_display_scale(float scale) {
    if (!std::isfinite(scale) || scale <= 0) {
        throw std::invalid_argument("Window display scale must be finite and positive");
    }
    if (display_scale_ != scale) {
        display_scale_ = scale;
        scene_structure_dirty_ = true;
        request_frame();
    }
}

WindowComponentServices::WindowComponentServices(runtime::NodeStore& nodes, layout::LayoutEngine& layout,
                                                 runtime::DirtyQueues& dirty, TextSceneService& text_scene,
                                                 std::vector<font::FontIdentity> default_font_chain,
                                                 runtime::FrameRequestState& frame_requests)
    : WindowComponentServices(
          nodes, layout, dirty, text_scene,
          [chain = std::move(default_font_chain)](SystemFontFamily, std::uint32_t, bool, std::uint32_t) {
              return chain;
          },
          frame_requests) {}

WindowComponentServices::WindowComponentServices(runtime::NodeStore& nodes, layout::LayoutEngine& layout,
                                                 runtime::DirtyQueues& dirty, TextSceneService& text_scene,
                                                 ThemeFontResolver font_resolver,
                                                 runtime::FrameRequestState& frame_requests)
    : nodes_(&nodes), layout_(&layout), dirty_(&dirty), frame_requests_(&frame_requests),
      text_(nodes, layout, dirty, text_scene, std::move(font_resolver)), interactions_(text_.components(), nodes),
      hit_test_(interactions_, nodes), scene_composer_(text_.components(), interactions_, hit_test_),
      surfaces_(text_.components(), nodes, scene_composer_), focus_(interactions_, &frame_requests),
      pointer_(interactions_, hit_test_, &frame_requests, &focus_) {
    pointer_.set_observer([this](const input::PointerInputEvent& event, auto hit, auto origin) {
        const auto registration = participant_registration_revision_;
        pointer_participants_scratch_.assign(participants_.begin(), participants_.end());
        for (std::size_t index = 0; index < pointer_participants_scratch_.size(); ++index) {
            auto* participant = pointer_participants_scratch_[index];
            if (std::find(participants_.begin(), participants_.end(), participant) != participants_.end()) {
                participant->on_pointer_input(event, hit, origin);
                if (participant_registration_revision_ != registration) {
                    break;
                }
            }
        }
    });
    focus_.set_command_filter([this](const input::KeyboardInputEvent& event) {
        const auto registration = participant_registration_revision_;
        for (std::size_t index = participants_.size(); index > 0; --index) {
            if (index > participants_.size()) {
                return true;
            }
            auto* participant = participants_[index - 1];
            if (participant->on_keyboard_input(event)) {
                return true;
            }
            if (participant_registration_revision_ != registration) {
                return true;
            }
        }
        return false;
    });
    text_.attach_component_scene(scene_composer_);
    text_.attach_surfaces(surfaces_);
    text_.attach_window_services(*this);
    animations_.reserve(256, 64, 256);
    animations_.set_schedule_observer(&frame_requests);
    overlay_stack_ = std::make_unique<WindowOverlayStack>(*this);
    responsive_layout_ = std::make_unique<ResponsiveLayoutHost>(*this);
    masonry_ = std::make_unique<MasonryComponentHost>(*this);
    shell_layout_ = std::make_unique<ShellLayoutComponentHost>(*this);
    splitter_ = std::make_unique<SplitterComponentHost>(*this);
    typography_ = std::make_unique<TypographyComponentHost>(*this);
    float_button_ = std::make_unique<FloatButtonComponentHost>(*this);
    divider_ = std::make_unique<DividerComponentHost>(*this);
    space_addon_ = std::make_unique<SpaceAddonHost>(*this);
    slider_ = std::make_unique<SliderComponentHost>(*this);
    color_picker_ = std::make_unique<ColorPickerComponentHost>(*this);
    notices_ = std::make_unique<NoticeComponentHost>(*this);
    app_ = std::make_unique<AppComponentHost>(*this);
    modal_ = std::make_unique<ModalComponentHost>(*this);
    table_ = std::make_unique<TableComponentHost>(*this);
    popover_ = std::make_unique<PopoverComponentHost>(*this);
    date_picker_ = std::make_unique<DatePickerComponentHost>(*this);
    time_picker_ = std::make_unique<TimePickerComponentHost>(*this);
    popconfirm_ = std::make_unique<PopconfirmComponentHost>(*this);
    dropdown_ = std::make_unique<DropdownComponentHost>(*this);
    tooltip_ = std::make_unique<TooltipComponentHost>(*this);
    otp_ = std::make_unique<OTPComponentHost>(*this);
    select_ = std::make_unique<SelectComponentHost>(*this);
    auto_complete_ = std::make_unique<AutoCompleteComponentHost>(*this);
    mentions_ = std::make_unique<MentionsComponentHost>(*this);
    focus_.set_observer([this] {
        if (!retiring_failed_component_) {
            if (carousel_) {
                carousel_->on_focus_changed();
            }
            auto_complete_->on_focus_changed();
            mentions_->on_focus_changed();
            if (cascader_) {
                cascader_->on_focus_changed();
            }
            if (transfer_) {
                transfer_->on_focus_changed();
            }
            if (tree_select_) {
                tree_select_->on_focus_changed();
            }
        }
    });
    card_ = std::make_unique<CardComponentHost>(*this);
    listy_ = std::make_unique<ListyComponentHost>(*this);
    tabs_ = std::make_unique<TabsComponentHost>(*this);
    calendar_ = std::make_unique<CalendarComponentHost>(*this);
    carousel_ = std::make_unique<CarouselComponentHost>(*this);
    decorations_ = std::make_unique<DecorationComponentHost>(*this);
    collapse_ = std::make_unique<CollapseComponentHost>(*this);
    segmented_ = std::make_unique<SegmentedComponentHost>(*this);
    form_ = std::make_unique<FormComponentHost>(*this);
    descriptions_ = std::make_unique<DescriptionsComponentHost>(*this);
    timeline_ = std::make_unique<TimelineComponentHost>(*this);
    avatar_ = std::make_unique<AvatarComponentHost>(*this);
    image_ = std::make_unique<ImageComponentHost>(*this);
    qrcode_ = std::make_unique<QRCodeComponentHost>(*this);
    upload_ = std::make_unique<UploadComponentHost>(*this);
    rate_ = std::make_unique<RateComponentHost>(*this);
    statistic_ = std::make_unique<StatisticComponentHost>(*this);
    tree_ = std::make_unique<TreeComponentHost>(*this);
    transfer_ = std::make_unique<TransferComponentHost>(*this);
    cascader_ = std::make_unique<CascaderComponentHost>(*this);
    tree_select_ = std::make_unique<TreeSelectComponentHost>(*this);
    anchor_ = std::make_unique<AnchorComponentHost>(*this);
    breadcrumb_ = std::make_unique<BreadcrumbComponentHost>(*this);
    steps_ = std::make_unique<StepsComponentHost>(*this);
    skeleton_ = std::make_unique<SkeletonComponentHost>(*this);
    tag_badge_ = std::make_unique<TagBadgeComponentHost>(*this);
    feedback_ = std::make_unique<FeedbackComponentHost>(*this);
    menu_ = std::make_unique<MenuComponentHost>(*this);
    pagination_ = std::make_unique<PaginationComponentHost>(*this);
    empty_result_ = std::make_unique<EmptyResultComponentHost>(*this);
    tour_ = std::make_unique<TourComponentHost>(*this);
}

WindowComponentServices::~WindowComponentServices() {
    dispose();
}

void WindowComponentServices::attach(WindowComponentParticipant& participant) {
    if (!interactions_.is_owner_thread()) {
        throw std::logic_error("Participant registration requires the owner thread");
    }
    if (participant_registration_revision_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::length_error("Participant registration generation exhausted");
    }
    if (std::find(participants_.begin(), participants_.end(), &participant) != participants_.end()) {
        throw std::logic_error("window component participant is already attached");
    }
    pointer_participants_scratch_.reserve(participants_.size() + 1);
    participants_.push_back(&participant);
    ++participant_registration_revision_;
}

void WindowComponentServices::attach_input_host(WindowComponentParticipant& participant) {
    if (input_host_ != nullptr) {
        throw std::logic_error("window already has an Input component host");
    }
    input::TextPointerQuery query;
    if (text_edit_) {
        query = [this](runtime::Point point) {
            return text_pointer_at(point);
        };
    }
    attach(participant);
    input_host_ = &participant;
    if (text_edit_) {
        text_edit_->bind_text_pointer_query(std::move(query));
    }
}

void WindowComponentServices::detach_input_host(WindowComponentParticipant& participant) noexcept {
    if (input_host_ == &participant) {
        input_host_ = nullptr;
        if (text_edit_) {
            text_edit_->bind_text_pointer_query({});
        }
    }
    detach(participant);
}

WindowTextEditServices& WindowComponentServices::bind_text_edit(input::TextInputPlatform& platform,
                                                                input::TextClipboard& clipboard) {
    if (!text_edit_) {
        text_edit_ = std::make_unique<WindowTextEditServices>(platform, clipboard, interactions_);
    } else if (!text_edit_->uses(platform, clipboard)) {
        throw std::logic_error("window text edit services are bound to different platform ports");
    }
    bind_clipboard(clipboard);
    return *text_edit_;
}

void WindowComponentServices::bind_clipboard(input::TextClipboard& clipboard) {
    if (clipboard_ == &clipboard) {
        return;
    }
    if (clipboard_) {
        throw std::logic_error("window clipboard is bound to a different port");
    }
    clipboard_ = &clipboard;
    for (auto* participant : participants_) {
        participant->on_clipboard_bound();
    }
}

bool WindowComponentServices::text_pointer_at(runtime::Point point) {
    if (!input_runtime_) {
        return false;
    }
    const auto hit = hit_test_.hit_test(point);
    if (!hit) {
        return false;
    }
    const auto* interaction = interactions_.find(*hit);
    if (!interaction || !interaction->eligible || !interactions_.branch_active(*hit)) {
        return false;
    }
    const auto contains = [point](runtime::Rect rect) {
        return point.x >= rect.x && point.x < rect.x + rect.width && point.y >= rect.y &&
               point.y < rect.y + rect.height;
    };
    for (const auto& mounted : input_runtime_->mounted_inputs()) {
        if (mounted.interaction != *hit) {
            continue;
        }
        const auto* editor = input_runtime_->editors().find(mounted.editor);
        if (!editor || editor->disabled()) {
            return false;
        }
        const auto geometry = input_runtime_->layout_snapshot(mounted.component);
        return contains(geometry.viewport) && contains(geometry.clip);
    }
    return false;
}

void WindowComponentServices::detach(WindowComponentParticipant& participant) noexcept {
    if (!interactions_.is_owner_thread()) {
        return;
    }
    if (std::erase(participants_, &participant) &&
        participant_registration_revision_ != std::numeric_limits<std::uint64_t>::max()) {
        ++participant_registration_revision_;
    }
}

bool WindowComponentServices::dispatch_scroll(const input::ScrollInputEvent& event) {
    if (!input::is_valid(event) || !focus_.state().window_active) {
        return false;
    }
    const auto registration = participant_registration_revision_;
    for (std::size_t index = participants_.size(); index > 0; --index) {
        if (index > participants_.size()) {
            return true;
        }
        auto* participant = participants_[index - 1];
        if (participant->on_scroll_input(event)) {
            return true;
        }
        if (participant_registration_revision_ != registration) {
            return true;
        }
    }
    return time_picker_->consume_scroll_boundary(event) || date_picker_->consume_scroll_boundary(event) ||
           dropdown_->consume_scroll_boundary(event) || popover_->consume_scroll_boundary(event) ||
           notices_->consume_scroll_boundary(event) || interactions_.consumes_unhandled_scroll();
}

bool WindowComponentServices::dispatch_file_drop(const input::FileDropEvent& event) {
    if (!input::is_valid(event) || !focus_.state().window_active || event.window_id != file_window_id_ ||
        synchronizing_layout_ || !input_layout_) {
        return false;
    }
    if (!dirty_->layout_roots().empty() || !dirty_->placement_roots().empty() || !dirty_->geometry_nodes().empty() ||
        !dirty_->transform_nodes().empty() || !dirty_->text_nodes().empty() || !dirty_->hit_test_nodes().empty() ||
        scene_composer_.needs_rebuild()) {
        const auto context = *input_layout_;
        if (!layout_and_synchronize(context.viewport, context.clip, context.origin, context.gap, context.unbounded)) {
            return false;
        }
    }
    const auto participants = participants_;
    const auto registration = participant_registration_revision_;
    for (auto it = participants.rbegin(); it != participants.rend(); ++it) {
        if (std::ranges::find(participants_, *it) != participants_.end() && (*it)->on_file_drop(event)) {
            return true;
        }
        if (participant_registration_revision_ != registration) {
            return true;
        }
    }
    return false;
}

bool WindowComponentServices::set_input_boundary(std::optional<runtime::ComponentId> root) {
    const bool changed = interactions_.set_input_boundary(root);
    return reconcile_input_boundary() || changed;
}

bool WindowComponentServices::set_input_blocked(runtime::ComponentId root, bool blocked) {
    const bool changed = interactions_.set_input_blocked(root, blocked);
    return reconcile_input_boundary() || changed;
}

void WindowComponentServices::enqueue_retained_translation(runtime::ComponentId owner, std::function<void()> compose) {
    if (!collecting_retained_translations_ || !compose) {
        throw std::logic_error("Retained translation callbacks require the collection phase");
    }
    if (!components().contains(owner)) {
        return;
    }
    if (std::ranges::any_of(retained_translation_work_, [&](const auto& entry) { return entry.first == owner; })) {
        throw std::logic_error("Duplicate retained translation owner");
    }
    retained_translation_work_.emplace_back(owner, std::move(compose));
}

void WindowComponentServices::ensure_input_owner_cleanup(const input::InputRootRef& owner) {
    if (!interactions_.owns_input_root(owner)) {
        throw std::invalid_argument("Input cleanup requires its exact live window owner");
    }
    const auto index = owner.component().index;
    if (index < input_constraint_owners_.size() && input_constraint_owners_[index] == owner) {
        return;
    }
    auto cleanup = interactions_.input_owner_cleanup(owner);
    if (index >= input_constraint_owners_.size()) {
        input_constraint_owners_.resize(std::size_t(index) + 1);
    }
    components().scope(owner.component()).on_cleanup(std::move(cleanup));
    input_constraint_owners_[index] = owner;
}

input::InputConstraintLease WindowComponentServices::create_input_constraint(const input::InputRootRef& owner,
                                                                             input::InputConstraintKind kind) {
    ensure_input_owner_cleanup(owner);
    return interactions_.create_input_constraint(owner, kind);
}

input::InputCompanionLease
WindowComponentServices::create_input_companion(const input::InputConstraintAuthority& authority,
                                                const input::InputRootRef& owner,
                                                std::span<const input::InputRootRef> controls) {
    ensure_input_owner_cleanup(owner);
    return interactions_.create_input_companion(authority, owner, controls);
}

bool WindowComponentServices::publish_input_constraints(input::InputConstraintBatch& batch) {
    const bool changed = interactions_.publish_input_constraints(batch);
    return reconcile_input_boundary() || changed;
}

bool WindowComponentServices::synchronize_input_constraints() {
    return reconcile_input_boundary();
}

bool WindowComponentServices::release_input_constraint(input::InputConstraintLease& lease) {
    const bool changed = interactions_.release_input_constraint(lease);
    return reconcile_input_boundary() || changed;
}

bool WindowComponentServices::release_input_companion(input::InputCompanionLease& lease) {
    const bool changed = interactions_.release_input_companion(lease);
    return reconcile_input_boundary() || changed;
}

void WindowComponentServices::request_frame() noexcept {
    frame_requests_->request_frame();
}

std::uint64_t WindowComponentServices::frame_request_revision() const noexcept {
    return frame_requests_->revision();
}

runtime::Rect WindowComponentServices::component_clip(runtime::ComponentId id, runtime::Size viewport,
                                                      runtime::Rect document_clip, runtime::ClipPurpose purpose) const {
    auto clip = components().in_window_layer(id) ? runtime::Rect{0, 0, viewport.width, viewport.height} : document_clip;
    const auto& root = nodes_->require(components().root(id));
    if (!root.clip_boundary && root.parent) {
        clip = nodes_->content_clip(*root.parent, clip, purpose);
    }
    return clip;
}

bool WindowComponentServices::reconcile_input_boundary() {
    if (reconciling_input_boundary_) {
        return false;
    }

    struct Guard final {
        bool& flag;

        ~Guard() {
            flag = false;
        }
    } guard{reconciling_input_boundary_};

    reconciling_input_boundary_ = true;
    bool changed = false;
    for (unsigned round = 0; round < 32; ++round) {
        static_cast<void>(overlay_stack_->reconcile(true));
        if (const auto root = interactions_.input_boundary(); root && !components().contains(*root)) {
            interactions_.set_input_boundary({});
        }
        const auto revision = interactions_.input_boundary_revision();
        if (revision == reconciled_input_boundary_revision_) {
            overlay_stack_->synchronize_focus();
            if (interactions_.input_boundary_revision() != revision) {
                continue;
            }
            return changed;
        }
        // Do not acknowledge a revision until every notification completed. A
        // throwing callback leaves it pending, while newer revisions retire this pass.
        scene_structure_dirty_ = true;
        for (auto root : components().root_components()) {
            dirty_->invalidate(components().root(root), runtime::DirtyFlags::HitTest);
        }
        if (text_edit_) {
            static_cast<void>(text_edit_->sessions().synchronize());
        }
        const auto order = interactions_.declaration_order();
        const std::vector<input::InteractionId> recipients{order.begin(), order.end()};
        bool superseded = false;
        for (auto interaction : recipients) {
            if (!interactions_.branch_active(interaction)) {
                pointer_.cancel_pointer_interaction(interaction);
            }
            if (interactions_.input_boundary_revision() != revision) {
                superseded = true;
                break;
            }
        }
        changed = true;
        if (superseded) {
            continue;
        }
        focus_.synchronize();
        if (interactions_.input_boundary_revision() != revision) {
            continue;
        }
        const auto participants = participants_;
        const auto registration = participant_registration_revision_;
        for (auto* participant : participants) {
            if (std::ranges::find(participants_, participant) != participants_.end()) {
                participant->on_input_boundary_changed();
            }
            if (interactions_.input_boundary_revision() != revision ||
                participant_registration_revision_ != registration) {
                superseded = true;
                break;
            }
        }
        if (superseded) {
            continue;
        }
        overlay_stack_->synchronize_focus();
        focus_.synchronize();
        if (interactions_.input_boundary_revision() != revision) {
            continue;
        }
        reconciled_input_boundary_revision_ = revision;
        return true;
    }
    throw std::logic_error("Input constraint callbacks did not converge within 32 publications");
}

void WindowComponentServices::mount(const Content& content) {
    std::vector<std::pair<WindowComponentParticipant*, void*>> active;
    active.reserve(participants_.size());
    try {
        for (auto* participant : participants_) {
            active.emplace_back(participant, participant->begin_mount());
        }
        text_.mount(content);
        scene_structure_dirty_ = true;
    } catch (...) {
        for (auto* participant : participants_) {
            participant->on_destroy();
        }
        overlay_stack_->on_destroy();
        for (auto it = active.rbegin(); it != active.rend(); ++it) {
            it->first->end_mount(it->second);
        }
        throw;
    }
    for (auto it = active.rbegin(); it != active.rend(); ++it) {
        it->first->end_mount(it->second);
    }
}

void WindowComponentServices::append_slot(runtime::ComponentId parent, const Content& content) {
    append_slot(parent, content, components().capture_context(parent));
}

void WindowComponentServices::append_slot(runtime::ComponentId parent, const Content& content,
                                          const runtime::CapturedComponentContext& context) {
    std::vector<std::pair<WindowComponentParticipant*, void*>> active;
    active.reserve(participants_.size());
    try {
        for (auto* participant : participants_) {
            active.emplace_back(participant, participant->begin_mount());
        }
        text_.append_slot(parent, content, context);
        scene_structure_dirty_ = true;
    } catch (...) {
        for (auto* participant : participants_) {
            participant->on_destroy();
        }
        overlay_stack_->on_destroy();
        for (auto it = active.rbegin(); it != active.rend(); ++it) {
            it->first->end_mount(it->second);
        }
        throw;
    }
    for (auto it = active.rbegin(); it != active.rend(); ++it) {
        it->first->end_mount(it->second);
    }
}

bool WindowComponentServices::destroy(runtime::ComponentId id) {
    if (!text_.destroy(id)) {
        return false;
    }
    for (auto* participant : participants_) {
        participant->on_destroy();
    }
    overlay_stack_->on_destroy();
    reconcile_input_boundary();
    scene_structure_dirty_ = true;
    return true;
}

bool WindowComponentServices::retire_failed_component(runtime::ComponentId id) noexcept {
    struct Restore final {
        bool& target;
        bool previous;

        ~Restore() {
            target = previous;
        }
    } restore{retiring_failed_component_, std::exchange(retiring_failed_component_, true)};

    // Drain the old subtree's event sources before resource destructors can cancel them again.
    // This reuses the dispatchers' non-delivering cancellation branch, including hover/capture.
    for (auto interaction : interactions_.declaration_order()) {
        const auto* record = interactions_.find(interaction);
        if (!record) {
            continue;
        }
        for (auto owner = std::optional{record->component}; owner && components().contains(*owner);
             owner = components().parent(*owner)) {
            if (*owner == id) {
                pointer_.cancel_interaction(interaction, false);
                break;
            }
        }
    }
    if (!text_.destroy(id)) {
        return false;
    }
    for (auto* participant : participants_) {
        participant->on_destroy();
    }
    overlay_stack_->on_destroy();
    if (const auto boundary = interactions_.input_boundary(); boundary && !components().contains(*boundary)) {
        interactions_.set_input_boundary({});
    }
    scene_structure_dirty_ = true;
    return true;
}

void WindowComponentServices::dispose() noexcept {
    input_layout_.reset();
    if (!components().active()) {
        return;
    }
    try {
        pointer_.cancel_all();
    } catch (...) {
    }
    text_.dispose();
    for (auto* participant : participants_) {
        participant->on_dispose();
    }
    overlay_stack_->dispose();
    interactions_.set_input_boundary({});
    reconciled_input_boundary_revision_ = interactions_.input_boundary_revision();
    scene_structure_dirty_ = true;
}

void WindowComponentServices::set_window_active(bool active) {
    if (!active) {
        pointer_.cancel_all();
    }
    focus_.set_window_active(active);
    text_.synchronize_icon_motion();
    for (auto* participant : participants_) {
        participant->on_window_active(active);
    }
    overlay_stack_->on_window_active(active);
    static_cast<void>(reconcile_input_boundary());
}

void WindowComponentServices::set_motion_preference(animation::MotionPreference preference) {
    if (motion_preference_ == preference) {
        return;
    }
    motion_preference_ = preference;
    text_.synchronize_icon_motion();
    for (auto* participant : participants_) {
        participant->synchronize_auxiliary_motion();
    }
}

std::size_t WindowComponentServices::tick_animations(animation::AnimationTime frame_time) {
    animation_time_ = frame_time;
    text_.synchronize_icon_motion();
    auto changed = animations_.tick(frame_time);
    for (auto* participant : participants_) {
        changed += participant->tick_auxiliary(frame_time);
    }
    return changed;
}

std::optional<animation::AnimationTime> WindowComponentServices::next_frame_deadline() const {
    auto next = animations_.next_deadline();
    for (const auto* participant : participants_) {
        const auto candidate = participant->next_auxiliary_deadline();
        if (candidate && (!next || *candidate < *next)) {
            next = candidate;
        }
    }
    return next;
}

bool WindowComponentServices::layout_and_synchronize(runtime::Size viewport, runtime::Rect clip, runtime::Point origin,
                                                     float gap, bool unbounded_root_height) {
    struct LayoutInputGuard final {
        bool& active;
        bool previous;

        LayoutInputGuard(bool& value) : active(value), previous(value) {
            active = true;
        }

        ~LayoutInputGuard() {
            active = previous;
        }
    } guard{synchronizing_layout_};

    reconcile_input_boundary();
    responsive_layout_->set_window_width(viewport.width);
    auto& text_scene = text_.scene_service();
    for (auto* participant : participants_) {
        participant->prepare_auxiliary_layout();
    }
    // A later Input count/formatter callback can retire an earlier compound's private Input.
    // Retire damaged AutoComplete owners before LayoutEngine starts walking their nodes.
    auto_complete_->validate_private_regions();
    mentions_->validate_private_regions();

    struct SceneBatch {
        TextSceneService& scene;

        ~SceneBatch() {
            scene.cancel_ordered_scene_batch();
        }
    } batch{text_scene};

    text_scene.begin_ordered_scene_batch();
    if (sync_profiling_enabled_) {
        ++sync_profile_.calls;
    }
    {
        SyncPhaseTimer timer(sync_profiling_enabled_, sync_profile_.text_nanoseconds);
        if (!text_.layout_and_synchronize(viewport, clip, origin, gap, false, unbounded_root_height, [this, viewport] {
                for (auto* participant : participants_) {
                    participant->position_window_layers(viewport, {0, 0, viewport.width, viewport.height});
                }
                retained_translation_work_.clear();
                collecting_retained_translations_ = true;
                try {
                    for (auto* participant : participants_) {
                        participant->synchronize_retained_translations();
                    }
                } catch (...) {
                    collecting_retained_translations_ = false;
                    retained_translation_work_.clear();
                    throw;
                }
                collecting_retained_translations_ = false;
                auto work = std::move(retained_translation_work_);
                const auto depth = [this](runtime::ComponentId id) {
                    std::size_t result{};
                    for (auto parent = components().parent(id); parent; parent = components().parent(*parent)) {
                        ++result;
                    }
                    return result;
                };
                std::erase_if(work, [this](const auto& entry) { return !components().contains(entry.first); });
                std::stable_sort(work.begin(), work.end(),
                                 [&](const auto& a, const auto& b) { return depth(a.first) < depth(b.first); });
                for (auto& [owner, compose] : work) {
                    if (!components().contains(owner)) {
                        continue;
                    }
                    try {
                        compose();
                    } catch (...) {
                        retire_failed_component(owner);
                        throw;
                    }
                }
            })) {
            return false;
        }
    }
    {
        SyncPhaseTimer timer(sync_profiling_enabled_, sync_profile_.auxiliary_geometry_nanoseconds);
        if (sync_profiling_enabled_) {
            sync_profile_.participant_count = participants_.size();
        }
        for (std::size_t index = 0; index < participants_.size(); ++index) {
            const auto started =
                sync_profiling_enabled_ ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            participants_[index]->synchronize_auxiliary_geometry(viewport, clip);
            if (sync_profiling_enabled_ && index < sync_profile_.participant_geometry_nanoseconds.size()) {
                sync_profile_.participant_geometry_nanoseconds[index] += static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started)
                        .count());
            }
        }
    }
    {
        SyncPhaseTimer timer(sync_profiling_enabled_, sync_profile_.effect_nanoseconds);
        text_scene.finish_ordered_scene_batch();
        const float logical_antialias_guard = 1.0F / display_scale_;
        static_cast<void>(surfaces_.synchronize_paint_opacity());
        if (surfaces_.compact_effects({0.0F, 0.0F, viewport.width, viewport.height}, clip, logical_antialias_guard)) {
            scene_structure_dirty_ = true;
        }
    }
    bool text_fragments_changed = false;
    {
        SyncPhaseTimer timer(sync_profiling_enabled_, sync_profile_.text_fragments_nanoseconds);
        text_fragments_changed = text_.synchronize_scene_fragments(
            [](runtime::ComponentId) { return std::optional<input::InteractionId>{}; });
    }
    {
        SyncPhaseTimer timer(sync_profiling_enabled_, sync_profile_.auxiliary_fragments_nanoseconds);
        for (auto* participant : participants_) {
            if (participant->synchronize_auxiliary_fragments()) {
                scene_structure_dirty_ = true;
            }
        }
    }
    // Window layers use the window extent; the composer separately clips every document interaction.
    const runtime::Rect hit_clip{0, 0, viewport.width, viewport.height};
    if (hit_clip_ != hit_clip || document_hit_clip_ != clip) {
        scene_structure_dirty_ = true;
        hit_clip_ = hit_clip;
        document_hit_clip_ = clip;
    }
    if (scene_structure_dirty_ || text_fragments_changed || scene_composer_.needs_rebuild()) {
        SyncPhaseTimer timer(sync_profiling_enabled_, sync_profile_.composer_nanoseconds);
        scene_composer_.rebuild(hit_clip, clip);
        scene_structure_dirty_ = false;
    } else if (text_.layout_performed_last_sync()) {
        SyncPhaseTimer timer(sync_profiling_enabled_, sync_profile_.hit_nanoseconds);
        const auto started = std::chrono::steady_clock::now();
        for (const auto interaction : interactions_.declaration_order()) {
            static_cast<void>(hit_test_.refresh_interaction(interaction));
        }
        hit_test_refresh_nanoseconds_ += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count());
    } else if (!dirty_->hit_test_nodes().empty()) {
        SyncPhaseTimer timer(sync_profiling_enabled_, sync_profile_.hit_nanoseconds);
        const auto started = std::chrono::steady_clock::now();
        static_cast<void>(hit_test_.refresh(dirty_->hit_test_nodes()));
        hit_test_refresh_nanoseconds_ += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count());
    }
    {
        SyncPhaseTimer timer(sync_profiling_enabled_, sync_profile_.focus_nanoseconds);
        focus_.synchronize();
    }
    dirty_->clear();
    if (text_edit_) {
        text_edit_->synchronize_pointer_cursor();
    }
    input_layout_ = InputLayoutContext{viewport, clip, origin, gap, unbounded_root_height};
    ++successful_sync_revision_;
    return true;
}

void WindowComponentServices::set_sync_profiling_enabled(bool enabled) noexcept {
    sync_profiling_enabled_ = enabled;
    text_.set_sync_profiling_enabled(enabled);
}

void WindowComponentServices::reset_sync_profile() noexcept {
    sync_profile_ = {};
    text_.reset_sync_profile();
}

} // namespace ryn::detail
