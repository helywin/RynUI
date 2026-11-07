#include "component/config_defaults.hpp"
#include "component/timeline_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "component/data_display_retained.hpp"
#include "component/navigation_transition.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/icon.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local TimelineComponentHost* active_timeline{};
constexpr float infinity = std::numeric_limits<float>::infinity();

struct RegionState final {
    bool align_end{};
};

struct TailRange final {
    runtime::SceneFragmentId fragment;
    component::RetainedSurfaceId surface;
};

struct ItemState final {
    TimelineItemSnapshot snapshot;
    std::optional<TimelineItem> definition;
    std::shared_ptr<Scope> bindings;
    std::optional<Color> color;
    Color color_from;
    Color color_target;
    Color color_painted;
    Color fill_from;
    Color fill_target;
    Color fill_painted;
    bool palette_ready{};
    Signal<String> content_text{String{}};
    bool rich_title{};
    bool custom_icon{};
    String title;
    Signal<String> title_text{String{}};
    Signal<runtime::SemanticForeground> icon_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> icon_font{runtime::SemanticTypography{}};
    Signal<bool> spin{false};
    component::RetainedSurfaceId surface;
    std::shared_ptr<std::vector<TailRange>> tails{std::make_shared<std::vector<TailRange>>()};
    runtime::Rect frame;
    runtime::Rect title_frame;
    runtime::Rect content_frame;
    runtime::Rect icon_frame;
};

struct TimelineState final {
    runtime::NodeId node;
    input::InteractionId anchor;
    TimelineMode mode{TimelineMode::Start};
    bool reverse{};
    TimelineOrientation orientation{TimelineOrientation::Vertical};
    TimelineVariant variant{TimelineVariant::Outlined};
    TimelineTitleSpan title_span;
    bool rtl{};
    bool updating{};
    std::unique_ptr<NavigationTransition> motion;
    std::optional<std::vector<TimelineItem>> pending;
    bool visible{true};
    std::vector<TimelineModelItem> model;
    std::vector<TimelineModelEntry> order;
    std::vector<ItemState> items;
    Signal<runtime::SemanticForeground> text_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

using TimelineOwner = std::shared_ptr<TimelineState>;

TimelineOwner timeline_state(WindowComponentServices& services, runtime::ComponentId id) {
    const auto* slot = services.components().state<TimelineOwner>(id);
    return slot ? *slot : nullptr;
}

void validate_span(TimelineTitleSpan value) {
    if (!std::isfinite(value.value) || value.value < 0 || (!value.logical && value.value > 1)) {
        throw std::invalid_argument("Invalid Timeline titleSpan");
    }
}

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

Color tone_color(const TimelineColorToken& c, TimelineTone tone) {
    switch (tone) {
    case TimelineTone::Blue:
        return c.blue;
    case TimelineTone::Red:
        return c.red;
    case TimelineTone::Green:
        return c.green;
    case TimelineTone::Gray:
        return c.gray;
    default:
        return c.node;
    }
}

runtime::Rect offset(runtime::Rect r, runtime::Point p) {
    return {r.x + p.x, r.y + p.y, r.width, r.height};
}

runtime::Rect intersect(runtime::Rect a, runtime::Rect b) {
    const float x = std::clamp(a.x, b.x, b.x + b.width);
    const float y = std::clamp(a.y, b.y, b.y + b.height);
    return {x, y, std::max(0.0F, std::min(a.x + a.width, b.x + b.width) - x),
            std::max(0.0F, std::min(a.y + a.height, b.y + b.height) - y)};
}

void clear_tail_ranges(WindowComponentServices& services, std::vector<TailRange>& ranges) {
    for (const auto& range : ranges) {
        services.surfaces().destroy_content_range(range.surface);
        services.components().remove_scene_fragment(range.fragment);
    }
    ranges.clear();
}
} // namespace

TimelineComponentHost::TimelineComponentHost(WindowComponentServices& s) : services_(&s) {
    s.attach(*this);
}

TimelineComponentHost::~TimelineComponentHost() {
    services_->detach(*this);
}

void* TimelineComponentHost::begin_mount() noexcept {
    auto* old = active_timeline;
    active_timeline = this;
    return old;
}

void TimelineComponentHost::end_mount(void* old) noexcept {
    active_timeline = static_cast<TimelineComponentHost*>(old);
}

void TimelineComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void TimelineComponentHost::on_dispose() noexcept {
    mounted_.clear();
}

TimelineSnapshot TimelineComponentHost::snapshot(runtime::ComponentId id) const {
    const auto s = timeline_state(*services_, id);
    if (!s) {
        throw std::out_of_range("Timeline component is stale");
    }
    TimelineSnapshot result{s->order, {}};
    result.motion = s->motion ? s->motion->value() : 1;
    for (const auto& item : s->items) {
        result.items.push_back(item.snapshot);
    }
    return result;
}

void TimelineComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    run_untracked([&] {
        auto s = timeline_state(*services_, id);
        if (!s || !services_->components().scope(id).active()) {
            return;
        }
        if (s->updating) {
            return;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().timeline();
        s->text_color.set(foreground(t.colors.text));
        s->font.set({t.metrics.font_family, 400, false, t.metrics.font_size, t.metrics.line_height});
        bool palette_changed = false;
        for (auto& item : s->items) {
            const auto index = static_cast<std::size_t>(&item - s->items.data());
            const auto target = item.color.value_or(tone_color(t.colors, s->model[index].tone));
            const auto fill = s->variant == TimelineVariant::Filled ? target : t.colors.dot_background;
            if (!item.palette_ready) {
                item.palette_ready = true;
                item.color_from = item.color_painted = item.color_target = target;
                item.fill_from = item.fill_painted = item.fill_target = fill;
            } else if (item.color_target != target || item.fill_target != fill) {
                palette_changed = true;
                item.color_target = target;
                item.fill_target = fill;
            }
        }
        if (palette_changed) {
            for (auto& item : s->items) {
                item.color_from = item.color_painted;
                item.fill_from = item.fill_painted;
            }
            s->motion->restart(s->visible);
        }
        if (!s->visible || !services_->components().branch_active(id) || !services_->focus().state().window_active) {
            s->motion->settle();
        }
        std::vector<runtime::ComponentId> boundaries;
        const bool root_visible = s->visible && !s->order.empty();
        geometry |= services_->components().set_branch_active(id, root_visible);
        services_->nodes().require(s->node).out_of_flow = !root_visible;
        boundaries.push_back(id);
        for (std::size_t i = 0; i < s->items.size(); ++i) {
            auto& item = s->items[i];
            item.snapshot.visible = s->model[i].visible;
            if (!root_visible || !item.snapshot.visible) {
                clear_tail_ranges(*services_, *item.tails);
            }
            item.icon_color.set(foreground(item.color.value_or(tone_color(t.colors, s->model[i].tone))));
            item.icon_font.set({t.metrics.font_family, 400, false, t.metrics.icon_size, t.metrics.icon_size});
            if (!root_visible || !item.snapshot.visible || !item.snapshot.loading || item.custom_icon) {
                item.spin.set(false);
            }
            geometry |= services_->components().set_branch_active(item.snapshot.viewport, item.snapshot.visible);
            services_->nodes().require(services_->components().root(item.snapshot.viewport)).out_of_flow =
                !item.snapshot.visible;
            geometry |= services_->components().set_branch_active(item.snapshot.icon,
                                                                  item.custom_icon || item.snapshot.loading);
            boundaries.push_back(item.snapshot.viewport);
        }
        if (geometry) {
            std::vector<std::vector<input::InteractionId>> groups(s->items.size());
            for (auto hit : services_->interactions().declaration_order()) {
                for (auto ancestor = std::optional{services_->interactions().require(hit).component};
                     ancestor && *ancestor != id; ancestor = services_->components().parent(*ancestor)) {
                    const auto found = std::ranges::find_if(
                        s->items, [ancestor](const auto& item) { return item.snapshot.viewport == *ancestor; });
                    if (found != s->items.end()) {
                        groups[static_cast<std::size_t>(found - s->items.begin())].push_back(hit);
                        break;
                    }
                }
            }
            std::vector<input::InteractionId> order;
            std::vector<std::size_t> indices;
            for (const auto entry : s->order) {
                indices.push_back(entry.item);
            }
            for (std::size_t i = 0; i < s->items.size(); ++i) {
                if (!s->model[i].visible) {
                    indices.push_back(i);
                }
            }
            for (auto i : indices) {
                order.insert(order.end(), groups[i].begin(), groups[i].end());
            }
            services_->interactions().reorder_after(s->anchor, order);
            services_->mark_scene_structure_dirty();
        }
        services_->dirty().invalidate(s->node, geometry
                                                   ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                         runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                   : runtime::DirtyFlags::Material);
        // Boundary reconciliation can synchronously invoke user blur/authority/close callbacks.
        // No retained state references cross a call; ComponentId includes the full generation.
        for (const auto branch : boundaries) {
            const auto current = timeline_state(*services_, id);
            if (!current) {
                return;
            }
            const auto item = std::ranges::find_if(
                current->items, [branch](const auto& value) { return value.snapshot.viewport == branch; });
            const bool blocked = branch == id ? !current->visible || current->order.empty()
                                              : item == current->items.end() || !item->snapshot.visible;
            if (services_->components().contains(branch)) {
                services_->set_input_blocked(branch, blocked);
            }
        }
    });
}

void TimelineComponentHost::mount_item(runtime::ComponentId id, std::size_t i, const std::vector<TimelineItem>& items,
                                       bool initial) {
    auto owner = timeline_state(*services_, id);
    if (!owner) {
        return;
    }
    auto& s = *owner;
    auto& services = *services_;
    const auto content = Content{[&] {
        auto& context = runtime::require_component_build_context();
        const auto container = [&](runtime::ComponentId parent) {
            runtime::ComponentId child;
            context.mount_slot(parent, Content{[&] {
                                   auto& children = runtime::require_component_build_context();
                                   child = children.mount_component<RegionState>();
                                   const auto node = children.root(child);
                                   services.nodes().require(node).clip_content = true;
                                   services.layout().set_layout(node, layout::LeafLayout{});
                                   children.on_resource_cleanup(child, [&services, node, child] {
                                       services.interactions().set_input_blocked(child, false);
                                       services.layout().remove_layout(node);
                                   });
                               }});
            return child;
        };
        const auto region = [&](runtime::ComponentId parent, Content body,
                                const Signal<runtime::SemanticForeground>& color,
                                const Signal<runtime::SemanticTypography>& font) {
            const auto child = container(parent);
            const auto node = context.root(child);
            services.nodes().require(node).clip_decorations = false;
            services.layout().set_layout(
                node, layout::ComponentLayout{
                          [&services](layout::LayoutEngine& engine, runtime::NodeId node, layout::Constraints limits) {
                              runtime::Size natural{};
                              for (auto descendant : services.nodes().require(node).children) {
                                  const auto size = engine.measure_child(descendant, {0, infinity, 0, infinity});
                                  natural.width = std::max(natural.width, size.width);
                                  natural.height = std::max(natural.height, size.height);
                              }
                              return limits.constrain(natural);
                          },
                          [&services, child](layout::LayoutEngine& engine, runtime::NodeId node, runtime::Rect bounds) {
                              const bool end = services.components().state<RegionState>(child)->align_end;
                              for (auto descendant : services.nodes().require(node).children) {
                                  const auto size = services.nodes().require(descendant).layout_size;
                                  engine.place_child(descendant,
                                                     {bounds.x + (end ? std::max(0.0F, bounds.width - size.width) : 0),
                                                      bounds.y, size.width, size.height});
                              }
                          }});
            context.mount_slot_with_semantic_text_style(child, body, Prop<runtime::SemanticForeground>{color},
                                                        Prop<runtime::SemanticTypography>{font});
            return child;
        };
        auto& item = s.items[i];
        item.definition = items[i];
        item.content_text.set(read_prop(items[i].content_));
        if (items[i].color_) {
            item.color = read_prop(*items[i].color_);
        }
        item.rich_title = items[i].rich_title_.has_value();
        item.custom_icon = items[i].icon_.has_value();
        item.title = read_prop(items[i].title_);
        item.title_text.set(item.title);
        item.snapshot.loading = read_prop(items[i].loading_);
        item.snapshot.viewport = container(id);
        // Item viewports crop content but let focus decoration reach Timeline's hardclip.
        services.nodes().require(context.root(item.snapshot.viewport)).clip_decorations = false;
        item.snapshot.title =
            region(item.snapshot.viewport, Content{[slot = items[i].rich_title_, text = item.title_text] {
                       if (slot) {
                           SlotContentAccess::function (*slot)();
                       } else {
                           Text(TextProps{}.content(text));
                       }
                   }},
                   s.text_color, s.font);
        item.snapshot.content =
            region(item.snapshot.viewport, Content{[slot = items[i].rich_content_, text = item.content_text] {
                       if (slot) {
                           SlotContentAccess::function (*slot)();
                       } else {
                           Text(TextProps{}.content(text));
                       }
                   }},
                   s.text_color, s.font);
        item.snapshot.icon = region(item.snapshot.viewport, Content{[slot = items[i].icon_, spin = item.spin] {
                                        if (slot) {
                                            SlotContentAccess::function (*slot)();
                                        } else {
                                            Icon(IconProps{}.name(IconName::LoadingOutlined).spin(spin));
                                        }
                                    }},
                                    item.icon_color, item.icon_font);
        const auto fragment =
            context.register_scene_fragment(item.snapshot.viewport, runtime::SceneFragmentPlacement::before_children);
        item.surface = services.surfaces().create_content_range(fragment, {});
        context.on_resource_cleanup(item.snapshot.viewport, [&services, surface = item.surface, tails = item.tails] {
            clear_tail_ranges(services, *tails);
            services.surfaces().destroy_content_range(surface);
        });
    }};
    display_append(services, id, id, content, initial);
}

void TimelineComponentHost::mount(const TimelineProps& props, const std::vector<TimelineItem>& items) {
    const auto mode = read_prop(props.mode_);
    const bool reverse = read_prop(props.reverse_);
    const bool visible = read_prop(props.visible_);
    std::vector<TimelineModelItem> model;
    std::vector<String> titles;
    std::vector<bool> loading;
    for (const auto& item : items) {
        model.push_back({item.key_, read_prop(item.placement_), read_prop(item.tone_), read_prop(item.visible_)});
        titles.push_back(read_prop(item.title_));
        loading.push_back(read_prop(item.loading_));
    }
    auto order = timeline_order(mode, reverse, model);
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<TimelineOwner>(std::make_shared<TimelineState>());
    auto owner = build.state<TimelineOwner>(id);
    auto& s = *owner;
    s.node = build.root(id);
    s.motion = std::make_unique<NavigationTransition>(services, id);
    s.mode = mode;
    s.orientation = read_prop(props.orientation_);
    s.variant = read_prop(props.variant_);
    s.title_span = read_prop(props.title_span_);
    validate_span(s.title_span);
    s.rtl = read_prop(props.rtl_);
    s.reverse = reverse;
    s.visible = visible;
    s.model = std::move(model);
    s.order = std::move(order);
    s.items.reserve(items.size());
    services.nodes().require(s.node).clip_content = true;
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.on_resource_cleanup(id, [&services, id, node = s.node] {
        services.interactions().set_input_blocked(id, false);
        services.layout().remove_layout(node);
    });
    build.on_resource_cleanup(id, [weak = std::weak_ptr<TimelineState>{owner}] {
        if (auto s = weak.lock()) {
            s->motion.reset();
            for (auto& item : s->items) {
                item.bindings.reset();
                item.spin.set(false);
            }
        }
    });
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            const auto container = [&](runtime::ComponentId parent) {
                runtime::ComponentId child;
                context.mount_slot(parent, Content{[&] {
                                       auto& children = runtime::require_component_build_context();
                                       child = children.mount_component<RegionState>();
                                       const auto node = children.root(child);
                                       services.nodes().require(node).clip_content = true;
                                       services.layout().set_layout(node, layout::LeafLayout{});
                                       children.on_resource_cleanup(child, [&services, node, child] {
                                           services.interactions().set_input_blocked(child, false);
                                           services.layout().remove_layout(node);
                                       });
                                   }});
                return child;
            };
            const auto anchor = container(id);
            const auto anchor_node = context.root(anchor);
            services.nodes().require(anchor_node).out_of_flow = true;
            s.anchor = services.interactions().create({anchor, anchor_node, {}, false, false});
            context.on_resource_cleanup(anchor, [&services, hit = s.anchor] { services.interactions().remove(hit); });
            const auto region = [&](runtime::ComponentId parent, Content body,
                                    const Signal<runtime::SemanticForeground>& color,
                                    const Signal<runtime::SemanticTypography>& font) {
                const auto child = container(parent);
                const auto node = context.root(child);
                services.nodes().require(node).clip_decorations = false;
                services.layout().set_layout(
                    node,
                    layout::ComponentLayout{
                        [&services](layout::LayoutEngine& engine, runtime::NodeId node, layout::Constraints limits) {
                            runtime::Size natural{};
                            for (auto descendant : services.nodes().require(node).children) {
                                const auto size = engine.measure_child(descendant, {0, infinity, 0, infinity});
                                natural.width = std::max(natural.width, size.width);
                                natural.height = std::max(natural.height, size.height);
                            }
                            return limits.constrain(natural);
                        },
                        [&services, child](layout::LayoutEngine& engine, runtime::NodeId node, runtime::Rect bounds) {
                            const bool end = services.components().state<RegionState>(child)->align_end;
                            for (auto descendant : services.nodes().require(node).children) {
                                const auto size = services.nodes().require(descendant).layout_size;
                                engine.place_child(descendant,
                                                   {bounds.x + (end ? std::max(0.0F, bounds.width - size.width) : 0),
                                                    bounds.y, size.width, size.height});
                            }
                        }});
                context.mount_slot_with_semantic_text_style(child, body, Prop<runtime::SemanticForeground>{color},
                                                            Prop<runtime::SemanticTypography>{font});
                return child;
            };
            s.items.resize(items.size());
            for (std::size_t i = 0; i < items.size(); ++i) {
                mount_item(id, i, items, true);
            }
        }});
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) { return measure(id, e, c); },
            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) { place(id, e, r); }});
    mounted_.push_back(id);
    auto& scope = build.scope(id);
    connect_prop(scope, props.mode_, [this, id](TimelineMode value) {
        if (auto s = timeline_state(*services_, id)) {
            auto order = timeline_order(value, s->reverse, s->model);
            s->mode = value;
            s->order = std::move(order);
            refresh(id, true);
        }
    });
    connect_prop(scope, props.reverse_, [this, id](bool value) {
        if (auto s = timeline_state(*services_, id)) {
            s->order = timeline_order(s->mode, value, s->model);
            s->reverse = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.visible_, [this, id](bool value) {
        if (auto s = timeline_state(*services_, id)) {
            s->visible = value;
            refresh(id, true);
        }
    });
    for (std::size_t i = 0; i < items.size(); ++i) {
        bind_item(id, i);
    }
    connect_prop(scope, props.orientation_, [this, id](TimelineOrientation value) {
        if (value < TimelineOrientation::Vertical || value > TimelineOrientation::Horizontal) {
            throw std::invalid_argument("Invalid Timeline orientation");
        }
        if (auto s = timeline_state(*services_, id)) {
            s->orientation = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.variant_, [this, id](TimelineVariant value) {
        if (value < TimelineVariant::Outlined || value > TimelineVariant::Filled) {
            throw std::invalid_argument("Invalid Timeline variant");
        }
        if (auto s = timeline_state(*services_, id)) {
            s->variant = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.title_span_, [this, id](TimelineTitleSpan value) {
        validate_span(value);
        if (auto s = timeline_state(*services_, id)) {
            s->title_span = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.rtl_, [this, id](bool value) {
        if (auto s = timeline_state(*services_, id)) {
            s->rtl = value;
            refresh(id, true);
        }
    });
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::timeline_colors,
                               &theme_runtime::ThemeScope::timeline_metrics};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { refresh(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    refresh(id, true);
}

void TimelineComponentHost::bind_item(runtime::ComponentId id, std::size_t index) {
    auto s = timeline_state(*services_, id);
    auto& item = s->items[index];
    item.bindings = std::make_shared<Scope>();
    const auto bindings = item.bindings;
    const auto definition = *item.definition;
    const auto apply = [this, id] {
        auto s = timeline_state(*services_, id);
        if (!s || s->updating) {
            return;
        }
        auto model = s->model;
        std::vector<String> titles;
        std::vector<String> content;
        std::vector<bool> loading;
        std::vector<std::optional<Color>> colors;
        for (std::size_t i = 0; i < s->items.size(); ++i) {
            const auto& d = *s->items[i].definition;
            model[i] = {d.key_, read_prop(d.placement_), read_prop(d.tone_), read_prop(d.visible_)};
            titles.push_back(read_prop(d.title_));
            content.push_back(read_prop(d.content_));
            loading.push_back(read_prop(d.loading_));
            colors.push_back(d.color_ ? std::optional{read_prop(*d.color_)} : std::nullopt);
        }
        auto order = timeline_order(s->mode, s->reverse, model);
        s->model = std::move(model);
        s->order = std::move(order);
        for (std::size_t i = 0; i < s->items.size(); ++i) {
            s->items[i].title = titles[i];
            s->items[i].title_text.set(titles[i]);
            s->items[i].content_text.set(content[i]);
            s->items[i].snapshot.loading = loading[i];
            s->items[i].color = colors[i];
        }
        refresh(id, true);
    };
    connect_prop(*bindings, definition.title_, [apply](String) { apply(); });
    connect_prop(*bindings, definition.content_, [apply](String) { apply(); });
    connect_prop(*bindings, definition.placement_, [apply](std::optional<TimelinePlacement>) { apply(); });
    connect_prop(*bindings, definition.tone_, [apply](TimelineTone) { apply(); });
    connect_prop(*bindings, definition.visible_, [apply](bool) { apply(); });
    connect_prop(*bindings, definition.loading_, [apply](bool) { apply(); });
    if (definition.color_) {
        connect_prop(*bindings, *definition.color_, [apply](Color) { apply(); });
    }
}

void TimelineComponentHost::update(runtime::ComponentId id, std::vector<TimelineItem> definitions) {
    run_untracked([&] {
        auto s = timeline_state(*services_, id);
        if (!s) {
            return;
        }
        std::vector<TimelineModelItem> model;
        for (const auto& d : definitions) {
            model.push_back({d.key_, read_prop(d.placement_), read_prop(d.tone_), read_prop(d.visible_)});
            static_cast<void>(read_prop(d.title_));
            static_cast<void>(read_prop(d.content_));
            static_cast<void>(read_prop(d.loading_));
            if (d.color_) {
                static_cast<void>(read_prop(*d.color_));
            }
        }
        auto order = timeline_order(s->mode, s->reverse, model);
        if (!services_->components().contains(id)) {
            return;
        }
        if (s->updating) {
            s->pending = std::move(definitions);
            return;
        }
        s->updating = true;
        try {
            auto old = std::move(s->items);
            s->items.clear();
            s->items.resize(definitions.size());
            s->model = std::move(model);
            s->order = std::move(order);
            for (std::size_t i = 0; i < definitions.size(); ++i) {
                const auto& d = definitions[i];
                const auto found = std::ranges::find_if(
                    old, [&](const auto& item) { return item.definition && item.definition->key_ == d.key_; });
                if (found == old.end()) {
                    mount_item(id, i, definitions, false);
                } else {
                    s->items[i] = std::move(*found);
                    auto& item = s->items[i];
                    const auto previous = *item.definition;
                    item.bindings.reset();
                    item.definition = d;
                    const auto replace = [&](runtime::ComponentId region, const Content& content) {
                        display_clear(*services_, id, region);
                        if (services_->components().contains(id)) {
                            display_append_styled(*services_, id, region, content,
                                                  region == item.snapshot.icon ? item.icon_color : s->text_color,
                                                  region == item.snapshot.icon ? item.icon_font : s->font, false);
                        }
                    };
                    if (previous.title_revision_ != d.title_revision_ ||
                        previous.rich_title_.has_value() != d.rich_title_.has_value()) {
                        replace(item.snapshot.title, Content{[slot = d.rich_title_, text = item.title_text] {
                                    if (slot) {
                                        SlotContentAccess::function (*slot)();
                                    } else {
                                        Text(TextProps{}.content(text));
                                    }
                                }});
                    }
                    if (!services_->components().contains(id)) {
                        return;
                    }
                    if (previous.content_revision_ != d.content_revision_ ||
                        previous.rich_content_.has_value() != d.rich_content_.has_value()) {
                        replace(item.snapshot.content, Content{[slot = d.rich_content_, text = item.content_text] {
                                    if (slot) {
                                        SlotContentAccess::function (*slot)();
                                    } else {
                                        Text(TextProps{}.content(text));
                                    }
                                }});
                    }
                    if (!services_->components().contains(id)) {
                        return;
                    }
                    if (previous.icon_revision_ != d.icon_revision_ ||
                        previous.icon_.has_value() != d.icon_.has_value()) {
                        replace(item.snapshot.icon, Content{[slot = d.icon_, spin = item.spin] {
                                    if (slot) {
                                        SlotContentAccess::function (*slot)();
                                    } else {
                                        Icon(IconProps{}.name(IconName::LoadingOutlined).spin(spin));
                                    }
                                }});
                    }
                    item.rich_title = d.rich_title_.has_value();
                    item.custom_icon = d.icon_.has_value();
                }
                if (!services_->components().contains(id)) {
                    return;
                }
            }
            for (auto& item : old) {
                if (std::ranges::any_of(
                        s->items, [&](const auto& kept) { return kept.snapshot.viewport == item.snapshot.viewport; })) {
                    continue;
                }
                item.bindings.reset();
                services_->destroy(item.snapshot.viewport);
                if (!services_->components().contains(id)) {
                    return;
                }
            }
            s->updating = false;
            for (std::size_t i = 0; i < s->items.size(); ++i) {
                bind_item(id, i);
            }
            refresh(id, true);
            if (s->pending && services_->components().contains(id)) {
                auto pending = std::move(*s->pending);
                s->pending.reset();
                update(id, std::move(pending));
            }
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
    });
}

void TimelineComponentHost::mount(const TimelineProps& props, const Prop<std::vector<TimelineItem>>& items) {
    mount(props, read_prop(items));
    const auto id = mounted_.back();
    connect_prop(
        services_->components().scope(id), items,
        [this, id](std::vector<TimelineItem> value) { update(id, std::move(value)); },
        [](const auto&, const auto&) { return false; });
}

runtime::Size TimelineComponentHost::measure_horizontal(runtime::ComponentId id, layout::LayoutEngine& engine,
                                                        layout::Constraints limits, float width) {
    auto& s = *timeline_state(*services_, id);
    const auto& m = services_->components().theme_scope(id)->snapshot().timeline().metrics;
    const float cell = s.order.empty() ? 0 : width / static_cast<float>(s.order.size());
    const float gap = std::min(m.content_gap, cell / 4);
    float above = 0;
    float below = 0;
    float rail = m.dot_size;
    std::vector<runtime::Size> titles;
    std::vector<runtime::Size> contents;
    std::vector<runtime::Size> icons;
    const auto measure = [&](runtime::ComponentId child, float available) {
        return engine.measure_child(services_->components().root(child), {0, std::max(0.0F, available), 0, infinity});
    };
    for (auto entry : s.order) {
        auto& item = s.items[entry.item];
        item.snapshot.placement = entry.placement;
        const auto title = measure(item.snapshot.title, std::max(0.0F, cell - gap * 2));
        const auto content = measure(item.snapshot.content, std::max(0.0F, cell - gap * 2));
        const auto icon = measure(item.snapshot.icon, cell);
        const bool end = entry.placement == TimelinePlacement::End;
        above = std::max(above, end ? content.height : title.height);
        below = std::max(below, end ? title.height : content.height);
        rail = std::max(rail, item.custom_icon ? icon.height : item.snapshot.loading ? m.icon_size : m.dot_size);
        titles.push_back(title);
        contents.push_back(content);
        icons.push_back(icon);
    }
    const float axis = above + gap + rail / 2;
    const float height = above + below + rail + gap * 2;
    for (std::size_t position = 0; position < s.order.size(); ++position) {
        auto& item = s.items[s.order[position].item];
        const bool end = item.snapshot.placement == TimelinePlacement::End;
        const float diameter = item.custom_icon        ? std::max(m.dot_size, icons[position].width)
                               : item.snapshot.loading ? m.icon_size
                                                       : m.dot_size;
        const float head = item.custom_icon ? std::max(m.dot_size, icons[position].height) : diameter;
        const float center = cell / 2;
        item.frame = {s.rtl ? width - (static_cast<float>(position) + 1) * cell : static_cast<float>(position) * cell,
                      0, cell, height};
        item.title_frame = {gap, end ? axis + rail / 2 + gap : above - titles[position].height,
                            std::max(0.0F, cell - gap * 2), titles[position].height};
        item.content_frame = {gap, end ? above - contents[position].height : axis + rail / 2 + gap,
                              std::max(0.0F, cell - gap * 2), contents[position].height};
        item.icon_frame = {center - std::min(cell, icons[position].width) / 2, axis - icons[position].height / 2,
                           std::min(cell, icons[position].width), icons[position].height};
        item.snapshot.dot = {center - diameter / 2, axis - head / 2, diameter, head};
        item.snapshot.tail = {};
        services_->components().state<RegionState>(item.snapshot.title)->align_end = s.rtl;
        services_->components().state<RegionState>(item.snapshot.content)->align_end = s.rtl;
        static_cast<void>(engine.measure_child(services_->components().root(item.snapshot.viewport),
                                               layout::Constraints::fixed(cell, height)));
    }
    for (std::size_t position = 0; position + 1 < s.order.size(); ++position) {
        auto& item = s.items[s.order[position].item];
        const auto& next = s.items[s.order[position + 1].item];
        const float current_center = item.frame.x + item.snapshot.dot.x + item.snapshot.dot.width / 2;
        const float next_center = next.frame.x + next.snapshot.dot.x + next.snapshot.dot.width / 2;
        const float start =
            s.rtl ? next_center + next.snapshot.dot.width / 2 : current_center + item.snapshot.dot.width / 2;
        const float finish =
            s.rtl ? current_center - item.snapshot.dot.width / 2 : next_center - next.snapshot.dot.width / 2;
        item.snapshot.tail = {start - item.frame.x, axis - m.tail_width / 2, std::max(0.0F, finish - start),
                              m.tail_width};
    }
    return limits.constrain({width, height});
}

runtime::Size TimelineComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& e,
                                             layout::Constraints limits) {
    auto& s = *timeline_state(*services_, id);
    const auto& m = services_->components().theme_scope(id)->snapshot().timeline().metrics;
    const auto node = [&](runtime::ComponentId child) {
        return services_->components().root(child);
    };
    const auto size = [&](runtime::ComponentId child, float width) {
        return e.measure_child(node(child), {0, std::max(0.0F, width), 0, infinity});
    };
    const bool dual = s.mode == TimelineMode::Alternate || std::ranges::any_of(s.order, [&](auto entry) {
                          const auto& item = s.items[entry.item];
                          return item.rich_title || !item.title.empty();
                      });
    float width = limits.max_width;
    if (!std::isfinite(width)) {
        width = limits.min_width;
        for (auto entry : s.order) {
            const auto& item = s.items[entry.item];
            const auto title = size(item.snapshot.title, infinity);
            const auto content = size(item.snapshot.content, infinity);
            const auto icon = size(item.snapshot.icon, infinity);
            const float natural = (dual ? 2 * std::max(title.width, content.width) : content.width) +
                                  std::max(m.dot_size, item.custom_icon ? icon.width : m.icon_size) + m.content_gap * 2;
            if (!std::isfinite(natural)) {
                throw std::invalid_argument("Timeline intrinsic extent overflow");
            }
            width = std::max(width, natural);
        }
    }
    if (s.orientation == TimelineOrientation::Horizontal) {
        return measure_horizontal(id, e, limits, width);
    }
    float y = 0;
    for (std::size_t position = 0; position < s.order.size(); ++position) {
        const auto entry = s.order[position];
        auto& item = s.items[entry.item];
        item.snapshot.placement = entry.placement;
        const bool end = entry.placement == TimelinePlacement::End;
        const auto icon = size(item.snapshot.icon, infinity);
        const float diameter = item.custom_icon        ? std::max(m.dot_size, icon.width)
                               : item.snapshot.loading ? m.icon_size
                                                       : m.dot_size;
        const float head_height = item.custom_icon ? std::max(m.dot_size, icon.height) : diameter;
        const float rail_width = std::min(width, diameter);
        const float gap = std::min(m.content_gap, std::max(0.0F, (width - rail_width) / (dual ? 2 : 1)));
        const float body_width = std::max(0.0F, (width - rail_width - gap * (dual ? 2 : 1)) / (dual ? 2 : 1));
        const float title_extent =
            std::clamp(s.title_span.logical ? s.title_span.value : width * s.title_span.value, 0.0F, width);
        const float axis = dual  ? (s.mode == TimelineMode::End ? width - title_extent : title_extent)
                           : end ? width - rail_width / 2
                                 : rail_width / 2;
        const float left_width = std::max(0.0F, axis - rail_width / 2 - gap);
        const float right_width = std::max(0.0F, width - axis - rail_width / 2 - gap);
        const float left = 0;
        const float right = dual ? axis + rail_width / 2 + gap : end ? 0 : rail_width + gap;
        const auto title = size(item.snapshot.title, dual ? (end ? right_width : left_width) : 0);
        const auto content = size(item.snapshot.content, dual ? (end ? left_width : right_width) : body_width);
        const float height = std::max({content.height, dual ? title.height : 0, m.line_height, head_height});
        const float center = std::max(m.line_height, head_height) / 2;
        item.frame = {0, y, width, height + (position + 1 < s.order.size() ? m.item_padding_bottom : 0)};
        item.title_frame = {end ? right : left, 0, dual ? (end ? right_width : left_width) : 0,
                            dual ? title.height : 0};
        item.content_frame = {dual ? (end ? left : right) : right, 0,
                              dual ? (end ? left_width : right_width) : body_width, content.height};
        item.icon_frame = {std::max(0.0F, axis - rail_width / 2), std::max(0.0F, center - icon.height / 2), rail_width,
                           icon.height};
        item.snapshot.dot = {axis - diameter / 2, center - head_height / 2, diameter, head_height};
        item.snapshot.tail = position + 1 < s.order.size()
                                 ? runtime::Rect{axis - m.tail_width / 2, center + head_height / 2, m.tail_width,
                                                 std::max(0.0F, item.frame.height - center)}
                                 : runtime::Rect{};
        services_->components().state<RegionState>(item.snapshot.title)->align_end = !end;
        services_->components().state<RegionState>(item.snapshot.content)->align_end = end;
        static_cast<void>(
            e.measure_child(node(item.snapshot.viewport), layout::Constraints::fixed(width, item.frame.height)));
        y += item.frame.height;
        if (!std::isfinite(y)) {
            throw std::invalid_argument("Timeline height overflow");
        }
    }
    for (std::size_t position = 0; position + 1 < s.order.size(); ++position) {
        auto& item = s.items[s.order[position].item];
        const auto& next = s.items[s.order[position + 1].item];
        item.snapshot.tail.height =
            std::max(0.0F, next.frame.y + next.snapshot.dot.y - item.frame.y - item.snapshot.tail.y);
    }
    if (s.rtl) {
        for (auto entry : s.order) {
            auto& item = s.items[entry.item];
            for (auto* r :
                 {&item.title_frame, &item.content_frame, &item.icon_frame, &item.snapshot.dot, &item.snapshot.tail}) {
                r->x = width - r->x - r->width;
            }
            services_->components().state<RegionState>(item.snapshot.title)->align_end =
                item.snapshot.placement == TimelinePlacement::End;
            services_->components().state<RegionState>(item.snapshot.content)->align_end =
                item.snapshot.placement != TimelinePlacement::End;
        }
    }
    return limits.constrain({width, y});
}

void TimelineComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& e, runtime::Rect bounds) {
    const auto& s = *timeline_state(*services_, id);
    for (auto entry : s.order) {
        const auto& item = s.items[entry.item];
        const runtime::Point origin{bounds.x + item.frame.x, bounds.y + item.frame.y};
        const auto frame = intersect(offset(item.frame, {bounds.x, bounds.y}), bounds);
        e.place_child(services_->components().root(item.snapshot.viewport), frame, true, true);
        e.place_child(services_->components().root(item.snapshot.title),
                      intersect(offset(item.title_frame, origin), frame), true, true);
        e.place_child(services_->components().root(item.snapshot.content),
                      intersect(offset(item.content_frame, origin), frame), true, true);
        e.place_child(services_->components().root(item.snapshot.icon),
                      intersect(offset(item.icon_frame, origin), frame), true, true);
    }
}

void TimelineComponentHost::synchronize_auxiliary_motion() {
    for (auto id : mounted_) {
        if (auto s = timeline_state(*services_, id);
            s && s->motion &&
            (!animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                               services_->motion_preference())
                  .enabled() ||
             !services_->components().branch_active(id) || !services_->interactions().component_active(id) ||
             !services_->focus().state().window_active)) {
            s->motion->settle();
        }
    }
}

void TimelineComponentHost::on_window_active(bool) {
    synchronize_auxiliary_motion();
}

void TimelineComponentHost::on_input_boundary_changed() {
    synchronize_auxiliary_motion();
}

void TimelineComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        const auto s = timeline_state(*services_, id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().timeline();
        const auto& root = services_->nodes().require(s->node);
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
        const float opacity = services_->nodes().content_opacity(s->node);
        if (clip.width <= 0 || clip.height <= 0 || opacity <= 0 || !services_->focus().state().window_active ||
            !services_->interactions().component_active(id) ||
            !animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                              services_->motion_preference())
                 .enabled()) {
            s->motion->settle();
        }
        const float progress = s->motion->value();
        const auto blend = [progress](Color from, Color to) {
            if (progress >= 1) {
                return to;
            }
            const auto mix = [progress](float a, float b) {
                return a + (b - a) * progress;
            };
            return Color(mix(from.red(), to.red()), mix(from.green(), to.green()), mix(from.blue(), to.blue()),
                         mix(from.alpha(), to.alpha()));
        };
        for (std::size_t position = 0; position < s->order.size(); ++position) {
            auto& item = s->items[s->order[position].item];
            const runtime::Point origin{root.bounds.x + root.translation.x + item.frame.x,
                                        root.bounds.y + root.translation.y + item.frame.y};
            item.color_painted = blend(item.color_from, item.color_target);
            item.fill_painted = blend(item.fill_from, item.fill_target);
            item.icon_color.set(foreground(item.color_painted));
            const auto color = item.color_painted;
            std::vector<graphics::RoundedEffectInstance> effects;
            const auto dot = offset(item.snapshot.dot, {root.bounds.x + item.frame.x, root.bounds.y + item.frame.y});
            const auto icon_bounds = offset(item.icon_frame, origin);
            const auto icon_visible = intersect(icon_bounds, clip);
            item.spin.set(!item.custom_icon && item.snapshot.loading && opacity > 0 && icon_visible.width > 0 &&
                          icon_visible.height > 0);
            if (!item.custom_icon && !item.snapshot.loading && dot.width > 0) {
                auto fill = graphics::make_corner_fill_effects({dot, dot.width / 2}, {true, true, true, true},
                                                               item.fill_painted, opacity, root.translation,
                                                               graphics::EffectClip{1, clip});
                effects.insert(effects.end(), fill.begin(), fill.end());
                auto ring = graphics::make_corner_outline_effects(
                    {dot, dot.width / 2}, {true, true, true, true}, std::min(t.metrics.dot_border_width, dot.width / 2),
                    0, color, opacity, root.translation, graphics::EffectClip{1, clip});
                effects.insert(effects.end(), ring.begin(), ring.end());
            }
            static_cast<void>(services_->surfaces().update_content_effects(item.surface, effects));
            std::vector<graphics::QuadInstance> lines;
            const auto tail = offset(item.snapshot.tail, origin);
            const bool dashed =
                s->reverse ? item.snapshot.loading
                           : position + 1 < s->order.size() && s->items[s->order[position + 1].item].snapshot.loading;
            auto& ranges = *item.tails;
            std::size_t used_ranges = 0;
            const auto publish = [&] {
                if (lines.empty()) {
                    return;
                }
                if (used_ranges == ranges.size()) {
                    TailRange range{services_->components().register_scene_fragment(
                                        item.snapshot.viewport, runtime::SceneFragmentPlacement::before_children),
                                    {}};
                    try {
                        range.surface = services_->surfaces().create_content_range(range.fragment, {});
                        ranges.push_back(range);
                    } catch (...) {
                        services_->surfaces().destroy_content_range(range.surface);
                        services_->components().remove_scene_fragment(range.fragment);
                        throw;
                    }
                }
                static_cast<void>(services_->surfaces().update_content_range(ranges[used_ranges++].surface, lines));
                lines.clear();
            };
            const auto line = [&](runtime::Rect rect) {
                rect = intersect(rect, clip);
                if (rect.width > 0 && rect.height > 0) {
                    lines.push_back(
                        {{rect.x, rect.y, rect.width, rect.height},
                         {t.colors.tail.red(), t.colors.tail.green(), t.colors.tail.blue(), t.colors.tail.alpha()},
                         opacity});
                    if (lines.size() == component::retained_content_visual_capacity) {
                        publish();
                    }
                }
            };
            if (s->orientation == TimelineOrientation::Horizontal && dashed) {
                const float dash = std::max(2.0F, t.metrics.tail_width * 2);
                const float begin = std::max(tail.x, clip.x);
                const float finish = std::min(tail.x + tail.width, clip.x + clip.width);
                const double first = begin - std::fmod(static_cast<double>(begin - tail.x), dash * 2);
                for (double x = first; x < finish; x += dash * 2) {
                    const float left = std::max(begin, static_cast<float>(x));
                    const float right = std::min(finish, static_cast<float>(x) + dash);
                    if (right > left) {
                        line({left, tail.y, right - left, tail.height});
                    }
                }
            } else if (dashed && tail.width > 0 && tail.height > 0 && clip.width > 0 &&
                       tail.x < static_cast<double>(clip.x) + clip.width &&
                       tail.x + static_cast<double>(tail.width) > clip.x) {
                const double dash = std::max(2.0, static_cast<double>(t.metrics.tail_width) * 2);
                const double period = dash * 2;
                const double start = std::max<double>(tail.y, clip.y);
                const double end =
                    std::min(static_cast<double>(tail.y) + tail.height, static_cast<double>(clip.y) + clip.height);
                if (end > start) {
                    // Preserve phase when the visible clip begins halfway through the trajectory.
                    double first = start - std::fmod(start - tail.y, period);
                    if (first + dash <= start) {
                        first += period;
                    }
                    const double count = std::max(0.0, std::ceil((end - first) / period));
                    if (!std::isfinite(count) || count > std::numeric_limits<std::uint32_t>::max()) {
                        throw std::length_error("Timeline visible dash count exceeds logical scene index capacity");
                    }
                    const auto segments = static_cast<std::uint64_t>(count);
                    lines.reserve(static_cast<std::size_t>(
                        std::min<std::uint64_t>(segments, component::retained_content_visual_capacity)));
                    for (std::uint64_t index = 0; index < segments; ++index) {
                        const double y = first + static_cast<double>(index) * period;
                        const double top = std::max(y, start);
                        const double bottom = std::min(y + dash, end);
                        line({tail.x, static_cast<float>(top), tail.width,
                              static_cast<float>(std::max(0.0, bottom - top))});
                    }
                }
            } else {
                line(tail);
            }
            publish();
            while (ranges.size() > used_ranges) {
                services_->surfaces().destroy_content_range(ranges.back().surface);
                services_->components().remove_scene_fragment(ranges.back().fragment);
                ranges.pop_back();
            }
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Timeline(TimelineProps props, Prop<std::vector<TimelineItem>> items) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_timeline) {
        throw std::logic_error("Timeline requires window component services");
    }
    detail::active_timeline->mount(props, items);
}

void Timeline(TimelineProps props, std::vector<TimelineItem> items) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_timeline) {
        throw std::logic_error("Timeline requires window component services");
    }
    detail::active_timeline->mount(props, items);
}
} // namespace ryn
