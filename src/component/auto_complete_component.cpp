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

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto p = std::optional{child}; p && services.components().contains(*p); p = services.components().parent(*p)) {
        if (*p == parent) {
            return true;
        }
    }
    return false;
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
    const auto* s = services_->components().state<State>(id);
    if (!s) {
        throw std::out_of_range("stale AutoComplete");
    }
    return {s->input,          s->popup,     s->rows, s->model.value(), s->model.query(), s->display.get(),
            s->model.active(), s->bounds,    s->clip, s->scroll,        s->visible,       s->model.backfilled(),
            s->open,           s->suppressed};
}

bool AutoCompleteComponentHost::editable(runtime::ComponentId id, bool composition) const {
    const auto* s = services_->components().state<State>(id);
    if (!s || !window_active_ || !services_->components().scope(id).active() ||
        !services_->interactions().component_active(id)) {
        return false;
    }
    const auto* record = services_->interactions().find(s->input_hit);
    const auto* editor = services_->input_runtime()->editors().find(s->editor);
    return record && record->eligible && services_->interactions().component_active(s->input) && editor &&
           !editor->disabled() && !editor->read_only() && (!composition || !editor->composition().active);
}

void AutoCompleteComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<State>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const bool visible = s->open && !s->suppressed && !s->model.filtered().empty() && editable(id, false);
    const bool changed = visible != s->visible;
    s->visible = visible;
    if (services_->components().set_branch_active(s->popup, visible)) {
        services_->mark_scene_structure_dirty();
    }
    s->clear_visible.set(s->allow_clear && !s->display.get().empty() && editable(id, false));
    s->action_disabled.set(!editable(id));
    const auto& t = services_->components().theme_scope(id)->snapshot().auto_complete();
    std::vector<std::pair<runtime::ComponentId, bool>> boundaries;
    for (auto child : s->rows) {
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
        const auto& option = *found;
        const bool shown = std::ranges::find(s->model.filtered(), row->index) != s->model.filtered().end();
        boundaries.emplace_back(child, !visible || !shown || option.disabled);
        if (services_->components().set_branch_active(child, shown)) {
            services_->mark_scene_structure_dirty();
        }
        services_->interactions().set_eligible(row->interaction, visible && shown && !option.disabled);
        row->label.set(option.label.empty() ? option.value : option.label);
        const bool selected = option.value == s->model.value();
        row->color.set(foreground(option.disabled ? t.colors.option_disabled_color
                                  : selected      ? t.colors.option_selected_color
                                                  : t.colors.option_color));
        row->font.set({t.typography.font_family,
                       selected ? t.typography.selected_font_weight : t.typography.font_weight, false,
                       t.typography.font_size, t.typography.line_height});
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
    for (const auto& [child, blocked] : boundaries) {
        if (!services_->components().contains(id)) {
            return;
        }
        if (services_->components().contains(child)) {
            services_->set_input_blocked(child, blocked);
        }
    }
}

void AutoCompleteComponentHost::request_open(runtime::ComponentId id, bool open, bool restore_focus) {
    auto* s = services_->components().state<State>(id);
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
        s->model.restore();
        s->display.set(s->model.display());
    }
    refresh(id);
    if (changed && services_->components().contains(id) && callback) {
        callback(open);
    }
    if (restore_focus && services_->components().contains(id) && editable(id, false)) {
        services_->focus().defer_focus(input, input::FocusModality::keyboard);
    }
}

void AutoCompleteComponentHost::suppress(runtime::ComponentId id) {
    auto* s = services_->components().state<State>(id);
    if (!s || s->suppressed) {
        return;
    }
    s->suppressed = true;
    s->model.restore();
    s->display.set(s->model.display());
    s->scroll = 0;
    request_open(id, false);
}

void AutoCompleteComponentHost::publish(runtime::ComponentId id, String value, std::optional<AutoCompleteOption> option,
                                        bool clear) {
    if (!editable(id)) {
        return;
    }
    auto* s = services_->components().state<State>(id);
    const auto previous = s->model.value();
    const auto on_change = s->on_change;
    const auto on_search = s->on_search;
    const auto on_select = s->on_select;
    const auto on_clear = s->on_clear;
    if (option || clear) {
        s->model.request(value);
    } else {
        s->model.edit(value);
    }
    // Reflect the committed editor text before reading an owner echo; a refused controlled intent restores it.
    s->display.set(value);
    if (previous != value && on_change) {
        on_change(value);
    }
    s = services_->components().state<State>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto authority = s->authority;
    if (authority) {
        auto accepted = read_prop(*authority);
        s = services_->components().state<State>(id);
        if (!s || !services_->components().scope(id).active()) {
            return;
        }
        s->model.reconcile(std::move(accepted));
    }
    s->display.set(s->model.display());
    refresh(id);
    if (!services_->components().contains(id)) {
        return;
    }
    if (option && on_select) {
        on_select(value, *option);
    } else if (!option && on_search) {
        on_search(value);
    }
    if (!services_->components().contains(id)) {
        return;
    }
    if (clear && on_clear) {
        on_clear();
    }
    if (services_->components().contains(id)) {
        request_open(id, !option && !clear);
    }
}

void AutoCompleteComponentHost::choose(runtime::ComponentId id, std::size_t index) {
    const auto* s = services_->components().state<State>(id);
    if (!s || !s->visible || index >= s->model.options().size() || s->model.options()[index].disabled ||
        std::ranges::find(s->model.filtered(), index) == s->model.filtered().end()) {
        return;
    }
    auto option = s->model.options()[index];
    publish(id, option.value, option);
}

void AutoCompleteComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<State>(id);
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
    s->input_theme.set(std::move(*config));
    if (services_->components().set_window_layer(s->popup, t.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    refresh(id, geometry);
}

void AutoCompleteComponentHost::mount_label(runtime::ComponentId child) {
    auto* row = services_->components().state<Row>(child);
    auto& build = runtime::require_component_build_context();
    row->label_root = group(*services_);
    const auto label_root = row->label_root;
    build.on_resource_cleanup(label_root, [services = services_, label_root] {
        services->interactions().set_input_blocked(label_root, false);
    });
    build.mount_slot_with_semantic_text_style(row->label_root, Content{[row] {
                                                  if (const auto* label = row->rich.content()) {
                                                      SlotContentAccess::function (*label)();
                                                  } else {
                                                      Text(TextProps{}.content(row->label));
                                                  }
                                              }},
                                              row->color, row->font);
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
    auto& s = *services.components().state<State>(id);
    const auto child = build.mount_component<Row>();
    auto& row = build.state<Row>(child);
    row.node = build.root(child);
    row.index = index;
    row.key = s.model.options()[index].key;
    row.rich = s.model.options()[index].rich_label;
    services.layout().set_layout(row.node, layout::BoxLayout{});
    services.nodes().require(row.node).clip_content = true;
    row.interaction = services.interactions().create({child, row.node, s.popup_hit, false, false, {}, false, false});
    input::InteractionHandlers handlers;
    handlers.target = [this, id, child](input::PointerDispatchContext& e) {
        const auto* s = services_->components().state<State>(id);
        const auto* row = services_->components().state<Row>(child);
        if (!s || !row || !s->visible || !editable(id)) {
            return;
        }
        const auto index = row->index;
        if (e.kind() == input::PointerEventKind::enter || e.kind() == input::PointerEventKind::move) {
            auto* live = services_->components().state<State>(id);
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
    build.mount_slot(child, Content{[this, child] { mount_label(child); }});
    s.rows.push_back(child);
}

void AutoCompleteComponentHost::reconcile_rows(runtime::ComponentId id) {
    auto* s = services_->components().state<State>(id);
    if (!s) {
        return;
    }
    auto previous = s->rows;
    std::vector<runtime::ComponentId> next;
    for (std::size_t i = 0; i < s->model.options().size(); ++i) {
        const auto option = s->model.options()[i];
        const auto found = std::ranges::find_if(previous, [&](auto child) {
            const auto* row = services_->components().state<Row>(child);
            return row && row->key == option.key;
        });
        if (found == previous.end()) {
            const auto list = s->list;
            services_->append_slot(list, Content{[this, id, i] { mount_row(id, i); }});
            s = services_->components().state<State>(id);
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
                services_->set_input_blocked(label, true);
                if (!services_->components().contains(id)) {
                    return;
                }
                services_->destroy(label);
                if (!services_->components().contains(id)) {
                    return;
                }
                services_->append_slot(child, Content{[this, child] { mount_label(child); }});
                s = services_->components().state<State>(id);
                if (!s) {
                    return;
                }
            }
            next.push_back(child);
            previous.erase(found);
        }
    }
    s->rows = std::move(next);
    for (auto child : previous) {
        services_->set_input_blocked(child, true);
        if (!services_->components().contains(id)) {
            return;
        }
        services_->destroy(child);
        if (!services_->components().contains(id)) {
            return;
        }
    }
    refresh(id);
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
    const auto height = read_prop(props.list_height_);
    validate_height(height);
    AutoCompleteModel model{read_prop(props.options_),
                            props.value_ ? read_prop(*props.value_) : props.default_value_.value_or(String{}),
                            props.value_.has_value(), read_prop(props.filter_), read_prop(props.active_first_)};
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<State>(std::move(model));
    auto& s = build.state<State>(id);
    s.node = build.root(id);
    s.reference = props.ref_.value_or(AutoCompleteRef{});
    s.authority = props.value_;
    s.filter = read_prop(props.filter_);
    s.active_first = read_prop(props.active_first_);
    s.backfill = read_prop(props.backfill_);
    s.allow_clear = read_prop(props.allow_clear_);
    s.list_height = height;
    s.open = props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false);
    s.controlled_open = props.open_.has_value();
    s.on_change = props.on_change_;
    s.on_search = props.on_search_;
    s.on_select = props.on_select_;
    s.on_clear = props.on_clear_;
    s.on_open = props.on_open_;
    s.on_focus = props.on_focus_;
    s.on_blur = props.on_blur_;
    s.display.set(s.model.display());
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.mount_slot(
        id, Content{[&] {
            Theme(ThemeProps{}.config(s.input_theme), ThemeContent{[&] {
                      std::optional<InputPrefix> input_prefix;
                      if (prefix) {
                          input_prefix = InputPrefix{SlotContentAccess::function(*prefix)};
                      }
                      // Capture the main Input before rich options can mount further editors.
                      const auto count = inputs->mounted_inputs().size();
                      Input(InputProps{}
                                .value(s.display)
                                .placeholder(props.placeholder_)
                                .disabled(props.disabled_)
                                .readOnly(props.read_only_)
                                .size(props.size_)
                                .variant(props.variant_)
                                .status(props.status_)
                                .ref(s.reference)
                                .autoFocus(props.auto_focus_)
                                .onChange([this, id](String text) { publish(id, std::move(text), {}); })
                                .onFocus([this, id] {
                                    const auto* s = services_->components().state<State>(id);
                                    if (!s) {
                                        return;
                                    }
                                    const auto callback = s->on_focus;
                                    if (callback) {
                                        callback();
                                    }
                                })
                                .onBlur([this, id] {
                                    auto* s = services_->components().state<State>(id);
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
                                    if (services_->components().contains(id) && callback) {
                                        callback();
                                    }
                                }),
                            input_prefix, InputSuffix{[&] {
                                s.clear_wrapper = group(services);
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
                                    SlotContentAccess::function (*suffix)();
                                }
                            }});
                      const auto input = inputs->mounted_inputs()[count];
                      s.input = input.component;
                      s.input_hit = input.interaction;
                      s.editor = input.editor;
                      for (auto hit : services.interactions().declaration_order()) {
                          const auto* record = services.interactions().find(hit);
                          if (record && hit != s.input_hit && record->focusable &&
                              descendant(services, record->component, s.input)) {
                              throw std::invalid_argument("AutoComplete prefix/suffix must be non-interactive");
                          }
                      }
                      inputs->configure_form_reset(s.input, [this, id, editor = s.editor] {
                          auto* s = services_->components().state<State>(id);
                          if (!s || s->editor != editor || !services_->components().scope(id).active()) {
                              return;
                          }
                          const auto authority = s->authority;
                          if (authority) {
                              auto accepted = read_prop(*authority);
                              s = services_->components().state<State>(id);
                              if (!s || s->editor != editor || !services_->components().scope(id).active()) {
                                  return;
                              }
                              s->model.reconcile(std::move(accepted));
                          }
                          s->model.restore();
                          s->display.set(s->model.display());
                          s->scroll = 0;
                          refresh(id);
                      });
                  }});
            s.popup = group(services, true);
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
                                 services.nodes().require(build.root(s.list)).clip_content = true;
                                 build.mount_slot(s.list, Content{[&] {
                                                      for (std::size_t i = 0; i < s.model.options().size(); ++i) {
                                                          mount_row(id, i);
                                                      }
                                                  }});
                             }});
        }});
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                return engine.measure_child(
                    services_->components().root(services_->components().state<State>(id)->input), limits);
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                engine.place_child(services_->components().root(services_->components().state<State>(id)->input), rect);
            }});
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    mounted_.push_back(id);
    auto& scope = build.scope(id);
    connect_prop(scope, props.options_, [this, id](std::vector<AutoCompleteOption> value) {
        auto* s = services_->components().state<State>(id);
        if (s && s->model.set_options(std::move(value))) {
            s->display.set(s->model.display());
            reconcile_rows(id);
        }
    });
    if (props.value_) {
        connect_prop(scope, *props.value_, [this, id](String value) {
            auto* s = services_->components().state<State>(id);
            if (s && s->model.reconcile(std::move(value))) {
                s->display.set(s->model.display());
                refresh(id);
            }
        });
    }
    if (props.open_) {
        connect_prop(scope, *props.open_, [this, id](bool open) {
            auto* s = services_->components().state<State>(id);
            if (!s) {
                return;
            }
            const bool changed = s->open != open;
            s->open = open;
            if (changed && open) {
                s->suppressed = false;
            }
            if (!open) {
                s->model.restore();
                s->display.set(s->model.display());
            }
            refresh(id);
        });
    }
    connect_prop(scope, props.filter_, [this, id](AutoCompleteFilter filter) {
        if (auto* s = services_->components().state<State>(id)) {
            s->model.configure(filter, s->active_first);
            s->filter = std::move(filter);
            s->display.set(s->model.display());
            refresh(id);
        }
    });
    connect_prop(scope, props.active_first_, [this, id](bool value) {
        if (auto* s = services_->components().state<State>(id)) {
            s->model.configure(s->filter, value);
            s->active_first = value;
            s->display.set(s->model.display());
            refresh(id);
        }
    });
    connect_prop(scope, props.backfill_, [this, id](bool value) {
        if (auto* s = services_->components().state<State>(id)) {
            s->backfill = value;
            if (!value) {
                s->model.restore();
                s->display.set(s->model.display());
            }
            refresh(id, false);
        }
    });
    connect_prop(scope, props.allow_clear_, [this, id](bool value) {
        if (auto* s = services_->components().state<State>(id)) {
            s->allow_clear = value;
            refresh(id, false);
        }
    });
    connect_prop(scope, props.disabled_, [this, id](bool value) {
        if (value) {
            suppress(id);
        }
        refresh(id);
    });
    connect_prop(scope, props.read_only_, [this, id](bool value) {
        if (value) {
            suppress(id);
        }
        refresh(id);
    });
    connect_prop(scope, props.list_height_, [this, id](float value) {
        validate_height(value);
        if (auto* s = services_->components().state<State>(id)) {
            s->list_height = value;
            refresh(id);
        }
    });
    update_theme(id, true);
    const auto theme = build.theme_scope();
    const std::array accessors{
        &theme_runtime::ThemeScope::auto_complete_colors, &theme_runtime::ThemeScope::auto_complete_metrics,
        &theme_runtime::ThemeScope::auto_complete_typography, &theme_runtime::ThemeScope::auto_complete_effects,
        &theme_runtime::ThemeScope::auto_complete_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] = theme->capture(
            [this, id, geometry = i == 1 || i == 2](theme_runtime::DirtyPhase) { update_theme(id, geometry); },
            [theme, accessor = accessors[i], shadow = i == 0] {
                (void)(theme.get()->*accessor)();
                if (shadow) {
                    (void)theme->input_shadows();
                }
            });
    }
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
        const auto* s = services_->components().state<State>(id);
        if (s && s->has_been_active && !editable(id, false)) {
            suppress(id);
        }
        refresh(id, false);
    }
}

void AutoCompleteComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<State>(id);
        if (!s) {
            continue;
        }
        s->closed_on_blur = false;
        const bool active = editable(id, false);
        if (active) {
            s->has_been_active = true;
        }
        if (s->has_been_active && !active) {
            suppress(id);
        }
        s = services_->components().state<State>(id);
        if (s) {
            s->action_disabled.set(!editable(id));
            const bool visible = s->open && !s->suppressed && !s->model.filtered().empty() && editable(id, false);
            if (visible != s->visible) {
                refresh(id);
            }
        }
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
        auto* s = services_->components().state<State>(id);
        if (!s || !editable(id) || !descendant(*services_, component, id)) {
            continue;
        }
        const bool main = focused == s->input_hit;
        if (!main && layer(*services_, component) != std::optional{s->popup}) {
            continue;
        }
        const auto session = services_->input_runtime()->sessions().active();
        const auto* editor = services_->input_runtime()->editors().find(session.owner);
        if (editor && editor->composition().active) {
            return false;
        }
        using input::Key;
        if (event.key == Key::tab) {
            request_open(id, false);
            return false;
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
            request_open(id, false, !main);
            return true;
        }
        if (!main || event.modifiers != input::KeyModifier::none) {
            return false;
        }
        if (event.key == Key::enter) {
            if (s->visible && s->model.active()) {
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
        request_open(id, true);
        s = services_->components().state<State>(id);
        if (!s || !s->visible) {
            return true;
        }
        if (visible || !s->model.active()) {
            s->model.move(event.key == Key::up ? -1 : 1);
        }
        if (s->backfill) {
            s->model.backfill();
            s->display.set(s->model.display());
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
        auto* s = services_->components().state<State>(id);
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
        auto* s = services_->components().state<State>(id);
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
    auto* s = services_->components().state<State>(id);
    if (!s || !s->visible) {
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
        const auto* s = services_->components().state<State>(id);
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
