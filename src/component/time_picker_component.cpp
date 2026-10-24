#include "component/time_picker_component.hpp"
#include "component/input_component.hpp"
#include "component/input_affix_action.hpp"
#include "component/table_component.hpp"
#include "component/date_picker_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "theme/input_tokens.hpp"
#include <ryn/button.hpp>
#include <algorithm>
#include <cmath>

namespace ryn::detail {
namespace {
thread_local TimePickerComponentHost* active_time_pickers{};

struct TimeRow final {
    runtime::NodeId node;
    input::InteractionId interaction;
    runtime::SceneFragmentId hit;
    component::RetainedSurfaceId surface;
    Signal<runtime::SemanticForeground> color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    bool enabled{};
    bool hovered{};
};

struct TimePickerState final {
    explicit TimePickerState(TimePickerModel value) : model(std::move(value)) {}

    TimePickerModel model;
    TimePickerSnapshot view;
    runtime::NodeId node;
    runtime::ComponentId actions;
    input::InteractionId selector_hit;
    input::InteractionId popup_hit;
    input::TextInputOwnerId editor;
    component::RetainedSurfaceId background;
    TimePickerRef reference;
    Signal<String> display{String{}};
    Signal<bool> clear_visible{false};
    Signal<bool> action_disabled{false};
    Signal<bool> confirm_disabled{false};
    Signal<ThemeConfig> input_theme{ThemeConfig{}};
    std::array<theme_runtime::Subscription, 4> subscriptions;
    std::array<bool, 3> reveal{true, true, true};
    bool open{};
    bool controlled_open{};
    bool suppressed{};
    bool disabled{};
    bool allow_clear{true};
    bool need_confirm{true};
    bool needs_layout{true};
    bool focus_within{};
    runtime::Rect anchor;
    runtime::Rect clip;
    std::function<void(TimePickerValue)> on_change;
    std::function<void(bool)> on_open;
};

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto current = std::optional{child}; current; current = services.components().parent(*current)) {
        if (*current == parent) {
            return true;
        }
    }
    return false;
}

std::optional<runtime::ComponentId> nearest_layer(WindowComponentServices& services, runtime::ComponentId id) {
    for (auto current = std::optional{id}; current; current = services.components().parent(*current)) {
        if (services.components().is_window_layer_root(*current)) {
            return current;
        }
    }
    return {};
}

bool inside(runtime::Rect b, float x, float y) {
    return x >= b.x && y >= b.y && x < b.x + b.width && y < b.y + b.height;
}

runtime::Rect intersect(runtime::Rect a, runtime::Rect b) {
    const float x = std::max(a.x, b.x);
    const float y = std::max(a.y, b.y);
    return {x, y, std::max(0.0F, std::min(a.x + a.width, b.x + b.width) - x),
            std::max(0.0F, std::min(a.y + a.height, b.y + b.height) - y)};
}

runtime::SemanticForeground foreground(Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

graphics::QuadInstance quad(runtime::Rect bounds, Color color, float radius = 0) {
    graphics::QuadInstance result;
    result.bounds = {bounds.x, bounds.y, bounds.width, bounds.height};
    result.color = foreground(color);
    result.corner_radius = std::min(radius, std::min(bounds.width, bounds.height) / 2);
    return result;
}

runtime::ComponentId group(WindowComponentServices& services, layout::LayoutModel model, bool clip = false) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    services.layout().set_layout(node, std::move(model));
    services.nodes().require(node).clip_content = clip;
    build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
    return id;
}

int field(TimeOfDay value, int column) {
    return column == 0 ? value.hour() : column == 1 ? value.minute() : value.second();
}

int step(const TimePickerModel& model, int column) {
    const auto& r = model.rules();
    return column == 0 ? r.hour_step : column == 1 ? r.minute_step : r.second_step;
}
} // namespace

TimePickerComponentHost::TimePickerComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

TimePickerComponentHost::~TimePickerComponentHost() {
    services_->detach(*this);
}

void* TimePickerComponentHost::begin_mount() noexcept {
    auto* previous = active_time_pickers;
    active_time_pickers = this;
    return previous;
}

void TimePickerComponentHost::end_mount(void* previous) noexcept {
    active_time_pickers = static_cast<TimePickerComponentHost*>(previous);
}

void TimePickerComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

TimePickerSnapshot TimePickerComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<TimePickerState>(id);
    if (!s) {
        throw std::out_of_range("TimePicker component is stale");
    }
    auto result = s->view;
    result.value = s->model.value();
    result.pending = s->model.panel();
    result.draft = s->model.draft();
    result.confirmable = s->model.confirmable();
    return result;
}

void TimePickerComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const bool visible =
        s->open && !s->suppressed && !s->disabled && window_active_ && services_->interactions().component_active(id);
    const bool changed = visible != s->view.visible;
    s->view.visible = visible;
    if (changed || !visible) {
        const auto focused = services_->focus().state().focused;
        const auto* target = focused ? services_->interactions().find(*focused) : nullptr;
        s->focus_within = visible && target && descendant(*services_, target->component, id);
    }
    bool structure = services_->components().set_branch_active(s->view.popup, visible);
    structure |= services_->components().set_branch_active(s->actions, s->need_confirm);
    const bool seconds = s->model.precision() == TimePickerPrecision::Second;
    structure |= services_->components().set_branch_active(s->view.columns[2], seconds);
    if (structure) {
        services_->mark_scene_structure_dirty();
    }
    s->display.set(s->model.draft());
    s->clear_visible.set(s->allow_clear && s->model.value().has_value() && !s->disabled);
    s->action_disabled.set(s->disabled);
    s->confirm_disabled.set(!s->model.confirmable() || (s->model.text_dirty() && !s->model.text_value()));
    const auto& token = services_->components().theme_scope(id)->snapshot().time_picker();
    for (int column = 0; column < 3; ++column) {
        for (std::size_t index = 0; index < s->view.rows[column].size(); ++index) {
            const auto row_id = s->view.rows[column][index];
            auto* row = services_->components().state<TimeRow>(row_id);
            const bool shown = static_cast<int>(index) % step(s->model, column) == 0;
            structure |= services_->components().set_branch_active(row_id, shown);
            row->enabled = shown && (column != 2 || seconds) &&
                           s->model.candidate(static_cast<TimeColumn>(column), static_cast<int>(index)).has_value();
            services_->interactions().set_eligible(row->interaction, visible && row->enabled);
            const bool selected = field(s->model.panel(), column) == static_cast<int>(index);
            row->color.set(foreground(!row->enabled ? token.colors.disabled
                                      : selected    ? token.colors.selected
                                                    : token.colors.text));
            row->font.set({token.metrics.font_family, 400, false, token.metrics.font_size, token.metrics.line_height});
        }
    }
    if (structure) {
        services_->mark_scene_structure_dirty();
    }
    if (geometry || changed || structure) {
        s->needs_layout = true;
        if (geometry || changed) {
            s->reveal.fill(true);
        }
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    }
    if (changed && !visible) {
        const auto popup = s->view.popup;
        const auto order = services_->interactions().declaration_order();
        const std::vector<input::InteractionId> hits{order.begin(), order.end()};
        for (auto hit : hits) {
            const auto* record = services_->interactions().find(hit);
            if (record && services_->components().contains(popup) && descendant(*services_, record->component, popup)) {
                // FocusManager must deliver blur/sanitize and deferred focus
                // after the current command; only pointer state is cancelled.
                services_->pointer().cancel_pointer_interaction(hit);
            }
        }
    }
}

void TimePickerComponentHost::request_open(runtime::ComponentId id, bool open, bool restore) {
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s || !services_->components().scope(id).active() ||
        (open && (s->disabled || !window_active_ || !services_->interactions().component_active(id)))) {
        return;
    }
    const bool changed = open ? (!s->open || s->suppressed) : (s->open && !s->suppressed);
    const auto callback = s->on_open;
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    const bool in_popup = record && descendant(*services_, record->component, s->view.popup);
    const auto selector = s->selector_hit;
    if (!s->controlled_open) {
        s->open = open;
    }
    if (open) {
        s->suppressed = false;
    }
    if (!open || changed) {
        s->model.cancel();
        s->focus_within = false;
    }
    refresh(id);
    if (changed && callback) {
        callback(open);
    }
    // A controlled parent may accept, reject, destroy or reopen synchronously.
    // Restore only for an actual close of this same component/focus owner.
    s = services_->components().state<TimePickerState>(id);
    if (!open && in_popup && s && !s->view.visible && services_->focus().state().focused == focused) {
        services_->focus().defer_focus(restore ? std::optional{selector} : std::nullopt,
                                       input::FocusModality::keyboard);
    }
}

void TimePickerComponentHost::suppress(runtime::ComponentId id) {
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s || !s->open || s->suppressed) {
        return;
    }
    const auto callback = s->on_open;
    s->suppressed = true;
    s->focus_within = false;
    if (!s->controlled_open) {
        s->open = false;
    }
    s->model.cancel();
    refresh(id);
    if (callback) {
        callback(false);
    }
}

void TimePickerComponentHost::publish(runtime::ComponentId id, TimePickerValue value, bool close) {
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s || s->disabled || !services_->interactions().component_active(id)) {
        return;
    }
    const auto before = s->model.value();
    const auto intent = s->model.request(value);
    if (!intent) {
        return;
    }
    const auto callback = s->on_change;
    if (close) {
        s->model.cancel();
    }
    refresh(id);
    if (before != *intent && callback) {
        callback(*intent);
    }
    if (close && services_->components().contains(id)) {
        request_open(id, false, true);
    }
}

void TimePickerComponentHost::confirm(runtime::ComponentId id) {
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s) {
        return;
    }
    if (s->model.text_dirty()) {
        const auto value = s->model.text_value();
        if (value) {
            publish(id, *value, true);
        }
    } else if (s->model.confirmable()) {
        publish(id, s->model.panel(), true);
    }
}

void TimePickerComponentHost::choose(runtime::ComponentId id, TimeColumn column, int value) {
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s || !s->view.visible || !s->model.candidate(column, value)) {
        return;
    }
    s->model.select(column, value);
    s->view.active_column = static_cast<int>(column);
    refresh(id);
    services_->focus().defer_focus(s->selector_hit, input::FocusModality::pointer);
    if (!s->need_confirm) {
        publish(id, s->model.panel(), false);
    }
}

void TimePickerComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<TimePickerState>(id);
    if (!s) {
        return;
    }
    const auto& theme = services_->components().theme_scope(id)->snapshot();
    const auto& token = theme.time_picker();
    ThemeConfig config;
    config.input.tokens.background = token.colors.input_background;
    config.input.tokens.hover_background = token.colors.input_background;
    config.input.tokens.active_background = token.colors.input_background;
    config.input.tokens.hover_border_color = token.colors.hover_border;
    config.input.tokens.active_border_color = token.colors.active_border;
    const auto& original = InputTokenAccess::get(theme).active_shadow;
    std::array<ShadowLayer, ShadowList::capacity> shadows{};
    std::copy(original.layers().begin(), original.layers().end(), shadows.begin());
    for (std::size_t i = 0; i < original.size(); ++i) {
        shadows[i].color = token.colors.active_outline;
    }
    config.input.tokens.active_shadow = ShadowList{std::span<const ShadowLayer>{shadows.data(), original.size()}};
    s->input_theme.set(std::move(config));
    if (services_->components().set_window_layer(s->view.popup, token.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    refresh(id, geometry);
    // Panel-only colors and order may leave every child Prop unchanged.
    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
}

void TimePickerComponentHost::mount_rows(runtime::ComponentId id, int column) {
    auto& services = *services_;
    auto* s = services.components().state<TimePickerState>(id);
    auto& build = runtime::require_component_build_context();
    const int count = column == 0 ? 24 : 60;
    for (int value = 0; value < count; ++value) {
        const auto row_id = build.mount_component<TimeRow>();
        auto& row = build.state<TimeRow>(row_id);
        row.node = build.root(row_id);
        services.layout().set_layout(row.node, layout::BoxLayout{});
        services.nodes().require(row.node).clip_content = true;
        row.interaction =
            services.interactions().create({row_id, row.node, s->popup_hit, false, false, {}, false, false});
        input::InteractionHandlers handlers;
        handlers.target = [this, id, row_id, column, value](input::PointerDispatchContext& event) {
            const auto* s = services_->components().state<TimePickerState>(id);
            auto* row = services_->components().state<TimeRow>(row_id);
            if (!s || !row || !row->enabled || !s->view.visible) {
                return;
            }
            if (event.kind() == input::PointerEventKind::enter || event.kind() == input::PointerEventKind::leave) {
                row->hovered = event.kind() == input::PointerEventKind::enter;
                services_->dirty().invalidate(row->node, runtime::DirtyFlags::Material);
            }
            if (event.kind() == input::PointerEventKind::down &&
                event.event().button == input::PointerButton::primary) {
                static_cast<void>(event.capture_pointer());
                event.stop_propagation();
            }
            if (event.kind() == input::PointerEventKind::up && event.event().button == input::PointerButton::primary) {
                static_cast<void>(event.release_pointer_capture());
                const auto bounds =
                    intersect(services_->nodes().require(row->node).bounds, s->view.column_clips[column]);
                const bool hit = event.press_origin() == row->interaction &&
                                 inside(bounds, event.event().x, event.event().y) &&
                                 services_->hit_test().hit_test({event.event().x, event.event().y}) == row->interaction;
                event.stop_propagation();
                if (hit) {
                    choose(id, static_cast<TimeColumn>(column), value);
                }
            }
        };
        services.interactions().set_handlers(row.interaction, std::move(handlers));
        row.hit = build.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
        services.scene_composer().set_fragment(row.hit, {}, row.interaction);
        const auto fragment = build.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
        graphics::QuadInstance initial;
        initial.opacity = 0;
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        row.surface = services.surfaces().create_surface(row_id, row.node, fragment, std::span{&initial, 1}, effects);
        build.on_resource_cleanup(
            row_id, [&services, node = row.node, interaction = row.interaction, hit = row.hit, surface = row.surface] {
                services.pointer().cancel_interaction(interaction);
                services.interactions().remove(interaction);
                services.surfaces().destroy(surface);
                services.scene_composer().remove_fragment(hit);
                services.layout().remove_layout(node);
            });
        const auto formatted = format_time_of_day(TimeOfDay{0, 0, value});
        const auto text = String::from_utf8(formatted.view().bytes().substr(6, 2)).value();
        build.mount_slot_with_semantic_text_style(row_id, Content{[text] { Text(text); }},
                                                  Prop<runtime::SemanticForeground>{row.color},
                                                  Prop<runtime::SemanticTypography>{row.font});
        s->view.rows[column].push_back(row_id);
    }
}

void TimePickerComponentHost::mount(const TimePickerProps& props, const std::optional<TimePickerFooter>& footer) {
    auto& services = *services_;
    auto* inputs = services.input_runtime();
    if (!inputs) {
        throw std::logic_error("TimePicker requires a window Input host");
    }
    if ((props.value_ && props.default_value_) || (props.open_ && props.default_open_)) {
        throw std::invalid_argument("TimePicker cannot combine controlled and default values");
    }
    TimePickerModel model{read_prop(props.rules_), read_prop(props.precision_),
                          props.value_ ? read_prop(*props.value_) : props.default_value_.value_or(TimePickerValue{}),
                          props.value_.has_value(), props.panel_value_};
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<TimePickerState>(std::move(model));
    auto& s = build.state<TimePickerState>(id);
    s.node = build.root(id);
    s.reference = props.ref_.value_or(TimePickerRef{});
    s.open = props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false);
    s.controlled_open = props.open_.has_value();
    s.disabled = read_prop(props.disabled_);
    s.allow_clear = read_prop(props.allow_clear_);
    s.need_confirm = read_prop(props.need_confirm_);
    s.on_change = props.on_change_;
    s.on_open = props.on_open_;
    s.display.set(s.model.draft());
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.mount_slot(
        id, Content{[&] {
            Theme(ThemeProps{}.config(s.input_theme), ThemeContent{[&] {
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
                                .onChange([this, id](String value) {
                                    auto* s = services_->components().state<TimePickerState>(id);
                                    if (!s || s->disabled) {
                                        return;
                                    }
                                    s->model.edit(std::move(value));
                                    refresh(id);
                                })
                                .onSubmit([this, id](String) { confirm(id); })
                                .onBlur([this, id] {
                                    auto* s = services_->components().state<TimePickerState>(id);
                                    if (!s || !services_->components().scope(id).active()) {
                                        return;
                                    }
                                    const auto focused = services_->focus().state().focused;
                                    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
                                    if (record && descendant(*services_, record->component, s->view.popup)) {
                                        return;
                                    }
                                    s->model.cancel();
                                    // Pointer outside and prepare's actual focus leave
                                    // issue the close request once, after focus transfer.
                                    refresh(id, false);
                                }),
                            {}, InputSuffix{[&] {
                                s.view.clear = group(services, layout::BoxLayout{});
                                runtime::require_component_build_context().mount_slot(
                                    s.view.clear, Content{[&] {
                                        mount_input_affix_action(
                                            services, IconSource{IconName::CloseCircleFilled}, s.action_disabled,
                                            [this, id] { publish(id, {}, true); }, s.clear_visible, false, false);
                                    }});
                                Icon(IconProps{}
                                         .source(IconSource{IconName::ClockCircleOutlined})
                                         .tone(TextTone::Secondary));
                            }});
                      const auto input = inputs->mounted_inputs().back();
                      s.view.selector = input.component;
                      s.selector_hit = input.interaction;
                      s.editor = input.editor;
                      inputs->configure_form_reset(
                          input.component, [this, id, editor = input.editor, value = props.value_] {
                              // Reading a Binding invokes user code and may destroy
                              // this owner. Never keep its state across that read.
                              const auto accepted = value ? read_prop(*value) : TimePickerValue{};
                              auto* live = services_->components().state<TimePickerState>(id);
                              if (!live || live->editor != editor || !services_->components().scope(id).active()) {
                                  return;
                              }
                              if (value) {
                                  live->model.reconcile(accepted);
                              } else {
                                  live->model.cancel();
                              }
                              refresh(id);
                          });
                  }});
            s.view.popup = group(services, layout::LeafLayout{}, true);
            const auto popup_node = build.root(s.view.popup);
            services.components().set_window_layer(s.view.popup,
                                                   build.theme_scope()->snapshot().time_picker().z_index_popup);
            s.popup_hit = services.interactions().create({s.view.popup, popup_node, {}, true, false, {}, false, false});
            const auto fragment =
                build.register_scene_fragment(s.view.popup, runtime::SceneFragmentPlacement::before_children);
            std::array<graphics::QuadInstance, 4> initial{};
            for (auto& layer : initial) {
                layer.opacity = 0;
            }
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = false;
            s.background =
                services.surfaces().create(s.view.popup, popup_node, fragment, s.popup_hit, initial, effects);
            build.on_resource_cleanup(s.view.popup, [&services, hit = s.popup_hit, surface = s.background] {
                services.pointer().cancel_interaction(hit);
                services.interactions().remove(hit);
                services.surfaces().destroy(surface);
            });
            build.mount_slot(s.view.popup, Content{[&] {
                                 for (int column = 0; column < 3; ++column) {
                                     s.view.columns[column] = group(services, layout::LeafLayout{}, true);
                                     build.mount_slot(s.view.columns[column],
                                                      Content{[this, id, column] { mount_rows(id, column); }});
                                 }
                                 s.view.footer = group(services, layout::BoxLayout{}, true);
                                 if (footer) {
                                     build.mount_slot(s.view.footer, Content{SlotContentAccess::function(*footer)});
                                 }
                                 layout::FlexLayout actions;
                                 actions.main_gap = 8;
                                 actions.item_policy = layout::FlexItemPolicy::sequential;
                                 s.actions = group(services, actions, true);
                                 build.mount_slot(
                                     s.actions, Content{[&] {
                                         s.view.cancel = group(services, layout::BoxLayout{}, true);
                                         build.mount_slot(
                                             s.view.cancel, Content{[&] {
                                                 Button(ButtonProps{}.size(ControlSize::Small).onClick([this, id] {
                                                     request_open(id, false, true);
                                                 }),
                                                        ButtonContent{[] { Text(u8"Cancel"); }});
                                             }});
                                         s.view.confirm = group(services, layout::BoxLayout{}, true);
                                         build.mount_slot(s.view.confirm, Content{[&] {
                                                              Button(ButtonProps{}
                                                                         .type(ButtonType::Primary)
                                                                         .size(ControlSize::Small)
                                                                         .disabled(s.confirm_disabled)
                                                                         .onClick([this, id] { confirm(id); }),
                                                                     ButtonContent{[] { Text(u8"OK"); }});
                                                          }});
                                     }});
                             }});
        }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints constraints) {
                        const auto* s = services_->components().state<TimePickerState>(id);
                        return engine.measure_child(services_->components().root(s->view.selector), constraints);
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                        const auto* s = services_->components().state<TimePickerState>(id);
                        engine.place_child(services_->components().root(s->view.selector), bounds);
                    }});
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    mounted_.push_back(id);
    update_theme(id, true);
    auto& scope = build.scope(id);
    if (props.value_) {
        connect_prop(scope, *props.value_, [this, id](TimePickerValue value) {
            if (auto* s = services_->components().state<TimePickerState>(id)) {
                s->model.reconcile(value);
                refresh(id);
            }
        });
    }
    if (props.open_) {
        connect_prop(scope, *props.open_, [this, id](bool value) {
            if (auto* s = services_->components().state<TimePickerState>(id)) {
                s->open = value;
                s->suppressed = false;
                s->model.cancel();
                refresh(id);
            }
        });
    }
    connect_prop(scope, props.rules_, [this, id](TimePickerRules rules) {
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            s->model.configure(std::move(rules), s->model.precision());
            refresh(id);
        }
    });
    connect_prop(scope, props.precision_, [this, id](TimePickerPrecision precision) {
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            s->model.configure(s->model.rules(), precision);
            if (precision == TimePickerPrecision::Minute && s->view.active_column == 2) {
                s->view.active_column = 1;
            }
            refresh(id);
        }
    });
    connect_prop(scope, props.disabled_, [this, id](bool value) {
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            s->disabled = value;
            if (value) {
                suppress(id);
            }
            refresh(id);
        }
    });
    connect_prop(scope, props.allow_clear_, [this, id](bool value) {
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            s->allow_clear = value;
            refresh(id, false);
        }
    });
    connect_prop(scope, props.need_confirm_, [this, id](bool value) {
        if (auto* s = services_->components().state<TimePickerState>(id)) {
            s->need_confirm = value;
            refresh(id);
        }
    });
    const auto theme = build.theme_scope();
    const std::array accessors{
        &theme_runtime::ThemeScope::time_picker_colors, &theme_runtime::ThemeScope::time_picker_metrics,
        &theme_runtime::ThemeScope::time_picker_effects, &theme_runtime::ThemeScope::time_picker_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { update_theme(id, geometry); },
                           [theme, accessor = accessors[i], input_effects = i == 0] {
                               static_cast<void>((theme.get()->*accessor)());
                               if (input_effects) {
                                   static_cast<void>(theme->input_shadows());
                               }
                           });
    }
}

void TimePickerComponentHost::on_window_active(bool active) {
    window_active_ = active;
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!active) {
            suppress(id);
        }
        refresh(id);
    }
}

void TimePickerComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        const auto* s = services_->components().state<TimePickerState>(id);
        if (s && s->open && !s->suppressed && !services_->interactions().component_active(id)) {
            suppress(id);
        } else {
            refresh(id, false);
        }
    }
}

void TimePickerComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<TimePickerState>(id);
        if (s && s->open && !s->suppressed && !services_->interactions().component_active(id)) {
            suppress(id);
        } else if (s && s->view.visible) {
            const auto focused = services_->focus().state().focused;
            const auto* target = focused ? services_->interactions().find(*focused) : nullptr;
            const bool within = target && descendant(*services_, target->component, id);
            const bool left = s->focus_within && !within;
            s->focus_within = within;
            if (left) {
                request_open(id, false);
            }
        }
    }
}

bool TimePickerComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.action != input::KeyAction::down || !window_active_) {
        return false;
    }
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    if (!record) {
        return false;
    }
    std::optional<runtime::ComponentId> owner;
    for (auto current = std::optional{record->component}; current; current = services_->components().parent(*current)) {
        if (services_->components().state<TimePickerState>(*current)) {
            owner = current;
            break;
        }
        // A Date selector lives in the parent's layer, but its open picker
        // owns Escape before the enclosing Time footer does.
        const auto dates = services_->date_picker().mounted();
        if (std::ranges::find(dates, *current) != dates.end() && services_->date_picker().snapshot(*current).visible) {
            return false;
        }
    }
    if (!owner) {
        return false;
    }
    auto* s = services_->components().state<TimePickerState>(*owner);
    if (!s || s->disabled || !services_->interactions().component_active(*owner)) {
        return false;
    }
    const auto layer = nearest_layer(*services_, record->component);
    const bool selector = *focused == s->selector_hit;
    if (!selector && layer != s->view.popup) {
        return false;
    }
    for (const auto& input : services_->input_runtime()->mounted_inputs()) {
        if (input.interaction == focused &&
            services_->input_runtime()->editors().require(input.editor).composition().active) {
            return false;
        }
    }
    using input::Key;
    if (event.key == Key::escape && s->view.visible) {
        request_open(*owner, false, true);
        return true;
    }
    // Footer content keeps native editor/navigation/activation behavior.
    if (!selector && *focused != s->popup_hit) {
        return false;
    }
    if (event.key == Key::tab) {
        return false;
    }
    if (event.modifiers != input::KeyModifier::none) {
        return false;
    }
    if (event.key == Key::enter) {
        if (!event.repeat) {
            if (s->view.visible || s->model.text_dirty()) {
                confirm(*owner);
            } else {
                request_open(*owner, true);
            }
        }
        return true;
    }
    if (event.key != Key::up && event.key != Key::down && event.key != Key::left && event.key != Key::right &&
        event.key != Key::home && event.key != Key::end) {
        return false;
    }
    if (!s->view.visible && event.key != Key::up && event.key != Key::down) {
        return false;
    }
    if (!s->view.visible) {
        request_open(*owner, true);
        return true;
    }
    if (event.key == Key::left || event.key == Key::right) {
        const int columns = s->model.precision() == TimePickerPrecision::Minute ? 2 : 3;
        s->view.active_column = std::clamp(s->view.active_column + (event.key == Key::left ? -1 : 1), 0, columns - 1);
        refresh(*owner);
        return true;
    }
    const bool changed = s->model.navigate(static_cast<TimeColumn>(s->view.active_column),
                                           event.key == Key::up || event.key == Key::end ? -1 : 1,
                                           event.key == Key::home || event.key == Key::end);
    refresh(*owner);
    if (changed && !s->need_confirm) {
        publish(*owner, s->model.panel(), false);
    }
    return true;
}

void TimePickerComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                               std::optional<input::InteractionId> hit,
                                               std::optional<input::InteractionId> origin) {
    if (event.action != input::PointerAction::down && event.action != input::PointerAction::up) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        const auto* s = services_->components().state<TimePickerState>(id);
        if (!s || s->disabled) {
            continue;
        }
        const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
        const bool within = record && descendant(*services_, record->component, id);
        if (event.action == input::PointerAction::down && !within) {
            request_open(id, false);
        } else if (event.action == input::PointerAction::up && event.button == input::PointerButton::primary &&
                   hit == s->selector_hit && origin == hit) {
            request_open(id, true);
        }
    }
}

bool TimePickerComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    if (!target) {
        return false;
    }
    const auto layer = nearest_layer(*services_, target->component);
    for (auto id : mounted_) {
        auto* s = services_->components().state<TimePickerState>(id);
        if (!s || !s->view.visible || layer != s->view.popup) {
            continue;
        }
        for (int column = 0; column < 3; ++column) {
            const auto clip = s->view.column_clips[column];
            if (!inside(clip, event.x, event.y)) {
                continue;
            }
            const auto& metrics = services_->components().theme_scope(id)->snapshot().time_picker().metrics;
            const int count = column == 0 ? 24 : 60;
            const float content = static_cast<float>((count - 1) / step(s->model, column) + 1) * metrics.cell_height;
            const float next = std::clamp(s->view.scroll[column] - event.delta_y * metrics.cell_height * 3, 0.0F,
                                          std::max(0.0F, content - clip.height));
            if (next != s->view.scroll[column]) {
                s->view.scroll[column] = next;
                s->reveal[column] = false;
                s->needs_layout = true;
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
            }
            return true;
        }
    }
    return false;
}

bool TimePickerComponentHost::consume_scroll_boundary(const input::ScrollInputEvent& event) const {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    if (!target) {
        return false;
    }
    const auto layer = nearest_layer(*services_, target->component);
    return std::ranges::any_of(mounted_, [&](auto id) {
        const auto* s = services_->components().state<TimePickerState>(id);
        return s && s->view.visible && layer == s->view.popup;
    });
}

void TimePickerComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    // Parent TimePicker is appended after its footer children; position ancestors first.
    const auto ids = mounted_;
    bool positioned = false;
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        const auto id = *it;
        auto* s = services_->components().state<TimePickerState>(id);
        if (!s || !s->view.visible) {
            continue;
        }
        auto& engine = services_->layout();
        const auto& node = services_->nodes().require(s->node);
        const runtime::Rect anchor{node.bounds.x + node.translation.x, node.bounds.y + node.translation.y,
                                   node.bounds.width, node.bounds.height};
        const auto popup_node = services_->components().root(s->view.popup);
        if (!s->needs_layout && anchor == s->anchor && clip == s->clip &&
            services_->nodes().require(popup_node).measure_generation == engine.generation()) {
            continue;
        }
        const auto& m = services_->components().theme_scope(id)->snapshot().time_picker().metrics;
        const int columns = s->model.precision() == TimePickerPrecision::Minute ? 2 : 3;
        const float width = std::min(clip.width, std::max(anchor.width, m.column_width * columns + m.padding * 2));
        const float inner_width = std::max(0.0F, width - std::min(m.padding * 2, width));
        const auto footer_node = services_->components().root(s->view.footer);
        const auto actions_node = services_->components().root(s->actions);
        const auto footer_size = engine.measure_child(footer_node, {0, inner_width, 0, clip.height});
        const auto actions_size =
            s->need_confirm ? engine.measure_child(actions_node, {0, inner_width, 0, clip.height}) : runtime::Size{};
        const float footer_height = footer_size.height > 0 ? footer_size.height + m.footer_padding * 2 : 0;
        const float actions_height = s->need_confirm ? actions_size.height + m.footer_padding * 2 : 0;
        const float wanted = m.column_height + m.padding * 2 + footer_height + actions_height;
        const float below = std::clamp(clip.y + clip.height - anchor.y - anchor.height - m.gap, 0.0F, clip.height);
        const float above = std::clamp(anchor.y - m.gap - clip.y, 0.0F, clip.height);
        const bool upwards = below < wanted && above > below;
        const float height = std::min(wanted, upwards ? above : below);
        s->view.bounds = {std::clamp(anchor.x, clip.x, clip.x + clip.width - width),
                          std::clamp(upwards ? anchor.y - m.gap - height : anchor.y + anchor.height + m.gap, clip.y,
                                     clip.y + clip.height - height),
                          width, height};
        static_cast<void>(engine.measure_child(popup_node, layout::Constraints::fixed(width, height)));
        engine.place_child(popup_node, s->view.bounds);
        const float inset = std::min(m.padding, std::min(width, height) / 2);
        const runtime::Rect inner{s->view.bounds.x + inset, s->view.bounds.y + inset, std::max(0.0F, width - inset * 2),
                                  std::max(0.0F, height - inset * 2)};
        const float rows_height = std::max(0.0F, inner.height - footer_height - actions_height);
        for (int column = 0; column < 3; ++column) {
            auto rect =
                runtime::Rect{inner.x + inner.width * column / columns, inner.y, inner.width / columns, rows_height};
            if (column >= columns) {
                rect = {};
            }
            s->view.column_clips[column] = rect;
            const auto column_node = services_->components().root(s->view.columns[column]);
            static_cast<void>(engine.measure_child(column_node, layout::Constraints::fixed(rect.width, rect.height)));
            engine.place_child(column_node, rect);
            const int count = column == 0 ? 24 : 60;
            const int stride = step(s->model, column);
            const float content = static_cast<float>((count - 1) / stride + 1) * m.cell_height;
            auto& scroll = s->view.scroll[column];
            const float maximum = std::max(0.0F, content - rows_height);
            scroll = std::clamp(scroll, 0.0F, maximum);
            if (s->reveal[column]) {
                const float top = static_cast<float>(field(s->model.panel(), column) / stride) * m.cell_height;
                scroll = std::clamp(rows_height < m.cell_height
                                        ? top
                                        : std::clamp(scroll, std::max(0.0F, top + m.cell_height - rows_height), top),
                                    0.0F, maximum);
            }
            s->reveal[column] = false;
            for (int value = 0; value < count; value += stride) {
                const auto* row = services_->components().state<TimeRow>(s->view.rows[column][value]);
                const float padding = std::min(m.padding, rect.width / 2);
                engine.set_layout(row->node, layout::BoxLayout{{padding, 0, padding, 0}});
                static_cast<void>(
                    engine.measure_child(row->node, layout::Constraints::fixed(rect.width, m.cell_height)));
                engine.place_child(
                    row->node, {rect.x, rect.y + value / stride * m.cell_height - scroll, rect.width, m.cell_height});
            }
        }
        const float footer_top = inner.y + rows_height;
        const float fp = std::min(m.footer_padding, inner.width / 2);
        const runtime::Rect footer_rect{inner.x + fp, footer_top + std::min(fp, footer_height / 2),
                                        std::max(0.0F, inner.width - fp * 2), std::max(0.0F, footer_height - fp * 2)};
        static_cast<void>(
            engine.measure_child(footer_node, layout::Constraints::fixed(footer_rect.width, footer_rect.height)));
        engine.place_child(footer_node, footer_rect);
        const runtime::Rect actions_rect{inner.x + fp, footer_top + footer_height + std::min(fp, actions_height / 2),
                                         std::max(0.0F, inner.width - fp * 2), std::max(0.0F, actions_height - fp * 2)};
        static_cast<void>(
            engine.measure_child(actions_node, layout::Constraints::fixed(actions_rect.width, actions_rect.height)));
        engine.place_child(actions_node, actions_rect);
        s->needs_layout = false;
        s->anchor = anchor;
        s->clip = clip;
        positioned = true;
        // Date's normal pass precedes Time's. Refresh this footer's final Date
        // anchors before placing any deeper retained Time children.
        if (std::ranges::any_of(services_->date_picker().mounted(),
                                [&](auto date) { return descendant(*services_, date, s->view.footer); })) {
            services_->date_picker().position_window_layers(viewport, clip);
        }
    }
    // Footer placement can change a retained Table's ancestor clip after the
    // normal Modal -> Table pass. Refresh its cached clip/scroll translations
    // before later child popup positioning, without changing dispatch order.
    if (positioned && std::ranges::any_of(services_->table().mounted(), [&](auto table) {
            return std::ranges::any_of(ids, [&](auto id) {
                const auto* s = services_->components().state<TimePickerState>(id);
                return s && s->view.visible && descendant(*services_, table, s->view.footer);
            });
        })) {
        services_->table().position_window_layers(viewport, clip);
    }
}

void TimePickerComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect) {
    const runtime::Rect window{0, 0, viewport.width, viewport.height};
    for (auto id : mounted_) {
        const auto* s = services_->components().state<TimePickerState>(id);
        if (!s || !s->view.visible) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().time_picker();
        const auto background = quad(s->view.bounds, t.colors.background, t.metrics.border_radius);
        std::array<graphics::QuadInstance, 4> panel{};
        panel[0] = background;
        const int count = s->model.precision() == TimePickerPrecision::Minute ? 2 : 3;
        const auto b = s->view.bounds;
        const float border = std::min(t.metrics.border_width, std::min(b.width, b.height) / 2);
        for (int column = 1; column < count; ++column) {
            const auto c = s->view.column_clips[column];
            panel[column] = quad(intersect({c.x, c.y, border, c.height}, b), t.colors.border);
        }
        const auto c = s->view.column_clips[0];
        panel[3] = quad(intersect({b.x, c.y + c.height, b.width, border}, b), t.colors.border);
        static_cast<void>(services_->surfaces().update_surface(s->background, panel));
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.shape = {{s->view.bounds.x, s->view.bounds.y, s->view.bounds.width, s->view.bounds.height},
                         background.corner_radius};
        effects.shadows = t.effects.shadow;
        effects.ancestor_clip = graphics::EffectClip{1, window};
        static_cast<void>(services_->surfaces().update_effects(s->background, effects));
        for (int column = 0; column < 3; ++column) {
            const auto clip = s->view.column_clips[column];
            for (std::size_t value = 0; value < s->view.rows[column].size(); ++value) {
                const auto* row = services_->components().state<TimeRow>(s->view.rows[column][value]);
                const bool selected = static_cast<int>(value) == field(s->model.panel(), column);
                const auto bounds = intersect(services_->nodes().require(row->node).bounds, clip);
                auto item = quad(bounds, selected ? t.colors.selected_background : t.colors.hover_background,
                                 t.metrics.border_radius);
                item.opacity = selected || (row->enabled && row->hovered) ? 1.0F : 0.0F;
                static_cast<void>(services_->surfaces().update_surface(row->surface, std::span{&item, 1}));
                services_->scene_composer().set_fragment(row->hit, {}, row->interaction, clip);
            }
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void TimePicker(TimePickerProps props, std::optional<TimePickerFooter> footer) {
    if (!detail::active_time_pickers) {
        throw std::logic_error("TimePicker requires window component services");
    }
    detail::active_time_pickers->mount(props, footer);
}
} // namespace ryn
