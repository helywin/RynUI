#include "component/auto_complete_component.hpp"
#include "component/auto_complete_model.hpp"
#include "component/input_component.hpp"
#include "component/input_affix_action.hpp"
#include "component/popover_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "theme/input_tokens.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
namespace {
thread_local AutoCompleteComponentHost* active_auto_complete{};

struct PrivateRegion final {
    runtime::ComponentId id;
    runtime::NodeId node;
    std::optional<runtime::ComponentId> parent;
};

struct Row final {
    runtime::NodeId node;
    runtime::ComponentId label_root;
    input::InteractionId interaction;
    runtime::SceneFragmentId hit;
    component::RetainedSurfaceId surface;
    String key;
    std::size_t index{};
    AutoCompleteRichLabel rich;
    Signal<String> label{String{}};
    Signal<runtime::SemanticForeground> color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
};

struct State final {
    explicit State(AutoCompleteModel value) : model(std::move(value)) {}

    AutoCompleteModel model;
    runtime::NodeId node;
    runtime::ComponentId input;
    runtime::ComponentId popup;
    runtime::ComponentId list;
    runtime::ComponentId clear_wrapper;
    input::InteractionId input_hit;
    input::InteractionId popup_hit;
    input::TextInputOwnerId editor;
    component::RetainedSurfaceId background;
    component::RetainedSurfaceId clear_background;
    std::vector<PrivateRegion> regions;
    runtime::ComponentId prefix_region;
    runtime::ComponentId suffix_region;
    runtime::NodeId input_node;
    std::optional<Prop<runtime::SemanticForeground>> affix_color;
    std::optional<Prop<runtime::SemanticTypography>> affix_font;
    std::shared_ptr<Scope> bindings{std::make_shared<Scope>()};
    std::uint64_t model_revision{};
    bool private_ready{};
    bool mounting{true};
    bool reconciling{};
    bool rows_dirty{};
    bool focus_was_inside{};
    bool zero_geometry{};
    Signal<String> placeholder{String{}};
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<InputVariant> variant{InputVariant::Outlined};
    Signal<InputStatus> status{InputStatus::Default};
    std::vector<runtime::ComponentId> rows;
    AutoCompleteRef reference;
    std::optional<Prop<String>> authority;
    Signal<String> display{String{}};
    Signal<bool> clear_visible{false};
    Signal<bool> action_disabled{false};
    Signal<ThemeConfig> input_theme{*std::make_unique<ThemeConfig>()};
    std::array<theme_runtime::Subscription, 5> subscriptions;
    AutoCompleteFilter filter;
    bool active_first{true};
    bool backfill{};
    bool allow_clear{};
    bool controlled_open{};
    bool open{};
    bool suppressed{};
    bool visible{};
    bool has_been_active{};
    bool closed_on_blur{};
    bool reveal_active{true};
    bool needs_layout{true};
    float list_height{256};
    float scroll{};
    runtime::Rect anchor;
    runtime::Rect available;
    runtime::Rect bounds;
    runtime::Rect clip;
    std::function<void(String)> on_change;
    std::function<void(String)> on_search;
    std::function<void(String, AutoCompleteOption)> on_select;
    std::function<void()> on_clear;
    std::function<void(bool)> on_open;
    std::function<void()> on_focus;
    std::function<void()> on_blur;
};

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

graphics::QuadInstance quad(runtime::Rect r, Color c, float radius = 0) {
    graphics::QuadInstance q;
    q.bounds = {r.x, r.y, r.width, r.height};
    q.color = {c.red(), c.green(), c.blue(), c.alpha()};
    q.corner_radius = radius;
    return q;
}

runtime::Rect intersect(runtime::Rect a, runtime::Rect b) {
    const float x = std::max(a.x, b.x);
    const float y = std::max(a.y, b.y);
    return {x, y, std::max(0.0F, std::min(a.x + a.width, b.x + b.width) - x),
            std::max(0.0F, std::min(a.y + a.height, b.y + b.height) - y)};
}

bool inside(runtime::Rect r, float x, float y) {
    return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

std::optional<input::InteractionId> tab_neighbor(WindowComponentServices& services, input::InteractionId anchor,
                                                 bool reverse) {
    std::vector<input::InteractionId> order;
    for (auto hit : services.interactions().declaration_order()) {
        const auto* record = services.interactions().find(hit);
        if (record && record->eligible && record->focusable && record->tab_stop &&
            services.interactions().branch_active(hit)) {
            order.push_back(hit);
        }
    }
    const auto found = std::ranges::find(order, anchor);
    if (found == order.end()) {
        return {};
    }
    const auto index = static_cast<std::size_t>(found - order.begin());
    return order[(index + (reverse ? order.size() - 1 : 1)) % order.size()];
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto p = std::optional{child}; p && services.components().contains(*p); p = services.components().parent(*p)) {
        if (*p == parent) {
            return true;
        }
    }
    return false;
}

// All callbacks return through this exact owner and private-region generation check.
// Retiring a damaged owner never targets a replacement that reused a store index.
State* checked(WindowComponentServices& services, runtime::ComponentId id) {
    auto& components = services.components();
    auto* state = components.state<State>(id);
    if (!state || !components.scope(id).active()) {
        return nullptr;
    }
    bool valid = services.nodes().find(state->node) && components.root(id) == state->node;
    for (const auto& region : state->regions) {
        valid = valid && components.contains(region.id) && components.scope(region.id).active() &&
                components.root(region.id) == region.node && services.nodes().find(region.node) &&
                components.parent(region.id) == region.parent && descendant(services, region.id, id);
    }
    if (state->input.valid()) {
        const auto* record = services.interactions().find(state->input_hit);
        const auto inputs = services.input_runtime()->mounted_inputs();
        valid = valid && record && record->component == state->input && record->node == state->input_node &&
                services.input_runtime()->editors().find(state->editor) &&
                std::ranges::any_of(inputs, [&](const auto& input) {
                    return input.component == state->input && input.node == state->input_node &&
                           input.interaction == state->input_hit && input.editor == state->editor;
                });
    } else if (state->private_ready) {
        valid = false;
    }
    if (state->private_ready) {
        valid = valid && state->popup.valid() && state->list.valid() && state->clear_wrapper.valid();
    }
    for (auto child : state->rows) {
        const auto* row = components.state<Row>(child);
        valid = valid && row && components.scope(child).active() && components.parent(child) == state->list &&
                components.root(child) == row->node && services.nodes().find(row->node) &&
                components.contains(row->hit);
    }
    if (!valid) {
        services.destroy(id);
        return nullptr;
    }
    return state;
}

void remember_region(WindowComponentServices& services, runtime::ComponentId owner, runtime::ComponentId child) {
    if (auto* state = checked(services, owner)) {
        state->regions.push_back({child, services.components().root(child), services.components().parent(child)});
    }
}

void forget_region(WindowComponentServices& services, runtime::ComponentId owner, runtime::ComponentId child) {
    if (auto* state = checked(services, owner)) {
        std::erase_if(state->regions, [&](const auto& region) { return descendant(services, region.id, child); });
    }
}

template <class F>
std::optional<bool> change_model(WindowComponentServices& services, runtime::ComponentId id, F operation) {
    const auto* state = checked(services, id);
    if (!state) {
        return {};
    }
    const auto revision = state->model_revision;
    auto candidate = state->model;
    const bool changed = operation(candidate);
    auto* current = checked(services, id);
    if (!current || current->model_revision != revision) {
        return {};
    }
    if (changed) {
        current->model = std::move(candidate);
        ++current->model_revision;
    }
    return changed;
}

bool display_model(WindowComponentServices& services, runtime::ComponentId id) {
    const auto* state = checked(services, id);
    if (!state) {
        return false;
    }
    auto display = state->display;
    auto value = state->model.display();
    display.set(std::move(value));
    return checked(services, id) != nullptr;
}

AutoCompleteFilter guarded_filter(WindowComponentServices& services, runtime::ComponentId id,
                                  AutoCompleteFilter filter) {
    return AutoCompleteFilter{
        [&services, id, filter = std::move(filter)](StringView query, const AutoCompleteOption& option) {
            if (!checked(services, id)) {
                return false;
            }
            const bool result = filter.accepts(query, option);
            return checked(services, id) && result;
        }};
}

void mount_content(WindowComponentServices& services, runtime::ComponentId parent, const Content& content,
                   bool mounting, std::optional<Prop<runtime::SemanticForeground>> color = {},
                   std::optional<Prop<runtime::SemanticTypography>> font = {}) {
    const auto theme = services.components().theme_scope(parent);
    const Content typed{[parent, content, color, font] {
        auto& build = runtime::require_component_build_context();
        if (color && font) {
            build.mount_slot_with_semantic_text_style(parent, content, *color, *font);
        } else {
            build.mount_slot(parent, content);
        }
    }};
    const Content themed{
        [theme, typed] { runtime::require_component_build_context().mount_slot_with_theme_scope(typed, theme); }};
    if (mounting) {
        SlotContentAccess::function(themed)();
    } else {
        services.append_slot(parent, themed);
    }
}

std::optional<runtime::ComponentId> layer(WindowComponentServices& services, runtime::ComponentId child) {
    for (auto p = std::optional{child}; p && services.components().contains(*p); p = services.components().parent(*p)) {
        if (services.components().is_window_layer_root(*p)) {
            return p;
        }
    }
    return {};
}

runtime::ComponentId group(WindowComponentServices& services, bool leaf = false) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    if (leaf) {
        services.layout().set_layout(node, layout::LeafLayout{});
    } else {
        services.layout().set_layout(node, layout::BoxLayout{});
    }
    build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
    return id;
}

void validate_height(float height) {
    if (!std::isfinite(height) || height <= 0) {
        throw std::invalid_argument("AutoComplete listHeight must be finite and positive");
    }
}
} // namespace

AutoCompleteComponentHost::AutoCompleteComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

AutoCompleteComponentHost::~AutoCompleteComponentHost() {
    services_->detach(*this);
}

void* AutoCompleteComponentHost::begin_mount() noexcept {
    ++mount_depth_;
    auto* old = active_auto_complete;
    active_auto_complete = this;
    return old;
}

void AutoCompleteComponentHost::end_mount(void* old) noexcept {
    --mount_depth_;
    active_auto_complete = static_cast<AutoCompleteComponentHost*>(old);
}

void AutoCompleteComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

AutoCompleteSnapshot AutoCompleteComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = checked(*services_, id);
    if (!s) {
        throw std::out_of_range("stale AutoComplete");
    }
    return {s->input,          s->popup,     s->rows, s->model.value(), s->model.query(), s->display.get(),
            s->model.active(), s->bounds,    s->clip, s->scroll,        s->visible,       s->model.backfilled(),
            s->open,           s->suppressed};
}

bool AutoCompleteComponentHost::editable(runtime::ComponentId id, bool composition) const {
    const auto* s = checked(*services_, id);
    if (!s || s->zero_geometry || !window_active_ || !services_->components().scope(id).active() ||
        !services_->interactions().component_active(id)) {
        return false;
    }
    const auto* record = services_->interactions().find(s->input_hit);
    const auto* editor = services_->input_runtime()->editors().find(s->editor);
    return record && record->eligible && services_->interactions().component_active(s->input) && editor &&
           !editor->disabled() && !editor->read_only() && (!composition || !editor->composition().active);
}

void AutoCompleteComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = checked(*services_, id);
    if (!s || !s->private_ready || s->mounting) {
        return;
    }
    const bool visible = s->open && !s->suppressed && !s->model.filtered().empty() && editable(id, false);
    const bool changed = visible != s->visible;
    s->visible = visible;
    if (services_->components().set_branch_active(s->popup, visible)) {
        services_->mark_scene_structure_dirty();
    }
    auto clear_visible = s->clear_visible;
    clear_visible.set(s->allow_clear && !s->display.get().empty() && editable(id, false));
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    auto action_disabled = s->action_disabled;
    action_disabled.set(!editable(id));
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    const auto t = services_->components().theme_scope(id)->snapshot().auto_complete();
    std::vector<std::pair<runtime::ComponentId, bool>> boundaries;
    const auto children = s->rows;
    for (auto child : children) {
        s = checked(*services_, id);
        if (!s) {
            return;
        }
        auto* row = services_->components().state<Row>(child);
        if (!row) {
            continue;
        }
        const auto found = std::ranges::find(s->model.options(), row->key, &AutoCompleteOption::key);
        if (found == s->model.options().end()) {
            if (services_->components().set_branch_active(child, false)) {
                services_->mark_scene_structure_dirty();
            }
            services_->interactions().set_eligible(row->interaction, false);
            boundaries.emplace_back(child, true);
            continue;
        }
        row->index = static_cast<std::size_t>(found - s->model.options().begin());
        const auto option = *found;
        const bool shown = std::ranges::find(s->model.filtered(), row->index) != s->model.filtered().end();
        boundaries.emplace_back(child, !visible || !shown || option.disabled);
        if (services_->components().set_branch_active(child, shown)) {
            services_->mark_scene_structure_dirty();
        }
        services_->interactions().set_eligible(row->interaction, visible && shown && !option.disabled);
        const bool selected = option.value == s->model.value();
        auto label = row->label;
        auto color = row->color;
        auto font = row->font;
        label.set(option.label.empty() ? option.value : option.label);
        if (!checked(*services_, id)) {
            return;
        }
        color.set(foreground(option.disabled ? t.colors.option_disabled_color
                             : selected      ? t.colors.option_selected_color
                                             : t.colors.option_color));
        if (!checked(*services_, id)) {
            return;
        }
        font.set({t.typography.font_family, selected ? t.typography.selected_font_weight : t.typography.font_weight,
                  false, t.typography.font_size, t.typography.line_height});
    }
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    if (geometry || changed) {
        s->needs_layout = true;
        s->reveal_active = true;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    } else {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
    }
    // This can synchronously blur a native child and destroy this entire control.
    const auto popup = s->popup;
    services_->set_input_blocked(popup, !visible);
    if (!checked(*services_, id)) {
        return;
    }
    for (const auto& [child, blocked] : boundaries) {
        if (services_->components().contains(child)) {
            services_->set_input_blocked(child, blocked);
            if (!checked(*services_, id)) {
                return;
            }
        }
    }
}

void AutoCompleteComponentHost::request_open(runtime::ComponentId id, bool open, bool restore_focus) {
    auto* s = checked(*services_, id);
    if (!s || !services_->components().scope(id).active() || (open && !editable(id))) {
        return;
    }
    const bool changed = s->open != open;
    const auto callback = s->on_open;
    const auto input = s->input_hit;
    if (!s->controlled_open) {
        s->open = open;
    }
    if (open) {
        s->suppressed = false;
    } else {
        if (!change_model(*services_, id,
                          [](auto& model) {
                              model.restore();
                              return true;
                          }) ||
            !display_model(*services_, id)) {
            return;
        }
    }
    refresh(id);
    if (changed && checked(*services_, id) && callback) {
        callback(open);
    }
    s = checked(*services_, id);
    if (restore_focus && s && s->input_hit == input && editable(id, false)) {
        services_->focus().defer_focus(input, input::FocusModality::keyboard);
    }
}

void AutoCompleteComponentHost::suppress(runtime::ComponentId id) {
    auto* s = checked(*services_, id);
    if (!s || s->suppressed) {
        return;
    }
    s->suppressed = true;
    s->scroll = 0;
    if (!change_model(*services_, id,
                      [](auto& model) {
                          model.restore();
                          return true;
                      }) ||
        !display_model(*services_, id)) {
        return;
    }
    request_open(id, false);
}

void AutoCompleteComponentHost::publish(runtime::ComponentId id, String value, std::optional<AutoCompleteOption> option,
                                        bool clear) {
    if (!editable(id)) {
        return;
    }
    auto* s = checked(*services_, id);
    if (!s) {
        return;
    }
    const auto previous = s->model.value();
    const auto on_change = s->on_change;
    const auto on_search = s->on_search;
    const auto on_select = s->on_select;
    const auto on_clear = s->on_clear;
    if (!change_model(*services_, id, [&](auto& model) {
            if (option || clear) {
                model.request(value);
            } else {
                model.edit(value);
            }
            return true;
        })) {
        return;
    }
    // Reflect the committed editor text before reading an owner echo; a refused controlled intent restores it.
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    auto display = s->display;
    display.set(value);
    if (!checked(*services_, id)) {
        return;
    }
    if (previous != value && on_change) {
        on_change(value);
    }
    s = checked(*services_, id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto authority = s->authority;
    if (authority) {
        auto accepted = read_prop(*authority);
        if (!change_model(*services_, id, [&](auto& model) { return model.reconcile(std::move(accepted)); })) {
            return;
        }
    }
    if (!display_model(*services_, id)) {
        return;
    }
    refresh(id);
    if (!checked(*services_, id)) {
        return;
    }
    if (option && on_select) {
        on_select(value, *option);
    } else if (!option && on_search) {
        on_search(value);
    }
    if (!checked(*services_, id)) {
        return;
    }
    if (clear && on_clear) {
        on_clear();
    }
    if (checked(*services_, id)) {
        request_open(id, !option && !clear);
    }
}

void AutoCompleteComponentHost::choose(runtime::ComponentId id, std::size_t index) {
    const auto* s = checked(*services_, id);
    if (!s || !s->visible || index >= s->model.options().size() || s->model.options()[index].disabled ||
        std::ranges::find(s->model.filtered(), index) == s->model.filtered().end()) {
        return;
    }
    auto option = s->model.options()[index];
    publish(id, option.value, option);
}

void AutoCompleteComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    auto* s = checked(*services_, id);
    if (!s) {
        return;
    }
    const auto& theme = services_->components().theme_scope(id)->snapshot();
    const auto& t = theme.auto_complete();
    auto config = std::make_unique<ThemeConfig>();
    config->input.tokens.background = t.colors.selector_background;
    config->input.tokens.hover_background = t.colors.selector_background;
    config->input.tokens.active_background = t.colors.selector_background;
    config->input.tokens.hover_border_color = t.colors.hover_border_color;
    config->input.tokens.active_border_color = t.colors.active_border_color;
    const auto& original = InputTokenAccess::get(theme).active_shadow;
    std::array<ShadowLayer, ShadowList::capacity> shadow{};
    std::copy(original.layers().begin(), original.layers().end(), shadow.begin());
    for (std::size_t i = 0; i < original.size(); ++i) {
        shadow[i].color = t.colors.active_outline_color;
    }
    config->input.tokens.active_shadow = ShadowList{std::span<const ShadowLayer>{shadow.data(), original.size()}};
    auto input_theme = s->input_theme;
    input_theme.set(std::move(*config));
    s = checked(*services_, id);
    if (!s) {
        return;
    }
    if (services_->components().set_window_layer(s->popup, t.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    refresh(id, geometry);
}

void AutoCompleteComponentHost::mount_label(runtime::ComponentId id, runtime::ComponentId child) {
    if (!checked(*services_, id)) {
        return;
    }
    auto* row = services_->components().state<Row>(child);
    if (!row) {
        return;
    }
    auto& build = runtime::require_component_build_context();
    row->label_root = group(*services_);
    const auto label_root = row->label_root;
    remember_region(*services_, id, label_root);
    build.on_resource_cleanup(label_root, [services = services_, label_root] {
        services->interactions().set_input_blocked(label_root, false);
    });
    const auto rich = row->rich;
    const auto label_signal = row->label;
    const auto color = row->color;
    const auto font = row->font;
    build.mount_slot_with_semantic_text_style(label_root, Content{[rich, label_signal] {
                                                  if (const auto* label = rich.content()) {
                                                      SlotContentAccess::function (*label)();
                                                  } else {
                                                      Text(TextProps{}.content(label_signal));
                                                  }
                                              }},
                                              color, font);
    if (!checked(*services_, id)) {
        return;
    }
    // Rich native children remain pointer operable, while the compound control has one Tab entry.
    for (auto hit : services_->interactions().declaration_order()) {
        const auto* r = services_->interactions().find(hit);
        if (r && descendant(*services_, r->component, child)) {
            services_->interactions().set_tab_stop(hit, false);
        }
    }
}

void AutoCompleteComponentHost::mount_row(runtime::ComponentId id, std::size_t index) {
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    auto* live = checked(services, id);
    if (!live || index >= live->model.options().size()) {
        return;
    }
    const auto option = live->model.options()[index];
    const auto popup_hit = live->popup_hit;
    const auto child = build.mount_component<Row>();
    auto& row = build.state<Row>(child);
    row.node = build.root(child);
    row.index = index;
    row.key = option.key;
    row.rich = option.rich_label;
    services.layout().set_layout(row.node, layout::BoxLayout{});
    services.nodes().require(row.node).clip_content = true;
    row.interaction = services.interactions().create({child, row.node, popup_hit, false, false, {}, false, false});
    input::InteractionHandlers handlers;
    handlers.target = [this, id, child](input::PointerDispatchContext& e) {
        const auto* s = checked(*services_, id);
        const auto* row = services_->components().state<Row>(child);
        if (!s || !row || !s->visible || !editable(id)) {
            return;
        }
        const auto index = row->index;
        if (e.kind() == input::PointerEventKind::enter || e.kind() == input::PointerEventKind::move) {
            auto* live = checked(*services_, id);
            if (live->model.activate(index)) {
                refresh(id, false);
            }
            return;
        }
        if (e.event().button != input::PointerButton::primary) {
            return;
        }
        if (e.kind() == input::PointerEventKind::down) {
            (void)e.capture_pointer();
            e.stop_propagation();
        }
        if (e.kind() == input::PointerEventKind::up) {
            (void)e.release_pointer_capture();
            const bool hit =
                e.press_origin() == row->interaction &&
                inside(intersect(services_->nodes().require(row->node).bounds, s->clip), e.event().x, e.event().y);
            e.stop_propagation();
            if (hit) {
                choose(id, index);
            }
        }
    };
    services.interactions().set_handlers(row.interaction, std::move(handlers));
    row.hit = build.register_scene_fragment(child, runtime::SceneFragmentPlacement::before_children);
    services.scene_composer().set_fragment(row.hit, {}, row.interaction);
    const auto body = build.register_scene_fragment(child, runtime::SceneFragmentPlacement::before_children);
    graphics::QuadInstance initial;
    initial.opacity = 0;
    component::RetainedSurfaceEffects effects;
    effects.focus_enabled = false;
    row.surface = services.surfaces().create_surface(child, row.node, body, std::span{&initial, 1}, effects);
    build.on_resource_cleanup(
        child, [&services, child, node = row.node, hit = row.interaction, fragment = row.hit, surface = row.surface] {
            services.interactions().set_input_blocked(child, false);
            services.pointer().cancel_interaction(hit);
            services.interactions().remove(hit);
            services.surfaces().destroy(surface);
            services.scene_composer().remove_fragment(fragment);
            services.layout().remove_layout(node);
        });
    live = checked(services, id);
    if (!live) {
        return;
    }
    live->rows.push_back(child);
    remember_region(services, id, child);
    build.mount_slot(child, Content{[this, id, child] { mount_label(id, child); }});
    (void)checked(services, id);
}

void AutoCompleteComponentHost::reconcile_rows(runtime::ComponentId id) {
    auto* s = checked(*services_, id);
    if (!s) {
        return;
    }
    if (s->mounting || s->reconciling) {
        s->rows_dirty = true;
        return;
    }
    s->reconciling = true;
    do {
        s->rows_dirty = false;
        const auto revision = s->model_revision;
        const auto options = s->model.options();
        auto previous = s->rows;
        std::vector<runtime::ComponentId> next;
        bool restart = false;
        for (std::size_t i = 0; i < options.size(); ++i) {
            const auto option = options[i];
            const auto found = std::ranges::find_if(previous, [&](auto child) {
                const auto* row = services_->components().state<Row>(child);
                return row && row->key == option.key;
            });
            if (found == previous.end()) {
                const auto list = s->list;
                mount_content(*services_, list, Content{[this, id, i] { mount_row(id, i); }}, mount_depth_ != 0);
                s = checked(*services_, id);
                if (!s) {
                    return;
                }
                next.push_back(s->rows.back());
            } else {
                const auto child = *found;
                auto* row = services_->components().state<Row>(child);
                row->index = i;
                if (row->rich != option.rich_label) {
                    const auto label = row->label_root;
                    row->rich = option.rich_label;
                    row->label_root = {};
                    forget_region(*services_, id, label);
                    services_->set_input_blocked(label, true);
                    if (!checked(*services_, id)) {
                        return;
                    }
                    services_->destroy(label);
                    if (!checked(*services_, id)) {
                        return;
                    }
                    mount_content(*services_, child, Content{[this, id, child] { mount_label(id, child); }},
                                  mount_depth_ != 0);
                    s = checked(*services_, id);
                    if (!s) {
                        return;
                    }
                }
                next.push_back(child);
                previous.erase(found);
            }
            s = checked(*services_, id);
            if (!s) {
                return;
            }
            if (s->model_revision != revision) {
                restart = true;
                break;
            }
        }
        if (restart) {
            s->rows_dirty = true;
            continue;
        }
        s->rows = std::move(next);
        for (auto child : previous) {
            forget_region(*services_, id, child);
            services_->set_input_blocked(child, true);
            if (!checked(*services_, id)) {
                return;
            }
            services_->destroy(child);
            if (!checked(*services_, id)) {
                return;
            }
        }
        refresh(id);
        s = checked(*services_, id);
        if (!s) {
            return;
        }
    } while (s->rows_dirty);
    s->reconciling = false;
}

void AutoCompleteComponentHost::mount(const AutoCompleteProps& props, const std::optional<AutoCompletePrefix>& prefix,
                                      const std::optional<AutoCompleteSuffix>& suffix) {
    auto& services = *services_;
    auto* inputs = services.input_runtime();
    if (!inputs) {
        throw std::logic_error("AutoComplete requires window Input host");
    }
    if ((props.value_ && props.default_value_) || (props.open_ && props.default_open_)) {
        throw std::invalid_argument("AutoComplete controlled/default props conflict");
    }
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<State>(AutoCompleteModel{
        {}, props.default_value_.value_or(String{}), props.value_.has_value(), AutoCompleteFilter{}, true});
    auto& s = build.state<State>(id);
    s.node = build.root(id);
    const auto node = s.node;
    // connect_binding owns an observer only after its first synchronous read/apply.
    // Keep this Scope alive even if that read or apply retires the component record.
    const auto bindings = s.bindings;
    build.on_resource_cleanup(id, [&services, node, bindings] {
        bindings->dispose();
        services.layout().remove_layout(node);
    });
    build.on_resource_cleanup(id, [&services, id] { services.interactions().set_input_blocked(id, false); });
    s.reference = props.ref_.value_or(AutoCompleteRef{});
    s.authority = props.value_;
    s.open = props.default_open_.value_or(false);
    s.controlled_open = props.open_.has_value();
    s.on_change = props.on_change_;
    s.on_search = props.on_search_;
    s.on_select = props.on_select_;
    s.on_clear = props.on_clear_;
    s.on_open = props.on_open_;
    s.on_focus = props.on_focus_;
    s.on_blur = props.on_blur_;
    s.display.set(s.model.display());
    runtime::connect_layout_style(*bindings, props.layout_, node, services.nodes(), services.dirty(),
                                  [&services, id] { return checked(services, id) != nullptr; });
    if (!checked(services, id)) {
        return;
    }
    build.mount_slot(
        id, Content{[&] {
            Theme(ThemeProps{}.config(s.input_theme), ThemeContent{[&] {
                      std::optional<InputPrefix> input_prefix;
                      if (prefix) {
                          input_prefix = InputPrefix{[&] {
                              s.affix_color = runtime::require_component_build_context().semantic_foreground();
                              s.affix_font = runtime::require_component_build_context().semantic_typography();
                              s.prefix_region = group(services);
                              remember_region(services, id, s.prefix_region);
                          }};
                      }
                      // Capture the main Input before rich options can mount further editors.
                      const auto count = inputs->mounted_inputs().size();
                      Input(InputProps{}
                                .value(s.display)
                                .placeholder(s.placeholder)
                                .disabled(s.disabled)
                                .readOnly(s.read_only)
                                .size(s.size)
                                .variant(s.variant)
                                .status(s.status)
                                .ref(s.reference)
                                .autoFocus(props.auto_focus_)
                                .onChange([this, id](String text) { publish(id, std::move(text), {}); })
                                .onFocus([this, id] {
                                    const auto* s = checked(*services_, id);
                                    if (!s) {
                                        return;
                                    }
                                    const auto callback = s->on_focus;
                                    if (callback) {
                                        callback();
                                    }
                                    (void)checked(*services_, id);
                                })
                                .onBlur([this, id] {
                                    auto* s = checked(*services_, id);
                                    if (!s || !services_->components().scope(id).active()) {
                                        return;
                                    }
                                    const auto callback = s->on_blur;
                                    const auto focus = services_->focus().state().focused;
                                    const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
                                    const bool internal = record && descendant(*services_, record->component, s->popup);
                                    if (!internal) {
                                        s->closed_on_blur = true;
                                        request_open(id, false);
                                    }
                                    if (checked(*services_, id) && callback) {
                                        callback();
                                    }
                                    (void)checked(*services_, id);
                                }),
                            input_prefix, InputSuffix{[&] {
                                s.affix_color = runtime::require_component_build_context().semantic_foreground();
                                s.affix_font = runtime::require_component_build_context().semantic_typography();
                                s.clear_wrapper = group(services);
                                remember_region(services, id, s.clear_wrapper);
                                const auto node = build.root(s.clear_wrapper);
                                const auto fragment = build.register_scene_fragment(
                                    s.clear_wrapper, runtime::SceneFragmentPlacement::before_children);
                                graphics::QuadInstance initial;
                                initial.opacity = 0;
                                component::RetainedSurfaceEffects effects;
                                effects.focus_enabled = false;
                                s.clear_background = services.surfaces().create_surface(
                                    s.clear_wrapper, node, fragment, std::span{&initial, 1}, effects);
                                build.on_resource_cleanup(s.clear_wrapper, [&services, surface = s.clear_background] {
                                    services.surfaces().destroy(surface);
                                });
                                build.mount_slot(s.clear_wrapper, Content{[&] {
                                                     mount_input_affix_action(
                                                         services, IconSource{IconName::CloseCircleFilled},
                                                         s.action_disabled, [this, id] { publish(id, {}, {}, true); },
                                                         s.clear_visible, false, false);
                                                 }});
                                if (suffix) {
                                    s.suffix_region = group(services);
                                    remember_region(services, id, s.suffix_region);
                                }
                            }});
                      const auto input = inputs->mounted_inputs()[count];
                      s.input = input.component;
                      s.input_node = input.node;
                      s.input_hit = input.interaction;
                      s.editor = input.editor;
                      remember_region(services, id, s.input);
                      for (auto hit : services.interactions().declaration_order()) {
                          const auto* record = services.interactions().find(hit);
                          if (record && hit != s.input_hit && record->focusable &&
                              descendant(services, record->component, s.input)) {
                              throw std::invalid_argument("AutoComplete prefix/suffix must be non-interactive");
                          }
                      }
                      inputs->configure_form_reset(s.input, [this, id, editor = s.editor] {
                          auto* s = checked(*services_, id);
                          if (!s || s->editor != editor || !services_->components().scope(id).active()) {
                              return;
                          }
                          const auto authority = s->authority;
                          if (authority) {
                              auto accepted = read_prop(*authority);
                              if (!change_model(*services_, id,
                                                [&](auto& model) { return model.reconcile(std::move(accepted)); })) {
                                  return;
                              }
                          }
                          if (!change_model(*services_, id,
                                            [](auto& model) {
                                                model.restore();
                                                return true;
                                            }) ||
                              !display_model(*services_, id)) {
                              return;
                          }
                          s = checked(*services_, id);
                          if (!s || s->editor != editor) {
                              return;
                          }
                          s->scroll = 0;
                          refresh(id);
                      });
                  }});
            s.popup = group(services, true);
            remember_region(services, id, s.popup);
            const auto popup_node = build.root(s.popup);
            services.nodes().require(popup_node).clip_content = true;
            services.components().set_window_layer(s.popup, 1050);
            s.popup_hit = services.interactions().create({s.popup, popup_node, {}, true, false, {}, false, false});
            const auto fragment =
                build.register_scene_fragment(s.popup, runtime::SceneFragmentPlacement::before_children);
            graphics::QuadInstance initial;
            initial.opacity = 0;
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = false;
            s.background =
                services.surfaces().create(s.popup, popup_node, fragment, s.popup_hit, std::span{&initial, 1}, effects);
            build.on_resource_cleanup(s.popup, [&services, popup = s.popup, hit = s.popup_hit, surface = s.background] {
                services.interactions().set_input_blocked(popup, false);
                services.pointer().cancel_interaction(hit);
                services.interactions().remove(hit);
                services.surfaces().destroy(surface);
            });
            build.mount_slot(s.popup, Content{[&] {
                                 s.list = group(services, true);
                                 remember_region(services, id, s.list);
                                 services.nodes().require(build.root(s.list)).clip_content = true;
                             }});
        }});
    s.private_ready = true;
    services.layout().set_layout(
        node,
        layout::ComponentLayout{[this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                                    const auto* live = checked(*services_, id);
                                    return live ? engine.measure_child(live->input_node, limits) : runtime::Size{};
                                },
                                [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                                    if (const auto* live = checked(*services_, id)) {
                                        engine.place_child(live->input_node, rect);
                                    }
                                }});
    mounted_.push_back(id);
    auto& scope = *bindings;
    const auto connect = [&](const auto& prop, auto apply) {
        if (!checked(services, id)) {
            return false;
        }
        connect_prop(scope, prop, [&services, id, apply = std::move(apply)](auto value) {
            if (checked(services, id)) {
                apply(std::move(value));
            }
            (void)checked(services, id);
        });
        return checked(services, id) != nullptr;
    };
    if (!connect(props.options_, [this, id](std::vector<AutoCompleteOption> value) {
            const auto changed =
                change_model(*services_, id, [&](auto& model) { return model.set_options(std::move(value)); });
            if (changed && *changed && display_model(*services_, id)) {
                reconcile_rows(id);
            }
        })) {
        return;
    }
    if (props.value_) {
        if (!connect(*props.value_, [this, id](String value) {
                const auto changed =
                    change_model(*services_, id, [&](auto& model) { return model.reconcile(std::move(value)); });
                if (changed && *changed && display_model(*services_, id)) {
                    refresh(id);
                }
            })) {
            return;
        }
    }
    if (props.open_) {
        if (!connect(*props.open_, [this, id](bool open) {
                auto* s = checked(*services_, id);
                if (!s) {
                    return;
                }
                const bool changed = s->open != open;
                s->open = open;
                if (changed && open) {
                    s->suppressed = false;
                }
                if (!open) {
                    if (!change_model(*services_, id,
                                      [](auto& model) {
                                          model.restore();
                                          return true;
                                      }) ||
                        !display_model(*services_, id)) {
                        return;
                    }
                }
                refresh(id);
            })) {
            return;
        }
    }
    if (!connect(props.filter_, [this, id](AutoCompleteFilter filter) {
            auto* s = checked(*services_, id);
            if (!s) {
                return;
            }
            const bool active_first = s->active_first;
            const auto guarded = guarded_filter(*services_, id, filter);
            if (!change_model(*services_, id, [&](auto& model) {
                    model.configure(guarded, active_first);
                    return true;
                })) {
                return;
            }
            s = checked(*services_, id);
            if (!s) {
                return;
            }
            s->filter = std::move(filter);
            if (display_model(*services_, id)) {
                refresh(id);
            }
        })) {
        return;
    }
    if (!connect(props.active_first_, [this, id](bool value) {
            auto* s = checked(*services_, id);
            if (!s) {
                return;
            }
            const auto filter = guarded_filter(*services_, id, s->filter);
            if (!change_model(*services_, id, [&](auto& model) {
                    model.configure(filter, value);
                    return true;
                })) {
                return;
            }
            s = checked(*services_, id);
            if (!s) {
                return;
            }
            s->active_first = value;
            if (display_model(*services_, id)) {
                refresh(id);
            }
        })) {
        return;
    }
    if (!connect(props.backfill_, [this, id](bool value) {
            if (auto* s = checked(*services_, id)) {
                s->backfill = value;
                if (!value) {
                    if (!change_model(*services_, id,
                                      [](auto& model) {
                                          model.restore();
                                          return true;
                                      }) ||
                        !display_model(*services_, id)) {
                        return;
                    }
                }
                refresh(id, false);
            }
        })) {
        return;
    }
    if (!connect(props.allow_clear_, [this, id](bool value) {
            if (auto* s = checked(*services_, id)) {
                s->allow_clear = value;
                refresh(id, false);
            }
        })) {
        return;
    }
    if (!connect(props.disabled_, [this, id](bool value) {
            auto* s = checked(*services_, id);
            if (!s) {
                return;
            }
            auto signal = s->disabled;
            signal.set(value);
            if (!checked(*services_, id)) {
                return;
            }
            if (value) {
                suppress(id);
            }
            refresh(id);
        })) {
        return;
    }
    if (!connect(props.read_only_, [this, id](bool value) {
            auto* s = checked(*services_, id);
            if (!s) {
                return;
            }
            auto signal = s->read_only;
            signal.set(value);
            if (!checked(*services_, id)) {
                return;
            }
            if (value) {
                suppress(id);
            }
            refresh(id);
        })) {
        return;
    }
    if (!connect(props.list_height_, [this, id](float value) {
            validate_height(value);
            if (auto* s = checked(*services_, id)) {
                s->list_height = value;
                refresh(id);
            }
        })) {
        return;
    }
    const auto mirror = [&](const auto& prop, auto member) {
        return connect(prop, [&services, id, member](auto value) {
            if (const auto* state = checked(services, id)) {
                auto signal = state->*member;
                signal.set(std::move(value));
            }
        });
    };
    if (!mirror(props.placeholder_, &State::placeholder) || !mirror(props.size_, &State::size) ||
        !mirror(props.variant_, &State::variant) || !mirror(props.status_, &State::status)) {
        return;
    }
    update_theme(id, true);
    if (!checked(services, id)) {
        return;
    }
    const auto theme = services.components().theme_scope(id);
    const std::array accessors{
        &theme_runtime::ThemeScope::auto_complete_colors, &theme_runtime::ThemeScope::auto_complete_metrics,
        &theme_runtime::ThemeScope::auto_complete_typography, &theme_runtime::ThemeScope::auto_complete_effects,
        &theme_runtime::ThemeScope::auto_complete_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        auto subscription = theme->capture(
            [this, id, geometry = i == 1 || i == 2](theme_runtime::DirtyPhase) { update_theme(id, geometry); },
            [theme, accessor = accessors[i], shadow = i == 0] {
                (void)(theme.get()->*accessor)();
                if (shadow) {
                    (void)theme->input_shadows();
                }
            });
        auto* live = checked(services, id);
        if (!live) {
            return;
        }
        live->subscriptions[i] = std::move(subscription);
    }
    for (const auto& [region, content] : std::array{
             std::pair{s.prefix_region, prefix ? Content{SlotContentAccess::function(*prefix)} : Content{[] {}}},
             std::pair{s.suffix_region, suffix ? Content{SlotContentAccess::function(*suffix)} : Content{[] {}}}}) {
        if (region.valid()) {
            const auto* live = checked(services, id);
            if (!live) {
                return;
            }
            mount_content(services, region, content, true, live->affix_color, live->affix_font);
            if (!checked(services, id)) {
                return;
            }
        }
    }
    auto* live = checked(services, id);
    if (!live) {
        return;
    }
    for (auto hit : services.interactions().declaration_order()) {
        const auto* record = services.interactions().find(hit);
        if (record && hit != live->input_hit && record->focusable &&
            descendant(services, record->component, live->input)) {
            throw std::invalid_argument("AutoComplete prefix/suffix must be non-interactive");
        }
    }
    live->mounting = false;
    reconcile_rows(id);
    (void)checked(services, id);
}

void AutoCompleteComponentHost::on_window_active(bool value) {
    window_active_ = value;
    on_input_boundary_changed();
}

void AutoCompleteComponentHost::on_input_boundary_changed() {
    if (mount_depth_) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        const auto* s = checked(*services_, id);
        if (s && s->has_been_active && !editable(id, false)) {
            suppress(id);
        }
        refresh(id, false);
    }
}

void AutoCompleteComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = checked(*services_, id);
        if (!s) {
            continue;
        }
        s->closed_on_blur = false;
        const auto focus = services_->focus().state().focused;
        const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
        const bool focus_inside = record && descendant(*services_, record->component, id);
        const bool exited = s->focus_was_inside && !focus_inside;
        s->focus_was_inside = focus_inside;
        if (exited) {
            request_open(id, false);
        }
        s = checked(*services_, id);
        if (!s) {
            continue;
        }
        const bool active = editable(id, false);
        if (active) {
            s->has_been_active = true;
        }
        if (s->has_been_active && !active) {
            suppress(id);
        }
        s = checked(*services_, id);
        if (s) {
            auto action_disabled = s->action_disabled;
            action_disabled.set(!editable(id));
            s = checked(*services_, id);
            if (!s) {
                continue;
            }
            const bool visible = s->open && !s->suppressed && !s->model.filtered().empty() && editable(id, false);
            if (visible != s->visible) {
                refresh(id);
            }
        }
    }
}

void AutoCompleteComponentHost::validate_private_regions() {
    const auto ids = mounted_;
    for (auto id : ids) {
        (void)checked(*services_, id);
    }
}

bool AutoCompleteComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.action != input::KeyAction::down || !window_active_) {
        return false;
    }
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    if (!record) {
        return false;
    }
    const auto component = record->component;
    const auto ids = mounted_;
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        const auto id = *it;
        auto* s = checked(*services_, id);
        if (!s || !editable(id) || !descendant(*services_, component, id)) {
            continue;
        }
        const bool main = focused == s->input_hit;
        s->focus_was_inside = true;
        const auto session = services_->input_runtime()->sessions().active();
        const auto* editor = services_->input_runtime()->editors().find(session.owner);
        if (editor && editor->composition().active) {
            return false;
        }
        using input::Key;
        if (event.key == Key::tab) {
            const auto target =
                tab_neighbor(*services_, s->input_hit, input::has_modifier(event.modifiers, input::KeyModifier::shift));
            request_open(id, false);
            if (checked(*services_, id) && target) {
                services_->focus().defer_focus(target, input::FocusModality::keyboard);
            }
            return true;
        }
        if (!main && layer(*services_, component) != std::optional{s->popup}) {
            continue;
        }
        if (event.key == Key::escape && s->visible) {
            for (const auto& entry : services_->components().paint_traversal()) {
                const auto child = entry.component;
                if (child == s->popup || !services_->components().is_window_layer_root(child) ||
                    !descendant(*services_, child, id) || !services_->interactions().component_active(child)) {
                    continue;
                }
                const auto owner = services_->components().parent(child);
                if (owner && descendant(*services_, component, *owner)) {
                    return false;
                }
            }
            if (services_->popover().owns_escape(id, component)) {
                return false;
            }
            services_->focus().defer_focus(focused, input::FocusModality::keyboard);
            request_open(id, false, !main);
            return true;
        }
        if (!main || event.modifiers != input::KeyModifier::none) {
            return false;
        }
        if (event.key == Key::enter) {
            if (s->visible && s->model.active()) {
                services_->focus().defer_focus(focused, input::FocusModality::keyboard);
                if (!event.repeat) {
                    choose(id, *s->model.active());
                }
                return true;
            }
            return false;
        }
        if (event.key != Key::up && event.key != Key::down) {
            return false;
        }
        const bool visible = s->visible;
        const auto input = s->input_hit;
        services_->focus().defer_focus(focused, input::FocusModality::keyboard);
        request_open(id, true);
        s = checked(*services_, id);
        if (!s || s->input_hit != input || !editable(id) || !s->visible) {
            return true;
        }
        if (visible || !s->model.active()) {
            s->model.move(event.key == Key::up ? -1 : 1);
        }
        if (s->backfill) {
            s->model.backfill();
            if (!display_model(*services_, id)) {
                return true;
            }
        }
        refresh(id);
        return true;
    }
    return false;
}

void AutoCompleteComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                                 std::optional<input::InteractionId> hit,
                                                 std::optional<input::InteractionId> origin) {
    if (event.action != input::PointerAction::down && event.action != input::PointerAction::up) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = checked(*services_, id);
        if (!s) {
            continue;
        }
        const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
        const bool own = record && descendant(*services_, record->component, id);
        if (event.action == input::PointerAction::down) {
            const bool closed = s->closed_on_blur;
            s->closed_on_blur = false;
            if (!own && s->visible && !closed) {
                request_open(id, false);
            }
        } else if (event.button == input::PointerButton::primary && hit == s->input_hit && hit == origin) {
            request_open(id, true);
        }
    }
}

bool AutoCompleteComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
    if (!record) {
        return false;
    }
    const auto owner = layer(*services_, record->component);
    for (auto id : mounted_) {
        auto* s = checked(*services_, id);
        if (!s || !s->visible || owner != std::optional{s->popup} || !inside(s->bounds, event.x, event.y)) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().auto_complete();
        const float content = static_cast<float>(s->model.filtered().size()) * t.metrics.option_height;
        const float next = std::clamp(s->scroll - event.delta_y * t.metrics.option_height * 3, 0.0F,
                                      std::max(0.0F, content - s->clip.height));
        if (next != s->scroll) {
            s->scroll = next;
            s->reveal_active = false;
            s->needs_layout = true;
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
        }
        return true;
    }
    return false;
}

void AutoCompleteComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        position_window_layer(id, viewport, clip);
    }
}

void AutoCompleteComponentHost::position_window_layer(runtime::ComponentId id, runtime::Size viewport,
                                                      runtime::Rect clip) {
    if (std::ranges::find(positioning_, id) != positioning_.end()) {
        return;
    }
    positioning_.push_back(id);

    struct Guard {
        std::vector<runtime::ComponentId>& stack;

        ~Guard() {
            stack.pop_back();
        }
    } guard{positioning_};

    services_->popover().position_ancestors(id, viewport, clip);
    auto* s = checked(*services_, id);
    if (!s) {
        return;
    }
    const auto& placed = services_->nodes().require(s->node);
    if (placed.place_count != 0) {
        const bool zero = placed.bounds.width <= 0 || placed.bounds.height <= 0;
        if (zero != s->zero_geometry) {
            s->zero_geometry = zero;
            if (zero) {
                suppress(id);
            }
            if (!checked(*services_, id)) {
                return;
            }
            services_->set_input_blocked(id, zero);
            s = checked(*services_, id);
            if (!s) {
                return;
            }
        }
    }
    if (!s->visible) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().auto_complete();
    const auto& m = t.metrics;
    auto anchor = services_->nodes().require(s->node).bounds;
    const auto translation = services_->nodes().require(s->node).translation;
    anchor.x += translation.x;
    anchor.y += translation.y;
    const auto available = services_->component_clip(s->popup, viewport, clip);
    const auto popup_node = services_->components().root(s->popup);
    auto& engine = services_->layout();
    if (!s->needs_layout && s->anchor == anchor && s->available == available &&
        services_->nodes().require(popup_node).measure_generation == engine.generation()) {
        return;
    }
    const auto filtered = s->model.filtered();
    const float content = static_cast<float>(filtered.size()) * m.option_height;
    const float wanted = std::min(s->list_height, content) + 2 * m.popup_padding;
    const float below =
        std::clamp(available.y + available.height - anchor.y - anchor.height - m.gap, 0.0F, available.height);
    const float above = std::clamp(anchor.y - m.gap - available.y, 0.0F, available.height);
    const bool use_above = below < wanted && above > below;
    const float height = std::min(wanted, use_above ? above : below);
    const float width = std::min(anchor.width, available.width);
    s->bounds = {std::clamp(anchor.x, available.x, available.x + available.width - width),
                 std::clamp(use_above ? anchor.y - m.gap - height : anchor.y + anchor.height + m.gap, available.y,
                            available.y + available.height - height),
                 width, height};
    const float inset = std::min(m.popup_padding, std::min(width, height) / 2);
    s->clip = {s->bounds.x + inset, s->bounds.y + inset, std::max(0.0F, width - 2 * inset),
               std::max(0.0F, height - 2 * inset)};
    s->scroll = std::clamp(s->scroll, 0.0F, std::max(0.0F, content - s->clip.height));
    if (s->reveal_active && s->model.active()) {
        const auto it = std::ranges::find(filtered, *s->model.active());
        if (it != filtered.end()) {
            const float top = static_cast<float>(it - filtered.begin()) * m.option_height;
            s->scroll = s->clip.height < m.option_height
                            ? top
                            : std::clamp(s->scroll, std::max(0.0F, top + m.option_height - s->clip.height), top);
        }
    }
    s->reveal_active = false;
    (void)engine.measure_child(popup_node, layout::Constraints::fixed(width, height));
    engine.place_child(popup_node, s->bounds);
    const auto list_node = services_->components().root(s->list);
    (void)engine.measure_child(list_node, layout::Constraints::fixed(s->clip.width, s->clip.height));
    engine.place_child(list_node, s->clip);
    float y = s->clip.y - s->scroll;
    for (auto index : filtered) {
        const auto* row = services_->components().state<Row>(s->rows[index]);
        const float px = std::min(m.option_padding_inline, s->clip.width / 2);
        engine.set_layout(row->node, layout::BoxLayout{{px, m.option_padding_block, px, m.option_padding_block}});
        (void)engine.measure_child(row->node, layout::Constraints::fixed(s->clip.width, m.option_height));
        engine.place_child(row->node, {s->clip.x, y, s->clip.width, m.option_height});
        y += m.option_height;
    }
    s->needs_layout = false;
    s->anchor = anchor;
    s->available = available;
}

void AutoCompleteComponentHost::synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) {
    for (auto id : mounted_) {
        const auto* s = checked(*services_, id);
        if (!s) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().auto_complete();
        const auto clear_bounds = services_->nodes().require(services_->components().root(s->clear_wrapper)).bounds;
        auto clear = quad(clear_bounds, t.colors.clear_background);
        clear.opacity = s->clear_visible.get() ? 1.0F : 0.0F;
        (void)services_->surfaces().update_surface(s->clear_background, std::span{&clear, 1});
        if (!s->visible) {
            continue;
        }
        const float radius = std::min({t.metrics.border_radius, s->bounds.width / 2, s->bounds.height / 2});
        auto background = quad(s->bounds, t.colors.popup_background, radius);
        (void)services_->surfaces().update_surface(s->background, std::span{&background, 1});
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.shape = {{s->bounds.x, s->bounds.y, s->bounds.width, s->bounds.height}, radius};
        effects.shadows = t.shadow;
        effects.ancestor_clip = graphics::EffectClip{1, s->available};
        (void)services_->surfaces().update_effects(s->background, effects);
        for (auto child : s->rows) {
            const auto* row = services_->components().state<Row>(child);
            const bool shown = services_->components().branch_active(child);
            const bool selected = s->model.options()[row->index].value == s->model.value();
            const bool active = s->model.active() == row->index;
            auto fill =
                quad(intersect(services_->nodes().require(row->node).bounds, s->clip),
                     selected ? t.colors.option_selected_background : t.colors.option_active_background, radius);
            fill.opacity = shown && (selected || active) ? 1.0F : 0.0F;
            (void)services_->surfaces().update_surface(row->surface, std::span{&fill, 1});
            services_->scene_composer().set_fragment(row->hit, {}, row->interaction, s->clip);
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void AutoComplete(AutoCompleteProps props, std::optional<AutoCompletePrefix> prefix,
                  std::optional<AutoCompleteSuffix> suffix) {
    if (!detail::active_auto_complete) {
        throw std::logic_error("AutoComplete requires window component services");
    }
    detail::active_auto_complete->mount(props, prefix, suffix);
}
} // namespace ryn
