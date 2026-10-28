#include "component/tree_select_component.hpp"
#include "component/tree_select_model.hpp"
#include "component/input_affix_action.hpp"
#include "component/input_component.hpp"
#include "component/popover_component.hpp"
#include "component/tree_component.hpp"
#include "theme/input_tokens.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <cmath>

namespace ryn::detail {
namespace {
thread_local TreeSelectComponentHost* active_tree_select{};

struct ItemState final {
    Signal<String> title{String{}};
    Signal<bool> disabled{false};
    Signal<bool> visible{true};
};

struct TreeSelectState final {
    explicit TreeSelectState(TreeSelectModel value) : model(std::move(value)) {}

    TreeSelectModel model;
    Scope bindings;
    runtime::NodeId node;
    runtime::ComponentId selector;
    runtime::ComponentId popover;
    runtime::ComponentId tree;
    runtime::ComponentId empty;
    input::InteractionId selector_hit;
    TreeSelectRef reference;
    std::vector<ItemState> items;
    Signal<String> display{String{}};
    Signal<String> placeholder{String{}};
    Signal<String> empty_label{String{u8"No data"}};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<InputVariant> variant{InputVariant::Outlined};
    Signal<InputStatus> status{InputStatus::Default};
    Signal<TreeKeys> selected{TreeKeys{}};
    Signal<TreeKeys> expanded{TreeKeys{}};
    Signal<bool> popup_open{false};
    Signal<bool> input_disabled{false};
    Signal<bool> clear_visible{false};
    Signal<bool> action_disabled{false};
    Signal<float> width{180};
    Signal<float> height{256};
    Signal<ThemeConfig> child_theme{ThemeConfig{}};
    std::array<theme_runtime::Subscription, 6> subscriptions;
    std::function<void(String)> on_change;
    std::function<void(bool)> on_open;
    std::function<void(TreeExpandInfo)> on_expand;
    bool controlled_expanded{};
    bool disabled{};
    bool read_only{};
    bool visible{true};
    bool allow_clear{};
    bool geometry_blocked{};
    bool focused_inside{};
    bool focus_pending{};
    bool ready{};
    bool refreshing{};
    bool pending{};
    bool notifying_close{};
    std::optional<std::uint64_t> close_dispatch;
};

using State = std::shared_ptr<TreeSelectState>;

State state(WindowComponentServices& services, runtime::ComponentId id) {
    const auto* ptr = services.components().state<State>(id);
    return ptr ? *ptr : State{};
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto p = std::optional{child}; p && services.components().contains(*p); p = services.components().parent(*p)) {
        if (*p == parent) {
            return true;
        }
    }
    return false;
}

struct MountTransaction final {
    WindowComponentServices& services;
    runtime::ComponentId id;
    bool complete{};

    ~MountTransaction() noexcept {
        if (!complete) {
            services.retire_failed_component(id);
        }
    }
};

runtime::Rect bounds(WindowComponentServices& services, runtime::ComponentId id) {
    const auto& node = services.nodes().require(services.components().root(id));
    return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
            node.bounds.height};
}
} // namespace

TreeSelectComponentHost::TreeSelectComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

TreeSelectComponentHost::~TreeSelectComponentHost() {
    services_->detach(*this);
}

void* TreeSelectComponentHost::begin_mount() noexcept {
    auto* previous = active_tree_select;
    active_tree_select = this;
    return previous;
}

void TreeSelectComponentHost::end_mount(void* previous) noexcept {
    active_tree_select = static_cast<TreeSelectComponentHost*>(previous);
}

void TreeSelectComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void TreeSelectComponentHost::on_dispose() noexcept {
    mounted_.clear();
}

bool TreeSelectComponentHost::intact(runtime::ComponentId id) {
    const auto s = state(*services_, id);
    if (!s || !services_->components().scope(id).active()) {
        return false;
    }
    if (s->ready) {
        for (auto child : {s->selector, s->popover, s->tree, s->empty}) {
            if (!services_->components().contains(child) || !descendant(*services_, child, id)) {
                services_->retire_failed_component(id);
                return false;
            }
        }
    }
    return true;
}

bool TreeSelectComponentHost::editable(runtime::ComponentId id) {
    if (!intact(id)) {
        return false;
    }
    const auto s = state(*services_, id);
    return s->visible && !s->disabled && !s->read_only && !s->geometry_blocked && window_active_ &&
           services_->interactions().component_active(id);
}

TreeSelectSnapshot TreeSelectComponentHost::snapshot(runtime::ComponentId id) const {
    const auto s = state(*services_, id);
    if (!s || !s->ready) {
        throw std::out_of_range("TreeSelect component is stale or incomplete");
    }
    const auto popup = services_->popover().snapshot(s->popover);
    return {s->selector,           s->popover, s->tree, popup.geometry.bounds, s->model.value(), s->model.label(),
            popup.geometry.visible};
}

void TreeSelectComponentHost::refresh(runtime::ComponentId id) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    if (!s->ready) {
        return;
    }
    s->pending = true;
    if (s->refreshing) {
        return;
    }
    s->refreshing = true;

    struct RefreshGuard final {
        State state;

        ~RefreshGuard() {
            state->refreshing = false;
        }
    } guard{s};

    do {
        s->pending = false;
        const bool active = editable(id);
        // A containing popup temporarily blocks its content while constructing.
        // Commit its initial geometry before treating that temporary state as a
        // lasting suppression. Explicit local gates apply immediately.
        const bool constructing_ancestor = services_->nodes().require(s->node).place_generation == 0 && s->visible &&
                                           !s->disabled && !s->read_only && !s->geometry_blocked && window_active_;
        const bool notify = !active && !constructing_ancestor && s->model.suppress();
        const bool input_blocked = !s->visible || s->disabled || s->geometry_blocked || !window_active_;
        if (services_->components().set_branch_active(id, s->visible)) {
            services_->mark_scene_structure_dirty();
        }
        services_->set_input_blocked(id, input_blocked);
        if (!intact(id)) {
            return;
        }
        s->input_disabled.set(s->disabled);
        if (!intact(id)) {
            return;
        }
        s->action_disabled.set(!active);
        if (!intact(id)) {
            return;
        }
        s->display.set(s->model.label());
        if (!intact(id)) {
            return;
        }
        s->selected.set(s->model.value().empty() ? TreeKeys{} : TreeKeys{s->model.value()});
        if (!intact(id)) {
            return;
        }
        s->clear_visible.set(s->allow_clear && !s->model.value().empty() && active);
        if (!intact(id)) {
            return;
        }
        s->popup_open.set(active && s->model.open());
        if (!intact(id)) {
            return;
        }
        const bool any_root = std::ranges::any_of(
            s->model.nodes(), [](const auto& node) { return node.parent == tree_select_no_parent && node.visible; });
        if (services_->components().set_branch_active(s->empty, !any_root)) {
            services_->nodes().require(services_->components().root(s->empty)).out_of_flow = any_root;
            services_->mark_scene_structure_dirty();
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
        }
        if (notify) {
            notify_open(id, false);
            if (!intact(id)) {
                return;
            }
        }
    } while (s->pending);
}

void TreeSelectComponentHost::request_open(runtime::ComponentId id, bool open, bool keyboard) {
    if (!intact(id) || (open && !editable(id))) {
        return;
    }
    const auto s = state(*services_, id);
    s->focus_pending = open && keyboard;
    if (!open && !editable(id)) {
        refresh(id);
        return;
    }
    const bool notify = s->model.request_open(open);
    refresh(id);
    if (notify && intact(id)) {
        notify_open(id, open);
    }
}

void TreeSelectComponentHost::notify_open(runtime::ComponentId id, bool open) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    if (open) {
        s->close_dispatch.reset();
        const auto callback = s->on_open;
        if (callback) {
            callback(true);
        }
        return;
    }
    const auto dispatch = services_->pointer().dispatch_token();
    if (s->notifying_close || (dispatch && s->close_dispatch == dispatch)) {
        return;
    }
    s->close_dispatch = dispatch;
    s->notifying_close = true;

    struct CloseGuard final {
        State state;

        ~CloseGuard() {
            state->notifying_close = false;
        }
    } guard{s};

    const auto callback = s->on_open;
    if (callback) {
        callback(false);
    }
}

void TreeSelectComponentHost::choose(runtime::ComponentId id, String key) {
    if (!editable(id)) {
        return;
    }
    const auto s = state(*services_, id);
    const bool notify = s->model.request_value(key);
    const auto callback = s->on_change;
    refresh(id);
    if (notify && callback && intact(id)) {
        callback(std::move(key));
    }
    if (intact(id)) {
        request_open(id, false);
    }
}

void TreeSelectComponentHost::update_theme(runtime::ComponentId id) {
    if (!intact(id)) {
        return;
    }
    const auto s = state(*services_, id);
    const auto& snapshot = services_->components().theme_scope(id)->snapshot();
    const auto& token = snapshot.tree_select();
    const auto& t = token.selector;
    auto config = std::make_shared<ThemeConfig>();
    config->input.tokens.background = t.selector_background;
    config->input.tokens.hover_background = t.selector_background;
    config->input.tokens.active_background = t.selector_background;
    config->input.tokens.hover_border_color = t.hover_border_color;
    config->input.tokens.active_border_color = t.active_border_color;
    const auto& original = InputTokenAccess::get(snapshot).active_shadow;
    std::array<ShadowLayer, ShadowList::capacity> shadow{};
    std::copy(original.layers().begin(), original.layers().end(), shadow.begin());
    for (std::size_t i = 0; i < original.size(); ++i) {
        shadow[i].color = t.active_outline_color;
    }
    config->input.tokens.active_shadow = ShadowList{std::span{shadow.data(), original.size()}};
    config->popover.tokens.padding = dp(t.popup_padding);
    config->popover.tokens.gap = dp(t.gap);
    config->popover.tokens.max_width = dp(std::max(1.0F, s->width.get()));
    config->popover.tokens.background = t.popup_background;
    config->popover.tokens.border_radius = dp(t.border_radius);
    config->popover.tokens.shadow = t.shadow;
    config->popover.tokens.z_index_popup = t.z_index_popup;
    config->popover.tokens.font_family = token.tree.metrics.font_family;
    config->popover.tokens.font_size = dp(token.tree.metrics.font_size);
    config->popover.tokens.line_height = dp(token.tree.metrics.line_height);
    config->tree.tokens.background = token.tree.colors.background;
    config->tree.tokens.text = token.tree.colors.text;
    config->tree.tokens.disabled = token.tree.colors.disabled;
    config->tree.tokens.node_hover_background = token.tree.colors.node_hover_background;
    config->tree.tokens.node_hover_text = token.tree.colors.node_hover_text;
    config->tree.tokens.node_selected_background = token.tree.colors.node_selected_background;
    config->tree.tokens.node_selected_text = token.tree.colors.node_selected_text;
    config->tree.tokens.checkbox_border = token.tree.colors.checkbox_border;
    config->tree.tokens.checkbox_background = token.tree.colors.checkbox_background;
    config->tree.tokens.checkbox_checked = token.tree.colors.checkbox_checked;
    config->tree.tokens.checkbox_mark = token.tree.colors.checkbox_mark;
    config->tree.tokens.focus = token.tree.colors.focus;
    config->tree.tokens.title_height = dp(token.tree.metrics.title_height);
    config->tree.tokens.switcher_size = dp(token.tree.metrics.switcher_size);
    config->tree.tokens.indent_size = dp(token.tree.metrics.indent_size);
    config->tree.tokens.row_gap = dp(token.tree.metrics.row_gap);
    config->tree.tokens.icon_size = dp(token.tree.metrics.icon_size);
    config->tree.tokens.icon_gap = dp(token.tree.metrics.icon_gap);
    config->tree.tokens.padding_inline = dp(token.tree.metrics.padding_inline);
    config->tree.tokens.border_radius = dp(token.tree.metrics.border_radius);
    config->tree.tokens.checkbox_size = dp(token.tree.metrics.checkbox_size);
    config->tree.tokens.border_width = dp(token.tree.metrics.border_width);
    config->tree.tokens.focus_width = dp(token.tree.metrics.focus_width);
    config->tree.tokens.focus_offset = dp(token.tree.metrics.focus_offset);
    config->tree.tokens.font_size = dp(token.tree.metrics.font_size);
    config->tree.tokens.line_height = dp(token.tree.metrics.line_height);
    config->tree.tokens.font_family = token.tree.metrics.font_family;
    s->child_theme.set(*config);
}

void TreeSelectComponentHost::mount(const TreeSelectProps& p, const std::vector<TreeSelectItem>& items) {
    if ((p.value_ && p.default_value_) || (p.open_ && p.default_open_) || (p.expanded_ && p.default_expanded_)) {
        throw std::invalid_argument("TreeSelect controlled and default properties conflict");
    }
    auto& services = *services_;
    auto* inputs = services.input_runtime();
    if (!inputs) {
        throw std::logic_error("TreeSelect requires a window Input host");
    }
    std::vector<const TreeSelectItem*> flat;
    std::vector<TreeSelectNode> nodes;
    const auto flatten = [&](auto&& self, const std::vector<TreeSelectItem>& list, std::size_t parent,
                             std::size_t depth) -> void {
        if (depth >= 64 && !list.empty()) {
            throw std::invalid_argument("TreeSelect supports at most 64 levels");
        }
        for (const auto& item : list) {
            if (flat.size() >= 1024) {
                throw std::invalid_argument("TreeSelect supports at most 1024 nodes");
            }
            const auto index = flat.size();
            flat.push_back(&item);
            nodes.push_back({item.key_, {}, parent});
            self(self, item.children_, index, depth + 1);
        }
    };
    flatten(flatten, items, tree_select_no_parent, 0);
    auto s = std::make_shared<TreeSelectState>(TreeSelectModel{std::move(nodes), p.default_value_.value_or(String{}),
                                                               p.value_.has_value(), p.default_open_.value_or(false),
                                                               p.open_.has_value()});
    s->items.resize(flat.size());
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<State>(s);
    MountTransaction transaction{services, id};
    s->node = build.root(id);
    mounted_.push_back(id);
    s->reference = p.ref_.value_or(TreeSelectRef{});
    s->on_change = p.on_change_;
    s->on_open = p.on_open_;
    s->on_expand = p.on_expand_;
    s->controlled_expanded = p.expanded_.has_value();
    s->expanded.set(p.default_expanded_.value_or(TreeKeys{}));
    services.layout().set_layout(s->node, layout::BoxLayout{{}, true});
    build.on_resource_cleanup(id, [&services, id, node = s->node, weak = std::weak_ptr{s}] {
        if (const auto current = weak.lock()) {
            current->bindings.dispose();
        }
        services.interactions().set_input_blocked(id, false);
        services.layout().remove_layout(node);
    });
    runtime::connect_layout_style(s->bindings, p.layout_, s->node, services.nodes(), services.dirty(),
                                  [this, id] { return intact(id); });
    if (!intact(id)) {
        return;
    }
    const auto bind_value = [&]<class T, class F>(const Prop<T>& prop, F apply) {
        connect_prop(s->bindings, prop, [this, id, apply](T value) {
            if (intact(id)) {
                const auto current = state(*services_, id);
                apply(*current, std::move(value));
                if (intact(id)) {
                    refresh(id);
                }
            }
        });
        return intact(id);
    };
    if ((p.value_ && !bind_value(*p.value_, [](auto& v, String x) { v.model.reconcile_value(std::move(x)); })) ||
        (p.open_ && !bind_value(*p.open_, [](auto& v, bool x) { v.model.reconcile_open(x); })) ||
        (p.expanded_ && !bind_value(*p.expanded_, [](auto& v, TreeKeys x) { v.expanded.set(std::move(x)); })) ||
        !bind_value(p.visible_, [](auto& v, bool x) { v.visible = x; }) ||
        !bind_value(p.disabled_, [](auto& v, bool x) { v.disabled = x; }) ||
        !bind_value(p.read_only_, [](auto& v, bool x) { v.read_only = x; }) ||
        !bind_value(p.allow_clear_, [](auto& v, bool x) { v.allow_clear = x; }) ||
        !bind_value(p.placeholder_, [](auto& v, String x) { v.placeholder.set(std::move(x)); }) ||
        !bind_value(p.not_found_content_, [](auto& v, String x) { v.empty_label.set(std::move(x)); }) ||
        !bind_value(p.size_, [](auto& v, ControlSize x) { v.size.set(x); }) ||
        !bind_value(p.variant_, [](auto& v, InputVariant x) { v.variant.set(x); }) ||
        !bind_value(p.status_, [](auto& v, InputStatus x) { v.status.set(x); }) ||
        !bind_value(p.list_height_, [](auto& v, float x) {
            if (!std::isfinite(x) || x <= 0) {
                throw std::invalid_argument("TreeSelect listHeight must be finite and positive");
            }
            v.height.set(x);
        })) {
        return;
    }
    for (std::size_t i = 0; i < flat.size(); ++i) {
        if (!bind_value(flat[i]->title_,
                        [i](auto& v, String x) {
                            v.model.presentation(i, x, v.model.nodes()[i].disabled, v.model.nodes()[i].visible);
                            v.items[i].title.set(std::move(x));
                        }) ||
            !bind_value(flat[i]->disabled_,
                        [i](auto& v, bool x) {
                            const auto& node = v.model.nodes()[i];
                            v.model.presentation(i, node.title, x, node.visible);
                            v.items[i].disabled.set(x);
                        }) ||
            !bind_value(flat[i]->visible_, [i](auto& v, bool x) {
                const auto& node = v.model.nodes()[i];
                v.model.presentation(i, node.title, node.disabled, x);
                v.items[i].visible.set(x);
            })) {
            return;
        }
    }
    s->display.set(s->model.label());
    s->selected.set(s->model.value().empty() ? TreeKeys{} : TreeKeys{s->model.value()});
    s->input_disabled.set(s->disabled);
    s->action_disabled.set(s->disabled || !s->visible || s->read_only);
    services.components().set_branch_active(id, s->visible);
    services.set_input_blocked(id, s->disabled || !s->visible || !window_active_);
    if (!intact(id)) {
        return;
    }
    update_theme(id);
    build.mount_slot(
        id, Content{[&] {
            MountTransaction inner{services, id};
            Theme(
                ThemeProps{}.config(s->child_theme), ThemeContent{[&] {
                    Popover(
                        PopoverProps{}
                            .trigger(PopoverTriggerMode::Manual)
                            .placement(PopoverPlacement::BottomLeft)
                            .arrow(false)
                            .open(s->popup_open)
                            .disabled(s->input_disabled)
                            .onOpenChange([this, id](bool value) { request_open(id, value); }),
                        PopoverTrigger{[&] {
                            Input(InputProps{}
                                      .value(s->display)
                                      .placeholder(s->placeholder)
                                      .readOnly(true)
                                      .disabled(s->input_disabled)
                                      .size(s->size)
                                      .variant(s->variant)
                                      .status(s->status)
                                      .ref(s->reference)
                                      .autoFocus(p.auto_focus_)
                                      .onFocus(p.on_focus_)
                                      .onBlur(p.on_blur_),
                                  {}, InputSuffix{[&] {
                                      mount_input_affix_action(
                                          services, IconSource{IconName::CloseCircleFilled}, s->action_disabled,
                                          [this, id] { choose(id, {}); }, s->clear_visible, false, false);
                                      Icon(IconProps{}.name(IconName::DownOutlined).tone(TextTone::Secondary));
                                  }});
                            if (intact(id)) {
                                const auto input = inputs->mounted_inputs().back();
                                s->selector = input.component;
                                s->selector_hit = input.interaction;
                            }
                        }},
                        PopoverSlots{
                            .content = PopoverContent{[&] {
                                const auto convert = [&](auto&& self, const std::vector<TreeSelectItem>& list,
                                                         std::size_t& index) -> std::vector<TreeItem> {
                                    std::vector<TreeItem> result;
                                    for (const auto& item : list) {
                                        const auto i = index++;
                                        auto children = self(self, item.children_, index);
                                        result.push_back(TreeItem{item.key_, s->items[i].title, std::move(children)}
                                                             .disabled(s->items[i].disabled)
                                                             .visible(s->items[i].visible));
                                    }
                                    return result;
                                };
                                std::size_t index = 0;
                                Tree(TreeProps{}
                                         .expandedKeys(s->expanded)
                                         .selectedKeys(s->selected)
                                         .disabled(s->action_disabled)
                                         .onExpand([this, id](TreeExpandInfo info) {
                                             if (!editable(id)) {
                                                 return;
                                             }
                                             const auto current = state(*services_, id);
                                             const auto callback = current->on_expand;
                                             if (!current->controlled_expanded) {
                                                 current->expanded.set(info.expanded_keys);
                                             }
                                             if (callback && intact(id)) {
                                                 callback(std::move(info));
                                             }
                                         })
                                         .onSelect([this, id](TreeSelectInfo info) { choose(id, std::move(info.key)); })
                                         .layout(LayoutStyle{}
                                                     .width(bind([width = s->width] { return dp(width.get()); }))
                                                     .height(bind([height = s->height] { return dp(height.get()); }))),
                                     convert(convert, items, index));
                                if (!intact(id)) {
                                    return;
                                }
                                s->tree = services.tree().mounted().back();
                                auto& c = runtime::require_component_build_context();
                                s->empty = c.mount_component<int>(0);
                                const auto node = c.root(s->empty);
                                services.layout().set_layout(node, layout::BoxLayout{});
                                c.on_resource_cleanup(s->empty,
                                                      [&services, node] { services.layout().remove_layout(node); });
                                c.mount_slot(s->empty, Content{[&] { Text(TextProps{}.content(s->empty_label)); }});
                                services.components().set_branch_active(s->empty, items.empty());
                                services.nodes().require(node).out_of_flow = !items.empty();
                            }}});
                    if (intact(id)) {
                        s->popover = services.popover().mounted().back();
                    }
                }});
            inner.complete = intact(id);
        }});
    if (!intact(id)) {
        return;
    }
    s->ready = true;
    if (!intact(id)) {
        return;
    }
    refresh(id);
    if (!intact(id)) {
        return;
    }
    const auto theme = services.components().theme_scope(id);
    const std::array accessors{
        &theme_runtime::ThemeScope::tree_select_colors,      &theme_runtime::ThemeScope::tree_select_metrics,
        &theme_runtime::ThemeScope::tree_select_effects,     &theme_runtime::ThemeScope::tree_select_order,
        &theme_runtime::ThemeScope::tree_select_tree_colors, &theme_runtime::ThemeScope::tree_select_tree_metrics};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s->subscriptions[i] =
            theme->capture([this, id](theme_runtime::DirtyPhase) { update_theme(id); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    on_focus_changed();
    transaction.complete = intact(id);
}

bool TreeSelectComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    const auto focus = services_->focus().state().focused;
    const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
    if (!record) {
        return false;
    }
    const auto component = record->component;
    const auto ids = mounted_;
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        const auto id = *it;
        if (!editable(id) || !descendant(*services_, component, id)) {
            continue;
        }
        const auto s = state(*services_, id);
        const bool selector = descendant(*services_, component, s->selector);
        if (event.key == input::Key::escape && s->model.open() && event.modifiers == input::KeyModifier::none) {
            if (event.action == input::KeyAction::down && !event.repeat) {
                request_open(id, false);
            }
            return true;
        }
        if (selector && event.modifiers == input::KeyModifier::none &&
            (event.key == input::Key::down || event.key == input::Key::up || event.key == input::Key::enter ||
             event.key == input::Key::space)) {
            if (event.action == input::KeyAction::down && !event.repeat) {
                request_open(id, true, true);
            }
            return true;
        }
        if (event.key == input::Key::tab && (s->model.open() || !selector) &&
            (event.modifiers == input::KeyModifier::none || event.modifiers == input::KeyModifier::shift)) {
            if (event.action == input::KeyAction::down) {
                std::vector<input::InteractionId> order;
                for (auto hit : services_->interactions().declaration_order()) {
                    const auto* r = services_->interactions().find(hit);
                    if (r && r->focusable && r->tab_stop && r->eligible &&
                        services_->interactions().branch_active(hit) &&
                        (hit == s->selector_hit || !descendant(*services_, r->component, id))) {
                        order.push_back(hit);
                    }
                }
                const auto position = std::ranges::find(order, s->selector_hit);
                std::optional<input::InteractionId> target;
                if (position != order.end() && order.size() > 1) {
                    const auto i = static_cast<std::size_t>(position - order.begin());
                    const auto next = event.modifiers == input::KeyModifier::shift
                                          ? (i + order.size() - 1) % order.size()
                                          : (i + 1) % order.size();
                    target = order[next];
                }
                if (intact(id)) {
                    services_->focus().defer_focus(target, input::FocusModality::keyboard);
                }
            }
            return true;
        }
    }
    return false;
}

void TreeSelectComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                               std::optional<input::InteractionId> hit,
                                               std::optional<input::InteractionId> origin) {
    if (event.action != input::PointerAction::up || event.button != input::PointerButton::primary || hit != origin ||
        !hit) {
        return;
    }
    const auto* record = services_->interactions().find(*hit);
    if (!record) {
        return;
    }
    const auto component = record->component;
    const auto ids = mounted_;
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        if (editable(*it)) {
            const auto s = state(*services_, *it);
            // Affix actions have their own component; never treat clear as selector activation.
            if (component == s->selector) {
                request_open(*it, !s->model.open());
                return;
            }
        }
    }
}

void TreeSelectComponentHost::on_focus_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        if (!s->ready) {
            continue;
        }
        const auto focused = services_->focus().state().focused;
        const auto* r = focused ? services_->interactions().find(*focused) : nullptr;
        const bool inside = r && descendant(*services_, r->component, id);
        const bool left = s->focused_inside && !inside;
        s->focused_inside = inside;
        if (left) {
            request_open(id, false);
        }
    }
}

void TreeSelectComponentHost::on_window_active(bool active) {
    window_active_ = active;
    on_input_boundary_changed();
}

void TreeSelectComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        refresh(id);
    }
}

void TreeSelectComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!intact(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        services_->popover().position_ancestors(id, viewport, clip);
        if (!intact(id)) {
            continue;
        }
        const auto anchor = bounds(*services_, s->selector);
        const auto available = services_->component_clip(s->selector, viewport, clip);
        const bool blocked = anchor.width <= 0 || anchor.height <= 0 ||
                             graphics::intersect_effect_bounds(anchor, available).width <= 0 ||
                             graphics::intersect_effect_bounds(anchor, available).height <= 0;
        s->geometry_blocked = blocked;
        refresh(id);
        if (!intact(id)) {
            continue;
        }
        const float width = std::max(0.0F, std::min(anchor.width, viewport.width));
        if (s->width.get() != width) {
            s->width.set(width);
            if (!intact(id)) {
                continue;
            }
            update_theme(id);
        }
        if (intact(id)) {
            services_->popover().position_window_layer(s->popover, viewport, clip);
        }
    }
}

void TreeSelectComponentHost::synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!editable(id)) {
            continue;
        }
        const auto s = state(*services_, id);
        if (!s->focus_pending || !services_->popover().snapshot(s->popover).geometry.visible) {
            continue;
        }
        s->focus_pending = false;
        const auto tree = services_->tree().snapshot(s->tree);
        std::optional<input::InteractionId> target;
        for (const auto& row : tree.rows) {
            const auto* r = services_->interactions().find(row.interaction);
            if (row.shown && r && r->eligible && r->focusable &&
                services_->interactions().branch_active(row.interaction)) {
                if (!target || row.selected) {
                    target = row.interaction;
                }
                if (row.selected) {
                    break;
                }
            }
        }
        if (target) {
            services_->focus().defer_focus(target, input::FocusModality::keyboard);
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void TreeSelect(TreeSelectProps props, std::vector<TreeSelectItem> items) {
    if (!detail::active_tree_select) {
        throw std::logic_error("TreeSelect requires an active window component context");
    }
    detail::active_tree_select->mount(props, items);
}
} // namespace ryn
