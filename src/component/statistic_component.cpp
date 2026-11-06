#include "component/statistic_component.hpp"
#include "component/skeleton_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <limits>

namespace ryn::detail {
namespace {
thread_local StatisticComponentHost* active_statistic{};
constexpr float infinity = std::numeric_limits<float>::infinity();

struct StatisticState final {
    runtime::NodeId node;
    bool ready{};
    std::optional<runtime::ComponentId> parent;
    std::array<runtime::ComponentId, 4> regions{};
    runtime::ComponentId skeleton;
    runtime::ComponentId wrapper;
    runtime::ComponentId content;
    StatisticSnapshot snapshot;
    StatisticModel model;
    std::uint64_t model_revision{};
    std::array<bool, 4> rich{};
    Signal<bool> loading{false};
    Signal<String> title{String{}};
    Signal<String> value{String{}};
    Signal<String> prefix{String{}};
    Signal<String> suffix{String{}};
    Signal<runtime::SemanticForeground> title_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> content_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> title_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> content_font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 2> subscriptions;
    bool animated{};
    bool geometry_visible{};
    StatisticValue displayed;
    std::optional<animation::AnimationTime> value_start;
    std::int64_t value_duration{};
    double value_from{};
    double value_target{};
    std::optional<StatisticTimer> timer;
    animation::AnimationTime timer_origin;
    std::uint64_t timer_revision{};
    bool timer_finished{};
    std::optional<std::int64_t> timer_sample;
    std::optional<animation::AnimationTime> deadline;
    std::function<void(Duration)> on_change;
    std::function<void()> on_finish;
};

std::optional<double> animatable_number(const StatisticValue& value) {
    return std::visit(
        [](const auto& item) -> std::optional<double> {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::same_as<T, String>) {
                return {};
            } else {
                const auto number = static_cast<double>(item);
                return std::isfinite(number) && std::abs(number) <= 9007199254740991.0 ? std::optional{number}
                                                                                       : std::nullopt;
            }
        },
        value.storage());
}

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

std::array<runtime::ComponentId, 4> regions(const StatisticSnapshot& s) {
    return {s.title, s.prefix, s.value, s.suffix};
}

bool present(const StatisticState& s, std::size_t index) {
    return s.rich[index] || (index == 0   ? !s.title.get().empty()
                             : index == 1 ? !s.prefix.get().empty()
                             : index == 2 ? !s.value.get().empty()
                                          : !s.suffix.get().empty());
}
} // namespace

StatisticComponentHost::StatisticComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

StatisticComponentHost::~StatisticComponentHost() {
    services_->detach(*this);
}

void* StatisticComponentHost::begin_mount() noexcept {
    auto* previous = active_statistic;
    active_statistic = this;
    return previous;
}

void StatisticComponentHost::end_mount(void* previous) noexcept {
    active_statistic = static_cast<StatisticComponentHost*>(previous);
}

void StatisticComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void StatisticComponentHost::on_dispose() noexcept {
    mounted_.clear();
}

StatisticSnapshot StatisticComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<StatisticState>(id);
    if (!s) {
        throw std::out_of_range("Statistic component is stale");
    }
    return s->snapshot;
}

bool StatisticComponentHost::valid_composition(runtime::ComponentId id, bool complete) {
    auto& components = services_->components();
    const auto* s = components.state<StatisticState>(id);
    if (!s || !components.scope(id).active()) {
        return false;
    }
    complete |= s->ready;
    const auto edge = [&](auto child, auto parent) {
        return components.contains(child) && components.contains(parent) &&
               components.parent(child) == std::optional{parent};
    };
    bool valid = components.parent(id) == s->parent;
    for (std::size_t i = 0; i < s->regions.size(); ++i) {
        valid &= s->regions[i].valid() ? edge(s->regions[i], i == 0 ? id : s->content) : !complete;
    }
    valid &= s->skeleton.valid() ? edge(s->skeleton, id) : !complete;
    valid &= s->wrapper.valid() ? edge(s->wrapper, s->skeleton) : !complete;
    valid &= s->content.valid() ? edge(s->content, s->wrapper) : !complete;
    if (!valid) {
        // Retire only the captured generation, including when a callback reused a private child's index.
        services_->destroy(id);
    }
    return valid;
}

void StatisticComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    if (!valid_composition(id)) {
        return;
    }
    auto* s = services_->components().state<StatisticState>(id);
    if (!s->ready) {
        return;
    }
    const auto t = services_->components().theme_scope(id)->snapshot().statistic();
    const auto title_color = s->title_color;
    const auto content_color = s->content_color;
    const auto title_font = s->title_font;
    const auto content_font = s->content_font;
    title_color.set(foreground(t.colors.title));
    if (!valid_composition(id, true)) {
        return;
    }
    content_color.set(foreground(t.colors.content));
    if (!valid_composition(id, true)) {
        return;
    }
    title_font.set({t.metrics.font_family, 400, false, t.metrics.title_font_size, t.metrics.title_line_height});
    if (!valid_composition(id, true)) {
        return;
    }
    content_font.set({t.metrics.font_family, 400, false, t.metrics.content_font_size, t.metrics.content_line_height});
    if (!valid_composition(id, true)) {
        return;
    }
    s = services_->components().state<StatisticState>(id);
    geometry |= services_->components().set_branch_active(id, s->snapshot.visible);
    services_->nodes().require(s->node).out_of_flow = !s->snapshot.visible;
    const auto parts = regions(s->snapshot);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const bool shown = present(*s, i);
        geometry |= services_->components().set_branch_active(parts[i], shown);
        services_->nodes().require(services_->components().root(parts[i])).out_of_flow = !shown;
    }
    if (geometry) {
        services_->mark_scene_structure_dirty();
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
    // Input reconciliation may synchronously dispose this entire generation.
    const bool blocked = !s->snapshot.visible;
    services_->set_input_blocked(id, blocked);
    static_cast<void>(valid_composition(id, true));
}

template <typename T, typename Apply, typename Equal>
void StatisticComponentHost::watch(runtime::ComponentId id, const Prop<T>& prop, Apply apply, Equal equal) {
    if (!valid_composition(id)) {
        return;
    }
    auto run = [this, id, prop, apply = std::move(apply), equal = std::move(equal),
                last = std::optional<T>{}]() mutable {
        {
            auto value = read_prop(prop);
            if (!valid_composition(id)) {
                return;
            }
            if (!last || !std::invoke(equal, *last, value)) {
                last = value;
                apply(std::move(value));
            }
        }
        static_cast<void>(valid_composition(id));
    };
    if (PropAccess::static_value(prop)) {
        run();
        return;
    }
    auto observer = detail::observe(detail::ObserverPhase::binding, std::move(run), false);
    // Own/deactivate before the first getter/apply. connect_binding owns after run(), when the
    // component Scope may already have been destroyed by user code.
    services_->components().scope(id).on_cleanup([observer] { observer->deactivate(); });
    observer->run();
}

template <typename T>
void StatisticComponentHost::connect(runtime::ComponentId id, const Prop<T>& prop, T StatisticModel::* member) {
    watch(
        id, prop,
        [this, id, member](T value) {
            const auto* current = services_->components().state<StatisticState>(id);
            if (!current) {
                return;
            }
            if constexpr (!std::same_as<T, StatisticFormatter>) {
                if (current->model.*member == value) {
                    return;
                }
            }
            auto candidate = current->model;
            candidate.*member = std::move(value);
            if (!current->ready) {
                auto* s = services_->components().state<StatisticState>(id);
                s->model = std::move(candidate);
                ++s->model_revision;
                return;
            }
            const auto revision = current->model_revision;
            // formatter is user code. Do not retain a borrowed state across it.
            auto validation = candidate;
            if (current->timer) {
                validation.formatter = {};
            }
            auto text = format_statistic(validation);
            if (!valid_composition(id, true)) {
                return;
            }
            auto* s = services_->components().state<StatisticState>(id);
            if (s->model_revision != revision) {
                return;
            }
            s->value_start.reset();
            s->deadline.reset();
            if constexpr (std::same_as<T, StatisticValue>) {
                const auto from = animatable_number(s->displayed);
                const auto to = animatable_number(candidate.value);
                const auto policy = animation::resolve_motion_policy(
                    services_->components().theme_scope(id)->snapshot(), services_->motion_preference());
                if (s->animated && !s->timer && !candidate.formatter && from && to && *from != *to &&
                    dynamic_allowed(id) && policy.tokens().slow.count_microseconds() > 0) {
                    s->value_from = *from;
                    s->value_target = *to;
                    s->value_start = clock_.observe(services_->animation_time()).effective;
                    s->value_duration = policy.tokens().slow.count_microseconds();
                }
            }
            s->model = std::move(candidate);
            ++s->model_revision;
            if (s->timer || s->value_start) {
                services_->request_frame();
                update_dynamic(id, services_->animation_time());
            } else {
                s->displayed = s->model.value;
                s->snapshot.animating = false;
                publish_text(id, std::move(text));
            }
        },
        [](const T& a, const T& b) {
            if constexpr (std::same_as<T, StatisticFormatter>) {
                static_cast<void>(a);
                static_cast<void>(b);
                return false;
            } else {
                return a == b;
            }
        });
}

void StatisticComponentHost::mount(const StatisticProps& props) {
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<StatisticState>();

    struct MountRollback final {
        WindowComponentServices& services;
        runtime::ComponentId owner;
        bool committed{};

        ~MountRollback() noexcept {
            if (!committed) {
                static_cast<void>(services.retire_failed_component(owner));
            }
        }
    } rollback{services, id};

    const auto root_node = build.root(id);
    auto& s = build.state<StatisticState>(id);
    s.node = root_node;
    s.parent = services.components().parent(id);
    s.model.explicit_decimal = props.explicit_decimal_;
    s.model.explicit_group = props.explicit_group_;
    s.on_change = props.on_change_;
    s.on_finish = props.on_finish_;
    s.rich = {props.rich_title_.has_value(), props.rich_prefix_.has_value(), props.rich_value_.has_value(),
              props.rich_suffix_.has_value()};
    // Keep owned signal handles, never a state reference, across user slot execution.
    const auto text_signals = std::array{s.title, s.prefix, s.value, s.suffix};
    const auto title_color = s.title_color;
    const auto content_color = s.content_color;
    const auto title_font = s.title_font;
    const auto content_font = s.content_font;
    const auto loading_signal = s.loading;
    services.nodes().require(root_node).clip_content = true;
    build.on_resource_cleanup(id, [&services, id, node = root_node] {
        services.interactions().set_input_blocked(id, false);
        services.layout().remove_layout(node);
    });
    const auto valid_mount = [this, id](bool complete = false) {
        return valid_composition(id, complete);
    };
    watch(id, props.visible_, [this, id](bool value) {
        services_->components().state<StatisticState>(id)->snapshot.visible = value;
        refresh(id, true);
    });
    if (!valid_mount()) {
        return;
    }
    watch(id, props.loading_, [this, id, loading_signal](bool value) {
        auto* current = services_->components().state<StatisticState>(id);
        current->snapshot.loading = value;
        if (current->ready) {
            loading_signal.set(value);
        }
        refresh(id, true);
    });
    if (!valid_mount()) {
        return;
    }
    // Keep user getters in the guarded, already-owned observers. The shared layout adapter
    // and private Skeleton receive only owned Signal bindings, with no user getter/apply.
    LayoutStyle style;
    const auto bridge = [&]<typename T>(const std::optional<Prop<T>>& source,
                                        LayoutStyle& (LayoutStyle::*setter)(Prop<T>)) {
        if (source) {
            Signal<T> signal{T{}};
            watch(id, *source, [signal](T value) { signal.set(std::move(value)); });
            (style.*setter)(signal);
        }
    };
    bridge(LayoutStyleAccess::width(props.layout_), &LayoutStyle::width);
    bridge(LayoutStyleAccess::height(props.layout_), &LayoutStyle::height);
    bridge(LayoutStyleAccess::min_width(props.layout_), &LayoutStyle::min_width);
    bridge(LayoutStyleAccess::max_width(props.layout_), &LayoutStyle::max_width);
    bridge(LayoutStyleAccess::min_height(props.layout_), &LayoutStyle::min_height);
    bridge(LayoutStyleAccess::max_height(props.layout_), &LayoutStyle::max_height);
    bridge(LayoutStyleAccess::margin_left(props.layout_), &LayoutStyle::margin_left);
    bridge(LayoutStyleAccess::margin_top(props.layout_), &LayoutStyle::margin_top);
    bridge(LayoutStyleAccess::margin_right(props.layout_), &LayoutStyle::margin_right);
    bridge(LayoutStyleAccess::margin_bottom(props.layout_), &LayoutStyle::margin_bottom);
    bridge(LayoutStyleAccess::flex_grow(props.layout_), &LayoutStyle::flex_grow);
    bridge(LayoutStyleAccess::flex_shrink(props.layout_), &LayoutStyle::flex_shrink);
    bridge(LayoutStyleAccess::flex_basis(props.layout_), &LayoutStyle::flex_basis);
    bridge(LayoutStyleAccess::align_self(props.layout_), &LayoutStyle::align_self);
    bridge(LayoutStyleAccess::order(props.layout_), &LayoutStyle::order);
    if (!valid_mount()) {
        return;
    }
    runtime::connect_layout_style(build.scope(id), style, root_node, services.nodes(), services.dirty());
    if (!valid_mount()) {
        return;
    }
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            const auto region = [&](std::size_t index, runtime::ComponentId parent, Content body, bool title) {
                runtime::ComponentId child;
                if (!valid_mount()) {
                    return child;
                }
                context.mount_slot(
                    parent, Content{[&] {
                        auto& c = runtime::require_component_build_context();
                        child = c.mount_component<int>(0);
                        services.components().state<StatisticState>(id)->regions[index] = child;
                        if (!valid_mount()) {
                            return;
                        }
                        const auto node = c.root(child);
                        services.nodes().require(node).clip_content = true;
                        services.nodes().require(node).clip_decorations = false;
                        services.layout().set_layout(
                            node,
                            layout::ComponentLayout{
                                [&services](layout::LayoutEngine& engine, runtime::NodeId node,
                                            layout::Constraints limits) {
                                    runtime::Size natural{};
                                    for (auto descendant : services.nodes().require(node).children) {
                                        const auto size = engine.measure_child(descendant, {0, infinity, 0, infinity});
                                        natural.width = std::max(natural.width, size.width);
                                        natural.height = std::max(natural.height, size.height);
                                    }
                                    return limits.constrain(natural);
                                },
                                [&services](layout::LayoutEngine& engine, runtime::NodeId node, runtime::Rect bounds) {
                                    for (auto descendant : services.nodes().require(node).children) {
                                        const auto size = services.nodes().require(descendant).layout_size;
                                        engine.place_child(descendant, {bounds.x, bounds.y, size.width, size.height});
                                    }
                                }});
                        c.on_resource_cleanup(child, [&services, node] { services.layout().remove_layout(node); });
                        c.mount_slot_with_semantic_text_style(
                            child, Content{[&] {
                                // Run before mount_slot's ordinary child rollback can deliver blur.
                                MountRollback failed_slot{services, id};
                                SlotContentAccess::function(body)();
                                failed_slot.committed = true;
                            }},
                            Prop<runtime::SemanticForeground>{title ? title_color : content_color},
                            Prop<runtime::SemanticTypography>{title ? title_font : content_font});
                    }});
                return child;
            };
            const auto title_region = region(0, id, Content{[slot = props.rich_title_, text = text_signals[0]] {
                                                 if (slot) {
                                                     SlotContentAccess::function (*slot)();
                                                 } else {
                                                     Text(TextProps{}.content(text));
                                                 }
                                             }},
                                             true);
            if (!valid_mount()) {
                return;
            }
            services.components().state<StatisticState>(id)->snapshot.title = title_region;
            loading_signal.set(services.components().state<StatisticState>(id)->snapshot.loading);
            if (!valid_mount()) {
                return;
            }
            Skeleton(SkeletonProps{}.loading(loading_signal).paragraph(false), SkeletonContent{[&] {
                         if (!valid_mount()) {
                             return;
                         }
                         auto& c = runtime::require_component_build_context();
                         const auto content = c.mount_component<int>(0);
                         if (!services.components().contains(id)) {
                             return;
                         }
                         const auto wrapper = services.components().contains(content)
                                                  ? services.components().parent(content)
                                                  : std::nullopt;
                         const auto skeleton = wrapper && services.components().contains(*wrapper)
                                                   ? services.components().parent(*wrapper)
                                                   : std::nullopt;
                         if (!wrapper || !skeleton) {
                             services.destroy(id);
                             return;
                         }
                         auto* current = services.components().state<StatisticState>(id);
                         current->content = content;
                         current->wrapper = *wrapper;
                         current->skeleton = *skeleton;
                         if (!valid_mount()) {
                             return;
                         }
                         current->snapshot.content = content;
                         current->snapshot.skeleton = *skeleton;
                         const auto node = c.root(content);
                         services.nodes().require(node).clip_content = true;
                         services.nodes().require(node).clip_decorations = false;
                         services.layout().set_layout(
                             node, layout::ComponentLayout{
                                       [this, id](layout::LayoutEngine& e, runtime::NodeId,
                                                  layout::Constraints limits) { return measure_value(id, e, limits); },
                                       [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect bounds) {
                                           place_value(id, e, bounds);
                                       }});
                         c.on_resource_cleanup(content, [&services, node] { services.layout().remove_layout(node); });
                         c.mount_slot(
                             content, Content{[&] {
                                 const auto prefix_region =
                                     region(1, content, Content{[slot = props.rich_prefix_, text = text_signals[1]] {
                                                if (slot) {
                                                    SlotContentAccess::function (*slot)();
                                                } else {
                                                    Text(TextProps{}.content(text));
                                                }
                                            }},
                                            false);
                                 if (!valid_mount()) {
                                     return;
                                 }
                                 services.components().state<StatisticState>(id)->snapshot.prefix = prefix_region;
                                 const auto value_region =
                                     region(2, content, Content{[slot = props.rich_value_, text = text_signals[2]] {
                                                if (slot) {
                                                    SlotContentAccess::function (*slot)();
                                                } else {
                                                    Text(TextProps{}.content(text));
                                                }
                                            }},
                                            false);
                                 if (!valid_mount()) {
                                     return;
                                 }
                                 services.components().state<StatisticState>(id)->snapshot.value = value_region;
                                 const auto suffix_region =
                                     region(3, content, Content{[slot = props.rich_suffix_, text = text_signals[3]] {
                                                if (slot) {
                                                    SlotContentAccess::function (*slot)();
                                                } else {
                                                    Text(TextProps{}.content(text));
                                                }
                                            }},
                                            false);
                                 if (!valid_mount()) {
                                     return;
                                 }
                                 services.components().state<StatisticState>(id)->snapshot.suffix = suffix_region;
                             }});
                         static_cast<void>(valid_mount());
                     }});
            if (!valid_mount(true)) {
                return;
            }
            const auto skeleton = services.components().state<StatisticState>(id)->skeleton;
            services.nodes().require(services.components().root(skeleton)).clip_decorations = false;
        }});
    if (!valid_mount(true)) {
        return;
    }
    services.layout().set_layout(
        root_node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) { return measure(id, e, c); },
            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) { place(id, e, r); }});
    mounted_.push_back(id);
    connect(id, props.value_, &StatisticModel::value);
    if (!valid_mount(true)) {
        return;
    }
    connect(id, props.precision_, &StatisticModel::precision);
    if (!valid_mount(true)) {
        return;
    }
    connect(id, props.decimal_separator_, &StatisticModel::decimal_separator);
    if (!valid_mount(true)) {
        return;
    }
    connect(id, props.group_separator_, &StatisticModel::group_separator);
    if (!valid_mount(true)) {
        return;
    }
    connect(id, props.formatter_, &StatisticModel::formatter);
    if (!valid_mount(true)) {
        return;
    }
    connect(id, props.locale_, &StatisticModel::locale);
    if (!valid_mount(true)) {
        return;
    }
    watch(id, props.animated_, [this, id](bool value) {
        auto* current = services_->components().state<StatisticState>(id);
        current->animated = value;
        if (current->ready) {
            update_dynamic(id, services_->animation_time());
        }
    });
    if (!valid_mount(true)) {
        return;
    }
    watch(id, props.timer_, [this, id](std::optional<StatisticTimer> value) {
        if (value) {
            validate_statistic_timer(*value);
        }
        auto* current = services_->components().state<StatisticState>(id);
        std::optional<String> static_text;
        if (current->ready && !value) {
            const auto model_revision = current->model_revision;
            const auto timer_revision = current->timer_revision;
            auto model = current->model;
            static_text = format_statistic(model);
            if (!valid_composition(id, true)) {
                return;
            }
            current = services_->components().state<StatisticState>(id);
            if (current->model_revision != model_revision || current->timer_revision != timer_revision) {
                return;
            }
        }
        current->timer = std::move(value);
        ++current->timer_revision;
        current->timer_origin = clock_.observe(services_->animation_time()).effective;
        current->timer_finished = false;
        current->timer_sample.reset();
        current->value_start.reset();
        current->deadline.reset();
        if (current->ready) {
            services_->request_frame();
            current->snapshot.animating = false;
            current->snapshot.timer_running = false;
            if (current->timer) {
                update_dynamic(id, services_->animation_time());
            } else {
                current->displayed = current->model.value;
                publish_text(id, std::move(*static_text));
            }
        }
    });
    if (!valid_mount(true)) {
        return;
    }
    const auto text = [this, id](const Prop<String>& prop, Signal<String> StatisticState::* member) {
        watch(id, prop, [this, id, member](String value) {
            if (auto* s = services_->components().state<StatisticState>(id)) {
                const auto signal = s->*member;
                signal.set(std::move(value));
                refresh(id, true);
            }
        });
    };
    text(props.title_, &StatisticState::title);
    if (!valid_mount(true)) {
        return;
    }
    text(props.prefix_, &StatisticState::prefix);
    if (!valid_mount(true)) {
        return;
    }
    text(props.suffix_, &StatisticState::suffix);
    if (!valid_mount(true)) {
        return;
    }
    const auto theme = build.theme_scope();
    auto* current = services.components().state<StatisticState>(id);
    current->subscriptions[0] = theme->capture([this, id](auto) { refresh(id, false); },
                                               [theme] { static_cast<void>(theme->statistic_colors()); });
    current->subscriptions[1] = theme->capture([this, id](auto) { refresh(id, true); },
                                               [theme] { static_cast<void>(theme->statistic_metrics()); });
    // Every getter/subscription is now installed. Format the final model once, unless the
    // formatter itself changes a model property; then only the newer revision may publish.
    for (;;) {
        if (!valid_mount(true)) {
            return;
        }
        const auto revision = services.components().state<StatisticState>(id)->model_revision;
        auto candidate = services.components().state<StatisticState>(id)->model;
        const auto timer = services.components().state<StatisticState>(id)->timer;
        if (timer) {
            auto validation = candidate;
            validation.formatter = {};
            static_cast<void>(format_statistic(validation));
        }
        auto formatted =
            timer ? format_statistic_timer(timer->value, timer->format, candidate.locale) : format_statistic(candidate);
        if (!valid_mount(true)) {
            return;
        }
        current = services.components().state<StatisticState>(id);
        if (current->model_revision != revision) {
            continue;
        }
        current->snapshot.formatted_value = formatted;
        text_signals[2].set(std::move(formatted));
        if (!valid_mount(true)) {
            return;
        }
        current = services.components().state<StatisticState>(id);
        if (current->model_revision == revision) {
            break;
        }
    }
    // The candidate/formatter copies from the loop have also been released before this check.
    if (!valid_mount(true)) {
        return;
    }
    services.components().state<StatisticState>(id)->ready = true;
    services.components().state<StatisticState>(id)->displayed =
        services.components().state<StatisticState>(id)->model.value;
    if (const auto timer = services.components().state<StatisticState>(id)->timer) {
        services.components().state<StatisticState>(id)->snapshot.timer_value = timer->value;
    }
    loading_signal.set(services.components().state<StatisticState>(id)->snapshot.loading);
    if (!valid_mount(true)) {
        return;
    }
    refresh(id, true);
    rollback.committed = valid_mount(true);
}

bool StatisticComponentHost::dynamic_allowed(runtime::ComponentId id) const {
    const auto* s = services_->components().state<StatisticState>(id);
    return s && s->ready && s->snapshot.visible && !s->snapshot.loading && !s->rich[2] && s->geometry_visible &&
           services_->focus().state().window_active && services_->components().branch_active(id) &&
           services_->interactions().component_active(id) &&
           animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                            services_->motion_preference())
               .enabled();
}

void StatisticComponentHost::publish_text(runtime::ComponentId id, String text) {
    if (!valid_composition(id, true)) {
        return;
    }
    auto* s = services_->components().state<StatisticState>(id);
    if (s->snapshot.formatted_value == text) {
        return;
    }
    s->snapshot.formatted_value = text;
    const auto signal = s->value;
    signal.set(std::move(text));
    refresh(id, true);
}

void StatisticComponentHost::update_dynamic(runtime::ComponentId id, animation::AnimationTime now, bool notify) {
    auto* s = services_->components().state<StatisticState>(id);
    if (!s || !s->ready) {
        return;
    }
    if (!s->timer && !s->value_start) {
        s->deadline.reset();
        s->snapshot.timer_running = false;
        return;
    }
    now = clock_.observe(now).effective;
    s->deadline.reset();
    s->snapshot.timer_running = false;
    if (!dynamic_allowed(id)) {
        if (s->value_start) {
            s->value_start.reset();
            s->snapshot.animating = false;
            s->displayed = s->model.value;
            auto model = s->model;
            const auto text = format_statistic(model);
            publish_text(id, text);
        }
        return;
    }
    if (s->timer) {
        const auto timer = *s->timer;
        const auto revision = s->timer_revision;
        const auto elapsed =
            std::max<std::int64_t>(0, now.count_microseconds() - s->timer_origin.count_microseconds()) / 1000;
        const auto initial = static_cast<std::int64_t>(timer.value.count_milliseconds());
        constexpr auto maximum = std::numeric_limits<std::int64_t>::max() / 1000;
        const auto value = timer.type == StatisticTimerType::Countdown ? std::max<std::int64_t>(0, initial - elapsed)
                                                                       : initial + std::min(elapsed, maximum - initial);
        const auto duration = Duration::milliseconds(double(value));
        s->snapshot.timer_value = duration;
        const auto quantum = statistic_timer_quantum(timer.format);
        const bool finished = timer.type == StatisticTimerType::Countdown && value == 0;
        if (!s->timer_finished && (!finished || !notify)) {
            std::int64_t wait = quantum == 0                                  ? 0
                                : timer.type == StatisticTimerType::Countdown ? value % quantum + 1
                                                                              : quantum - value % quantum;
            if (timer.type == StatisticTimerType::Countdown) {
                wait = wait == 0 ? std::max<std::int64_t>(1, value) : std::min(wait, std::max<std::int64_t>(1, value));
            }
            if (wait > 0 && (timer.type == StatisticTimerType::Countdown || value < maximum)) {
                // Millisecond formats are sampled at most once per display frame.
                if (quantum > 0 && quantum < 17 && !finished) {
                    wait = std::max<std::int64_t>(17, wait);
                    if (timer.type == StatisticTimerType::Countdown) {
                        wait = std::min(wait, value);
                    }
                }
                const auto remaining_time = std::numeric_limits<std::int64_t>::max() - now.count_microseconds();
                s->deadline = now + animation::AnimationDuration::microseconds(std::min(wait * 1000, remaining_time));
                s->snapshot.timer_running = true;
            }
        }
        const auto changed = s->timer_sample != value;
        const auto on_change = s->on_change;
        const auto on_finish = s->on_finish;
        const bool finish_callback = notify && finished && !s->timer_finished;
        if (notify) {
            s->timer_sample = value;
            s->timer_finished |= finished;
        }
        auto text = format_statistic_timer(duration, timer.format, s->model.locale);
        publish_text(id, std::move(text));
        const auto current = [&]() {
            const auto* live = services_->components().state<StatisticState>(id);
            return live && live->timer_revision == revision && dynamic_allowed(id);
        };
        try {
            if (notify && changed && on_change && current()) {
                on_change(duration);
            }
            if (finish_callback && on_finish && current()) {
                on_finish();
            }
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
        return;
    }
    if (s->value_start) {
        const auto elapsed = std::max<std::int64_t>(0, now.count_microseconds() - s->value_start->count_microseconds());
        const double progress = std::clamp(double(elapsed) / double(s->value_duration), 0.0, 1.0);
        const double eased = 1 - (1 - progress) * (1 - progress) * (1 - progress);
        auto model = s->model;
        s->displayed =
            progress >= 1 ? s->model.value : StatisticValue{s->value_from + (s->value_target - s->value_from) * eased};
        model.value = s->displayed;
        s->snapshot.animating = progress < 1;
        if (progress >= 1 || !s->animated) {
            s->value_start.reset();
            s->snapshot.animating = false;
            s->displayed = model.value = s->model.value;
        } else {
            s->deadline = now + animation::AnimationDuration::microseconds(16667);
        }
        auto text = format_statistic(model);
        publish_text(id, std::move(text));
    }
}

void StatisticComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (const auto id : ids) {
        update_dynamic(id, services_->animation_time());
    }
}

void StatisticComponentHost::synchronize_auxiliary_motion() {
    prepare_auxiliary_layout();
}

void StatisticComponentHost::on_window_active(bool) {
    prepare_auxiliary_layout();
}

void StatisticComponentHost::on_input_boundary_changed() {
    prepare_auxiliary_layout();
}

void StatisticComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    const auto ids = mounted_;
    for (const auto id : ids) {
        auto* s = services_->components().state<StatisticState>(id);
        if (!s) {
            continue;
        }
        const auto& node = services_->nodes().require(s->node);
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
        const auto visible =
            graphics::intersect_effect_bounds({node.bounds.x + node.translation.x, node.bounds.y + node.translation.y,
                                               node.bounds.width, node.bounds.height},
                                              clip);
        s->geometry_visible =
            visible.width > 0 && visible.height > 0 && services_->nodes().content_opacity(s->node) > 0;
        update_dynamic(id, services_->animation_time());
    }
}

std::size_t StatisticComponentHost::tick_auxiliary(animation::AnimationTime now) {
    const auto ids = mounted_;
    std::size_t changed{};
    for (const auto id : ids) {
        const auto* s = services_->components().state<StatisticState>(id);
        if (s && s->deadline && now >= *s->deadline) {
            update_dynamic(id, now, true);
            ++changed;
        }
    }
    return changed;
}

std::optional<animation::AnimationTime> StatisticComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> result;
    for (const auto id : mounted_) {
        const auto* s = services_->components().state<StatisticState>(id);
        if (s && s->deadline && dynamic_allowed(id) && (!result || *s->deadline < *result)) {
            result = s->deadline;
        }
    }
    return result;
}

runtime::Size StatisticComponentHost::measure_value(runtime::ComponentId id, layout::LayoutEngine& e,
                                                    layout::Constraints c) {
    const auto& s = *services_->components().state<StatisticState>(id);
    const auto gap = services_->components().theme_scope(id)->snapshot().statistic().metrics.affix_gap;
    runtime::Size natural{};
    bool previous = false;
    const auto parts = regions(s.snapshot);
    for (std::size_t i = 1; i < parts.size(); ++i) {
        const auto size = e.measure_child(services_->components().root(parts[i]), {0, infinity, 0, infinity});
        if (!present(s, i)) {
            continue;
        }
        natural.width += size.width + (previous ? gap : 0);
        natural.height = std::max(natural.height, size.height);
        previous = true;
    }
    return c.constrain(natural);
}

void StatisticComponentHost::place_value(runtime::ComponentId id, layout::LayoutEngine& e, runtime::Rect r) {
    const auto& s = *services_->components().state<StatisticState>(id);
    const auto gap = services_->components().theme_scope(id)->snapshot().statistic().metrics.affix_gap;
    float x = r.x;
    bool previous = false;
    const auto parts = regions(s.snapshot);
    for (std::size_t i = 1; i < parts.size(); ++i) {
        const auto node = services_->components().root(parts[i]);
        const bool shown = present(s, i);
        if (shown && previous) {
            x += std::min(gap, std::max(0.0F, r.x + r.width - x));
        }
        const auto natural = services_->nodes().require(node).layout_size;
        const auto w = shown ? std::min(natural.width, std::max(0.0F, r.x + r.width - x)) : 0;
        const auto h = shown ? std::min(natural.height, r.height) : 0;
        e.place_child(node, {x, r.y + (r.height - h) / 2, w, h}, true, true);
        x += w;
        previous |= shown;
    }
}

runtime::Size StatisticComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& e, layout::Constraints c) {
    const auto& s = *services_->components().state<StatisticState>(id);
    if (!s.snapshot.visible) {
        return {};
    }
    const auto& m = services_->components().theme_scope(id)->snapshot().statistic().metrics;
    const auto measured_title =
        e.measure_child(services_->components().root(s.snapshot.title), {0, c.max_width, 0, infinity});
    const auto title = present(s, 0) ? measured_title : runtime::Size{};
    const auto content =
        e.measure_child(services_->components().root(s.snapshot.skeleton), {0, c.max_width, 0, infinity});
    return c.constrain({std::max(title.width, content.width), title.height + (present(s, 0) ? m.title_gap : 0) +
                                                                  content.height +
                                                                  (s.snapshot.loading ? m.loading_padding : 0)});
}

void StatisticComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& e, runtime::Rect r) {
    const auto& s = *services_->components().state<StatisticState>(id);
    if (!s.snapshot.visible) {
        return;
    }
    const auto& m = services_->components().theme_scope(id)->snapshot().statistic().metrics;
    const auto title_node = services_->components().root(s.snapshot.title);
    const auto skeleton_node = services_->components().root(s.snapshot.skeleton);
    const auto title = services_->nodes().require(title_node).layout_size;
    const bool shown = present(s, 0);
    const auto title_height = shown ? std::min(title.height, r.height) : 0;
    e.place_child(title_node, {r.x, r.y, r.width, title_height}, true, true);
    float y = r.y + title_height;
    y += std::min((shown ? m.title_gap : 0) + (s.snapshot.loading ? m.loading_padding : 0),
                  std::max(0.0F, r.y + r.height - y));
    e.place_child(skeleton_node, {r.x, y, r.width, std::max(0.0F, r.y + r.height - y)}, true, true);
}
} // namespace ryn::detail

namespace ryn {
void Statistic(StatisticProps props) {
    if (!detail::active_statistic) {
        throw std::logic_error("Statistic requires window component services");
    }
    detail::active_statistic->mount(props);
}
} // namespace ryn
