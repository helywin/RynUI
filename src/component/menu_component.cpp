#include "component/menu_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local MenuComponentHost* active_menus{};

struct MenuRow final {
    String key;
    MenuItemKind kind{};
    std::optional<std::size_t> parent;
    std::size_t depth{};
    runtime::NodeId node;
    input::InteractionId interaction;
    runtime::SceneFragmentId hit;
    component::RetainedSurfaceId surface;
    runtime::ComponentId label_component;
    std::optional<runtime::ComponentId> icon;
    std::optional<runtime::ComponentId> arrow;
    Signal<String> label{String{}};
    Signal<String> arrow_label{String{u8"›"}};
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
    bool disabled{};
    bool danger{};
    bool shown{true};
    bool eligible{};
};

struct MenuState final {
    runtime::NodeId node;
    input::InteractionId interaction;
    component::RetainedSurfaceId background;
    std::vector<runtime::ComponentId> rows;
    MenuKeys selected;
    MenuKeys open;
    std::optional<std::size_t> active;
    bool controlled_selected{};
    bool controlled_open{};
    bool selectable{true};
    bool multiple{};
    bool disabled{};
    bool action_menu{};
    bool focus_visible{};
    bool reveal{};
    float scroll{};
    float content_height{};
    std::array<theme_runtime::Subscription, 4> subscriptions;
    std::function<void(MenuClickInfo)> on_click;
    std::function<void(MenuSelectInfo)> on_select;
    std::function<void(MenuKeys)> on_open;
};

bool contains(const MenuKeys& keys, const String& key) {
    return std::ranges::find(keys, key) != keys.end();
}

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

graphics::QuadInstance quad(runtime::Rect bounds, Color color, float radius = 0) {
    graphics::QuadInstance q;
    q.bounds = {bounds.x, bounds.y, bounds.width, bounds.height};
    q.color = foreground(color);
    q.corner_radius = radius;
    return q;
}

bool inside(runtime::Rect rect, float x, float y) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
}

runtime::Rect intersect(runtime::Rect a, runtime::Rect b) {
    const float x = std::max(a.x, b.x);
    const float y = std::max(a.y, b.y);
    return {x, y, std::max(0.0F, std::min(a.x + a.width, b.x + b.width) - x),
            std::max(0.0F, std::min(a.y + a.height, b.y + b.height) - y)};
}

float row_height(const MenuRow& row, const MenuThemeToken& token, float item_height) {
    return row.kind == MenuItemKind::Divider ? token.metrics.divider_height
           : row.kind == MenuItemKind::Group ? token.metrics.group_height
                                             : item_height;
}

void validate_keys(const MenuKeys& keys) {
    MenuKeys seen;
    for (const auto& key : keys) {
        if (key.empty() || contains(seen, key)) {
            throw std::invalid_argument("Menu state keys must be nonempty and unique");
        }
        seen.push_back(key);
    }
}

void validate_items(const std::vector<MenuItem>& items, MenuKeys& keys, MenuMode mode, std::size_t depth) {
    if (depth > 32) {
        throw std::invalid_argument("Menu nesting exceeds 32 levels");
    }
    for (const auto& item : items) {
        if (item.key.empty() || contains(keys, item.key)) {
            throw std::invalid_argument("Menu item keys must be nonempty and unique");
        }
        keys.push_back(item.key);
        if (item.kind < MenuItemKind::Item || item.kind > MenuItemKind::SubMenu ||
            (item.kind != MenuItemKind::Group && item.kind != MenuItemKind::SubMenu && !item.children.empty()) ||
            (item.kind == MenuItemKind::SubMenu && mode != MenuMode::Inline)) {
            throw std::invalid_argument("Menu item topology is invalid; submenus require Inline mode");
        }
        validate_items(item.children, keys, mode, depth + 1);
    }
}
} // namespace

MenuComponentHost::MenuComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

MenuComponentHost::~MenuComponentHost() {
    services_->detach(*this);
}

void* MenuComponentHost::begin_mount() noexcept {
    auto* previous = active_menus;
    active_menus = this;
    return previous;
}

void MenuComponentHost::end_mount(void* previous) noexcept {
    active_menus = static_cast<MenuComponentHost*>(previous);
}

void MenuComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void MenuComponentHost::validate(const MenuProps& props) {
    if ((props.selected_ && props.default_selected_) || (props.open_ && props.default_open_) ||
        (props.mode_ != MenuMode::Vertical && props.mode_ != MenuMode::Inline)) {
        throw std::invalid_argument("Menu controlled/default state or mode conflicts");
    }
    MenuKeys keys;
    validate_items(props.items_, keys, props.mode_, 0);
    validate_keys(props.selected_ ? read_prop(*props.selected_) : props.default_selected_.value_or(MenuKeys{}));
    validate_keys(props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(MenuKeys{}));
}

MenuSnapshot MenuComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<MenuState>(id);
    if (!s) {
        throw std::out_of_range("Menu component is stale");
    }
    return {s->rows,
            s->selected,
            s->open,
            s->active ? std::optional{services_->components().state<MenuRow>(s->rows[*s->active])->key} : std::nullopt,
            s->interaction,
            s->scroll};
}

void MenuComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<MenuState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto& token = services_->components().theme_scope(id)->snapshot().menu();
    services_->interactions().set_eligible(s->interaction, !s->disabled);
    std::optional<std::size_t> first;
    for (std::size_t i = 0; i < s->rows.size(); ++i) {
        auto* row = services_->components().state<MenuRow>(s->rows[i]);
        bool shown = true;
        bool disabled = row->disabled || s->disabled;
        for (auto parent = row->parent; parent;) {
            const auto* ancestor = services_->components().state<MenuRow>(s->rows[*parent]);
            shown &= ancestor->shown && (ancestor->kind != MenuItemKind::SubMenu || contains(s->open, ancestor->key));
            disabled |= ancestor->disabled;
            parent = ancestor->parent;
        }
        row->shown = shown;
        row->eligible = shown && !disabled && (row->kind == MenuItemKind::Item || row->kind == MenuItemKind::SubMenu);
        if (row->eligible && !first) {
            first = i;
        }
        if (services_->components().set_branch_active(s->rows[i], shown)) {
            geometry = true;
            services_->mark_scene_structure_dirty();
        }
        services_->interactions().set_eligible(row->interaction, row->eligible);
        const bool selected = row->kind == MenuItemKind::Item && contains(s->selected, row->key);
        row->foreground.set(foreground(disabled                           ? token.colors.disabled
                                       : row->kind == MenuItemKind::Group ? token.colors.group
                                       : row->danger                      ? token.colors.danger
                                       : selected                         ? token.colors.selected
                                                                          : token.colors.text));
        const auto& font = token.typography;
        row->typography.set({font.font_family, font.font_weight, false, font.font_size, font.line_height});
        if (row->kind == MenuItemKind::SubMenu) {
            row->arrow_label.set(contains(s->open, row->key) ? String{u8"⌄"} : String{u8"›"});
        }
    }
    if (!s->active || *s->active >= s->rows.size() ||
        !services_->components().state<MenuRow>(s->rows[*s->active])->eligible) {
        s->active = first;
        s->reveal = true;
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
}

void MenuComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    refresh(id, geometry);
}

void MenuComponentHost::toggle(runtime::ComponentId id, std::size_t index, bool open) {
    auto* s = services_->components().state<MenuState>(id);
    if (!s || index >= s->rows.size() || !services_->interactions().component_active(id)) {
        return;
    }
    const auto* row = services_->components().state<MenuRow>(s->rows[index]);
    if (!row || !row->eligible || row->kind != MenuItemKind::SubMenu || contains(s->open, row->key) == open) {
        return;
    }
    auto keys = s->open;
    if (open) {
        keys.push_back(row->key);
    } else {
        std::erase(keys, row->key);
    }
    const auto callback = s->on_open;
    if (!s->controlled_open) {
        s->open = keys;
    }
    refresh(id, true);
    if (callback) {
        callback(std::move(keys));
    }
}

void MenuComponentHost::activate(runtime::ComponentId id, std::size_t index) {
    auto* s = services_->components().state<MenuState>(id);
    if (!s || !services_->components().scope(id).active() || index >= s->rows.size() ||
        !services_->interactions().component_active(id)) {
        return;
    }
    const auto* row = services_->components().state<MenuRow>(s->rows[index]);
    if (!row || !row->eligible) {
        return;
    }
    if (row->kind == MenuItemKind::SubMenu) {
        toggle(id, index, !contains(s->open, row->key));
        return;
    }
    if (row->kind != MenuItemKind::Item) {
        return;
    }
    MenuClickInfo click{row->key, {row->key}};
    for (auto parent = row->parent; parent;) {
        const auto* ancestor = services_->components().state<MenuRow>(s->rows[*parent]);
        if (ancestor->kind == MenuItemKind::SubMenu) {
            click.key_path.push_back(ancestor->key);
        }
        parent = ancestor->parent;
    }
    const auto on_click = s->on_click;
    const auto on_select = s->on_select;
    const bool selected = !s->multiple || !contains(s->selected, row->key);
    auto next = s->multiple ? s->selected : MenuKeys{};
    if (selected && !contains(next, row->key)) {
        next.push_back(row->key);
    }
    if (!selected) {
        std::erase(next, row->key);
    }
    const bool notify = s->selectable && next != s->selected;
    const MenuSelectInfo info{row->key, next, selected};
    if (notify && !s->controlled_selected) {
        s->selected = std::move(next);
    }
    refresh(id);
    if (notify && on_select) {
        on_select(info);
    }
    if (on_click && services_->components().contains(id) && services_->components().scope(id).active()) {
        on_click(std::move(click));
    }
}

void MenuComponentHost::mount_rows(runtime::ComponentId id, const std::vector<MenuItem>& items,
                                   std::optional<std::size_t> parent, std::size_t depth) {
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    auto& s = *services.components().state<MenuState>(id);
    for (const auto& item : items) {
        const auto row_id = build.mount_component<MenuRow>();
        auto& row = build.state<MenuRow>(row_id);
        row.node = build.root(row_id);
        row.key = item.key;
        row.kind = item.kind;
        row.parent = parent;
        row.depth = depth;
        row.label.set(read_prop(item.label));
        row.disabled = read_prop(item.disabled);
        row.danger = read_prop(item.danger);
        const auto index = s.rows.size();
        s.rows.push_back(row_id);
        services.nodes().require(row.node).clip_content = true;
        row.interaction =
            services.interactions().create({row_id, row.node, s.interaction, true, false, {}, false, false});
        input::InteractionHandlers handlers;
        handlers.target = [this, id, row_id, index](input::PointerDispatchContext& event) {
            auto* state = services_->components().state<MenuState>(id);
            const auto* row = services_->components().state<MenuRow>(row_id);
            if (!state || !row || !row->eligible) {
                return;
            }
            if (event.kind() == input::PointerEventKind::enter || event.kind() == input::PointerEventKind::move) {
                if (state->active != index) {
                    state->active = index;
                    refresh(id);
                }
            }
            if (event.event().button != input::PointerButton::primary) {
                return;
            }
            if (event.kind() == input::PointerEventKind::down) {
                state->active = index;
                services_->focus().defer_focus(state->interaction, input::FocusModality::pointer);
                static_cast<void>(event.capture_pointer());
                event.stop_propagation();
            }
            if (event.kind() == input::PointerEventKind::up) {
                const auto rect = services_->nodes().require(row->node).bounds;
                const auto clip =
                    services_->nodes().content_clip(row->node, services_->nodes().require(state->node).bounds);
                const bool hit = event.press_origin() == row->interaction &&
                                 inside(rect, event.event().x, event.event().y) &&
                                 inside(clip, event.event().x, event.event().y);
                static_cast<void>(event.release_pointer_capture());
                event.stop_propagation();
                if (hit) {
                    activate(id, index);
                }
            }
        };
        services.interactions().set_handlers(row.interaction, std::move(handlers));
        row.hit = build.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
        services.scene_composer().set_fragment(row.hit, {}, row.interaction);
        const auto surface = build.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
        auto initial = quad({}, Color{});
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        row.surface = services.surfaces().create_surface(row_id, row.node, surface, std::span{&initial, 1}, effects);
        build.on_resource_cleanup(
            row_id, [&services, node = row.node, interaction = row.interaction, hit = row.hit, surface = row.surface] {
                services.pointer().cancel_interaction(interaction);
                services.interactions().remove(interaction);
                services.scene_composer().remove_fragment(hit);
                services.surfaces().destroy(surface);
                services.layout().remove_layout(node);
            });
        if (item.kind != MenuItemKind::Divider) {
            build.mount_slot_with_semantic_text_style(
                row_id, Content{[&] {
                    auto& children = runtime::require_component_build_context();
                    if (item.icon) {
                        row.icon = children.mount_component<int>(0);
                        const auto icon_node = children.root(*row.icon);
                        services.layout().set_layout(icon_node, layout::BoxLayout{});
                        children.on_resource_cleanup(
                            *row.icon, [&services, icon_node] { services.layout().remove_layout(icon_node); });
                        children.mount_slot(*row.icon, *item.icon);
                    }
                    Text(TextProps{}.content(row.label));
                    row.label_component = services.components().children(row_id).back();
                    if (item.kind == MenuItemKind::SubMenu) {
                        Text(TextProps{}.content(row.arrow_label));
                        row.arrow = services.components().children(row_id).back();
                    }
                }},
                row.foreground, row.typography);
        }
        services.layout().set_layout(
            row.node,
            layout::ComponentLayout{
                [this, id, row_id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                    const auto& state = *services_->components().state<MenuState>(id);
                    const auto& row = *services_->components().state<MenuRow>(row_id);
                    const auto& theme = services_->components().theme_scope(id)->snapshot();
                    const auto& token = theme.menu();
                    float width =
                        token.metrics.padding_inline * 2 + static_cast<float>(row.depth) * token.metrics.inline_indent;
                    float height = row_height(row, token,
                                              state.action_menu ? theme.dropdown().metrics.item_height
                                                                : token.metrics.item_height);
                    for (auto child : services_->components().children(row_id)) {
                        const auto size =
                            engine.measure_child(services_->components().root(child), {0, limits.max_width, 0, height});
                        width += size.width + (child == row.label_component ? 0 : 8);
                    }
                    return limits.constrain({width, height});
                },
                [this, id, row_id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                    const auto& row = *services_->components().state<MenuRow>(row_id);
                    const auto& token = services_->components().theme_scope(id)->snapshot().menu();
                    const float inset =
                        std::min(rect.width / 2, token.metrics.padding_inline +
                                                     static_cast<float>(row.depth) * token.metrics.inline_indent);
                    float x = rect.x + inset;
                    float end = rect.x + rect.width - std::min(rect.width / 2, token.metrics.padding_inline);
                    if (row.arrow) {
                        const auto node = services_->components().root(*row.arrow);
                        const auto size = engine.measure_child(node, {0, std::max(0.0F, end - x), 0, rect.height});
                        end -= size.width;
                        engine.place_child(node,
                                           {end, rect.y + (rect.height - size.height) / 2, size.width, size.height});
                        end = std::max(x, end - 8);
                    }
                    if (row.icon) {
                        const auto node = services_->components().root(*row.icon);
                        const auto size = engine.measure_child(node, {0, std::max(0.0F, end - x), 0, rect.height});
                        engine.place_child(node,
                                           {x, rect.y + (rect.height - size.height) / 2, size.width, size.height});
                        x = std::min(end, x + size.width + 8);
                    }
                    if (row.kind != MenuItemKind::Divider) {
                        const auto node = services_->components().root(row.label_component);
                        const auto size = engine.measure_child(node, {0, std::max(0.0F, end - x), 0, rect.height});
                        engine.place_child(
                            node, {x, rect.y + (rect.height - size.height) / 2, std::max(0.0F, end - x), size.height});
                    }
                }});
        connect_prop(build.scope(row_id), item.label, [this, id, row_id](String value) {
            if (auto* row = services_->components().state<MenuRow>(row_id)) {
                row->label.set(std::move(value));
            }
        });
        connect_prop(build.scope(row_id), item.disabled, [this, id, row_id](bool value) {
            if (auto* row = services_->components().state<MenuRow>(row_id)) {
                row->disabled = value;
                refresh(id);
            }
        });
        connect_prop(build.scope(row_id), item.danger, [this, id, row_id](bool value) {
            if (auto* row = services_->components().state<MenuRow>(row_id)) {
                row->danger = value;
                refresh(id);
            }
        });
        mount_rows(id, item.children, index, depth + (item.kind == MenuItemKind::SubMenu ? 1 : 0));
    }
}

void MenuComponentHost::mount(const MenuProps& props, bool action_menu) {
    validate(props);
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<MenuState>();
    auto& s = build.state<MenuState>(id);
    s.node = build.root(id);
    s.action_menu = action_menu;
    s.disabled = read_prop(props.disabled_);
    s.selectable = read_prop(props.selectable_);
    s.multiple = read_prop(props.multiple_);
    s.controlled_selected = props.selected_.has_value();
    s.controlled_open = props.open_.has_value();
    s.selected = props.selected_ ? read_prop(*props.selected_) : props.default_selected_.value_or(MenuKeys{});
    s.open = props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(MenuKeys{});
    s.on_click = props.on_click_;
    s.on_select = props.on_select_;
    s.on_open = props.on_open_;
    services.nodes().require(s.node).clip_content = true;
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    s.interaction = services.interactions().create({id, s.node, {}, !s.disabled, true, {}, true, true});
    input::FocusHandlers focus;
    focus.state_changed = [this, id](input::FocusPresentation p) {
        if (auto* state = services_->components().state<MenuState>(id)) {
            state->focus_visible = p.focus_visible;
            refresh(id);
        }
    };
    focus.activate = [this, id] {
        if (const auto* state = services_->components().state<MenuState>(id); state && state->active) {
            activate(id, *state->active);
        }
    };
    services.interactions().set_focus_handlers(s.interaction, std::move(focus));
    const auto surface = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    auto initial = quad({}, Color{});
    component::RetainedSurfaceEffects effects;
    effects.focus_enabled = false;
    s.background = services.surfaces().create(id, s.node, surface, s.interaction, std::span{&initial, 1}, effects);
    build.on_resource_cleanup(id, [&services, node = s.node, interaction = s.interaction, surface = s.background] {
        services.pointer().cancel_interaction(interaction);
        services.interactions().remove(interaction);
        services.surfaces().destroy(surface);
        services.layout().remove_layout(node);
    });
    build.mount_slot(id, Content{[&] { mount_rows(id, props.items_, {}, 0); }});
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                auto& state = *services_->components().state<MenuState>(id);
                const auto& token = services_->components().theme_scope(id)->snapshot().menu();
                const float margin = token.metrics.item_margin;
                float width = 0;
                float height = margin;
                for (auto row_id : state.rows) {
                    const auto* row = services_->components().state<MenuRow>(row_id);
                    if (!row->shown) {
                        continue;
                    }
                    const auto size = engine.measure_child(row->node, {0, std::max(0.0F, limits.max_width - margin * 2),
                                                                       0, std::numeric_limits<float>::infinity()});
                    width = std::max(width, size.width);
                    height += size.height + margin;
                }
                state.content_height = height;
                return limits.constrain({width + margin * 2, height});
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                auto& state = *services_->components().state<MenuState>(id);
                const auto& theme = services_->components().theme_scope(id)->snapshot();
                const auto& token = theme.menu();
                const float margin = std::min(token.metrics.item_margin, rect.width / 2);
                state.scroll = std::clamp(state.scroll, 0.0F, std::max(0.0F, state.content_height - rect.height));
                if (state.reveal && state.active) {
                    float top = margin;
                    for (std::size_t i = 0; i < state.rows.size(); ++i) {
                        const auto& row = *services_->components().state<MenuRow>(state.rows[i]);
                        if (!row.shown) {
                            continue;
                        }
                        const float height = row_height(row, token,
                                                        state.action_menu ? theme.dropdown().metrics.item_height
                                                                          : token.metrics.item_height);
                        if (i == *state.active) {
                            if (top < state.scroll) {
                                state.scroll = top;
                            }
                            if (top + height > state.scroll + rect.height) {
                                state.scroll = std::max(0.0F, top + height - rect.height);
                            }
                            break;
                        }
                        top += height + margin;
                    }
                    state.reveal = false;
                }
                float y = rect.y + margin - state.scroll;
                for (auto row_id : state.rows) {
                    const auto& row = *services_->components().state<MenuRow>(row_id);
                    if (!row.shown) {
                        continue;
                    }
                    const float height = row_height(row, token,
                                                    state.action_menu ? theme.dropdown().metrics.item_height
                                                                      : token.metrics.item_height);
                    const float width = std::max(0.0F, rect.width - margin * 2);
                    static_cast<void>(engine.measure_child(row.node, layout::Constraints::fixed(width, height)));
                    engine.place_child(row.node, {rect.x + margin, y, width, height});
                    y += height + margin;
                }
            }});
    mounted_.push_back(id);
    refresh(id, true);
    auto& scope = build.scope(id);
    if (props.selected_) {
        connect_prop(scope, *props.selected_, [this, id](MenuKeys keys) {
            validate_keys(keys);
            if (auto* s = services_->components().state<MenuState>(id)) {
                s->selected = std::move(keys);
                refresh(id);
            }
        });
    }
    if (props.open_) {
        connect_prop(scope, *props.open_, [this, id](MenuKeys keys) {
            validate_keys(keys);
            if (auto* s = services_->components().state<MenuState>(id)) {
                s->open = std::move(keys);
                refresh(id, true);
            }
        });
    }
    const auto flag = [&](const Prop<bool>& prop, bool MenuState::* field) {
        connect_prop(scope, prop, [this, id, field](bool value) {
            if (auto* s = services_->components().state<MenuState>(id)) {
                s->*field = value;
                refresh(id);
            }
        });
    };
    flag(props.disabled_, &MenuState::disabled);
    flag(props.selectable_, &MenuState::selectable);
    flag(props.multiple_, &MenuState::multiple);
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::menu_colors, &theme_runtime::ThemeScope::menu_metrics,
                               &theme_runtime::ThemeScope::menu_typography};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i != 0](theme_runtime::DirtyPhase) { update_theme(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    s.subscriptions[3] = theme->capture([this, id](theme_runtime::DirtyPhase) { update_theme(id, true); },
                                        [theme] { static_cast<void>(theme->dropdown_metrics()); });
}

void MenuComponentHost::focus_menu(runtime::ComponentId id, bool last) {
    auto* s = services_->components().state<MenuState>(id);
    if (!s || s->disabled) {
        return;
    }
    s->active.reset();
    for (std::size_t i = 0; i < s->rows.size(); ++i) {
        if (services_->components().state<MenuRow>(s->rows[i])->eligible) {
            s->active = i;
            if (!last) {
                break;
            }
        }
    }
    s->reveal = true;
    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout);
    services_->focus().defer_focus(s->interaction, input::FocusModality::keyboard);
}

bool MenuComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.action != input::KeyAction::down || event.modifiers != input::KeyModifier::none) {
        return false;
    }
    const auto focused = services_->focus().state().focused;
    for (auto id : mounted_) {
        auto* s = services_->components().state<MenuState>(id);
        if (!s || s->disabled || focused != s->interaction || !services_->interactions().component_active(id)) {
            continue;
        }
        using input::Key;
        if (event.key == Key::home || event.key == Key::end) {
            focus_menu(id, event.key == Key::end);
            return true;
        }
        if (event.key == Key::down || event.key == Key::up) {
            std::vector<std::size_t> eligible;
            for (std::size_t i = 0; i < s->rows.size(); ++i) {
                if (services_->components().state<MenuRow>(s->rows[i])->eligible) {
                    eligible.push_back(i);
                }
            }
            if (eligible.empty()) {
                return true;
            }
            auto found = s->active ? std::ranges::find(eligible, *s->active) : eligible.end();
            auto pos = found == eligible.end() ? (event.key == Key::down ? eligible.size() - 1 : 0)
                                               : static_cast<std::size_t>(found - eligible.begin());
            pos = (pos + (event.key == Key::down ? 1 : eligible.size() - 1)) % eligible.size();
            s->active = eligible[pos];
        } else if ((event.key == Key::left || event.key == Key::right) && s->active) {
            const auto index = *s->active;
            const auto* row = services_->components().state<MenuRow>(s->rows[index]);
            if (row->kind == MenuItemKind::SubMenu && ((event.key == Key::right && !contains(s->open, row->key)) ||
                                                       (event.key == Key::left && contains(s->open, row->key)))) {
                toggle(id, index, event.key == Key::right);
                s = services_->components().state<MenuState>(id);
                if (!s) {
                    return true;
                }
            } else if (event.key == Key::left) {
                for (auto parent = row->parent; parent;) {
                    const auto* ancestor = services_->components().state<MenuRow>(s->rows[*parent]);
                    if (ancestor->kind == MenuItemKind::SubMenu) {
                        s->active = parent;
                        break;
                    }
                    parent = ancestor->parent;
                }
            } else if (row->kind == MenuItemKind::SubMenu && contains(s->open, row->key)) {
                for (std::size_t i = index + 1; i < s->rows.size(); ++i) {
                    const auto* child = services_->components().state<MenuRow>(s->rows[i]);
                    if (child->depth <= row->depth) {
                        break;
                    }
                    if (child->eligible) {
                        s->active = i;
                        break;
                    }
                }
            }
        } else {
            return false;
        }
        s->reveal = true;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Material);
        return true;
    }
    return false;
}

bool MenuComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    for (auto it = mounted_.rbegin(); it != mounted_.rend(); ++it) {
        auto* s = services_->components().state<MenuState>(*it);
        if (!s || !services_->interactions().component_active(*it)) {
            continue;
        }
        const auto rect = services_->nodes().require(s->node).bounds;
        if (!inside(rect, event.x, event.y)) {
            continue;
        }
        const float next =
            std::clamp(s->scroll - event.delta_y * 96, 0.0F, std::max(0.0F, s->content_height - rect.height));
        if (next != s->scroll) {
            s->scroll = next;
            s->reveal = false;
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::HitTest);
        }
        return true;
    }
    return false;
}

void MenuComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<MenuState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().menu();
        const auto rect = services_->nodes().require(s->node).bounds;
        auto background = quad(rect, token.colors.background);
        if (s->action_menu) {
            background.opacity = 0;
        }
        static_cast<void>(services_->surfaces().update_surface(s->background, std::span{&background, 1}));
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            const auto* row = services_->components().state<MenuRow>(s->rows[i]);
            if (!row->shown) {
                continue;
            }
            auto bounds = services_->nodes().require(row->node).bounds;
            const bool selected = row->kind == MenuItemKind::Item && contains(s->selected, row->key);
            const bool active = s->active == i && row->eligible;
            if (row->kind == MenuItemKind::Divider) {
                bounds.y += bounds.height / 2;
                bounds.height = 1;
            }
            auto q = quad(intersect(bounds, clip),
                          row->kind == MenuItemKind::Divider ? token.colors.divider
                          : selected                         ? token.colors.selected_background
                                                             : token.colors.hover_background,
                          token.metrics.border_radius);
            q.opacity = row->kind == MenuItemKind::Divider || selected || active ? 1.0F : 0.0F;
            static_cast<void>(services_->surfaces().update_surface(row->surface, std::span{&q, 1}));
            services_->scene_composer().set_fragment(row->hit, {}, row->interaction, clip);
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = active && s->focus_visible;
            effects.focus_color = token.colors.focus;
            effects.focus_width = 2;
            effects.focus_offset = 0;
            effects.focus_opacity = active && s->focus_visible ? 1.0F : 0.0F;
            effects.shape = {{bounds.x, bounds.y, bounds.width, bounds.height}, token.metrics.border_radius};
            effects.ancestor_clip = graphics::EffectClip{1, clip};
            static_cast<void>(services_->surfaces().update_effects(row->surface, effects));
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Menu(MenuProps props) {
    if (!detail::active_menus) {
        throw std::logic_error("Menu requires window component services");
    }
    detail::active_menus->mount(props);
}
} // namespace ryn
