#include "component/form_component.hpp"
#include "component/form_focus_context.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace ryn::detail {
namespace {
thread_local FormComponentHost* active_forms{};

struct FormState final {
    runtime::ComponentId id;
    std::shared_ptr<FormModel> model{std::make_shared<FormModel>()};
};

struct FormItemState final {
    runtime::NodeId node;
    std::shared_ptr<FormFieldState> field;
    FormItemSnapshot regions;
    Signal<String> label{String{}};
    Signal<String> help{String{}};
    Signal<String> feedback{String{}};
    Signal<runtime::SemanticForeground> label_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> required_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> feedback_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 2> subscriptions;
    std::function<bool(const FormFocusContext&)> focus;
    std::function<void()> reset;
};

runtime::SemanticForeground foreground(Color value) {
    return {value.red(), value.green(), value.blue(), value.alpha()};
}
} // namespace

FormComponentHost::FormComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

FormComponentHost::~FormComponentHost() {
    services_->detach(*this);
}

void* FormComponentHost::begin_mount() noexcept {
    auto* previous = active_forms;
    active_forms = this;
    return previous;
}

void FormComponentHost::end_mount(void* previous) noexcept {
    active_forms = static_cast<FormComponentHost*>(previous);
}

void FormComponentHost::on_destroy() noexcept {
    std::erase_if(forms_, [this](auto id) { return !services_->components().contains(id); });
    std::erase_if(items_, [this](auto id) { return !services_->components().contains(id); });
}

void FormComponentHost::on_dispose() noexcept {
    forms_.clear();
    items_.clear();
}

bool FormComponentHost::interactive(runtime::ComponentId id) const {
    return services_->components().contains(id) && services_->interactions().component_active(id);
}

void FormComponentHost::mount(const FormProps& props, const FormContent& content) {
    auto& build = runtime::require_component_build_context();
    if (build.nearest_state<FormState>()) {
        throw std::invalid_argument("Nested Form is not supported");
    }
    if (props.reference_) {
        if (const auto model = props.reference_->model.lock(); model && model->alive) {
            throw std::invalid_argument("FormRef is already bound");
        }
    }
    const auto id = build.mount_component<FormState>();
    auto& state = build.state<FormState>(id);
    state.id = id;
    const auto model = state.model;
    const auto node = build.root(id);
    auto& services = *services_;
    runtime::connect_layout_style(build.scope(id), props.layout_, node, services.nodes(), services.dirty());
    services.layout().set_layout(node, layout::FlexLayout{layout::FlexDirection::vertical, 0, {}, true});
    model->eligible = [this, id] {
        return interactive(id);
    };
    model->finish = props.on_finish_;
    model->failed = props.on_failed_;
    if (props.reference_) {
        props.reference_->model = model;
    }
    build.on_resource_cleanup(id, [&services, node, model] {
        model->dispose();
        services.layout().remove_layout(node);
    });
    auto& scope = build.scope(id);
    connect_prop(scope, props.disabled_, [model](bool value) { model->disabled.set(value); });
    connect_prop(scope, props.read_only_, [model](bool value) { model->read_only.set(value); });
    connect_prop(scope, props.focus_first_invalid_, [model](bool value) { model->focus_first_invalid = value; });
    forms_.push_back(id);
    build.mount_slot(id, content);
}

void FormComponentHost::mount_item(const FormItemProps& props, const FormItemContent& content) {
    auto& build = runtime::require_component_build_context();
    auto* parent = build.nearest_state<FormState>();
    if (!parent || !props.field_) {
        throw std::invalid_argument("FormItem requires a Form ancestor and an explicit field");
    }
    const auto model = parent->model;
    const auto id = build.mount_component<FormItemState>();
    auto& state = build.state<FormItemState>(id);
    state.node = build.root(id);
    state.field = props.field_;
    state.label.set(read_prop(props.label_));
    state.help.set(read_prop(props.help_));
    const auto field = state.field;
    const auto generation = model->add(
        props.name_, field, [this, id] { return interactive(id); },
        [this, id] {
            const auto* item = services_->components().state<FormItemState>(id);
            if (!item || !interactive(id)) {
                return false;
            }
            const auto callback = item->focus;
            return callback && callback(FormFocusContext{*services_, viewport_, clip_});
        },
        [this, id] {
            if (const auto* item = services_->components().state<FormItemState>(id)) {
                const auto callback = item->reset;
                if (callback) {
                    callback();
                }
            }
        });
    auto& services = *services_;
    build.on_resource_cleanup(id, [model, field, generation, &services, node = state.node] {
        model->remove(field, generation);
        services.layout().remove_layout(node);
    });
    runtime::connect_layout_style(build.scope(id), props.layout_, state.node, services.nodes(), services.dirty());
    connect_prop(build.scope(id), props.disabled_, [field](bool value) { field->disabled.set(value); });
    connect_prop(build.scope(id), props.read_only_, [field](bool value) { field->read_only.set(value); });
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            const auto region = [&](const Content& body, const Signal<runtime::SemanticForeground>& color,
                                    bool clipped) {
                const auto child = context.mount_component<int>(0);
                const auto node = context.root(child);
                services.nodes().require(node).clip_content = clipped;
                services.layout().set_layout(node, layout::BoxLayout{{}, true});
                context.on_resource_cleanup(child, [&services, node] { services.layout().remove_layout(node); });
                if (clipped) {
                    context.mount_slot_with_semantic_text_style(child, body, Prop<runtime::SemanticForeground>{color},
                                                                Prop<runtime::SemanticTypography>{state.font});
                } else {
                    context.mount_slot(child, body);
                }
                return child;
            };
            state.regions.label = region(
                Content{[&] {
                    if (field->required) {
                        const auto mark = context.mount_component<int>(0);
                        const auto node = context.root(mark);
                        services.layout().set_layout(node, layout::BoxLayout{});
                        context.on_resource_cleanup(mark, [&services, node] { services.layout().remove_layout(node); });
                        context.mount_slot_with_semantic_foreground(
                            mark, Content{[] { Text(String{u8"*"}); }},
                            Prop<runtime::SemanticForeground>{state.required_color});
                    }
                    Text(TextProps{}.content(state.label));
                }},
                state.label_color, true);
            state.regions.control =
                region(Content{[content] { SlotContentAccess::function(content)(); }}, state.label_color, false);
            state.regions.feedback =
                region(Content{[feedback = state.feedback] { Text(TextProps{}.content(feedback)); }},
                       state.feedback_color, true);
        }});
    if (props.control_) {
        auto callbacks = props.control_();
        if (!callbacks.first) {
            throw std::invalid_argument("FormItem control ref must be bound by its content slot");
        }
        state.focus = std::move(callbacks.first);
        state.reset = std::move(callbacks.second);
    } else {
        state.focus = [callback = props.focus_, control = state.regions.control](const FormFocusContext& context) {
            return callback && context.visible(context.window, control) && callback();
        };
    }
    items_.push_back(id);
    auto& scope = build.scope(id);
    connect_prop(scope, props.label_, [this, id](String value) {
        if (auto* item = services_->components().state<FormItemState>(id)) {
            item->label.set(std::move(value));
            refresh(id, true);
        }
    });
    connect_prop(scope, props.help_, [this, id](String value) {
        if (auto* item = services_->components().state<FormItemState>(id)) {
            item->help.set(std::move(value));
            refresh(id, true);
        }
    });
    connect_prop(scope, Prop<String>{field->error}, [this, id](String) { refresh(id, true); });
    // Observe authoritative values as well as internal writes; an unrelated field never reruns this slot.
    connect_prop(scope, Prop<FormValue>{bind([field] { return read_form_field(field); })},
                 [weak = std::weak_ptr{model}, field, previous = read_form_field(field)](FormValue value) mutable {
                     if (previous != value) {
                         previous = std::move(value);
                         if (const auto owner = weak.lock()) {
                             owner->value_changed(field);
                         }
                     }
                 });
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::form_colors, &theme_runtime::ThemeScope::form_metrics};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        state.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { refresh(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    refresh(id, true);
}

void FormComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* state = services_->components().state<FormItemState>(id);
    if (!state || !services_->components().scope(id).active()) {
        return;
    }
    const auto& token = services_->components().theme_scope(id)->snapshot().form();
    const auto& metrics = token.metrics;
    const auto error = state->field->error.get();
    const auto feedback = error.empty() ? state->help.get() : error;
    state->feedback.set(feedback);
    state->label_color.set(foreground(token.colors.label));
    state->required_color.set(foreground(token.colors.required));
    state->feedback_color.set(foreground(error.empty() ? token.colors.help : token.colors.error));
    state->font.set({metrics.font_family, 400, false, metrics.font_size, metrics.line_height});
    for (const auto& [component, shown] : {std::pair{state->regions.label, !state->label.get().empty()},
                                           std::pair{state->regions.feedback, !feedback.empty()}}) {
        auto& node = services_->nodes().require(services_->components().root(component));
        if (services_->components().set_branch_active(component, shown)) {
            services_->mark_scene_structure_dirty();
            geometry = true;
        }
        node.out_of_flow = !shown;
        if (!shown) {
            node.layout_size = {};
            node.bounds = {};
        }
    }
    if (geometry) {
        services_->layout().set_layout(
            state->node,
            layout::FlexLayout{layout::FlexDirection::vertical, 0, {0, 0, 0, metrics.item_margin_bottom}, true});
        services_->layout().set_layout(services_->components().root(state->regions.label),
                                       layout::FlexLayout{layout::FlexDirection::horizontal,
                                                          metrics.required_gap,
                                                          {0, 0, 0, metrics.label_gap},
                                                          true,
                                                          false,
                                                          layout::FlexWrap::no_wrap,
                                                          layout::FlexJustify::start,
                                                          layout::FlexAlign::start});
    }
    services_->dirty().invalidate(state->node, geometry
                                                   ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                         runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                   : runtime::DirtyFlags::Material);
}

FormItemSnapshot FormComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* state = services_->components().state<FormItemState>(id);
    if (!state) {
        throw std::out_of_range("FormItem component is stale");
    }
    auto result = state->regions;
    result.error = state->field->error.get();
    return result;
}

void FormComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    viewport_ = viewport;
    clip_ = clip;
}
} // namespace ryn::detail

namespace ryn {
void Form(FormProps props, FormContent content) {
    if (!detail::active_forms) {
        throw std::logic_error("Form requires window component services");
    }
    detail::active_forms->mount(props, content);
}

void FormItem(FormItemProps props, FormItemContent content) {
    if (!detail::active_forms) {
        throw std::logic_error("FormItem requires window component services");
    }
    detail::active_forms->mount_item(props, content);
}
} // namespace ryn
