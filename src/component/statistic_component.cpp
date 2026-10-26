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
    StatisticSnapshot snapshot;
    StatisticModel model;
    std::array<bool, 4> rich{};
    Signal<String> title{String{}};
    Signal<String> value{String{}};
    Signal<String> prefix{String{}};
    Signal<String> suffix{String{}};
    Signal<runtime::SemanticForeground> title_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> content_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> title_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> content_font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

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

void StatisticComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<StatisticState>(id);
    if (!s || !s->ready || !services_->components().scope(id).active()) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().statistic();
    s->title_color.set(foreground(t.colors.title));
    s->content_color.set(foreground(t.colors.content));
    s->title_font.set({t.metrics.font_family, 400, false, t.metrics.title_font_size, t.metrics.title_line_height});
    s->content_font.set(
        {t.metrics.font_family, 400, false, t.metrics.content_font_size, t.metrics.content_line_height});
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
}

template <typename T>
void StatisticComponentHost::connect(runtime::ComponentId id, const Prop<T>& prop, T StatisticModel::* member) {
    if (PropAccess::static_value(prop)) {
        return;
    }
    connect_prop(
        services_->components().scope(id), prop,
        [this, id, member, initial = true](T value) mutable {
            if (std::exchange(initial, false)) {
                return;
            }
            const auto* current = services_->components().state<StatisticState>(id);
            if (!current) {
                return;
            }
            auto candidate = current->model;
            candidate.*member = std::move(value);
            // formatter is user code. Do not retain a borrowed state across it.
            auto text = format_statistic(candidate);
            auto* s = services_->components().state<StatisticState>(id);
            if (!s || !services_->components().scope(id).active()) {
                return;
            }
            s->model = std::move(candidate);
            s->snapshot.formatted_value = text;
            s->value.set(std::move(text));
            refresh(id, true);
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
    StatisticModel model{read_prop(props.value_), read_prop(props.precision_), read_prop(props.decimal_separator_),
                         read_prop(props.group_separator_), read_prop(props.formatter_)};
    auto formatted = format_statistic(model);
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<StatisticState>();
    auto& s = build.state<StatisticState>(id);
    s.node = build.root(id);
    s.model = std::move(model);
    s.snapshot.formatted_value = formatted;
    s.value.set(std::move(formatted));
    s.title.set(read_prop(props.title_));
    s.prefix.set(read_prop(props.prefix_));
    s.suffix.set(read_prop(props.suffix_));
    s.rich = {props.rich_title_.has_value(), props.rich_prefix_.has_value(), props.rich_value_.has_value(),
              props.rich_suffix_.has_value()};
    s.snapshot.visible = read_prop(props.visible_);
    s.snapshot.loading = read_prop(props.loading_);
    services.nodes().require(s.node).clip_content = true;
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.on_resource_cleanup(id, [&services, id, node = s.node] {
        services.interactions().set_input_blocked(id, false);
        services.layout().remove_layout(node);
    });
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            const auto region = [&](runtime::ComponentId parent, Content body, bool title) {
                runtime::ComponentId child;
                context.mount_slot(
                    parent, Content{[&] {
                        auto& c = runtime::require_component_build_context();
                        child = c.mount_component<int>(0);
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
                            child, body, Prop<runtime::SemanticForeground>{title ? s.title_color : s.content_color},
                            Prop<runtime::SemanticTypography>{title ? s.title_font : s.content_font});
                    }});
                return child;
            };
            s.snapshot.title = region(id, Content{[slot = props.rich_title_, text = s.title] {
                                          if (slot) {
                                              SlotContentAccess::function (*slot)();
                                          } else {
                                              Text(TextProps{}.content(text));
                                          }
                                      }},
                                      true);
            Skeleton(SkeletonProps{}.loading(props.loading_).paragraph(false), SkeletonContent{[&] {
                         auto& c = runtime::require_component_build_context();
                         const auto content = c.mount_component<int>(0);
                         s.snapshot.content = content;
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
                         c.mount_slot(content, Content{[&] {
                                          s.snapshot.prefix =
                                              region(content, Content{[slot = props.rich_prefix_, text = s.prefix] {
                                                         if (slot) {
                                                             SlotContentAccess::function (*slot)();
                                                         } else {
                                                             Text(TextProps{}.content(text));
                                                         }
                                                     }},
                                                     false);
                                          s.snapshot.value =
                                              region(content, Content{[slot = props.rich_value_, text = s.value] {
                                                         if (slot) {
                                                             SlotContentAccess::function (*slot)();
                                                         } else {
                                                             Text(TextProps{}.content(text));
                                                         }
                                                     }},
                                                     false);
                                          s.snapshot.suffix =
                                              region(content, Content{[slot = props.rich_suffix_, text = s.suffix] {
                                                         if (slot) {
                                                             SlotContentAccess::function (*slot)();
                                                         } else {
                                                             Text(TextProps{}.content(text));
                                                         }
                                                     }},
                                                     false);
                                      }});
                     }});
            s.snapshot.skeleton = services.skeleton().mounted().back();
            services.nodes().require(services.components().root(s.snapshot.skeleton)).clip_decorations = false;
        }});
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) { return measure(id, e, c); },
            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) { place(id, e, r); }});
    mounted_.push_back(id);
    auto& scope = build.scope(id);
    connect(id, props.value_, &StatisticModel::value);
    connect(id, props.precision_, &StatisticModel::precision);
    connect(id, props.decimal_separator_, &StatisticModel::decimal_separator);
    connect(id, props.group_separator_, &StatisticModel::group_separator);
    connect(id, props.formatter_, &StatisticModel::formatter);
    const auto text = [this, id](const Prop<String>& prop, Signal<String> StatisticState::* member) {
        connect_prop(services_->components().scope(id), prop, [this, id, member](String value) {
            if (auto* s = services_->components().state<StatisticState>(id)) {
                (s->*member).set(std::move(value));
                refresh(id, true);
            }
        });
    };
    text(props.title_, &StatisticState::title);
    text(props.prefix_, &StatisticState::prefix);
    text(props.suffix_, &StatisticState::suffix);
    connect_prop(scope, props.loading_, [this, id](bool value) {
        if (auto* s = services_->components().state<StatisticState>(id)) {
            s->snapshot.loading = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.visible_, [this, id](bool value) {
        if (auto* s = services_->components().state<StatisticState>(id)) {
            s->snapshot.visible = value;
            refresh(id, true);
        }
    });
    const auto theme = build.theme_scope();
    s.subscriptions[0] = theme->capture([this, id](auto) { refresh(id, false); },
                                        [theme] { static_cast<void>(theme->statistic_colors()); });
    s.subscriptions[1] = theme->capture([this, id](auto) { refresh(id, true); },
                                        [theme] { static_cast<void>(theme->statistic_metrics()); });
    // Boundary reconciliation can retire this generation. Install every connection first,
    // and leave no borrowed state/scope access after the initial refresh.
    s.ready = true;
    refresh(id, true);
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
