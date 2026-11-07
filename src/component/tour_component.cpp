#include "component/config_defaults.hpp"
#include "component/tour_component.hpp"
#include "component/modal_component.hpp"
#include "component/localized_prop.hpp"
#include "component/retained_slot_mount.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/button.hpp>
#include <ryn/flex.hpp>
#include <ryn/text.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <limits>

namespace ryn::detail {
struct TourTargetState final {
    WindowComponentServices* window{};
    input::InputRootRef root;
    std::uint64_t generation{};
};

namespace {
thread_local TourComponentHost* active_tour{};
constexpr auto infinity = std::numeric_limits<float>::infinity();

struct StepView final {
    explicit StepView(TourStep value) : step(std::move(value)) {}

    TourStep step;
    runtime::ComponentId component;
    Signal<String> title{String{}};
    Signal<String> description{String{}};
    std::unique_ptr<Scope> bindings{std::make_unique<Scope>()};
};

struct TourState final {
    TourProps props;
    runtime::NodeId node;
    TourSnapshot view;
    runtime::ComponentId layer;
    runtime::ComponentId content;
    component::RetainedSurfaceId surface;
    input::InputConstraintLease restriction;
    ModalInputCompanion companion;
    std::optional<input::InputConstraintSpec> policy;
    std::vector<TourStep> steps;
    std::map<std::string, std::shared_ptr<StepView>, std::less<>> pages;
    Signal<String> next_label{String{}};
    Signal<String> previous_label{String{}};
    Signal<String> close_label{String{}};
    Signal<String> indicator{String{}};
    Signal<bool> previous_disabled{true};
    TourLocale locale;
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
    theme_runtime::Subscription theme;
    std::optional<input::InteractionId> restore;
    bool open{};
    bool keyboard{true};
    bool mask{true};
    bool shown{true};
    bool disabled{};
    bool rtl{};
    bool focus_pending{};
    bool input_enabled{};
    std::size_t current{};
    std::uint64_t revision{};
};

using State = std::shared_ptr<TourState>;

State state(WindowComponentServices& services, runtime::ComponentId id) {
    const auto* value = services.components().state<State>(id);
    return value ? *value : State{};
}

bool alive(WindowComponentServices& services, runtime::ComponentId id, const State& value) {
    return state(services, id) == value && services.components().scope(id).active();
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto current = std::optional{child}; current; current = services.components().parent(*current)) {
        if (*current == parent) {
            return true;
        }
    }
    return false;
}

runtime::Rect window_bounds(WindowComponentServices& services, runtime::ComponentId component) {
    const auto& node = services.nodes().require(services.components().root(component));
    return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
            node.bounds.height};
}

void translate(WindowComponentServices& services, runtime::NodeId id, runtime::Point value) {
    auto& node = services.nodes().require(id);
    if (node.clip_boundary) {
        return;
    }
    runtime::NodePropertyWriter{services.nodes(), services.dirty()}.set_translation(id, value);
    if (node.owns_child_translation) {
        return;
    }
    for (auto child : node.children) {
        translate(services, child, value);
    }
}

void activate(WindowComponentServices& services, runtime::ComponentId id, bool enabled) {
    if (services.components().set_branch_active(id, enabled)) {
        services.mark_scene_structure_dirty();
    }
}

runtime::ComponentId box(WindowComponentServices& services, runtime::ComponentId parent, const Content& content,
                         bool enabled = true, const State& style = {}) {
    auto& build = runtime::require_component_build_context();
    runtime::ComponentId result;
    build.mount_slot(
        parent, Content{[&] {
            auto& context = runtime::require_component_build_context();
            result = context.mount_component<int>(0);
            const auto node = context.root(result);
            services.components().set_branch_active(result, enabled);
            services.layout().set_layout(node, layout::BoxLayout{});
            context.on_resource_cleanup(result, [&services, node] { services.layout().remove_layout(node); });
            if (style) {
                context.mount_slot_with_semantic_text_style(result, content,
                                                            Prop<runtime::SemanticForeground>{style->foreground},
                                                            Prop<runtime::SemanticTypography>{style->typography});
            } else {
                context.mount_slot(result, content);
            }
        }});
    return result;
}

input::InteractionId button_hit(WindowComponentServices& services, runtime::ComponentId parent) {
    for (auto hit : services.interactions().declaration_order()) {
        if (descendant(services, services.interactions().require(hit).component, parent)) {
            return hit;
        }
    }
    return {};
}
} // namespace

TourComponentHost::TourComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

TourComponentHost::~TourComponentHost() {
    services_->detach(*this);
}

void* TourComponentHost::begin_mount() noexcept {
    return std::exchange(active_tour, this);
}

void TourComponentHost::end_mount(void* previous) noexcept {
    active_tour = static_cast<TourComponentHost*>(previous);
}

void TourComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void TourComponentHost::on_dispose() noexcept {
    mounted_.clear();
}

TourSnapshot TourComponentHost::snapshot(runtime::ComponentId id) const {
    const auto s = state(*services_, id);
    if (!s) {
        throw std::out_of_range("Tour is retired");
    }
    return s->view;
}

void TourComponentHost::mount_target(TourTargetRef ref, TourTargetContent content) {
    if (ref.bound()) {
        throw std::invalid_argument("TourTargetRef already has a live native binding");
    }
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    services.layout().set_layout(node, layout::BoxLayout{});
    auto target = ref.state_;
    const auto generation = ++target->generation;
    target->window = &services;
    target->root = services.interactions().input_root(id);
    build.scope(id).on_cleanup(services.interactions().input_owner_cleanup(target->root));
    build.on_resource_cleanup(id, [&services, target, generation, node] {
        if (target->generation == generation) {
            target->root = {};
            target->window = nullptr;
        }
        services.layout().remove_layout(node);
        services.request_frame();
    });
    build.mount_slot(id, Content{[content] { SlotContentAccess::function(content)(); }});
}

void TourComponentHost::refresh(runtime::ComponentId id) {
    const auto s = state(*services_, id);
    if (!s) {
        return;
    }
    ++s->revision;
    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                               runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    services_->request_frame();
}

void TourComponentHost::reconcile(runtime::ComponentId id, std::vector<TourStep> steps) {
    std::set<std::string, std::less<>> keys;
    for (const auto& step : steps) {
        if (step.key_.empty() || !keys.emplace(step.key_.bytes()).second || step.placement_ < TourPlacement::Top ||
            step.placement_ > TourPlacement::Center) {
            throw std::invalid_argument("Tour steps require unique nonempty keys and valid placement");
        }
    }
    const auto s = state(*services_, id);
    if (!s) {
        return;
    }
    const auto revision = ++s->revision;
    const auto old_key = s->current < s->steps.size() ? s->steps[s->current].key_ : String{};
    s->steps = std::move(steps);
    if (!s->props.current_ && !old_key.empty()) {
        const auto found = std::ranges::find_if(s->steps, [&](const auto& step) { return step.key_ == old_key; });
        s->current = found != s->steps.end() ? static_cast<std::size_t>(found - s->steps.begin())
                                             : std::min(s->current, s->steps.empty() ? 0 : s->steps.size() - 1);
    }
    std::vector<runtime::ComponentId> removed;
    for (auto it = s->pages.begin(); it != s->pages.end();) {
        const auto found =
            std::ranges::find_if(s->steps, [&](const auto& step) { return step.key_.bytes() == it->first; });
        if (found == s->steps.end() || found->title_slot_ != it->second->step.title_slot_ ||
            found->description_slot_ != it->second->step.description_slot_ ||
            found->cover_ != it->second->step.cover_) {
            removed.push_back(it->second->component);
            it = s->pages.erase(it);
        } else {
            const auto page = it->second;
            page->step = *found;
            page->bindings = std::make_unique<Scope>();
            connect_prop(*page->bindings, found->title_,
                         [title = page->title](String value) { title.set(std::move(value)); });
            if (!alive(*services_, id, s) || s->revision != revision) {
                return;
            }
            connect_prop(*page->bindings, found->description_,
                         [description = page->description](String value) { description.set(std::move(value)); });
            if (!alive(*services_, id, s) || s->revision != revision) {
                return;
            }
            ++it;
        }
    }
    refresh(id);
    for (auto component : removed) {
        if (s->view.content == component) {
            s->view.content = {};
        }
        services_->destroy(component);
        if (!alive(*services_, id, s)) {
            return;
        }
    }
}

void TourComponentHost::navigate(runtime::ComponentId id, int delta) {
    const auto s = state(*services_, id);
    if (!s || !s->view.visible || !services_->interactions().component_active(s->view.panel)) {
        return;
    }
    if (delta > 0 && s->view.current + 1 >= s->steps.size()) {
        close(id, TourCloseReason::Finish);
        return;
    }
    const auto next = delta < 0 ? (s->view.current ? s->view.current - 1 : 0) : s->view.current + 1;
    if (next == s->view.current) {
        return;
    }
    const auto callback = s->props.on_change_;
    if (!s->props.current_) {
        s->current = next;
        s->focus_pending = true;
        refresh(id);
    }
    try {
        if (callback) {
            callback(next);
        }
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

void TourComponentHost::close(runtime::ComponentId id, TourCloseReason reason) {
    const auto s = state(*services_, id);
    if (!s || !s->view.visible || !services_->interactions().component_active(s->view.panel)) {
        return;
    }
    const auto callback = s->props.on_open_;
    const auto closed = s->props.on_close_;
    if (!s->props.open_) {
        s->open = false;
        deactivate(id, true);
        refresh(id);
    }
    try {
        if (callback) {
            callback(false);
        }
        if (alive(*services_, id, s) && closed) {
            closed(reason);
        }
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

void TourComponentHost::deactivate(runtime::ComponentId id, bool restore) {
    const auto s = state(*services_, id);
    if (!s) {
        return;
    }
    const bool was_visible = std::exchange(s->view.visible, false);
    s->view.target.reset();
    s->policy.reset();
    s->focus_pending = false;
    const auto focused = services_->focus().state().focused;
    const auto restore_ticket = services_->focus().focus_request_revision();
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    const bool own_focus = record && descendant(*services_, record->component, s->view.panel);
    activate(*services_, s->layer, false);
    if (!alive(*services_, id, s)) {
        return;
    }
    if (std::exchange(s->input_enabled, false)) {
        const std::array updates{input::InputConstraintUpdate{s->restriction, {}}};
        static_cast<void>(s->companion.update(false, updates));
        if (!alive(*services_, id, s)) {
            return;
        }
    }
    if (was_visible && restore && (own_focus || !services_->focus().state().focused) && s->restore) {
        static_cast<void>(
            services_->focus().defer_focus_if_current(restore_ticket, s->restore, input::FocusModality::keyboard));
    }
    if (was_visible) {
        services_->request_frame();
    }
}

void TourComponentHost::mount(TourProps props) {
    auto& build = runtime::require_component_build_context();
    const auto locale =
        localized_prop(props.locale_, build.config_scope(), [](const LocaleData& data) { return data.tour; });
    auto& services = *services_;
    const auto s = std::make_shared<TourState>();
    const auto id = build.mount_component<State>(s);
    s->props = std::move(props);
    s->node = build.root(id);
    services.nodes().require(s->node).out_of_flow = true;
    services.layout().set_layout(
        s->node, layout::ComponentLayout{[](auto&, auto, auto limits) { return limits.constrain(runtime::Size{}); },
                                         [](auto&, auto, auto) {}});
    build.on_resource_cleanup(id, [&services, s] {
        static_cast<void>(s->companion.release());
        static_cast<void>(s->restriction.release());
        services.layout().remove_layout(s->node);
    });
    s->open = s->props.open_ ? read_prop(*s->props.open_) : s->props.default_open_;
    if (!alive(services, id, s)) {
        return;
    }
    s->current = s->props.current_ ? read_prop(*s->props.current_) : s->props.default_current_;
    if (!alive(services, id, s)) {
        return;
    }
    s->layer = box(services, id, Content{[] {}}, false);
    const auto layer_node = services.components().root(s->layer);
    services.layout().set_layout(layer_node, layout::LeafLayout{});
    services.nodes().require(layer_node).out_of_flow = true;
    services.nodes().require(layer_node).clip_boundary = true;
    services.components().set_window_layer(s->layer, services.components().theme_scope(id)->snapshot().tour().z_index);
    const auto fragment = build.register_scene_fragment(s->layer, runtime::SceneFragmentPlacement::before_children);
    s->surface = services.surfaces().create_content_range(fragment, {});
    build.on_resource_cleanup(
        s->layer, [&services, surface = s->surface] { services.surfaces().destroy_content_range(surface); });
    mount_retained_slot(services, s->layer, Content{[&] {
                            s->view.panel = box(services, s->layer, Content{[&] {
                                                    auto& context = runtime::require_component_build_context();
                                                    // The panel itself is built below with explicit retained child
                                                    // slots.
                                                    (void)context;
                                                }});
                        }});
    services.nodes().require(services.components().root(s->view.panel)).clip_content = true;
    mount_retained_slot(
        services, s->view.panel, Content{[&] {
            s->content = box(services, s->view.panel, Content{[] {}});
            s->view.previous =
                box(services, s->view.panel, Content{[&] {
                        Button(ButtonProps{}.wave(false).disabled(s->previous_disabled).onClick([this, id] {
                            navigate(id, -1);
                        }),
                               ButtonContent{[label = s->previous_label] { Text(TextProps{}.content(label)); }});
                    }});
            s->view.next = box(
                services, s->view.panel, Content{[&] {
                    Button(ButtonProps{}.type(ButtonType::Primary).wave(false).onClick([this, id] { navigate(id, 1); }),
                           ButtonContent{[label = s->next_label] { Text(TextProps{}.content(label)); }});
                }});
            s->view.close =
                box(services, s->view.panel, Content{[&] {
                        Button(ButtonProps{}.type(ButtonType::Text).wave(false).onClick([this, id] {
                            close(id, TourCloseReason::CloseButton);
                        }),
                               ButtonContent{[label = s->close_label] { Text(TextProps{}.content(label)); }});
                    }});
        }});
    services.layout().set_layout(
        services.components().root(s->content),
        layout::ComponentLayout{[this, id](auto& engine, auto, layout::Constraints limits) {
                                    const auto value = state(*services_, id);
                                    return value->view.content.valid()
                                               ? engine.measure_child(services_->components().root(value->view.content),
                                                                      limits)
                                               : limits.constrain(runtime::Size{});
                                },
                                [this, id](auto& engine, auto, runtime::Rect bounds) {
                                    const auto value = state(*services_, id);
                                    if (value->view.content.valid()) {
                                        engine.place_child(services_->components().root(value->view.content), bounds);
                                    }
                                }});
    const auto panel_node = services.components().root(s->view.panel);
    services.layout().set_layout(
        panel_node,
        layout::ComponentLayout{
            [this, id](auto& engine, auto, layout::Constraints limits) {
                const auto value = state(*services_, id);
                const auto& token = services_->components().theme_scope(id)->snapshot().tour();
                const auto width = std::min(limits.max_width, token.width);
                const auto inner = std::max(0.0F, width - 2 * token.padding);
                float actions = 0;
                for (auto child : {value->view.previous, value->view.next, value->view.close}) {
                    actions = std::max(
                        actions,
                        engine.measure_child(services_->components().root(child), {0, inner / 3, 0, infinity}).height);
                }
                const auto content =
                    engine.measure_child(services_->components().root(value->content), {inner, inner, 0, infinity});
                return limits.constrain({width, content.height + actions + token.gap + 2 * token.padding});
            },
            [this, id](auto& engine, auto, runtime::Rect bounds) {
                const auto value = state(*services_, id);
                const auto& token = services_->components().theme_scope(id)->snapshot().tour();
                const auto inner = std::max(0.0F, bounds.width - 2 * token.padding);
                float actions = 0;
                for (auto child : {value->view.previous, value->view.next, value->view.close}) {
                    actions = std::max(
                        actions, services_->nodes().require(services_->components().root(child)).measured_size.height);
                }
                engine.place_child(services_->components().root(value->content),
                                   {bounds.x + token.padding, bounds.y + token.padding, inner,
                                    std::max(0.0F, bounds.height - 2 * token.padding - token.gap - actions)});
                std::array children{value->view.previous, value->view.next, value->view.close};
                if (value->rtl) {
                    std::ranges::reverse(children);
                }
                const auto width = std::max(0.0F, (inner - 2 * token.gap) / 3);
                float x = bounds.x + token.padding;
                for (auto child : children) {
                    engine.place_child(services_->components().root(child),
                                       {x, bounds.y + bounds.height - token.padding - actions, width, actions});
                    x += width + token.gap;
                }
            }});
    const auto owner = services.interactions().input_root(id);
    build.scope(id).on_cleanup(services.interactions().input_owner_cleanup(owner));
    s->restriction = services.create_input_constraint(owner, input::InputConstraintKind::RestrictToSubtrees);
    s->companion = services.modal().register_tour_controls(owner, {services.interactions().input_root(s->view.panel)});
    mounted_.push_back(id);
    const auto steps = read_prop(s->props.steps_);
    if (!alive(services, id, s)) {
        return;
    }
    reconcile(id, steps);
    const auto connect = [&]<class T, class F>(const Prop<T>& prop, F apply) {
        if (!alive(services, id, s)) {
            return;
        }
        connect_prop(build.scope(id), prop, [this, id, s, apply](T value) {
            if (!alive(*services_, id, s)) {
                return;
            }
            apply(*s, std::move(value));
            if (alive(*services_, id, s)) {
                refresh(id);
            }
        });
    };
    connect(s->props.steps_, [this, id](auto&, auto value) { reconcile(id, std::move(value)); });
    if (!alive(services, id, s)) {
        return;
    }
    if (s->props.open_) {
        connect(*s->props.open_, [this, id](auto& value, bool open) {
            value.open = open;
            if (!open) {
                deactivate(id, true);
            }
        });
    }
    if (!alive(services, id, s)) {
        return;
    }
    if (s->props.current_) {
        connect(*s->props.current_, [](auto& value, std::size_t current) {
            value.current = current;
            value.focus_pending = true;
        });
    }
    connect(s->props.keyboard_, [](auto& value, bool enabled) { value.keyboard = enabled; });
    connect(s->props.mask_, [](auto& value, bool enabled) { value.mask = enabled; });
    connect(s->props.visible_, [this, id](auto& value, bool enabled) {
        value.shown = enabled;
        if (!enabled) {
            deactivate(id, true);
        }
    });
    connect(s->props.disabled_, [this, id](auto& value, bool disabled) {
        value.disabled = disabled;
        if (disabled) {
            deactivate(id, true);
        }
    });
    connect(s->props.direction_, [](auto& value, TextDirection direction) {
        if (direction != TextDirection::LeftToRight && direction != TextDirection::RightToLeft) {
            throw std::invalid_argument("Tour direction is invalid");
        }
        value.rtl = direction == TextDirection::RightToLeft;
    });
    connect(locale, [](auto& value, TourLocale locale) { value.locale = std::move(locale); });
    if (!alive(services, id, s)) {
        return;
    }
    const auto theme = build.theme_scope();
    s->theme = theme->capture([this, id](auto) { refresh(id); },
                              [theme] {
                                  static_cast<void>(theme->tour());
                                  static_cast<void>(theme->text_font_family());
                                  static_cast<void>(theme->text_font_weight());
                                  static_cast<void>(theme->text_font_size());
                                  static_cast<void>(theme->text_line_height());
                              });
}

void TourComponentHost::prepare_auxiliary_layout() {
    if (updating_) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        const auto s = state(*services_, id);
        if (s && (!active_ || !s->open || !s->shown || s->disabled || s->steps.empty() ||
                  !services_->components().branch_active(id))) {
            deactivate(id, active_);
        }
    }
}

void TourComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect document_clip) {
    viewport_ = viewport;
    document_clip_ = document_clip;
}

void TourComponentHost::synchronize_retained_translations() {
    std::optional<runtime::ComponentId> anchor;
    std::size_t maximum_depth = 0;
    const auto consider = [&](runtime::ComponentId candidate) {
        std::size_t depth = 0;
        for (auto ancestor = services_->components().parent(candidate); ancestor;
             ancestor = services_->components().parent(*ancestor)) {
            ++depth;
        }
        if (!anchor || depth > maximum_depth) {
            anchor = candidate;
            maximum_depth = depth;
        }
    };
    for (auto id : mounted_) {
        const auto s = state(*services_, id);
        if (!s) {
            continue;
        }
        consider(id);
        if (!s->steps.empty()) {
            const auto& step = s->steps[std::min(s->current, s->steps.size() - 1)];
            if (step.target_ && step.target_->state_->window == services_ && step.target_->state_->root.valid()) {
                consider(step.target_->state_->root.component());
            }
        }
    }
    if (anchor) {
        // Target references are real descendants of their translating owners.
        // The existing ancestry-ordered phase settles those owners before this
        // geometry read, including targets inside virtual Tables and popups.
        services_->enqueue_retained_translation(*anchor, [this] {
            try {
                position(viewport_, document_clip_);
            } catch (...) {
                // The ordering anchor can be an external target. A failed Tour
                // factory retires the Tour, never that unrelated target.
                positioning_error_ = std::current_exception();
                if (positioning_owner_) {
                    services_->retire_failed_component(*positioning_owner_);
                }
            }
            positioning_owner_.reset();
        });
    }
}

void TourComponentHost::position(runtime::Size viewport, runtime::Rect document_clip) {
    if (updating_) {
        return;
    }
    updating_ = true;

    struct Reset {
        bool& value;

        ~Reset() {
            value = false;
        }
    } reset{updating_};

    const auto ids = mounted_;
    for (auto id : ids) {
        positioning_owner_ = id;
        const auto s = state(*services_, id);
        if (!s || !alive(*services_, id, s)) {
            continue;
        }
        if (!active_ || !s->open || !s->shown || s->disabled || s->steps.empty() ||
            !services_->components().branch_active(id)) {
            deactivate(id, active_);
            continue;
        }
        const auto revision = s->revision;
        const auto focus_ticket = services_->focus().focus_request_revision();
        const auto current = [&] {
            return alive(*services_, id, s) && revision == s->revision;
        };
        const auto index = std::min(s->current, s->steps.size() - 1);
        const auto step = s->steps[index];
        const auto token = services_->components().theme_scope(id)->snapshot().tour();
        const auto& text = services_->components().theme_scope(id)->snapshot().text();
        std::optional<runtime::ComponentId> target;
        runtime::Rect hole;
        if (step.target_) {
            const auto ref = step.target_->state_;
            if (ref->window == services_ && services_->interactions().owns_input_root(ref->root) && ref->root.valid()) {
                const auto candidate = ref->root.component();
                const auto modal = services_->modal().effective_input_domain();
                if ((!modal || descendant(*services_, candidate, *modal)) &&
                    services_->interactions().subtree_intersects_input_domain(candidate, &s->restriction)) {
                    const auto bounds = window_bounds(*services_, candidate);
                    const auto clip =
                        services_->nodes().content_clip(services_->components().root(candidate),
                                                        services_->component_clip(candidate, viewport, document_clip));
                    hole = graphics::intersect_effect_bounds(bounds, clip);
                    if (hole.width > 0 && hole.height > 0) {
                        target = candidate;
                    }
                }
            }
        }
        // Select the complete candidate before publishing any restriction. This
        // preserves an admitted target's containing popup on first activation.
        const auto panel_root = services_->interactions().input_root(s->view.panel);
        input::InputConstraintSpec policy{{panel_root}, true};
        if (target) {
            policy.roots.push_back(services_->interactions().input_root(*target));
        }
        if (!s->view.visible) {
            s->restore = services_->focus().state().focused;
            s->focus_pending = true;
        }
        activate(*services_, s->layer, true);
        if (!current()) {
            continue;
        }
        if (!s->pages.contains(step.key_.bytes())) {
            runtime::ComponentId page;
            const auto data = std::make_shared<StepView>(step);
            connect_prop(*data->bindings, step.title_,
                         [title = data->title](String value) { title.set(std::move(value)); });
            if (!current()) {
                continue;
            }
            connect_prop(*data->bindings, step.description_,
                         [description = data->description](String value) { description.set(std::move(value)); });
            if (!current()) {
                continue;
            }
            try {
                mount_retained_slot(*services_, s->content, Content{[&] {
                    page = box(*services_, s->content, Content{[&] {
                        Flex(FlexProps{}.vertical(true).gap(dp(token.gap)), FlexContent{[&] {
                                 if (step.cover_) {
                                     SlotContentAccess::function (*step.cover_)();
                                 }
                                 if (!current()) {
                                     return;
                                 }
                                 if (step.title_slot_) {
                                     SlotContentAccess::function (*step.title_slot_)();
                                 } else {
                                     Text(TextProps{}.content(data->title));
                                 }
                                 if (!current()) {
                                     return;
                                 }
                                 if (step.description_slot_) {
                                     SlotContentAccess::function (*step.description_slot_)();
                                 } else {
                                     Text(TextProps{}.content(data->description));
                                 }
                             }});
                    }},
                               false, s);
                }});
            } catch (...) {
                services_->retire_failed_component(id);
                throw;
            }
            if (!current()) {
                if (services_->components().contains(page)) {
                    services_->destroy(page);
                }
                continue;
            }
            data->component = page;
            services_->components().scope(page).on_cleanup([data] { data->bindings.reset(); });
            s->pages.emplace(step.key_.bytes(), data);
        }
        for (const auto& [key, page] : s->pages) {
            activate(*services_, page->component, key == step.key_.bytes());
            if (!current()) {
                break;
            }
        }
        if (!current()) {
            continue;
        }
        s->view.content = s->pages.at(std::string{step.key_.bytes()})->component;
        s->foreground.set({token.text.red(), token.text.green(), token.text.blue(), token.text.alpha()});
        if (!current()) {
            continue;
        }
        s->typography.set({text.font_family, text.font_weight, false, text.font_size, text.line_height});
        if (!current()) {
            continue;
        }
        const auto locale = s->locale;
        s->previous_label.set(locale.previous);
        if (!current()) {
            continue;
        }
        s->next_label.set(index + 1 == s->steps.size() ? locale.finish : locale.next);
        if (!current()) {
            continue;
        }
        s->close_label.set(locale.close);
        if (!current()) {
            continue;
        }
        s->previous_disabled.set(index == 0);
        if (!current()) {
            continue;
        }
        services_->components().set_window_layer(s->layer, token.z_index);
        const std::optional<input::InputConstraintSpec> desired = s->mask ? std::optional{policy} : std::nullopt;
        if (desired != s->policy || !s->view.visible) {
            s->policy = desired;
            const std::array updates{input::InputConstraintUpdate{s->restriction, desired}};
            static_cast<void>(s->companion.update(true, updates));
            s->input_enabled = true;
            if (!current()) {
                continue;
            }
            static_cast<void>(services_->synchronize_input_constraints());
            if (!current()) {
                continue;
            }
        }
        if (!services_->interactions().subtree_intersects_input_domain(s->view.panel, &s->restriction)) {
            deactivate(id, false);
            continue;
        }
        const auto modal_domain = services_->modal().effective_input_domain();
        if (!modal_domain || !descendant(*services_, *modal_domain, s->view.panel)) {
            if (services_->components().set_window_layer_stack(s->layer, s->companion.stack_order())) {
                services_->mark_scene_structure_dirty();
            }
        }
        const auto panel_node = services_->components().root(s->view.panel);
        static_cast<void>(
            services_->layout().measure_child(services_->components().root(s->layer),
                                              {viewport.width, viewport.width, viewport.height, viewport.height}));
        const auto margin = std::min({token.viewport_margin, viewport.width / 2, viewport.height / 2});
        const auto width = std::max(0.0F, viewport.width - margin * 2);
        const auto height = std::max(0.0F, viewport.height - margin * 2);
        const auto size = services_->layout().measure_child(panel_node, {0, width, 0, height});
        runtime::Rect panel{(viewport.width - size.width) / 2, (viewport.height - size.height) / 2, size.width,
                            size.height};
        if (target && step.placement_ != TourPlacement::Center) {
            const auto gap = token.target_gap;
            panel.x = hole.x + (hole.width - size.width) / 2;
            panel.y = hole.y + hole.height + gap;
            if (step.placement_ == TourPlacement::Top || panel.y + size.height > viewport.height - margin) {
                panel.y = hole.y - gap - size.height;
            }
            if (step.placement_ == TourPlacement::Start || step.placement_ == TourPlacement::End) {
                const auto left = (step.placement_ == TourPlacement::Start) != s->rtl;
                panel.x = left ? hole.x - size.width - gap : hole.x + hole.width + gap;
                panel.y = hole.y + (hole.height - size.height) / 2;
            }
        }
        panel.x = std::clamp(panel.x, margin, std::max(margin, viewport.width - margin - panel.width));
        panel.y = std::clamp(panel.y, margin, std::max(margin, viewport.height - margin - panel.height));
        services_->layout().place_child(services_->components().root(s->layer),
                                        {0, 0, viewport.width, viewport.height});
        services_->layout().place_child(panel_node, panel);
        translate(*services_, panel_node, {});
        s->view.bounds = panel;
        s->view.target = target;
        s->view.target_bounds = hole;
        s->view.centered = !target || step.placement_ == TourPlacement::Center;
        s->view.current = index;
        s->view.total = s->steps.size();
        s->view.visible = true;
        if (std::exchange(s->focus_pending, false)) {
            const auto hit = button_hit(*services_, s->view.next);
            if (hit.valid()) {
                static_cast<void>(
                    services_->focus().defer_focus_if_current(focus_ticket, hit, input::FocusModality::keyboard));
            }
        }
    }
}

void TourComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect) {
    if (positioning_error_) {
        std::rethrow_exception(std::exchange(positioning_error_, {}));
    }
    for (auto id : mounted_) {
        const auto s = state(*services_, id);
        if (!s || !s->view.visible) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().tour();
        std::vector<graphics::RoundedEffectInstance> effects;
        const runtime::Rect clip{0, 0, viewport.width, viewport.height};
        const auto quad = [&](runtime::Rect rect, Color color) {
            for (auto effect : graphics::make_corner_fill_effects({rect, 0}, {false, false, false, false}, color,
                                                                  services_->nodes().content_opacity(s->node), {},
                                                                  graphics::EffectClip{1, clip})) {
                effects.push_back(std::move(effect));
            }
        };
        if (s->mask) {
            const auto h = s->view.target ? s->view.target_bounds : runtime::Rect{};
            if (s->view.target) {
                quad({0, 0, viewport.width, h.y}, token.mask);
                quad({0, h.y + h.height, viewport.width, std::max(0.0F, viewport.height - h.y - h.height)}, token.mask);
                quad({0, h.y, h.x, h.height}, token.mask);
                quad({h.x + h.width, h.y, std::max(0.0F, viewport.width - h.x - h.width), h.height}, token.mask);
                const auto b = token.highlight_width;
                quad({h.x, h.y, h.width, std::min(b, h.height)}, token.highlight);
                quad({h.x, h.y + std::max(0.0F, h.height - b), h.width, std::min(b, h.height)}, token.highlight);
                quad({h.x, h.y, std::min(b, h.width), h.height}, token.highlight);
                quad({h.x + std::max(0.0F, h.width - b), h.y, std::min(b, h.width), h.height}, token.highlight);
            } else {
                quad(clip, token.mask);
            }
        }
        for (auto effect : graphics::make_corner_fill_effects(
                 {s->view.bounds, token.radius}, {true, true, true, true}, token.background,
                 services_->nodes().content_opacity(s->node), {}, graphics::EffectClip{1, clip})) {
            effects.push_back(std::move(effect));
        }
        static_cast<void>(services_->surfaces().update_content_effects(s->surface, effects));
    }
}

void TourComponentHost::on_window_active(bool active) {
    active_ = active;
    if (!active) {
        const auto ids = mounted_;
        for (auto id : ids) {
            deactivate(id, false);
        }
    } else {
        for (auto id : mounted_) {
            refresh(id);
        }
    }
}

void TourComponentHost::on_input_boundary_changed() {
    if (updating_) {
        return;
    }
    // Re-evaluate in the next ordered layout pass; no speculative timer or
    // controls-only publication is introduced during boundary callbacks.
    for (auto id : mounted_) {
        refresh(id);
    }
}

bool TourComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.key != input::Key::escape || event.action != input::KeyAction::down || !active_) {
        return false;
    }
    const auto ids = mounted_;
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        const auto s = state(*services_, *it);
        if (s && s->keyboard && s->view.visible && services_->interactions().component_active(s->view.panel)) {
            const auto focused = services_->focus().state().focused;
            const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
            if (record) {
                for (auto ancestor = std::optional{record->component}; ancestor;
                     ancestor = services_->components().parent(*ancestor)) {
                    if (services_->components().is_window_layer_root(*ancestor)) {
                        if (*ancestor != s->layer) {
                            return false;
                        }
                        break;
                    }
                }
            }
            close(*it, TourCloseReason::Escape);
            return true;
        }
    }
    return false;
}
} // namespace ryn::detail

namespace ryn {
TourTargetRef::TourTargetRef() : state_(std::make_shared<detail::TourTargetState>()) {}

bool TourTargetRef::bound() const {
    return state_->window && state_->root.valid();
}

void TourTarget(TourTargetRef ref, TourTargetContent content) {
    if (!detail::active_tour) {
        throw std::logic_error("TourTarget requires window component services");
    }
    detail::active_tour->mount_target(std::move(ref), std::move(content));
}

void Tour(TourProps props) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_tour) {
        throw std::logic_error("Tour requires window component services");
    }
    detail::active_tour->mount(std::move(props));
}
} // namespace ryn
