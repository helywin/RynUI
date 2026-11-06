#include "component/float_button_component.hpp"
#include "animation/motion_policy.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "reactive/untracked.hpp"
#include <ryn/tooltip.hpp>
#include <algorithm>
#include <cmath>

namespace ryn::detail {
namespace {
thread_local FloatButtonComponentHost* active_host{};

struct FloatState final {
    runtime::ComponentId id;
    runtime::ComponentId menu;
    runtime::ComponentId trigger;
    runtime::ComponentId closed_icon;
    runtime::ComponentId open_icon;
    runtime::ComponentId progress_view;
    runtime::ComponentId body;
    runtime::NodeId button_node;
    component::RetainedSurfaceId progress_range;
    runtime::NodeId node;
    Signal<FloatButtonShape> shape{FloatButtonShape::Circle};
    Signal<ButtonShape> button_shape{ButtonShape::Circle};
    Signal<ButtonType> type{ButtonType::Default};
    Signal<bool> disabled{false};
    Signal<LogicalLength> size{dp(40)};
    Signal<ThemeConfig> button_theme{ThemeConfig{}};
    FloatButtonTrigger mode{FloatButtonTrigger::None};
    FloatButtonPlacement placement{FloatButtonPlacement::Top};
    FloatButtonAnchor anchor{FloatButtonAnchor::BottomRight};
    FloatButtonScrollState scroll;
    FloatButtonSnapshot view;
    Signal<runtime::SemanticTypography> icon_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> content_font{runtime::SemanticTypography{}};
    std::optional<input::InteractionId> capture;
    bool floating{true};
    bool draggable{};
    bool group{};
    bool controlled{};
    bool open{};
    bool back_top{};
    bool progress{};
    bool press{};
    bool hovered{};
    bool drag_trigger_anchor{};
    bool dragged{};
    bool disposing{};
    float threshold{400};
    Duration duration{Duration::milliseconds(450)};
    runtime::Point press_point;
    runtime::Point press_origin;
    std::optional<runtime::Point> dragged_origin;
    input::PointerIdentity pointer;
    runtime::Size viewport;
    std::optional<animation::AnimationTime> started;
    float start_offset{};
    float last_fraction{};
    theme_runtime::Subscription theme;
    std::function<void()> click;
    std::function<void(bool)> change;
    std::function<void(float)> scroll_to;
};

void validate(float value, const char* message) {
    if (!std::isfinite(value) || value < 0) {
        throw std::invalid_argument(message);
    }
}

bool inside(runtime::Rect bounds, float x, float y) {
    return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height;
}

void dirty(WindowComponentServices& services, runtime::ComponentId id) {
    if (services.components().contains(id)) {
        services.dirty().invalidate(services.components().root(id),
                                    runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                        runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    }
}

bool descendant(WindowComponentServices& services, runtime::ComponentId id, runtime::ComponentId ancestor) {
    for (auto current = std::optional{id}; current; current = services.components().parent(*current)) {
        if (*current == ancestor) {
            return true;
        }
    }
    return false;
}
} // namespace

FloatButtonComponentHost::FloatButtonComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

FloatButtonComponentHost::~FloatButtonComponentHost() {
    services_->detach(*this);
}

void* FloatButtonComponentHost::begin_mount() noexcept {
    return std::exchange(active_host, this);
}

void FloatButtonComponentHost::end_mount(void* previous) noexcept {
    active_host = static_cast<FloatButtonComponentHost*>(previous);
}

void FloatButtonComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void FloatButtonComponentHost::on_dispose() noexcept {
    mounted_.clear();
}

FloatButtonSnapshot FloatButtonComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* state = services_->components().state<FloatState>(id);
    if (!state) {
        throw std::out_of_range("FloatButton component is stale");
    }
    return state->view;
}

void FloatButtonComponentHost::refresh(runtime::ComponentId id) {
    run_untracked([&] {
        auto* state = services_->components().state<FloatState>(id);
        if (!state || state->disposing) {
            return;
        }
        auto& s = *state;
        const auto token = services_->components().theme_scope(id)->snapshot().float_button();
        s.size.set(dp(token.size));
        const auto& text = services_->components().theme_scope(id)->snapshot().text();
        s.icon_font.set({text.font_family, text.font_weight, false, token.icon_size, token.icon_size});
        s.content_font.set(
            {text.font_family, text.font_weight, false, token.content_font_size, token.content_font_size * 1.2F});
        const auto shape = s.shape.get();
        if (shape != FloatButtonShape::Circle && shape != FloatButtonShape::Square) {
            throw std::invalid_argument("Invalid FloatButton shape");
        }
        s.button_shape.set(shape == FloatButtonShape::Circle ? ButtonShape::Circle : ButtonShape::Default);
        ThemeConfig config;
        config.button.algorithm = true;
        config.button.seed.control_height = dp(token.size);
        config.button.tokens.padding_inline = dp(0);
        config.button.tokens.border_width = dp(0);
        config.button.tokens.border_radius = dp(token.border_radius);
        config.button.tokens.default_background = token.background;
        config.button.tokens.primary_background = token.primary_background;
        config.button.tokens.default_color = token.color;
        config.button.tokens.primary_color = token.primary_color;
        config.button.tokens.default_shadow = token.shadow;
        config.button.tokens.primary_shadow = token.shadow;
        s.button_theme.set(std::move(config));
        const auto focused = services_->focus().state().focused;
        const auto* focus_record = focused ? services_->interactions().find(*focused) : nullptr;
        const bool closing_focused_menu = s.group && s.view.open && !s.open && s.mode != FloatButtonTrigger::None &&
                                          !s.disabled.get() && focus_record &&
                                          descendant(*services_, focus_record->component, s.menu);
        std::optional<input::InteractionId> restore;
        if (closing_focused_menu && s.trigger.valid()) {
            for (const auto interaction : services_->interactions().declaration_order()) {
                const auto& record = services_->interactions().require(interaction);
                if (record.focusable && descendant(*services_, record.component, s.trigger)) {
                    restore = interaction;
                    break;
                }
            }
        }
        s.view.visible = !s.back_top || s.scroll.offset >= s.threshold;
        s.view.menu = s.menu;
        s.view.trigger = s.trigger;
        s.view.progress_view = s.progress_view;
        s.view.open = s.group && (s.mode == FloatButtonTrigger::None || s.open) && !s.disabled.get();
        if (s.disabled.get()) {
            stop(id);
        } else if (!s.view.visible && (s.press || s.capture)) {
            // Crossing the visibility threshold cancels a pointer gesture, but
            // a BackTop scroll already in flight must still reach zero.
            const auto started = s.started;
            stop(id);
            s.started = started;
        }
        s.view.animating = s.started.has_value();
        s.view.progress = s.scroll.extent > s.scroll.viewport
                              ? std::clamp(s.scroll.offset / (s.scroll.extent - s.scroll.viewport), 0.0F, 1.0F)
                              : 0;
        services_->nodes().require(s.node).out_of_flow = s.floating || !s.view.visible;
        const auto branch = [&](runtime::ComponentId branch_id, bool active) {
            if (branch_id.valid() && services_->components().set_branch_active(branch_id, active)) {
                services_->mark_scene_structure_dirty();
            }
        };
        branch(id, s.view.visible);
        branch(s.menu, s.view.open);
        branch(s.trigger, s.mode != FloatButtonTrigger::None);
        branch(s.open_icon, s.view.open);
        branch(s.closed_icon, !s.view.open);
        branch(s.progress_view, s.progress && s.back_top);
        if (s.group && s.menu.valid()) {
            const bool horizontal =
                s.placement == FloatButtonPlacement::Left || s.placement == FloatButtonPlacement::Right;
            services_->layout().set_layout(services_->components().root(s.menu),
                                           layout::FlexLayout{.direction = horizontal
                                                                               ? layout::FlexDirection::horizontal
                                                                               : layout::FlexDirection::vertical,
                                                              .main_gap = token.gap});
        }
        dirty(*services_, id);
        // Boundary reconciliation may run application focus callbacks and destroy us.
        const bool blocked = s.disabled.get() || !s.view.visible;
        const auto menu = s.menu;
        const bool menu_blocked = !s.view.open;
        const auto trigger = s.trigger;
        const bool trigger_blocked = s.mode == FloatButtonTrigger::None;
        services_->set_input_blocked(id, blocked);
        if (menu.valid() && services_->components().contains(menu)) {
            services_->set_input_blocked(menu, menu_blocked);
        }
        if (trigger.valid() && services_->components().contains(trigger)) {
            services_->set_input_blocked(trigger, trigger_blocked);
        }
        if (restore && services_->components().contains(id)) {
            services_->focus().defer_focus(restore, input::FocusModality::keyboard);
        }
    });
}

runtime::ComponentId FloatButtonComponentHost::create(const FloatButtonProps& props, const FloatButtonSlots& slots) {
    auto& build = runtime::require_component_build_context();
    auto* parent = build.nearest_state<FloatState>();
    const auto group = parent && parent->group ? parent : nullptr;
    const auto id = build.mount_component<FloatState>();
    auto& s = build.state<FloatState>(id);
    s.id = id;
    s.node = build.root(id);
    s.floating = group ? false : read_prop(props.floating_);
    s.click = props.on_click_;
    s.type.set(read_prop(props.type_) == FloatButtonType::Primary ? ButtonType::Primary : ButtonType::Default);
    s.shape.set(group ? group->shape.get() : read_prop(props.shape_));
    s.disabled.set(read_prop(props.disabled_) || (group && group->disabled.get()));
    s.draggable = read_prop(props.draggable_);
    s.anchor = read_prop(props.anchor_);
    services_->layout().set_layout(s.node, layout::BoxLayout{});
    if (s.floating) {
        services_->components().set_window_layer(id, 100);
    }
    build.on_resource_cleanup(id, [this, id, node = s.node] {
        if (auto* current = services_->components().state<FloatState>(id)) {
            current->disposing = true;
            current->started.reset();
        }
        services_->layout().remove_layout(node);
    });
    auto& scope = build.scope(id);
    runtime::connect_layout_style(scope, props.layout_, s.node, services_->nodes(), services_->dirty());
    const auto connect = [&]<typename T, typename F>(const Prop<T>& prop, F update) {
        static_cast<void>(connect_prop(scope, prop, [this, id, update](T value) {
            if (auto* current = services_->components().state<FloatState>(id)) {
                update(*current, std::move(value));
                refresh(id);
            }
        }));
    };
    connect(props.type_, [](auto& value, FloatButtonType type) {
        if (type != FloatButtonType::Default && type != FloatButtonType::Primary) {
            throw std::invalid_argument("Invalid FloatButton type");
        }
        value.type.set(type == FloatButtonType::Primary ? ButtonType::Primary : ButtonType::Default);
    });
    connect(group ? Prop<FloatButtonShape>{group->shape} : props.shape_,
            [](auto& value, auto shape) { value.shape.set(shape); });
    const Prop<bool> disabled = group ? Prop<bool>{bind([own = props.disabled_, inherited = group->disabled] {
        return read_prop(own) || inherited.get();
    })}
                                      : props.disabled_;
    connect(disabled, [](auto& value, bool disabled) { value.disabled.set(disabled); });
    connect(props.draggable_, [this, id](auto& value, bool enabled) {
        value.draggable = enabled;
        if (!enabled) {
            stop(id);
        }
    });
    connect(props.anchor_, [](auto& value, FloatButtonAnchor anchor) {
        if (anchor < FloatButtonAnchor::BottomRight || anchor > FloatButtonAnchor::TopLeft) {
            throw std::invalid_argument("Invalid FloatButton anchor");
        }
        value.anchor = anchor;
        value.dragged_origin.reset();
    });
    connect(props.floating_, [this, id, grouped = group != nullptr](auto& value, bool floating) {
        value.floating = !grouped && floating;
        if (!value.floating) {
            stop(id);
        }
        services_->components().set_window_layer(id, value.floating ? std::optional{100} : std::nullopt);
    });
    const auto theme = build.theme_scope();
    s.theme = theme->capture([this, id](auto) { refresh(id); },
                             [theme] {
                                 static_cast<void>(theme->float_button());
                                 static_cast<void>(theme->text_font_family());
                                 static_cast<void>(theme->text_font_weight());
                                 static_cast<void>(theme->alias());
                                 static_cast<void>(theme->primary_color());
                                 static_cast<void>(theme->line_width());
                             });
    refresh(id);
    build.mount_slot(
        id, Content{[&, id] {
            Theme(
                ThemeProps{}.config(s.button_theme), ThemeContent{[&, id] {
                    Tooltip(
                        props.tooltip_config_.value_or(
                            TooltipProps{}.title(props.tooltip_).titleAvailable(bind([title = props.tooltip_] {
                                return !read_prop(title).empty();
                            }))),
                        TooltipTrigger{[&, id] {
                            const auto button = [&, id] {
                                ButtonSlots button_slots;
                                button_slots.icon = ButtonIcon{[this, id, slots] {
                                    auto& current_build = runtime::require_component_build_context();
                                    const auto mount_region = [&](const std::function<void()>& content,
                                                                  Prop<runtime::SemanticTypography> typography) {
                                        auto& current_build = runtime::require_component_build_context();
                                        const auto region = current_build.mount_component<int>(0);
                                        services_->layout().set_layout(current_build.root(region), layout::BoxLayout{});
                                        current_build.on_resource_cleanup(region,
                                                                          [this, node = current_build.root(region)] {
                                                                              services_->layout().remove_layout(node);
                                                                          });
                                        current_build.mount_slot_with_semantic_text_style(
                                            region, Content{content}, *current_build.semantic_foreground(),
                                            std::move(typography));
                                    };
                                    const auto* current = services_->components().state<FloatState>(id);
                                    const auto icon_font = current->icon_font;
                                    const auto content_font = current->content_font;
                                    const auto body = current_build.mount_component<int>(0);
                                    services_->components().state<FloatState>(id)->body = body;
                                    const auto body_node = current_build.root(body);
                                    services_->layout().set_layout(
                                        body_node, layout::FlexLayout{.direction = layout::FlexDirection::vertical,
                                                                      .main_gap = 2,
                                                                      .align = layout::FlexAlign::center});
                                    current_build.on_resource_cleanup(
                                        body, [this, body_node] { services_->layout().remove_layout(body_node); });
                                    current_build.mount_slot(
                                        body, Content{[&] {
                                            mount_region(slots.icon
                                                             ? SlotContentAccess::function(*slots.icon)
                                                             : std::function<void()>{[] {
                                                                   Icon(IconProps{}.name(IconName::AppstoreOutlined));
                                                               }},
                                                         icon_font);
                                            if (slots.content) {
                                                mount_region(SlotContentAccess::function(*slots.content), content_font);
                                            }
                                        }});
                                }};
                                Button(ButtonProps{}
                                           .type(s.type)
                                           .shape(s.button_shape)
                                           .disabled(s.disabled)
                                           .layout(LayoutStyle{}.width(s.size).min_height(s.size))
                                           .onClick([this, id] { activate(id); }),
                                       std::move(button_slots));
                            };
                            if (props.badge_) {
                                Badge(*props.badge_, BadgeContent{button});
                            } else {
                                button();
                            }
                        }});
                }});
        }});
    for (const auto interaction : services_->interactions().declaration_order()) {
        const auto& record = services_->interactions().require(interaction);
        if (record.focusable && descendant(*services_, record.component, id)) {
            s.button_node = record.node;
            break;
        }
    }
    services_->layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId node, layout::Constraints limits) {
                        const auto& value = *services_->components().state<FloatState>(id);
                        const auto size = services_->components().theme_scope(id)->snapshot().float_button().size;
                        const auto body =
                            engine.measure_child(services_->components().root(value.body),
                                                 {0, std::min(size, limits.max_width), 0, limits.max_height});
                        // Button owns input/materials; the compound owns its intrinsic height.
                        // Re-measure its retained subtree with the actual vertical content.
                        const auto height = std::max(size, body.height);
                        services_->nodes().require(value.button_node).external_layout.height = height;
                        engine.set_layout(value.button_node, layout::HorizontalContentLayout{.control_height = height,
                                                                                             .padding_inline = 0,
                                                                                             .border_width = 0,
                                                                                             .gap = 0,
                                                                                             .first_is_icon = true,
                                                                                             .minimum_width = size});
                        runtime::Size result;
                        for (const auto child : services_->nodes().require(node).children) {
                            const auto measured = engine.measure_child(child, limits);
                            result.width = std::max(result.width, measured.width);
                            result.height = std::max(result.height, measured.height);
                        }
                        return limits.constrain(result);
                    },
                    [this](layout::LayoutEngine& engine, runtime::NodeId node, runtime::Rect bounds) {
                        for (const auto child : services_->nodes().require(node).children) {
                            engine.place_child(child, bounds, true, true);
                        }
                    }});
    mounted_.push_back(id);
    return id;
}

void FloatButtonComponentHost::mount(const FloatButtonProps& props, const FloatButtonSlots& slots) {
    static_cast<void>(create(props, slots));
}

void FloatButtonComponentHost::mount(const FloatButtonGroupProps& props, const FloatButtonGroupContent& content) {
    if (props.open_ && props.default_open_) {
        throw std::invalid_argument("FloatButtonGroup cannot combine open and defaultOpen");
    }
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<FloatState>();
    auto& s = build.state<FloatState>(id);
    s.id = id;
    s.node = build.root(id);
    s.group = true;
    s.controlled = props.open_.has_value();
    s.open = props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false);
    s.change = props.on_open_;
    s.click = props.on_click_;
    s.floating = read_prop(props.floating_);
    s.draggable = read_prop(props.draggable_);
    s.anchor = read_prop(props.anchor_);
    s.mode = read_prop(props.trigger_);
    s.placement = read_prop(props.placement_);
    s.shape.set(read_prop(props.shape_));
    s.disabled.set(read_prop(props.disabled_));
    if (s.floating) {
        services_->components().set_window_layer(id, 100);
    }
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services_->nodes(), services_->dirty());
    build.on_resource_cleanup(id, [this, id, node = s.node] {
        stop(id);
        services_->layout().remove_layout(node);
    });
    build.mount_slot(
        id, Content{[&] {
            auto& build = runtime::require_component_build_context();
            s.menu = build.mount_component<int>(0);
            services_->layout().set_layout(build.root(s.menu),
                                           layout::FlexLayout{.direction = layout::FlexDirection::vertical});
            build.on_resource_cleanup(s.menu,
                                      [this, node = build.root(s.menu)] { services_->layout().remove_layout(node); });
            build.mount_slot(s.menu, content);
            FloatButtonSlots slots;
            slots.icon = FloatButtonIcon{[&] {
                auto& build = runtime::require_component_build_context();
                s.closed_icon = build.mount_component<int>(0);
                services_->layout().set_layout(build.root(s.closed_icon), layout::BoxLayout{});
                build.on_resource_cleanup(s.closed_icon, [this, node = build.root(s.closed_icon)] {
                    services_->layout().remove_layout(node);
                });
                build.mount_slot(
                    s.closed_icon,
                    Content{props.icon_ ? SlotContentAccess::function(*props.icon_) : std::function<void()>{[] {
                        Icon(IconProps{}.name(IconName::AppstoreOutlined));
                    }}});
                s.open_icon = build.mount_component<int>(0);
                services_->layout().set_layout(build.root(s.open_icon), layout::BoxLayout{});
                build.on_resource_cleanup(
                    s.open_icon, [this, node = build.root(s.open_icon)] { services_->layout().remove_layout(node); });
                build.mount_slot(s.open_icon,
                                 Content{props.close_icon_ ? SlotContentAccess::function(*props.close_icon_)
                                                           : std::function<void()>{[] {
                                                                 Icon(IconProps{}.name(IconName::CloseOutlined));
                                                             }}});
            }};
            s.trigger = create(
                FloatButtonProps{}.floating(false).type(props.type_).disabled(props.disabled_).onClick([this, id] {
                    if (auto* current = services_->components().state<FloatState>(id); current && !current->dragged) {
                        const auto click = current->click;
                        request_open(id, !current->view.open);
                        if (click) {
                            click();
                        }
                    }
                }),
                slots);
        }});
    services_->layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        auto& value = *services_->components().state<FloatState>(id);
                        const auto gap = services_->components().theme_scope(id)->snapshot().float_button().gap;
                        const bool horizontal = value.placement == FloatButtonPlacement::Left ||
                                                value.placement == FloatButtonPlacement::Right;
                        const auto menu = value.view.open
                                              ? engine.measure_child(services_->components().root(value.menu), limits)
                                              : runtime::Size{};
                        const auto trigger =
                            value.mode != FloatButtonTrigger::None
                                ? engine.measure_child(services_->components().root(value.trigger), limits)
                                : runtime::Size{};
                        const float separation = menu.width > 0 && trigger.width > 0 ? gap : 0;
                        return limits.constrain(horizontal ? runtime::Size{menu.width + trigger.width + separation,
                                                                           std::max(menu.height, trigger.height)}
                                                           : runtime::Size{std::max(menu.width, trigger.width),
                                                                           menu.height + trigger.height + separation});
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                        const auto& value = *services_->components().state<FloatState>(id);
                        const bool horizontal = value.placement == FloatButtonPlacement::Left ||
                                                value.placement == FloatButtonPlacement::Right;
                        const bool before = value.placement == FloatButtonPlacement::Top ||
                                            value.placement == FloatButtonPlacement::Left;
                        const auto place = [&](runtime::ComponentId child, bool active, bool first) {
                            if (!active) {
                                return;
                            }
                            const auto node = services_->components().root(child);
                            const auto size = services_->nodes().require(node).measured_size;
                            const float x = horizontal ? (first ? bounds.x : bounds.x + bounds.width - size.width)
                                                       : bounds.x + (bounds.width - size.width) / 2;
                            const float y = horizontal ? bounds.y + (bounds.height - size.height) / 2
                                                       : (first ? bounds.y : bounds.y + bounds.height - size.height);
                            engine.place_child(node, {x, y, size.width, size.height});
                        };
                        place(value.menu, value.view.open, before);
                        place(value.trigger, value.mode != FloatButtonTrigger::None, !before);
                    }});
    const auto connect = [&]<typename T, typename F>(const Prop<T>& prop, F update) {
        static_cast<void>(connect_prop(build.scope(id), prop, [this, id, update](T value) {
            if (auto* current = services_->components().state<FloatState>(id)) {
                update(*current, value);
                refresh(id);
            }
        }));
    };
    connect(props.shape_, [](auto& value, auto shape) { value.shape.set(shape); });
    connect(props.disabled_, [](auto& value, bool disabled) { value.disabled.set(disabled); });
    connect(props.trigger_, [](auto& value, FloatButtonTrigger mode) {
        if (mode < FloatButtonTrigger::None || mode > FloatButtonTrigger::Hover) {
            throw std::invalid_argument("Invalid FloatButton trigger");
        }
        value.mode = mode;
        value.hovered = false;
    });
    connect(props.placement_, [](auto& value, FloatButtonPlacement placement) {
        if (placement < FloatButtonPlacement::Top || placement > FloatButtonPlacement::Right) {
            throw std::invalid_argument("Invalid FloatButton placement");
        }
        value.placement = placement;
    });
    connect(props.anchor_, [](auto& value, auto anchor) {
        if (anchor < FloatButtonAnchor::BottomRight || anchor > FloatButtonAnchor::TopLeft) {
            throw std::invalid_argument("Invalid FloatButton anchor");
        }
        value.anchor = anchor;
        value.dragged_origin.reset();
    });
    connect(props.draggable_, [this, id](auto& value, bool enabled) {
        value.draggable = enabled;
        if (!enabled) {
            stop(id);
        }
    });
    connect(props.floating_, [this, id](auto& value, bool floating) {
        value.floating = floating;
        if (!floating) {
            stop(id);
        }
        services_->components().set_window_layer(id, floating ? std::optional{100} : std::nullopt);
    });
    if (props.open_) {
        connect(*props.open_, [](auto& value, bool open) { value.open = open; });
    }
    const auto theme = build.theme_scope();
    s.theme = theme->capture([this, id](auto) { refresh(id); },
                             [theme] {
                                 static_cast<void>(theme->float_button());
                                 static_cast<void>(theme->text_font_family());
                                 static_cast<void>(theme->text_font_weight());
                                 static_cast<void>(theme->alias());
                                 static_cast<void>(theme->primary_color());
                                 static_cast<void>(theme->line_width());
                             });
    mounted_.push_back(id);
    refresh(id);
}

void FloatButtonComponentHost::mount(const FloatButtonBackTopProps& props, FloatButtonSlots slots) {
    if (!props.on_scroll_) {
        throw std::invalid_argument("BackTop requires an explicit scroll target callback");
    }
    if (!slots.icon) {
        slots.icon = FloatButtonIcon{[] { Icon(IconProps{}.name(IconName::VerticalAlignTopOutlined)); }};
    }
    const auto id = create(props.button_, slots);
    auto& s = *services_->components().state<FloatState>(id);
    s.back_top = true;
    s.scroll_to = props.on_scroll_;
    auto& build = runtime::require_component_build_context();
    const auto connect = [&]<typename T, typename F>(const Prop<T>& prop, F update) {
        const auto apply = [this, id, update](T value) {
            if (auto* current = services_->components().state<FloatState>(id)) {
                update(*current, value);
                refresh(id);
            }
        };
        static_cast<void>(connect_prop(build.scope(id), prop, apply));
    };
    connect(props.scroll_, [](auto& value, FloatButtonScrollState scroll) {
        validate(scroll.offset, "Invalid scroll offset");
        validate(scroll.extent, "Invalid scroll extent");
        validate(scroll.viewport, "Invalid scroll viewport");
        value.scroll = scroll;
    });
    connect(props.visibilityHeight_, [](auto& value, float threshold) {
        validate(threshold, "Invalid BackTop threshold");
        value.threshold = threshold;
    });
    connect(props.duration_, [](auto& value, Duration duration) {
        validate(duration.count_milliseconds(), "Invalid BackTop duration");
        value.duration = duration;
    });
    connect(props.showProgress_, [](auto& value, bool progress) { value.progress = progress; });
    build.mount_slot(id, Content{[&] {
                         auto& build = runtime::require_component_build_context();
                         s.progress_view = build.mount_component<int>(0);
                         services_->layout().set_layout(build.root(s.progress_view), layout::BoxLayout{});
                         build.on_resource_cleanup(s.progress_view, [this, node = build.root(s.progress_view)] {
                             services_->layout().remove_layout(node);
                         });
                         const auto fragment = build.register_scene_fragment(
                             s.progress_view, runtime::SceneFragmentPlacement::after_children);
                         s.progress_range = services_->surfaces().create_content_range(fragment, {});
                         build.on_resource_cleanup(s.progress_view, [this, range = s.progress_range] {
                             services_->surfaces().destroy_content_range(range);
                         });
                     }});
    refresh(id);
}

void FloatButtonComponentHost::request_open(runtime::ComponentId id, bool open) {
    auto* s = services_->components().state<FloatState>(id);
    if (!s || s->disabled.get() || s->mode == FloatButtonTrigger::None || s->open == open) {
        return;
    }
    const auto callback = s->change;
    if (!s->controlled) {
        s->open = open;
        refresh(id);
    }
    if (callback) {
        callback(open);
    }
}

void FloatButtonComponentHost::activate(runtime::ComponentId id) {
    auto* s = services_->components().state<FloatState>(id);
    if (!s || s->disabled.get() || s->dragged || !s->view.visible || !services_->interactions().component_active(id)) {
        return;
    }
    for (auto parent = services_->components().parent(id); parent; parent = services_->components().parent(*parent)) {
        const auto* group = services_->components().state<FloatState>(*parent);
        if (group && group->dragged) {
            return;
        }
    }
    auto click = s->click;
    if (s->back_top) {
        if (animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                             services_->motion_preference())
                .reduced() ||
            s->duration.count_milliseconds() == 0) {
            const auto scroll = s->scroll_to;
            s->started.reset();
            s->view.animating = false;
            scroll(0);
        } else {
            s->started = services_->animation_time();
            s->start_offset = s->scroll.offset;
            s->last_fraction = 0;
            s->view.animating = true;
            services_->request_frame();
        }
    }
    if (click) {
        click();
    }
}

void FloatButtonComponentHost::stop(runtime::ComponentId id) {
    if (auto* s = services_->components().state<FloatState>(id)) {
        s->hovered = false;
        if (!s->capture && !s->started && !s->press) {
            return;
        }
        const auto capture = std::exchange(s->capture, {});
        s->started.reset();
        s->press = false;
        s->dragged = false;
        s->view.dragging = false;
        s->view.animating = false;
        if (capture) {
            services_->pointer().cancel_pointer_interaction(*capture);
        }
        const auto order = services_->interactions().declaration_order();
        const std::vector<input::InteractionId> interactions{order.begin(), order.end()};
        for (const auto interaction : interactions) {
            const auto* record = services_->interactions().find(interaction);
            if (record && services_->components().contains(id) && descendant(*services_, record->component, id)) {
                services_->pointer().cancel_pointer_interaction(interaction);
            }
        }
    }
}

void FloatButtonComponentHost::on_window_active(bool active) {
    if (!active) {
        for (const auto id : mounted_) {
            stop(id);
        }
    }
}

void FloatButtonComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (const auto id : ids) {
        const auto* s = services_->components().state<FloatState>(id);
        if (!s) {
            continue;
        }
        const bool maintained = services_->interactions().subtree_intersects_input_domain(id);
        if (!maintained || s->disabled.get()) {
            stop(id);
        } else if (!services_->interactions().component_active(id)) {
            // A permitted child popup/target can outlive its non-receiving host.
            // End only the host gesture; never cancel an allowed child's capture.
            auto* state = services_->components().state<FloatState>(id);
            const auto capture = std::exchange(state->capture, {});
            state->hovered = false;
            state->started.reset();
            state->press = false;
            state->dragged = false;
            state->view.dragging = false;
            state->view.animating = false;
            if (capture && !services_->interactions().branch_active(*capture)) {
                services_->pointer().cancel_pointer_interaction(*capture);
            }
        }
    }
}

void FloatButtonComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect) {
    for (const auto id : mounted_) {
        auto* s = services_->components().state<FloatState>(id);
        if (!s || !s->floating || !s->view.visible) {
            continue;
        }
        s->viewport = viewport;
        const auto size = services_->layout().measure_child(s->node, {0, viewport.width, 0, viewport.height});
        const auto& token = services_->components().theme_scope(id)->snapshot().float_button();
        const bool left = s->anchor == FloatButtonAnchor::BottomLeft || s->anchor == FloatButtonAnchor::TopLeft;
        const bool top = s->anchor == FloatButtonAnchor::TopLeft || s->anchor == FloatButtonAnchor::TopRight;
        auto point = s->dragged_origin.value_or(
            runtime::Point{left ? token.inset_inline : viewport.width - size.width - token.inset_inline,
                           top ? token.inset_block : viewport.height - size.height - token.inset_block});
        if (s->dragged_origin && s->drag_trigger_anchor && s->mode != FloatButtonTrigger::None) {
            // Store the trigger origin, so opening/closing or changing menu
            // direction cannot move the dragged operation button.
            const auto trigger_size =
                services_->nodes().require(services_->components().root(s->trigger)).measured_size;
            if (s->placement == FloatButtonPlacement::Top || s->placement == FloatButtonPlacement::Bottom) {
                point.x -= (size.width - trigger_size.width) / 2;
                if (s->placement == FloatButtonPlacement::Top) {
                    point.y -= size.height - trigger_size.height;
                }
            } else {
                point.y -= (size.height - trigger_size.height) / 2;
                if (s->placement == FloatButtonPlacement::Left) {
                    point.x -= size.width - trigger_size.width;
                }
            }
        }
        point.x = std::clamp(point.x, 0.0F, std::max(0.0F, viewport.width - size.width));
        point.y = std::clamp(point.y, 0.0F, std::max(0.0F, viewport.height - size.height));
        services_->layout().place_child(s->node, {point.x, point.y, size.width, size.height});
        s->view.bounds = services_->nodes().require(s->node).bounds;
    }
}

void FloatButtonComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    for (const auto id : mounted_) {
        auto* s = services_->components().state<FloatState>(id);
        if (!s) {
            continue;
        }
        const auto& node = services_->nodes().require(s->node);
        s->view.bounds = node.bounds;
        if (!s->back_top || !s->progress || !s->view.visible || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& theme = services_->components().theme_scope(id)->snapshot();
        const auto r = node.bounds;
        const auto stroke = std::min(theme.seed().line_width * 2, std::min(r.width, r.height) / 2);
        if (stroke <= 0 || r.width <= 0 || r.height <= 0) {
            services_->surfaces().update_content_effects(s->progress_range, {});
            continue;
        }
        const auto radius = s->shape.get() == FloatButtonShape::Circle
                                ? std::min(r.width, r.height) / 2
                                : std::min(theme.float_button().border_radius, std::min(r.width, r.height) / 2);
        const graphics::LogicalRoundedRect shape{r, radius};
        const graphics::LogicalRoundedRect centerline{
            {r.x + stroke / 2, r.y + stroke / 2, r.width - stroke, r.height - stroke},
            std::max(0.0F, radius - stroke / 2)};
        const auto length = graphics::rounded_rect_path_sample({}, centerline).length;
        const auto progress = std::max(length * s->view.progress, 0.0001F);
        const auto gap = std::max(0.0F, length - progress);
        const auto phase = -(centerline.rect.width - 2 * centerline.radius) / 2;
        const graphics::EffectClip effect_clip{1, services_->component_clip(id, viewport, clip)};
        const std::array effects{graphics::make_dashed_outline_effect(shape, stroke, {std::max(length, 0.0001F), 0, 0},
                                                                      theme.alias().color_border_secondary,
                                                                      node.opacity, node.translation, effect_clip),
                                 graphics::make_dashed_outline_effect(
                                     shape, stroke, {progress, gap, phase}, theme.map().color_primary,
                                     s->view.progress > 0 ? node.opacity : 0, node.translation, effect_clip)};
        services_->surfaces().update_content_effects(s->progress_range, effects);
    }
}

void FloatButtonComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                                std::optional<input::InteractionId> hit,
                                                std::optional<input::InteractionId> origin) {
    const auto ids = mounted_;
    for (const auto id : ids) {
        auto* s = services_->components().state<FloatState>(id);
        if (!s || s->disabled.get() || !s->view.visible || !services_->interactions().component_active(id)) {
            continue;
        }
        const auto owns = [&](std::optional<input::InteractionId> target) {
            const auto* record = target ? services_->interactions().find(*target) : nullptr;
            return record && descendant(*services_, record->component, id);
        };
        const bool over = owns(hit) || (!hit && inside(s->view.bounds, event.x, event.y));
        if (event.action == input::PointerAction::down && event.button == input::PointerButton::primary) {
            if (owns(hit)) {
                s->pointer = event.pointer;
                const auto pointer = services_->pointer().state(event.pointer);
                s->capture = pointer ? pointer->capture : std::nullopt;
            }
            if (owns(hit) && s->floating && s->draggable) {
                s->press = true;
                s->dragged = false;
                s->pointer = event.pointer;
                s->capture = services_->pointer().state(event.pointer)->capture;
                s->press_point = {event.x, event.y};
                s->drag_trigger_anchor = s->group && s->mode != FloatButtonTrigger::None;
                const auto origin = s->drag_trigger_anchor
                                        ? services_->nodes().require(services_->components().root(s->trigger)).bounds
                                        : s->view.bounds;
                s->press_origin = {origin.x, origin.y};
            } else if (s->group && !over && !owns(origin)) {
                request_open(id, false);
            }
        }
        s = services_->components().state<FloatState>(id);
        if (!s) {
            continue;
        }
        if (s->press && s->pointer == event.pointer) {
            if (event.action == input::PointerAction::move) {
                const float dx = event.x - s->press_point.x;
                const float dy = event.y - s->press_point.y;
                if (s->dragged || std::hypot(dx, dy) >= 4) {
                    s->dragged = true;
                    s->view.dragging = true;
                    s->dragged_origin = runtime::Point{s->press_origin.x + dx, s->press_origin.y + dy};
                    dirty(*services_, id);
                }
            } else if (event.action == input::PointerAction::up || event.action == input::PointerAction::cancel) {
                s->press = false;
                s->view.dragging = false;
                s->dragged = false;
                s->capture.reset();
            }
        }
        if (event.action == input::PointerAction::up || event.action == input::PointerAction::cancel) {
            if (s->pointer == event.pointer) {
                s->capture.reset();
            }
        }
        if (s->group && s->mode == FloatButtonTrigger::Hover &&
            (event.action == input::PointerAction::move || event.action == input::PointerAction::cancel)) {
            const bool hovered = event.action != input::PointerAction::cancel && over;
            if (s->hovered != hovered) {
                s->hovered = hovered;
                request_open(id, hovered);
            }
        }
    }
}

bool FloatButtonComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.key != input::Key::escape || event.action != input::KeyAction::down) {
        return false;
    }
    const auto ids = mounted_;
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        const auto* s = services_->components().state<FloatState>(*it);
        if (s && s->group && s->view.open && s->mode != FloatButtonTrigger::None &&
            services_->interactions().component_active(*it)) {
            request_open(*it, false);
            return true;
        }
    }
    return false;
}

std::size_t FloatButtonComponentHost::tick_auxiliary(animation::AnimationTime time) {
    std::size_t count{};
    const auto ids = mounted_;
    for (const auto id : ids) {
        auto* s = services_->components().state<FloatState>(id);
        if (!s || !s->started) {
            continue;
        }
        const auto parent = services_->components().parent(id);
        if ((parent && !services_->interactions().component_active(*parent)) || s->disabled.get()) {
            stop(id);
            continue;
        }
        const auto elapsed = time >= *s->started ? (time - *s->started).count_microseconds() : 0;
        const bool reduced = animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                                              services_->motion_preference())
                                 .reduced();
        const float requested_fraction =
            reduced || s->duration.count_milliseconds() == 0
                ? 1.0F
                : std::clamp(static_cast<float>(elapsed / (1000.0 * s->duration.count_milliseconds())), 0.0F, 1.0F);
        const float fraction = std::max(s->last_fraction, requested_fraction);
        s->last_fraction = fraction;
        const auto callback = s->scroll_to;
        const float offset = s->start_offset * std::pow(1 - fraction, 3);
        if (fraction == 1) {
            s->started.reset();
            s->view.animating = false;
        }
        callback(offset);
        ++count;
    }
    return count;
}

std::optional<animation::AnimationTime> FloatButtonComponentHost::next_auxiliary_deadline() const {
    for (const auto id : mounted_) {
        const auto* s = services_->components().state<FloatState>(id);
        if (s && s->started) {
            return services_->animation_time() + animation::AnimationDuration::milliseconds(16);
        }
    }
    return {};
}
} // namespace ryn::detail

namespace ryn {
void FloatButton(FloatButtonProps props, FloatButtonSlots slots) {
    if (!detail::active_host) {
        throw std::logic_error("FloatButton requires an active component host");
    }
    detail::active_host->mount(props, slots);
}

void FloatButtonGroup(FloatButtonGroupProps props, FloatButtonGroupContent content) {
    if (!detail::active_host) {
        throw std::logic_error("FloatButtonGroup requires an active component host");
    }
    detail::active_host->mount(props, content);
}

void FloatButtonBackTop(FloatButtonBackTopProps props, FloatButtonSlots slots) {
    if (!detail::active_host) {
        throw std::logic_error("FloatButtonBackTop requires an active component host");
    }
    detail::active_host->mount(props, std::move(slots));
}
} // namespace ryn
