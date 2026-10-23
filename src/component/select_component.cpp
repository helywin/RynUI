#include "component/select_component.hpp"
#include "component/select_model.hpp"
#include "component/input_component.hpp"
#include "component/input_affix_action.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "theme/input_tokens.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
namespace {
thread_local SelectComponentHost* active_selects{};

struct SelectRow final {
    runtime::NodeId node;
    input::InteractionId interaction;
    runtime::SceneFragmentId hit;
    component::RetainedSurfaceId surface;
    std::optional<std::size_t> index;
    Signal<String> label{String{}};
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
};

struct SelectState final {
    explicit SelectState(SelectModel value) : model(std::move(value)) {}

    SelectModel model;
    runtime::NodeId node;
    runtime::ComponentId selector;
    runtime::ComponentId popup;
    runtime::ComponentId list;
    runtime::ComponentId clear_wrapper;
    input::InteractionId selector_hit;
    input::InteractionId popup_hit;
    input::TextInputOwnerId editor;
    component::RetainedSurfaceId background;
    component::RetainedSurfaceId clear_background;
    std::vector<runtime::ComponentId> rows;
    SelectRef reference;
    Signal<String> display{String{}};
    Signal<String> placeholder{String{}};
    String external_placeholder;
    String empty_label{u8"No data"};
    Signal<bool> read_only{true};
    Signal<bool> clear_visible{false};
    Signal<bool> action_disabled{false};
    Signal<IconSource> arrow{IconSource{IconName::DownOutlined}};
    Signal<bool> spinning{false};
    Signal<ThemeConfig> selector_theme{ThemeConfig{}};
    std::array<theme_runtime::Subscription, 5> subscriptions;
    bool show_search{};
    bool allow_clear{};
    bool disabled{};
    bool loading{};
    bool focused{};
    bool controlled_open{};
    bool open{};
    bool suppressed{};
    bool visible{};
    bool above{};
    bool reveal_active{true};
    bool needs_layout{true};
    runtime::Rect layout_anchor;
    runtime::Rect layout_clip;
    float list_height{256};
    float scroll{};
    runtime::Rect popup_bounds;
    std::function<void(SelectSelection)> on_change;
    std::function<void(String)> on_search;
    std::function<void(bool)> on_open;
    std::function<void()> on_focus;
    std::function<void()> on_blur;
};

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

graphics::QuadInstance quad(runtime::Rect bounds, Color color, float radius = 0) {
    graphics::QuadInstance result;
    result.bounds = {bounds.x, bounds.y, bounds.width, bounds.height};
    result.color = {color.red(), color.green(), color.blue(), color.alpha()};
    result.corner_radius = radius;
    return result;
}

runtime::Rect intersect(runtime::Rect a, runtime::Rect b) {
    const float x = std::max(a.x, b.x);
    const float y = std::max(a.y, b.y);
    return {x, y, std::max(0.0F, std::min(a.x + a.width, b.x + b.width) - x),
            std::max(0.0F, std::min(a.y + a.height, b.y + b.height) - y)};
}

bool inside(runtime::Rect bounds, float x, float y) {
    return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height;
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

void validate_height(float height) {
    if (!std::isfinite(height) || height <= 0) {
        throw std::invalid_argument("Select listHeight must be positive and finite");
    }
}
} // namespace

SelectComponentHost::SelectComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

SelectComponentHost::~SelectComponentHost() {
    services_->detach(*this);
}

void* SelectComponentHost::begin_mount() noexcept {
    auto* previous = active_selects;
    active_selects = this;
    return previous;
}

void SelectComponentHost::end_mount(void* previous) noexcept {
    active_selects = static_cast<SelectComponentHost*>(previous);
}

void SelectComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

SelectSnapshot SelectComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<SelectState>(id);
    if (!s) {
        throw std::out_of_range("Select component is stale");
    }
    return {s->selector,      s->popup_bounds,   s->visible, s->above, s->model.value(),
            s->model.query(), s->model.active(), s->rows,    s->scroll};
}

void SelectComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<SelectState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const bool visible =
        s->open && !s->suppressed && !s->disabled && window_active_ && services_->components().branch_active(id);
    const bool changed = s->visible != visible;
    s->visible = visible;
    if (services_->components().set_branch_active(s->popup, visible)) {
        services_->mark_scene_structure_dirty();
    }
    const bool editing = visible && s->show_search;
    s->display.set(editing ? s->model.query() : s->model.label());
    s->placeholder.set(editing && s->model.value() ? s->model.label() : s->external_placeholder);
    s->read_only.set(!s->show_search);
    s->clear_visible.set(s->allow_clear && s->model.value().has_value() && !s->disabled && !s->loading);
    s->action_disabled.set(s->disabled || s->loading);
    s->arrow.set(IconSource{s->loading ? IconName::LoadingOutlined : IconName::DownOutlined});
    s->spinning.set(s->loading);
    const auto& token = services_->components().theme_scope(id)->snapshot().select();
    const auto filtered = s->model.filtered();
    for (auto row_id : s->rows) {
        auto* row = services_->components().state<SelectRow>(row_id);
        if (!row) {
            continue;
        }
        const bool empty = filtered.empty() && !row->index;
        const bool shown = empty || (row->index && std::ranges::find(filtered, *row->index) != filtered.end());
        if (services_->components().set_branch_active(row_id, shown)) {
            services_->mark_scene_structure_dirty();
        }
        const bool selected = row->index && s->model.value() == SelectSelection{s->model.options()[*row->index].value};
        const bool disabled = !row->index || s->disabled || s->loading || s->model.options()[*row->index].disabled;
        services_->interactions().set_eligible(row->interaction, visible && shown && !disabled);
        if (!row->index) {
            row->label.set(s->empty_label);
        }
        row->foreground.set(foreground(disabled   ? token.colors.option_disabled_color
                                       : selected ? token.colors.option_selected_color
                                                  : token.colors.option_color));
        row->typography.set({token.typography.font_family,
                             selected ? token.typography.selected_font_weight : token.typography.font_weight, false,
                             token.typography.font_size, token.typography.line_height});
    }
    if (geometry || changed) {
        s->needs_layout = true;
        s->reveal_active = true;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    }
    if (changed && !visible) {
        const auto popup_hit = s->popup_hit;
        const auto rows = s->rows;
        services_->pointer().cancel_interaction(popup_hit);
        for (auto row : rows) {
            if (const auto* live = services_->components().state<SelectRow>(row)) {
                services_->pointer().cancel_interaction(live->interaction);
            }
        }
    }
}

void SelectComponentHost::request_open(runtime::ComponentId id, bool open) {
    auto* s = services_->components().state<SelectState>(id);
    if (!s || !services_->components().scope(id).active() || (open && (s->disabled || !window_active_))) {
        return;
    }
    const bool changed = s->open != open;
    const auto callback = s->on_open;
    if (!s->controlled_open) {
        s->open = open;
    }
    if (open) {
        s->suppressed = false;
    }
    if (!open) {
        s->model.search({});
    }
    refresh(id);
    if (changed && callback) {
        callback(open);
    }
}

void SelectComponentHost::choose(runtime::ComponentId id, std::optional<std::size_t> index) {
    auto* s = services_->components().state<SelectState>(id);
    if (!s || s->disabled || s->loading) {
        return;
    }
    const auto previous = s->model.value();
    auto intent = s->model.request(index);
    if (!intent) {
        return;
    }
    const auto callback = s->on_change;
    s->model.search({});
    refresh(id);
    if (previous != *intent && callback) {
        callback(*intent);
    }
    if (services_->components().contains(id)) {
        request_open(id, false);
    }
}

void SelectComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<SelectState>(id);
    if (!s) {
        return;
    }
    const auto& theme = services_->components().theme_scope(id)->snapshot();
    const auto& token = theme.select();
    ThemeConfig config;
    config.input.tokens.background = token.colors.selector_background;
    config.input.tokens.hover_background = token.colors.selector_background;
    config.input.tokens.active_background = token.colors.selector_background;
    config.input.tokens.hover_border_color = token.colors.hover_border_color;
    config.input.tokens.active_border_color = token.colors.active_border_color;
    const auto& original = InputTokenAccess::get(theme).active_shadow;
    std::array<ShadowLayer, ShadowList::capacity> shadow{};
    std::copy(original.layers().begin(), original.layers().end(), shadow.begin());
    for (std::size_t i = 0; i < original.size(); ++i) {
        shadow[i].color = token.colors.active_outline_color;
    }
    config.input.tokens.active_shadow = ShadowList{std::span<const ShadowLayer>{shadow.data(), original.size()}};
    s->selector_theme.set(std::move(config));
    if (services_->components().set_window_layer(s->popup, token.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    refresh(id, geometry);
}

void SelectComponentHost::mount_rows(runtime::ComponentId id) {
    auto& services = *services_;
    auto* s = services.components().state<SelectState>(id);
    if (!s) {
        return;
    }
    auto& build = runtime::require_component_build_context();
    const auto count = s->model.options().size();
    s->rows.reserve(count + 1);
    for (std::size_t index = 0; index <= count; ++index) {
        const auto row_id = build.mount_component<SelectRow>();
        auto& row = build.state<SelectRow>(row_id);
        row.node = build.root(row_id);
        row.index = index < count ? std::optional{index} : std::nullopt;
        row.label.set(row.index ? s->model.options()[index].label : s->empty_label);
        services.layout().set_layout(row.node, layout::BoxLayout{});
        row.interaction =
            services.interactions().create({row_id, row.node, s->popup_hit, false, false, {}, false, false});
        input::InteractionHandlers handlers;
        handlers.target = [this, id, row_id](input::PointerDispatchContext& event) {
            auto* s = services_->components().state<SelectState>(id);
            auto* row = services_->components().state<SelectRow>(row_id);
            if (!s || !row || !row->index || s->disabled || s->loading) {
                return;
            }
            const auto index = *row->index;
            if (event.kind() == input::PointerEventKind::enter || event.kind() == input::PointerEventKind::move) {
                if (s->model.activate(index)) {
                    refresh(id, false);
                    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
                }
            }
            if (event.kind() == input::PointerEventKind::down &&
                event.event().button == input::PointerButton::primary) {
                static_cast<void>(event.capture_pointer());
                event.stop_propagation();
            }
            if (event.kind() == input::PointerEventKind::up && event.event().button == input::PointerButton::primary) {
                static_cast<void>(event.release_pointer_capture());
                const auto bounds = services_->nodes().require(row->node).bounds;
                const bool hit = event.press_origin() == row->interaction &&
                                 inside(intersect(bounds, s->popup_bounds), event.event().x, event.event().y);
                event.stop_propagation();
                if (hit) {
                    choose(id, index);
                }
            }
        };
        services.interactions().set_handlers(row.interaction, std::move(handlers));
        row.hit = build.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
        graphics::QuadInstance initial;
        initial.opacity = 0;
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        services.scene_composer().set_fragment(row.hit, {}, row.interaction);
        const auto body = build.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
        row.surface = services.surfaces().create_surface(row_id, row.node, body, std::span{&initial, 1}, effects);
        build.on_resource_cleanup(
            row_id, [&services, node = row.node, hit = row.interaction, fragment = row.hit, surface = row.surface] {
                services.pointer().cancel_interaction(hit);
                services.interactions().remove(hit);
                services.surfaces().destroy(surface);
                services.scene_composer().remove_fragment(fragment);
                services.layout().remove_layout(node);
            });
        build.mount_slot_with_semantic_text_style(row_id, Content{[&row] { Text(TextProps{}.content(row.label)); }},
                                                  Prop<runtime::SemanticForeground>{row.foreground},
                                                  Prop<runtime::SemanticTypography>{row.typography});
        s->rows.push_back(row_id);
    }
}

void SelectComponentHost::rebuild_rows(runtime::ComponentId id) {
    auto* s = services_->components().state<SelectState>(id);
    if (!s) {
        return;
    }
    const auto previous = std::move(s->rows);
    s->rows.clear();
    for (auto row : previous) {
        services_->destroy(row);
    }
    s = services_->components().state<SelectState>(id);
    if (!s) {
        return;
    }
    services_->append_slot(s->list, Content{[this, id] { mount_rows(id); }});
    refresh(id);
}

void SelectComponentHost::mount(const SelectProps& props, const std::optional<SelectPrefix>& prefix,
                                const std::optional<SelectSuffix>& suffix) {
    auto& services = *services_;
    auto* inputs = services.input_runtime();
    if (!inputs) {
        throw std::logic_error("Select requires a window Input host");
    }
    if ((props.value_ && props.default_value_) || (props.open_ && props.default_open_)) {
        throw std::invalid_argument("Select cannot combine controlled and default values");
    }
    const auto height = read_prop(props.list_height_);
    validate_height(height);
    SelectModel model{read_prop(props.options_), props.value_ ? read_prop(*props.value_) : props.default_value_,
                      props.value_.has_value()};
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<SelectState>(std::move(model));
    auto& s = build.state<SelectState>(id);
    s.node = build.root(id);
    s.reference = props.ref_.value_or(SelectRef{});
    s.show_search = read_prop(props.show_search_);
    s.allow_clear = read_prop(props.allow_clear_);
    s.disabled = read_prop(props.disabled_);
    s.loading = read_prop(props.loading_);
    s.open = props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false);
    s.controlled_open = props.open_.has_value();
    s.external_placeholder = read_prop(props.placeholder_);
    s.empty_label = read_prop(props.not_found_content_);
    s.list_height = height;
    s.on_change = props.on_change_;
    s.on_search = props.on_search_;
    s.on_open = props.on_open_change_;
    s.on_focus = props.on_focus_;
    s.on_blur = props.on_blur_;
    s.display.set(s.model.label());
    s.read_only.set(!s.show_search);
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.mount_slot(
        id, Content{[&] {
            Theme(ThemeProps{}.config(s.selector_theme), ThemeContent{[&] {
                      std::optional<InputPrefix> input_prefix;
                      if (prefix) {
                          input_prefix = InputPrefix{SlotContentAccess::function(*prefix)};
                      }
                      Input(InputProps{}
                                .value(s.display)
                                .placeholder(s.placeholder)
                                .readOnly(s.read_only)
                                .disabled(props.disabled_)
                                .size(props.size_)
                                .variant(props.variant_)
                                .status(props.status_)
                                .ref(s.reference)
                                .autoFocus(props.auto_focus_)
                                .onChange([this, id](String query) {
                                    auto* s = services_->components().state<SelectState>(id);
                                    if (!s || !s->show_search || s->disabled) {
                                        return;
                                    }
                                    const auto callback = s->on_search;
                                    s->model.search(query);
                                    request_open(id, true);
                                    if (services_->components().contains(id) && callback) {
                                        callback(std::move(query));
                                    }
                                })
                                .onFocus([this, id] {
                                    auto* s = services_->components().state<SelectState>(id);
                                    if (!s) {
                                        return;
                                    }
                                    s->focused = true;
                                    const auto callback = s->on_focus;
                                    if (callback) {
                                        callback();
                                    }
                                })
                                .onBlur([this, id] {
                                    auto* s = services_->components().state<SelectState>(id);
                                    if (!s || !services_->components().scope(id).active()) {
                                        return;
                                    }
                                    s->focused = false;
                                    s->suppressed = true;
                                    const auto callback = s->on_blur;
                                    request_open(id, false);
                                    if (services_->components().contains(id) && callback) {
                                        callback();
                                    }
                                }),
                            input_prefix, InputSuffix{[&] {
                                auto& children = runtime::require_component_build_context();
                                s.clear_wrapper = children.mount_component<int>(0);
                                const auto clear_node = children.root(s.clear_wrapper);
                                services.layout().set_layout(clear_node, layout::BoxLayout{});
                                const auto fragment = children.register_scene_fragment(
                                    s.clear_wrapper, runtime::SceneFragmentPlacement::before_children);
                                graphics::QuadInstance initial;
                                initial.opacity = 0;
                                component::RetainedSurfaceEffects effects;
                                effects.focus_enabled = false;
                                s.clear_background = services.surfaces().create_surface(
                                    s.clear_wrapper, clear_node, fragment, std::span{&initial, 1}, effects);
                                children.on_resource_cleanup(s.clear_wrapper,
                                                             [&services, clear_node, surface = s.clear_background] {
                                                                 services.surfaces().destroy(surface);
                                                                 services.layout().remove_layout(clear_node);
                                                             });
                                children.mount_slot(s.clear_wrapper, Content{[&] {
                                                        mount_input_affix_action(
                                                            services, IconSource{IconName::CloseCircleFilled},
                                                            s.action_disabled, [this, id] { choose(id, {}); },
                                                            s.clear_visible, false, false);
                                                    }});
                                if (suffix) {
                                    SlotContentAccess::function (*suffix)();
                                } else {
                                    Icon(IconProps{}.source(s.arrow).spin(s.spinning).tone(TextTone::Secondary));
                                }
                            }});
                      const auto input = inputs->mounted_inputs().back();
                      s.selector = input.component;
                      s.selector_hit = input.interaction;
                      s.editor = input.editor;
                      for (auto hit : services.interactions().declaration_order()) {
                          const auto* record = services.interactions().find(hit);
                          if (record && hit != s.selector_hit && record->focusable &&
                              descendant(services, record->component, s.selector)) {
                              throw std::invalid_argument("Select prefix and suffix must be non-interactive");
                          }
                      }
                  }});
            s.popup = runtime::require_component_build_context().mount_component<int>(0);
            const auto popup_node = build.root(s.popup);
            services.layout().set_layout(popup_node, layout::LeafLayout{});
            services.components().set_window_layer(s.popup, build.theme_scope()->snapshot().select().z_index_popup);
            s.popup_hit = services.interactions().create({s.popup, popup_node, {}, true, false, {}, false, false});
            const auto fragment =
                build.register_scene_fragment(s.popup, runtime::SceneFragmentPlacement::before_children);
            graphics::QuadInstance initial;
            initial.opacity = 0;
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = false;
            s.background =
                services.surfaces().create(s.popup, popup_node, fragment, s.popup_hit, std::span{&initial, 1}, effects);
            build.on_resource_cleanup(s.popup, [&services, popup_node, hit = s.popup_hit, surface = s.background] {
                services.pointer().cancel_interaction(hit);
                services.interactions().remove(hit);
                services.surfaces().destroy(surface);
                services.layout().remove_layout(popup_node);
            });
            build.mount_slot(s.popup, Content{[&] {
                                 auto& children = runtime::require_component_build_context();
                                 s.list = children.mount_component<int>(0);
                                 const auto list_node = children.root(s.list);
                                 services.layout().set_layout(list_node, layout::LeafLayout{});
                                 services.nodes().require(list_node).clip_content = true;
                                 children.on_resource_cleanup(
                                     s.list, [&services, list_node] { services.layout().remove_layout(list_node); });
                                 children.mount_slot(s.list, Content{[this, id] { mount_rows(id); }});
                             }});
        }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints constraints) {
                        const auto* s = services_->components().state<SelectState>(id);
                        return engine.measure_child(services_->components().root(s->selector), constraints);
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                        const auto* s = services_->components().state<SelectState>(id);
                        engine.place_child(services_->components().root(s->selector), bounds);
                    }});
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    mounted_.push_back(id);
    update_theme(id);
    auto& scope = build.scope(id);
    connect_prop(scope, props.options_, [this, id](std::vector<SelectOption> value) {
        if (auto* s = services_->components().state<SelectState>(id); s && s->model.set_options(std::move(value))) {
            rebuild_rows(id);
        }
    });
    if (props.value_) {
        connect_prop(scope, *props.value_, [this, id](SelectSelection value) {
            if (auto* s = services_->components().state<SelectState>(id); s && s->model.reconcile(std::move(value))) {
                refresh(id);
            }
        });
    }
    if (props.open_) {
        connect_prop(scope, *props.open_, [this, id](bool value) {
            if (auto* s = services_->components().state<SelectState>(id)) {
                s->open = value;
                s->suppressed = false;
                if (!value) {
                    s->model.search({});
                }
                refresh(id);
            }
        });
    }
    connect_prop(scope, props.placeholder_, [this, id](String value) {
        if (auto* s = services_->components().state<SelectState>(id)) {
            s->external_placeholder = std::move(value);
            refresh(id, false);
        }
    });
    connect_prop(scope, props.show_search_, [this, id](bool value) {
        if (auto* s = services_->components().state<SelectState>(id)) {
            s->show_search = value;
            s->model.search({});
            refresh(id);
        }
    });
    connect_prop(scope, props.allow_clear_, [this, id](bool value) {
        if (auto* s = services_->components().state<SelectState>(id)) {
            s->allow_clear = value;
            refresh(id, false);
        }
    });
    connect_prop(scope, props.loading_, [this, id](bool value) {
        if (auto* s = services_->components().state<SelectState>(id)) {
            s->loading = value;
            refresh(id, false);
        }
    });
    connect_prop(scope, props.disabled_, [this, id](bool value) {
        if (auto* s = services_->components().state<SelectState>(id)) {
            s->disabled = value;
            if (value) {
                s->suppressed = true;
                request_open(id, false);
            }
            refresh(id);
        }
    });
    connect_prop(scope, props.not_found_content_, [this, id](String value) {
        if (auto* s = services_->components().state<SelectState>(id)) {
            s->empty_label = std::move(value);
            refresh(id);
        }
    });
    connect_prop(scope, props.list_height_, [this, id](float value) {
        validate_height(value);
        if (auto* s = services_->components().state<SelectState>(id)) {
            s->list_height = value;
            refresh(id);
        }
    });
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::select_colors, &theme_runtime::ThemeScope::select_metrics,
                               &theme_runtime::ThemeScope::select_typography,
                               &theme_runtime::ThemeScope::select_effects, &theme_runtime::ThemeScope::select_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] = theme->capture(
            [this, id, geometry = i == 1 || i == 2](theme_runtime::DirtyPhase) { update_theme(id, geometry); },
            [theme, accessor = accessors[i], selector_effects = i == 0] {
                static_cast<void>((theme.get()->*accessor)());
                if (selector_effects) {
                    static_cast<void>(theme->input_shadows());
                }
            });
    }
}

void SelectComponentHost::on_window_active(bool active) {
    window_active_ = active;
    const auto ids = mounted_;
    for (auto id : ids) {
        if (auto* s = services_->components().state<SelectState>(id); s && !active) {
            s->suppressed = true;
            request_open(id, false);
        }
        refresh(id);
    }
}

bool SelectComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.action != input::KeyAction::down || !window_active_) {
        return false;
    }
    const auto focus = services_->focus().state().focused;
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<SelectState>(id);
        if (!s || s->disabled || focus != s->selector_hit) {
            continue;
        }
        if (services_->input_runtime()->editors().require(s->editor).composition().active) {
            return false;
        }
        using input::Key;
        if (event.key == Key::tab) {
            request_open(id, false);
            return false;
        }
        if (event.key == Key::escape && s->visible) {
            request_open(id, false);
            return true;
        }
        if (event.modifiers != input::KeyModifier::none) {
            return false;
        }
        if (event.key == Key::enter || (!s->show_search && event.key == Key::space)) {
            if (!event.repeat) {
                if (s->visible && s->model.active()) {
                    choose(id, *s->model.active());
                } else {
                    request_open(id, true);
                }
            }
            return true;
        }
        if (event.key != Key::up && event.key != Key::down && event.key != Key::home && event.key != Key::end) {
            return false;
        }
        if (!s->visible && (event.key == Key::home || event.key == Key::end)) {
            return false;
        }
        const bool was_open = s->visible;
        request_open(id, true);
        s = services_->components().state<SelectState>(id);
        if (!s) {
            return true;
        }
        if (event.key == Key::home || event.key == Key::end) {
            s->model.boundary(event.key == Key::end);
        } else if (was_open || !s->model.active()) {
            s->model.move(event.key == Key::up ? -1 : 1);
        }
        refresh(id);
        return true;
    }
    return false;
}

void SelectComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                           std::optional<input::InteractionId> hit,
                                           std::optional<input::InteractionId> origin) {
    if (event.action != input::PointerAction::down && event.action != input::PointerAction::up) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<SelectState>(id);
        if (!s || s->disabled) {
            continue;
        }
        const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
        const bool in_control = record && descendant(*services_, record->component, id);
        if (event.action == input::PointerAction::down && !in_control) {
            if (s->visible || (s->open && !s->suppressed)) {
                request_open(id, false);
            }
            continue;
        }
        if (event.action == input::PointerAction::up && event.button == input::PointerButton::primary &&
            hit == s->selector_hit && origin == hit) {
            request_open(id, s->show_search || !s->visible);
        }
    }
}

bool SelectComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    for (auto it = mounted_.rbegin(); it != mounted_.rend(); ++it) {
        auto* s = services_->components().state<SelectState>(*it);
        if (!s || !s->visible || !inside(s->popup_bounds, event.x, event.y)) {
            continue;
        }
        const auto& token = services_->components().theme_scope(*it)->snapshot().select();
        const float height = token.metrics.option_height;
        const float content = static_cast<float>(std::max<std::size_t>(1, s->model.filtered().size())) * height;
        const float available = std::max(0.0F, s->popup_bounds.height - token.metrics.popup_padding * 2);
        const float next =
            std::clamp(s->scroll - event.delta_y * height * 3, 0.0F, std::max(0.0F, content - available));
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

void SelectComponentHost::position_window_layers(runtime::Size, runtime::Rect clip) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<SelectState>(id);
        if (!s) {
            continue;
        }
        if (!services_->components().branch_active(id)) {
            refresh(id);
        }
        if (!s->visible) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().select();
        const auto& metrics = token.metrics;
        auto anchor = services_->nodes().require(s->node).bounds;
        const auto translation = services_->nodes().require(s->node).translation;
        anchor.x += translation.x;
        anchor.y += translation.y;
        const auto popup_node = services_->components().root(s->popup);
        if (!s->needs_layout && s->layout_anchor == anchor && s->layout_clip == clip &&
            services_->nodes().require(popup_node).measure_generation == services_->layout().generation()) {
            continue;
        }
        const auto filtered = s->model.filtered();
        const float content = static_cast<float>(std::max<std::size_t>(1, filtered.size())) * metrics.option_height;
        const float wanted = std::min(s->list_height, content) + metrics.popup_padding * 2;
        const float below =
            std::clamp(clip.y + clip.height - anchor.y - anchor.height - metrics.gap, 0.0F, clip.height);
        const float above = std::clamp(anchor.y - metrics.gap - clip.y, 0.0F, clip.height);
        s->above = below < wanted && above > below;
        const float height = std::min(wanted, s->above ? above : below);
        const float width = std::min(anchor.width, clip.width);
        s->popup_bounds = {
            std::clamp(anchor.x, clip.x, clip.x + clip.width - width),
            std::clamp(s->above ? anchor.y - metrics.gap - height : anchor.y + anchor.height + metrics.gap, clip.y,
                       clip.y + clip.height - height),
            width, height};
        const float available = std::max(0.0F, height - metrics.popup_padding * 2);
        s->scroll = std::clamp(s->scroll, 0.0F, std::max(0.0F, content - available));
        if (s->reveal_active && s->model.active()) {
            const auto it = std::ranges::find(filtered, *s->model.active());
            if (it != filtered.end()) {
                const float top = static_cast<float>(it - filtered.begin()) * metrics.option_height;
                s->scroll = available < metrics.option_height
                                ? top
                                : std::clamp(s->scroll, std::max(0.0F, top + metrics.option_height - available), top);
            }
        }
        s->reveal_active = false;
        auto& engine = services_->layout();
        static_cast<void>(engine.measure_child(popup_node, layout::Constraints::fixed(width, height)));
        engine.place_child(popup_node, s->popup_bounds);
        const float inset = std::min(metrics.popup_padding, std::min(width, height) / 2);
        const runtime::Rect menu{s->popup_bounds.x + inset, s->popup_bounds.y + inset,
                                 std::max(0.0F, width - inset * 2), std::max(0.0F, height - inset * 2)};
        const auto list_node = services_->components().root(s->list);
        static_cast<void>(engine.measure_child(list_node, layout::Constraints::fixed(menu.width, menu.height)));
        engine.place_child(list_node, menu);
        float y = s->popup_bounds.y + metrics.popup_padding - s->scroll;
        for (std::size_t i = 0; i < (filtered.empty() ? 1 : filtered.size()); ++i) {
            const auto row_id = s->rows[filtered.empty() ? s->rows.size() - 1 : filtered[i]];
            auto* row = services_->components().state<SelectRow>(row_id);
            const float padding = std::min(metrics.option_padding_inline, menu.width / 2);
            engine.set_layout(row->node, layout::BoxLayout{{padding, metrics.option_padding_block, padding,
                                                            metrics.option_padding_block}});
            static_cast<void>(engine.measure_child(
                row->node, {menu.width, menu.width, metrics.option_height, metrics.option_height}));
            engine.place_child(row->node, {menu.x, y, menu.width, metrics.option_height});
            y += metrics.option_height;
        }
        s->needs_layout = false;
        s->layout_anchor = anchor;
        s->layout_clip = clip;
    }
}

void SelectComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect) {
    const runtime::Rect window{0, 0, viewport.width, viewport.height};
    for (auto id : mounted_) {
        const auto* s = services_->components().state<SelectState>(id);
        if (!s) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().select();
        const auto clear_bounds = services_->nodes().require(services_->components().root(s->clear_wrapper)).bounds;
        auto clear = quad(clear_bounds, token.colors.clear_background);
        clear.opacity = s->clear_visible.get() ? 1.0F : 0.0F;
        static_cast<void>(services_->surfaces().update_surface(s->clear_background, std::span{&clear, 1}));
        if (!s->visible) {
            continue;
        }
        const auto bounds = s->popup_bounds;
        auto background = quad(bounds, token.colors.popup_background, token.metrics.border_radius);
        static_cast<void>(services_->surfaces().update_surface(s->background, std::span{&background, 1}));
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.shape = {{bounds.x, bounds.y, bounds.width, bounds.height}, token.metrics.border_radius};
        effects.shadows = token.shadow;
        effects.ancestor_clip = graphics::EffectClip{1, window};
        static_cast<void>(services_->surfaces().update_effects(s->background, effects));
        const float padding = std::min(token.metrics.popup_padding, std::min(bounds.width, bounds.height) / 2);
        const runtime::Rect menu_clip{bounds.x + padding, bounds.y + padding,
                                      std::max(0.0F, bounds.width - padding * 2),
                                      std::max(0.0F, bounds.height - padding * 2)};
        for (auto row_id : s->rows) {
            const auto* row = services_->components().state<SelectRow>(row_id);
            if (!row || !services_->components().branch_active(row_id)) {
                continue;
            }
            const bool selected =
                row->index && s->model.value() == SelectSelection{s->model.options()[*row->index].value};
            const bool active = row->index && s->model.active() == row->index;
            auto item = quad(intersect(services_->nodes().require(row->node).bounds, menu_clip),
                             selected ? token.colors.option_selected_background : token.colors.option_active_background,
                             token.metrics.border_radius);
            item.opacity = selected || (active && !s->loading) ? 1.0F : 0.0F;
            static_cast<void>(services_->surfaces().update_surface(row->surface, std::span{&item, 1}));
            services_->scene_composer().set_fragment(row->hit, {}, row->interaction, menu_clip);
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Select(SelectProps props, std::optional<SelectPrefix> prefix, std::optional<SelectSuffix> suffix) {
    if (!detail::active_selects) {
        throw std::logic_error("Select requires window component services");
    }
    detail::active_selects->mount(props, prefix, suffix);
}
} // namespace ryn
