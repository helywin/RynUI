#include "component/config_defaults.hpp"
#include "component/breadcrumb_component.hpp"
#include "component/navigation_model.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <exception>
#include <limits>

namespace ryn::detail {
namespace {
thread_local BreadcrumbComponentHost* active_breadcrumb{};

struct BreadcrumbRow final {
    std::optional<BreadcrumbItem> descriptor;
    std::shared_ptr<std::size_t> index{std::make_shared<std::size_t>(0)};
    std::shared_ptr<Scope> bindings;
    runtime::NodeId content_node;
    std::optional<runtime::ComponentId> dropdown;
    Signal<bool> menu_disabled{false};
    runtime::ComponentId component;
    runtime::ComponentId separator;
    runtime::NodeId node;
    runtime::NodeId title_node;
    std::optional<runtime::NodeId> icon_node;
    std::optional<runtime::NodeId> menu_icon_node;
    runtime::Size menu_icon_size;
    input::InteractionId interaction;
    component::RetainedSurfaceId surface;
    Signal<String> title{String{}};
    Signal<runtime::SemanticForeground> color{{0, 0, 0, 1}};
    runtime::Size title_size;
    runtime::Size icon_size;
    runtime::Size natural;
    runtime::Size separator_size;
};

struct BreadcrumbState final {
    runtime::NodeId node;
    BreadcrumbProps props;
    std::vector<BreadcrumbItem> pending;
    bool dirty_items{};
    std::uint64_t items_revision{};
    bool updating{};
    bool wrap{true};
    bool rtl{};
    BreadcrumbModel model;
    std::vector<BreadcrumbRow> rows;
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> icon_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticForeground> separator_color{{0, 0, 0, 1}};
    std::array<theme_runtime::Subscription, 3> subscriptions;
    std::optional<std::size_t> hovered;
    std::optional<std::size_t> pressed;
    runtime::Rect clip;
    std::function<void(BreadcrumbActivation)> callback;
};

runtime::SemanticForeground fg(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

runtime::Rect translated(const runtime::Node& n) {
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

bool inside(runtime::Rect b, float x, float y) {
    return x >= b.x && y >= b.y && x < b.x + b.width && y < b.y + b.height;
}

runtime::NodeId slot(WindowComponentServices& services, runtime::ComponentBuildContext& build, const Content& content,
                     const Signal<runtime::SemanticForeground>* color = nullptr,
                     const Signal<runtime::SemanticTypography>* font = nullptr) {
    const auto component = build.mount_component<int>(0);
    const auto node = build.root(component);
    services.nodes().require(node).clip_content = true;
    services.layout().set_layout(node, layout::BoxLayout{});
    build.on_resource_cleanup(component, [&services, node] { services.layout().remove_layout(node); });
    if (color && font) {
        build.mount_slot_with_semantic_text_style(component, content, Prop<runtime::SemanticForeground>{*color},
                                                  Prop<runtime::SemanticTypography>{*font});
    } else {
        build.mount_slot(component, content);
    }
    return node;
}
} // namespace

BreadcrumbComponentHost::BreadcrumbComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

BreadcrumbComponentHost::~BreadcrumbComponentHost() {
    services_->detach(*this);
}

void* BreadcrumbComponentHost::begin_mount() noexcept {
    auto* old = active_breadcrumb;
    active_breadcrumb = this;
    return old;
}

void BreadcrumbComponentHost::end_mount(void* old) noexcept {
    active_breadcrumb = static_cast<BreadcrumbComponentHost*>(old);
}

void BreadcrumbComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

BreadcrumbSnapshot BreadcrumbComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<BreadcrumbState>(id);
    if (!s) {
        throw std::out_of_range("Breadcrumb component is stale");
    }
    BreadcrumbSnapshot result{s->model.current(), {}, {}};
    for (const auto& row : s->rows) {
        result.items.push_back(row.component);
        result.separators.push_back(row.separator);
    }
    return result;
}

void BreadcrumbComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<BreadcrumbState>(id);
    if (!s || s->updating || !services_->components().scope(id).active()) {
        return;
    }
    const auto t = services_->components().theme_scope(id)->snapshot().breadcrumb();
    const auto& m = t.metrics;
    s->font.set({m.font_family, 400, false, m.font_size, m.line_height});
    s->icon_font.set({m.font_family, 400, false, m.icon_size, m.line_height});
    s->separator_color.set(fg(t.colors.separator));
    const auto current = s->model.current();
    const bool shown = current.has_value();
    auto& root = services_->nodes().require(s->node);
    if (root.out_of_flow == shown) {
        root.out_of_flow = !shown;
        geometry = true;
    }
    if (services_->components().set_branch_active(id, shown)) {
        services_->mark_scene_structure_dirty();
        geometry = true;
    }
    std::vector<std::pair<runtime::ComponentId, bool>> branches;
    for (std::size_t i = 0; i < s->rows.size(); ++i) {
        auto& row = s->rows[i];
        const bool visible = s->model.items[i].visible;
        for (auto [component, active] :
             {std::pair{row.component, visible}, std::pair{row.separator, visible && current != i}}) {
            if (services_->components().set_branch_active(component, active)) {
                services_->mark_scene_structure_dirty();
                geometry = true;
            }
            services_->nodes().require(services_->components().root(component)).out_of_flow = !active;
        }
        const bool eligible = (s->callback || row.descriptor->on_activate_) && s->model.eligible(i) && !row.dropdown;
        // Descendant interactions inherit this eligibility; navigation availability only controls this row's focus.
        if (services_->interactions().set_eligible(row.interaction, visible && !s->model.items[i].disabled)) {
            services_->dirty().invalidate(row.node, runtime::DirtyFlags::HitTest);
        }
        services_->interactions().set_focusable(row.interaction, eligible);
        services_->interactions().set_tab_stop(row.interaction, eligible);
        row.menu_disabled.set(!visible || s->model.items[i].disabled);
        if (!services_->components().contains(id)) {
            return;
        }
        row.color.set(fg(s->model.items[i].disabled    ? t.colors.disabled
                         : current == i                ? t.colors.current
                         : s->hovered == i && eligible ? t.colors.hover
                         : s->callback                 ? t.colors.link
                                                       : t.colors.item));
        branches.emplace_back(row.component,
                              !visible || s->model.items[i].disabled || !services_->focus().state().window_active);
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
    // Boundary changes can invoke user cleanup. Keep value snapshots after publication.
    for (auto [component, blocked] : branches) {
        if (services_->components().contains(component)) {
            services_->set_input_blocked(component, blocked);
        }
    }
    reconcile(id);
}

void BreadcrumbComponentHost::reconcile(runtime::ComponentId id) {
    const auto* s = services_->components().state<BreadcrumbState>(id);
    if (!s) {
        return;
    }
    std::vector<input::InteractionId> cancelled;
    for (std::size_t i = 0; i < s->rows.size(); ++i) {
        if (!(s->callback || s->rows[i].descriptor->on_activate_) || !s->model.eligible(i) ||
            !services_->interactions().branch_active(s->rows[i].interaction)) {
            cancelled.push_back(s->rows[i].interaction);
        }
    }
    for (auto hit : cancelled) {
        services_->pointer().cancel_interaction(hit);
        services_->focus().cancel_interaction(hit);
    }
}

void BreadcrumbComponentHost::activate(runtime::ComponentId id, std::size_t i) {
    const auto* s = services_->components().state<BreadcrumbState>(id);
    if (!s || i >= s->rows.size() || !(s->callback || s->rows[i].descriptor->on_activate_) || !s->model.eligible(i) ||
        !services_->interactions().branch_active(s->rows[i].interaction)) {
        return;
    }
    const auto callback = s->rows[i].descriptor->on_activate_ ? s->rows[i].descriptor->on_activate_ : s->callback;
    const BreadcrumbActivation value{s->model.items[i].key, i};
    callback(value);
}

void BreadcrumbComponentHost::append_item(runtime::ComponentId id, const BreadcrumbItem& item,
                                          runtime::ComponentBuildContext* initial) {
    auto& services = *services_;
    auto& s = *services.components().state<BreadcrumbState>(id);
    const auto i = s.rows.size();
    s.rows.emplace_back();
    s.rows.back().descriptor = item;
    *s.rows.back().index = i;
    s.model.items.push_back({item.key_, read_prop(item.disabled_), read_prop(item.visible_), {}});
    const auto content = Content{[&] {
        auto& children = runtime::require_component_build_context();
        auto& row = s.rows[i];
        row.component = children.mount_component<int>(0);
        row.node = children.root(row.component);
        row.title.set(read_prop(item.title_));
        row.interaction = services.interactions().create({row.component, row.node, {}, false, true});
        const auto hit =
            children.register_scene_fragment(row.component, runtime::SceneFragmentPlacement::before_children);
        services.scene_composer().set_fragment(hit, {}, row.interaction);
        const auto fragment =
            children.register_scene_fragment(row.component, runtime::SceneFragmentPlacement::before_children);
        row.surface = services.surfaces().create_content_range(fragment, {});
        children.on_resource_cleanup(row.component, [&services, row_component = row.component, node = row.node, hit,
                                                     interaction = row.interaction, surface = row.surface] {
            services.interactions().set_input_blocked(row_component, false);
            services.pointer().cancel_interaction(interaction);
            services.focus().cancel_interaction(interaction);
            services.interactions().remove(interaction);
            services.scene_composer().remove_fragment(hit);
            services.surfaces().destroy_content_range(surface);
            services.layout().remove_layout(node);
        });
        input::InteractionHandlers handlers;
        handlers.target = [this, id, index = row.index](input::PointerDispatchContext& event) {
            auto* current = services_->components().state<BreadcrumbState>(id);
            const auto i = *index;
            if (!current || i >= current->rows.size()) {
                return;
            }
            if (event.kind() == input::PointerEventKind::cancel) {
                current->pressed.reset();
                refresh(id, false);
                return;
            }
            if (!(current->callback || current->rows[i].descriptor->on_activate_) || !current->model.eligible(i) ||
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
                const bool hit = current->pressed == i && event.press_origin() == row.interaction &&
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
        services.layout().set_layout(row.node, layout::BoxLayout{});
        const auto mount_content = [&] {
            auto& inner_build = runtime::require_component_build_context();
            const auto content = inner_build.mount_component<int>(0);
            row.content_node = inner_build.root(content);
            services.nodes().require(row.content_node).clip_content = true;
            services.layout().set_layout(
                row.content_node,
                layout::ComponentLayout{
                    [this, id, index = row.index](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) {
                        return measure_item(id, *index, e, c);
                    },
                    [this, id, index = row.index](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect b) {
                        place_item(id, *index, e, b);
                    }});

            inner_build.on_resource_cleanup(
                content, [&services, node = row.content_node] { services.layout().remove_layout(node); });
            children.mount_slot_with_semantic_text_style(
                content, Content{[&] {
                    auto& inner = runtime::require_component_build_context();
                    if (item.icon_) {
                        row.icon_node = slot(services, inner,
                                             Content{[icon = *item.icon_] { SlotContentAccess::function(icon)(); }},
                                             &row.color, &s.icon_font);
                    }
                    row.title_node =
                        slot(services, inner,
                             item.rich_title_
                                 ? Content{[title = *item.rich_title_] { SlotContentAccess::function(title)(); }}
                                 : Content{[title = row.title] { Text(TextProps{}.content(title)); }});
                    if (item.menu_ || item.dropdown_) {
                        row.menu_icon_node =
                            slot(services, inner,
                                 item.dropdown_icon_
                                     ? Content{[icon = *item.dropdown_icon_] { SlotContentAccess::function(icon)(); }}
                                     : Content{[] { Icon(IconProps{}.name(IconName::DownOutlined)); }},
                                 &row.color, &s.icon_font);
                    }
                }},
                Prop<runtime::SemanticForeground>{row.color}, Prop<runtime::SemanticTypography>{s.font});
        };
        if (item.menu_ || item.dropdown_) {
            children.mount_slot(row.component, Content{[&] {
                                    const auto before = services.components().children(row.component).size();
                                    auto config =
                                        item.dropdown_ ? *item.dropdown_ : DropdownProps{}.disabled(row.menu_disabled);
                                    if (item.menu_) {
                                        config.menu(*item.menu_);
                                    }
                                    Dropdown(std::move(config), DropdownTrigger{mount_content});
                                    row.dropdown = services.components().children(row.component).at(before);
                                }});
        } else {
            children.mount_slot(row.component, Content{mount_content});
        }
        row.separator = children.mount_component<int>(0);
        const auto sep_node = children.root(row.separator);
        services.nodes().require(sep_node).clip_content = true;
        services.layout().set_layout(sep_node, layout::BoxLayout{});
        children.on_resource_cleanup(row.separator,
                                     [&services, sep_node] { services.layout().remove_layout(sep_node); });
        children.mount_slot_with_semantic_text_style(
            row.separator,
            (item.separator_ || s.props.separator_)
                ? Content{[separator = item.separator_ ? *item.separator_ : *s.props.separator_] {
                      SlotContentAccess::function(separator)();
                  }}
                : Content{[] { Text(u8"/"); }},
            Prop<runtime::SemanticForeground>{s.separator_color}, Prop<runtime::SemanticTypography>{s.font});
    }};
    if (initial) {
        initial->mount_slot(id, content);
    } else {
        services.append_slot(id, content);
    }
    bind_item(id, i, item);
}

void BreadcrumbComponentHost::mount(const BreadcrumbProps& props) {
    const auto items = read_prop(props.items_);
    BreadcrumbModel model;
    for (const auto& item : items) {
        model.items.push_back({item.key_, read_prop(item.disabled_), read_prop(item.visible_), {}});
    }
    validate_navigation_items(model.items);
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<BreadcrumbState>();
    auto& s = build.state<BreadcrumbState>(id);
    s.node = build.root(id);
    s.props = props;
    s.callback = props.on_activate_;
    services.nodes().require(s.node).clip_content = true;
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    services.layout().set_layout(
        s.node, layout::ComponentLayout{[this, id](auto& e, auto, auto c) { return measure(id, e, c); },
                                        [this, id](auto& e, auto, auto b) { place(id, e, b); }});
    mounted_.push_back(id);
    connect_prop(build.scope(id), props.wrap_, [this, id](bool value) {
        if (auto* s = services_->components().state<BreadcrumbState>(id)) {
            s->wrap = value;
            refresh(id, true);
        }
    });
    connect_prop(build.scope(id), props.direction_, [this, id](TextDirection value) {
        if (value != TextDirection::Auto && value != TextDirection::LeftToRight &&
            value != TextDirection::RightToLeft) {
            throw std::invalid_argument("Invalid Breadcrumb direction");
        }
        if (auto* s = services_->components().state<BreadcrumbState>(id)) {
            s->rtl = value == TextDirection::RightToLeft;
            refresh(id, true);
        }
    });
    connect_prop(build.scope(id), props.items_, [this, id](std::vector<BreadcrumbItem> value) {
        BreadcrumbModel model;
        for (const auto& item : value) {
            model.items.push_back({item.key_, read_prop(item.disabled_), read_prop(item.visible_), {}});
        }
        validate_navigation_items(model.items);
        if (auto* s = services_->components().state<BreadcrumbState>(id)) {
            ++s->items_revision;
            s->pending = std::move(value);
            s->dirty_items = true;
            services_->request_frame();
        }
    });
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::breadcrumb_colors,
                               &theme_runtime::ThemeScope::breadcrumb_metrics,
                               &theme_runtime::ThemeScope::breadcrumb_effects};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](auto) { refresh(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    update_items(id, &build);
}

void BreadcrumbComponentHost::bind_item(runtime::ComponentId id, std::size_t i, const BreadcrumbItem& item) {
    auto& row = services_->components().state<BreadcrumbState>(id)->rows[i];
    row.bindings = std::make_shared<Scope>();
    row.descriptor = item;
    const auto apply = [this, id, index = row.index](auto update, bool geometry) {
        return [this, id, index, update, geometry](auto value) {
            auto* s = services_->components().state<BreadcrumbState>(id);
            if (s && *index < s->rows.size()) {
                update(*s, *index, std::move(value));
                refresh(id, geometry);
            }
        };
    };
    connect_prop(*row.bindings, item.title_,
                 apply([](auto& s, auto i, String v) { s.rows[i].title.set(std::move(v)); }, false));
    connect_prop(*row.bindings, item.visible_,
                 apply([](auto& s, auto i, bool v) { s.model.items[i].visible = v; }, true));
    connect_prop(*row.bindings, item.disabled_,
                 apply([](auto& s, auto i, bool v) { s.model.items[i].disabled = v; }, false));
}

void BreadcrumbComponentHost::update_items(runtime::ComponentId id, runtime::ComponentBuildContext* initial) {
    auto* s = services_->components().state<BreadcrumbState>(id);
    if (!s || !s->dirty_items || s->updating) {
        return;
    }
    const auto items_revision = s->items_revision;
    s->updating = true;
    const auto pending = s->pending;
    const auto original_count = s->rows.size();
    const auto hovered = s->hovered && *s->hovered < s->rows.size() ? s->rows[*s->hovered].descriptor->key_ : String{};
    const auto pressed = s->pressed && *s->pressed < s->rows.size() ? s->rows[*s->pressed].descriptor->key_ : String{};
    std::vector<std::size_t> order;
    bool published{};
    try {
        for (const auto& item : pending) {
            s = services_->components().state<BreadcrumbState>(id);
            if (!s) {
                return;
            }
            auto found = std::ranges::find_if(s->rows, [&](const auto& r) {
                const auto& old = *r.descriptor;
                return old.key_ == item.key_ && old.rich_title_ == item.rich_title_ && old.icon_ == item.icon_ &&
                       old.menu_ == item.menu_ && old.dropdown_ == item.dropdown_ &&
                       old.dropdown_icon_ == item.dropdown_icon_ && old.separator_ == item.separator_;
            });
            if (found == s->rows.end()) {
                order.push_back(s->rows.size());
                append_item(id, item, initial);
            } else {
                const auto i = static_cast<std::size_t>(found - s->rows.begin());
                order.push_back(i);
                if (!(*found->descriptor == item)) {
                    bind_item(id, i, item);
                }
            }
        }
        s = services_->components().state<BreadcrumbState>(id);
        if (!s) {
            return;
        }
        std::vector<BreadcrumbRow> next;
        std::vector<BreadcrumbRow> removed;
        BreadcrumbModel model;
        for (auto i : order) {
            model.items.push_back(s->model.items[i]);
            next.push_back(std::move(s->rows[i]));
        }
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            if (std::ranges::find(order, i) == order.end()) {
                *s->rows[i].index = std::numeric_limits<std::size_t>::max();
                removed.push_back(std::move(s->rows[i]));
            }
        }
        s->rows = std::move(next);
        s->model = std::move(model);
        s->hovered.reset();
        s->pressed.reset();
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            *s->rows[i].index = i;
            if (s->rows[i].descriptor->key_ == hovered) {
                s->hovered = i;
            }
            if (s->rows[i].descriptor->key_ == pressed) {
                s->pressed = i;
            }
        }
        s->dirty_items = s->items_revision != items_revision;
        s->updating = false;
        published = true;
        std::exception_ptr retirement_error;
        for (const auto& row : removed) {
            row.bindings->dispose();
            try {
                services_->set_input_blocked(row.component, true);
            } catch (...) {
                if (!retirement_error) {
                    retirement_error = std::current_exception();
                }
            }
            if (!services_->components().contains(id)) {
                break;
            }
            services_->destroy(row.component);
            if (!services_->components().contains(id)) {
                break;
            }
            services_->destroy(row.separator);
            if (!services_->components().contains(id)) {
                break;
            }
        }
        if (services_->components().contains(id)) {
            refresh(id, true);
        }
        if (retirement_error) {
            std::rethrow_exception(retirement_error);
        }
    } catch (...) {
        s = services_->components().state<BreadcrumbState>(id);
        if (s && !published) {
            for (std::size_t i = original_count; i < s->rows.size(); ++i) {
                services_->retire_failed_component(s->rows[i].component);
                services_->retire_failed_component(s->rows[i].separator);
            }
            s->rows.resize(original_count);
            s->model.items.resize(original_count);
            s->updating = false;
            s->dirty_items = false;
        }
        throw;
    }
}

void BreadcrumbComponentHost::prepare_auxiliary_layout() {
    for (auto id : std::vector{mounted_}) {
        update_items(id);
    }
}

runtime::Size BreadcrumbComponentHost::measure_item(runtime::ComponentId id, std::size_t i,
                                                    layout::LayoutEngine& engine, layout::Constraints c) {
    auto& s = *services_->components().state<BreadcrumbState>(id);
    if (i >= s.rows.size()) {
        return c.constrain({});
    }
    auto& row = s.rows[i];
    const auto& m = services_->components().theme_scope(id)->snapshot().breadcrumb().metrics;
    const float available = std::max(0.0F, c.max_width - m.padding * 2);
    row.icon_size =
        row.icon_node ? engine.measure_child(*row.icon_node, {0, available, 0, c.max_height}) : runtime::Size{};
    row.menu_icon_size = row.menu_icon_node ? engine.measure_child(*row.menu_icon_node, {0, available, 0, c.max_height})
                                            : runtime::Size{};
    const float gap = row.icon_node ? std::min(m.icon_gap, std::max(0.0F, available - row.icon_size.width)) : 0;
    const float menu_gap = row.menu_icon_node ? m.icon_gap : 0;
    row.title_size = engine.measure_child(
        row.title_node, {0, std::max(0.0F, available - row.icon_size.width - gap - row.menu_icon_size.width - menu_gap),
                         0, c.max_height});
    const auto measured = c.constrain(
        {m.padding * 2 + row.icon_size.width + gap + row.title_size.width + row.menu_icon_size.width + menu_gap,
         std::max({m.line_height, row.icon_size.height, row.title_size.height, row.menu_icon_size.height})});
    const auto baseline = services_->nodes().require(row.title_node).first_baseline;
    services_->nodes().require(row.content_node).first_baseline =
        baseline ? std::optional{std::max(0.0F, measured.height - row.title_size.height) / 2 + *baseline}
                 : std::nullopt;
    return measured;
}

void BreadcrumbComponentHost::place_item(runtime::ComponentId id, std::size_t i, layout::LayoutEngine& engine,
                                         runtime::Rect b) {
    auto& s = *services_->components().state<BreadcrumbState>(id);
    if (i >= s.rows.size()) {
        return;
    }
    auto& row = s.rows[i];
    const auto& m = services_->components().theme_scope(id)->snapshot().breadcrumb().metrics;
    const float padding = std::min(m.padding, b.width / 2);
    float cursor = padding;
    float remaining = std::max(0.0F, b.width - padding * 2);
    const auto place = [&](runtime::NodeId node, runtime::Size size) {
        const float w = std::min(remaining, size.width);
        const float h = std::min(b.height, size.height);
        const float x = s.rtl ? b.x + b.width - cursor - w : b.x + cursor;
        engine.place_child(node, {x, b.y + (b.height - h) / 2, w, h});
        cursor += w;
        remaining -= w;
    };
    const auto gap = [&] {
        const float value = std::min(m.icon_gap, remaining);
        cursor += value;
        remaining -= value;
    };
    if (row.icon_node) {
        place(*row.icon_node, row.icon_size);
        gap();
    }
    place(row.title_node, row.title_size);
    if (row.menu_icon_node) {
        gap();
        place(*row.menu_icon_node, row.menu_icon_size);
    }
}

runtime::Size BreadcrumbComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                               layout::Constraints c) {
    auto& s = *services_->components().state<BreadcrumbState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().breadcrumb().metrics;
    float width{};
    float height{};
    float line_width{};
    float line_ascent{};
    float line_descent{};
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (!s.model.items[i].visible) {
            continue;
        }
        auto& row = s.rows[i];
        row.separator_size = s.model.current() != i
                                 ? engine.measure_child(services_->components().root(row.separator),
                                                        {0, std::numeric_limits<float>::infinity(), 0, c.max_height})
                                 : runtime::Size{};
        const float separator = s.model.current() != i ? row.separator_size.width + m.separator_margin * 2 : 0;
        row.natural = engine.measure_child(
            row.node, {0, s.wrap ? std::max(0.0F, c.max_width - separator) : std::numeric_limits<float>::infinity(), 0,
                       c.max_height});
        const float w = row.natural.width + separator;
        const float baseline = services_->nodes().require(row.node).first_baseline.value_or(row.natural.height / 2);
        const auto separator_node = services_->components().root(row.separator);
        const float separator_baseline =
            row.separator_size.height > 0
                ? services_->nodes().require(separator_node).first_baseline.value_or(row.separator_size.height / 2)
                : 0;
        if (s.wrap && line_width > 0 && line_width + w > c.max_width) {
            width = std::max(width, line_width);
            height += line_ascent + line_descent;
            line_width = 0;
            line_ascent = 0;
            line_descent = 0;
        }
        line_width += w;
        line_ascent = std::max({line_ascent, baseline, separator_baseline});
        line_descent =
            std::max({line_descent, row.natural.height - baseline, row.separator_size.height - separator_baseline});
    }
    return c.constrain({std::max(width, line_width), height + line_ascent + line_descent});
}

void BreadcrumbComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect b) {
    auto& s = *services_->components().state<BreadcrumbState>(id);
    const float margin = services_->components().theme_scope(id)->snapshot().breadcrumb().metrics.separator_margin;

    struct Entry {
        std::size_t index;
        float width;
        float separator;
    };

    std::vector<std::vector<Entry>> lines(1);
    float total{};
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (!s.model.items[i].visible) {
            continue;
        }
        const auto& row = s.rows[i];
        const float separator = s.model.current() != i ? row.separator_size.width + margin * 2 : 0;
        const float width = row.natural.width + separator;
        if (s.wrap && total > 0 && total + width > b.width) {
            lines.emplace_back();
            total = 0;
        }
        lines.back().push_back({i, row.natural.width, separator});
        total += width;
    }
    float y = b.y;
    for (const auto& line : lines) {
        float natural_width{};
        float ascent{};
        float descent{};
        for (const auto& entry : line) {
            natural_width += entry.width + entry.separator;
            const auto& row = s.rows[entry.index];
            const float baseline = services_->nodes().require(row.node).first_baseline.value_or(row.natural.height / 2);
            const auto separator_node = services_->components().root(row.separator);
            const float separator_baseline =
                row.separator_size.height > 0
                    ? services_->nodes().require(separator_node).first_baseline.value_or(row.separator_size.height / 2)
                    : 0;
            ascent = std::max({ascent, baseline, separator_baseline});
            descent =
                std::max({descent, row.natural.height - baseline, row.separator_size.height - separator_baseline});
        }
        const float scale = natural_width > b.width && natural_width > 0 ? b.width / natural_width : 1;
        const float height = std::min(ascent + descent, std::max(0.0F, b.y + b.height - y));
        float cursor{};
        for (const auto& entry : line) {
            auto& row = s.rows[entry.index];
            const float w = entry.width * scale;
            const float x = s.rtl ? b.x + b.width - cursor - w : b.x + cursor;
            const float rh = std::min(height, row.natural.height);
            const float baseline = services_->nodes().require(row.node).first_baseline.value_or(rh / 2);
            const float ry = y + std::min(std::max(0.0F, height - rh), std::max(0.0F, ascent - baseline));
            static_cast<void>(engine.measure_child(row.node, layout::Constraints::fixed(w, rh)));
            engine.place_child(row.node, {x, ry, w, rh}, true, true);
            cursor += w;
            if (entry.separator > 0) {
                const float sw = row.separator_size.width * scale;
                cursor += margin * scale;
                const float sx = s.rtl ? b.x + b.width - cursor - sw : b.x + cursor;
                const float sh = std::min(height, row.separator_size.height);
                const auto separator_node = services_->components().root(row.separator);
                const float separator_baseline =
                    services_->nodes().require(separator_node).first_baseline.value_or(sh / 2);
                const float sy = y + std::min(std::max(0.0F, height - sh), std::max(0.0F, ascent - separator_baseline));
                engine.place_child(separator_node, {sx, sy, sw, sh});
                cursor += sw + margin * scale;
            }
        }
        y += height;
    }
}

bool BreadcrumbComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    const auto focused = services_->focus().state().focused;
    for (auto id : mounted_) {
        const auto* s = services_->components().state<BreadcrumbState>(id);
        if (!s) {
            continue;
        }
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            if (focused != s->rows[i].interaction || !(s->callback || s->rows[i].descriptor->on_activate_) ||
                !s->model.eligible(i) || !services_->interactions().branch_active(s->rows[i].interaction)) {
                continue;
            }
            if (event.key != input::Key::enter && event.key != input::Key::space) {
                return false;
            }
            if (event.action == input::KeyAction::down && !event.repeat) {
                services_->focus().defer_focus(s->rows[i].interaction, input::FocusModality::keyboard);
                activate(id, i);
            }
            return true;
        }
    }
    return false;
}

void BreadcrumbComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                               std::optional<input::InteractionId> hit,
                                               std::optional<input::InteractionId>) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<BreadcrumbState>(id);
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

void BreadcrumbComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        reconcile(id);
    }
}

void BreadcrumbComponentHost::on_window_active(bool active) {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (auto* s = services_->components().state<BreadcrumbState>(id)) {
            if (!active) {
                s->pressed.reset();
                s->hovered.reset();
            }
            refresh(id, false);
        }
    }
}

void BreadcrumbComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<BreadcrumbState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        s->clip = services_->component_clip(id, viewport, document_clip);
        const auto& t = services_->components().theme_scope(id)->snapshot().breadcrumb();
        const auto focus = services_->focus().state();
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            const auto& row = s->rows[i];
            const auto& node = services_->nodes().require(row.node);
            const graphics::EffectClip clip{1, services_->nodes().content_clip(row.node, s->clip)};
            const graphics::EffectClip decoration_clip{2, s->clip};
            const float opacity = services_->nodes().content_opacity(row.node);
            const float radius = std::min(t.metrics.radius, std::min(node.bounds.width, node.bounds.height) / 2);
            const bool eligible = (s->callback || row.descriptor->on_activate_) && s->model.eligible(i) &&
                                  !row.dropdown && services_->interactions().branch_active(row.interaction);
            const auto bg = graphics::make_corner_fill_effects(
                {node.bounds, radius}, {true, true, true, true}, t.colors.hover_background,
                eligible && (s->hovered == i || s->pressed == i) ? opacity : 0, node.translation, clip);
            std::vector<graphics::RoundedEffectInstance> effects{bg.begin(), bg.end()};
            effects.push_back(graphics::make_outline_effect(
                {node.bounds, radius}, t.effects.focus_width > 0 ? t.effects.focus_width : 1, t.effects.focus_offset,
                t.colors.focus,
                eligible && focus.focused == row.interaction && focus.focus_visible && t.effects.focus_width > 0 &&
                        node.bounds.width > 0 && node.bounds.height > 0
                    ? opacity
                    : 0,
                node.translation, decoration_clip));
            static_cast<void>(services_->surfaces().update_content_effects(row.surface, effects));
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Breadcrumb(BreadcrumbProps props) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_breadcrumb) {
        throw std::logic_error("Breadcrumb requires window component services");
    }
    detail::active_breadcrumb->mount(props);
}

void Breadcrumb(BreadcrumbProps props, std::vector<BreadcrumbItem> items) {
    props.items(std::move(items));
    Breadcrumb(std::move(props));
}
} // namespace ryn
