#include "component/date_picker_component.hpp"
#include "component/date_picker_model.hpp"
#include "component/input_component.hpp"
#include "component/input_affix_action.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <stdexcept>

namespace ryn::detail {
namespace {
thread_local DatePickerComponentHost* active_dates{};

struct DateItem final {
    runtime::NodeId node;
    input::InteractionId interaction;
    runtime::SceneFragmentId hit;
    component::RetainedSurfaceId surface;
    DatePickerValue date;
    int months{};
    Signal<String> text{String{}};
    Signal<runtime::SemanticForeground> color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
};

struct DateState final {
    explicit DateState(DatePickerModel value) : model(std::move(value)) {}

    DatePickerModel model;
    DatePickerRules rules;
    runtime::NodeId node;
    runtime::ComponentId input;
    runtime::ComponentId popup;
    runtime::ComponentId footer;
    std::vector<runtime::ComponentId> items;
    input::InteractionId input_hit;
    input::InteractionId popup_hit;
    input::TextInputOwnerId editor;
    component::RetainedSurfaceId background;
    DatePickerRef reference;
    Signal<String> display{String{}};
    DatePickerValue hovered;
    Signal<bool> clear_visible{false};
    Signal<bool> action_disabled{false};
    Signal<ThemeConfig> input_theme{ThemeConfig{}};
    std::array<theme_runtime::Subscription, 4> subscriptions;
    bool dirty_draft{};
    // Pointer focus/blur runs before the outside-pointer observer in the same dispatch.
    bool closed_on_blur{};
    bool disabled{};
    bool allow_clear{true};
    bool controlled_open{};
    bool open{};
    bool suppressed{};
    bool visible{};
    bool needs_layout{true};
    runtime::Rect bounds;
    runtime::Rect clip;
    runtime::Rect anchor;
    std::function<void(DatePickerValue)> on_change;
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
    for (auto p = std::optional{child}; p; p = services.components().parent(*p)) {
        if (services.components().is_window_layer_root(*p)) {
            return p;
        }
    }
    return {};
}

String display(DatePickerValue value) {
    return value ? format_iso_date(*value) : String{};
}

runtime::ComponentId group(WindowComponentServices& services) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    services.layout().set_layout(node, layout::BoxLayout{});
    build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
    return id;
}
} // namespace

DatePickerComponentHost::DatePickerComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

DatePickerComponentHost::~DatePickerComponentHost() {
    services_->detach(*this);
}

void* DatePickerComponentHost::begin_mount() noexcept {
    auto* old = active_dates;
    active_dates = this;
    return old;
}

void DatePickerComponentHost::end_mount(void* old) noexcept {
    active_dates = static_cast<DatePickerComponentHost*>(old);
}

void DatePickerComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

DatePickerSnapshot DatePickerComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<DateState>(id);
    if (!s) {
        throw std::out_of_range("stale DatePicker");
    }
    std::vector<component::RetainedSurfaceId> surfaces{s->background};
    for (auto child : s->items) {
        surfaces.push_back(services_->components().state<DateItem>(child)->surface);
    }
    return {s->input,
            s->popup,
            s->footer,
            {s->items.begin() + 12, s->items.end()},
            {s->items.begin(), s->items.begin() + 4},
            s->model.value(),
            s->model.active(),
            s->display.get(),
            s->bounds,
            s->visible,
            std::move(surfaces)};
}

void DatePickerComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<DateState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const bool visible =
        s->open && !s->suppressed && !s->disabled && active_ && services_->interactions().component_active(id);
    const bool changed = visible != s->visible;
    s->visible = visible;
    if (!visible) {
        s->hovered.reset();
    }
    if (services_->components().set_branch_active(s->popup, visible)) {
        services_->mark_scene_structure_dirty();
    }
    s->clear_visible.set(s->allow_clear && s->model.value().has_value());
    s->action_disabled.set(s->disabled);
    const auto& t = services_->components().theme_scope(id)->snapshot().date_picker();
    const auto dates = s->model.grid();
    for (std::size_t i = 0; i < s->items.size(); ++i) {
        auto* item = services_->components().state<DateItem>(s->items[i]);
        if (i == 4) {
            item->text.set(
                String::from_utf8(std::string(format_iso_date(s->model.panel()).bytes().substr(0, 7))).value());
        }
        const bool date = i >= 12;
        if (date) {
            item->date = dates[i - 12];
            item->text.set(item->date ? String::from_utf8(std::to_string(item->date->day())).value() : String{});
        }
        const bool enabled = date && item->date && s->model.eligible(*item->date);
        bool navigable = i < 4;
        if (navigable) {
            auto probe = s->model;
            navigable = probe.navigate(item->months);
        }
        services_->interactions().set_eligible(item->interaction, visible && (enabled || navigable));
        const bool selected = date && item->date && item->date == s->model.value();
        const bool muted = date && item->date && item->date->month() != s->model.panel().month();
        item->color.set(foreground((date && !enabled) || (i < 4 && !navigable) ? t.colors.disabled
                                   : selected                                  ? t.colors.selected_text
                                   : muted                                     ? t.colors.muted
                                                                               : t.colors.text));
        item->font.set({t.metrics.font_family, 400, false, t.metrics.font_size, t.metrics.line_height});
    }
    if (geometry || changed) {
        s->needs_layout = true;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    } else {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
    }
    if (changed && !visible) {
        const auto items = s->items;
        const auto hit = s->popup_hit;
        services_->pointer().cancel_interaction(hit);
        for (auto item : items) {
            if (auto* live = services_->components().state<DateItem>(item)) {
                services_->pointer().cancel_interaction(live->interaction);
            }
        }
    }
}

void DatePickerComponentHost::request_open(runtime::ComponentId id, bool open, bool restore) {
    auto* s = services_->components().state<DateState>(id);
    if (!s || !services_->components().scope(id).active() ||
        (open && (s->disabled || !active_ || !services_->interactions().component_active(id)))) {
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
    }
    refresh(id);
    if (changed && callback) {
        callback(open);
    }
    if (restore && services_->components().contains(id) && services_->interactions().component_active(id)) {
        services_->focus().defer_focus(input, input::FocusModality::keyboard);
    }
}

void DatePickerComponentHost::choose(runtime::ComponentId id, DatePickerValue value, bool close) {
    auto* s = services_->components().state<DateState>(id);
    if (!s || s->disabled || !services_->interactions().component_active(id)) {
        return;
    }
    const auto* input = services_->interactions().find(s->input_hit);
    if (!input || !input->eligible || !services_->interactions().component_active(s->input)) {
        return;
    }
    const auto previous = s->model.value();
    const auto intent = s->model.request(value);
    if (!intent) {
        return;
    }
    const auto callback = s->on_change;
    s->dirty_draft = false;
    s->display.set(display(s->model.value()));
    refresh(id);
    if (previous != *intent && callback) {
        callback(*intent);
    }
    if (close && services_->components().contains(id)) {
        request_open(id, false, true);
    }
}

void DatePickerComponentHost::commit(runtime::ComponentId id, bool blur) {
    auto* s = services_->components().state<DateState>(id);
    if (!s) {
        return;
    }
    const bool composing = services_->input_runtime()->editors().require(s->editor).composition().active;
    if (s->dirty_draft && !composing) {
        const auto draft = s->display.get();
        const auto value = parse_iso_date(draft.bytes());
        if (draft.empty() || value) {
            choose(id, value, !blur);
        }
    }
    s = services_->components().state<DateState>(id);
    if (s && blur) {
        s->dirty_draft = false;
        s->display.set(display(s->model.value()));
    }
}

void DatePickerComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<DateState>(id);
    if (!s) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().date_picker();
    ThemeConfig config;
    config.input.tokens.background = t.colors.selector_background;
    config.input.tokens.hover_background = t.colors.selector_background;
    config.input.tokens.active_background = t.colors.selector_background;
    config.input.tokens.hover_border_color = t.colors.hover_border;
    config.input.tokens.active_border_color = t.colors.active_border;
    s->input_theme.set(std::move(config));
    if (services_->components().set_window_layer(s->popup, t.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    refresh(id, geometry);
}

void DatePickerComponentHost::mount_items(runtime::ComponentId id) {
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    auto& s = *services.components().state<DateState>(id);
    const std::array labels{u8"<<", u8"<", u8">", u8">>", u8"", u8"Su", u8"Mo", u8"Tu", u8"We", u8"Th", u8"Fr", u8"Sa"};
    for (std::size_t i = 0; i < 54; ++i) {
        const auto child = build.mount_component<DateItem>();
        auto& item = build.state<DateItem>(child);
        item.node = build.root(child);
        if (i < 12) {
            item.text.set(String::from_utf8(std::u8string_view{labels[i]}).value());
        }
        if (i < 4) {
            item.months = std::array{-12, -1, 1, 12}[i];
        }
        services.layout().set_layout(item.node, layout::BoxLayout{});
        services.nodes().require(item.node).clip_content = true;
        item.interaction =
            services.interactions().create({child, item.node, s.popup_hit, false, false, {}, false, false});
        input::InteractionHandlers handlers;
        handlers.target = [this, id, child](input::PointerDispatchContext& event) {
            auto* s = services_->components().state<DateState>(id);
            const auto* item = services_->components().state<DateItem>(child);
            if (!s || !item || !s->visible || s->disabled) {
                return;
            }
            if (item->date &&
                (event.kind() == input::PointerEventKind::enter || event.kind() == input::PointerEventKind::move ||
                 event.kind() == input::PointerEventKind::leave)) {
                const auto next = event.kind() == input::PointerEventKind::leave ? DatePickerValue{} : item->date;
                if (s->hovered != next) {
                    s->hovered = next;
                    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
                }
            }
            if (event.event().button != input::PointerButton::primary) {
                return;
            }
            if (event.kind() == input::PointerEventKind::down) {
                static_cast<void>(event.capture_pointer());
                event.stop_propagation();
            }
            if (event.kind() == input::PointerEventKind::up) {
                static_cast<void>(event.release_pointer_capture());
                const bool hit = event.press_origin() == item->interaction &&
                                 inside(intersect(services_->nodes().require(item->node).bounds, s->bounds),
                                        event.event().x, event.event().y);
                const auto date = item->date;
                const int months = item->months;
                event.stop_propagation();
                if (hit && date) {
                    choose(id, date);
                } else if (hit && months) {
                    s->model.navigate(months);
                    refresh(id);
                }
            }
        };
        services.interactions().set_handlers(item.interaction, std::move(handlers));
        item.hit = build.register_scene_fragment(child, runtime::SceneFragmentPlacement::before_children);
        services.scene_composer().set_fragment(item.hit, {}, item.interaction);
        const auto body = build.register_scene_fragment(child, runtime::SceneFragmentPlacement::before_children);
        graphics::QuadInstance initial;
        initial.opacity = 0;
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        item.surface = services.surfaces().create_surface(child, item.node, body, std::span{&initial, 1}, effects);
        build.on_resource_cleanup(
            child, [&services, node = item.node, hit = item.interaction, fragment = item.hit, surface = item.surface] {
                services.pointer().cancel_interaction(hit);
                services.interactions().remove(hit);
                services.surfaces().destroy(surface);
                services.scene_composer().remove_fragment(fragment);
                services.layout().remove_layout(node);
            });
        build.mount_slot_with_semantic_text_style(child, Content{[&item] { Text(TextProps{}.content(item.text)); }},
                                                  item.color, item.font);
        services.layout().set_layout(
            item.node,
            layout::ComponentLayout{
                [&services](layout::LayoutEngine& engine, runtime::NodeId node, layout::Constraints limits) {
                    runtime::Size size;
                    for (auto child : services.nodes().require(node).children) {
                        const auto measured = engine.measure_child(child, {0, limits.max_width, 0, limits.max_height});
                        size.width = std::max(size.width, measured.width);
                        size.height = std::max(size.height, measured.height);
                    }
                    return limits.constrain(size);
                },
                [&services](layout::LayoutEngine& engine, runtime::NodeId node, runtime::Rect rect) {
                    for (auto child : services.nodes().require(node).children) {
                        const auto size = engine.measure_child(child, {0, rect.width, 0, rect.height});
                        engine.place_child(child, {rect.x + (rect.width - size.width) / 2,
                                                   rect.y + (rect.height - size.height) / 2, size.width, size.height});
                    }
                }});
        s.items.push_back(child);
    }
}

void DatePickerComponentHost::mount(const DatePickerProps& props, const std::optional<DatePickerFooter>& footer) {
    auto& services = *services_;
    auto* inputs = services.input_runtime();
    if (!inputs) {
        throw std::logic_error("DatePicker requires a window Input host");
    }
    if ((props.value_ && props.default_value_) || (props.open_ && props.default_open_)) {
        throw std::invalid_argument("DatePicker controlled/default props conflict");
    }
    const auto value = props.value_ ? read_prop(*props.value_) : props.default_value_.value_or(DatePickerValue{});
    DatePickerRules rules{read_prop(props.min_), read_prop(props.max_), read_prop(props.disabled_date_)};
    const auto panel = value ? *value : props.panel_date_ ? *props.panel_date_ : DatePickerModel::utc_date();
    DatePickerModel model{value, props.value_.has_value(), panel, rules};
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<DateState>(std::move(model));
    auto& s = build.state<DateState>(id);
    s.node = build.root(id);
    s.rules = rules;
    s.reference = props.ref_.value_or(DatePickerRef{});
    s.disabled = read_prop(props.disabled_);
    s.allow_clear = read_prop(props.allow_clear_);
    s.open = props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false);
    s.controlled_open = props.open_.has_value();
    s.on_change = props.on_change_;
    s.on_open = props.on_open_;
    s.on_focus = props.on_focus_;
    s.on_blur = props.on_blur_;
    s.display.set(display(value));
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.mount_slot(
        id, Content{[&] {
            Theme(ThemeProps{}.config(s.input_theme), ThemeContent{[&] {
                      Input(InputProps{}
                                .value(s.display)
                                .placeholder(props.placeholder_)
                                .readOnly(props.read_only_)
                                .disabled(props.disabled_)
                                .size(props.size_)
                                .variant(props.variant_)
                                .status(props.status_)
                                .ref(s.reference)
                                .autoFocus(props.auto_focus_)
                                .onChange([this, id](String text) {
                                    if (auto* s = services_->components().state<DateState>(id)) {
                                        s->dirty_draft = true;
                                        s->display.set(std::move(text));
                                    }
                                })
                                .onSubmit([this, id](String) { commit(id, false); })
                                .onFocus([this, id] {
                                    if (auto* s = services_->components().state<DateState>(id)) {
                                        s->closed_on_blur = false;
                                        const auto callback = s->on_focus;
                                        if (callback) {
                                            callback();
                                        }
                                    }
                                })
                                .onBlur([this, id] {
                                    auto* s = services_->components().state<DateState>(id);
                                    if (!s || !services_->components().scope(id).active()) {
                                        return;
                                    }
                                    const auto callback = s->on_blur;
                                    const auto focused = services_->focus().state().focused;
                                    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
                                    const bool internal = record && descendant(*services_, record->component, s->popup);
                                    const auto* input = services_->interactions().find(s->input_hit);
                                    const bool input_active = input && input->eligible &&
                                                              services_->interactions().component_active(s->input) &&
                                                              services_->focus().state().window_active;
                                    commit(id, true);
                                    if (!internal && input_active && services_->components().contains(id)) {
                                        services_->components().state<DateState>(id)->closed_on_blur = true;
                                        request_open(id, false);
                                    }
                                    if (services_->components().contains(id) && callback) {
                                        callback();
                                    }
                                }),
                            {}, InputSuffix{[&] {
                                mount_input_affix_action(
                                    services, IconSource{IconName::CloseCircleFilled}, s.action_disabled,
                                    [this, id] { choose(id, {}); }, s.clear_visible, false, false);
                                Icon(IconProps{}.name(IconName::CalendarOutlined).tone(TextTone::Secondary));
                            }});
                      const auto input = inputs->mounted_inputs().back();
                      s.input = input.component;
                      s.input_hit = input.interaction;
                      s.editor = input.editor;
                  }});
            s.popup = group(services);
            const auto popup_node = build.root(s.popup);
            services.layout().set_layout(popup_node, layout::LeafLayout{});
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
            build.on_resource_cleanup(s.popup, [&services, hit = s.popup_hit, surface = s.background] {
                services.pointer().cancel_interaction(hit);
                services.interactions().remove(hit);
                services.surfaces().destroy(surface);
            });
            build.mount_slot(s.popup, Content{[&] {
                                 mount_items(id);
                                 s.footer = group(services);
                                 services.nodes().require(build.root(s.footer)).clip_content = true;
                                 if (footer) {
                                     build.mount_slot(s.footer, Content{SlotContentAccess::function(*footer)});
                                 }
                             }});
        }});
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{[this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                                    const auto& s = *services_->components().state<DateState>(id);
                                    return engine.measure_child(services_->components().root(s.input), limits);
                                },
                                [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                                    const auto& s = *services_->components().state<DateState>(id);
                                    engine.place_child(services_->components().root(s.input), rect);
                                }});
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    mounted_.push_back(id);
    auto& scope = build.scope(id);
    if (props.value_) {
        connect_prop(scope, *props.value_, [this, id](DatePickerValue value) {
            if (auto* s = services_->components().state<DateState>(id)) {
                if (s->model.value() != value) {
                    s->model.reconcile(value);
                    s->dirty_draft = false;
                    s->display.set(display(value));
                    refresh(id);
                }
            }
        });
    }
    if (props.open_) {
        connect_prop(scope, *props.open_, [this, id](bool open) {
            if (auto* s = services_->components().state<DateState>(id)) {
                const bool changed = s->open != open;
                s->open = open;
                if (changed && open) {
                    s->suppressed = false;
                }
                refresh(id);
            }
        });
    }
    connect_prop(scope, props.disabled_, [this, id](bool disabled) {
        if (auto* s = services_->components().state<DateState>(id)) {
            s->disabled = disabled;
            if (disabled) {
                s->suppressed = true;
                s->dirty_draft = false;
                s->display.set(display(s->model.value()));
                request_open(id, false);
            }
            refresh(id);
        }
    });
    connect_prop(scope, props.allow_clear_, [this, id](bool value) {
        if (auto* s = services_->components().state<DateState>(id)) {
            s->allow_clear = value;
            refresh(id, false);
        }
    });
    connect_prop(scope, props.min_, [this, id](DatePickerValue value) {
        if (auto* s = services_->components().state<DateState>(id)) {
            auto rules = s->rules;
            rules.min = value;
            s->model.configure(rules);
            s->rules = std::move(rules);
            refresh(id);
        }
    });
    connect_prop(scope, props.max_, [this, id](DatePickerValue value) {
        if (auto* s = services_->components().state<DateState>(id)) {
            auto rules = s->rules;
            rules.max = value;
            s->model.configure(rules);
            s->rules = std::move(rules);
            refresh(id);
        }
    });
    connect_prop(scope, props.disabled_date_, [this, id](DisabledDate value) {
        if (auto* s = services_->components().state<DateState>(id)) {
            auto rules = s->rules;
            rules.disabled = std::move(value);
            s->model.configure(rules);
            s->rules = std::move(rules);
            refresh(id);
        }
    });
    update_theme(id, true);
    const auto theme = build.theme_scope();
    const std::array accessors{
        &theme_runtime::ThemeScope::date_picker_colors, &theme_runtime::ThemeScope::date_picker_metrics,
        &theme_runtime::ThemeScope::date_picker_effects, &theme_runtime::ThemeScope::date_picker_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { update_theme(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
}

void DatePickerComponentHost::on_window_active(bool active) {
    active_ = active;
    on_input_boundary_changed();
}

void DatePickerComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<DateState>(id);
        if (!s) {
            continue;
        }
        if ((!active_ || !services_->interactions().component_active(id)) && !s->suppressed) {
            s->suppressed = true;
            s->dirty_draft = false;
            s->display.set(display(s->model.value()));
            request_open(id, false);
        }
        refresh(id, false);
    }
}

void DatePickerComponentHost::prepare_auxiliary_layout() {
    for (auto id : mounted_) {
        if (auto* s = services_->components().state<DateState>(id)) {
            s->closed_on_blur = false;
        }
    }
    if (std::ranges::any_of(mounted_, [this](auto id) {
            const auto* s = services_->components().state<DateState>(id);
            return s && !s->suppressed && !services_->interactions().component_active(id);
        })) {
        on_input_boundary_changed();
    }
}

bool DatePickerComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.action != input::KeyAction::down || !active_) {
        return false;
    }
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<DateState>(id);
        if (!s || s->disabled || !services_->interactions().component_active(id) || !record) {
            continue;
        }
        const bool input = focused == s->input_hit;
        const bool panel = descendant(*services_, record->component, s->popup) &&
                           layer(*services_, record->component) == std::optional{s->popup};
        if (!input && !panel) {
            continue;
        }
        const auto session = services_->input_runtime()->sessions().active();
        if (session.owner.valid() &&
            services_->input_runtime()->editors().require(session.owner).composition().active) {
            return false;
        }
        using input::Key;
        if (event.key == Key::escape && s->visible) {
            request_open(id, false, true);
            return true;
        }
        if (!input) {
            return false;
        }
        if (event.key == Key::enter && event.modifiers == input::KeyModifier::none) {
            if (!event.repeat) {
                if (s->dirty_draft) {
                    commit(id, false);
                } else if (s->visible) {
                    choose(id, s->model.active());
                } else {
                    request_open(id, true);
                }
            }
            return true;
        }
        const bool page = event.key == Key::page_up || event.key == Key::page_down;
        if (event.modifiers != input::KeyModifier::none && !(page && event.modifiers == input::KeyModifier::shift)) {
            return false;
        }
        if (!s->visible) {
            if (event.key == Key::down) {
                request_open(id, true);
                return true;
            }
            return false;
        }
        switch (event.key) {
        case Key::left:
            s->model.move(-1);
            break;
        case Key::right:
            s->model.move(1);
            break;
        case Key::up:
            s->model.move(-7);
            break;
        case Key::down:
            s->model.move(7);
            break;
        case Key::home:
            s->model.boundary(false);
            break;
        case Key::end:
            s->model.boundary(true);
            break;
        case Key::page_up:
            s->model.navigate(event.modifiers == input::KeyModifier::shift ? -12 : -1);
            break;
        case Key::page_down:
            s->model.navigate(event.modifiers == input::KeyModifier::shift ? 12 : 1);
            break;
        default:
            return false;
        }
        refresh(id);
        return true;
    }
    return false;
}

void DatePickerComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                               std::optional<input::InteractionId> hit,
                                               std::optional<input::InteractionId> origin) {
    if (event.action != input::PointerAction::down && event.action != input::PointerAction::up) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<DateState>(id);
        if (!s || s->disabled) {
            continue;
        }
        const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
        const bool own = record && descendant(*services_, record->component, id);
        if (event.action == input::PointerAction::down) {
            const bool already_closed = s->closed_on_blur;
            s->closed_on_blur = false;
            if (!own && s->visible && !already_closed) {
                request_open(id, false);
            }
        } else if (event.action == input::PointerAction::up && event.button == input::PointerButton::primary &&
                   hit == s->input_hit && hit == origin) {
            request_open(id, !s->visible);
        }
    }
}

bool DatePickerComponentHost::consume_scroll_boundary(const input::ScrollInputEvent& event) {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
    if (!record) {
        return false;
    }
    const auto owner = layer(*services_, record->component);
    for (auto id : mounted_) {
        const auto* s = services_->components().state<DateState>(id);
        if (s && s->visible && owner == std::optional{s->popup}) {
            return true;
        }
    }
    return false;
}

void DatePickerComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<DateState>(id);
        if (!s || !s->visible) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().date_picker();
        auto anchor = services_->nodes().require(s->node).bounds;
        const auto translation = services_->nodes().require(s->node).translation;
        anchor.x += translation.x;
        anchor.y += translation.y;
        const auto available = services_->component_clip(s->popup, viewport, clip);
        auto& engine = services_->layout();
        const auto popup_node = services_->components().root(s->popup);
        if (!s->needs_layout && s->anchor == anchor && s->clip == available &&
            services_->nodes().require(popup_node).measure_generation == engine.generation()) {
            continue;
        }
        const float width = std::min(t.metrics.panel_width, available.width);
        const float padding = std::min(t.metrics.padding, width / 2);
        const auto footer_node = services_->components().root(s->footer);
        const auto footer_size =
            engine.measure_child(footer_node, {0, std::max(0.0F, width - 2 * padding), 0, available.height});
        const float wanted = 2 * padding + t.metrics.header_height + 7 * t.metrics.row_height + footer_size.height;
        const float below = std::clamp(available.y + available.height - anchor.y - anchor.height - t.metrics.gap, 0.0F,
                                       available.height);
        const float above = std::clamp(anchor.y - t.metrics.gap - available.y, 0.0F, available.height);
        const bool use_above = below < wanted && above > below;
        const float height = std::min(wanted, use_above ? above : below);
        s->bounds = {
            std::clamp(anchor.x, available.x, available.x + available.width - width),
            std::clamp(use_above ? anchor.y - t.metrics.gap - height : anchor.y + anchor.height + t.metrics.gap,
                       available.y, available.y + available.height - height),
            width, height};
        static_cast<void>(engine.measure_child(popup_node, layout::Constraints::fixed(width, height)));
        engine.place_child(popup_node, s->bounds);
        const float x = s->bounds.x + padding;
        const float y = s->bounds.y + padding;
        const float body = std::max(0.0F, width - 2 * padding);
        const float nav = std::min(t.metrics.header_height, body / 6);
        const auto place = [&](runtime::ComponentId item, runtime::Rect rect) {
            const auto node = services_->components().root(item);
            static_cast<void>(engine.measure_child(node, layout::Constraints::fixed(rect.width, rect.height)));
            engine.place_child(node, rect);
        };
        for (std::size_t i = 0; i < 4; ++i) {
            const float offset = i < 2 ? nav * float(i) : body - nav * float(4 - i);
            place(s->items[i], {x + offset, y, nav, t.metrics.header_height});
        }
        place(s->items[4], {x + 2 * nav, y, std::max(0.0F, body - 4 * nav), t.metrics.header_height});
        for (std::size_t i = 5; i < s->items.size(); ++i) {
            const std::size_t index = i - 5;
            place(s->items[i], {x + float(index % 7) * body / 7,
                                y + t.metrics.header_height + float(index / 7) * t.metrics.row_height, body / 7,
                                t.metrics.row_height});
        }
        place(s->footer, {x, y + t.metrics.header_height + 7 * t.metrics.row_height, body, footer_size.height});
        s->needs_layout = false;
        s->anchor = anchor;
        s->clip = available;
    }
}

void DatePickerComponentHost::synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<DateState>(id);
        if (!s || !s->visible) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().date_picker();
        const float radius = std::min({t.metrics.radius, s->bounds.width / 2, s->bounds.height / 2});
        auto background = quad(s->bounds, t.colors.background, radius);
        static_cast<void>(services_->surfaces().update_surface(s->background, std::span{&background, 1}));
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.shape = {{s->bounds.x, s->bounds.y, s->bounds.width, s->bounds.height}, radius};
        effects.shadows = t.shadow;
        effects.ancestor_clip = graphics::EffectClip{1, s->clip};
        static_cast<void>(services_->surfaces().update_effects(s->background, effects));
        for (auto child : s->items) {
            const auto* item = services_->components().state<DateItem>(child);
            const bool selected = item->date && item->date == s->model.value();
            const bool active = item->date && *item->date == s->model.active();
            const auto r = services_->nodes().require(item->node).bounds;
            const float size = std::min({t.metrics.cell_height, r.width, r.height});
            const runtime::Rect inner{r.x + (r.width - size) / 2, r.y + (r.height - size) / 2, size, size};
            auto q = quad(intersect(inner, s->bounds), selected ? t.colors.selected : t.colors.hover, t.metrics.radius);
            q.opacity = selected || active || (item->date && item->date == s->hovered) ? 1.0F : 0.0F;
            static_cast<void>(services_->surfaces().update_surface(item->surface, std::span{&q, 1}));
            services_->scene_composer().set_fragment(item->hit, {}, item->interaction, s->bounds);
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void DatePicker(DatePickerProps props, std::optional<DatePickerFooter> footer) {
    if (!detail::active_dates) {
        throw std::logic_error("DatePicker requires window component services");
    }
    detail::active_dates->mount(props, footer);
}
} // namespace ryn
