#include "component/timeline_component.hpp"
#include "runtime/layout_style_adapter.hpp"
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

struct ItemState final {
    TimelineItemSnapshot snapshot;
    bool rich_title{};
    bool custom_icon{};
    String title;
    Signal<String> title_text{String{}};
    Signal<runtime::SemanticForeground> icon_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> icon_font{runtime::SemanticTypography{}};
    component::RetainedSurfaceId surface;
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
    bool visible{true};
    std::vector<TimelineModelItem> model;
    std::vector<TimelineModelEntry> order;
    std::vector<ItemState> items;
    Signal<runtime::SemanticForeground> text_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

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
    const auto* s = services_->components().state<TimelineState>(id);
    if (!s) {
        throw std::out_of_range("Timeline component is stale");
    }
    TimelineSnapshot result{s->order, {}};
    for (const auto& item : s->items) {
        result.items.push_back(item.snapshot);
    }
    return result;
}

void TimelineComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<TimelineState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().timeline();
    s->text_color.set(foreground(t.colors.text));
    s->font.set({t.metrics.font_family, 400, false, t.metrics.font_size, t.metrics.line_height});
    std::vector<runtime::ComponentId> boundaries;
    const bool root_visible = s->visible && !s->order.empty();
    geometry |= services_->components().set_branch_active(id, root_visible);
    services_->nodes().require(s->node).out_of_flow = !root_visible;
    boundaries.push_back(id);
    for (std::size_t i = 0; i < s->items.size(); ++i) {
        auto& item = s->items[i];
        item.snapshot.visible = s->model[i].visible;
        item.icon_color.set(foreground(tone_color(t.colors, s->model[i].tone)));
        item.icon_font.set({t.metrics.font_family, 400, false, t.metrics.icon_size, t.metrics.icon_size});
        geometry |= services_->components().set_branch_active(item.snapshot.viewport, item.snapshot.visible);
        services_->nodes().require(services_->components().root(item.snapshot.viewport)).out_of_flow =
            !item.snapshot.visible;
        geometry |=
            services_->components().set_branch_active(item.snapshot.icon, item.custom_icon || item.snapshot.loading);
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
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
    // Boundary reconciliation can synchronously invoke user blur/authority/close callbacks.
    // No retained state references cross a call; ComponentId includes the full generation.
    for (const auto branch : boundaries) {
        const auto* current = services_->components().state<TimelineState>(id);
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
    const auto id = build.mount_component<TimelineState>();
    auto& s = build.state<TimelineState>(id);
    s.node = build.root(id);
    s.mode = mode;
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
            for (std::size_t i = 0; i < items.size(); ++i) {
                s.items.emplace_back();
                auto& item = s.items.back();
                item.rich_title = items[i].rich_title_.has_value();
                item.custom_icon = items[i].icon_.has_value();
                item.title = titles[i];
                item.title_text.set(item.title);
                item.snapshot.loading = loading[i];
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
                    region(item.snapshot.viewport, Content{[slot = items[i].rich_content_, text = items[i].content_] {
                               if (slot) {
                                   SlotContentAccess::function (*slot)();
                               } else {
                                   Text(TextProps{}.content(text));
                               }
                           }},
                           s.text_color, s.font);
                item.snapshot.icon = region(item.snapshot.viewport, Content{[slot = items[i].icon_] {
                                                if (slot) {
                                                    SlotContentAccess::function (*slot)();
                                                } else {
                                                    Icon(IconProps{}.name(IconName::LoadingOutlined));
                                                }
                                            }},
                                            item.icon_color, item.icon_font);
                const auto fragment = context.register_scene_fragment(item.snapshot.viewport,
                                                                      runtime::SceneFragmentPlacement::before_children);
                item.surface = services.surfaces().create_content_range(fragment, {});
                context.on_resource_cleanup(item.snapshot.viewport, [&services, surface = item.surface] {
                    services.surfaces().destroy_content_range(surface);
                });
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
        if (auto* s = services_->components().state<TimelineState>(id)) {
            auto order = timeline_order(value, s->reverse, s->model);
            s->mode = value;
            s->order = std::move(order);
            refresh(id, true);
        }
    });
    connect_prop(scope, props.reverse_, [this, id](bool value) {
        if (auto* s = services_->components().state<TimelineState>(id)) {
            s->order = timeline_order(s->mode, value, s->model);
            s->reverse = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.visible_, [this, id](bool value) {
        if (auto* s = services_->components().state<TimelineState>(id)) {
            s->visible = value;
            refresh(id, true);
        }
    });
    for (std::size_t i = 0; i < items.size(); ++i) {
        connect_prop(scope, items[i].title_, [this, id, i](String value) {
            if (auto* s = services_->components().state<TimelineState>(id)) {
                s->items[i].title = value;
                s->items[i].title_text.set(value);
                refresh(id, true);
            }
        });
        connect_prop(scope, items[i].placement_, [this, id, i](std::optional<TimelinePlacement> value) {
            if (auto* s = services_->components().state<TimelineState>(id)) {
                auto model = s->model;
                model[i].placement = value;
                auto order = timeline_order(s->mode, s->reverse, model);
                s->model = std::move(model);
                s->order = std::move(order);
                refresh(id, true);
            }
        });
        connect_prop(scope, items[i].tone_, [this, id, i](TimelineTone value) {
            if (auto* s = services_->components().state<TimelineState>(id)) {
                auto model = s->model;
                model[i].tone = value;
                static_cast<void>(timeline_order(s->mode, s->reverse, model));
                s->model = std::move(model);
                refresh(id, false);
            }
        });
        connect_prop(scope, items[i].visible_, [this, id, i](bool value) {
            if (auto* s = services_->components().state<TimelineState>(id)) {
                s->model[i].visible = value;
                s->order = timeline_order(s->mode, s->reverse, s->model);
                refresh(id, true);
            }
        });
        connect_prop(scope, items[i].loading_, [this, id, i](bool value) {
            if (auto* s = services_->components().state<TimelineState>(id)) {
                s->items[i].snapshot.loading = value;
                refresh(id, true);
            }
        });
    }
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

runtime::Size TimelineComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& e,
                                             layout::Constraints limits) {
    auto& s = *services_->components().state<TimelineState>(id);
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
        const float axis = dual ? width / 2 : end ? width - rail_width / 2 : rail_width / 2;
        const float left = 0;
        const float right = dual ? axis + rail_width / 2 + gap : end ? 0 : rail_width + gap;
        const auto title = size(item.snapshot.title, dual ? body_width : 0);
        const auto content = size(item.snapshot.content, body_width);
        const float height = std::max({content.height, dual ? title.height : 0, m.line_height, head_height});
        const float center = std::max(m.line_height, head_height) / 2;
        item.frame = {0, y, width, height + (position + 1 < s.order.size() ? m.item_padding_bottom : 0)};
        item.title_frame = {end ? right : left, 0, dual ? body_width : 0, dual ? title.height : 0};
        item.content_frame = {dual ? (end ? left : right) : right, 0, body_width, content.height};
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
    return limits.constrain({width, y});
}

void TimelineComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& e, runtime::Rect bounds) {
    const auto& s = *services_->components().state<TimelineState>(id);
    for (auto entry : s.order) {
        const auto& item = s.items[entry.item];
        const runtime::Point origin{bounds.x, bounds.y + item.frame.y};
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

void TimelineComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<TimelineState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().timeline();
        const auto& root = services_->nodes().require(s->node);
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
        const float opacity = services_->nodes().content_opacity(s->node);
        for (std::size_t position = 0; position < s->order.size(); ++position) {
            const auto& item = s->items[s->order[position].item];
            const runtime::Point origin{root.bounds.x + root.translation.x,
                                        root.bounds.y + root.translation.y + item.frame.y};
            const auto color = tone_color(t.colors, s->model[s->order[position].item].tone);
            std::vector<graphics::RoundedEffectInstance> effects;
            const auto dot = offset(item.snapshot.dot, {root.bounds.x, root.bounds.y + item.frame.y});
            if (!item.custom_icon && !item.snapshot.loading && dot.width > 0) {
                auto fill = graphics::make_corner_fill_effects({dot, dot.width / 2}, {true, true, true, true},
                                                               t.colors.dot_background, opacity, root.translation,
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
            const bool dashed = s->reverse && position + 1 < s->order.size()
                                    ? s->items[s->order[position + 1].item].snapshot.loading
                                    : item.snapshot.loading;
            const auto line = [&](runtime::Rect rect) {
                rect = intersect(rect, clip);
                if (rect.width > 0 && rect.height > 0) {
                    lines.push_back(
                        {{rect.x, rect.y, rect.width, rect.height},
                         {t.colors.tail.red(), t.colors.tail.green(), t.colors.tail.blue(), t.colors.tail.alpha()},
                         opacity});
                }
            };
            if (dashed && tail.width > 0) {
                const float dash = std::max(2.0F, t.metrics.tail_width * 2);
                // Bound allocation/work even for unusually large but valid logical extents.
                const float start = std::max(tail.y, clip.y);
                const float end = std::min(tail.y + tail.height, clip.y + clip.height);
                for (float y = start; y < end;) {
                    const float next = y + dash * 2;
                    line({tail.x, y, tail.width, std::min(dash, end - y)});
                    if (next <= y) {
                        break;
                    }
                    y = next;
                }
            } else {
                line(tail);
            }
            static_cast<void>(services_->surfaces().update_content_range(item.surface, lines));
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Timeline(TimelineProps props, std::vector<TimelineItem> items) {
    if (!detail::active_timeline) {
        throw std::logic_error("Timeline requires window component services");
    }
    detail::active_timeline->mount(props, items);
}
} // namespace ryn
