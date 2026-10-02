#include "component/window_component_services.hpp"
#include "component/typography_component.hpp"
#include "component/divider_component.hpp"
#include "component/slider_component.hpp"

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
    : nodes_(&nodes), layout_(&layout), dirty_(&dirty),
      text_(nodes, layout, dirty, text_scene, std::move(font_resolver)), interactions_(text_.components(), nodes),
      hit_test_(interactions_, nodes), scene_composer_(text_.components(), interactions_, hit_test_),
      surfaces_(text_.components(), nodes, scene_composer_), focus_(interactions_, &frame_requests),
      pointer_(interactions_, hit_test_, &frame_requests, &focus_) {
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
    animations_.reserve(256, 64, 256);
    animations_.set_schedule_observer(&frame_requests);
    typography_ = std::make_unique<TypographyComponentHost>(*this);
    divider_ = std::make_unique<DividerComponentHost>(*this);
    slider_ = std::make_unique<SliderComponentHost>(*this);
}

WindowComponentServices::~WindowComponentServices() {
    dispose();
}

void WindowComponentServices::attach(WindowComponentParticipant& participant) {
    if (std::find(participants_.begin(), participants_.end(), &participant) != participants_.end()) {
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

bool WindowComponentServices::destroy(runtime::ComponentId id) {
    if (!text_.destroy(id)) {
        return false;
    }
    for (auto* participant : participants_) {
        participant->on_destroy();
    }
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
    scene_structure_dirty_ = true;
}

void WindowComponentServices::set_window_active(bool active) {
    if (!active) {
        pointer_.cancel_all();
    }
    focus_.set_window_active(active);
    for (auto* participant : participants_) {
        participant->on_window_active(active);
    }
}

void WindowComponentServices::set_motion_preference(animation::MotionPreference preference) {
    if (motion_preference_ == preference) {
        return;
    }
    motion_preference_ = preference;
    for (auto* participant : participants_) {
        participant->synchronize_auxiliary_motion();
    }
}

std::size_t WindowComponentServices::tick_animations(animation::AnimationTime frame_time) {
    animation_time_ = frame_time;
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
        if (surfaces_.compact_effects({0.0F, 0.0F, viewport.width, viewport.height})) {
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
    if (scene_structure_dirty_ || text_fragments_changed) {
        SyncPhaseTimer timer(sync_profiling_enabled_, sync_profile_.composer_nanoseconds);
        scene_composer_.rebuild(clip);
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
