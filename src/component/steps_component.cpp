#include "component/steps_component.hpp"
#include "component/navigation_model.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local StepsComponentHost* active_steps{};

struct StepsRow final {
    runtime::ComponentId component;
    runtime::NodeId node;
    runtime::NodeId title_node;
    runtime::ComponentId subtitle_component;
    runtime::ComponentId description_component;
    runtime::NodeId icon_node;
    runtime::ComponentId number_component;
    runtime::ComponentId status_component;
    input::InteractionId interaction;
    component::RetainedSurfaceId surface;
    Signal<String> title{String{}};
    Signal<String> subtitle{String{}};
    Signal<String> description{String{}};
    Signal<IconName> status_icon{IconName::CheckOutlined};
    Signal<runtime::SemanticForeground> title_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> detail_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> icon_color{{0, 0, 0, 1}};
    runtime::Size title_size;
    runtime::Size subtitle_size;
    runtime::Size description_size;
    runtime::Size natural;
    runtime::Size allocated;
    bool custom_icon{};
    bool rich_subtitle{};
    bool rich_description{};
};

struct StepsState final {
    runtime::NodeId node;
    StepsModel model;
    std::vector<StepsRow> rows;
    StepsSize size{StepsSize::Default};
    bool vertical{};
    Signal<runtime::SemanticTypography> title_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> detail_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> icon_font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 3> subscriptions;
    std::optional<std::size_t> hovered;
    std::optional<std::size_t> pressed;
    runtime::Rect clip;
    std::function<void(std::size_t)> callback;
};

void validate(StepsSize value) {
    if (value != StepsSize::Default && value != StepsSize::Small) {
        throw std::invalid_argument("Steps size is invalid");
    }
}

runtime::SemanticForeground fg(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

runtime::Rect translated(const runtime::Node& n) {
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

bool inside(runtime::Rect b, float x, float y) {
    return x >= b.x && y >= b.y && x < b.x + b.width && y < b.y + b.height;
}

bool subtitle_shown(const StepsRow& row) {
    return row.rich_subtitle || !row.subtitle.get().empty();
}

bool description_shown(const StepsRow& row) {
    return row.rich_description || !row.description.get().empty();
}

float icon_extent(const StepsState& s, const StepsMetricToken& m) {
    return s.size == StepsSize::Small ? m.icon_size_small : m.icon_size;
}

std::optional<std::size_t> focused(WindowComponentServices& services, const StepsState& s) {
    const auto focus = services.focus().state().focused;
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (focus == s.rows[i].interaction && s.model.eligible(i) &&
            services.interactions().branch_active(s.rows[i].interaction)) {
            return i;
        }
    }
    return {};
}

std::optional<std::size_t> edge(const StepsState& s, bool last = false) {
    for (std::size_t n = 0; n < s.rows.size(); ++n) {
        const auto i = last ? s.rows.size() - 1 - n : n;
        if (s.model.eligible(i)) {
            return i;
        }
    }
    return {};
}

runtime::ComponentId slot(WindowComponentServices& services, runtime::ComponentBuildContext& build,
                          const Content& content, const Signal<runtime::SemanticForeground>& color,
                          const Signal<runtime::SemanticTypography>& font) {
    const auto component = build.mount_component<int>(0);
    const auto node = build.root(component);
    services.nodes().require(node).clip_content = true;
    services.layout().set_layout(node, layout::BoxLayout{});
    build.on_resource_cleanup(component, [&services, node] { services.layout().remove_layout(node); });
    build.mount_slot_with_semantic_text_style(component, content, Prop<runtime::SemanticForeground>{color},
                                              Prop<runtime::SemanticTypography>{font});
    return component;
}
} // namespace

StepsComponentHost::StepsComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

StepsComponentHost::~StepsComponentHost() {
    services_->detach(*this);
}

void* StepsComponentHost::begin_mount() noexcept {
    auto* old = active_steps;
    active_steps = this;
    return old;
}

void StepsComponentHost::end_mount(void* old) noexcept {
    active_steps = static_cast<StepsComponentHost*>(old);
}

void StepsComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

StepsSnapshot StepsComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<StepsState>(id);
    if (!s) {
        throw std::out_of_range("Steps component is stale");
    }
    StepsSnapshot result{s->model.current, {}, {}, {}};
    for (std::size_t i = 0; i < s->rows.size(); ++i) {
        result.items.push_back(s->rows[i].component);
        result.statuses.push_back(s->model.status(i));
        result.icons.push_back(s->rows[i].icon_node);
    }
    return result;
}

void StepsComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<StepsState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().steps();
    const auto& m = t.metrics;
    const bool small = s->size == StepsSize::Small;
    s->title_font.set({m.font_family, 400, false, small ? m.title_font_size_small : m.title_font_size,
                       small ? m.title_line_height_small : m.title_line_height});
    s->detail_font.set({m.font_family, 400, false, m.font_size, m.line_height});
    s->icon_font.set({m.font_family, 400, false, m.icon_font_size, m.line_height});
    const bool shown = std::ranges::any_of(s->model.items, [](const auto& item) { return item.visible; });
    auto& root = services_->nodes().require(s->node);
    if (root.out_of_flow == shown) {
        root.out_of_flow = !shown;
        geometry = true;
    }
    if (services_->components().set_branch_active(id, shown)) {
        services_->mark_scene_structure_dirty();
        geometry = true;
    }
    auto entry = focused(*services_, *s);
    if (!entry && s->model.eligible(s->model.current)) {
        entry = s->model.current;
    }
    if (!entry) {
        entry = edge(*s);
    }
    std::vector<std::pair<runtime::ComponentId, bool>> boundaries;
    for (std::size_t i = 0; i < s->rows.size(); ++i) {
        auto& row = s->rows[i];
        const auto& item = s->model.items[i];
        const auto status = s->model.status(i);
        if (services_->components().set_branch_active(row.component, item.visible)) {
            services_->mark_scene_structure_dirty();
            geometry = true;
        }
        services_->nodes().require(row.node).out_of_flow = !item.visible;
        // A display-only row still hosts native child input; only true disabled/hidden state gates descendants.
        if (services_->interactions().set_eligible(row.interaction, item.visible && !item.disabled)) {
            services_->dirty().invalidate(row.node, runtime::DirtyFlags::HitTest);
        }
        services_->interactions().set_focusable(row.interaction, s->model.eligible(i));
        services_->interactions().set_tab_stop(row.interaction, entry == i);
        const Color title = item.disabled                             ? t.colors.disabled
                            : status == StepsStatus::Error            ? t.colors.error
                            : s->hovered == i && s->model.eligible(i) ? t.colors.hover
                            : status == StepsStatus::Wait             ? t.colors.description
                                                                      : t.colors.title;
        row.title_color.set(fg(title));
        row.detail_color.set(fg(item.disabled                  ? t.colors.disabled
                                : status == StepsStatus::Error ? t.colors.error
                                                               : t.colors.description));
        row.icon_color.set(fg(item.disabled                   ? t.colors.disabled
                              : status == StepsStatus::Error  ? t.colors.error
                              : status == StepsStatus::Finish ? t.colors.finish_text
                              : status == StepsStatus::Process
                                  ? (row.custom_icon ? t.colors.finish_text : t.colors.process_text)
                                  : t.colors.wait_text));
        const auto branch = [&](runtime::ComponentId component, bool active) {
            if (services_->components().set_branch_active(component, active)) {
                services_->mark_scene_structure_dirty();
                geometry = true;
            }
            services_->nodes().require(services_->components().root(component)).out_of_flow = !active;
        };
        branch(row.subtitle_component, subtitle_shown(row));
        branch(row.description_component, description_shown(row));
        if (!row.custom_icon) {
            const bool number = status == StepsStatus::Wait || status == StepsStatus::Process;
            branch(row.number_component, number);
            branch(row.status_component, !number);
            row.status_icon.set(status == StepsStatus::Error ? IconName::CloseOutlined : IconName::CheckOutlined);
        }
        boundaries.emplace_back(row.component,
                                !item.visible || item.disabled || !services_->focus().state().window_active);
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
    for (auto [component, blocked] : boundaries) {
        if (services_->components().contains(component)) {
            services_->set_input_blocked(component, blocked);
        }
    }
    reconcile(id);
}

void StepsComponentHost::reconcile(runtime::ComponentId id) {
    auto* s = services_->components().state<StepsState>(id);
    if (!s) {
        return;
    }
    std::vector<input::InteractionId> cancelled;
    for (std::size_t i = 0; i < s->rows.size(); ++i) {
        if (!s->model.eligible(i) || !services_->interactions().branch_active(s->rows[i].interaction)) {
            if (s->pressed == i) {
                s->pressed.reset();
            }
            if (s->hovered == i) {
                s->hovered.reset();
            }
            cancelled.push_back(s->rows[i].interaction);
        }
    }
    for (auto hit : cancelled) {
        services_->pointer().cancel_interaction(hit);
        services_->focus().cancel_interaction(hit);
    }
}

void StepsComponentHost::activate(runtime::ComponentId id, std::size_t i) {
    auto* s = services_->components().state<StepsState>(id);
    if (!s || i >= s->rows.size() || !services_->interactions().branch_active(s->rows[i].interaction) ||
        !s->model.request(i)) {
        return;
    }
    const auto callback = s->callback;
    refresh(id, false);
    if (callback && services_->components().contains(id) && services_->components().scope(id).active()) {
        callback(i);
    }
}

void StepsComponentHost::mount(const StepsProps& props, const std::vector<StepsItem>& items) {
    validate(read_prop(props.size_));
    validate_steps_status(read_prop(props.status_));
    if (props.current_ && props.default_current_) {
        throw std::invalid_argument("Steps current and defaultCurrent conflict");
    }
    StepsModel model;
    model.current = props.current_ ? read_prop(*props.current_) : props.default_current_.value_or(0);
    model.controlled = props.current_.has_value();
    model.interactive = static_cast<bool>(props.on_change_);
    model.current_status = read_prop(props.status_);
    for (const auto& item : items) {
        const auto status = read_prop(item.status_);
        if (status) {
            validate_steps_status(*status);
        }
        model.items.push_back({item.key_, read_prop(item.disabled_), read_prop(item.visible_), status});
    }
    validate_navigation_items(model.items);
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<StepsState>();
    auto& s = build.state<StepsState>(id);
    s.node = build.root(id);
    s.model = std::move(model);
    s.callback = props.on_change_;
    s.vertical = read_prop(props.vertical_);
    s.size = read_prop(props.size_);
    s.rows.resize(items.size());
    services.nodes().require(s.node).clip_content = true;
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    build.mount_slot(
        id, Content{[&] {
            auto& children = runtime::require_component_build_context();
            for (std::size_t i = 0; i < items.size(); ++i) {
                auto& row = s.rows[i];
                const auto& item = items[i];
                row.component = children.mount_component<int>(0);
                row.node = children.root(row.component);
                row.title.set(read_prop(item.title_));
                row.subtitle.set(read_prop(item.subtitle_));
                row.description.set(read_prop(item.description_));
                row.custom_icon = item.icon_.has_value();
                row.rich_subtitle = item.rich_subtitle_.has_value();
                row.rich_description = item.rich_description_.has_value();
                row.interaction =
                    services.interactions().create({row.component, row.node, {}, false, true, {}, true, false});
                const auto hit =
                    children.register_scene_fragment(row.component, runtime::SceneFragmentPlacement::before_children);
                services.scene_composer().set_fragment(hit, {}, row.interaction);
                const auto fragment =
                    children.register_scene_fragment(row.component, runtime::SceneFragmentPlacement::before_children);
                row.surface = services.surfaces().create_content_range(fragment, {});
                children.on_resource_cleanup(row.component, [&services, node = row.node, hit,
                                                             interaction = row.interaction, surface = row.surface] {
                    services.pointer().cancel_interaction(interaction);
                    services.focus().cancel_interaction(interaction);
                    services.interactions().remove(interaction);
                    services.scene_composer().remove_fragment(hit);
                    services.surfaces().destroy_content_range(surface);
                    services.layout().remove_layout(node);
                });
                services.layout().set_layout(
                    row.node,
                    layout::ComponentLayout{[this, id, i](layout::LayoutEngine& e, runtime::NodeId,
                                                          layout::Constraints c) { return measure_item(id, i, e, c); },
                                            [this, id, i](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect b) {
                                                place_item(id, i, e, b);
                                            }});
                input::InteractionHandlers handlers;
                handlers.target = [this, id, i](input::PointerDispatchContext& event) {
                    auto* current = services_->components().state<StepsState>(id);
                    if (!current) {
                        return;
                    }
                    if (event.kind() == input::PointerEventKind::cancel) {
                        current->pressed.reset();
                        refresh(id, false);
                        return;
                    }
                    if (!current->model.eligible(i) ||
                        !services_->interactions().branch_active(current->rows[i].interaction) ||
                        event.event().button != input::PointerButton::primary) {
                        return;
                    }
                    if (event.kind() == input::PointerEventKind::down) {
                        current->pressed = i;
                        static_cast<void>(event.capture_pointer());
                        event.stop_propagation();
                        refresh(id, false);
                    } else if (event.kind() == input::PointerEventKind::up) {
                        const auto& row = current->rows[i];
                        const auto& node = services_->nodes().require(row.node);
                        const bool hit =
                            current->pressed == i && event.press_origin() == row.interaction &&
                            inside(graphics::intersect_effect_bounds(
                                       translated(node), services_->nodes().content_clip(row.node, current->clip)),
                                   event.event().x, event.event().y);
                        current->pressed.reset();
                        static_cast<void>(event.release_pointer_capture());
                        event.stop_propagation();
                        refresh(id, false);
                        if (hit) {
                            activate(id, i);
                        }
                    }
                };
                services.interactions().set_handlers(row.interaction, std::move(handlers));
                input::FocusHandlers focus;
                focus.state_changed = [this, id](input::FocusPresentation) {
                    refresh(id, false);
                };
                services.interactions().set_focus_handlers(row.interaction, std::move(focus));
                children.mount_slot(
                    row.component, Content{[&] {
                        auto& inner = runtime::require_component_build_context();
                        const auto icon = inner.mount_component<int>(0);
                        row.icon_node = inner.root(icon);
                        services.nodes().require(row.icon_node).clip_content = true;
                        services.layout().set_layout(
                            row.icon_node,
                            layout::ComponentLayout{
                                [&services](layout::LayoutEngine& e, runtime::NodeId node, layout::Constraints c) {
                                    runtime::Size result;
                                    for (auto child : services.nodes().require(node).children) {
                                        if (services.nodes().require(child).out_of_flow) {
                                            continue;
                                        }
                                        const auto size = e.measure_child(child, {0, c.max_width, 0, c.max_height});
                                        result.width = std::max(result.width, size.width);
                                        result.height = std::max(result.height, size.height);
                                    }
                                    return c.constrain(result);
                                },
                                [&services](layout::LayoutEngine& e, runtime::NodeId node, runtime::Rect b) {
                                    for (auto child : services.nodes().require(node).children) {
                                        const auto& n = services.nodes().require(child);
                                        if (n.out_of_flow) {
                                            continue;
                                        }
                                        const float w = std::min(b.width, n.layout_size.width);
                                        const float h = std::min(b.height, n.layout_size.height);
                                        e.place_child(child, {b.x + (b.width - w) / 2, b.y + (b.height - h) / 2, w, h});
                                    }
                                }});
                        inner.on_resource_cleanup(
                            icon, [&services, node = row.icon_node] { services.layout().remove_layout(node); });
                        inner.mount_slot_with_semantic_text_style(
                            icon, Content{[&] {
                                auto& icon_build = runtime::require_component_build_context();
                                if (item.icon_) {
                                    SlotContentAccess::function (*item.icon_)();
                                } else {
                                    row.number_component =
                                        slot(services, icon_build,
                                             Content{[number = String::from_utf8(std::to_string(i + 1)).value()] {
                                                 Text(number);
                                             }},
                                             row.icon_color, s.icon_font);
                                    row.status_component =
                                        slot(services, icon_build,
                                             Content{[name = row.status_icon] { Icon(IconProps{}.name(name)); }},
                                             row.icon_color, s.icon_font);
                                }
                            }},
                            Prop<runtime::SemanticForeground>{row.icon_color},
                            Prop<runtime::SemanticTypography>{s.icon_font});
                        row.title_node = inner.root(slot(
                            services, inner,
                            item.rich_title_
                                ? Content{[content = *item.rich_title_] { SlotContentAccess::function(content)(); }}
                                : Content{[title = row.title] { Text(TextProps{}.content(title)); }},
                            row.title_color, s.title_font));
                        row.subtitle_component = slot(
                            services, inner,
                            item.rich_subtitle_
                                ? Content{[content = *item.rich_subtitle_] { SlotContentAccess::function(content)(); }}
                                : Content{[title = row.subtitle] { Text(TextProps{}.content(title)); }},
                            row.detail_color, s.detail_font);
                        row.description_component =
                            slot(services, inner,
                                 item.rich_description_
                                     ? Content{[content = *item.rich_description_] {
                                           SlotContentAccess::function(content)();
                                       }}
                                     : Content{[title = row.description] { Text(TextProps{}.content(title)); }},
                                 row.detail_color, s.detail_font);
                    }});
            }
        }});
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) { return measure(id, e, c); },
            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect b) { place(id, e, b); }});
    mounted_.push_back(id);
    if (props.current_) {
        connect_prop(build.scope(id), *props.current_, [this, id](std::size_t value) {
            if (auto* s = services_->components().state<StepsState>(id)) {
                s->model.current = value;
                refresh(id, false);
            }
        });
    }
    connect_prop(build.scope(id), props.status_, [this, id](StepsStatus value) {
        validate_steps_status(value);
        if (auto* s = services_->components().state<StepsState>(id)) {
            s->model.current_status = value;
            refresh(id, false);
        }
    });
    connect_prop(build.scope(id), props.size_, [this, id](StepsSize value) {
        validate(value);
        if (auto* s = services_->components().state<StepsState>(id)) {
            s->size = value;
            refresh(id, true);
        }
    });
    connect_prop(build.scope(id), props.vertical_, [this, id](bool value) {
        if (auto* s = services_->components().state<StepsState>(id)) {
            s->vertical = value;
            refresh(id, true);
        }
    });
    for (std::size_t i = 0; i < items.size(); ++i) {
        connect_prop(build.scope(id), items[i].title_, [this, id, i](String value) {
            if (auto* s = services_->components().state<StepsState>(id)) {
                s->rows[i].title.set(std::move(value));
            }
        });
        connect_prop(build.scope(id), items[i].subtitle_, [this, id, i](String value) {
            if (auto* s = services_->components().state<StepsState>(id)) {
                s->rows[i].subtitle.set(std::move(value));
                refresh(id, true);
            }
        });
        connect_prop(build.scope(id), items[i].description_, [this, id, i](String value) {
            if (auto* s = services_->components().state<StepsState>(id)) {
                s->rows[i].description.set(std::move(value));
                refresh(id, true);
            }
        });
        connect_prop(build.scope(id), items[i].visible_, [this, id, i](bool value) {
            if (auto* s = services_->components().state<StepsState>(id)) {
                s->model.items[i].visible = value;
                refresh(id, true);
            }
        });
        connect_prop(build.scope(id), items[i].disabled_, [this, id, i](bool value) {
            if (auto* s = services_->components().state<StepsState>(id)) {
                s->model.items[i].disabled = value;
                refresh(id, false);
            }
        });
        connect_prop(build.scope(id), items[i].status_, [this, id, i](std::optional<StepsStatus> value) {
            if (value) {
                validate_steps_status(*value);
            }
            if (auto* s = services_->components().state<StepsState>(id)) {
                s->model.items[i].status = value;
                refresh(id, false);
            }
        });
    }
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::steps_colors, &theme_runtime::ThemeScope::steps_metrics,
                               &theme_runtime::ThemeScope::steps_effects};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { refresh(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    refresh(id, true);
}

runtime::Size StepsComponentHost::measure_item(runtime::ComponentId id, std::size_t i, layout::LayoutEngine& engine,
                                               layout::Constraints c) {
    auto& s = *services_->components().state<StepsState>(id);
    auto& row = s.rows[i];
    const auto& m = services_->components().theme_scope(id)->snapshot().steps().metrics;
    const float icon = std::min(icon_extent(s, m), c.max_width);
    static_cast<void>(engine.measure_child(row.icon_node, {0, icon, 0, icon}));
    const float gap = std::min(m.icon_gap, std::max(0.0F, c.max_width - icon));
    const float available = std::max(0.0F, c.max_width - icon - gap);
    row.title_size = engine.measure_child(row.title_node, {0, available, 0, c.max_height});
    const float subtitle_gap =
        subtitle_shown(row) ? std::min(m.subtitle_gap, std::max(0.0F, available - row.title_size.width)) : 0;
    row.subtitle_size = subtitle_shown(row)
                            ? engine.measure_child(
                                  services_->components().root(row.subtitle_component),
                                  {0, std::max(0.0F, available - row.title_size.width - subtitle_gap), 0, c.max_height})
                            : runtime::Size{};
    row.description_size = description_shown(row)
                               ? engine.measure_child(services_->components().root(row.description_component),
                                                      {0, available, 0, c.max_height})
                               : runtime::Size{};
    const float header_height = std::max(row.title_size.height, row.subtitle_size.height);
    return c.constrain(
        {icon + gap +
             std::max(row.title_size.width + subtitle_gap + row.subtitle_size.width, row.description_size.width),
         std::max(icon, header_height) +
             (description_shown(row) ? m.description_gap + row.description_size.height : 0)});
}

void StepsComponentHost::place_item(runtime::ComponentId id, std::size_t i, layout::LayoutEngine& engine,
                                    runtime::Rect b) {
    auto& s = *services_->components().state<StepsState>(id);
    auto& row = s.rows[i];
    const auto& m = services_->components().theme_scope(id)->snapshot().steps().metrics;
    const float icon = std::min(icon_extent(s, m), std::min(b.width, b.height));
    engine.place_child(row.icon_node, {b.x, b.y, icon, icon}, true, true);
    const float gap = std::min(m.icon_gap, std::max(0.0F, b.width - icon));
    const float x = b.x + icon + gap;
    const float width = std::max(0.0F, b.width - icon - gap);
    const float header = std::min(b.height, std::max(std::max(row.title_size.height, row.subtitle_size.height), icon));
    const float th = std::min(header, row.title_size.height);
    const float tw = std::min(width, row.title_size.width);
    engine.place_child(row.title_node, {x, b.y + (header - th) / 2, tw, th});
    if (subtitle_shown(row)) {
        const float sg = std::min(m.subtitle_gap, width - tw);
        const float sh = std::min(header, row.subtitle_size.height);
        engine.place_child(services_->components().root(row.subtitle_component),
                           {x + tw + sg, b.y + (header - sh) / 2,
                            std::min(std::max(0.0F, width - tw - sg), row.subtitle_size.width), sh});
    }
    if (description_shown(row)) {
        const float dg = std::min(m.description_gap, std::max(0.0F, b.height - header));
        engine.place_child(services_->components().root(row.description_component),
                           {x, b.y + header + dg, std::min(width, row.description_size.width),
                            std::min(std::max(0.0F, b.height - header - dg), row.description_size.height)});
    }
}

runtime::Size StepsComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                          layout::Constraints c) {
    auto& s = *services_->components().state<StepsState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().steps().metrics;
    const auto count =
        static_cast<std::size_t>(std::ranges::count_if(s.model.items, [](const auto& item) { return item.visible; }));
    if (!count) {
        return c.constrain({});
    }
    float natural_width = 0;
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (s.model.items[i].visible) {
            auto& row = s.rows[i];
            row.natural = engine.measure_child(
                row.node, {0, std::numeric_limits<float>::infinity(), 0, std::numeric_limits<float>::infinity()});
            natural_width = s.vertical ? std::max(natural_width, row.natural.width) : natural_width + row.natural.width;
        }
    }
    const float total_gap = s.vertical ? 0 : m.item_gap * static_cast<float>(count - 1);
    const float width = std::isfinite(c.max_width) ? c.max_width : natural_width + total_gap;
    const float allocated = s.vertical ? width : std::max(0.0F, width - total_gap) / static_cast<float>(count);
    float height = 0;
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (s.model.items[i].visible) {
            auto& row = s.rows[i];
            row.allocated =
                engine.measure_child(row.node, {allocated, allocated, 0, std::numeric_limits<float>::infinity()});
            height = s.vertical ? height + row.allocated.height : std::max(height, row.allocated.height);
        }
    }
    if (s.vertical) {
        height += m.vertical_gap * static_cast<float>(count - 1);
    }
    return c.constrain({width, height});
}

void StepsComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect b) {
    auto& s = *services_->components().state<StepsState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().steps().metrics;
    const auto count =
        static_cast<std::size_t>(std::ranges::count_if(s.model.items, [](const auto& item) { return item.visible; }));
    if (!count) {
        return;
    }
    float natural_height = 0;
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (s.model.items[i].visible) {
            natural_height += s.rows[i].allocated.height;
        }
    }
    const float gap = count > 1 ? std::min(s.vertical ? m.vertical_gap : m.item_gap,
                                           (s.vertical ? b.height : b.width) / static_cast<float>(count - 1))
                                : 0;
    const float remaining = std::max(0.0F, (s.vertical ? b.height : b.width) - gap * static_cast<float>(count - 1));
    const float scale = natural_height > remaining && natural_height > 0 ? remaining / natural_height : 1;
    float cursor = 0;
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (!s.model.items[i].visible) {
            continue;
        }
        auto& row = s.rows[i];
        const float w = s.vertical ? b.width : remaining / static_cast<float>(count);
        const float h = s.vertical ? row.allocated.height * scale : b.height;
        static_cast<void>(engine.measure_child(row.node, layout::Constraints::fixed(w, h)));
        engine.place_child(row.node, {b.x + (s.vertical ? 0 : cursor), b.y + (s.vertical ? cursor : 0), w, h}, true,
                           true);
        cursor += (s.vertical ? h : w) + gap;
    }
}

bool StepsComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<StepsState>(id);
        if (!s) {
            continue;
        }
        auto target = focused(*services_, *s);
        if (!target) {
            continue;
        }
        const bool activation = event.key == input::Key::enter || event.key == input::Key::space;
        const bool previous = event.key == (s->vertical ? input::Key::up : input::Key::left);
        const bool next = event.key == (s->vertical ? input::Key::down : input::Key::right);
        if (!activation && !previous && !next && event.key != input::Key::home && event.key != input::Key::end) {
            return false;
        }
        if (event.action != input::KeyAction::down || (activation && event.repeat)) {
            return true;
        }
        if (previous || next) {
            for (std::size_t n = 0; n < s->rows.size(); ++n) {
                target = (*target + (previous ? s->rows.size() - 1 : 1)) % s->rows.size();
                if (s->model.eligible(*target)) {
                    break;
                }
            }
        } else if (event.key == input::Key::home || event.key == input::Key::end) {
            target = edge(*s, event.key == input::Key::end);
        }
        if (target) {
            services_->focus().defer_focus(s->rows[*target].interaction, input::FocusModality::keyboard);
            if (activation) {
                activate(id, *target);
            }
        }
        return true;
    }
    return false;
}

void StepsComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                          std::optional<input::InteractionId> hit,
                                          std::optional<input::InteractionId>) {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<StepsState>(id);
        if (!s) {
            continue;
        }
        std::optional<std::size_t> hovered;
        for (std::size_t i = 0; event.action != input::PointerAction::cancel && i < s->rows.size(); ++i) {
            if (hit == s->rows[i].interaction) {
                hovered = i;
            }
        }
        if (s->hovered != hovered) {
            s->hovered = hovered;
            refresh(id, false);
        }
    }
}

void StepsComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        reconcile(id);
    }
}

void StepsComponentHost::on_window_active(bool active) {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (auto* s = services_->components().state<StepsState>(id)) {
            if (!active) {
                s->pressed.reset();
                s->hovered.reset();
            }
            refresh(id, false);
        }
    }
}

void StepsComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<StepsState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        s->clip = services_->component_clip(id, viewport, document_clip);
        const auto& t = services_->components().theme_scope(id)->snapshot().steps();
        const auto focus = services_->focus().state();
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            if (!s->model.items[i].visible) {
                continue;
            }
            const auto& row = s->rows[i];
            const auto& node = services_->nodes().require(row.node);
            const auto& icon = services_->nodes().require(row.icon_node);
            const auto status = s->model.status(i);
            const float opacity = services_->nodes().content_opacity(row.node);
            const graphics::EffectClip clip{1, services_->nodes().content_clip(row.node, s->clip)};
            const graphics::EffectClip decoration_clip{2, s->clip};
            std::vector<graphics::RoundedEffectInstance> effects;
            if (!row.custom_icon) {
                const auto color = status == StepsStatus::Error     ? t.colors.error_background
                                   : status == StepsStatus::Finish  ? t.colors.finish_background
                                   : status == StepsStatus::Process ? t.colors.process_background
                                                                    : t.colors.wait_background;
                const auto circle = graphics::make_corner_fill_effects(
                    {icon.bounds, std::min(icon.bounds.width, icon.bounds.height) / 2}, {true, true, true, true}, color,
                    opacity, icon.translation, clip);
                effects.insert(effects.end(), circle.begin(), circle.end());
            }
            std::optional<std::size_t> next;
            for (std::size_t j = i + 1; j < s->rows.size(); ++j) {
                if (s->model.items[j].visible) {
                    next = j;
                    break;
                }
            }
            if (next) {
                const auto& following = services_->nodes().require(s->rows[*next].node);
                const float line_width =
                    std::min(t.metrics.line_width, s->vertical ? icon.bounds.width : icon.bounds.height);
                runtime::Rect line =
                    s->vertical ? runtime::Rect{icon.bounds.x + (icon.bounds.width - line_width) / 2,
                                                icon.bounds.y + icon.bounds.height, line_width,
                                                std::max(0.0F, following.bounds.y - icon.bounds.y - icon.bounds.height)}
                                : runtime::Rect{node.bounds.x + node.bounds.width,
                                                icon.bounds.y + (icon.bounds.height - line_width) / 2,
                                                std::max(0.0F, following.bounds.x - node.bounds.x - node.bounds.width),
                                                line_width};
                const auto rail = graphics::make_corner_fill_effects(
                    {line, 0}, {false, false, false, false},
                    s->model.status(*next) == StepsStatus::Error ? t.colors.error
                    : s->model.status(*next) == StepsStatus::Finish || s->model.status(*next) == StepsStatus::Process
                        ? t.colors.finished_connector
                        : t.colors.connector,
                    opacity, node.translation, clip);
                effects.insert(effects.end(), rail.begin(), rail.end());
            }
            const bool visible = s->model.eligible(i) && services_->interactions().branch_active(row.interaction) &&
                                 focus.focused == row.interaction && focus.focus_visible && t.effects.focus_width > 0 &&
                                 node.bounds.width > 0 && node.bounds.height > 0;
            effects.push_back(graphics::make_outline_effect(
                {node.bounds, 0}, t.effects.focus_width > 0 ? t.effects.focus_width : 1, t.effects.focus_offset,
                t.colors.focus, visible ? opacity : 0, node.translation, decoration_clip));
            static_cast<void>(services_->surfaces().update_content_effects(row.surface, effects));
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Steps(StepsProps props, std::vector<StepsItem> items) {
    if (!detail::active_steps) {
        throw std::logic_error("Steps requires window component services");
    }
    detail::active_steps->mount(props, items);
}
} // namespace ryn
