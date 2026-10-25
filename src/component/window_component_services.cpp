#include "component/window_component_services.hpp"
#include "component/typography_component.hpp"
#include "component/divider_component.hpp"
#include "component/space_addon.hpp"
#include "component/slider_component.hpp"
#include "component/tooltip_component.hpp"
#include "component/otp_component.hpp"
#include "component/select_component.hpp"
#include "component/date_picker_component.hpp"
#include "component/time_picker_component.hpp"
#include "component/card_component.hpp"
#include "component/tabs_component.hpp"
#include "component/collapse_component.hpp"
#include "component/segmented_component.hpp"
#include "component/form_component.hpp"
#include "component/descriptions_component.hpp"
#include "component/breadcrumb_component.hpp"
#include "component/steps_component.hpp"
#include "component/skeleton_component.hpp"
#include "component/modal_component.hpp"
#include "component/tag_badge_component.hpp"
#include "component/popconfirm_component.hpp"
#include "component/popover_component.hpp"
#include "component/feedback_component.hpp"
#include "component/menu_component.hpp"
#include "component/dropdown_component.hpp"
#include "component/pagination_component.hpp"
#include "component/table_component.hpp"
#include "component/empty_result_component.hpp"

#include <chrono>

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
        pointer_participants_scratch_.assign(participants_.begin(), participants_.end());
        for (std::size_t index = 0; index < pointer_participants_scratch_.size(); ++index) {
            auto* participant = pointer_participants_scratch_[index];
            if (std::find(participants_.begin(), participants_.end(), participant) != participants_.end()) {
                participant->on_pointer_input(event, hit, origin);
            }
        }
    });
    focus_.set_command_filter([this](const input::KeyboardInputEvent& event) {
        for (auto it = participants_.rbegin(); it != participants_.rend(); ++it) {
            if ((*it)->on_keyboard_input(event)) {
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
    typography_ = std::make_unique<TypographyComponentHost>(*this);
    divider_ = std::make_unique<DividerComponentHost>(*this);
    space_addon_ = std::make_unique<SpaceAddonHost>(*this);
    slider_ = std::make_unique<SliderComponentHost>(*this);
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
    card_ = std::make_unique<CardComponentHost>(*this);
    tabs_ = std::make_unique<TabsComponentHost>(*this);
    collapse_ = std::make_unique<CollapseComponentHost>(*this);
    segmented_ = std::make_unique<SegmentedComponentHost>(*this);
    form_ = std::make_unique<FormComponentHost>(*this);
    descriptions_ = std::make_unique<DescriptionsComponentHost>(*this);
    breadcrumb_ = std::make_unique<BreadcrumbComponentHost>(*this);
    steps_ = std::make_unique<StepsComponentHost>(*this);
    skeleton_ = std::make_unique<SkeletonComponentHost>(*this);
    tag_badge_ = std::make_unique<TagBadgeComponentHost>(*this);
    feedback_ = std::make_unique<FeedbackComponentHost>(*this);
    menu_ = std::make_unique<MenuComponentHost>(*this);
    pagination_ = std::make_unique<PaginationComponentHost>(*this);
    empty_result_ = std::make_unique<EmptyResultComponentHost>(*this);
}

WindowComponentServices::~WindowComponentServices() {
    dispose();
}

void WindowComponentServices::attach(WindowComponentParticipant& participant) {
    if (std::find(participants_.begin(), participants_.end(), &participant) != participants_.end()) {
        throw std::logic_error("window component participant is already attached");
    }
    pointer_participants_scratch_.reserve(participants_.size() + 1);
    participants_.push_back(&participant);
}

void WindowComponentServices::attach_input_host(WindowComponentParticipant& participant) {
    if (input_host_ != nullptr) {
        throw std::logic_error("window already has an Input component host");
    }
    attach(participant);
    input_host_ = &participant;
}

void WindowComponentServices::detach_input_host(WindowComponentParticipant& participant) noexcept {
    if (input_host_ == &participant) {
        input_host_ = nullptr;
    }
    detach(participant);
}

WindowTextEditServices& WindowComponentServices::bind_text_edit(input::TextInputPlatform& platform,
                                                                input::TextClipboard& clipboard) {
    if (!text_edit_) {
        text_edit_ = std::make_unique<WindowTextEditServices>(platform, clipboard);
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

void WindowComponentServices::detach(WindowComponentParticipant& participant) noexcept {
    std::erase(participants_, &participant);
}

bool WindowComponentServices::dispatch_scroll(const input::ScrollInputEvent& event) {
    if (!input::is_valid(event) || !focus_.state().window_active) {
        return false;
    }
    for (auto it = participants_.rbegin(); it != participants_.rend(); ++it) {
        if ((*it)->on_scroll_input(event)) {
            return true;
        }
    }
    return time_picker_->consume_scroll_boundary(event) || date_picker_->consume_scroll_boundary(event) ||
           dropdown_->consume_scroll_boundary(event) || popover_->consume_scroll_boundary(event) ||
           interactions_.input_boundary().has_value();
}

bool WindowComponentServices::set_input_boundary(std::optional<runtime::ComponentId> root) {
    const bool changed = interactions_.set_input_boundary(root);
    return reconcile_input_boundary() || changed;
}

bool WindowComponentServices::set_input_blocked(runtime::ComponentId root, bool blocked) {
    const bool changed = interactions_.set_input_blocked(root, blocked);
    return reconcile_input_boundary() || changed;
}

void WindowComponentServices::request_frame() noexcept {
    frame_requests_->request_frame();
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
    if (const auto root = interactions_.input_boundary(); root && !components().contains(*root)) {
        interactions_.set_input_boundary({});
    }
    const auto revision = interactions_.input_boundary_revision();
    if (revision == reconciled_input_boundary_revision_) {
        return false;
    }
    reconciled_input_boundary_revision_ = revision;
    scene_structure_dirty_ = true;
    for (auto root : components().root_components()) {
        dirty_->invalidate(components().root(root), runtime::DirtyFlags::HitTest);
    }
    // Cancel pointer state without silently removing focus inside a keyboard
    // command. FocusManager's outer sanitize/deferred transfer delivers blur.
    const auto order = interactions_.declaration_order();
    const std::vector<input::InteractionId> interactions{order.begin(), order.end()};
    for (auto interaction : interactions) {
        if (!interactions_.branch_active(interaction)) {
            pointer_.cancel_pointer_interaction(interaction);
            if (interactions_.input_boundary_revision() != revision) {
                return true;
            }
        }
    }
    focus_.synchronize();
    if (interactions_.input_boundary_revision() != revision) {
        return true;
    }
    const auto participants = participants_;
    for (auto* participant : participants) {
        if (std::find(participants_.begin(), participants_.end(), participant) != participants_.end()) {
            participant->on_input_boundary_changed();
            if (interactions_.input_boundary_revision() != revision) {
                return true;
            }
        }
    }
    focus_.synchronize();
    return true;
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
    std::vector<std::pair<WindowComponentParticipant*, void*>> active;
    active.reserve(participants_.size());
    try {
        for (auto* participant : participants_) {
            active.emplace_back(participant, participant->begin_mount());
        }
        text_.append_slot(parent, content);
        scene_structure_dirty_ = true;
    } catch (...) {
        for (auto* participant : participants_) {
            participant->on_destroy();
        }
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
    reconcile_input_boundary();
    scene_structure_dirty_ = true;
    return true;
}

void WindowComponentServices::dispose() noexcept {
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
    auto& text_scene = text_.scene_service();
    for (auto* participant : participants_) {
        participant->prepare_auxiliary_layout();
    }

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
        if (surfaces_.compact_effects({0.0F, 0.0F, viewport.width, viewport.height}, modal_->visible())) {
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
    const auto hit_clip = modal_->visible() ? runtime::Rect{0, 0, viewport.width, viewport.height} : clip;
    if (hit_clip_ != hit_clip) {
        scene_structure_dirty_ = true;
        hit_clip_ = hit_clip;
    }
    if (scene_structure_dirty_ || text_fragments_changed || scene_composer_.needs_rebuild()) {
        SyncPhaseTimer timer(sync_profiling_enabled_, sync_profile_.composer_nanoseconds);
        scene_composer_.rebuild(hit_clip);
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
