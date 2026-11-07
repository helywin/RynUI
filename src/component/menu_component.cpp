#include "component/config_defaults.hpp"
#include "component/menu_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "component/tooltip_component.hpp"
#include "input/text_boundary.hpp"
#include <ryn/icon.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local MenuComponentHost* active_menus{};

constexpr char8_t overflow_key[] = u8"$ryn.menu.overflow";

struct MenuPanel final {
    runtime::NodeId node;
    input::InteractionId interaction;
    component::RetainedSurfaceId surface;
    runtime::Rect bounds;
    float scroll{};
    float content_height{};
};

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
    std::optional<runtime::ComponentId> collapsed_label;
    Signal<String> collapsed_text{String{}};
    std::optional<runtime::ComponentId> icon;
    std::optional<runtime::ComponentId> arrow;
    Signal<String> label{String{}};
    Signal<IconName> arrow_icon{IconName::RightOutlined};
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
    std::unique_ptr<Scope> bindings;
    std::optional<runtime::ComponentId> extra;
    std::optional<runtime::ComponentId> popup;
    std::optional<std::size_t> panel;
    runtime::Rect full_bounds;
    bool overflowed{};
    bool overflow{};
    bool rich{};
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
    MenuMode mode{MenuMode::Vertical};
    MenuTheme theme{MenuTheme::Inherit};
    TextDirection direction{TextDirection::LeftToRight};
    MenuSubMenuTrigger submenu_trigger{MenuSubMenuTrigger::Hover};
    bool collapsed{};
    bool suppressed{};
    bool input_active{true};
    bool presented_popup{};
    bool overflow_open{};
    bool has_overflow{};
    runtime::Rect clip;
    bool reconciling{};
    float open_delay{};
    float close_delay{0.1F};
    std::optional<runtime::ComponentId> hover_row;
    std::optional<animation::AnimationTime> hover_deadline;
    animation::MonotonicTimeCursor hover_clock;
    bool controlled_selected{};
    bool controlled_open{};
    bool selectable{true};
    bool multiple{};
    bool disabled{};
    bool action_menu{};
    MenuColorToken applied_colors;
    bool focus_visible{};
    bool reveal{};
    float scroll{};
    float content_height{};
    std::array<theme_runtime::Subscription, 5> subscriptions;
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

runtime::Rect translated_bounds(WindowComponentServices& services, runtime::NodeId id) {
    const auto& node = services.nodes().require(id);
    auto rect = node.bounds;
    rect.x += node.translation.x;
    rect.y += node.translation.y;
    return rect;
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

void validate_items(const std::vector<MenuItem>& items, MenuKeys& keys, std::size_t depth) {
    if (depth > 32) {
        throw std::invalid_argument("Menu nesting exceeds 32 levels");
    }
    for (const auto& item : items) {
        if (item.key.empty() || item.key == String{overflow_key} || contains(keys, item.key)) {
            throw std::invalid_argument("Menu item keys must be nonempty and unique");
        }
        keys.push_back(item.key);
        if (item.kind < MenuItemKind::Item || item.kind > MenuItemKind::SubMenu ||
            (item.kind != MenuItemKind::Group && item.kind != MenuItemKind::SubMenu && !item.children.empty())) {
            throw std::invalid_argument("Menu item topology is invalid");
        }
        validate_items(item.children, keys, depth + 1);
    }
}

MenuThemeToken menu_token(WindowComponentServices& services, runtime::ComponentId id) {
    const auto& state = *services.components().state<MenuState>(id);
    auto token = services.components().theme_scope(id)->snapshot().menu();
    if (state.theme == MenuTheme::Dark) {
        token.colors = token.dark_colors;
    } else if (state.theme == MenuTheme::Light) {
        token.colors = token.light_colors;
    }
    return token;
}

int menu_layer(WindowComponentServices& services, runtime::ComponentId id, std::size_t depth, int offset) {
    const auto priority =
        static_cast<std::int64_t>(services.components().theme_scope(id)->snapshot().dropdown().z_index_popup) +
        static_cast<std::int64_t>(depth) * 3 + offset;
    if (priority > std::numeric_limits<int>::max()) {
        throw std::invalid_argument("Menu flyout z-index exceeds supported range");
    }
    return static_cast<int>(priority);
}

bool collapsed(const MenuState& s) {
    return s.mode == MenuMode::Inline && s.collapsed;
}

String first_grapheme(const String& value) {
    input::TextBoundaryMap boundaries;
    if (value.empty() || !boundaries.assign(value.bytes())) {
        return String{};
    }
    return String::from_utf8(value.bytes().substr(0, boundaries.next(0))).value();
}

bool flyout(const MenuState& s) {
    return s.mode != MenuMode::Inline || s.collapsed;
}

std::optional<std::size_t> row_index(WindowComponentServices&, const MenuState& s, runtime::ComponentId row) {
    const auto found = std::ranges::find(s.rows, row);
    return found == s.rows.end() ? std::nullopt : std::optional{static_cast<std::size_t>(found - s.rows.begin())};
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId ancestor) {
    for (auto current = std::optional{child}; current && services.components().contains(*current);
         current = services.components().parent(*current)) {
        if (*current == ancestor) {
            return true;
        }
    }
    return false;
}

bool row_open(const MenuState& s, const MenuRow& row) {
    return row.overflow ? s.overflow_open : contains(s.open, row.key);
}

MenuItem overflow_item() {
    MenuItem item{overflow_key, String{}, false, false, MenuItemKind::SubMenu};
    item.content = MenuLabel{[] { Icon(IconProps{}.name(IconName::EllipsisOutlined)); }};
    return item;
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
    for (auto id : mounted_) {
        auto* s = services_->components().state<MenuState>(id);
        if (s->hover_row && !services_->components().contains(*s->hover_row)) {
            s->hover_row.reset();
            s->hover_deadline.reset();
        }
    }
}

void MenuComponentHost::validate(const MenuProps& props) {
    if ((props.selected_ && props.default_selected_) || (props.open_ && props.default_open_) ||
        (read_prop(props.mode_) < MenuMode::Vertical || read_prop(props.mode_) > MenuMode::Horizontal)) {
        throw std::invalid_argument("Menu controlled/default state or mode conflicts");
    }
    MenuKeys keys;
    validate_items(props.dynamic_items_ ? read_prop(*props.dynamic_items_) : props.items_, keys, 0);
    validate_keys(props.selected_ ? read_prop(*props.selected_) : props.default_selected_.value_or(MenuKeys{}));
    validate_keys(props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(MenuKeys{}));
}

MenuSnapshot MenuComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<MenuState>(id);
    if (!s) {
        throw std::out_of_range("Menu component is stale");
    }
    MenuSnapshot result{s->rows,
                        s->selected,
                        s->open,
                        s->active ? std::optional{services_->components().state<MenuRow>(s->rows[*s->active])->key}
                                  : std::nullopt,
                        s->interaction,
                        s->scroll,
                        {},
                        s->has_overflow,
                        s->applied_colors};
    for (auto row_id : s->rows) {
        const auto* row = services_->components().state<MenuRow>(row_id);
        if (row->popup && services_->components().branch_active(*row->popup)) {
            result.popups.push_back(services_->components().state<MenuPanel>(*row->popup)->bounds);
        }
    }
    return result;
}

void MenuComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<MenuState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const bool active = services_->components().branch_active(id) && services_->interactions().component_active(id);
    if (!active && s->presented_popup) {
        s->suppressed = true;
        s->overflow_open = false;
    }
    s->input_active = active;
    if (s->disabled || !s->input_active) {
        s->hover_deadline.reset();
        s->hover_row.reset();
    }
    const auto token = menu_token(*services_, id);
    s->applied_colors = token.colors;
    services_->interactions().set_eligible(s->interaction, !s->disabled);
    std::optional<std::size_t> first;
    for (std::size_t i = 0; i < s->rows.size(); ++i) {
        auto* row = services_->components().state<MenuRow>(s->rows[i]);
        bool shown = !row->overflow || s->has_overflow;
        bool disabled = row->disabled || s->disabled;
        row->panel.reset();
        if (row->overflowed) {
            const auto overflow = std::ranges::find_if(
                s->rows, [this](auto r) { return services_->components().state<MenuRow>(r)->overflow; });
            if (overflow != s->rows.end()) {
                row->panel = static_cast<std::size_t>(overflow - s->rows.begin());
                shown &= s->overflow_open && !s->suppressed;
            }
        }
        for (auto parent = row->parent; parent;) {
            const auto* ancestor = services_->components().state<MenuRow>(s->rows[*parent]);
            shown &= ancestor->shown &&
                     (ancestor->kind != MenuItemKind::SubMenu ||
                      (row_open(*s, *ancestor) && !(flyout(*s) && (s->suppressed || ancestor->disabled))));
            if (flyout(*s) && ancestor->kind == MenuItemKind::SubMenu && !row->panel) {
                row->panel = *parent;
            }
            if (flyout(*s) && ancestor->panel && !row->panel) {
                row->panel = ancestor->panel;
            }
            disabled |= ancestor->disabled;
            parent = ancestor->parent;
        }
        if (row->panel) {
            shown &= !s->disabled && window_active_ && !s->suppressed && services_->interactions().component_active(id);
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
        const auto layer =
            row->panel ? std::optional<int>{menu_layer(
                             *services_, id, services_->components().state<MenuRow>(s->rows[*row->panel])->depth, 2)}
                       : std::nullopt;
        if (services_->components().set_window_layer(s->rows[i], layer)) {
            geometry = true;
            services_->mark_scene_structure_dirty();
        }
        if (row->popup) {
            if (services_->components().set_window_layer(*row->popup, menu_layer(*services_, id, row->depth, 1))) {
                services_->mark_scene_structure_dirty();
            }
            const bool visible = flyout(*s) && shown && row_open(*s, *row) && !s->disabled && !s->suppressed &&
                                 window_active_ && services_->interactions().component_active(id);
            if (services_->components().set_branch_active(*row->popup, visible)) {
                geometry = true;
                services_->mark_scene_structure_dirty();
            }
        }
        services_->interactions().set_input_blocked(s->rows[i], disabled || !shown);
        services_->interactions().set_eligible(row->interaction, row->eligible);
        if (row->kind != MenuItemKind::Divider) {
            const bool compact = collapsed(*s) && !row->panel && row->depth == 0;
            services_->components().set_branch_active(row->label_component, !compact);
            if (row->collapsed_label) {
                services_->components().set_branch_active(*row->collapsed_label, compact && !row->icon);
            }
            if (row->arrow) {
                services_->components().set_branch_active(*row->arrow, !compact && !row->overflow);
            }
            if (row->extra) {
                services_->components().set_branch_active(*row->extra, !compact);
            }
        }
        const bool selected = row->kind == MenuItemKind::Item && contains(s->selected, row->key);
        row->foreground.set(foreground(disabled                           ? token.colors.disabled
                                       : row->kind == MenuItemKind::Group ? token.colors.group
                                       : row->danger                      ? token.colors.danger
                                       : selected                         ? token.colors.selected
                                                                          : token.colors.text));
        const auto& font = token.typography;
        const bool compact_font = collapsed(*s) && !row->panel && row->depth == 0;
        row->typography.set({font.font_family, font.font_weight, false,
                             compact_font ? token.metrics.collapsed_icon_size : font.font_size, font.line_height});
        if (row->kind == MenuItemKind::SubMenu) {
            row->arrow_icon.set((row_open(*s, *row) && !flyout(*s)) || (s->mode == MenuMode::Horizontal && !row->panel)
                                    ? IconName::DownOutlined
                                : s->direction == TextDirection::RightToLeft ? IconName::LeftOutlined
                                                                             : IconName::RightOutlined);
        }
    }
    bool reveal = false;
    if (!s->active || *s->active >= s->rows.size() ||
        !services_->components().state<MenuRow>(s->rows[*s->active])->eligible) {
        s->active = first;
        s->reveal = true;
        reveal = true;
    }
    auto dirty = geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::HitTest : runtime::DirtyFlags::Material;
    if (reveal) {
        dirty = dirty | runtime::DirtyFlags::Layout | runtime::DirtyFlags::HitTest;
    }
    services_->dirty().invalidate(s->node, dirty);
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
    if (!row || !row->eligible || row->kind != MenuItemKind::SubMenu || row_open(*s, *row) == open) {
        return;
    }
    s->suppressed = false;
    s->hover_deadline.reset();
    if (row->overflow) {
        s->overflow_open = open;
        refresh(id, true);
        return;
    }
    auto keys = s->open;
    if (flyout(*s) && open) {
        for (auto row_id : s->rows) {
            const auto* other = services_->components().state<MenuRow>(row_id);
            if (other->parent == row->parent && other->kind == MenuItemKind::SubMenu) {
                std::erase(keys, other->key);
            }
        }
    }
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
        toggle(id, index, !row_open(*s, *row));
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
        handlers.target = [this, id, row_id](input::PointerDispatchContext& event) {
            auto* state = services_->components().state<MenuState>(id);
            const auto* row = services_->components().state<MenuRow>(row_id);
            const auto position = state ? row_index(*services_, *state, row_id) : std::nullopt;
            if (!state || !row || !row->eligible || !position) {
                return;
            }
            if (event.kind() == input::PointerEventKind::enter || event.kind() == input::PointerEventKind::move) {
                if (state->active != *position) {
                    state->active = *position;
                    refresh(id);
                }
            }
            if (event.event().button != input::PointerButton::primary) {
                return;
            }
            if (event.kind() == input::PointerEventKind::down) {
                state->active = *position;
                services_->focus().defer_focus(state->interaction, input::FocusModality::pointer);
                static_cast<void>(event.capture_pointer());
                event.stop_propagation();
            }
            if (event.kind() == input::PointerEventKind::up) {
                const auto rect = translated_bounds(*services_, row->node);
                const auto clip = services_->nodes().content_clip(
                    row->node, row->panel ? state->clip : translated_bounds(*services_, state->node));
                const bool hit = event.press_origin() == row->interaction &&
                                 inside(rect, event.event().x, event.event().y) &&
                                 inside(clip, event.event().x, event.event().y);
                static_cast<void>(event.release_pointer_capture());
                event.stop_propagation();
                if (hit) {
                    activate(id, *position);
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
                    if (item.content) {
                        row.rich = true;
                        row.label_component = children.mount_component<int>(0);
                        const auto node = children.root(row.label_component);
                        services.layout().set_layout(node, layout::BoxLayout{});
                        children.on_resource_cleanup(row.label_component,
                                                     [&services, node] { services.layout().remove_layout(node); });
                        children.mount_slot(row.label_component, *item.content);
                    } else {
                        Text(TextProps{}.content(row.label));
                        row.label_component = services.components().children(row_id).back();
                    }
                    if (item.extra) {
                        row.extra = children.mount_component<int>(0);
                        const auto node = children.root(*row.extra);
                        services.layout().set_layout(node, layout::BoxLayout{});
                        children.on_resource_cleanup(*row.extra,
                                                     [&services, node] { services.layout().remove_layout(node); });
                        children.mount_slot(*row.extra, *item.extra);
                    }
                    if (item.kind == MenuItemKind::SubMenu) {
                        Icon(IconProps{}.name(row.arrow_icon));
                        row.arrow = services.components().children(row_id).back();
                    }
                    Text(TextProps{}.content(row.collapsed_text));
                    row.collapsed_label = services.components().children(row_id).back();
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
                    const auto token = menu_token(*services_, id);
                    const bool compact = collapsed(state) && !row.panel && row.depth == 0;
                    if (compact) {
                        if (const auto content = row.icon ? row.icon : row.collapsed_label) {
                            static_cast<void>(
                                engine.measure_child(services_->components().root(*content),
                                                     {0, limits.max_width, 0, token.metrics.item_height}));
                        }
                        return limits.constrain(
                            {std::max(0.0F, token.metrics.collapsed_width - token.metrics.item_margin * 2),
                             row_height(row, token,
                                        state.action_menu ? theme.dropdown().metrics.item_height
                                                          : token.metrics.item_height)});
                    }
                    float width = token.metrics.padding_inline * 2 +
                                  static_cast<float>(flyout(state) ? 0 : row.depth) * token.metrics.inline_indent;
                    float height = row_height(row, token,
                                              state.action_menu ? theme.dropdown().metrics.item_height
                                                                : token.metrics.item_height);
                    for (auto child : services_->components().children(row_id)) {
                        if (child == row.collapsed_label || (row.overflow && child == row.arrow)) {
                            continue;
                        }
                        const auto size =
                            engine.measure_child(services_->components().root(child), {0, limits.max_width, 0, height});
                        width += size.width + (child == row.label_component ? 0 : 8);
                    }
                    return limits.constrain({width, height});
                },
                [this, id, row_id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect allocated) {
                    const auto& row = *services_->components().state<MenuRow>(row_id);
                    const auto& state = *services_->components().state<MenuState>(id);
                    const auto rect = row.panel ? row.full_bounds : allocated;
                    const auto token = menu_token(*services_, id);
                    if (collapsed(state) && !row.panel && row.depth == 0) {
                        if (const auto content = row.icon ? row.icon : row.collapsed_label) {
                            const auto node = services_->components().root(*content);
                            const auto size = engine.measure_child(node, {0, rect.width, 0, rect.height});
                            engine.place_child(node,
                                               {rect.x + (rect.width - size.width) / 2,
                                                rect.y + (rect.height - size.height) / 2, size.width, size.height});
                        }
                        return;
                    }
                    const auto place = [&](runtime::NodeId node, runtime::Rect child) {
                        if (state.direction == TextDirection::RightToLeft) {
                            child.x = rect.x + rect.width - (child.x - rect.x) - child.width;
                        }
                        engine.place_child(node, child);
                    };
                    const float inset = std::min(rect.width / 2, token.metrics.padding_inline +
                                                                     static_cast<float>(flyout(state) ? 0 : row.depth) *
                                                                         token.metrics.inline_indent);
                    float x = rect.x + inset;
                    float end = rect.x + rect.width - std::min(rect.width / 2, token.metrics.padding_inline);
                    if (row.extra && services_->components().branch_active(*row.extra)) {
                        const auto node = services_->components().root(*row.extra);
                        const auto size = engine.measure_child(node, {0, std::max(0.0F, end - x), 0, rect.height});
                        end -= size.width;
                        place(node, {end, rect.y + (rect.height - size.height) / 2, size.width, size.height});
                        end = std::max(x, end - 8);
                    }
                    if (row.arrow && services_->components().branch_active(*row.arrow)) {
                        const auto node = services_->components().root(*row.arrow);
                        const auto size = engine.measure_child(node, {0, std::max(0.0F, end - x), 0, rect.height});
                        end -= size.width;
                        place(node, {end, rect.y + (rect.height - size.height) / 2, size.width, size.height});
                        end = std::max(x, end - 8);
                    }
                    if (row.icon) {
                        const auto node = services_->components().root(*row.icon);
                        const auto size = engine.measure_child(node, {0, std::max(0.0F, end - x), 0, rect.height});
                        place(node, {x, rect.y + (rect.height - size.height) / 2, size.width, size.height});
                        x = std::min(end, x + size.width + 8);
                    }
                    if (row.kind != MenuItemKind::Divider &&
                        services_->components().branch_active(row.label_component)) {
                        const auto node = services_->components().root(row.label_component);
                        const auto size = engine.measure_child(node, {0, std::max(0.0F, end - x), 0, rect.height});
                        place(node, {x, rect.y + (rect.height - size.height) / 2, size.width, size.height});
                    }
                }});
        if (item.kind == MenuItemKind::SubMenu) {
            row.popup = build.mount_component<MenuPanel>();
            auto& panel = build.state<MenuPanel>(*row.popup);
            panel.node = build.root(*row.popup);
            services.components().set_window_layer(*row.popup, menu_layer(services, id, depth, 1));
            services.components().set_branch_active(*row.popup, false);
            panel.interaction =
                services.interactions().create({*row.popup, panel.node, s.interaction, true, false, {}, false, false});
            const auto fragment =
                build.register_scene_fragment(*row.popup, runtime::SceneFragmentPlacement::before_children);
            auto q = quad({}, Color{});
            panel.surface = services.surfaces().create(*row.popup, panel.node, fragment, panel.interaction,
                                                       std::span{&q, 1}, effects);
            services.layout().set_layout(panel.node, layout::BoxLayout{});
            build.on_resource_cleanup(*row.popup,
                                      [&services, node = panel.node, hit = panel.interaction, surface = panel.surface] {
                                          services.pointer().cancel_interaction(hit);
                                          services.interactions().remove(hit);
                                          services.surfaces().destroy(surface);
                                          services.layout().remove_layout(node);
                                      });
        }
        bind_row(id, row_id, item);
        mount_rows(id, item.children, index, depth + (item.kind == MenuItemKind::SubMenu ? 1 : 0));
    }
}

void MenuComponentHost::bind_row(runtime::ComponentId id, runtime::ComponentId row_id, const MenuItem& item) {
    auto* row = services_->components().state<MenuRow>(row_id);
    row->bindings = std::make_unique<Scope>();
    connect_prop(*row->bindings, item.label, [this, id, row_id](String value) {
        if (auto* row = services_->components().state<MenuRow>(row_id)) {
            row->collapsed_text.set(first_grapheme(value));
            row->label.set(std::move(value));
            if (!services_->components().state<MenuState>(id)->reconciling) {
                refresh(id, true);
            }
        }
    });
    const auto flag = [&](const Prop<bool>& prop, bool MenuRow::* field) {
        connect_prop(*row->bindings, prop, [this, id, row_id, field](bool value) {
            if (auto* row = services_->components().state<MenuRow>(row_id)) {
                row->*field = value;
                if (!services_->components().state<MenuState>(id)->reconciling) {
                    refresh(id);
                }
            }
        });
    };
    flag(item.disabled, &MenuRow::disabled);
    flag(item.danger, &MenuRow::danger);
}

void MenuComponentHost::reconcile(runtime::ComponentId id, const std::vector<MenuItem>& items) {
    auto* s = services_->components().state<MenuState>(id);
    if (!s) {
        return;
    }
    MenuKeys keys;
    validate_items(items, keys, 0);
    if (contains(keys, String{overflow_key})) {
        throw std::invalid_argument("reserved Menu key");
    }
    const auto previous = s->rows;
    const auto active = s->active && *s->active < previous.size()
                            ? std::optional{services_->components().state<MenuRow>(previous[*s->active])->key}
                            : std::nullopt;
    const auto active_index = s->active.value_or(0);
    std::vector<runtime::ComponentId> next;

    struct PendingRow final {
        runtime::ComponentId id;
        const MenuItem* item;
        std::optional<std::size_t> parent;
        std::size_t depth;
    };

    std::vector<PendingRow> pending;
    s->reconciling = true;
    try {
        const auto visit = [&](const auto& self, const std::vector<MenuItem>& list, std::optional<std::size_t> parent,
                               std::size_t depth) -> void {
            for (const auto& item : list) {
                const auto old = std::ranges::find_if(previous, [&](auto row_id) {
                    const auto* row = services_->components().state<MenuRow>(row_id);
                    return row->key == item.key && row->kind == item.kind &&
                           row->icon.has_value() == item.icon.has_value() && row->rich == item.content.has_value() &&
                           row->extra.has_value() == item.extra.has_value();
                });
                runtime::ComponentId row_id;
                if (old != previous.end()) {
                    row_id = *old;
                } else {
                    auto flat = item;
                    flat.children.clear();
                    services_->append_slot(id, Content{[&] { mount_rows(id, {flat}, {}, 0); }});
                    row_id = s->rows.back();
                }
                pending.push_back({row_id, &item, parent, depth});
                const auto index = next.size();
                next.push_back(row_id);
                self(self, item.children, index, depth + (item.kind == MenuItemKind::SubMenu ? 1 : 0));
            }
        };
        visit(visit, items, {}, 0);
        for (const auto& update : pending) {
            auto* row = services_->components().state<MenuRow>(update.id);
            row->parent = update.parent;
            row->depth = update.depth;
            row->overflowed = false;
            bind_row(id, update.id, *update.item);
        }
    } catch (...) {
        const auto partial = s->rows;
        s->rows = previous;
        s->reconciling = false;
        for (auto row_id : partial) {
            if (std::ranges::find(previous, row_id) == previous.end() && services_->components().contains(row_id)) {
                const auto* row = services_->components().state<MenuRow>(row_id);
                if (row->popup) {
                    services_->destroy(*row->popup);
                }
                services_->destroy(row_id);
            }
        }
        refresh(id, true);
        throw;
    }
    for (auto row_id : previous) {
        const auto* row = services_->components().state<MenuRow>(row_id);
        if (row->overflow) {
            next.push_back(row_id);
        }
    }
    s->rows = next;
    s->active.reset();
    for (std::size_t i = 0; i < next.size(); ++i) {
        if (active && services_->components().state<MenuRow>(next[i])->key == *active) {
            s->active = i;
        }
    }
    if (!s->active && !next.empty()) {
        s->active = std::min(active_index, next.size() - 1);
    }
    if (!s->controlled_selected) {
        std::erase_if(s->selected, [&](const auto& k) { return !contains(keys, k); });
    }
    if (!s->controlled_open) {
        std::erase_if(s->open, [&](const auto& k) { return !contains(keys, k); });
    }
    s->reconciling = false;
    for (auto row_id : previous) {
        if (std::ranges::find(next, row_id) == next.end()) {
            const auto* row = services_->components().state<MenuRow>(row_id);
            if (row->popup) {
                services_->destroy(*row->popup);
            }
            services_->destroy(row_id);
        }
    }
    s->reveal = true;
    refresh(id, true);
}

void MenuComponentHost::mount(const MenuProps& props, bool action_menu) {
    validate(props);
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<MenuState>();
    auto& s = build.state<MenuState>(id);
    s.node = build.root(id);
    s.action_menu = action_menu;
    s.mode = read_prop(props.mode_);
    s.theme = read_prop(props.theme_);
    s.direction = read_prop(props.direction_);
    s.collapsed = read_prop(props.collapsed_);
    s.submenu_trigger = read_prop(props.submenu_trigger_);
    s.open_delay = read_prop(props.open_delay_);
    s.close_delay = read_prop(props.close_delay_);
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
    build.mount_slot(id, Content{[&] {
                         mount_rows(id, props.dynamic_items_ ? read_prop(*props.dynamic_items_) : props.items_, {}, 0);
                         if (s.mode == MenuMode::Horizontal) {
                             mount_rows(id, {overflow_item()}, {}, 0);
                             services.components().state<MenuRow>(s.rows.back())->overflow = true;
                         }
                     }});
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                auto& state = *services_->components().state<MenuState>(id);
                const auto token = menu_token(*services_, id);
                const float margin = token.metrics.item_margin;
                float width = 0;
                float height = margin;
                for (auto row_id : state.rows) {
                    const auto* row = services_->components().state<MenuRow>(row_id);
                    if (state.mode == MenuMode::Horizontal) {
                        if (row->depth > 0) {
                            continue;
                        }
                    } else if (!row->shown || row->panel || row->overflow) {
                        continue;
                    }
                    const auto size = engine.measure_child(
                        row->node, {0,
                                    state.mode == MenuMode::Horizontal ? std::numeric_limits<float>::infinity()
                                                                       : std::max(0.0F, limits.max_width - margin * 2),
                                    0, std::numeric_limits<float>::infinity()});
                    if (state.mode == MenuMode::Horizontal) {
                        if (!row->overflow) {
                            width += size.width + margin;
                        }
                        height = std::max(height, size.height + margin * 2);
                    } else {
                        width = std::max(width, size.width);
                        height += size.height + margin;
                    }
                }
                state.content_height = height;
                return limits.constrain(
                    {collapsed(state) ? token.metrics.collapsed_width : width + margin * 2, height});
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                auto& state = *services_->components().state<MenuState>(id);
                const auto& theme = services_->components().theme_scope(id)->snapshot();
                const auto token = menu_token(*services_, id);
                const float margin = std::min(token.metrics.item_margin, rect.width / 2);
                if (state.mode == MenuMode::Horizontal) {
                    float total = margin;
                    float overflow_width = 0;
                    for (auto row_id : state.rows) {
                        const auto* row = services_->components().state<MenuRow>(row_id);
                        if (row->depth > 0) {
                            continue;
                        }
                        const auto size = engine.measure_child(
                            row->node, {0, std::numeric_limits<float>::infinity(), 0, rect.height});
                        if (row->overflow) {
                            overflow_width = size.width + margin;
                        } else {
                            total += size.width + margin;
                        }
                    }
                    const bool overflow = total > rect.width;
                    float x = margin;
                    bool tail = false;
                    bool changed = state.has_overflow != overflow;
                    for (auto row_id : state.rows) {
                        auto* row = services_->components().state<MenuRow>(row_id);
                        if (row->depth > 0) {
                            continue;
                        }
                        const auto size = engine.measure_child(
                            row->node, {0, std::numeric_limits<float>::infinity(), 0, rect.height});
                        if (row->overflow) {
                            if (!overflow) {
                                continue;
                            }
                        } else {
                            tail |= overflow && x + size.width + margin > rect.width - overflow_width;
                            changed |= row->overflowed != tail;
                            row->overflowed = tail;
                            if (tail) {
                                continue;
                            }
                        }
                        const float placed_x = state.direction == TextDirection::RightToLeft
                                                   ? rect.x + rect.width - x - size.width
                                                   : rect.x + x;
                        engine.place_child(row->node, {placed_x, rect.y + margin, size.width,
                                                       std::max(0.0F, rect.height - margin * 2)});
                        x += size.width + margin;
                    }
                    state.has_overflow = overflow;
                    if (!overflow) {
                        state.overflow_open = false;
                    }
                    if (changed) {
                        refresh(id);
                    }
                    return;
                }
                state.has_overflow = false;
                state.scroll = std::clamp(state.scroll, 0.0F, std::max(0.0F, state.content_height - rect.height));
                if (state.reveal && state.active) {
                    float top = margin;
                    for (std::size_t i = 0; i < state.rows.size(); ++i) {
                        const auto& row = *services_->components().state<MenuRow>(state.rows[i]);
                        if (!row.shown || row.panel || row.overflow) {
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
                    if (!row.shown || row.panel || row.overflow) {
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
    if (props.dynamic_items_) {
        connect_prop(
            scope, *props.dynamic_items_, [this, id](std::vector<MenuItem> items) { reconcile(id, items); },
            [](const auto&, const auto&) { return false; });
    }
    connect_prop(scope, props.mode_, [this, id](MenuMode value) {
        if (value < MenuMode::Vertical || value > MenuMode::Horizontal) {
            throw std::invalid_argument("invalid Menu mode");
        }
        auto* s = services_->components().state<MenuState>(id);
        if (s && s->mode != value) {
            s->mode = value;
            if (value == MenuMode::Horizontal && !std::ranges::any_of(s->rows, [this](auto row) {
                    return services_->components().state<MenuRow>(row)->overflow;
                })) {
                services_->append_slot(id, Content{[&] {
                                           mount_rows(id, {overflow_item()}, {}, 0);
                                           services_->components().state<MenuRow>(s->rows.back())->overflow = true;
                                       }});
            }
            for (auto row : s->rows) {
                services_->components().state<MenuRow>(row)->overflowed = false;
            }
            s->has_overflow = false;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.theme_, [this, id](MenuTheme value) {
        if (value < MenuTheme::Inherit || value > MenuTheme::Dark) {
            throw std::invalid_argument("invalid Menu theme");
        }
        if (auto* s = services_->components().state<MenuState>(id)) {
            s->theme = value;
            refresh(id);
        }
    });
    connect_prop(scope, props.direction_, [this, id](TextDirection value) {
        if (value < TextDirection::Auto || value > TextDirection::RightToLeft) {
            throw std::invalid_argument("invalid Menu direction");
        }
        if (auto* s = services_->components().state<MenuState>(id)) {
            s->direction = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.collapsed_, [this, id](bool value) {
        if (auto* s = services_->components().state<MenuState>(id)) {
            s->collapsed = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.submenu_trigger_, [this, id](MenuSubMenuTrigger value) {
        if (value < MenuSubMenuTrigger::Hover || value > MenuSubMenuTrigger::Click) {
            throw std::invalid_argument("invalid Menu submenu trigger");
        }
        if (auto* s = services_->components().state<MenuState>(id)) {
            s->submenu_trigger = value;
        }
    });
    const auto delay = [&](const Prop<float>& prop, float MenuState::* field) {
        connect_prop(scope, prop, [this, id, field](float value) {
            if (!std::isfinite(value) || value < 0 || value > 3600) {
                throw std::invalid_argument("invalid Menu hover delay");
            }
            if (auto* s = services_->components().state<MenuState>(id)) {
                s->*field = value;
            }
        });
    };
    delay(props.open_delay_, &MenuState::open_delay);
    delay(props.close_delay_, &MenuState::close_delay);
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
                s->suppressed = false;
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
    s.subscriptions[4] = theme->capture([this, id](theme_runtime::DirtyPhase) { update_theme(id, true); },
                                        [theme] { static_cast<void>(theme->dropdown_order()); });
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
        if (event.key == Key::escape && close_deepest(id)) {
            return true;
        }
        const auto* active_row = s->active ? services_->components().state<MenuRow>(s->rows[*s->active]) : nullptr;
        const auto panel = active_row ? active_row->panel : std::nullopt;
        const bool horizontal = s->mode == MenuMode::Horizontal && !panel;
        const bool forward =
            s->direction == TextDirection::RightToLeft ? event.key == Key::left : event.key == Key::right;
        const bool back = s->direction == TextDirection::RightToLeft ? event.key == Key::right : event.key == Key::left;
        if (event.key == Key::home || event.key == Key::end ||
            (horizontal ? (forward || back) : (event.key == Key::down || event.key == Key::up))) {
            std::vector<std::size_t> eligible;
            for (std::size_t i = 0; i < s->rows.size(); ++i) {
                const auto* candidate = services_->components().state<MenuRow>(s->rows[i]);
                if (candidate->eligible && (!flyout(*s) || candidate->panel == panel)) {
                    eligible.push_back(i);
                }
            }
            if (eligible.empty()) {
                return true;
            }
            auto found = s->active ? std::ranges::find(eligible, *s->active) : eligible.end();
            auto pos = found == eligible.end() ? (event.key == Key::down ? eligible.size() - 1 : 0)
                                               : static_cast<std::size_t>(found - eligible.begin());
            pos = event.key == Key::home ? 0
                  : event.key == Key::end
                      ? eligible.size() - 1
                      : (pos + ((horizontal ? forward : event.key == Key::down) ? 1 : eligible.size() - 1)) %
                            eligible.size();
            s->active = eligible[pos];
        } else if ((forward || back || (horizontal && (event.key == Key::down || event.key == Key::up))) && s->active) {
            const auto index = *s->active;
            const auto* row = services_->components().state<MenuRow>(s->rows[index]);
            if (row->kind == MenuItemKind::SubMenu &&
                (((forward || horizontal) && !row_open(*s, *row)) || (back && !horizontal && row_open(*s, *row)))) {
                const auto row_id = s->rows[index];
                toggle(id, index, forward || horizontal);
                s = services_->components().state<MenuState>(id);
                if (!s || !row_index(*services_, *s, row_id)) {
                    return true;
                }
                if (flyout(*s) && (forward || horizontal)) {
                    for (std::size_t i = 0; i < s->rows.size(); ++i) {
                        const auto* child = services_->components().state<MenuRow>(s->rows[i]);
                        if (child->eligible && child->panel == row_index(*services_, *s, row_id)) {
                            s->active = i;
                            break;
                        }
                    }
                }
            } else if (back) {
                if (flyout(*s) && panel) {
                    s->active = panel;
                    toggle(id, *panel, false);
                    return true;
                }
                for (auto parent = row->parent; parent;) {
                    const auto* ancestor = services_->components().state<MenuRow>(s->rows[*parent]);
                    if (ancestor->kind == MenuItemKind::SubMenu) {
                        s->active = parent;
                        break;
                    }
                    parent = ancestor->parent;
                }
            } else if (row->kind == MenuItemKind::SubMenu && row_open(*s, *row)) {
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

bool MenuComponentHost::close_deepest(runtime::ComponentId id) {
    auto* s = services_->components().state<MenuState>(id);
    if (!s || !flyout(*s)) {
        return false;
    }
    std::optional<std::size_t> deepest;
    for (std::size_t i = 0; i < s->rows.size(); ++i) {
        const auto* row = services_->components().state<MenuRow>(s->rows[i]);
        if (row->popup && services_->components().branch_active(*row->popup) &&
            (!deepest || row->depth >= services_->components().state<MenuRow>(s->rows[*deepest])->depth)) {
            deepest = i;
        }
    }
    if (!deepest) {
        return false;
    }
    s->active = deepest;
    toggle(id, *deepest, false);
    return true;
}

void MenuComponentHost::on_window_active(bool active) {
    window_active_ = active;
    if (!active) {
        for (auto id : mounted_) {
            if (auto* s = services_->components().state<MenuState>(id)) {
                s->hover_deadline.reset();
                s->suppressed = true;
                s->overflow_open = false;
                refresh(id, true);
            }
        }
    }
}

void MenuComponentHost::on_input_boundary_changed() {
    for (auto id : mounted_) {
        if (auto* s = services_->components().state<MenuState>(id);
            s && !services_->interactions().component_active(id)) {
            s->hover_deadline.reset();
            if (s->presented_popup) {
                s->suppressed = true;
                s->overflow_open = false;
            }
            refresh(id, true);
        }
    }
}

void MenuComponentHost::on_pointer_input(const input::PointerInputEvent& event, std::optional<input::InteractionId> hit,
                                         std::optional<input::InteractionId>) {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<MenuState>(id);
        if (!s || !flyout(*s) || s->disabled || !services_->interactions().component_active(id)) {
            continue;
        }
        const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
        const bool owned = target && descendant(*services_, target->component, id);
        if (event.action == input::PointerAction::cancel) {
            s->hover_row.reset();
            s->hover_deadline.reset();
            continue;
        }
        if (event.action == input::PointerAction::move) {
            if (s->submenu_trigger != MenuSubMenuTrigger::Hover ||
                event.pointer.device != input::PointerDevice::mouse) {
                continue;
            }
            std::optional<runtime::ComponentId> hovered;
            if (owned) {
                for (auto current = std::optional{target->component}; current && *current != id;
                     current = services_->components().parent(*current)) {
                    const auto* row = services_->components().state<MenuRow>(*current);
                    if (row && row->eligible && row->kind == MenuItemKind::SubMenu) {
                        hovered = current;
                        break;
                    }
                }
            }
            bool within_popup = false;
            if (owned && !hovered) {
                for (auto current = std::optional{target->component}; current && *current != id;
                     current = services_->components().parent(*current)) {
                    const auto* row = services_->components().state<MenuRow>(*current);
                    within_popup |= services_->components().state<MenuPanel>(*current) != nullptr ||
                                    (row && row->panel.has_value());
                }
                if (within_popup) {
                    s->hover_deadline.reset();
                    s->hover_row.reset();
                    continue;
                }
            }
            if (hovered != s->hover_row || !s->hover_deadline) {
                s->hover_row = hovered;
                const bool needs_open = hovered && !row_open(*s, *services_->components().state<MenuRow>(*hovered));
                const bool needs_close = !hovered && !within_popup && (!s->open.empty() || s->overflow_open);
                if (needs_open || needs_close) {
                    s->hover_deadline = s->hover_clock.observe(services_->animation_time()).effective +
                                        animation::AnimationDuration::milliseconds(
                                            (needs_open ? s->open_delay : s->close_delay) * 1000);
                    services_->request_frame();
                } else {
                    s->hover_deadline.reset();
                }
            }
            continue;
        }
        if (event.action != input::PointerAction::down || owned || (s->open.empty() && !s->overflow_open)) {
            continue;
        }
        s->hover_deadline.reset();
        const auto callback = s->on_open;
        const bool notify = !s->open.empty();
        s->overflow_open = false;
        if (!s->controlled_open) {
            s->open.clear();
        }
        refresh(id, true);
        if (notify && callback) {
            callback({});
        }
    }
}

std::size_t MenuComponentHost::tick_auxiliary(animation::AnimationTime time) {
    std::size_t changed = 0;
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<MenuState>(id);
        if (!s || !s->hover_deadline) {
            continue;
        }
        if (!window_active_ || s->disabled || !services_->interactions().component_active(id)) {
            s->hover_deadline.reset();
            continue;
        }
        const auto now = s->hover_clock.observe(time).effective;
        if (now < *s->hover_deadline) {
            continue;
        }
        const auto hovered = s->hover_row;
        s->hover_deadline.reset();
        if (hovered && services_->components().contains(*hovered)) {
            const auto index = row_index(*services_, *s, *hovered);
            if (index) {
                toggle(id, *index, true);
                ++changed;
            }
        } else if (!hovered) {
            const auto callback = s->on_open;
            const bool notify = !s->open.empty();
            s->overflow_open = false;
            if (!s->controlled_open) {
                s->open.clear();
            }
            refresh(id, true);
            if (notify && callback) {
                callback({});
            }
            ++changed;
        }
    }
    return changed;
}

std::optional<animation::AnimationTime> MenuComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> next;
    for (auto id : mounted_) {
        const auto* s = services_->components().state<MenuState>(id);
        if (s && s->hover_deadline && (!next || *s->hover_deadline < *next)) {
            next = s->hover_deadline;
        }
    }
    return next;
}

void MenuComponentHost::position_window_layers(runtime::Size, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<MenuState>(id);
        if (!s) {
            continue;
        }
        const bool active = services_->components().branch_active(id) && services_->interactions().component_active(id);
        if (active != s->input_active) {
            if (!active && s->presented_popup) {
                s->suppressed = true;
                s->overflow_open = false;
            }
            refresh(id, true);
        }
        if (!active) {
            continue;
        }
        s->clip = clip;
        if (!flyout(*s)) {
            continue;
        }
        const auto token = menu_token(*services_, id);
        const auto& dropdown = services_->components().theme_scope(id)->snapshot().dropdown();
        auto& engine = services_->layout();
        std::vector<bool> positioned(s->rows.size());
        const auto position = [&](const auto& self, std::size_t index) -> void {
            if (positioned[index]) {
                return;
            }
            positioned[index] = true;
            auto* row = services_->components().state<MenuRow>(s->rows[index]);
            if (!row->popup || !services_->components().branch_active(*row->popup)) {
                return;
            }
            s->presented_popup = true;
            if (row->panel) {
                self(self, *row->panel);
            }
            auto* panel = services_->components().state<MenuPanel>(*row->popup);
            const float margin = token.metrics.item_margin;
            float width = std::min(dropdown.metrics.min_width, clip.width);
            float height = margin;
            for (auto child_id : s->rows) {
                auto* child = services_->components().state<MenuRow>(child_id);
                if (child->panel != index || !child->shown) {
                    continue;
                }
                const auto size = engine.measure_child(child->node, {0, std::max(0.0F, clip.width - margin * 2), 0,
                                                                     std::numeric_limits<float>::infinity()});
                width = std::max(width, size.width + margin * 2);
                height += size.height + margin;
            }
            panel->content_height = height;
            const float max_height = std::max(0.0F, std::min(clip.height, dropdown.metrics.max_height));
            height = std::min(height, max_height);
            const auto anchor = translated_bounds(*services_, row->node);
            const bool below = (s->mode == MenuMode::Horizontal && !row->panel) || row->overflow;
            const auto placement = below ? (s->direction == TextDirection::RightToLeft ? TooltipPlacement::BottomRight
                                                                                       : TooltipPlacement::BottomLeft)
                                         : (s->direction == TextDirection::RightToLeft ? TooltipPlacement::LeftTop
                                                                                       : TooltipPlacement::RightTop);
            const auto placed = position_tooltip(anchor, {std::min(width, clip.width), height}, clip, placement,
                                                 dropdown.metrics.gap, true);
            panel->bounds = placed.bounds;
            static_cast<void>(engine.measure_child(
                panel->node, layout::Constraints::fixed(placed.bounds.width, placed.bounds.height)));
            engine.place_child(panel->node, placed.bounds);
            panel->scroll = std::clamp(panel->scroll, 0.0F, std::max(0.0F, panel->content_height - height));
            if (s->reveal && s->active) {
                float top = margin;
                for (std::size_t i = 0; i < s->rows.size(); ++i) {
                    const auto* child = services_->components().state<MenuRow>(s->rows[i]);
                    if (child->panel != index || !child->shown) {
                        continue;
                    }
                    const float h = row_height(
                        *child, token, s->action_menu ? dropdown.metrics.item_height : token.metrics.item_height);
                    if (s->active == i) {
                        if (top < panel->scroll) {
                            panel->scroll = top;
                        }
                        if (top + h > panel->scroll + height) {
                            panel->scroll = std::max(0.0F, top + h - height);
                        }
                        break;
                    }
                    top += h + margin;
                }
            }
            float y = placed.bounds.y + margin - panel->scroll;
            for (auto child_id : s->rows) {
                auto* child = services_->components().state<MenuRow>(child_id);
                if (child->panel != index || !child->shown) {
                    continue;
                }
                const float h = row_height(*child, token,
                                           s->action_menu ? dropdown.metrics.item_height : token.metrics.item_height);
                child->full_bounds = {placed.bounds.x + margin, y, std::max(0.0F, placed.bounds.width - margin * 2), h};
                static_cast<void>(
                    engine.measure_child(child->node, layout::Constraints::fixed(child->full_bounds.width, h)));
                engine.place_child(child->node, intersect(child->full_bounds, placed.bounds));
                y += h + margin;
            }
        };
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            position(position, i);
        }
        s->reveal = false;
    }
}

bool MenuComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    if (mounted_.empty()) {
        return false;
    }
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    if (!target) {
        return false;
    }
    // Registration order is unrelated to popup ownership. Only the closest Menu
    // in the topmost hit's layer may scroll; a child popup belongs to its own host.
    for (auto current = std::optional{target->component}; current; current = services_->components().parent(*current)) {
        auto* s = services_->components().state<MenuState>(*current);
        if (!s) {
            if (services_->components().is_window_layer_root(*current) &&
                !services_->components().state<MenuRow>(*current) &&
                !services_->components().state<MenuPanel>(*current)) {
                return false;
            }
            continue;
        }
        if (!services_->interactions().component_active(*current)) {
            return false;
        }
        MenuPanel* panel = nullptr;
        for (auto child = std::optional{target->component}; child && *child != *current;
             child = services_->components().parent(*child)) {
            if (auto* candidate = services_->components().state<MenuPanel>(*child)) {
                panel = candidate;
                break;
            }
            if (const auto* row = services_->components().state<MenuRow>(*child); row && row->panel) {
                const auto* owner = services_->components().state<MenuRow>(s->rows[*row->panel]);
                panel = services_->components().state<MenuPanel>(*owner->popup);
                break;
            }
        }
        if (panel) {
            const float next = std::clamp(panel->scroll - event.delta_y * 96, 0.0F,
                                          std::max(0.0F, panel->content_height - panel->bounds.height));
            if (next != panel->scroll) {
                panel->scroll = next;
                s->reveal = false;
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::HitTest);
            }
            return true;
        }
        const auto rect = translated_bounds(*services_, s->node);
        const auto clip = services_->nodes().content_clip(s->node, rect);
        if (!inside(clip, event.x, event.y)) {
            return false;
        }
        const float next =
            std::clamp(s->scroll - event.delta_y * 96, 0.0F, std::max(0.0F, s->content_height - rect.height));
        if (next != s->scroll) {
            s->scroll = next;
            s->reveal = false;
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::HitTest);
            return true;
        }
        return false;
    }
    return false;
}

void MenuComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<MenuState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto token = menu_token(*services_, id);
        const auto rect = translated_bounds(*services_, s->node);
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
        auto background = quad(intersect(rect, clip), token.colors.background);
        if (s->action_menu) {
            background.opacity = 0;
        }
        static_cast<void>(services_->surfaces().update_surface(s->background, std::span{&background, 1}));
        for (auto row_id : s->rows) {
            const auto* row = services_->components().state<MenuRow>(row_id);
            if (!row->popup || !services_->components().branch_active(*row->popup)) {
                continue;
            }
            const auto* panel = services_->components().state<MenuPanel>(*row->popup);
            auto q = quad(panel->bounds, token.colors.background, token.metrics.border_radius);
            static_cast<void>(services_->surfaces().update_surface(panel->surface, std::span{&q, 1}));
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = false;
            effects.shape = {{panel->bounds.x, panel->bounds.y, panel->bounds.width, panel->bounds.height},
                             token.metrics.border_radius};
            effects.shadows = services_->components().theme_scope(id)->snapshot().dropdown().shadow;
            effects.ancestor_clip = graphics::EffectClip{1, {0, 0, viewport.width, viewport.height}};
            static_cast<void>(services_->surfaces().update_effects(panel->surface, effects));
        }
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            const auto* row = services_->components().state<MenuRow>(s->rows[i]);
            if (!row->shown) {
                continue;
            }
            const auto row_clip =
                row->panel && services_->components().state<MenuRow>(s->rows[*row->panel])->popup
                    ? services_->components()
                          .state<MenuPanel>(*services_->components().state<MenuRow>(s->rows[*row->panel])->popup)
                          ->bounds
                    : clip;
            auto bounds = translated_bounds(*services_, row->node);
            const bool selected = row->kind == MenuItemKind::Item && contains(s->selected, row->key);
            const bool active = s->active == i && row->eligible;
            if (row->kind == MenuItemKind::Divider) {
                bounds.y += bounds.height / 2;
                bounds.height = 1;
            }
            auto q = quad(intersect(bounds, row_clip),
                          row->kind == MenuItemKind::Divider ? token.colors.divider
                          : selected                         ? token.colors.selected_background
                                                             : token.colors.hover_background,
                          token.metrics.border_radius);
            q.opacity = row->kind == MenuItemKind::Divider || selected || active ? 1.0F : 0.0F;
            static_cast<void>(services_->surfaces().update_surface(row->surface, std::span{&q, 1}));
            services_->scene_composer().set_fragment(row->hit, {}, row->interaction, row_clip);
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = active && s->focus_visible;
            effects.focus_color = token.colors.focus;
            effects.focus_width = 2;
            effects.focus_offset = 0;
            effects.focus_opacity = active && s->focus_visible ? 1.0F : 0.0F;
            effects.shape = {{bounds.x, bounds.y, bounds.width, bounds.height}, token.metrics.border_radius};
            effects.ancestor_clip = graphics::EffectClip{1, row_clip};
            static_cast<void>(services_->surfaces().update_effects(row->surface, effects));
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Menu(MenuProps props) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_menus) {
        throw std::logic_error("Menu requires window component services");
    }
    detail::active_menus->mount(props);
}
} // namespace ryn
