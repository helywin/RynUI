#include "component/shell_layout_component.hpp"
#include "component/responsive_layout.hpp"

#include "input/pressable_behavior.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
namespace {

thread_local ShellLayoutComponentHost* active_shell{};

struct ShellState final {
    ShellLayoutSnapshot snapshot;
    component::RetainedSurfaceId background;
    component::RetainedSurfaceId trigger_surface;
    std::array<theme_runtime::Subscription, 2> subscriptions;
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> trigger_foreground{{1, 1, 1, 1}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> icon_typography{runtime::SemanticTypography{}};
    Signal<IconName> arrow{IconName::LeftOutlined};
    input::PressableBehavior press;
    input::FocusPresentation focus;
    std::optional<bool> has_sider;
    std::optional<SiderBreakpoint> breakpoint;
    bool breakpoint_initialized{};
    bool controlled{};
    bool collapsible{};
    bool reverse{};
    bool custom_trigger{};
    SiderTriggerMode trigger_mode{};
    float width{200};
    float collapsed_width{80};
    std::vector<runtime::ComponentId> children;
    std::vector<runtime::Size> sizes;
    std::function<void(bool)> on_breakpoint;
    std::function<void(bool, SiderCollapseType)> on_collapse;
};

float length(LogicalLength value) {
    if (value.is_auto() || !std::isfinite(value.value()) || value.value() < 0) {
        throw std::invalid_argument("Sider width must be a finite non-negative logical length");
    }
    return value.value();
}

float breakpoint_width(SiderBreakpoint value) {
    if (static_cast<std::size_t>(value) >= 7) {
        throw std::invalid_argument("Sider breakpoint is invalid");
    }
    return Breakpoints{}.width(value);
}

void validate(SiderTheme value) {
    if (value != SiderTheme::Light && value != SiderTheme::Dark) {
        throw std::invalid_argument("Sider theme is invalid");
    }
}

runtime::SemanticForeground foreground(Color value) {
    return {value.red(), value.green(), value.blue(), value.alpha()};
}

bool flexible(ShellLayoutRole role) {
    return role == ShellLayoutRole::Layout || role == ShellLayoutRole::Content;
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId root) {
    for (auto current = std::optional{child}; current; current = services.components().parent(*current)) {
        if (*current == root) {
            return true;
        }
    }
    return false;
}

runtime::ComponentId mount_body(WindowComponentServices& services, runtime::ComponentBuildContext& build,
                                runtime::ComponentId parent, const Content& content, ShellState& s) {
    runtime::ComponentId body;
    build.mount_slot(
        parent, Content{[&] {
            auto& context = runtime::require_component_build_context();
            body = context.mount_component<int>(0);
            services.components().set_branch_active(
                body, !(s.snapshot.role == ShellLayoutRole::Sider && s.snapshot.collapsed && s.collapsed_width == 0));
            const auto node = context.root(body);
            services.nodes().require(node).clip_content = true;
            if (s.snapshot.role == ShellLayoutRole::Header) {
                services.layout().set_layout(
                    node,
                    layout::FlexLayout{.fill_width = true, .fill_height = true, .align = layout::FlexAlign::center});
            } else {
                services.layout().set_layout(node,
                                             layout::BoxLayout{{}, true, s.snapshot.role != ShellLayoutRole::Footer});
            }
            context.on_resource_cleanup(body, [&services, node] { services.layout().remove_layout(node); });
            context.mount_slot_with_semantic_text_style(body, content, Prop<runtime::SemanticForeground>{s.foreground},
                                                        Prop<runtime::SemanticTypography>{s.typography});
        }});
    return body;
}

} // namespace

ShellLayoutComponentHost::ShellLayoutComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

ShellLayoutComponentHost::~ShellLayoutComponentHost() {
    services_->detach(*this);
}

void* ShellLayoutComponentHost::begin_mount() noexcept {
    auto* previous = active_shell;
    active_shell = this;
    return previous;
}

void ShellLayoutComponentHost::end_mount(void* previous) noexcept {
    active_shell = static_cast<ShellLayoutComponentHost*>(previous);
}

void ShellLayoutComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

ShellLayoutSnapshot ShellLayoutComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<ShellState>(id);
    if (!s) {
        throw std::out_of_range("Layout component is stale");
    }
    return s->snapshot;
}

runtime::ComponentId ShellLayoutComponentHost::mount_shell(ShellLayoutRole role, const LayoutStyle& style) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<ShellState>();
    auto& s = build.state<ShellState>(id);
    s.snapshot.role = role;
    s.snapshot.node = build.root(id);
    auto& services = *services_;
    services.nodes().require(s.snapshot.node).clip_content = role != ShellLayoutRole::Sider;
    runtime::connect_layout_style(build.scope(id), style, s.snapshot.node, services.nodes(), services.dirty());
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.background = services.surfaces().create_content_range(fragment, {});
    build.on_resource_cleanup(id, [&services, node = s.snapshot.node, surface = s.background] {
        services.surfaces().destroy_content_range(surface);
        services.layout().remove_layout(node);
    });
    services.layout().set_layout(
        s.snapshot.node,
        layout::ComponentLayout{[this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                                    return measure(id, engine, limits);
                                },
                                [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                                    place(id, engine, bounds);
                                }});
    mounted_.push_back(id);
    return id;
}

void ShellLayoutComponentHost::complete_mount(runtime::ComponentId id) {
    auto& build = runtime::require_component_build_context();
    auto& s = build.state<ShellState>(id);
    refresh(id);
    const auto theme = services_->components().theme_scope(id);
    const std::array accessors{&theme_runtime::ThemeScope::layout_colors, &theme_runtime::ThemeScope::layout_metrics};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id](theme_runtime::DirtyPhase) { refresh(id); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
}

void ShellLayoutComponentHost::mount(const LayoutProps& props, const LayoutContent& content) {
    const auto id = mount_shell(ShellLayoutRole::Layout, props.layout_);
    auto& build = runtime::require_component_build_context();
    auto& s = build.state<ShellState>(id);
    if (props.has_sider_) {
        s.has_sider = read_prop(*props.has_sider_);
    }
    build.mount_slot(id, content);
    if (props.has_sider_) {
        connect_prop(build.scope(id), *props.has_sider_, [this, id](bool value) {
            if (auto* state = services_->components().state<ShellState>(id)) {
                state->has_sider = value;
                refresh(id);
            }
        });
    }
    complete_mount(id);
}

void ShellLayoutComponentHost::mount(const HeaderProps& props, const HeaderContent& content) {
    const auto id = mount_shell(ShellLayoutRole::Header, props.layout_);
    auto& build = runtime::require_component_build_context();
    auto& s = build.state<ShellState>(id);
    s.snapshot.body =
        mount_body(*services_, build, id, Content{[content] { SlotContentAccess::function(content)(); }}, s);
    complete_mount(id);
}

void ShellLayoutComponentHost::mount(const FooterProps& props, const FooterContent& content) {
    const auto id = mount_shell(ShellLayoutRole::Footer, props.layout_);
    auto& build = runtime::require_component_build_context();
    auto& s = build.state<ShellState>(id);
    s.snapshot.body =
        mount_body(*services_, build, id, Content{[content] { SlotContentAccess::function(content)(); }}, s);
    complete_mount(id);
}

void ShellLayoutComponentHost::mount(const ContentProps& props, const ContentBody& content) {
    const auto id = mount_shell(ShellLayoutRole::Content, props.layout_);
    auto& build = runtime::require_component_build_context();
    auto& s = build.state<ShellState>(id);
    s.snapshot.body =
        mount_body(*services_, build, id, Content{[content] { SlotContentAccess::function(content)(); }}, s);
    complete_mount(id);
}

void ShellLayoutComponentHost::mount(const SiderProps& props, const SiderContent& content) {
    const auto width = length(read_prop(props.width_));
    const auto collapsed_width = length(read_prop(props.collapsed_width_));
    const auto theme = read_prop(props.theme_);
    validate(theme);
    const auto breakpoint = read_prop(props.breakpoint_);
    if (breakpoint) {
        static_cast<void>(breakpoint_width(*breakpoint));
    }
    if (props.collapsed_ && props.default_collapsed_) {
        throw std::invalid_argument("Sider collapsed/defaultCollapsed conflict");
    }
    if (props.trigger_mode_ != SiderTriggerMode::Default && props.trigger_mode_ != SiderTriggerMode::Hidden) {
        throw std::invalid_argument("Sider trigger mode is invalid");
    }
    const auto id = mount_shell(ShellLayoutRole::Sider, props.layout_);
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    auto& s = build.state<ShellState>(id);
    s.width = width;
    s.collapsed_width = collapsed_width;
    s.snapshot.theme = theme;
    s.breakpoint = breakpoint;
    s.controlled = props.collapsed_.has_value();
    s.snapshot.collapsed = props.collapsed_ ? read_prop(*props.collapsed_) : props.default_collapsed_.value_or(false);
    s.collapsible = read_prop(props.collapsible_);
    s.reverse = read_prop(props.reverse_arrow_);
    s.trigger_mode = props.trigger_mode_;
    s.custom_trigger = props.trigger_.has_value();
    s.on_breakpoint = props.on_breakpoint_;
    s.on_collapse = props.on_collapse_;
    s.snapshot.body =
        mount_body(services, build, id, Content{[content] { SlotContentAccess::function(content)(); }}, s);
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            const auto trigger = context.mount_component<int>(0);
            const auto node = context.root(trigger);
            s.snapshot.trigger = trigger;
            services.components().set_branch_active(trigger,
                                                    s.trigger_mode != SiderTriggerMode::Hidden && s.collapsible);
            services.layout().set_layout(node, layout::FlexLayout{.fill_width = true,
                                                                  .fill_height = true,
                                                                  .justify = layout::FlexJustify::center,
                                                                  .align = layout::FlexAlign::center});
            s.snapshot.interaction = services.interactions().create({trigger, node, {}, true, true, {}, true, true});
            const auto hit = context.register_scene_fragment(trigger, runtime::SceneFragmentPlacement::before_children);
            services.scene_composer().set_fragment(hit, {}, s.snapshot.interaction);
            const auto fragment =
                context.register_scene_fragment(trigger, runtime::SceneFragmentPlacement::before_children);
            s.trigger_surface = services.surfaces().create_content_range(fragment, {});
            context.on_resource_cleanup(
                trigger, [&services, node, hit, interaction = s.snapshot.interaction, surface = s.trigger_surface] {
                    services.pointer().cancel_interaction(interaction);
                    services.interactions().remove(interaction);
                    services.scene_composer().remove_fragment(hit);
                    services.surfaces().destroy_content_range(surface);
                    services.layout().remove_layout(node);
                });
            input::InteractionHandlers handlers;
            handlers.target = [this, id](input::PointerDispatchContext& event) {
                auto* state = services_->components().state<ShellState>(id);
                if (!state) {
                    return;
                }
                const auto result =
                    state->press.dispatch(event, state->snapshot.interaction,
                                          state->snapshot.trigger_visible &&
                                              services_->interactions().component_active(state->snapshot.trigger));
                if (event.kind() == input::PointerEventKind::down || event.kind() == input::PointerEventKind::up) {
                    event.stop_propagation();
                }
                if (result.activate) {
                    collapse(id, !state->snapshot.collapsed, SiderCollapseType::ClickTrigger);
                }
            };
            services.interactions().set_handlers(s.snapshot.interaction, std::move(handlers));
            input::FocusHandlers focus;
            focus.state_changed = [this, id](input::FocusPresentation value) {
                if (auto* state = services_->components().state<ShellState>(id)) {
                    state->focus = value;
                    services_->dirty().invalidate(state->snapshot.node, runtime::DirtyFlags::Material);
                }
            };
            focus.activate = [this, id] {
                if (const auto* state = services_->components().state<ShellState>(id)) {
                    collapse(id, !state->snapshot.collapsed, SiderCollapseType::ClickTrigger);
                }
            };
            services.interactions().set_focus_handlers(s.snapshot.interaction, std::move(focus));
            if (props.trigger_) {
                context.mount_slot_with_semantic_text_style(trigger, *props.trigger_,
                                                            Prop<runtime::SemanticForeground>{s.trigger_foreground},
                                                            Prop<runtime::SemanticTypography>{s.typography});
            } else {
                context.mount_slot_with_semantic_text_style(trigger, Content{[&s] { Icon(IconProps{}.name(s.arrow)); }},
                                                            Prop<runtime::SemanticForeground>{s.trigger_foreground},
                                                            Prop<runtime::SemanticTypography>{s.icon_typography});
            }
        }});
    auto& scope = build.scope(id);
    connect_prop(scope, props.width_, [this, id](LogicalLength value) {
        const auto result = length(value);
        if (auto* state = services_->components().state<ShellState>(id)) {
            state->width = result;
            refresh(id);
        }
    });
    connect_prop(scope, props.collapsed_width_, [this, id](LogicalLength value) {
        const auto result = length(value);
        if (auto* state = services_->components().state<ShellState>(id)) {
            state->collapsed_width = result;
            refresh(id);
        }
    });
    if (props.collapsed_) {
        connect_prop(scope, *props.collapsed_, [this, id](bool value) {
            if (auto* state = services_->components().state<ShellState>(id)) {
                state->snapshot.collapsed = value;
                refresh(id);
            }
        });
    }
    connect_prop(scope, props.collapsible_, [this, id](bool value) {
        if (auto* state = services_->components().state<ShellState>(id)) {
            state->collapsible = value;
            refresh(id);
        }
    });
    connect_prop(scope, props.reverse_arrow_, [this, id](bool value) {
        if (auto* state = services_->components().state<ShellState>(id)) {
            state->reverse = value;
            refresh(id);
        }
    });
    connect_prop(scope, props.theme_, [this, id](SiderTheme value) {
        validate(value);
        if (auto* state = services_->components().state<ShellState>(id)) {
            state->snapshot.theme = value;
            refresh(id);
        }
    });
    connect_prop(scope, props.breakpoint_, [this, id](std::optional<SiderBreakpoint> value) {
        if (value) {
            static_cast<void>(breakpoint_width(*value));
        }
        if (auto* state = services_->components().state<ShellState>(id)) {
            state->breakpoint = value;
            state->breakpoint_initialized = false;
            if (!value) {
                state->snapshot.broken = false;
            }
            refresh(id);
        }
    });
    complete_mount(id);
}

void ShellLayoutComponentHost::cancel_hidden(runtime::ComponentId root) {
    const auto order = services_->interactions().declaration_order();
    const std::vector<input::InteractionId> interactions{order.begin(), order.end()};
    for (auto hit : interactions) {
        const auto* record = services_->interactions().find(hit);
        if (record && services_->components().contains(root) && descendant(*services_, record->component, root)) {
            services_->pointer().cancel_interaction(hit);
        }
    }
}

void ShellLayoutComponentHost::refresh(runtime::ComponentId id) {
    auto* s = services_->components().state<ShellState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto& token = services_->components().theme_scope(id)->snapshot().layout();
    const auto& colors = token.colors;
    const auto& metrics = token.metrics;
    const auto role = s->snapshot.role;
    const bool light = s->snapshot.theme == SiderTheme::Light;
    const auto text = role == ShellLayoutRole::Header  ? colors.header_text
                      : role == ShellLayoutRole::Sider ? (light ? colors.light_sider_text : colors.sider_text)
                                                       : colors.footer_text;
    s->foreground.set(foreground(text));
    s->typography.set({SystemFontFamily::ui_sans, 400, false, metrics.font_size, metrics.line_height});
    s->icon_typography.set(
        {SystemFontFamily::ui_sans, 400, false, metrics.trigger_icon_size, metrics.trigger_icon_size});
    runtime::ComponentId hidden_body;
    runtime::ComponentId hidden_trigger;
    if (role == ShellLayoutRole::Sider) {
        const bool zero = s->snapshot.collapsed && s->collapsed_width == 0;
        const bool shown = s->trigger_mode != SiderTriggerMode::Hidden && (s->collapsible || s->snapshot.broken);
        s->snapshot.zero_width = zero;
        s->snapshot.trigger_visible = shown;
        s->trigger_foreground.set(foreground(light ? colors.light_trigger_text : colors.trigger_text));
        s->arrow.set(zero ? IconName::MenuOutlined
                          : (s->snapshot.collapsed != s->reverse ? IconName::RightOutlined : IconName::LeftOutlined));
        if (services_->components().set_branch_active(s->snapshot.body, !zero)) {
            services_->mark_scene_structure_dirty();
            if (zero) {
                hidden_body = s->snapshot.body;
            }
        }
        if (services_->components().set_branch_active(s->snapshot.trigger, shown)) {
            services_->mark_scene_structure_dirty();
            if (!shown) {
                hidden_trigger = s->snapshot.trigger;
            }
        }
        if (services_->components().set_window_layer(s->snapshot.trigger,
                                                     zero && shown ? std::optional{1} : std::nullopt)) {
            services_->mark_scene_structure_dirty();
        }
        // The trigger escapes the zero-width Sider, which does not clip. It
        // still belongs to the surrounding document's scroll and clip domain.
        services_->nodes().require(services_->components().root(s->snapshot.trigger)).clip_boundary = false;
    }
    services_->dirty().invalidate(s->snapshot.node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                        runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Material |
                                                        runtime::DirtyFlags::HitTest);
    if (hidden_body.valid()) {
        cancel_hidden(hidden_body);
    }
    if (hidden_trigger.valid()) {
        cancel_hidden(hidden_trigger);
    }
}

void ShellLayoutComponentHost::collapse(runtime::ComponentId id, bool value, SiderCollapseType type) {
    auto* s = services_->components().state<ShellState>(id);
    if (!s || !services_->components().branch_active(id) || s->snapshot.collapsed == value) {
        return;
    }
    if (type == SiderCollapseType::ClickTrigger &&
        (!s->snapshot.trigger_visible || !services_->interactions().component_active(s->snapshot.trigger))) {
        return;
    }
    const auto callback = s->on_collapse;
    if (!s->controlled) {
        s->snapshot.collapsed = value;
        refresh(id);
    }
    if (callback && services_->components().contains(id) && services_->components().scope(id).active()) {
        callback(value, type);
    }
}

void ShellLayoutComponentHost::prepare_auxiliary_layout() {
    const auto mounted = mounted_;
    const float width = services_->responsive_layout().window_width().get();
    for (auto id : mounted) {
        auto* s = services_->components().state<ShellState>(id);
        if (!s || s->snapshot.role != ShellLayoutRole::Sider || !s->breakpoint) {
            continue;
        }
        const bool broken = width < breakpoint_width(*s->breakpoint);
        if (s->breakpoint_initialized && s->snapshot.broken == broken) {
            continue;
        }
        s->snapshot.broken = broken;
        s->breakpoint_initialized = true;
        const auto callback = s->on_breakpoint;
        refresh(id);
        if (callback && services_->components().contains(id)) {
            callback(broken);
        }
        collapse(id, broken, SiderCollapseType::Responsive);
    }
}

runtime::Size ShellLayoutComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                                layout::Constraints limits) {
    auto& s = *services_->components().state<ShellState>(id);
    const auto& metrics = services_->components().theme_scope(id)->snapshot().layout().metrics;
    const auto role = s.snapshot.role;
    if (role == ShellLayoutRole::Layout) {
        s.children = services_->components().children(id);
        s.sizes.assign(s.children.size(), {});
        const bool horizontal = s.has_sider.value_or(std::ranges::any_of(s.children, [this](auto child) {
            const auto* state = services_->components().state<ShellState>(child);
            return state && state->snapshot.role == ShellLayoutRole::Sider;
        }));
        s.snapshot.horizontal = horizontal;
        const float available = horizontal ? limits.max_width : limits.max_height;
        float fixed{};
        float cross{};
        float weight{};
        const auto is_flexible = [this](auto child) {
            const auto* state = services_->components().state<ShellState>(child);
            return state && flexible(state->snapshot.role);
        };
        const auto grow = [this](auto child) {
            return std::max(1.0F,
                            services_->nodes().require(services_->components().root(child)).external_layout.flex_grow);
        };
        for (std::size_t i = 0; i < s.children.size(); ++i) {
            const auto child = s.children[i];
            if (!services_->components().branch_active(child)) {
                continue;
            }
            if (is_flexible(child) && std::isfinite(available)) {
                weight += grow(child);
                continue;
            }
            auto constraints = limits;
            constraints.min_width = 0;
            constraints.min_height = 0;
            if (horizontal) {
                constraints.max_width = std::max(0.0F, available - fixed);
            } else {
                constraints.max_height = std::max(0.0F, available - fixed);
            }
            s.sizes[i] = engine.measure_child(services_->components().root(child), constraints);
            fixed += horizontal ? s.sizes[i].width : s.sizes[i].height;
            cross = std::max(cross, horizontal ? s.sizes[i].height : s.sizes[i].width);
        }
        float remaining = std::max(0.0F, available - fixed);
        for (std::size_t i = 0; i < s.children.size(); ++i) {
            const auto child = s.children[i];
            if (!services_->components().branch_active(child) || !is_flexible(child) || !std::isfinite(available)) {
                continue;
            }
            const float part = weight > 0 ? remaining * grow(child) / weight : 0;
            auto constraints = limits;
            constraints.min_width = 0;
            constraints.min_height = 0;
            if (horizontal) {
                constraints.min_width = part;
                constraints.max_width = part;
            } else {
                constraints.min_height = part;
                constraints.max_height = part;
            }
            s.sizes[i] = engine.measure_child(services_->components().root(child), constraints);
            fixed += horizontal ? s.sizes[i].width : s.sizes[i].height;
            cross = std::max(cross, horizontal ? s.sizes[i].height : s.sizes[i].width);
        }
        return limits.constrain({horizontal ? (std::isfinite(available) ? available : fixed)
                                            : (std::isfinite(limits.max_width) ? limits.max_width : cross),
                                 horizontal ? (std::isfinite(limits.max_height) ? limits.max_height : cross)
                                            : (std::isfinite(available) ? available : fixed)});
    }
    float width = limits.max_width;
    float height = limits.max_height;
    float padding_inline{};
    float padding_block{};
    float trigger_height{};
    if (role == ShellLayoutRole::Header) {
        height = std::min(metrics.header_height, height);
        padding_inline = metrics.header_padding_inline;
    }
    if (role == ShellLayoutRole::Footer) {
        padding_inline = metrics.footer_padding_inline;
        padding_block = metrics.footer_padding_block;
    }
    if (role == ShellLayoutRole::Sider) {
        width = std::min(s.snapshot.collapsed ? s.collapsed_width : s.width, width);
        trigger_height = s.snapshot.trigger_visible && !s.snapshot.zero_width ? metrics.trigger_height : 0;
    }
    const float inner_width = std::max(0.0F, width - padding_inline * 2);
    const float inner_height = std::max(0.0F, height - padding_block * 2 - trigger_height);
    const bool visible = services_->components().branch_active(s.snapshot.body);
    runtime::Size body;
    if (visible) {
        body = engine.measure_child(services_->components().root(s.snapshot.body), {0, inner_width, 0, inner_height});
    }
    if (role == ShellLayoutRole::Sider && s.snapshot.trigger_visible) {
        const float tw = s.snapshot.zero_width ? metrics.zero_trigger_width : width;
        const float th = s.snapshot.zero_width ? metrics.zero_trigger_height : std::min(trigger_height, height);
        static_cast<void>(
            engine.measure_child(services_->components().root(s.snapshot.trigger), layout::Constraints::fixed(tw, th)));
    }
    if (!std::isfinite(width)) {
        width = body.width + padding_inline * 2;
    }
    if (!std::isfinite(height) || role == ShellLayoutRole::Footer) {
        height = body.height + padding_block * 2 + trigger_height;
    }
    return limits.constrain({width, height});
}

void ShellLayoutComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto& s = *services_->components().state<ShellState>(id);
    const auto role = s.snapshot.role;
    if (role == ShellLayoutRole::Layout) {
        float offset{};
        for (std::size_t i = 0; i < s.children.size(); ++i) {
            const auto child = s.children[i];
            if (!services_->components().contains(child) || !services_->components().branch_active(child)) {
                continue;
            }
            const auto size = s.sizes[i];
            const auto rect =
                s.snapshot.horizontal
                    ? runtime::Rect{bounds.x + offset, bounds.y,
                                    std::min(size.width, std::max(0.0F, bounds.width - offset)), bounds.height}
                    : runtime::Rect{bounds.x, bounds.y + offset, bounds.width,
                                    std::min(size.height, std::max(0.0F, bounds.height - offset))};
            engine.place_child(services_->components().root(child), rect, true, true);
            offset += s.snapshot.horizontal ? rect.width : rect.height;
        }
        return;
    }
    const auto& metrics = services_->components().theme_scope(id)->snapshot().layout().metrics;
    const float pi = role == ShellLayoutRole::Header   ? metrics.header_padding_inline
                     : role == ShellLayoutRole::Footer ? metrics.footer_padding_inline
                                                       : 0;
    const float pb = role == ShellLayoutRole::Footer ? metrics.footer_padding_block : 0;
    const float inline_padding = std::min(pi, bounds.width / 2);
    const float block_padding = std::min(pb, bounds.height / 2);
    const float trigger_height = role == ShellLayoutRole::Sider && s.snapshot.trigger_visible && !s.snapshot.zero_width
                                     ? std::min(metrics.trigger_height, bounds.height)
                                     : 0;
    if (services_->components().branch_active(s.snapshot.body)) {
        engine.place_child(services_->components().root(s.snapshot.body),
                           {bounds.x + inline_padding, bounds.y + block_padding,
                            std::max(0.0F, bounds.width - inline_padding * 2),
                            std::max(0.0F, bounds.height - block_padding * 2 - trigger_height)},
                           true, true);
    }
    if (role == ShellLayoutRole::Sider && s.snapshot.trigger_visible) {
        const auto trigger =
            s.snapshot.zero_width
                ? runtime::Rect{bounds.x - (s.reverse ? metrics.zero_trigger_width : 0),
                                bounds.y + std::min(metrics.zero_trigger_top,
                                                    std::max(0.0F, bounds.height - metrics.zero_trigger_height)),
                                metrics.zero_trigger_width, metrics.zero_trigger_height}
                : runtime::Rect{bounds.x, bounds.y + bounds.height - trigger_height, bounds.width, trigger_height};
        engine.place_child(services_->components().root(s.snapshot.trigger), trigger, true, true);
    }
}

void ShellLayoutComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<ShellState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().layout();
        const auto role = s->snapshot.role;
        const bool light = s->snapshot.theme == SiderTheme::Light;
        auto color = role == ShellLayoutRole::Header ? token.colors.header_background
                     : role == ShellLayoutRole::Sider
                         ? (light ? token.colors.light_sider_background : token.colors.sider_background)
                     : role == ShellLayoutRole::Footer ? token.colors.footer_background
                                                       : token.colors.body_background;
        const auto& node = services_->nodes().require(s->snapshot.node);
        const auto clip = graphics::EffectClip{
            1, services_->nodes().content_clip(node.id, services_->component_clip(id, viewport, document_clip))};
        const auto effects =
            graphics::make_corner_fill_effects({node.bounds, 0}, {true, true, true, true}, color,
                                               services_->nodes().content_opacity(node.id), node.translation, clip);
        static_cast<void>(services_->surfaces().update_content_effects(s->background, effects));
        if (role != ShellLayoutRole::Sider || !s->snapshot.trigger_visible) {
            continue;
        }
        const auto& trigger = services_->nodes().require(services_->components().root(s->snapshot.trigger));
        const auto tc = graphics::EffectClip{
            1, services_->nodes().content_clip(
                   trigger.id, services_->component_clip(s->snapshot.trigger, viewport, document_clip))};
        const auto opacity = services_->nodes().content_opacity(trigger.id);
        std::array<graphics::RoundedEffectInstance, 8> trigger_effects;
        const auto fill = graphics::make_corner_fill_effects({trigger.bounds, 0}, {true, true, true, true},
                                                             light ? token.colors.light_trigger_background
                                                                   : token.colors.trigger_background,
                                                             opacity, trigger.translation, tc);
        std::copy(fill.begin(), fill.end(), trigger_effects.begin());
        const auto ring = graphics::make_corner_outline_effects(
            {trigger.bounds, 0}, {true, true, true, true}, token.metrics.focus_width, 0, token.colors.focus,
            s->focus.focus_visible ? opacity : 0, trigger.translation, tc);
        std::copy(ring.begin(), ring.end(), trigger_effects.begin() + 4);
        static_cast<void>(services_->surfaces().update_content_effects(s->trigger_surface, trigger_effects));
    }
}

} // namespace ryn::detail

namespace ryn {
namespace {
detail::ShellLayoutComponentHost& shell_host() {
    if (!detail::active_shell) {
        throw std::logic_error("Layout requires window component services");
    }
    return *detail::active_shell;
}
} // namespace

void Layout(LayoutProps props, LayoutContent content) {
    shell_host().mount(props, content);
}

void Header(HeaderProps props, HeaderContent content) {
    shell_host().mount(props, content);
}

void Footer(FooterProps props, FooterContent content) {
    shell_host().mount(props, content);
}

void ContentArea(ContentProps props, ContentBody content) {
    shell_host().mount(props, content);
}

void Sider(SiderProps props, SiderContent content) {
    shell_host().mount(props, content);
}

} // namespace ryn
