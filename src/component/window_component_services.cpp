#include "component/window_component_services.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ryn::detail {

WindowComponentServices::WindowComponentServices(
    runtime::NodeStore& nodes,
    layout::LayoutEngine& layout,
    runtime::DirtyQueues& dirty,
    TextSceneService& text_scene,
    std::vector<font::FontIdentity> default_font_chain,
    runtime::FrameRequestState& frame_requests)
    : WindowComponentServices(
          nodes,
          layout,
          dirty,
          text_scene,
          [chain = std::move(default_font_chain)](
              SystemFontFamily,
              std::uint32_t,
              std::uint32_t) { return chain; },
          frame_requests) {}

WindowComponentServices::WindowComponentServices(
    runtime::NodeStore& nodes,
    layout::LayoutEngine& layout,
    runtime::DirtyQueues& dirty,
    TextSceneService& text_scene,
    ThemeFontResolver font_resolver,
    runtime::FrameRequestState& frame_requests)
    : nodes_(&nodes),
      layout_(&layout),
      dirty_(&dirty),
      text_(
          nodes,
          layout,
          dirty,
          text_scene,
          std::move(font_resolver)),
      interactions_(text_.components(), nodes),
      hit_test_(interactions_, nodes),
      scene_composer_(text_.components(), interactions_, hit_test_),
      surfaces_(text_.components(), nodes, scene_composer_),
      focus_(interactions_, &frame_requests),
      pointer_(interactions_, hit_test_, &frame_requests, &focus_) {
    text_.attach_component_scene(scene_composer_);
    animations_.reserve(256, 64, 256);
}

WindowComponentServices::~WindowComponentServices() {
    dispose();
}

void WindowComponentServices::attach(WindowComponentParticipant& participant) {
    if (std::find(participants_.begin(), participants_.end(), &participant)
            != participants_.end()) {
        throw std::logic_error("window component participant is already attached");
    }
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
    if (input_host_ == &participant) input_host_ = nullptr;
    detach(participant);
}

WindowTextEditServices& WindowComponentServices::bind_text_edit(
    input::TextInputPlatform& platform, input::TextClipboard& clipboard) {
    if (!text_edit_) {
        text_edit_ = std::make_unique<WindowTextEditServices>(platform, clipboard);
    } else if (!text_edit_->uses(platform, clipboard)) {
        throw std::logic_error("window text edit services are bound to different platform ports");
    }
    return *text_edit_;
}

void WindowComponentServices::detach(WindowComponentParticipant& participant) noexcept {
    std::erase(participants_, &participant);
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
        for (auto* participant : participants_) participant->on_destroy();
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
    if (!text_.destroy(id)) return false;
    for (auto* participant : participants_) participant->on_destroy();
    scene_structure_dirty_ = true;
    return true;
}

void WindowComponentServices::dispose() noexcept {
    if (!components().active()) return;
    try { pointer_.cancel_all(); } catch (...) {}
    text_.dispose();
    for (auto* participant : participants_) participant->on_dispose();
    scene_structure_dirty_ = true;
}

void WindowComponentServices::set_window_active(bool active) {
    if (!active) pointer_.cancel_all();
    focus_.set_window_active(active);
}

void WindowComponentServices::set_motion_preference(animation::MotionPreference preference) {
    if (motion_preference_ == preference) return;
    motion_preference_ = preference;
    for (auto* participant : participants_) participant->synchronize_auxiliary_motion();
}

std::size_t WindowComponentServices::tick_animations(animation::AnimationTime frame_time) {
    animation_time_ = frame_time;
    auto changed = animations_.tick(frame_time);
    for (auto* participant : participants_) changed += participant->tick_auxiliary(frame_time);
    return changed;
}

std::optional<animation::AnimationTime> WindowComponentServices::next_frame_deadline() const {
    auto next = animations_.next_deadline();
    for (const auto* participant : participants_) {
        const auto candidate = participant->next_auxiliary_deadline();
        if (candidate && (!next || *candidate < *next)) next = candidate;
    }
    return next;
}

bool WindowComponentServices::layout_and_synchronize(
    runtime::Size viewport, runtime::Rect clip, runtime::Point origin,
    float gap, bool unbounded_root_height) {
    if (!text_.layout_and_synchronize(
            viewport, clip, origin, gap, false, unbounded_root_height)) return false;
    for (auto* participant : participants_) {
        participant->synchronize_auxiliary_geometry(viewport, clip);
    }
    if (surfaces_.compact_effects({0.0F, 0.0F, viewport.width, viewport.height})) {
        scene_structure_dirty_ = true;
    }
    const bool text_fragments_changed = text_.synchronize_scene_fragments(
        [](runtime::ComponentId) { return std::optional<input::InteractionId>{}; });
    for (auto* participant : participants_) {
        if (participant->synchronize_auxiliary_fragments()) scene_structure_dirty_ = true;
    }
    if (scene_structure_dirty_ || text_fragments_changed) {
        scene_composer_.rebuild(clip);
        scene_structure_dirty_ = false;
    } else if (text_.layout_performed_last_sync()) {
        for (const auto interaction : interactions_.declaration_order()) {
            static_cast<void>(hit_test_.refresh_interaction(interaction));
        }
    } else if (!dirty_->hit_test_nodes().empty()) {
        static_cast<void>(hit_test_.refresh(dirty_->hit_test_nodes()));
    }
    focus_.synchronize();
    dirty_->clear();
    return true;
}

} // namespace ryn::detail
