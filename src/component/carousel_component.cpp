#include "component/carousel_component.hpp"
#include "component/form_control_binding.hpp"
#include "animation/easing.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/button.hpp>
#include <ryn/text.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>

namespace ryn::detail {
struct CarouselRefState final {
    std::thread::id owner{std::this_thread::get_id()};
    std::uint64_t generation{};
    std::function<bool(std::size_t, bool)> go;
    std::function<bool(int)> step;
    std::function<bool(bool)> playback;
    std::function<bool()> focus;

    void check() const {
        if (owner != std::this_thread::get_id()) {
            throw std::logic_error("CarouselRef requires its owner thread");
        }
    }
};

namespace {
thread_local CarouselComponentHost* active_carousel{};

struct Slide final {
    runtime::NodeId node;
    runtime::ComponentId body;
};

struct Dot final {
    runtime::NodeId node;
    Signal<CarouselDotInfo> info{CarouselDotInfo{}};
    Signal<bool> disabled{false};
    component::RetainedSurfaceId surface;
};

struct Arrow final {
    Signal<CarouselArrowInfo> info{CarouselArrowInfo{}};
    Signal<bool> disabled{false};
};

struct CarouselState final {
    CarouselProps props;
    runtime::NodeId node;
    runtime::ComponentId viewport;
    input::InteractionId interaction;
    component::RetainedSurfaceId surface;
    component::RetainedSurfaceId background;
    std::vector<CarouselSlide> descriptors;
    std::vector<CarouselSlide> pending;
    std::vector<runtime::ComponentId> slides;
    std::vector<runtime::ComponentId> dots;
    std::array<runtime::ComponentId, 2> arrows;
    std::vector<runtime::Size> sizes;
    std::vector<runtime::Size> dot_sizes;
    std::array<runtime::Size, 2> arrow_sizes;
    std::optional<std::size_t> current;
    std::optional<std::size_t> previous;
    std::optional<std::size_t> preview;
    std::optional<std::size_t> requested;
    // A synchronous request owns this stack ticket; accepted callbacks may move authority again.
    bool* request_acceptance{};
    bool requested_instant{};
    int requested_direction{};
    std::optional<std::size_t> property;
    std::optional<std::size_t> after_pending;
    String key;
    bool pending_items{true};
    bool updating{};
    bool dispatching{};
    bool requesting{};
    std::uint64_t items_generation{};
    bool infinite{true};
    bool adaptive{};
    bool show_arrows{};
    bool show_dots{true};
    bool progress_dots{};
    bool vertical{};
    bool rtl{};
    bool draggable{};
    bool swipe{true};
    bool autoplay{};
    bool pause_hover{true};
    bool pause_focus{true};
    bool wait{};
    bool animated{true};
    bool visible{true};
    bool disabled{};
    bool hovered{};
    std::optional<runtime::ComponentId> hovered_component;
    bool manually_paused{};
    bool playing_override{};
    bool scheduled_paused{true};
    std::optional<bool> motion_enabled;
    bool transition{};
    bool dragging{};
    int direction{1};
    float threshold{5};
    float progress{1};
    float resume_progress{};
    float initial_progress{};
    float autoplay_progress{};
    float drag{};
    runtime::Point drag_origin;
    std::optional<input::PointerIdentity> pointer;
    CarouselEffect effect{CarouselEffect::ScrollX};
    animation::Easing easing{animation::Easing::cubic_bezier(CubicBezier{0.25F, 0.1F, 0.25F, 1})};
    CarouselDotPlacement placement{CarouselDotPlacement::Bottom};
    animation::AnimationDuration speed{animation::AnimationDuration::milliseconds(500)};
    animation::AnimationDuration transition_speed{animation::AnimationDuration::milliseconds(500)};
    animation::AnimationDuration interval{animation::AnimationDuration::milliseconds(3000)};
    animation::AnimationDuration remaining{animation::AnimationDuration::milliseconds(3000)};
    animation::AnimationTime start;
    animation::AnimationTime last;
    std::optional<animation::AnimationTime> deadline;
    runtime::Rect bounds;
    runtime::Rect viewport_bounds;
    runtime::Rect clip;
    Signal<runtime::SemanticForeground> color{{1, 1, 1, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 3> subscriptions;
};

struct CarouselFlagGuard final {
    WindowComponentServices& services;
    runtime::ComponentId id;
    bool CarouselState::* field;
    bool previous;

    CarouselFlagGuard(WindowComponentServices& s, runtime::ComponentId owner, bool CarouselState::* member)
        : services(s), id(owner), field(member), previous(s.components().state<CarouselState>(owner)->*member) {
        s.components().state<CarouselState>(owner)->*field = true;
    }

    ~CarouselFlagGuard() {
        if (auto* current = services.components().state<CarouselState>(id)) {
            current->*field = previous;
        }
    }
};

bool descendant(const runtime::ComponentHost& host, runtime::ComponentId child, runtime::ComponentId ancestor) {
    for (auto current = std::optional{child}; current && host.contains(*current); current = host.parent(*current)) {
        if (*current == ancestor) {
            return true;
        }
    }
    return false;
}

std::optional<std::size_t> key_index(const CarouselState& s, const String& key) {
    for (std::size_t i = 0; i < s.descriptors.size(); ++i) {
        if (s.descriptors[i].key() == key) {
            return i;
        }
    }
    return {};
}

void validate(CarouselEffect v) {
    if (v != CarouselEffect::ScrollX && v != CarouselEffect::Fade) {
        throw std::invalid_argument("Invalid Carousel effect");
    }
}

void validate(CarouselDotPlacement v) {
    if (v < CarouselDotPlacement::Top || v > CarouselDotPlacement::End) {
        throw std::invalid_argument("Invalid Carousel dot placement");
    }
}

animation::AnimationDuration duration(Duration v, bool positive) {
    const auto ms = v.count_milliseconds();
    if (!std::isfinite(ms) || ms < 0 || (positive && ms == 0)) {
        throw std::invalid_argument("Carousel duration must be finite and bounded");
    }
    const auto result = animation::AnimationDuration::milliseconds(ms);
    if (positive && result.count_microseconds() == 0) {
        throw std::invalid_argument("Carousel interval is below clock resolution");
    }
    return result;
}

runtime::SemanticForeground fg(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

} // namespace

CarouselComponentHost::CarouselComponentHost(WindowComponentServices& s) : services_(&s) {
    s.attach(*this);
}

CarouselComponentHost::~CarouselComponentHost() {
    services_->detach(*this);
}

void* CarouselComponentHost::begin_mount() noexcept {
    return std::exchange(active_carousel, this);
}

void CarouselComponentHost::end_mount(void* old) noexcept {
    active_carousel = static_cast<CarouselComponentHost*>(old);
}

void CarouselComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

CarouselSnapshot CarouselComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<CarouselState>(id);
    if (!s) {
        throw std::out_of_range("Carousel is stale");
    }
    const auto translation = services_->nodes().require(s->node).translation;
    const auto visible =
        graphics::intersect_effect_bounds({s->viewport_bounds.x + translation.x, s->viewport_bounds.y + translation.y,
                                           s->viewport_bounds.width, s->viewport_bounds.height},
                                          s->clip);
    return {s->current,           s->key,        s->slides,   s->dots,   s->arrows, visible, s->progress,
            s->autoplay_progress, s->transition, s->dragging, paused(id)};
}

bool CarouselComponentHost::paused(runtime::ComponentId id) const {
    const auto* s = services_->components().state<CarouselState>(id);
    if (!s || !s->visible || s->disabled || s->manually_paused || !(s->autoplay || s->playing_override) ||
        s->slides.size() < 2 || s->pointer || s->transition || s->viewport_bounds.width <= 0 ||
        s->viewport_bounds.height <= 0 || s->clip.width <= 0 || s->clip.height <= 0 ||
        !services_->focus().state().window_active || !services_->interactions().component_active(id) ||
        !animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                          services_->motion_preference())
             .enabled()) {
        return true;
    }
    if (s->pause_hover && s->hovered) {
        return true;
    }
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    return s->pause_focus && record && descendant(services_->components(), record->component, id);
}

void CarouselComponentHost::schedule(runtime::ComponentId id, bool reset) {
    auto* s = services_->components().state<CarouselState>(id);
    if (!s) {
        return;
    }
    const auto now = std::max(s->last, services_->animation_time());
    s->last = now;
    if (reset) {
        s->remaining = s->interval;
        s->deadline.reset();
        s->autoplay_progress = 0;
    }
    const bool stop = paused(id) || (!s->infinite && s->current && *s->current + 1 == s->slides.size());
    if (stop) {
        if (s->deadline) {
            s->remaining = *s->deadline > now ? *s->deadline - now : animation::AnimationDuration{};
        }
        s->deadline.reset();
    } else if (!s->deadline) {
        s->deadline = now + s->remaining;
    }
    s->scheduled_paused = stop;
}

void CarouselComponentHost::cancel_gesture(runtime::ComponentId id) {
    auto* s = services_->components().state<CarouselState>(id);
    if (!s) {
        return;
    }
    const bool captured = s->pointer.has_value();
    s->pointer.reset();
    s->dragging = false;
    s->drag = 0;
    s->preview.reset();
    if (captured) {
        services_->pointer().cancel_pointer_interaction(s->interaction);
    }
    if (services_->components().contains(id)) {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry |
                                                   runtime::DirtyFlags::HitTest);
        schedule(id);
    }
}

namespace {
bool slide_receives_input(WindowComponentServices& services, const CarouselState& s, runtime::ComponentId owner,
                          runtime::ComponentId slide, std::size_t index) {
    if (!s.visible || s.disabled || !services.components().branch_active(owner) || s.current != index ||
        s.viewport_bounds.width <= 0 || s.viewport_bounds.height <= 0 || !services.focus().state().window_active ||
        !services.components().contains(slide)) {
        return false;
    }
    const auto& node = services.nodes().require(services.components().root(slide));
    const auto bounds = graphics::intersect_effect_bounds(
        {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width, node.bounds.height},
        s.clip);
    return bounds.width > 0 && bounds.height > 0 && node.paint_opacity > 0;
}
} // namespace

void CarouselComponentHost::refresh_clip(runtime::ComponentId id, runtime::Size viewport, runtime::Rect clip) {
    if (auto* owner = services_->components().state<CarouselState>(id)) {
        const auto& node = services_->nodes().require(owner->node);
        const auto r = owner->viewport_bounds;
        owner->clip =
            graphics::intersect_effect_bounds({r.x + node.translation.x, r.y + node.translation.y, r.width, r.height},
                                              services_->component_clip(id, viewport, clip));
    }
}

bool CarouselComponentHost::refresh_target_admission(runtime::ComponentId target, runtime::Size viewport,
                                                     runtime::Rect document_clip) {
    auto& host = services_->components();
    std::optional<runtime::ComponentId> ceiling;
    const auto bound = host.component_count();
    // Two bounded ancestry walks per nested producer avoid copying mounted/slides or allocating a per-query path.
    for (std::size_t pass = 0; pass <= bound; ++pass) {
        if (!host.contains(target) || !services_->interactions().component_lifetime_active(target)) {
            return false;
        }
        std::optional<runtime::ComponentId> owner;
        std::optional<runtime::ComponentId> slide;
        for (auto current = std::optional{target}; current && current != ceiling; current = host.parent(*current)) {
            if (!host.contains(*current)) {
                return false;
            }
            if (!host.state<Slide>(*current)) {
                continue;
            }
            const auto parent = host.parent(*current);
            const auto candidate = parent ? host.parent(*parent) : std::nullopt;
            if (candidate && candidate != ceiling && host.state<CarouselState>(*candidate)) {
                owner = candidate;
                slide = current;
            }
        }
        if (!owner) {
            return true;
        }
        auto* state = host.state<CarouselState>(*owner);
        if (!host.scope(*owner).active()) {
            return false;
        }
        const auto found = std::ranges::find(state->slides, *slide);
        if (found == state->slides.end()) {
            return false;
        }
        const auto index = static_cast<std::size_t>(found - state->slides.begin());
        const auto generation = state->items_generation;
        const auto current = state->current;
        const auto property = state->property;
        const auto transition = state->transition;
        const auto visible = state->visible;
        const auto disabled = state->disabled;
        refresh_clip(*owner, viewport, document_clip);
        const bool blocked = !slide_receives_input(*services_, *state, *owner, *slide, index);
        services_->set_input_blocked(*slide, blocked);
        // The setter reconciles boundaries and may run callbacks, retire a producer, or accept new state.
        state = host.state<CarouselState>(*owner);
        if (!host.contains(target) || !services_->interactions().component_lifetime_active(target) || !state ||
            !host.scope(*owner).active() || state->items_generation != generation || state->current != current ||
            state->property != property || state->transition != transition || state->visible != visible ||
            state->disabled != disabled || index >= state->slides.size() || state->slides[index] != *slide ||
            !host.contains(*slide)) {
            return false;
        }
        ceiling = owner;
    }
    throw std::logic_error("Carousel target ancestry did not converge");
}

void CarouselComponentHost::gates(runtime::ComponentId id) {
    auto* s = services_->components().state<CarouselState>(id);
    if (!s) {
        return;
    }
    const auto slides = s->slides;
    const auto current = s->current;
    const auto previous = s->previous;
    const bool shown = s->visible && services_->components().branch_active(id);
    for (std::size_t i = 0; i < slides.size(); ++i) {
        if (!services_->components().contains(id) || !services_->components().contains(slides[i])) {
            return;
        }
        const bool paint =
            shown && (current == i || (s->transition && previous == i) || (s->dragging && s->preview == i));
        if (services_->components().set_branch_active(slides[i], paint)) {
            services_->mark_scene_structure_dirty();
        }
        services_->set_input_blocked(slides[i], !slide_receives_input(*services_, *s, id, slides[i], i));
        s = services_->components().state<CarouselState>(id);
        if (!s) {
            return;
        }
    }
}

void CarouselComponentHost::finish(runtime::ComponentId id, bool notify) {
    auto* s = services_->components().state<CarouselState>(id);
    if (!s || !s->transition) {
        return;
    }
    CarouselFlagGuard callback_guard{*services_, id, &CarouselState::dispatching};
    const auto accepted = s->current;
    const auto callback = s->props.after_;
    s->transition = false;
    s->previous.reset();
    s->progress = 1;
    s->drag = 0;
    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                               runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Material |
                                               runtime::DirtyFlags::HitTest);
    gates(id);
    schedule(id, true);
    if (notify && accepted && callback && services_->components().contains(id)) {
        try {
            callback(*accepted);
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
    }
}

bool CarouselComponentHost::accept(runtime::ComponentId id, std::size_t target, bool instant, int direction) {
    auto* s = services_->components().state<CarouselState>(id);
    if (!s || s->updating || s->dispatching || target >= s->slides.size()) {
        return false;
    }
    if (s->current == target && s->key == s->descriptors[target].key()) {
        return false;
    }
    CarouselFlagGuard callback_guard{*services_, id, &CarouselState::dispatching};
    if (s->transition) {
        finish(id);
    }
    s = services_->components().state<CarouselState>(id);
    if (!s) {
        return false;
    }
    const auto old = s->current;
    const auto before = s->props.before_;
    const auto key = s->descriptors[target].key();
    try {
        if (old && before) {
            before(*old, target);
        }
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
    s = services_->components().state<CarouselState>(id);
    if (!s || target >= s->slides.size() || s->descriptors[target].key() != key) {
        return false;
    }
    s->previous = old;
    if (s->requested == target && s->request_acceptance) {
        *s->request_acceptance = true;
    }
    s->current = target;
    s->key = key;
    if (!direction) {
        direction = old && target < *old ? -1 : 1;
    }
    s->direction = direction;
    s->start = std::max(s->last, services_->animation_time());
    const bool motion = !instant && old && s->animated && s->speed.count_microseconds() > 0 && s->visible &&
                        s->clip.width > 0 && s->clip.height > 0 && services_->focus().state().window_active &&
                        animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                                         services_->motion_preference())
                            .enabled();
    s->initial_progress = motion ? std::clamp(s->resume_progress, 0.0F, 1.0F) : 1;
    s->resume_progress = 0;
    s->transition_speed = animation::AnimationDuration::microseconds(
        std::int64_t(double(s->speed.count_microseconds()) * (1 - s->initial_progress)));
    s->transition = true;
    s->progress = motion ? 0 : 1;
    cancel_gesture(id);
    refresh(id);
    if (!motion) {
        finish(id);
    }
    return true;
}

bool CarouselComponentHost::request(runtime::ComponentId id, std::size_t target, CarouselChangeSource source,
                                    bool instant, int direction) {
    auto* s = services_->components().state<CarouselState>(id);
    if (!s || s->updating || s->requesting || s->dispatching || s->disabled || !s->visible ||
        !services_->focus().state().window_active || !services_->interactions().component_active(id) ||
        target >= s->slides.size() || (s->wait && s->transition)) {
        return false;
    }
    if (source != CarouselChangeSource::Ref && (s->clip.width <= 0 || s->clip.height <= 0)) {
        return false;
    }
    if (s->current == target) {
        return false;
    }
    CarouselFlagGuard request_guard{*services_, id, &CarouselState::requesting};
    schedule(id, true);
    const auto callback = s->props.request_;
    const bool controlled = s->props.current_.has_value();
    if (controlled) {
        bool accepted{};
        s->request_acceptance = &accepted;
        s->requested = target;
        s->requested_instant = instant;
        s->requested_direction = direction;
        try {
            if (callback) {
                callback(target, source);
            }
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
        s = services_->components().state<CarouselState>(id);
        if (s) {
            s->requested.reset();
            s->request_acceptance = nullptr;
        }
        return accepted;
    }
    const bool result = accept(id, target, instant, direction);
    if (callback && result && services_->components().contains(id)) {
        try {
            callback(target, source);
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
    }
    return result;
}

bool CarouselComponentHost::step(runtime::ComponentId id, int direction, CarouselChangeSource source) {
    const auto* s = services_->components().state<CarouselState>(id);
    if (!s || !s->current || s->slides.size() < 2) {
        return false;
    }
    const auto n = s->slides.size();
    const auto i = *s->current;
    if (!s->infinite && ((direction < 0 && i == 0) || (direction > 0 && i + 1 == n))) {
        return false;
    }
    return request(id, direction < 0 ? (i + n - 1) % n : (i + 1) % n, source, false, direction);
}

bool CarouselComponentHost::playback(runtime::ComponentId id, bool play) {
    auto* s = services_->components().state<CarouselState>(id);
    if (!s || !services_->interactions().component_active(id)) {
        return false;
    }
    s->manually_paused = !play;
    if (play) {
        s->playing_override = true;
    }
    schedule(id);
    services_->request_frame();
    return true;
}

bool CarouselComponentHost::focus(runtime::ComponentId id) {
    const auto* s = services_->components().state<CarouselState>(id);
    if (!s || !s->current || s->disabled || !s->visible || s->clip.width <= 0 || s->clip.height <= 0 ||
        !services_->focus().state().window_active || !services_->interactions().component_active(id)) {
        return false;
    }
    services_->focus().defer_focus(s->interaction, input::FocusModality::keyboard);
    return services_->components().contains(id);
}

void CarouselComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<CarouselState>(id);
    if (!s) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().carousel();
    s->color.set(fg(t.colors.arrow));
    s->font.set({t.metrics.font_family, 400, false, t.metrics.font_size, t.metrics.line_height});
    const bool active = s->visible && !s->disabled && s->current.has_value();
    services_->interactions().set_eligible(s->interaction, active);
    if (services_->components().set_branch_active(id, s->visible)) {
        services_->mark_scene_structure_dirty();
    }
    services_->nodes().require(s->node).out_of_flow = !s->visible;
    for (std::size_t i = 0; i < 2; ++i) {
        const auto* arrow = services_->components().state<Arrow>(s->arrows[i]);
        const bool disabled = !active || !s->current || s->slides.size() < 2 ||
                              (!s->infinite && (i == 0 ? *s->current == 0 : *s->current + 1 == s->slides.size()));
        if (arrow) {
            arrow->disabled.set(disabled);
            const auto info = arrow->info;
            info.set({i == 1, disabled});
        }
        s = services_->components().state<CarouselState>(id);
        if (!s) {
            return;
        }
        if (s->arrows[i].valid() &&
            services_->components().set_branch_active(s->arrows[i], s->show_arrows && s->slides.size() > 1)) {
            services_->mark_scene_structure_dirty();
        }
    }
    const auto dots = s->dots;
    for (std::size_t i = 0; i < dots.size(); ++i) {
        const auto* d = services_->components().state<Dot>(dots[i]);
        if (!d) {
            continue;
        }
        d->disabled.set(!active);
        if (services_->components().set_branch_active(dots[i], s->show_dots)) {
            services_->mark_scene_structure_dirty();
        }
        const auto info = d->info;
        const CarouselDotInfo meta{i, s->descriptors[i].key(), s->current == i,
                                   s->current == i ? s->autoplay_progress : 0};
        info.set(meta);
        s = services_->components().state<CarouselState>(id);
        if (!s) {
            return;
        }
    }
    gates(id);
    s = services_->components().state<CarouselState>(id);
    if (!s) {
        return;
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry |
                                                          runtime::DirtyFlags::Material | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material | runtime::DirtyFlags::Geometry);
    schedule(id);
    services_->set_input_blocked(id, s->disabled || !s->visible);
}

runtime::ComponentId CarouselComponentHost::slide(runtime::ComponentId id, const CarouselSlide& descriptor,
                                                  runtime::ComponentBuildContext* initial) {
    runtime::ComponentId result;
    const auto content = descriptor.content_;
    auto factory = Content{[&] {
        auto& b = runtime::require_component_build_context();
        result = b.mount_component<Slide>();
        auto& row = b.state<Slide>(result);
        row.node = b.root(result);
        services_->nodes().require(row.node).clip_content = true;
        services_->layout().set_layout(row.node, layout::BoxLayout{{}, true});
        b.on_resource_cleanup(result, [this, root = result, node = row.node] {
            services_->interactions().set_input_blocked(root, false);
            services_->layout().remove_layout(node);
        });
        b.mount_slot(result, Content{[content] { SlotContentAccess::function (*content)(); }});
    }};
    const auto viewport = services_->components().state<CarouselState>(id)->viewport;
    if (initial) {
        initial->mount_slot(viewport, factory);
    } else {
        services_->append_slot(viewport, factory);
    }
    return result;
}

runtime::ComponentId CarouselComponentHost::dot(runtime::ComponentId id, String key,
                                                runtime::ComponentBuildContext* initial) {
    runtime::ComponentId result;
    auto factory = Content{[&] {
        auto& b = runtime::require_component_build_context();
        result = b.mount_component<Dot>();
        auto& d = b.state<Dot>(result);
        d.node = b.root(result);
        const auto& owner = *services_->components().state<CarouselState>(id);
        const auto& pending = owner.pending;
        std::size_t index{};
        for (; index < pending.size() && pending[index].key() != key; ++index) {
        }
        std::size_t active = owner.props.current_ ? read_prop(*owner.props.current_)
                                                  : owner.current.value_or(owner.props.default_current_);
        if (!owner.props.current_ && !owner.key.empty()) {
            for (std::size_t i = 0; i < pending.size(); ++i) {
                if (pending[i].key() == owner.key) {
                    active = i;
                    break;
                }
            }
        }
        if (!pending.empty()) {
            active = std::min(active, pending.size() - 1);
        }
        d.info.set({index, key, index == active, 0});
        services_->layout().set_layout(d.node, layout::BoxLayout{{}, true});
        auto fragment = b.register_scene_fragment(result, runtime::SceneFragmentPlacement::before_children);
        d.surface = services_->surfaces().create_content_range(fragment, {});
        b.on_resource_cleanup(result, [this, root = result, node = d.node, surface = d.surface] {
            services_->interactions().set_input_blocked(root, false);
            services_->layout().remove_layout(node);
            services_->surfaces().destroy_content_range(surface);
        });
        b.mount_slot(result, Content{[this, id, key, info = d.info, disabled = d.disabled] {
                         FormPrivateEditorScope private_control;
                         const auto action = [this, id, key] {
                             const auto* s = services_->components().state<CarouselState>(id);
                             if (!s) {
                                 return;
                             }
                             const auto index = key_index(*s, key);
                             if (index) {
                                 request(id, *index, CarouselChangeSource::Dot);
                             }
                         };
                         const auto renderer = services_->components().state<CarouselState>(id)->props.dot_;
                         if (renderer) {
                             SlotContentAccess::function(renderer(bind([info] { return info.get(); }), action))();
                         } else {
                             Button(ButtonProps{}.type(ButtonType::Text).disabled(disabled).wave(false).onClick(action),
                                    [] {});
                         }
                     }});
    }};
    if (initial) {
        initial->mount_slot(id, factory);
    } else {
        services_->append_slot(id, factory);
    }
    return result;
}

void CarouselComponentHost::items(runtime::ComponentId id, runtime::ComponentBuildContext* initial) {
    auto* s = services_->components().state<CarouselState>(id);
    if (!s || !s->pending_items || s->updating) {
        return;
    }
    const auto pending = s->pending;
    const auto generation = s->items_generation;
    s->pending_items = false;
    for (std::size_t i = 0; i < pending.size(); ++i) {
        if (pending[i].key_.empty()) {
            throw std::invalid_argument("Carousel slide keys must be nonempty");
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (pending[j].key_ == pending[i].key_) {
                throw std::invalid_argument("Carousel slide keys must be unique");
            }
        }
    }
    if (s->transition) {
        finish(id);
    }
    cancel_gesture(id);
    s = services_->components().state<CarouselState>(id);
    if (!s || s->items_generation != generation) {
        return;
    }
    const auto old_descriptors = s->descriptors;
    const auto old_slides = s->slides;
    const auto old_dots = s->dots;
    const auto old_current = s->current;
    const auto old_key = s->key;
    s->updating = true;
    s->pending_items = false;
    std::vector<runtime::ComponentId> next_slides;
    std::vector<runtime::ComponentId> next_dots;
    std::vector<runtime::ComponentId> created;
    try {
        for (const auto& descriptor : pending) {
            const auto found =
                std::ranges::find_if(old_descriptors, [&](const auto& v) { return v.key_ == descriptor.key_; });
            const auto index = std::size_t(found - old_descriptors.begin());
            const bool retained = found != old_descriptors.end() && found->content_ == descriptor.content_;
            const auto child = retained ? old_slides[index] : slide(id, descriptor, initial);
            if (!retained) {
                created.push_back(child);
            }
            if (!services_->components().contains(id)) {
                throw std::runtime_error("Carousel factory retired owner");
            }
            const auto control = found != old_descriptors.end() ? old_dots[index] : dot(id, descriptor.key_, initial);
            if (found == old_descriptors.end()) {
                created.push_back(control);
            }
            if (!services_->components().contains(id)) {
                throw std::runtime_error("Carousel dot factory retired owner");
            }
            next_slides.push_back(child);
            next_dots.push_back(control);
        }
    } catch (...) {
        for (auto child : created) {
            services_->retire_failed_component(child);
        }
        if (auto* current = services_->components().state<CarouselState>(id)) {
            current->updating = false;
        }
        throw;
    }
    s = services_->components().state<CarouselState>(id);
    if (!s) {
        return;
    }
    if (s->items_generation != generation) {
        for (auto child : created) {
            services_->retire_failed_component(child);
        }
        if (auto* current = services_->components().state<CarouselState>(id)) {
            current->updating = false;
            services_->request_frame();
        }
        return;
    }
    s->descriptors = pending;
    s->slides = next_slides;
    s->dots = next_dots;
    s->sizes.resize(pending.size());
    s->dot_sizes.resize(pending.size());
    s->updating = false;
    const auto retained = key_index(*s, old_key);
    if (pending.empty()) {
        s->current.reset();
        s->key = {};
    } else if (s->props.current_) {
        const auto target = std::min(read_prop(*s->props.current_), pending.size() - 1);
        if (!old_current) {
            s->current = target;
            s->key = pending[target].key_;
        } else if (old_key == pending[target].key_) {
            s->current = target;
        } else {
            s->current = old_current;
            accept(id, target, true);
        }
    } else if (retained) {
        s->current = retained;
    } else if (old_current) {
        accept(id, std::min(*old_current, pending.size() - 1), true);
    } else {
        const auto first = std::min(s->props.default_current_, pending.size() - 1);
        s->current = first;
        s->key = pending[first].key_;
    }
    for (auto child : old_slides) {
        if (std::ranges::find(next_slides, child) == next_slides.end()) {
            services_->destroy(child);
        }
    }
    for (auto child : old_dots) {
        if (std::ranges::find(next_dots, child) == next_dots.end()) {
            services_->destroy(child);
        }
    }
    if (services_->components().contains(id)) {
        refresh(id);
        schedule(id, true);
    }
}

void CarouselComponentHost::mount(const CarouselProps& props) {
    validate(read_prop(props.effect_));
    validate(read_prop(props.placement_));
    const auto interval = duration(read_prop(props.autoplay_speed_), true);
    const auto speed = duration(read_prop(props.speed_), false);
    const auto threshold = read_prop(props.threshold_);
    if (!std::isfinite(threshold) || threshold <= 0) {
        throw std::invalid_argument("Carousel touchThreshold must be positive and finite");
    }
    auto& b = runtime::require_component_build_context();
    const auto id = b.mount_component<CarouselState>();
    auto& s = b.state<CarouselState>(id);
    s.node = b.root(id);
    s.props = props;
    s.interval = interval;
    s.remaining = interval;
    s.speed = speed;
    s.threshold = threshold;
    s.pending = read_prop(props.slides_);
    s.infinite = read_prop(props.infinite_);
    s.adaptive = read_prop(props.adaptive_height_);
    s.show_arrows = read_prop(props.arrows_);
    s.show_dots = read_prop(props.dots_);
    s.progress_dots = read_prop(props.progress_);
    s.placement = read_prop(props.placement_);
    s.vertical = props.vertical_
                     ? read_prop(*props.vertical_)
                     : (s.placement == CarouselDotPlacement::Start || s.placement == CarouselDotPlacement::End);
    s.rtl = read_prop(props.direction_) == TextDirection::RightToLeft;
    s.draggable = read_prop(props.draggable_);
    s.swipe = read_prop(props.swipe_);
    s.autoplay = read_prop(props.autoplay_);
    s.pause_hover = read_prop(props.pause_hover_);
    s.pause_focus = read_prop(props.pause_focus_);
    s.wait = read_prop(props.wait_);
    s.animated = read_prop(props.animated_);
    s.visible = read_prop(props.visible_);
    s.disabled = read_prop(props.disabled_);
    s.effect = read_prop(props.effect_);
    runtime::connect_layout_style(b.scope(id), props.layout_, s.node, services_->nodes(), services_->dirty());
    services_->nodes().require(s.node).clip_content = true;
    const auto reference = props.ref_.state_;
    reference->check();
    const auto generation = ++reference->generation;
    reference->go = [this, id](auto index, bool instant) {
        return request(id, index, CarouselChangeSource::Ref, instant);
    };
    reference->step = [this, id](int n) {
        return step(id, n, CarouselChangeSource::Ref);
    };
    reference->playback = [this, id](bool play) {
        return playback(id, play);
    };
    reference->focus = [this, id] {
        return focus(id);
    };
    s.interaction = services_->interactions().create({id, s.node, {}, true, true, {}, true, true});
    auto hit = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    services_->scene_composer().set_fragment(hit, {}, s.interaction);
    auto background = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.background = services_->surfaces().create_content_range(background, {});
    auto fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::after_children);
    s.surface = services_->surfaces().create_content_range(fragment, {});
    b.on_resource_cleanup(id, [this, id, node = s.node, interaction = s.interaction, hit, surface = s.surface,
                               background = s.background, reference, generation] {
        services_->interactions().set_input_blocked(id, false);
        if (reference->generation == generation) {
            ++reference->generation;
            reference->go = {};
            reference->step = {};
            reference->playback = {};
            reference->focus = {};
        }
        services_->pointer().cancel_interaction(interaction);
        services_->interactions().remove(interaction);
        services_->scene_composer().remove_fragment(hit);
        services_->surfaces().destroy_content_range(surface);
        services_->surfaces().destroy_content_range(background);
        services_->layout().remove_layout(node);
    });
    input::InteractionHandlers handlers;
    handlers.capture = [this, id](input::PointerDispatchContext& e) {
        if (auto* s = services_->components().state<CarouselState>(id)) {
            if (e.kind() == input::PointerEventKind::down && s->pointer && *s->pointer != e.event().pointer) {
                cancel_gesture(id);
            }
        }
    };
    handlers.target = [this, id](input::PointerDispatchContext& e) {
        auto* s = services_->components().state<CarouselState>(id);
        if (!s) {
            return;
        }
        if (e.kind() == input::PointerEventKind::enter || e.kind() == input::PointerEventKind::leave) {
            s->hovered = e.kind() == input::PointerEventKind::enter;
            schedule(id);
            return;
        }
        if (e.kind() == input::PointerEventKind::cancel) {
            cancel_gesture(id);
            return;
        }
        if (e.kind() == input::PointerEventKind::down && e.event().button == input::PointerButton::primary &&
            !s->disabled && s->visible) {
            const bool permitted = e.event().pointer.device == input::PointerDevice::touch ? s->swipe : s->draggable;
            if (!permitted || s->dispatching || (s->wait && s->transition)) {
                return;
            }
            if (s->pointer) {
                cancel_gesture(id);
                return;
            }
            if (s->transition) {
                finish(id);
            }
            s = services_->components().state<CarouselState>(id);
            if (!s) {
                return;
            }
            s->pointer = e.event().pointer;
            s->drag_origin = {e.event().x, e.event().y};
            s->drag = 0;
            static_cast<void>(e.capture_pointer());
            e.stop_propagation();
            schedule(id);
            return;
        }
        if (!s->pointer || *s->pointer != e.event().pointer) {
            return;
        }
        const auto axis = s->vertical ? e.event().y - s->drag_origin.y : e.event().x - s->drag_origin.x;
        const auto cross = s->vertical ? e.event().x - s->drag_origin.x : e.event().y - s->drag_origin.y;
        const auto extent = s->vertical ? s->viewport_bounds.height : s->viewport_bounds.width;
        if (e.kind() == input::PointerEventKind::move) {
            if (!s->dragging && std::abs(cross) > std::abs(axis) && std::abs(cross) > 8) {
                cancel_gesture(id);
                return;
            }
            s->dragging = s->dragging || std::abs(axis) > 8;
            if (s->dragging) {
                auto offset = std::clamp(axis, -extent, extent);
                const int direction = (axis < 0 ? 1 : -1) * (!s->vertical && s->rtl ? -1 : 1);
                if (!s->infinite && s->current &&
                    ((direction < 0 && *s->current == 0) || (direction > 0 && *s->current + 1 == s->slides.size()))) {
                    offset = 0;
                }
                s->drag = offset;
                s->preview.reset();
                if (s->current && s->slides.size() > 1 && offset != 0) {
                    const auto n = s->slides.size();
                    s->preview = direction < 0 ? (*s->current + n - 1) % n : (*s->current + 1) % n;
                }
                gates(id);
                s = services_->components().state<CarouselState>(id);
                if (!s) {
                    return;
                }
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry |
                                                           runtime::DirtyFlags::HitTest);
            }
            e.stop_propagation();
            return;
        }
        if (e.kind() == input::PointerEventKind::up && e.event().button == input::PointerButton::primary) {
            const bool accepted = s->dragging && extent > 0 && std::abs(axis) >= std::max(8.0F, extent / s->threshold);
            const int direction = (axis < 0 ? 1 : -1) * (!s->vertical && s->rtl ? -1 : 1);
            s->pointer.reset();
            s->dragging = false;
            s->drag = 0;
            s->preview.reset();
            static_cast<void>(e.release_pointer_capture());
            e.stop_propagation();
            if (accepted) {
                s->resume_progress = std::min(1.0F, std::abs(axis) / extent);
                step(id, direction, CarouselChangeSource::Drag);
            }
            if (auto* current = services_->components().state<CarouselState>(id)) {
                current->resume_progress = 0;
            }
            refresh(id);
            schedule(id, true);
        }
    };
    services_->interactions().set_handlers(s.interaction, std::move(handlers));
    b.mount_slot(
        id, Content{[&] {
            auto& c = runtime::require_component_build_context();
            s.viewport = c.mount_component<int>(0);
            const auto viewport_node = c.root(s.viewport);
            services_->nodes().require(viewport_node).clip_content = true;
            services_->layout().set_layout(viewport_node, layout::LeafLayout{});
            c.on_resource_cleanup(s.viewport,
                                  [this, viewport_node] { services_->layout().remove_layout(viewport_node); });
            for (std::size_t i = 0; i < 2; ++i) {
                const auto arrow = c.mount_component<Arrow>();
                s.arrows[i] = arrow;
                const auto node = c.root(arrow);
                auto& a = c.state<Arrow>(arrow);
                a.info.set({i == 1, false});
                services_->layout().set_layout(node, layout::BoxLayout{{}, true});
                c.on_resource_cleanup(arrow, [this, arrow, node] {
                    services_->interactions().set_input_blocked(arrow, false);
                    services_->layout().remove_layout(node);
                });
                c.mount_slot_with_semantic_text_style(
                    arrow, Content{[this, id, i, info = a.info, disabled = a.disabled] {
                        FormPrivateEditorScope private_control;
                        const auto action = [this, id, i] {
                            step(id, i == 0 ? -1 : 1, CarouselChangeSource::Arrow);
                        };
                        const auto& props = services_->components().state<CarouselState>(id)->props;
                        const auto renderer = i == 0 ? props.prev_ : props.next_;
                        if (renderer) {
                            SlotContentAccess::function(renderer(bind([info] { return info.get(); }), action))();
                        } else {
                            Button(ButtonProps{}.type(ButtonType::Text).disabled(disabled).wave(false).onClick(action),
                                   [] {});
                        }
                    }},
                    s.color, s.font);
            }
        }});
    items(id, &b);
    services_->layout().set_layout(
        s.node, layout::ComponentLayout{[this, id](auto& e, auto, auto c) { return measure(id, e, c); },
                                        [this, id](auto& e, auto, runtime::Rect r) { place(id, e, r); }});
    mounted_.push_back(id);
    auto first = std::make_shared<bool>(true);
    connect_prop(b.scope(id), props.slides_, [this, id, first](std::vector<CarouselSlide> slides) {
        if (std::exchange(*first, false)) {
            return;
        }
        if (auto* s = services_->components().state<CarouselState>(id)) {
            s->pending = std::move(slides);
            ++s->items_generation;
            s->pending_items = true;
            services_->request_frame();
        }
    });
    if (props.current_) {
        connect_prop(b.scope(id), *props.current_, [this, id](std::size_t value) {
            auto* s = services_->components().state<CarouselState>(id);
            if (!s) {
                return;
            }
            if (s->dispatching || s->updating) {
                s->property = value;
                services_->request_frame();
                return;
            }
            if (!s->slides.empty()) {
                accept(id, std::min(value, s->slides.size() - 1),
                       s->requesting && s->requested == value && s->requested_instant,
                       s->requesting && s->requested == value ? s->requested_direction : 0);
            }
        });
    }
    const auto boolean = [&](const Prop<bool>& prop, bool CarouselState::* field) {
        connect_prop(b.scope(id), prop, [this, id, field](bool value) {
            if (auto* s = services_->components().state<CarouselState>(id)) {
                s->*field = value;
                if (!s->visible || s->disabled || !s->animated || field == &CarouselState::vertical) {
                    finish(id);
                    cancel_gesture(id);
                }
                refresh(id);
            }
        });
    };
    boolean(props.infinite_, &CarouselState::infinite);
    boolean(props.adaptive_height_, &CarouselState::adaptive);
    boolean(props.arrows_, &CarouselState::show_arrows);
    boolean(props.dots_, &CarouselState::show_dots);
    boolean(props.progress_, &CarouselState::progress_dots);
    boolean(props.draggable_, &CarouselState::draggable);
    boolean(props.swipe_, &CarouselState::swipe);
    boolean(props.autoplay_, &CarouselState::autoplay);
    boolean(props.pause_hover_, &CarouselState::pause_hover);
    boolean(props.pause_focus_, &CarouselState::pause_focus);
    boolean(props.wait_, &CarouselState::wait);
    boolean(props.animated_, &CarouselState::animated);
    boolean(props.visible_, &CarouselState::visible);
    boolean(props.disabled_, &CarouselState::disabled);
    if (props.vertical_) {
        boolean(*props.vertical_, &CarouselState::vertical);
    }
    connect_prop(b.scope(id), props.easing_, [this, id](CubicBezier v) {
        if (auto* s = services_->components().state<CarouselState>(id)) {
            s->easing = animation::Easing::cubic_bezier(CubicBezier{v.x1, v.y1, v.x2, v.y2});
            finish(id);
            refresh(id);
        }
    });
    connect_prop(b.scope(id), props.effect_, [this, id](CarouselEffect v) {
        validate(v);
        if (auto* s = services_->components().state<CarouselState>(id)) {
            if (s->effect != v) {
                finish(id);
                cancel_gesture(id);
            }
            s = services_->components().state<CarouselState>(id);
            if (s) {
                s->effect = v;
                refresh(id);
            }
        }
    });
    connect_prop(b.scope(id), props.placement_, [this, id](CarouselDotPlacement v) {
        validate(v);
        if (auto* s = services_->components().state<CarouselState>(id)) {
            finish(id);
            cancel_gesture(id);
            s = services_->components().state<CarouselState>(id);
            if (!s) {
                return;
            }
            s->placement = v;
            if (!s->props.vertical_) {
                s->vertical = v == CarouselDotPlacement::Start || v == CarouselDotPlacement::End;
            }
            refresh(id);
        }
    });
    connect_prop(b.scope(id), props.direction_, [this, id](TextDirection v) {
        if (auto* s = services_->components().state<CarouselState>(id)) {
            s->rtl = v == TextDirection::RightToLeft;
            finish(id);
            cancel_gesture(id);
            refresh(id);
        }
    });
    connect_prop(b.scope(id), props.speed_, [this, id](Duration v) {
        const auto value = duration(v, false);
        if (auto* s = services_->components().state<CarouselState>(id)) {
            s->speed = value;
            finish(id);
            refresh(id);
        }
    });
    connect_prop(b.scope(id), props.autoplay_speed_, [this, id](Duration v) {
        const auto value = duration(v, true);
        if (auto* s = services_->components().state<CarouselState>(id)) {
            s->interval = value;
            schedule(id, true);
        }
    });
    connect_prop(b.scope(id), props.threshold_, [this, id](float v) {
        if (!std::isfinite(v) || v <= 0) {
            throw std::invalid_argument("Carousel threshold");
        }
        if (auto* s = services_->components().state<CarouselState>(id)) {
            s->threshold = v;
        }
    });
    const auto theme = b.theme_scope();
    for (std::size_t i = 0; i < 3; ++i) {
        s.subscriptions[i] = theme->capture([this, id, geometry = i == 1](auto) { refresh(id, geometry); },
                                            [theme, i] {
                                                if (i == 0) {
                                                    static_cast<void>(theme->carousel_colors());
                                                } else if (i == 1) {
                                                    static_cast<void>(theme->carousel_metrics());
                                                } else {
                                                    static_cast<void>(theme->carousel_effects());
                                                }
                                            });
    }
    refresh(id);
}

runtime::Size CarouselComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& e, layout::Constraints c) {
    auto& s = *services_->components().state<CarouselState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().carousel().metrics;
    const auto width = std::clamp(m.width, c.min_width, c.max_width);
    float height{};
    for (std::size_t i = 0; i < s.slides.size(); ++i) {
        const auto node = services_->components().root(s.slides[i]);
        s.sizes[i] = e.measure_child(node, {width, width, 0, c.max_height});
        height = std::max(height, s.sizes[i].height);
    }
    if (s.adaptive && s.current) {
        height = s.sizes[*s.current].height;
        if (s.transition && s.previous && *s.previous < s.sizes.size()) {
            height = s.sizes[*s.previous].height * (1 - s.progress) + height * s.progress;
        }
    } else {
        height = std::max(height, m.height);
    }
    height = std::clamp(height, c.min_height, c.max_height);
    for (auto child : s.slides) {
        static_cast<void>(
            e.measure_child(services_->components().root(child), layout::Constraints::fixed(width, height)));
    }
    static_cast<void>(
        e.measure_child(services_->components().root(s.viewport), layout::Constraints::fixed(width, height)));
    for (std::size_t i = 0; i < 2; ++i) {
        const bool custom = i == 0 ? bool(s.props.prev_) : bool(s.props.next_);
        auto measured = custom ? e.measure_child(services_->components().root(s.arrows[i]), {0, width, 0, height})
                               : runtime::Size{m.arrow_hit_size, m.arrow_hit_size};
        s.arrow_sizes[i] = {std::min(width, std::max(m.arrow_hit_size, measured.width)),
                            std::min(height, std::max(m.arrow_hit_size, measured.height))};
        static_cast<void>(e.measure_child(services_->components().root(s.arrows[i]),
                                          layout::Constraints::fixed(s.arrow_sizes[i].width, s.arrow_sizes[i].height)));
    }
    const bool side = s.placement == CarouselDotPlacement::Start || s.placement == CarouselDotPlacement::End;
    for (std::size_t i = 0; i < s.dots.size(); ++i) {
        const auto length = s.current == i ? m.dot_active_width : m.dot_width;
        auto measured = s.props.dot_ ? e.measure_child(services_->components().root(s.dots[i]), {0, width, 0, height})
                                     : runtime::Size{side ? m.dot_height : length, side ? length : m.dot_height};
        s.dot_sizes[i] = {std::min(width, std::max(m.dot_hit_size, measured.width)),
                          std::min(height, std::max(m.dot_hit_size, measured.height))};
        static_cast<void>(e.measure_child(services_->components().root(s.dots[i]),
                                          layout::Constraints::fixed(s.dot_sizes[i].width, s.dot_sizes[i].height)));
    }
    return c.constrain({width, height});
}

void CarouselComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& e, runtime::Rect bounds) {
    auto& s = *services_->components().state<CarouselState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().carousel().metrics;
    if ((s.bounds.width != bounds.width || s.bounds.height != bounds.height) &&
        (s.bounds.width > 0 || s.bounds.height > 0)) {
        if (s.transition && (s.bounds.width != bounds.width || !s.adaptive)) {
            s.after_pending = s.current;
            s.transition = false;
            s.previous.reset();
            s.progress = 1;
            services_->request_frame();
        }
        s.pointer.reset();
        s.dragging = false;
        s.drag = 0;
        s.preview.reset();
        services_->pointer().cancel_pointer_interaction(s.interaction, false);
    }
    s.bounds = bounds;
    s.viewport_bounds = bounds;
    e.place_child(services_->components().root(s.viewport), bounds);
    const auto extent = s.vertical ? bounds.height : bounds.width;
    const auto direction = float(s.direction) * (!s.vertical && s.rtl ? -1.0F : 1.0F);
    const auto eased = s.initial_progress + (1 - s.initial_progress) * s.easing.sample(s.progress);
    // Poses beyond these limits are fully clipped; bounding them preserves output and finite geometry.
    const auto progress =
        s.effect == CarouselEffect::Fade ? std::clamp(eased, 0.0F, 1.0F) : std::clamp(eased, -1.0F, 2.0F);
    for (std::size_t i = 0; i < s.slides.size(); ++i) {
        auto r = bounds;
        float offset{};
        float opacity = 1;
        if (s.current == i) {
            if (s.transition) {
                offset = direction * extent * (1 - progress);
                if (s.effect == CarouselEffect::Fade) {
                    offset = 0;
                    opacity = progress;
                }
            } else {
                offset = s.drag;
                if (s.effect == CarouselEffect::Fade && s.dragging) {
                    offset = 0;
                    opacity = extent > 0 ? 1 - std::min(1.0F, std::abs(s.drag) / extent) : 1;
                }
            }
        } else if (s.transition && s.previous == i) {
            offset = -direction * extent * progress;
            if (s.effect == CarouselEffect::Fade) {
                offset = 0;
                opacity = 1 - progress;
            }
        } else if (s.dragging && s.preview == i) {
            offset = s.drag + (s.drag < 0 ? extent : -extent);
            if (s.effect == CarouselEffect::Fade) {
                offset = 0;
                opacity = extent > 0 ? std::min(1.0F, std::abs(s.drag) / extent) : 0;
            }
        } else {
            offset = extent * 2;
            opacity = 0;
        }
        if (s.vertical) {
            r.y += offset;
        } else {
            r.x += offset;
        }
        const auto node = services_->components().root(s.slides[i]);
        services_->nodes().set_paint_opacity(node, opacity);
        e.place_child(node, r);
    }
    for (std::size_t i = 0; i < 2; ++i) {
        const auto size = s.arrow_sizes[i];
        const bool end = (i == 1) ^ (!s.vertical && s.rtl);
        auto r = runtime::Rect{bounds.x + (bounds.width - size.width) / 2, bounds.y + (bounds.height - size.height) / 2,
                               size.width, size.height};
        if (s.vertical) {
            r.y = end ? bounds.y + std::max(0.0F, bounds.height - size.height - m.arrow_offset)
                      : bounds.y + std::min(m.arrow_offset, std::max(0.0F, bounds.height - size.height));
        } else {
            r.x = end ? bounds.x + std::max(0.0F, bounds.width - size.width - m.arrow_offset)
                      : bounds.x + std::min(m.arrow_offset, std::max(0.0F, bounds.width - size.width));
        }
        e.place_child(services_->components().root(s.arrows[i]), r);
    }
    const bool side = s.placement == CarouselDotPlacement::Start || s.placement == CarouselDotPlacement::End;
    float length{};
    for (std::size_t i = 0; i < s.dots.size(); ++i) {
        length += (side ? s.dot_sizes[i].height : s.dot_sizes[i].width) + (i ? m.dot_gap : 0);
    }
    float position = side ? bounds.y + (bounds.height - length) / 2 : bounds.x + (bounds.width - length) / 2;
    for (std::size_t i = 0; i < s.dots.size(); ++i) {
        const auto size = s.dot_sizes[i];
        runtime::Rect r;
        if (side) {
            const bool end = (s.placement == CarouselDotPlacement::End) ^ s.rtl;
            r = {end ? bounds.x + std::max(0.0F, bounds.width - m.dot_offset - size.width)
                     : bounds.x + std::min(m.dot_offset, std::max(0.0F, bounds.width - size.width)),
                 position, size.width, size.height};
        } else {
            r = {position,
                 s.placement == CarouselDotPlacement::Bottom
                     ? bounds.y + std::max(0.0F, bounds.height - m.dot_offset - size.height)
                     : bounds.y + std::min(m.dot_offset, std::max(0.0F, bounds.height - size.height)),
                 size.width, size.height};
            if (s.rtl) {
                r.x = bounds.x + bounds.width - (r.x - bounds.x) - r.width;
            }
        }
        e.place_child(services_->components().root(s.dots[i]), r);
        position += (side ? size.height : size.width) + m.dot_gap;
    }
}

void CarouselComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (auto id : ids) {
        items(id);
        auto* s = services_->components().state<CarouselState>(id);
        if (!s) {
            continue;
        }
        if (s->after_pending) {
            CarouselFlagGuard callback_guard{*services_, id, &CarouselState::dispatching};
            const auto target = *s->after_pending;
            const auto callback = s->props.after_;
            s->after_pending.reset();
            if (callback) {
                try {
                    callback(target);
                } catch (...) {
                    services_->retire_failed_component(id);
                    throw;
                }
            }
            s = services_->components().state<CarouselState>(id);
            if (!s) {
                continue;
            }
        }
        if (s->property) {
            const auto value = *s->property;
            s->property.reset();
            if (!s->slides.empty()) {
                accept(id, std::min(value, s->slides.size() - 1),
                       s->requesting && s->requested == value && s->requested_instant,
                       s->requesting && s->requested == value ? s->requested_direction : 0);
            }
        }
        if (!services_->components().contains(id)) {
            continue;
        }
        auto* current = services_->components().state<CarouselState>(id);
        const bool enabled = animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                                              services_->motion_preference())
                                 .enabled();
        const bool interrupted = current->motion_enabled.value_or(enabled) && !enabled;
        current->motion_enabled = enabled;
        if (interrupted) {
            cancel_gesture(id);
        }
        current = services_->components().state<CarouselState>(id);
        if (!current) {
            continue;
        }
        if (current->transition &&
            (!current->visible || !services_->components().branch_active(id) ||
             !services_->focus().state().window_active ||
             !animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                               services_->motion_preference())
                  .enabled())) {
            finish(id);
            cancel_gesture(id);
        }
        schedule(id);
        gates(id);
    }
}

void CarouselComponentHost::synchronize_auxiliary_motion() {
    prepare_auxiliary_layout();
}

void CarouselComponentHost::on_window_active(bool active) {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!active) {
            finish(id);
            cancel_gesture(id);
        }
        schedule(id);
        gates(id);
    }
}

void CarouselComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                             std::optional<input::InteractionId> hit,
                                             std::optional<input::InteractionId>) {
    if (event.pointer.device != input::PointerDevice::mouse) {
        return;
    }
    const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
    const auto component = record ? std::optional{record->component} : std::nullopt;
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<CarouselState>(id);
        if (!s) {
            continue;
        }
        const bool hovered = component && descendant(services_->components(), *component, id);
        const auto owned = hovered ? component : std::nullopt;
        if (s->hovered_component != owned) {
            s->hovered_component = owned;
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material | runtime::DirtyFlags::Geometry);
        }
        if (s->hovered != hovered) {
            s->hovered = hovered;
            schedule(id);
        }
    }
}

void CarouselComponentHost::on_focus_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        schedule(id);
    }
}

void CarouselComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!services_->interactions().component_active(id)) {
            finish(id);
            cancel_gesture(id);
        }
        schedule(id);
    }
}

std::size_t CarouselComponentHost::tick_auxiliary(animation::AnimationTime time) {
    std::size_t updates{};
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<CarouselState>(id);
        if (!s) {
            continue;
        }
        const auto now = std::max(s->last, time);
        s->last = now;
        if (s->transition) {
            const auto span = s->transition_speed.count_microseconds();
            const auto elapsed = now >= s->start ? (now - s->start).count_microseconds() : 0;
            s->progress = span > 0 ? std::clamp(float(double(elapsed) / double(span)), 0.0F, 1.0F) : 1;
            services_->dirty().invalidate(s->node,
                                          (s->adaptive ? runtime::DirtyFlags::Measure : runtime::DirtyFlags::None) |
                                              runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry |
                                              runtime::DirtyFlags::Material | runtime::DirtyFlags::HitTest);
            ++updates;
            if (s->progress >= 1) {
                finish(id);
            } else {
                gates(id);
            }
        }
        schedule(id);
        s = services_->components().state<CarouselState>(id);
        if (!s) {
            continue;
        }
        if (s->deadline && now >= *s->deadline) {
            s->deadline.reset();
            s->remaining = s->interval;
            static_cast<void>(step(id, 1, CarouselChangeSource::Autoplay));
            schedule(id, true);
            ++updates;
        }
        s = services_->components().state<CarouselState>(id);
        if (!s) {
            continue;
        }
        if (s->deadline && s->progress_dots) {
            const auto left = *s->deadline > now ? (*s->deadline - now).count_microseconds() : 0;
            s->autoplay_progress =
                std::clamp(1 - float(double(left) / double(s->interval.count_microseconds())), 0.0F, 1.0F);
            const auto dots = s->dots;
            for (std::size_t i = 0; i < dots.size(); ++i) {
                auto* d = services_->components().state<Dot>(dots[i]);
                if (d) {
                    auto meta = d->info.get();
                    meta.progress = s->current == i ? s->autoplay_progress : 0;
                    const auto signal = d->info;
                    signal.set(meta);
                }
                s = services_->components().state<CarouselState>(id);
                if (!s) {
                    break;
                }
            }
            if (s) {
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
                ++updates;
            }
        }
    }
    return updates;
}

std::optional<animation::AnimationTime> CarouselComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> result;
    for (auto id : mounted_) {
        const auto* s = services_->components().state<CarouselState>(id);
        if (!s) {
            continue;
        }
        const auto minimum = [&](auto t) {
            if (!result || t < *result) {
                result = t;
            }
        };
        if (s->transition) {
            minimum(
                std::min(s->last + animation::AnimationDuration::microseconds(16667), s->start + s->transition_speed));
        }
        if (s->deadline && !paused(id)) {
            minimum(s->progress_dots
                        ? std::min(*s->deadline, s->last + animation::AnimationDuration::microseconds(16667))
                        : *s->deadline);
        }
    }
    return result;
}

bool CarouselComponentHost::on_keyboard_input(const input::KeyboardInputEvent& e) {
    if (e.action != input::KeyAction::down || input::has_modifier(e.modifiers, input::KeyModifier::control) ||
        input::has_modifier(e.modifiers, input::KeyModifier::alt) ||
        input::has_modifier(e.modifiers, input::KeyModifier::meta)) {
        return false;
    }
    const auto focused = services_->focus().state().focused;
    for (auto id : mounted_) {
        const auto* s = services_->components().state<CarouselState>(id);
        if (!s || !focused || *focused != s->interaction || !s->current) {
            continue;
        }
        if (e.key == input::Key::home || e.key == input::Key::end) {
            request(id, e.key == input::Key::home ? 0 : s->slides.size() - 1, CarouselChangeSource::Keyboard);
            return true;
        }
        int direction{};
        if ((s->vertical && e.key == input::Key::up) || (!s->vertical && e.key == input::Key::left)) {
            direction = -1;
        }
        if ((s->vertical && e.key == input::Key::down) || (!s->vertical && e.key == input::Key::right)) {
            direction = 1;
        }
        if (direction) {
            if (!s->vertical && s->rtl) {
                direction = -direction;
            }
            step(id, direction, CarouselChangeSource::Keyboard);
            return true;
        }
    }
    return false;
}

void CarouselComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        refresh_clip(id, viewport, clip);
        auto* pending = services_->components().state<CarouselState>(id);
        if (pending && pending->after_pending) {
            CarouselFlagGuard callback_guard{*services_, id, &CarouselState::dispatching};
            const auto target = *pending->after_pending;
            const auto callback = pending->props.after_;
            pending->after_pending.reset();
            if (callback) {
                try {
                    callback(target);
                } catch (...) {
                    services_->retire_failed_component(id);
                    throw;
                }
            }
        }
        schedule(id);
        gates(id);
        const auto* s = services_->components().state<CarouselState>(id);
        if (!s) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().carousel();
        const auto& m = t.metrics;
        const auto focused = services_->focus().state().focused;
        const auto* focus_record = focused ? services_->interactions().find(*focused) : nullptr;
        const auto emphasized = [&](runtime::ComponentId control) {
            return (s->hovered && s->hovered_component &&
                    descendant(services_->components(), *s->hovered_component, control)) ||
                   (focus_record && descendant(services_->components(), focus_record->component, control));
        };
        const auto node = services_->nodes().require(s->node);
        const auto c = s->clip;
        const auto opacity =
            services_->components().branch_active(id) ? services_->nodes().content_opacity(s->node) : 0;
        services_->interactions().set_eligible(s->interaction, s->visible && !s->disabled && s->current.has_value() &&
                                                                   c.width > 0 && c.height > 0);
        std::vector<std::pair<runtime::ComponentId, bool>> controls;
        for (auto arrow : s->arrows) {
            const auto& n = services_->nodes().require(services_->components().root(arrow));
            const auto bounds = graphics::intersect_effect_bounds(
                {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height}, c);
            controls.emplace_back(arrow, !s->show_arrows || s->disabled || bounds.width <= 0 || bounds.height <= 0);
        }
        std::vector<graphics::RoundedEffectInstance> back;
        if (node.bounds.width > 0 && node.bounds.height > 0) {
            for (std::size_t i = 0; i < t.shadow.size(); ++i) {
                auto part = graphics::make_shadow_effect({node.bounds, 0}, t.shadow[i], node.translation,
                                                         graphics::EffectClip{1, c});
                part.material.opacity = opacity;
                back.push_back(part);
            }
            auto fill =
                graphics::make_corner_fill_effects({node.bounds, 0}, {true, true, true, true}, t.colors.background,
                                                   opacity, node.translation, graphics::EffectClip{1, c});
            back.insert(back.end(), fill.begin(), fill.end());
        }
        services_->surfaces().update_content_effects(s->background, back);
        std::vector<graphics::RoundedEffectInstance> effects;
        const auto fill = [&](runtime::Rect r, float radius, Color color, float alpha = 1) {
            if (r.width > 0 && r.height > 0) {
                auto parts = graphics::make_corner_fill_effects({r, std::min(radius, std::min(r.width, r.height) / 2)},
                                                                {true, true, true, true}, color, opacity * alpha,
                                                                node.translation, graphics::EffectClip{1, c});
                effects.insert(effects.end(), parts.begin(), parts.end());
            }
        };
        if (s->show_arrows && s->slides.size() > 1) {
            for (std::size_t i = 0; i < 2; ++i) {
                const auto* arrow = services_->components().state<Arrow>(s->arrows[i]);
                const auto renderer = i == 0 ? s->props.prev_ : s->props.next_;
                if (renderer) {
                    continue;
                }
                const auto hit_bounds = services_->nodes().require(services_->components().root(s->arrows[i])).bounds;
                const auto glyph_size = std::min({m.arrow_size, hit_bounds.width, hit_bounds.height});
                const auto r =
                    runtime::Rect{hit_bounds.x + (hit_bounds.width - glyph_size) / 2,
                                  hit_bounds.y + (hit_bounds.height - glyph_size) / 2, glyph_size, glyph_size};
                const bool end = (i == 1) ^ (!s->vertical && s->rtl);
                const auto size = std::min(r.width, r.height);
                const auto alpha = arrow && arrow->info.get().disabled ? (s->disabled ? t.disabled_opacity : 0.0F)
                                   : emphasized(s->arrows[i])          ? 1.0F
                                                                       : .4F;
                for (int segment = 0; segment < 5; ++segment) {
                    const float d = float(segment) * size / 14;
                    const float stroke = std::max(1.0F, size / 14);
                    if (s->vertical) {
                        fill({r.x + r.width / 2 - d, r.y + r.height / 2 + (end ? -d : d), stroke, stroke}, 0,
                             t.colors.arrow, alpha);
                        fill({r.x + r.width / 2 + d, r.y + r.height / 2 + (end ? -d : d), stroke, stroke}, 0,
                             t.colors.arrow, alpha);
                    } else {
                        fill({r.x + r.width / 2 + (end ? -d : d), r.y + r.height / 2 - d, stroke, stroke}, 0,
                             t.colors.arrow, alpha);
                        fill({r.x + r.width / 2 + (end ? -d : d), r.y + r.height / 2 + d, stroke, stroke}, 0,
                             t.colors.arrow, alpha);
                    }
                }
            }
        }
        if (services_->focus().state().focused == s->interaction && services_->focus().state().focus_visible &&
            node.bounds.width > 0 && node.bounds.height > 0) {
            effects.push_back(graphics::make_outline_effect({node.bounds, 0}, m.focus_width, 0, t.colors.focus, opacity,
                                                            node.translation, graphics::EffectClip{1, c}));
        }
        services_->surfaces().update_content_effects(s->surface, effects);
        for (std::size_t i = 0; i < s->dots.size(); ++i) {
            const auto* d = services_->components().state<Dot>(s->dots[i]);
            if (!d) {
                continue;
            }
            const auto n = services_->nodes().require(d->node);
            auto r = n.bounds;
            if (!s->props.dot_) {
                const bool side =
                    s->placement == CarouselDotPlacement::Start || s->placement == CarouselDotPlacement::End;
                const auto length = s->current == i ? m.dot_active_width : m.dot_width;
                const auto w = std::min(r.width, side ? m.dot_height : length);
                const auto h = std::min(r.height, side ? length : m.dot_height);
                r = {r.x + (r.width - w) / 2, r.y + (r.height - h) / 2, w, h};
            }
            std::vector<graphics::RoundedEffectInstance> parts;
            if (s->show_dots && !s->props.dot_ && r.width > 0 && r.height > 0) {
                const bool duration = s->progress_dots && (s->autoplay || s->playing_override) && s->current == i;
                auto base = graphics::make_corner_fill_effects(
                    {r, std::min(r.width, r.height) / 2}, {true, true, true, true},
                    s->current == i && !duration ? t.colors.dot_active
                    : emphasized(s->dots[i])     ? Color(t.colors.dot_active.red(), t.colors.dot_active.green(),
                                                         t.colors.dot_active.blue(), t.colors.dot_active.alpha() * .75F)
                                                 : t.colors.dot,
                    opacity, n.translation, graphics::EffectClip{1, c});
                parts.insert(parts.end(), base.begin(), base.end());
                if (s->progress_dots && s->current == i) {
                    auto progress = r;
                    if (s->placement == CarouselDotPlacement::Start || s->placement == CarouselDotPlacement::End) {
                        progress.height *= s->autoplay_progress;
                    } else {
                        progress.width *= s->autoplay_progress;
                        if (s->rtl) {
                            progress.x = r.x + r.width - progress.width;
                        }
                    }
                    if (progress.width > 0 && progress.height > 0) {
                        auto p = graphics::make_corner_fill_effects(
                            {progress, std::min(progress.width, progress.height) / 2}, {true, true, true, true},
                            t.colors.progress, opacity, n.translation, graphics::EffectClip{1, c});
                        parts.insert(parts.end(), p.begin(), p.end());
                    }
                }
            }
            services_->surfaces().update_content_effects(d->surface, parts);
            const auto bounds = graphics::intersect_effect_bounds(
                {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height}, c);
            controls.emplace_back(s->dots[i], !s->show_dots || s->disabled || bounds.width <= 0 || bounds.height <= 0);
        }
        for (const auto& [control, blocked] : controls) {
            if (!services_->components().contains(id)) {
                break;
            }
            if (services_->components().contains(control)) {
                services_->set_input_blocked(control, blocked);
            }
        }
    }
}
} // namespace ryn::detail

namespace ryn {
CarouselRef::CarouselRef() : state_(std::make_shared<detail::CarouselRefState>()) {}

bool CarouselRef::goTo(std::size_t i, bool instant) const {
    state_->check();
    const auto f = state_->go;
    return f && f(i, instant);
}

bool CarouselRef::next() const {
    state_->check();
    const auto f = state_->step;
    return f && f(1);
}

bool CarouselRef::prev() const {
    state_->check();
    const auto f = state_->step;
    return f && f(-1);
}

bool CarouselRef::play() const {
    state_->check();
    const auto f = state_->playback;
    return f && f(true);
}

bool CarouselRef::pause() const {
    state_->check();
    const auto f = state_->playback;
    return f && f(false);
}

bool CarouselRef::focus() const {
    state_->check();
    const auto f = state_->focus;
    return f && f();
}

void Carousel(CarouselProps props) {
    if (!detail::active_carousel) {
        throw std::logic_error("Carousel requires an active window component host");
    }
    detail::active_carousel->mount(props);
}
} // namespace ryn
