#include "component/tree_component.hpp"
#include "component/tree_model.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local TreeComponentHost* active_tree{};
constexpr float infinity = std::numeric_limits<float>::infinity();

struct TreeRow final {
    TreeRowSnapshot view;
    runtime::ComponentId check_mark;
    Signal<String> title{String{}};
    Signal<IconName> arrow{IconName::RightOutlined};
    Signal<runtime::SemanticForeground> color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> mark_color{{1, 1, 1, 1}};
    component::RetainedSurfaceId surface;
    bool custom_icon{};
    bool disabled_before{};
    runtime::Size title_size;
    float height{};
    float top{};
    float indent{};
    float switcher_width{};
    float checkbox_width{};
    float icon_width{};
};

struct TreeState final {
    Scope bindings;
    runtime::NodeId node;
    TreeModel model;
    std::vector<TreeRow> rows;
    std::vector<runtime::ComponentId> required;
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> icon_font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 2> subscriptions;
    component::RetainedSurfaceId background;
    std::function<void(TreeExpandInfo)> expand;
    std::function<void(TreeSelectInfo)> select;
    std::function<void(TreeCheckInfo)> check;
    std::optional<std::size_t> entry;
    std::optional<std::size_t> hover;
    std::optional<std::size_t> pressed;
    std::optional<std::size_t> reveal;
    std::optional<input::InteractionId> last_focus;
    int pressed_part{};
    float scroll{};
    float content_height{};
    runtime::Rect clip;
    bool ready{};
    bool refreshing{};
    bool pending{};
    bool geometry_blocked{};
};

using State = std::shared_ptr<TreeState>;

struct MountTransaction final {
    WindowComponentServices& services;
    std::vector<runtime::ComponentId>& mounted;
    runtime::ComponentId owner;
    bool committed{};

    ~MountTransaction() noexcept {
        if (committed) {
            return;
        }
        services.retire_failed_component(owner);
        std::erase(mounted, owner);
    }
};

struct RetireOnFailure final {
    WindowComponentServices& services;
    runtime::ComponentId owner;
    bool complete{};

    ~RetireOnFailure() noexcept {
        if (!complete) {
            services.retire_failed_component(owner);
        }
    }
};

State state(WindowComponentServices& services, runtime::ComponentId id) {
    const auto* record = services.components().state<State>(id);
    return record ? *record : State{};
}

runtime::SemanticForeground fg(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

runtime::Rect bounds(WindowComponentServices& services, runtime::ComponentId id) {
    const auto& n = services.nodes().require(services.components().root(id));
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

bool contains(runtime::Rect r, float x, float y) {
    return r.width > 0 && r.height > 0 && x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto current = std::optional{child}; current; current = services.components().parent(*current)) {
        if (*current == parent) {
            return true;
        }
    }
    return false;
}

std::optional<std::size_t> focused_row(WindowComponentServices& services, const TreeState& s) {
    const auto focus = services.focus().state().focused;
    const auto* record = focus ? services.interactions().find(*focus) : nullptr;
    if (!record) {
        return {};
    }
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (s.rows[i].view.component.valid() && descendant(services, record->component, s.rows[i].view.component)) {
            return i;
        }
    }
    return {};
}

int part(WindowComponentServices& services, const TreeRow& row, float x, float y, bool checkable) {
    if (contains(bounds(services, row.view.switcher), x, y)) {
        return 1;
    }
    if (checkable && contains(bounds(services, row.view.checkbox), x, y)) {
        return 2;
    }
    return 0;
}
} // namespace

TreeComponentHost::TreeComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

TreeComponentHost::~TreeComponentHost() {
    services_->detach(*this);
}

void* TreeComponentHost::begin_mount() noexcept {
    auto* old = active_tree;
    active_tree = this;
    return old;
}

void TreeComponentHost::end_mount(void* old) noexcept {
    active_tree = static_cast<TreeComponentHost*>(old);
}

bool TreeComponentHost::intact(runtime::ComponentId id) {
    const auto s = state(*services_, id);
    if (!s || !services_->components().scope(id).active()) {
        return false;
    }
    for (auto child : s->required) {
        if (!services_->components().contains(child)) {
            services_->retire_failed_component(id);
            return false;
        }
    }
    return true;
}

void TreeComponentHost::on_destroy() noexcept {
    const auto ids = mounted_;
    for (auto id : ids) {
        static_cast<void>(intact(id));
    }
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

TreeSnapshot TreeComponentHost::snapshot(runtime::ComponentId id) const {
    const auto s = state(*services_, id);
    if (!s) {
        throw std::out_of_range("Tree component is stale");
    }
    const auto checks = s->model.checks(s->model.checked);
    TreeSnapshot result{s->model.normalize(s->model.expanded),
                        s->model.normalize(s->model.selected, true),
                        checks.checked,
                        checks.half_checked,
                        {},
                        s->scroll,
                        s->content_height};
    for (const auto& row : s->rows) {
        result.rows.push_back(row.view);
    }
    return result;
}

void TreeComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    if (!s->ready) {
        return;
    }
    if (s->refreshing) {
        s->pending = true;
        return;
    }
    s->refreshing = true;

    struct Reset final {
        State s;

        ~Reset() {
            s->refreshing = false;
        }
    } reset{s};

    do {
        s->pending = false;
        const auto t = services_->components().theme_scope(id)->snapshot().tree();
        s->font.set({t.metrics.font_family, 400, false, t.metrics.font_size, t.metrics.line_height});
        if (!intact(id)) {
            return;
        }
        s->icon_font.set({t.metrics.font_family, 400, false, t.metrics.icon_size, t.metrics.icon_size});
        if (!intact(id)) {
            return;
        }
        const auto shown = s->model.shown();
        const auto previous_interaction = services_->focus().state().focused;
        const auto previous_focus = focused_row(*services_, *s);
        if (s->reveal && std::ranges::find(shown, *s->reveal) == shown.end()) {
            s->reveal.reset();
        }
        if (geometry && previous_focus && std::ranges::find(shown, *previous_focus) != shown.end()) {
            s->reveal = previous_focus;
        }
        const auto selected = s->model.normalize(s->model.selected, true);
        const auto eligible = [&](std::size_t i) {
            return s->model.eligible(i) && std::ranges::find(shown, i) != shown.end();
        };
        if (previous_focus && eligible(*previous_focus)) {
            s->entry = previous_focus;
        }
        if (!s->entry || !eligible(*s->entry)) {
            s->entry.reset();
            for (auto i : shown) {
                if (eligible(i) && std::ranges::find(selected, s->model.nodes[i].key) != selected.end()) {
                    s->entry = i;
                    break;
                }
            }
            for (auto i : shown) {
                if (!s->entry && eligible(i)) {
                    s->entry = i;
                    break;
                }
            }
        }
        std::optional<std::size_t> transfer;
        const bool transfer_focus =
            previous_focus &&
            (std::ranges::find(shown, *previous_focus) == shown.end() || s->model.disabled ||
             (previous_interaction == s->rows[*previous_focus].view.interaction && !eligible(*previous_focus)) ||
             (s->model.nodes[*previous_focus].disabled && !s->rows[*previous_focus].disabled_before));
        if (transfer_focus) {
            auto ancestor = s->model.nodes[*previous_focus].parent;
            while (ancestor != tree_no_parent && !eligible(ancestor)) {
                ancestor = s->model.nodes[ancestor].parent;
            }
            transfer = ancestor != tree_no_parent ? std::optional{ancestor} : s->entry;
            s->entry = transfer;
        }
        const bool root_shown = s->model.visible && !shown.empty();
        geometry |= services_->components().set_branch_active(id, root_shown);
        services_->nodes().require(s->node).out_of_flow = !root_shown;
        const auto checked = s->model.checks(s->model.checked);
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            auto& row = s->rows[i];
            auto& view = row.view;
            view.shown = std::ranges::find(shown, i) != shown.end();
            view.expanded = s->model.open(i);
            view.selected = std::ranges::find(selected, s->model.nodes[i].key) != selected.end();
            view.checked = std::ranges::find(checked.checked, s->model.nodes[i].key) != checked.checked.end();
            view.indeterminate =
                std::ranges::find(checked.half_checked, s->model.nodes[i].key) != checked.half_checked.end();
            geometry |= services_->components().set_branch_active(view.component, view.shown);
            services_->nodes().require(services_->components().root(view.component)).out_of_flow = !view.shown;
            // Keep the wrapper eligible: a disabled Tree row can contain independently enabled native controls.
            services_->interactions().set_eligible(view.interaction, view.shown);
            services_->interactions().set_focusable(view.interaction, eligible(i));
            services_->interactions().set_tab_stop(view.interaction, s->entry == i);
            const bool disabled = s->model.disabled || s->model.nodes[i].disabled;
            row.color.set(fg(disabled        ? t.colors.disabled
                             : view.selected ? t.colors.node_selected_text
                             : s->hover == i ? t.colors.node_hover_text
                                             : t.colors.text));
            if (!intact(id)) {
                return;
            }
            row.mark_color.set(fg(disabled ? t.colors.disabled : t.colors.checkbox_mark));
            if (!intact(id)) {
                return;
            }
            row.arrow.set(view.expanded ? IconName::DownOutlined : IconName::RightOutlined);
            if (!intact(id)) {
                return;
            }
            geometry |= services_->components().set_branch_active(view.switcher, s->model.nodes[i].end > i + 1);
            geometry |= services_->components().set_branch_active(view.checkbox, s->model.checkable);
            geometry |= services_->components().set_branch_active(row.check_mark, s->model.checkable && view.checked);
            if (row.disabled_before != disabled) {
                row.disabled_before = disabled;
                if (disabled) {
                    services_->set_input_blocked(view.component, true);
                    if (!intact(id)) {
                        return;
                    }
                }
            }
            services_->set_input_blocked(view.component, !view.shown);
            if (!intact(id)) {
                return;
            }
            if (!eligible(i) && s->pressed == i) {
                s->pressed.reset();
                services_->pointer().cancel_pointer_interaction(view.interaction);
                if (!intact(id)) {
                    return;
                }
            }
        }
        services_->mark_scene_structure_dirty();
        services_->dirty().invalidate(s->node, geometry
                                                   ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                         runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                   : runtime::DirtyFlags::Material | runtime::DirtyFlags::HitTest);
        services_->set_input_blocked(id, !root_shown || s->model.disabled || s->geometry_blocked ||
                                             !services_->focus().state().window_active);
        if (!intact(id)) {
            return;
        }
        if (transfer &&
            (!services_->focus().state().focused || services_->focus().state().focused == previous_interaction)) {
            s->reveal = transfer;
            services_->focus().defer_focus(s->rows[*transfer].view.interaction, services_->focus().state().modality);
            if (!intact(id)) {
                return;
            }
        }
        geometry = true;
    } while (s->pending);
}

void TreeComponentHost::mount(const TreeProps& p, const std::vector<TreeItem>& items) {
    if ((p.expanded_ && p.default_expanded_) || (p.selected_ && p.default_selected_) ||
        (p.checked_ && p.default_checked_)) {
        throw std::invalid_argument("Tree controlled and default keys conflict");
    }
    auto s = std::make_shared<TreeState>();
    std::vector<const TreeItem*> flat;
    const auto flatten = [&](auto&& self, const std::vector<TreeItem>& list, std::size_t parent,
                             std::size_t depth) -> void {
        if (depth >= 64 && !list.empty()) {
            throw std::invalid_argument("Tree supports at most 64 levels");
        }
        for (const auto& item : list) {
            if (flat.size() >= 1024) {
                throw std::invalid_argument("Tree supports at most 1024 nodes");
            }
            const auto i = flat.size();
            flat.push_back(&item);
            s->model.nodes.push_back({item.key_, parent, depth});
            self(self, item.children_, i, depth + 1);
            s->model.nodes[i].end = flat.size();
        }
    };
    flatten(flatten, items, tree_no_parent, 0);
    s->model.validate();
    s->rows.resize(flat.size());
    s->model.controlled_expanded = p.expanded_.has_value();
    s->model.controlled_selected = p.selected_.has_value();
    s->model.controlled_checked = p.checked_.has_value();
    s->model.expanded = p.default_expanded_.value_or(TreeKeys{});
    s->model.selected = p.default_selected_.value_or(TreeKeys{});
    s->model.checked = p.default_checked_.value_or(TreeKeys{});
    s->expand = p.on_expand_;
    s->select = p.on_select_;
    s->check = p.on_check_;
    auto& services = *services_;
    auto& b = runtime::require_component_build_context();
    const auto id = b.mount_component<State>(s);
    MountTransaction transaction{services, mounted_, id};
    const auto user_slot = [this, id](const auto& slot) {
        return Content{[this, id, callback = SlotContentAccess::function(slot)] {
            // Retire before the enclosing slot/append transaction can roll back a focused child.
            RetireOnFailure rollback{*services_, id};
            callback();
            rollback.complete = intact(id);
        }};
    };
    s->node = b.root(id);
    mounted_.push_back(id);
    services.nodes().require(s->node).clip_content = true;
    b.on_resource_cleanup(id, [&services, id, node = s->node, weak = std::weak_ptr{s}] {
        if (auto current = weak.lock()) {
            current->bindings.dispose();
        }
        services.interactions().set_input_blocked(id, false);
        services.layout().remove_layout(node);
    });
    const auto fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s->background = services.surfaces().create_content_range(fragment, {});
    b.on_resource_cleanup(id,
                          [&services, surface = s->background] { services.surfaces().destroy_content_range(surface); });
    const auto root_interaction = services.interactions().create({id, s->node, {}, true, false, {}, false, false});
    const auto root_hit = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    services.scene_composer().set_fragment(root_hit, {}, root_interaction);
    b.on_resource_cleanup(id, [&services, root_hit, root_interaction] {
        services.pointer().cancel_interaction(root_interaction);
        services.focus().cancel_interaction(root_interaction);
        services.interactions().remove(root_interaction);
        services.scene_composer().remove_fragment(root_hit);
    });
    runtime::connect_layout_style(s->bindings, p.layout_, s->node, services.nodes(), services.dirty(),
                                  [this, id] { return intact(id); });
    if (!intact(id)) {
        return;
    }
    const auto bind_value = [&]<class T, class Apply>(const Prop<T>& prop, Apply apply) {
        connect_prop(s->bindings, prop, [this, id, apply](T value) {
            if (intact(id)) {
                const auto current = state(*services_, id);
                apply(*current, std::move(value));
                if (intact(id)) {
                    refresh(id, true);
                }
            }
        });
        return intact(id);
    };
    if (!bind_value(p.visible_, [](auto& v, bool x) { v.model.visible = x; }) ||
        !bind_value(p.disabled_, [](auto& v, bool x) { v.model.disabled = x; }) ||
        !bind_value(p.checkable_, [](auto& v, bool x) { v.model.checkable = x; })) {
        return;
    }
    if (p.expanded_ && !bind_value(*p.expanded_, [](auto& v, TreeKeys x) { v.model.expanded = std::move(x); })) {
        return;
    }
    if (p.selected_ && !bind_value(*p.selected_, [](auto& v, TreeKeys x) { v.model.selected = std::move(x); })) {
        return;
    }
    if (p.checked_ && !bind_value(*p.checked_, [](auto& v, TreeKeys x) { v.model.checked = std::move(x); })) {
        return;
    }
    for (std::size_t i = 0; i < flat.size(); ++i) {
        if (!bind_value(flat[i]->title_, [i](auto& v, String x) { v.rows[i].title.set(std::move(x)); }) ||
            !bind_value(flat[i]->visible_, [i](auto& v, bool x) { v.model.nodes[i].visible = x; }) ||
            !bind_value(flat[i]->disabled_, [i](auto& v, bool x) { v.model.nodes[i].disabled = x; })) {
            return;
        }
    }
    const auto remember_generated = [&](auto&& self, runtime::ComponentId parent) -> void {
        for (auto child : services.components().children(parent)) {
            s->required.push_back(child);
            self(self, child);
        }
    };
    const auto region = [&](bool centered = false) {
        auto& c = runtime::require_component_build_context();
        const auto child = c.mount_component<int>(0);
        s->required.push_back(child);
        const auto node = c.root(child);
        services.nodes().require(node).clip_content = true;
        services.nodes().require(node).clip_decorations = false;
        services.layout().set_layout(node, layout::BoxLayout{});
        if (centered) {
            services.layout().set_layout(
                node, layout::ComponentLayout{
                          [&services](layout::LayoutEngine& e, runtime::NodeId n, layout::Constraints c) {
                              runtime::Size result;
                              for (auto child : services.nodes().require(n).children) {
                                  const auto size = e.measure_child(child, {0, c.max_width, 0, c.max_height});
                                  result.width = std::max(result.width, size.width);
                                  result.height = std::max(result.height, size.height);
                              }
                              return c.constrain(result);
                          },
                          [&services](layout::LayoutEngine& e, runtime::NodeId n, runtime::Rect r) {
                              for (auto child : services.nodes().require(n).children) {
                                  const auto natural = services.nodes().require(child).layout_size;
                                  const float w = std::min(r.width, natural.width);
                                  const float h = std::min(r.height, natural.height);
                                  e.place_child(child, {r.x + (r.width - w) / 2, r.y + (r.height - h) / 2, w, h});
                              }
                          }});
        }
        c.on_resource_cleanup(child, [&services, child, node] {
            services.interactions().set_input_blocked(child, false);
            services.layout().remove_layout(node);
        });
        return child;
    };
    const auto t = b.theme_scope()->snapshot().tree();
    s->font.set({t.metrics.font_family, 400, false, t.metrics.font_size, t.metrics.line_height});
    s->icon_font.set({t.metrics.font_family, 400, false, t.metrics.icon_size, t.metrics.icon_size});
    b.mount_slot(
        id, Content{[&] {
            auto& c = runtime::require_component_build_context();
            for (std::size_t i = 0; i < flat.size(); ++i) {
                if (!intact(id)) {
                    return;
                }
                auto& row = s->rows[i];
                row.view.component = region();
                const auto row_id = row.view.component;
                const auto node = c.root(row_id);
                row.view.interaction = services.interactions().create({row_id, node, {}, true, true, {}, true, false});
                const auto hit = c.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
                services.scene_composer().set_fragment(hit, {}, row.view.interaction);
                const auto paint = c.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
                row.surface = services.surfaces().create_content_range(paint, {});
                c.on_resource_cleanup(row_id,
                                      [&services, hit, interaction = row.view.interaction, surface = row.surface] {
                                          services.pointer().cancel_interaction(interaction);
                                          services.focus().cancel_interaction(interaction);
                                          services.interactions().remove(interaction);
                                          services.scene_composer().remove_fragment(hit);
                                          services.surfaces().destroy_content_range(surface);
                                      });
                input::InteractionHandlers handlers;
                handlers.target = [this, id, i](input::PointerDispatchContext& event) {
                    if (!intact(id)) {
                        return;
                    }
                    const auto current = state(*services_, id);
                    if (event.kind() == input::PointerEventKind::cancel) {
                        current->pressed.reset();
                        refresh(id, false);
                        return;
                    }
                    const auto& row = current->rows[i];
                    if (!current->model.eligible(i) || !row.view.shown ||
                        !services_->interactions().branch_active(row.view.interaction) ||
                        event.event().button != input::PointerButton::primary) {
                        return;
                    }
                    const int zone = part(*services_, row, event.event().x, event.event().y, current->model.checkable);
                    if (event.kind() == input::PointerEventKind::down) {
                        current->pressed = i;
                        current->pressed_part = zone;
                        current->entry = i;
                        static_cast<void>(event.capture_pointer());
                        event.stop_propagation();
                        refresh(id, false);
                    } else if (event.kind() == input::PointerEventKind::up) {
                        const auto clip = services_->nodes().content_clip(
                            services_->components().root(row.view.component), current->clip);
                        const bool hit =
                            current->pressed == i && current->pressed_part == zone &&
                            event.press_origin() == row.view.interaction &&
                            contains(graphics::intersect_effect_bounds(bounds(*services_, row.view.component), clip),
                                     event.event().x, event.event().y);
                        current->pressed.reset();
                        static_cast<void>(event.release_pointer_capture());
                        event.stop_propagation();
                        refresh(id, false);
                        if (hit && intact(id)) {
                            activate(id, i, zone);
                        }
                    }
                };
                services.interactions().set_handlers(row.view.interaction, std::move(handlers));
                input::FocusHandlers focus;
                focus.state_changed = [this, id, i](input::FocusPresentation value) {
                    if (intact(id)) {
                        if (value.focused) {
                            const auto current = state(*services_, id);
                            current->entry = i;
                            current->reveal = i;
                            services_->dirty().invalidate(current->node, runtime::DirtyFlags::Layout);
                        }
                        refresh(id, false);
                    }
                };
                services.interactions().set_focus_handlers(row.view.interaction, std::move(focus));
                c.mount_slot(row_id, Content{[&] {
                                 auto& inner = runtime::require_component_build_context();
                                 row.view.switcher = region(true);
                                 inner.mount_slot_with_semantic_text_style(
                                     row.view.switcher, Content{[arrow = row.arrow] { Icon(IconProps{}.name(arrow)); }},
                                     row.color, s->icon_font);
                                 if (!intact(id)) {
                                     return;
                                 }
                                 remember_generated(remember_generated, row.view.switcher);
                                 row.view.checkbox = region(true);
                                 inner.mount_slot(row.view.checkbox, Content{[&] { row.check_mark = region(); }});
                                 inner.mount_slot_with_semantic_text_style(
                                     row.check_mark, Content{[] { Icon(IconProps{}.name(IconName::CheckOutlined)); }},
                                     row.mark_color, s->icon_font);
                                 if (!intact(id)) {
                                     return;
                                 }
                                 remember_generated(remember_generated, row.check_mark);
                                 row.view.icon = region(true);
                                 row.custom_icon = flat[i]->icon_.has_value();
                                 if (flat[i]->icon_) {
                                     inner.mount_slot_with_semantic_text_style(
                                         row.view.icon, user_slot(*flat[i]->icon_), row.color, s->font);
                                 }
                                 if (!intact(id)) {
                                     return;
                                 }
                                 row.view.title = region();
                                 inner.mount_slot_with_semantic_text_style(
                                     row.view.title,
                                     flat[i]->rich_title_
                                         ? user_slot(*flat[i]->rich_title_)
                                         : Content{[title = row.title] { Text(TextProps{}.content(title)); }},
                                     row.color, s->font);
                                 if (intact(id) && !flat[i]->rich_title_) {
                                     remember_generated(remember_generated, row.view.title);
                                 }
                             }});
                if (!intact(id)) {
                    return;
                }
                services.layout().set_layout(
                    node,
                    layout::ComponentLayout{[this, id, i](layout::LayoutEngine& e, runtime::NodeId,
                                                          layout::Constraints c) { return measure_row(id, i, e, c); },
                                            [this, id, i](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) {
                                                place_row(id, i, e, r);
                                            }});
            }
        }});
    if (!intact(id)) {
        return;
    }
    services.layout().set_layout(
        s->node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) { return measure(id, e, c); },
            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) { place(id, e, r); }});
    const auto theme = b.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::tree_colors, &theme_runtime::ThemeScope::tree_metrics};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        auto subscription =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { refresh(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
        if (!intact(id)) {
            return;
        }
        s->subscriptions[i] = std::move(subscription);
    }
    s->ready = true;
    refresh(id, true);
    transaction.committed = intact(id);
}

void TreeComponentHost::activate(runtime::ComponentId id, std::size_t i, int zone) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    if (i >= s->rows.size() || !s->rows[i].view.shown || !s->model.eligible(i) ||
        !services_->interactions().branch_active(s->rows[i].view.interaction)) {
        return;
    }
    const auto key = s->model.nodes[i].key;
    std::function<void()> callback;
    if (zone == 1) {
        if (s->model.nodes[i].end == i + 1) {
            return;
        }
        auto next = s->model.expand(i);
        const bool expanded = std::ranges::find(next, key) != next.end();
        if (!s->model.controlled_expanded) {
            s->model.expanded = next;
        }
        if (s->expand) {
            callback = [f = s->expand, info = TreeExpandInfo{key, next, expanded}] {
                f(info);
            };
        }
    } else if (zone == 2) {
        if (!s->model.checkable) {
            return;
        }
        auto next = s->model.toggle_check(i);
        const bool checked = std::ranges::find(next.checked, key) != next.checked.end();
        if (!s->model.controlled_checked) {
            s->model.checked = next.checked;
        }
        if (s->check) {
            callback = [f = s->check, info = TreeCheckInfo{key, next.checked, next.half_checked, checked}] {
                f(info);
            };
        }
    } else {
        auto next = s->model.select(i);
        const bool selected = !next.empty();
        if (!s->model.controlled_selected) {
            s->model.selected = next;
        }
        if (s->select) {
            callback = [f = s->select, info = TreeSelectInfo{key, next, selected}] {
                f(info);
            };
        }
    }
    refresh(id, true);
    if (intact(id) && callback) {
        callback();
        static_cast<void>(intact(id));
    }
}

runtime::Size TreeComponentHost::measure_row(runtime::ComponentId id, std::size_t i, layout::LayoutEngine& e,
                                             layout::Constraints c) {
    if (!intact(id)) {
        return c.constrain({});
    }
    const auto s = state(*services_, id);
    auto& row = s->rows[i];
    const auto m = services_->components().theme_scope(id)->snapshot().tree().metrics;
    row.indent = std::min(c.max_width, static_cast<float>(s->model.nodes[i].depth) * m.indent_size);
    float available = std::max(0.0F, c.max_width - row.indent);
    const auto take = [&](float amount) {
        const float value = std::min(available, amount);
        available -= value;
        return value;
    };
    row.switcher_width = take(m.switcher_size);
    row.checkbox_width = s->model.checkable ? take(m.checkbox_size + m.icon_gap) : 0;
    row.icon_width = row.custom_icon ? take(m.title_height + m.icon_gap) : 0;
    const auto measure = [&](runtime::ComponentId child, float width) {
        return e.measure_child(services_->components().root(child), {0, width, 0, infinity});
    };
    static_cast<void>(measure(row.view.switcher, std::min(m.icon_size, row.switcher_width)));
    static_cast<void>(measure(row.view.checkbox, std::min(m.checkbox_size, row.checkbox_width)));
    static_cast<void>(measure(row.view.icon, std::min(m.title_height, row.icon_width)));
    row.title_size = measure(row.view.title, std::max(0.0F, available - 2 * std::min(m.padding_inline, available / 2)));
    row.height = std::max({m.title_height, s->model.checkable ? m.checkbox_size : 0, row.title_size.height});
    return c.constrain({row.indent + row.switcher_width + row.checkbox_width + row.icon_width + row.title_size.width +
                            2 * m.padding_inline,
                        row.height});
}

runtime::Size TreeComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& e, layout::Constraints c) {
    if (!intact(id)) {
        return c.constrain({});
    }
    const auto s = state(*services_, id);
    const auto m = services_->components().theme_scope(id)->snapshot().tree().metrics;
    runtime::Size natural;
    bool first = true;
    for (auto i : s->model.shown()) {
        if (!first) {
            natural.height += m.row_gap;
        }
        first = false;
        s->rows[i].top = natural.height;
        const auto size =
            e.measure_child(services_->components().root(s->rows[i].view.component), {0, c.max_width, 0, infinity});
        natural.width = std::max(natural.width, size.width);
        natural.height += size.height;
    }
    s->content_height = natural.height;
    return c.constrain(natural);
}

void TreeComponentHost::place_row(runtime::ComponentId id, std::size_t i, layout::LayoutEngine& e, runtime::Rect r) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    const auto& row = s->rows[i];
    const auto m = services_->components().theme_scope(id)->snapshot().tree().metrics;
    float x = r.x + row.indent;
    const auto place_center = [&](runtime::ComponentId child, float width, float extent) {
        const float side = std::max(0.0F, std::min({width, extent, r.height}));
        e.place_child(services_->components().root(child),
                      {x + (width - side) / 2, r.y + (r.height - side) / 2, side, side});
        x += width;
    };
    place_center(row.view.switcher, row.switcher_width, m.switcher_size);
    place_center(row.view.checkbox, row.checkbox_width, m.checkbox_size);
    place_center(row.view.icon, row.icon_width, m.title_height);
    const float remaining = std::max(0.0F, r.x + r.width - x);
    const float padding = std::min(m.padding_inline, remaining / 2);
    e.place_child(services_->components().root(row.view.title),
                  {x + padding, r.y + (r.height - std::min(r.height, row.title_size.height)) / 2,
                   std::max(0.0F, remaining - 2 * padding), std::min(r.height, row.title_size.height)});
}

void TreeComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& e, runtime::Rect r) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    s->scroll = std::clamp(s->scroll, 0.0F, std::max(0.0F, s->content_height - r.height));
    if (s->reveal && s->rows[*s->reveal].view.shown) {
        const auto& row = s->rows[*s->reveal];
        if (row.top < s->scroll) {
            s->scroll = row.top;
        } else if (row.top + row.height > s->scroll + r.height) {
            s->scroll = std::max(row.top, row.top + row.height - r.height);
        }
        s->reveal.reset();
    }
    s->scroll = std::clamp(s->scroll, 0.0F, std::max(0.0F, s->content_height - r.height));
    for (auto i : s->model.shown()) {
        const auto& row = s->rows[i];
        e.place_child(services_->components().root(row.view.component),
                      {r.x, r.y + row.top - s->scroll, r.width, row.height});
    }
}

bool TreeComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.modifiers != input::KeyModifier::none) {
        return false;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        const auto focus = services_->focus().state().focused;
        auto current = std::ranges::find_if(s->rows, [&](const auto& row) { return focus == row.view.interaction; });
        if (current == s->rows.end()) {
            continue;
        }
        const auto index = static_cast<std::size_t>(current - s->rows.begin());
        if (!s->model.eligible(index) || !current->view.shown ||
            !services_->interactions().branch_active(current->view.interaction)) {
            return false;
        }
        const bool activation = event.key == input::Key::enter || event.key == input::Key::space;
        const bool navigation = event.key == input::Key::up || event.key == input::Key::down ||
                                event.key == input::Key::left || event.key == input::Key::right ||
                                event.key == input::Key::home || event.key == input::Key::end;
        if (!activation && !navigation) {
            return false;
        }
        if (event.action != input::KeyAction::down || (activation && event.repeat)) {
            return true;
        }
        auto target = index;
        auto shown = s->model.shown();
        std::erase_if(shown, [&](auto i) { return !s->model.eligible(i); });
        auto position = std::ranges::find(shown, index);
        if (event.key == input::Key::home) {
            target = shown.front();
        } else if (event.key == input::Key::end) {
            target = shown.back();
        } else if (event.key == input::Key::up && position != shown.begin()) {
            target = *std::prev(position);
        } else if (event.key == input::Key::down && std::next(position) != shown.end()) {
            target = *std::next(position);
        } else if (event.key == input::Key::right) {
            if (!s->model.open(index)) {
                activate(id, index, 1);
                return true;
            }
            if (std::next(position) != shown.end() && *std::next(position) < s->model.nodes[index].end) {
                target = *std::next(position);
            }
        } else if (event.key == input::Key::left) {
            if (s->model.open(index)) {
                activate(id, index, 1);
                return true;
            }
            auto parent = s->model.nodes[index].parent;
            while (parent != tree_no_parent && !s->model.eligible(parent)) {
                parent = s->model.nodes[parent].parent;
            }
            if (parent != tree_no_parent) {
                target = parent;
            }
        }
        s->entry = target;
        s->reveal = target;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout);
        services_->focus().defer_focus(s->rows[target].view.interaction, input::FocusModality::keyboard);
        if (activation && intact(id)) {
            activate(id, index, event.key == input::Key::space && s->model.checkable ? 2 : 0);
        }
        return true;
    }
    return false;
}

void TreeComponentHost::on_pointer_input(const input::PointerInputEvent& event, std::optional<input::InteractionId> hit,
                                         std::optional<input::InteractionId>) {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        std::optional<std::size_t> next;
        for (std::size_t i = 0; event.action != input::PointerAction::cancel && i < s->rows.size(); ++i) {
            if (hit == s->rows[i].view.interaction && s->model.eligible(i)) {
                next = i;
            }
        }
        if (s->hover != next) {
            s->hover = next;
            refresh(id, false);
        }
    }
}

void TreeComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        if (s->pressed && !services_->interactions().branch_active(s->rows[*s->pressed].view.interaction)) {
            const auto hit = s->rows[*s->pressed].view.interaction;
            s->pressed.reset();
            services_->pointer().cancel_pointer_interaction(hit);
        }
    }
}

void TreeComponentHost::on_window_active(bool) {
    const auto ids = mounted_;
    for (auto id : ids) {
        refresh(id, false);
    }
}

void TreeComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        const auto focus = services_->focus().state().focused;
        if (focus != s->last_focus) {
            s->last_focus = focus;
            if (auto row = focused_row(*services_, *s)) {
                s->reveal = row;
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout);
            }
        }
    }
}

bool TreeComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
    if (!record) {
        return false;
    }
    // A Tree at its scroll boundary hands off to a logical Tree ancestor,
    // while the nearest window layer remains an ownership boundary.
    for (auto owner = std::optional{record->component}; owner; owner = services_->components().parent(*owner)) {
        if (std::ranges::find(mounted_, *owner) != mounted_.end() && intact(*owner)) {
            const auto s = state(*services_, *owner);
            if (!services_->interactions().component_active(*owner) ||
                !contains(services_->nodes().content_clip(s->node, s->clip), event.x, event.y)) {
                return false;
            }
            const float height = bounds(*services_, *owner).height;
            const float maximum = std::max(0.0F, s->content_height - height);
            if (maximum > 0) {
                const float next = std::clamp(s->scroll - event.delta_y * 32, 0.0F, maximum);
                if (s->scroll != next) {
                    s->scroll = next;
                    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry |
                                                               runtime::DirtyFlags::HitTest);
                    return true;
                }
            }
        }
        if (services_->components().is_window_layer_root(*owner)) {
            return false;
        }
    }
    return false;
}

void TreeComponentHost::position_window_layers(runtime::Size, runtime::Rect) {
    // Coordinate committed zero-area bounds before child popup/effect painting.
    // Never invoke the input boundary callbacks from LayoutEngine::place_child.
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        const auto& committed = services_->nodes().require(s->node);
        const bool geometry_blocked =
            committed.place_generation != 0 && (committed.bounds.width <= 0 || committed.bounds.height <= 0);
        if (s->geometry_blocked != geometry_blocked) {
            s->geometry_blocked = geometry_blocked;
            refresh(id, false);
        }
    }
}

void TreeComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        s->clip = services_->component_clip(id, viewport, document_clip);
        const auto t = services_->components().theme_scope(id)->snapshot().tree();
        const auto& m = t.metrics;
        const auto& root = services_->nodes().require(s->node);
        const auto clip = services_->nodes().content_clip(s->node, s->clip);
        const float opacity =
            services_->components().branch_active(id) ? services_->nodes().content_opacity(s->node) : 0;
        const auto background =
            graphics::make_corner_fill_effects({root.bounds, 0}, {false, false, false, false}, t.colors.background,
                                               opacity, root.translation, graphics::EffectClip{1, clip});
        static_cast<void>(services_->surfaces().update_content_effects(s->background, background));
        for (std::size_t i = 0; i < s->rows.size(); ++i) {
            const auto& row = s->rows[i];
            const auto& node = services_->nodes().require(services_->components().root(row.view.component));
            std::vector<graphics::RoundedEffectInstance> effects;
            const float alpha = row.view.shown ? opacity : 0;
            const auto add = [&](runtime::Rect r, float radius, Color color, graphics::EffectClip c) {
                auto values =
                    graphics::make_corner_fill_effects({r, std::min(radius, std::min(r.width, r.height) / 2)},
                                                       {true, true, true, true}, color, alpha, node.translation, c);
                effects.insert(effects.end(), values.begin(), values.end());
            };
            const auto row_clip =
                services_->nodes().content_clip(services_->components().root(row.view.component), clip);
            const float start = std::min(node.bounds.width, row.indent + row.switcher_width + row.checkbox_width);
            const runtime::Rect content{node.bounds.x + start, node.bounds.y,
                                        std::min(std::max(0.0F, node.bounds.width - start),
                                                 row.icon_width + row.title_size.width + 2 * m.padding_inline),
                                        node.bounds.height};
            if (row.view.selected || s->hover == i) {
                add(content, m.border_radius,
                    row.view.selected ? t.colors.node_selected_background : t.colors.node_hover_background,
                    {1, row_clip});
            }
            if (s->model.checkable) {
                const auto checkbox =
                    services_->nodes().require(services_->components().root(row.view.checkbox)).bounds;
                const bool checked = row.view.checked || row.view.indeterminate;
                const bool disabled = s->model.disabled || s->model.nodes[i].disabled;
                const auto check_clip =
                    graphics::intersect_effect_bounds(bounds(*services_, row.view.checkbox), row_clip);
                add(checkbox, m.border_radius,
                    disabled  ? t.colors.disabled
                    : checked ? t.colors.checkbox_checked
                              : t.colors.checkbox_border,
                    graphics::EffectClip{1, check_clip});
                if (!checked || disabled) {
                    const float border = std::min({m.border_width, checkbox.width / 2, checkbox.height / 2});
                    add({checkbox.x + border, checkbox.y + border, checkbox.width - 2 * border,
                         checkbox.height - 2 * border},
                        std::max(0.0F, m.border_radius - border), t.colors.checkbox_background,
                        graphics::EffectClip{1, check_clip});
                }
                if (row.view.indeterminate) {
                    const float side = std::min(checkbox.width, checkbox.height) / 2;
                    add({checkbox.x + (checkbox.width - side) / 2, checkbox.y + (checkbox.height - side) / 2, side,
                         side},
                        0, disabled ? t.colors.disabled : t.colors.checkbox_mark, graphics::EffectClip{1, check_clip});
                }
            }
            const auto focus = services_->focus().state();
            const bool focused = focus.focused == row.view.interaction && focus.focus_visible && s->model.eligible(i) &&
                                 services_->interactions().branch_active(row.view.interaction) && row.view.shown &&
                                 content.width > 0 && content.height > 0;
            const auto external = services_->component_clip(row.view.component, viewport, document_clip,
                                                            runtime::ClipPurpose::decoration);
            effects.push_back(graphics::make_outline_effect(
                {content, std::min({m.border_radius, content.width / 2, content.height / 2})},
                m.focus_width > 0 ? m.focus_width : 1, m.focus_offset, t.colors.focus,
                focused && m.focus_width > 0 ? alpha : 0, node.translation, graphics::EffectClip{2, external}));
            static_cast<void>(services_->surfaces().update_content_effects(row.surface, effects));
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Tree(TreeProps props, std::vector<TreeItem> items) {
    if (!detail::active_tree) {
        throw std::logic_error("Tree requires window component services");
    }
    detail::active_tree->mount(props, items);
}
} // namespace ryn
