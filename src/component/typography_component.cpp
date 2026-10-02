#include "component/typography_component.hpp"
#include "component/input_component.hpp"
#include "input/pressable_behavior.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <ryn/icon.hpp>

namespace ryn::detail {
namespace {
thread_local TypographyComponentHost* active_host{};

struct ActionState {
    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId interaction;
    component::RetainedSurfaceId surface;
    bool visible{true};
    bool disabled{};
    bool hovered{};
    TypographyType type{TypographyType::Default};
    input::PressableBehavior press;
    input::FocusPresentation focus;
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
    std::function<void()> activate;
    theme_runtime::Subscription theme;
};

struct TypographyState {
    runtime::ComponentId component;
    runtime::ComponentId display;
    runtime::ComponentId row;
    runtime::ComponentId editor;
    input::InteractionId editor_interaction;
    runtime::ComponentId expand_action;
    runtime::ComponentId copy_action;
    runtime::ComponentId edit_action;
    TextSceneId scene;
    Signal<String> content{String{}};
    Signal<String> draft{String{}};
    Signal<String> expand_label{String{}};
    Signal<IconName> copy_icon{IconName::CopyOutlined};
    Signal<bool> editing{false};
    Signal<bool> expand_visible{false};
    Signal<bool> copy_visible{false};
    Signal<bool> edit_visible{false};
    Signal<bool> copy_disabled{true};
    Signal<bool> actions_disabled{false};
    Signal<std::size_t> max_length{std::numeric_limits<std::size_t>::max()};
    Signal<TypographyEllipsis> ellipsis{TypographyEllipsis{}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
    String original;
    bool controlled{};
    bool disabled{};
    bool expanded{};
    bool pending{};
    bool copied{};
    bool copy_failed{};
    bool has_ellipsis{};
    bool has_copy{};
    bool has_edit{};
    TypographyEllipsis ellipsis_config;
    TypographyCopyable copy_config;
    TypographyEditable edit_config;
    runtime::Size display_size;
    runtime::Size action_size;
    float display_width{};
    std::optional<animation::AnimationTime> copied_until;
    std::function<void(String)> on_edit;
    std::function<void(bool)> on_copy;
    theme_runtime::Subscription theme;
};

void invalidate(WindowComponentServices& services, runtime::ComponentId component) {
    if (!services.components().contains(component)) {
        return;
    }
    services.dirty().invalidate(services.components().root(component),
                                runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                    runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
}

void color_action(WindowComponentServices& services, runtime::ComponentId id) {
    auto* state = services.components().state<ActionState>(id);
    if (!state) {
        return;
    }
    const auto& theme = services.components().theme_scope(id)->snapshot();
    const auto& colors = theme.typography().colors;
    const bool pressed = state->press.pressed() || state->focus.keyboard_pressed;
    Color color = colors.link;
    if (state->type == TypographyType::Danger) {
        color = pressed ? colors.error_text_active : state->hovered ? colors.error_text_hover : colors.error;
    } else if (pressed) {
        color = theme.map().color_link_active;
    } else if (state->hovered) {
        color = theme.map().color_link_hover;
    }
    if (state->disabled) {
        color = colors.disabled;
    }
    state->foreground.set({color.red(), color.green(), color.blue(), color.alpha()});
    services.dirty().invalidate(state->node, runtime::DirtyFlags::Material);
}

void eligibility(WindowComponentServices& services, runtime::ComponentId id) {
    auto* state = services.components().state<ActionState>(id);
    if (!state) {
        return;
    }
    const bool eligible = state->visible && !state->disabled;
    const bool visibility_changed = services.components().set_branch_active(id, state->visible);
    if (visibility_changed) {
        services.mark_scene_structure_dirty();
    }
    services.interactions().set_eligible(state->interaction, eligible);
    if (!eligible) {
        services.pointer().cancel_interaction(state->interaction);
        services.focus().cancel_interaction(state->interaction);
        (void)state->press.reset();
        state->hovered = false;
    }
    color_action(services, id);
    if (visibility_changed) {
        invalidate(services, id);
    } else {
        services.dirty().invalidate(state->node, runtime::DirtyFlags::Material | runtime::DirtyFlags::HitTest);
    }
}

runtime::ComponentId mount_action(WindowComponentServices& services, Prop<String> label,
                                  std::optional<Prop<IconName>> icon, Prop<bool> visible, Prop<bool> disabled,
                                  Prop<TypographyType> type, Prop<runtime::SemanticTypography> typography,
                                  std::function<void()> activate, LayoutStyle style = {}) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<ActionState>();
    auto& state = build.state<ActionState>(id);
    state.component = id;
    state.node = build.root(id);
    state.activate = std::move(activate);
    state.visible = read_prop(visible);
    state.disabled = read_prop(disabled);
    state.type = read_prop(type);
    state.typography.set(read_prop(typography));
    runtime::connect_layout_style(build.scope(id), style, state.node, services.nodes(), services.dirty());
    build.on_resource_cleanup(id, [&services, node = state.node] { services.layout().remove_layout(node); });
    services.layout().set_layout(
        state.node,
        layout::ComponentLayout{
            [&services, id](layout::LayoutEngine& engine, runtime::NodeId node, layout::Constraints limits) {
                const auto* current = services.components().state<ActionState>(id);
                if (!current || !current->visible) {
                    return runtime::Size{};
                }
                float width = 0;
                float height = 0;
                for (auto child : services.nodes().require(node).children) {
                    const auto size = engine.measure_child(
                        child, {0, std::max(0.0F, limits.max_width - width), 0, limits.max_height});
                    width += size.width;
                    height = std::max(height, size.height);
                }
                return limits.constrain({width, height});
            },
            [&services, id](layout::LayoutEngine& engine, runtime::NodeId node, runtime::Rect bounds) {
                const auto* current = services.components().state<ActionState>(id);
                if (!current || !current->visible) {
                    return;
                }
                float x = bounds.x;
                for (auto child : services.nodes().require(node).children) {
                    const auto size = services.nodes().require(child).measured_size;
                    engine.place_child(child,
                                       {x, bounds.y + (bounds.height - size.height) * 0.5F, size.width, size.height});
                    x += size.width;
                }
            }});
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    state.interaction =
        services.interactions().create({id, state.node, {}, state.visible && !state.disabled, true, {}});
    graphics::QuadInstance transparent;
    transparent.opacity = 0;
    state.surface = services.surfaces().create(id, state.node, fragment, state.interaction, {&transparent, 1});
    input::InteractionHandlers pointer;
    pointer.target = [&services, id](input::PointerDispatchContext& event) {
        auto* current = services.components().state<ActionState>(id);
        if (!current) {
            return;
        }
        if (event.kind() == input::PointerEventKind::enter) {
            current->hovered = true;
        }
        if (event.kind() == input::PointerEventKind::leave) {
            current->hovered = false;
        }
        const auto result =
            current->press.dispatch(event, current->interaction, current->visible && !current->disabled);
        color_action(services, id);
        if (result.activate) {
            auto callback = current->activate;
            if (callback) {
                callback();
            }
        }
    };
    services.interactions().set_handlers(state.interaction, std::move(pointer));
    input::FocusHandlers focus;
    focus.state_changed = [&services, id](input::FocusPresentation value) {
        if (auto* current = services.components().state<ActionState>(id)) {
            current->focus = value;
            color_action(services, id);
        }
    };
    focus.activation_allowed = [&services, id] {
        auto* current = services.components().state<ActionState>(id);
        return current && current->visible && !current->disabled;
    };
    focus.activate = [&services, id] {
        if (auto* current = services.components().state<ActionState>(id)) {
            auto callback = current->activate;
            if (callback) {
                callback();
            }
        }
    };
    services.interactions().set_focus_handlers(state.interaction, std::move(focus));
    build.on_resource_cleanup(id, [&services, interaction = state.interaction, surface = state.surface] {
        services.pointer().cancel_interaction(interaction);
        services.focus().cancel_interaction(interaction);
        services.interactions().remove(interaction);
        services.surfaces().destroy(surface);
    });
    auto& scope = build.scope(id);
    connect_prop(scope, visible, [&services, id](bool value) {
        if (auto* s = services.components().state<ActionState>(id)) {
            s->visible = value;
            eligibility(services, id);
        }
    });
    connect_prop(scope, disabled, [&services, id](bool value) {
        if (auto* s = services.components().state<ActionState>(id)) {
            s->disabled = value;
            eligibility(services, id);
        }
    });
    connect_prop(scope, type, [&services, id](TypographyType value) {
        if (auto* s = services.components().state<ActionState>(id)) {
            s->type = value;
            color_action(services, id);
        }
    });
    connect_prop(scope, typography, [&services, id](runtime::SemanticTypography value) {
        if (auto* s = services.components().state<ActionState>(id)) {
            s->typography.set(value);
        }
    });
    const auto theme = build.theme_scope();
    state.theme = theme->capture([&services, id](theme_runtime::DirtyPhase) { color_action(services, id); },
                                 [theme] {
                                     (void)theme->typography_colors();
                                     (void)theme->color_link_hover();
                                     (void)theme->color_link_active();
                                 });
    build.mount_slot_with_semantic_text_style(id, Content{[label, icon] {
                                                  if (icon) {
                                                      ryn::Icon(IconProps{}.name(*icon));
                                                  }
                                                  ryn::Text(TextProps{}.content(label));
                                              }},
                                              Prop<runtime::SemanticForeground>{state.foreground},
                                              Prop<runtime::SemanticTypography>{state.typography});
    return id;
}
} // namespace

TypographyComponentHost::TypographyComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

TypographyComponentHost::~TypographyComponentHost() {
    services_->detach(*this);
}

void* TypographyComponentHost::begin_mount() noexcept {
    auto* previous = active_host;
    active_host = this;
    return previous;
}

void TypographyComponentHost::end_mount(void* previous) noexcept {
    active_host = static_cast<TypographyComponentHost*>(previous);
}

void TypographyComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
    std::erase_if(actions_, [this](auto id) { return !services_->components().contains(id); });
}

bool try_mount_typography_interactions(const TypographyProps& props, TypographySemantics::Role role,
                                       const Prop<TypographyLevel>& level) {
    return active_host && active_host->mount(props, role, level);
}

bool TypographyComponentHost::mount(const TypographyProps& props, TypographySemantics::Role role,
                                    const Prop<TypographyLevel>& level) {
    if (!props.ellipsis_ && !props.copyable_ && !props.editable_) {
        return false;
    }
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<TypographyState>();
    auto& state = build.state<TypographyState>(id);
    state.component = id;
    state.original = read_prop(props.content_);
    state.content.set(state.original);
    state.draft.set(state.original);
    state.controlled = PropAccess::binding(props.content_) != nullptr;
    state.has_ellipsis = props.ellipsis_.has_value();
    state.has_copy = props.copyable_.has_value();
    state.has_edit = props.editable_.has_value();
    state.on_edit = props.on_edit_;
    state.on_copy = props.on_copy_;
    runtime::connect_layout_style(build.scope(id), props.layout_, build.root(id), services.nodes(), services.dirty());
    build.on_resource_cleanup(id, [&services, node = build.root(id)] { services.layout().remove_layout(node); });
    TypographyProps display = props;
    display.layout_ = {};
    display.copyable_.reset();
    display.editable_.reset();
    display.on_edit_ = {};
    display.on_copy_ = {};
    display.content_ = Prop<String>{state.content};
    if (state.has_ellipsis) {
        display.ellipsis_ = Prop<TypographyEllipsis>{state.ellipsis};
    }
    build.mount_slot(id, Content{[display, role, level] { mount_typography_component(display, role, level); }});
    state.display = services.text().mounted_texts().back().component;
    state.scene = services.text().mounted_texts().back().scene;
    state.typography.set(services.text().resolved_typography(state.display));
    build.mount_slot(
        id, Content{[this, id] {
            auto& services = *services_;
            auto& context = runtime::require_component_build_context();
            auto& state = *services.components().state<TypographyState>(id);
            state.row = context.mount_component<int>(0);
            context.on_resource_cleanup(
                state.row, [&services, node = context.root(state.row)] { services.layout().remove_layout(node); });
            services.layout().set_layout(
                context.root(state.row),
                layout::ComponentLayout{
                    [&services](layout::LayoutEngine& engine, runtime::NodeId node, layout::Constraints limits) {
                        float width = 0;
                        float height = 0;
                        for (auto child : services.nodes().require(node).children) {
                            const auto size = engine.measure_child(
                                child, {0, std::max(0.0F, limits.max_width - width), 0, limits.max_height});
                            if (size.width > 0) {
                                if (width > 0) {
                                    width += 4;
                                }
                                width += size.width;
                                height = std::max(height, size.height);
                            }
                        }
                        return limits.constrain({width, height});
                    },
                    [&services](layout::LayoutEngine& engine, runtime::NodeId node, runtime::Rect bounds) {
                        float x = bounds.x;
                        bool first = true;
                        for (auto child : services.nodes().require(node).children) {
                            const auto size = services.nodes().require(child).measured_size;
                            if (size.width <= 0) {
                                continue;
                            }
                            if (!first) {
                                x += 4;
                            }
                            first = false;
                            engine.place_child(
                                child, {x, bounds.y + (bounds.height - size.height) * 0.5F, size.width, size.height});
                            x += size.width;
                        }
                    }});
            context.mount_slot(
                state.row, Content{[this, id] {
                    auto& s = *services_->components().state<TypographyState>(id);
                    if (s.has_ellipsis) {
                        s.expand_action =
                            mount_action(*services_, Prop<String>{s.expand_label}, {}, Prop<bool>{s.expand_visible},
                                         Prop<bool>{s.actions_disabled}, TypographyType::Default,
                                         Prop<runtime::SemanticTypography>{s.typography}, [this, id] {
                                             if (auto* value = services_->components().state<TypographyState>(id)) {
                                                 value->expanded = !value->expanded;
                                                 refresh(id);
                                             }
                                         });
                        actions_.push_back(s.expand_action);
                    }
                    if (s.has_copy) {
                        s.copy_action =
                            mount_action(*services_, String{}, Prop<IconName>{s.copy_icon}, Prop<bool>{s.copy_visible},
                                         Prop<bool>{s.copy_disabled}, TypographyType::Default,
                                         Prop<runtime::SemanticTypography>{s.typography}, [this, id] { copy(id); });
                        actions_.push_back(s.copy_action);
                    }
                    if (s.has_edit) {
                        s.edit_action =
                            mount_action(*services_, String{}, IconName::EditOutlined, Prop<bool>{s.edit_visible},
                                         Prop<bool>{s.actions_disabled}, TypographyType::Default,
                                         Prop<runtime::SemanticTypography>{s.typography}, [this, id] { edit(id); });
                        actions_.push_back(s.edit_action);
                    }
                }});
        }});
    if (state.has_edit && services.input_runtime()) {
        build.mount_slot(id, Content{[this, id] {
                             auto& s = *services_->components().state<TypographyState>(id);
                             ryn::Input(InputProps{}
                                            .value(Prop<String>{s.draft})
                                            .maxLength(Prop<std::size_t>{s.max_length})
                                            .onChange([this, id](String value) {
                                                if (auto* current =
                                                        services_->components().state<TypographyState>(id)) {
                                                    current->draft.set(std::move(value));
                                                    current->pending = false;
                                                }
                                            }));
                         }});
        const auto input = services.input_runtime()->mounted_inputs().back();
        state.editor = input.component;
        state.editor_interaction = input.interaction;
        services.input_runtime()->configure_typography_editor(
            state.editor, Prop<runtime::SemanticTypography>{state.typography}, Prop<bool>{state.editing},
            [this, id](String value) { commit(id, std::move(value), true); }, [this, id] { cancel(id); },
            [this, id](String value) { commit(id, std::move(value), false); });
    }
    services.layout().set_layout(
        build.root(id),
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                auto& s = *services_->components().state<TypographyState>(id);
                if (s.editing.get()) {
                    return engine.measure_child(services_->components().root(s.editor), limits);
                }
                auto measure = [&](bool exclude_expand) {
                    s.action_size = engine.measure_child(services_->components().root(s.row),
                                                         {0, limits.max_width, 0, limits.max_height});
                    float actions = s.action_size.width;
                    if (exclude_expand && s.expand_visible.get() && s.expand_action.valid()) {
                        const auto expand = services_->nodes()
                                                .require(services_->components().root(s.expand_action))
                                                .measured_size.width;
                        actions = std::max(0.0F, actions - expand);
                        if (actions > 0) {
                            actions = std::max(0.0F, actions - 4);
                        }
                    }
                    const float reserve = actions > 0 ? actions + 4 : 0;
                    services_->text().reserve_ellipsis_inline(s.display, reserve);
                    const float width =
                        s.has_ellipsis && !s.expanded ? limits.max_width : std::max(0.0F, limits.max_width - reserve);
                    s.display_size =
                        engine.measure_child(services_->components().root(s.display), {0, width, 0, limits.max_height});
                    s.display_width = std::isfinite(width) ? width : s.display_size.width;
                };
                measure(!s.expanded);
                if (s.has_ellipsis) {
                    s.expand_visible.set(
                        s.ellipsis_config.expandable &&
                        (s.expanded || services_->text().scene_service().text_state(s.scene).truncated()));
                    if (s.expand_visible.get()) {
                        measure(false);
                    }
                }
                const float reserve = s.action_size.width > 0 ? s.action_size.width + 4 : 0;
                float result_width = s.display_size.width + reserve;
                if (std::isfinite(limits.max_width) && s.has_ellipsis) {
                    result_width = limits.max_width;
                }
                return limits.constrain({result_width, std::max(s.display_size.height, s.action_size.height)});
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                auto& s = *services_->components().state<TypographyState>(id);
                if (s.editing.get()) {
                    engine.place_child(services_->components().root(s.editor), bounds);
                    return;
                }
                engine.place_child(services_->components().root(s.display),
                                   {bounds.x, bounds.y, s.display_width, s.display_size.height});
                engine.place_child(services_->components().root(s.row),
                                   {bounds.x + std::max(0.0F, bounds.width - s.action_size.width),
                                    bounds.y + std::max(0.0F, bounds.height - s.action_size.height),
                                    s.action_size.width, s.action_size.height});
            }});
    mounted_.push_back(id);
    auto& scope = build.scope(id);
    connect_prop(scope, props.content_, [this, id](String value) {
        if (auto* s = services_->components().state<TypographyState>(id)) {
            s->original = std::move(value);
            s->content.set(s->original);
            if (s->pending && s->original == s->draft.get()) {
                s->pending = false;
                s->editing.set(false);
            }
            if (!s->editing.get()) {
                s->draft.set(s->original);
            }
            refresh(id);
        }
    });
    if (props.disabled_) {
        connect_prop(scope, *props.disabled_, [this, id](bool value) {
            if (auto* s = services_->components().state<TypographyState>(id)) {
                s->disabled = value;
                if (value && s->editing.get()) {
                    s->editing.set(false);
                    s->draft.set(s->original);
                    s->pending = false;
                }
                refresh(id);
            }
        });
    }
    connect_prop(scope, level, [this, id](TypographyLevel) { refresh(id); });
    for (const auto* source : {&props.strong_, &props.italic_, &props.code_, &props.keyboard_}) {
        if (*source) {
            connect_prop(scope, **source, [this, id](bool) { refresh(id); });
        }
    }
    if (props.ellipsis_) {
        connect_prop(scope, *props.ellipsis_, [this, id](TypographyEllipsis value) {
            if (auto* s = services_->components().state<TypographyState>(id)) {
                s->ellipsis_config = std::move(value);
                s->expanded = s->ellipsis_config.expanded;
                refresh(id);
            }
        });
    }
    if (props.copyable_) {
        connect_prop(scope, *props.copyable_, [this, id](TypographyCopyable value) {
            if (!std::isfinite(value.feedback_milliseconds) || value.feedback_milliseconds < 0) {
                throw std::invalid_argument("copy feedback duration is invalid");
            }
            if (auto* s = services_->components().state<TypographyState>(id)) {
                s->copy_config = std::move(value);
                refresh(id);
            }
        });
    }
    if (props.editable_) {
        connect_prop(scope, *props.editable_, [this, id](TypographyEditable value) {
            if (auto* s = services_->components().state<TypographyState>(id)) {
                s->edit_config = std::move(value);
                refresh(id);
            }
        });
    }
    const auto theme = build.theme_scope();
    state.theme = theme->capture(
        [this, id](theme_runtime::DirtyPhase phase) {
            if (auto* s = services_->components().state<TypographyState>(id)) {
                const bool feedback = s->copied;
                s->copied = false;
                s->copy_failed = false;
                s->copied_until.reset();
                if (feedback || phase != theme_runtime::DirtyPhase::paint_material) {
                    refresh(id);
                }
            }
        },
        [theme] {
            (void)theme->typography_colors();
            (void)theme->typography_fonts();
            (void)theme->typography_headings();
            (void)theme->typography_base_typography();
            (void)theme->typography_inline_code();
            (void)theme->typography_inline_keyboard();
        });
    refresh(id);
    return true;
}

void TypographyComponentHost::refresh(runtime::ComponentId id) {
    auto* s = services_->components().state<TypographyState>(id);
    if (!s) {
        return;
    }
    const bool editing = s->editing.get();
    s->typography.set(services_->text().resolved_typography(s->display));
    s->actions_disabled.set(s->disabled);
    s->copy_disabled.set(s->disabled || !services_->clipboard());
    s->copy_visible.set(s->has_copy && s->copy_config.enabled && !editing);
    s->edit_visible.set(s->has_edit && s->edit_config.enabled && s->editor.valid() && !editing);
    if (!s->has_ellipsis || !s->ellipsis_config.expandable || editing) {
        s->expand_visible.set(false);
    } else if (s->expanded) {
        s->expand_visible.set(true);
    }
    s->expand_label.set(s->expanded ? s->ellipsis_config.collapse_text : s->ellipsis_config.expand_text);
    s->copy_icon.set(s->copied ? IconName::CheckOutlined : IconName::CopyOutlined);
    s->max_length.set(s->edit_config.max_length);
    auto ellipsis = s->ellipsis_config;
    ellipsis.expanded = s->expanded;
    s->ellipsis.set(std::move(ellipsis));
    if (services_->components().set_branch_active(s->display, !editing)) {
        services_->mark_scene_structure_dirty();
    }
    if (services_->components().set_branch_active(s->row, !editing)) {
        services_->mark_scene_structure_dirty();
    }
    if (s->editor.valid()) {
        services_->input_runtime()->set_active(s->editor, editing);
    }
    invalidate(*services_, id);
}

void TypographyComponentHost::copy(runtime::ComponentId id) {
    auto* s = services_->components().state<TypographyState>(id);
    if (!s || s->disabled || !s->copy_config.enabled || !services_->clipboard()) {
        return;
    }
    const auto original = s->original;
    const bool success = services_->clipboard()->write_text(original.view()) == input::ClipboardError::none;
    s = services_->components().state<TypographyState>(id);
    if (!s) {
        return;
    }
    s->copied = success;
    s->copy_failed = !success;
    s->copied_until = success ? std::optional{services_->animation_time() + animation::AnimationDuration::milliseconds(
                                                                                s->copy_config.feedback_milliseconds)}
                              : std::nullopt;
    auto callback = s->on_copy;
    refresh(id);
    if (callback) {
        callback(success);
    }
}

void TypographyComponentHost::edit(runtime::ComponentId id) {
    auto* s = services_->components().state<TypographyState>(id);
    if (!s || s->disabled || !s->edit_config.enabled || !s->editor.valid()) {
        return;
    }
    s->draft.set(s->original);
    s->pending = false;
    s->editing.set(true);
    refresh(id);
    services_->focus().defer_focus(s->editor_interaction, input::FocusModality::keyboard);
}

void TypographyComponentHost::commit(runtime::ComponentId id, String value, bool focus_back) {
    auto* s = services_->components().state<TypographyState>(id);
    if (!s || !s->editing.get()) {
        return;
    }
    s->draft.set(value);
    s->pending = s->controlled;
    if (!s->controlled) {
        s->original = value;
        s->content.set(value);
        s->editing.set(false);
    }
    auto callback = s->on_edit;
    if (callback) {
        callback(value);
    }
    s = services_->components().state<TypographyState>(id);
    if (!s) {
        return;
    }
    if (s->controlled && s->original == s->draft.get()) {
        s->pending = false;
        s->editing.set(false);
    }
    refresh(id);
    if (focus_back && !s->editing.get()) {
        if (auto* action = services_->components().state<ActionState>(s->edit_action)) {
            services_->focus().defer_focus(action->interaction, input::FocusModality::keyboard);
        }
    }
}

void TypographyComponentHost::cancel(runtime::ComponentId id) {
    auto* s = services_->components().state<TypographyState>(id);
    if (!s) {
        return;
    }
    s->draft.set(s->original);
    s->pending = false;
    s->editing.set(false);
    refresh(id);
    if (auto* action = services_->components().state<ActionState>(s->edit_action)) {
        services_->focus().defer_focus(action->interaction, input::FocusModality::keyboard);
    }
}

TypographyInteractionSnapshot TypographyComponentHost::snapshot(runtime::ComponentId id) const {
    auto* s = services_->components().state<TypographyState>(id);
    if (!s) {
        throw std::out_of_range("Typography component is stale");
    }
    const auto& text = services_->text().scene_service().text_state(s->scene);
    return {s->expanded,   text.truncated(), services_->clipboard() != nullptr,
            s->copied,     s->copy_failed,   s->editing.get(),
            s->pending,    s->original,      String::from_utf8(text.display_content().utf8()).value(),
            s->draft.get()};
}

void TypographyComponentHost::on_clipboard_bound() {
    for (auto id : mounted_) {
        refresh(id);
    }
}

void TypographyComponentHost::on_window_active(bool active) {
    if (!active) {
        for (auto id : mounted_) {
            if (auto* s = services_->components().state<TypographyState>(id)) {
                s->copied = false;
                s->copy_failed = false;
                s->copied_until.reset();
                refresh(id);
            }
        }
    }
}

std::size_t TypographyComponentHost::tick_auxiliary(animation::AnimationTime time) {
    std::size_t changed = 0;
    for (auto id : mounted_) {
        if (auto* s = services_->components().state<TypographyState>(id);
            s && s->copied_until && time >= *s->copied_until) {
            s->copied_until.reset();
            s->copied = false;
            refresh(id);
            ++changed;
        }
    }
    return changed;
}

std::optional<animation::AnimationTime> TypographyComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> result;
    for (auto id : mounted_) {
        if (auto* s = services_->components().state<TypographyState>(id);
            s && s->copied_until && (!result || *s->copied_until < *result)) {
            result = s->copied_until;
        }
    }
    return result;
}

void TypographyComponentHost::synchronize_auxiliary_geometry(runtime::Size, runtime::Rect clip) {
    for (auto id : actions_) {
        if (auto* s = services_->components().state<ActionState>(id)) {
            const auto& node = services_->nodes().require(s->node);
            const auto& theme = services_->components().theme_scope(id)->snapshot();
            component::RetainedSurfaceEffects effects;
            effects.shape = {node.bounds, {}};
            effects.translation = node.translation;
            const auto scope = services_->components().theme_scope(id);
            effects.focus_color = scope->focus_outline_color();
            effects.focus_width = scope->focus_outline_width();
            effects.focus_offset = scope->focus_outline_offset();
            effects.focus_opacity = s->visible && !s->disabled && s->focus.focus_visible ? 1.0F : 0.0F;
            effects.ancestor_clip = graphics::EffectClip{1, clip};
            (void)services_->surfaces().update_effects(s->surface, effects);
        }
    }
}

void TypographyComponentHost::mount_link(const LinkProps& props) {
    auto& build = runtime::require_component_build_context();
    const auto theme = build.theme_scope();
    const auto& token = theme->snapshot().typography();
    const auto id = mount_action(*services_, props.content_, {}, true, props.disabled_, props.type_,
                                 runtime::SemanticTypography{token.font_family, token.font_weight, false,
                                                             token.base_font_size, token.base_line_height},
                                 props.click_, props.layout_);
    actions_.push_back(id);
    auto* s = services_->components().state<ActionState>(id);
    s->theme = theme->capture(
        [this, id](theme_runtime::DirtyPhase) {
            if (auto* s = services_->components().state<ActionState>(id)) {
                const auto& token = services_->components().theme_scope(id)->snapshot().typography();
                s->typography.set(
                    {token.font_family, token.font_weight, false, token.base_font_size, token.base_line_height});
                color_action(*services_, id);
            }
        },
        [theme] {
            (void)theme->typography_colors();
            (void)theme->typography_fonts();
            (void)theme->typography_base_typography();
            (void)theme->color_link_hover();
            (void)theme->color_link_active();
        });
}
} // namespace ryn::detail

namespace ryn {
void Link(LinkProps props) {
    if (!detail::active_host) {
        throw std::logic_error("Link requires window component services");
    }
    detail::active_host->mount_link(props);
}
} // namespace ryn
