#include "component/form_component.hpp"
#include "component/form_focus_context.hpp"
#include "component/form_native_control.hpp"
#include "component/form_control_binding.hpp"
#include "component/responsive_layout.hpp"
#include "component/input_component.hpp"
#include <ryn/tooltip.hpp>
#include <ryn/flex.hpp>
#include <limits>
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ryn::detail {
struct FormHostLifetime final {
    WindowComponentServices* window{};
    std::thread::id owner{std::this_thread::get_id()};
    runtime::Size viewport;
    runtime::Rect clip;
};

struct FormControlConnection final {
    std::weak_ptr<FormHostLifetime> lifetime;
    std::weak_ptr<FormModel> model;
    std::weak_ptr<FormFieldState> field;
    runtime::ComponentId form;
    runtime::ComponentId item;
    runtime::ComponentId region;
    std::uint64_t registration_generation{};
    std::uint64_t epoch{};
    FormNativeControl native;
    bool connected{};
    bool retired{};
};

struct FormListRow final {
    std::uint64_t key{};
    Signal<std::size_t> index{0};
    FormData initial;
    runtime::ComponentId component;
};

struct FormListState final {
    runtime::ComponentId id;
    std::weak_ptr<FormModel> model;
    std::function<FormNamePath()> prefix;
    std::vector<std::shared_ptr<FormListRow>> rows;
    FormListItem render;
    std::shared_ptr<FormFieldState> validation{make_form_field(String{})};
    std::uint64_t next_key{};
    bool alive{true};
    bool busy{};
};

struct FormListRefState final {
    std::weak_ptr<FormListState> list;
    std::function<bool(std::vector<std::shared_ptr<FormListRow>>)> update;
};

struct FormListRowContext final {
    std::shared_ptr<FormListState> list;
    std::shared_ptr<FormListRow> row;
};

namespace {
thread_local FormComponentHost* active_forms{};
thread_local std::optional<runtime::ComponentId> private_editor_item;
thread_local const runtime::ComponentHost* private_editor_host{};

struct FormProviderState final {
    bool alive{true};
    std::vector<std::pair<String, FormRef>> forms;
    std::function<void(FormProviderEvent)> change;
    std::function<void(FormProviderEvent)> finish;
};

struct FormProviderContext final {
    std::shared_ptr<FormProviderState> state;
};

struct FormState final {
    runtime::ComponentId id;
    std::shared_ptr<FormModel> model{std::make_shared<FormModel>()};
    Signal<FormPresentation> presentation{FormPresentation{}};
    FormScrollTarget scroll;
    std::uint64_t scroll_generation{};
    std::function<void()> pending_scroll_focus;
};

struct FormItemState final {
    runtime::NodeId node;
    runtime::ComponentId id;
    runtime::ComponentId form;
    std::shared_ptr<FormFieldState> field;
    std::uint64_t registration_generation{};
    std::uint64_t binding_epoch{};
    std::shared_ptr<FormControlConnection> connection;
    std::weak_ptr<FormHostLifetime> lifetime;
    runtime::ComponentId native_enter_claimant;
    bool control_claimed{};
    bool native_enter_claimed{};
    bool no_style{};
    bool hidden{};
    bool applied_hidden{};
    bool has_feedback{};
    bool stacked_columns{};
    bool custom_label{};
    bool custom_help{};
    bool custom_extra{};
    runtime::ComponentId feedback_text;
    runtime::ComponentId help_content;
    Signal<String> colon{String{}};
    Signal<IconSource> icon_source{IconSource{IconName::CheckCircleFilled}};
    Signal<bool> icon_spinning{false};
    float control_width{};
    float icon_width{};
    float icon_height{};
    float control_row_height{};
    std::optional<bool> required_override;
    std::optional<FormValidateStatus> explicit_status;
    FormPresentation presentation;
    Signal<String> displayed_label{String{}};
    Signal<String> required_mark{String{}};
    Signal<String> extra{String{}};
    std::function<std::optional<FormControlGeometry>(const FormFocusContext&)> geometry;
    float label_width{};
    float wrapper_x{};
    float wrapper_width{};
    float label_height{};
    float control_height{};
    float feedback_height{};
    float extra_height{};
    FormItemSnapshot regions;
    Signal<String> label{String{}};
    Signal<String> help{String{}};
    Signal<String> feedback{String{}};
    Signal<runtime::SemanticForeground> label_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> required_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> feedback_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 3> subscriptions;
    std::function<bool(const FormFocusContext&)> focus;
    std::function<void()> reset;
};

bool current_lifetime(const std::shared_ptr<FormHostLifetime>& life) {
    return life && life->window && life->owner == std::this_thread::get_id();
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto id = std::optional{child}; id; id = services.components().parent(*id)) {
        if (!services.components().contains(*id)) {
            return false;
        }
        if (*id == parent) {
            return true;
        }
    }
    return false;
}

FormItemState* registered_item(const std::shared_ptr<FormHostLifetime>& life, runtime::ComponentId id) {
    if (!current_lifetime(life)) {
        return nullptr;
    }
    auto& components = life->window->components();
    auto* item = components.state<FormItemState>(id);
    if (!item || !components.scope(id).active() || !item->registration_generation || !item->field ||
        !item->field->registered || item->field->generation != item->registration_generation) {
        return nullptr;
    }
    const auto* form = components.state<FormState>(item->form);
    const auto model = item->field->owner.lock();
    if (!form || !components.scope(item->form).active() || !model || !model->alive || form->model != model ||
        !descendant(*life->window, id, item->form)) {
        return nullptr;
    }
    const bool current = std::ranges::any_of(model->fields(), [&](const auto& registration) {
        return registration.field == item->field && registration.generation == item->registration_generation;
    });
    return current ? item : nullptr;
}

bool item_receives(const std::shared_ptr<FormHostLifetime>& life, runtime::ComponentId id) {
    const auto* item = registered_item(life, id);
    return item && !item->hidden && life->window->focus().state().window_active &&
           life->window->interactions().component_active(id) &&
           life->window->interactions().subtree_intersects_input_domain(id);
}

bool connection_current(const std::shared_ptr<FormControlConnection>& connection) {
    if (!connection || connection->retired) {
        return false;
    }
    const auto life = connection->lifetime.lock();
    const auto* item = registered_item(life, connection->item);
    const auto field = connection->field.lock();
    return item && field && item->form == connection->form && item->field == field &&
           field->owner.lock() == connection->model.lock() &&
           item->registration_generation == connection->registration_generation &&
           item->binding_epoch == connection->epoch && item->regions.control == connection->region &&
           item->connection == connection && field->connection.lock() == connection;
}

bool native_admitted(const std::shared_ptr<FormControlConnection>& connection) {
    if (!connection_current(connection) || !connection->connected || !connection->native.current ||
        !connection->native.current()) {
        return false;
    }
    const auto life = connection->lifetime.lock();
    const auto* item = registered_item(life, connection->item);
    const auto field = connection->field.lock();
    const auto& native = connection->native;
    if (!item || item->hidden || !field || form_field_disabled(field) || form_field_read_only(field) ||
        native.window != life->window || !descendant(*life->window, native.component, connection->region) ||
        !life->window->interactions().subtree_intersects_input_domain(native.component)) {
        return false;
    }
    for (auto id = std::optional{native.component}; id; id = life->window->components().parent(*id)) {
        if (life->window->components().state<FormItemState>(*id)) {
            if (*id != connection->item) {
                return false;
            }
            break;
        }
    }
    const auto geometry =
        native.geometry ? native.geometry(FormFocusContext{*life->window, life->viewport, life->clip}) : std::nullopt;
    if (!geometry || !geometry->eligible) {
        return false;
    }
    const auto& bounds = geometry->bounds;
    const auto& clip = geometry->clip;
    return bounds.width > 0 && bounds.height > 0 && clip.width > 0 && clip.height > 0 &&
           bounds.x < clip.x + clip.width && bounds.x + bounds.width > clip.x && bounds.y < clip.y + clip.height &&
           bounds.y + bounds.height > clip.y;
}

bool item_maintainable(const std::shared_ptr<FormHostLifetime>& life, runtime::ComponentId id) {
    const auto* item = registered_item(life, id);
    return item && (item_receives(life, id) || native_admitted(item->connection));
}

void retire_connection(std::shared_ptr<FormControlConnection> connection) noexcept {
    if (!connection || std::exchange(connection->retired, true)) {
        return;
    }
    connection->connected = false;
    const auto life = connection->lifetime.lock();
    const auto field = connection->field.lock();
    const auto model = connection->model.lock();
    if (!current_lifetime(life)) {
        return;
    }
    if (auto* item = life->window->components().state<FormItemState>(connection->item);
        item && item->connection == connection && item->binding_epoch == connection->epoch) {
        item->connection.reset();
        item->control_claimed = false;
        if (!connection->native.component.valid() || item->native_enter_claimant == connection->native.component) {
            item->native_enter_claimant = {};
            item->native_enter_claimed = false;
        }
    }
    if (field && field->connection.lock() == connection) {
        field->connection.reset();
        field->due.reset();
        field->timeout.reset();
    }
    if (model && model->alive) {
        try {
            model->cancel_validation();
        } catch (...) {
            // Scope cleanup is noexcept. Use the same ComponentHost retirement
            // transaction; never reconcile WindowServices from inside retirement.
            static_cast<void>(life->window->components().destroy(connection->form));
        }
    }
}

bool form_scope_interactive(const std::shared_ptr<FormHostLifetime>& life, runtime::ComponentId id,
                            const std::shared_ptr<FormModel>& model, std::span<const runtime::ComponentId> items) {
    if (!current_lifetime(life)) {
        return false;
    }
    auto& services = *life->window;
    auto& components = services.components();
    const auto* form = components.state<FormState>(id);
    if (!model || !model->alive || !form || form->model != model || !components.scope(id).active() ||
        !components.branch_active(id) || !services.focus().state().window_active) {
        return false;
    }
    if (services.interactions().component_active(id)) {
        return true;
    }
    for (auto item_id : items) {
        const auto* item = registered_item(life, item_id);
        if (item && item->form == id && item->field->owner.lock() == model && item_maintainable(life, item_id)) {
            return true;
        }
    }
    return false;
}

void validate_presentation(const FormPresentation& value) {
    if (value.layout != FormLayout::Horizontal && value.layout != FormLayout::Vertical &&
        value.layout != FormLayout::Inline) {
        throw std::invalid_argument("Invalid Form layout");
    }
    if (value.label_align != FormLabelAlign::Left && value.label_align != FormLabelAlign::Right) {
        throw std::invalid_argument("Invalid Form label alignment");
    }
    if (value.required_mark != FormRequiredMark::Required && value.required_mark != FormRequiredMark::Optional &&
        value.required_mark != FormRequiredMark::None) {
        throw std::invalid_argument("Invalid Form required mark");
    }
    if (value.size != ControlSize::Small && value.size != ControlSize::Middle && value.size != ControlSize::Large) {
        throw std::invalid_argument("Invalid Form control size");
    }
    if (value.variant != InputVariant::Outlined && value.variant != InputVariant::Filled &&
        value.variant != InputVariant::Borderless && value.variant != InputVariant::Underlined) {
        throw std::invalid_argument("Invalid Form control variant");
    }
    if (value.direction != TextDirection::Auto && value.direction != TextDirection::LeftToRight &&
        value.direction != TextDirection::RightToLeft) {
        throw std::invalid_argument("Invalid Form direction");
    }
    if (value.locale.required.empty()) {
        throw std::invalid_argument("Form required locale message cannot be empty");
    }
    for (const auto& column : {value.label_col, value.wrapper_col}) {
        const auto validate = [](FormColumn col) {
            if (col.span < 0 || col.span > 24 || col.offset < 0 || col.offset > 24 || col.span + col.offset > 24) {
                throw std::invalid_argument("Invalid Form column span or offset");
            }
        };
        validate(column.fallback());
        for (const auto& col : column.overrides()) {
            if (col) {
                validate(*col);
            }
        }
    }
}

runtime::SemanticForeground foreground(Color value) {
    return {value.red(), value.green(), value.blue(), value.alpha()};
}
} // namespace

FormComponentHost::FormComponentHost(WindowComponentServices& services)
    : services_(&services), lifetime_(std::make_shared<FormHostLifetime>()) {
    lifetime_->window = &services;
    services.attach(*this);
}

FormComponentHost::~FormComponentHost() {
    lifetime_->window = nullptr;
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
    return services_->components().contains(id) && services_->focus().state().window_active &&
           services_->interactions().component_active(id);
}

void FormComponentHost::mount(const FormProps& props, const FormContent& content) {
    auto& build = runtime::require_component_build_context();
    if (props.reference_) {
        if (const auto model = props.reference_->model.lock(); model && model->alive) {
            throw std::invalid_argument("FormRef is already bound");
        }
    }
    const auto initial_presentation = props.presentation_ ? read_prop(*props.presentation_) : props.options_;
    validate_presentation(initial_presentation);
    const auto id = build.mount_component<FormState>();
    auto& state = build.state<FormState>(id);
    state.id = id;
    state.scroll = props.scroll_;
    state.presentation.set(initial_presentation);
    const auto model = state.model;
    const auto node = build.root(id);
    auto& services = *services_;
    runtime::connect_layout_style(build.scope(id), props.layout_, node, services.nodes(), services.dirty());
    services.layout().set_layout(node, layout::FlexLayout{layout::FlexDirection::vertical, 0, {}, true});
    model->eligible = [this, id, weak = std::weak_ptr{model}] {
        return form_scope_interactive(lifetime_, id, weak.lock(), items_);
    };
    model->preserve = props.preserve_;
    model->clear_on_destroy = props.clear_on_destroy_;
    model->changed = props.changed_;
    model->fields_changed = props.fields_changed_;
    model->scroll_first = props.scroll_first_;
    model->scroll = [this, id](FormNamePath path, FormScrollOptions options) {
        return scroll_field(id, std::move(path), options);
    };
    model->control = [this, id](FormNamePath path) {
        return field_control(id, std::move(path));
    };
    model->retire = [this, id] {
        services_->destroy(id);
    };
    model->clock = [this] {
        return services_->animation_time();
    };
    model->wake = [this] {
        services_->request_frame();
    };
    model->initial_values = props.initial_values_;
    model->stored_values.set(props.initial_values_);
    model->finish = props.on_finish_;
    model->failed = props.on_failed_;
    if (!props.name_.empty()) {
        if (auto* context = build.nearest_state<FormProviderContext>()) {
            const auto provider = context->state;
            if (std::ranges::any_of(provider->forms, [&](const auto& form) { return form.first == props.name_; })) {
                throw std::invalid_argument("FormProvider names must be unique");
            }
            FormRef ref;
            ref.state_->model = model;
            provider->forms.emplace_back(props.name_, ref);
            const auto event = [provider, ref, name = props.name_](FormResult result) {
                return FormProviderEvent{name, ref, provider->forms, std::move(result)};
            };
            const auto finish = model->finish;
            model->finish = [provider, finish, event, weak = std::weak_ptr{model}](FormResult result) {
                if (finish) {
                    finish(result);
                }
                const auto model = weak.lock();
                if (provider->alive && model && model->alive) {
                    const auto callback = provider->finish;
                    if (callback) {
                        callback(event(std::move(result)));
                    }
                }
            };
            const auto changed = model->changed;
            model->changed = [provider, changed, event, weak = std::weak_ptr{model}](FormFieldValue value,
                                                                                     FormData values) {
                if (changed) {
                    changed(value, values);
                }
                const auto model = weak.lock();
                if (provider->alive && model && model->alive) {
                    const auto callback = provider->change;
                    if (callback) {
                        FormResult result;
                        result.values = {value};
                        result.structure = std::move(values);
                        auto change = event(std::move(result));
                        change.changed_fields = {
                            {value.path, value.value, model->get_meta(value.path).value_or(FormFieldMeta{})}};
                        callback(std::move(change));
                    }
                }
            };
            build.on_resource_cleanup(id, [provider, name = props.name_] {
                std::erase_if(provider->forms, [&](const auto& form) { return form.first == name; });
            });
        }
    }

    if (props.reference_) {
        props.reference_->model = model;
    }
    build.on_resource_cleanup(id, [&services, node, model, reference = props.reference_] {
        model->dispose();
        if (reference && reference->model.lock() == model) {
            reference->revision.set(reference->revision.get() + 1);
        }
        services.layout().remove_layout(node);
    });
    auto& scope = build.scope(id);
    connect_prop(scope, props.disabled_, [model](bool value) { model->disabled.set(value); });
    connect_prop(scope, props.read_only_, [model](bool value) { model->read_only.set(value); });
    connect_prop(scope, props.focus_first_invalid_, [model](bool value) { model->focus_first_invalid = value; });
    connect_prop(scope, props.presentation_.value_or(Prop<FormPresentation>{props.options_}),
                 [this, id](FormPresentation value) {
                     auto* form = services_->components().state<FormState>(id);
                     if (!form) {
                         return;
                     }
                     validate_presentation(value);
                     form->presentation.set(value);
                     const auto& token = services_->components().theme_scope(id)->snapshot().form();
                     auto layout =
                         layout::FlexLayout{value.layout == FormLayout::Inline ? layout::FlexDirection::horizontal
                                                                               : layout::FlexDirection::vertical,
                                            value.layout == FormLayout::Inline ? token.metrics.item_margin_bottom : 0,
                                            {},
                                            true};
                     layout.wrap =
                         value.layout == FormLayout::Inline ? layout::FlexWrap::wrap : layout::FlexWrap::no_wrap;
                     layout.right_to_left = value.direction == TextDirection::RightToLeft;
                     services_->layout().set_layout(services_->components().root(id), layout);
                     services_->dirty().invalidate(services_->components().root(id),
                                                   runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                       runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
                 });
    forms_.push_back(id);
    build.mount_slot(id, content);
    if (props.reference_ && props.reference_->model.lock() == model) {
        props.reference_->revision.set(props.reference_->revision.get() + 1);
    }
}

void FormComponentHost::mount_item(const FormItemProps& props, const FormItemContent& content) {
    auto& build = runtime::require_component_build_context();
    auto* parent = build.nearest_state<FormState>();
    if (!parent) {
        throw std::invalid_argument("FormItem requires a Form ancestor");
    }
    const auto* containing_item = build.nearest_state<FormItemState>();
    const auto containing_field = containing_item ? containing_item->field : std::shared_ptr<FormFieldState>{};
    const auto resolve_presentation = [props, inherited = parent->presentation] {
        auto value = props.presentation_ ? read_prop(*props.presentation_) : inherited.get();
        if (props.form_layout_) {
            value.layout = *props.form_layout_;
        }
        if (props.label_col_) {
            value.label_col = *props.label_col_;
        }
        if (props.wrapper_col_) {
            value.wrapper_col = *props.wrapper_col_;
        }
        if (props.label_align_) {
            value.label_align = *props.label_align_;
        }
        if (props.label_wrap_) {
            value.label_wrap = *props.label_wrap_;
        }
        if (props.colon_) {
            value.colon = *props.colon_;
        }
        validate_presentation(value);
        return value;
    };
    const auto model = parent->model;
    const auto id = build.mount_component<FormItemState>();
    auto& state = build.state<FormItemState>(id);
    state.node = build.root(id);
    state.id = id;
    state.custom_label = props.label_slot_.has_value();
    state.custom_help = props.help_slot_.has_value();
    state.custom_extra = props.extra_slot_.has_value();
    state.form = parent->id;
    state.lifetime = lifetime_;
    state.presentation = resolve_presentation();
    state.field = props.field_ ? props.field_ : make_form_field(String{});
    state.label.set(read_prop(props.label_));
    state.help.set(read_prop(props.help_));
    const auto field = state.field;
    field->preserve = props.preserve_.value_or(model->preserve);
    if (!props.field_) {
        field->owner = model;
    }
    field->presentation.set(state.presentation);
    auto path = props.path_;
    auto* row = build.nearest_state<FormListRowContext>();
    if (props.name_.empty() && row && row->list->model.lock() == model) {
        path = row->list->prefix().append(FormNamePath{row->row->index.get()}).append(path);
        if (const auto* initial = row->row->initial.at(props.path_)) {
            if (const auto* value = std::get_if<FormValue>(&initial->value)) {
                if (value->index() != field->initial.index()) {
                    throw std::invalid_argument("FormList initial field type mismatch");
                }
                field->initial = *value;
                write_form_field(field, *value);
            }
        }
    }
    auto initial = props.initial_;
    if (const auto* data = props.field_ ? model->initial_values.at(path) : nullptr) {
        if (const auto* value = std::get_if<FormValue>(&data->value)) {
            initial = *value;
        } else {
            throw std::invalid_argument("Form initialValues expected a typed leaf");
        }
    }
    if (initial) {
        if (initial->index() != field->initial.index()) {
            throw std::invalid_argument("Form initialValue type mismatch");
        }
        field->initial = *initial;
        if (!field->source) {
            field->value.set(*initial);
        }
    }
    const auto generation =
        props.field_
            ? model->add(
                  path, field, [weak = std::weak_ptr{lifetime_}, id] { return item_maintainable(weak.lock(), id); },
                  [weak = std::weak_ptr{lifetime_}, id] {
                      const auto life = weak.lock();
                      const auto* item = registered_item(life, id);
                      if (!item || !item_maintainable(life, id)) {
                          return false;
                      }
                      const auto connection = item->connection;
                      const auto callback = item_receives(life, id) ? item->focus : connection->native.focus;
                      const bool focused =
                          callback && callback(FormFocusContext{*life->window, life->viewport, life->clip});
                      return focused && item_maintainable(life, id) && (!connection || connection_current(connection));
                  },
                  [this, id] {
                      if (const auto* item = services_->components().state<FormItemState>(id)) {
                          const auto callback = item->reset;
                          if (callback) {
                              callback();
                          }
                      }
                  })
            : 0;
    auto* registered_item = services_->components().state<FormItemState>(id);
    if (!registered_item) {
        if (generation) {
            model->remove(field, generation);
        }
        return;
    }
    registered_item->registration_generation = generation;
    if (generation) {
        field->direct_eligible = [weak = std::weak_ptr{lifetime_}, id] {
            return item_receives(weak.lock(), id);
        };
    }
    auto& services = *services_;
    build.on_resource_cleanup(id, [model, field, generation, &services, id, node = state.node] {
        if (const auto* item = services.components().state<FormItemState>(id)) {
            retire_connection(item->connection);
        }
        if (generation) {
            model->remove(field, generation);
        } else {
            field->status_override.set(std::nullopt);
            field->display_status.set(FormValidateStatus::None);
            field->owner.reset();
        }
        services.layout().remove_layout(node);
    });
    runtime::connect_layout_style(build.scope(id), props.layout_, state.node, services.nodes(), services.dirty());
    connect_prop(build.scope(id), props.disabled_, [field](bool value) { field->disabled.set(value); });
    connect_prop(build.scope(id), props.read_only_, [field](bool value) { field->read_only.set(value); });
    FormRegistrationScope registration_scope{field, generation};
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
                    state.regions.control = child;
                    context.mount_slot(child, body);
                }
                return child;
            };
            state.regions.label = region(
                Content{[&] {
                    {
                        const auto mark = context.mount_component<int>(0);
                        const auto node = context.root(mark);
                        services.layout().set_layout(node, layout::BoxLayout{});
                        context.on_resource_cleanup(mark, [&services, node] { services.layout().remove_layout(node); });
                        context.mount_slot_with_semantic_foreground(
                            mark, Content{[marker = state.required_mark] { Text(TextProps{}.content(marker)); }},
                            Prop<runtime::SemanticForeground>{state.required_color});
                    }
                    const auto label_content = [label = state.displayed_label, colon = state.colon,
                                                slot = props.label_slot_] {
                        if (slot) {
                            Flex(FlexProps{}.gap(dp(0)), FlexContent{[slot, colon] {
                                     SlotContentAccess::function (*slot)();
                                     Text(TextProps{}.content(colon));
                                 }});
                        } else {
                            Text(TextProps{}.content(label));
                        }
                    };
                    if (props.tooltip_configured_) {
                        Tooltip(TooltipProps{}.title(props.tooltip_).titleAvailable(bind([tooltip = props.tooltip_] {
                            return !read_prop(tooltip).empty();
                        })),
                                TooltipTrigger{label_content});
                    } else {
                        label_content();
                    }
                }},
                state.label_color, true);
            state.regions.control =
                region(Content{[content] { SlotContentAccess::function(content)(); }}, state.label_color, false);
            state.regions.feedback = region(
                Content{[&] {
                    if (!props.help_slot_) {
                        Text(TextProps{}.content(state.feedback));
                        return;
                    }
                    const auto child = context.mount_component<int>(0);
                    state.feedback_text = child;
                    const auto node = context.root(child);
                    services.layout().set_layout(node, layout::BoxLayout{{}, true});
                    context.on_resource_cleanup(child, [&services, node] { services.layout().remove_layout(node); });
                    context.mount_slot(child,
                                       Content{[feedback = state.feedback] { Text(TextProps{}.content(feedback)); }});
                    if (props.help_slot_) {
                        const auto help = context.mount_component<int>(0);
                        state.help_content = help;
                        const auto help_node = context.root(help);
                        services.layout().set_layout(help_node, layout::BoxLayout{{}, true});
                        context.on_resource_cleanup(
                            help, [&services, help_node] { services.layout().remove_layout(help_node); });
                        context.mount_slot(help, *props.help_slot_);
                    }
                }},
                state.feedback_color, true);
            state.regions.extra = region(Content{[extra = state.extra, slot = props.extra_slot_] {
                                             if (slot) {
                                                 SlotContentAccess::function (*slot)();
                                             } else {
                                                 Text(TextProps{}.content(extra));
                                             }
                                         }},
                                         state.feedback_color, true);
            state.regions.icon = region(
                Content{[source = state.icon_source, spinning = state.icon_spinning, slot = props.feedback_icon_] {
                    if (slot) {
                        SlotContentAccess::function (*slot)();
                    } else {
                        Icon(IconProps{}.source(source).spin(spinning));
                    }
                }},
                state.feedback_color, true);
            services.layout().set_layout(services.components().root(state.regions.icon), layout::BoxLayout{});
        }});
    if (!services_->components().state<FormItemState>(id)) {
        return;
    }
    if (props.geometry_) {
        state.geometry = props.geometry_();
    }
    if (props.control_) {
        auto callbacks = props.control_();
        if (!callbacks.first) {
            throw std::invalid_argument("FormItem control ref must be bound by its content slot");
        }
        state.focus = std::move(callbacks.first);
        state.reset = std::move(callbacks.second);
    } else if (!state.focus) {
        state.focus = [callback = props.focus_, control = state.regions.control](const FormFocusContext& context) {
            return callback && context.visible(context.window, control) && callback();
        };
    }
    items_.push_back(id);
    auto& scope = build.scope(id);
    connect_prop(scope, Prop<FormPresentation>{bind(resolve_presentation)}, [this, id, field](FormPresentation value) {
        if (auto* item = services_->components().state<FormItemState>(id)) {
            item->presentation = value;
            field->presentation.set(value);
            refresh(id, true);
        }
    });
    const auto flag = [this, id, &scope](const Prop<bool>& prop, bool FormItemState::* member) {
        connect_prop(scope, prop, [this, id, member](bool value) {
            if (auto* item = services_->components().state<FormItemState>(id)) {
                item->*member = value;
                refresh(id, true);
            }
        });
    };
    connect_prop(scope, Prop<float>{services_->responsive_layout().window_width()},
                 [this, id](float) { refresh(id, true); });
    flag(props.no_style_, &FormItemState::no_style);
    flag(props.hidden_, &FormItemState::hidden);
    flag(props.has_feedback_, &FormItemState::has_feedback);
    if (props.required_) {
        connect_prop(scope, *props.required_, [this, id](bool value) {
            if (auto* item = services_->components().state<FormItemState>(id)) {
                item->required_override = value;
                refresh(id, true);
            }
        });
    }
    if (!props.status_ && containing_field) {
        connect_prop(scope,
                     Prop<std::optional<FormValidateStatus>>{bind(
                         [containing_field, field, no_style = props.no_style_]() -> std::optional<FormValidateStatus> {
                             if (!read_prop(no_style)) {
                                 return {};
                             }
                             if (!field->meta.get().errors.empty()) {
                                 return FormValidateStatus::Error;
                             }
                             const auto explicit_status = containing_field->status_override.get();
                             if (explicit_status) {
                                 return explicit_status;
                             }
                             const auto meta = containing_field->meta.get();
                             return !containing_field->error.get().empty() || !meta.errors.empty()
                                        ? FormValidateStatus::Error
                                    : !meta.warnings.empty() ? FormValidateStatus::Warning
                                                             : FormValidateStatus::None;
                         })},
                     [this, id, field](std::optional<FormValidateStatus> status) {
                         field->status_override.set(status);
                         if (auto* item = services_->components().state<FormItemState>(id)) {
                             item->explicit_status = status;
                             refresh(id, true);
                         }
                     });
    }
    if (props.status_) {
        connect_prop(scope, *props.status_, [this, id, field](FormValidateStatus value) {
            if (auto* item = services_->components().state<FormItemState>(id)) {
                item->explicit_status = value;
                field->status_override.set(value);
                refresh(id, false);
            }
        });
    }
    connect_prop(scope, props.extra_, [this, id](String value) {
        if (auto* item = services_->components().state<FormItemState>(id)) {
            item->extra.set(std::move(value));
            refresh(id, true);
        }
    });
    connect_prop(scope, Prop<FormFieldMeta>{field->meta}, [this, id](FormFieldMeta) { refresh(id, true); });
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
                             owner->value_changed(field, &previous);
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
    state.subscriptions[2] = theme->capture([this, id](theme_runtime::DirtyPhase) { refresh(id, false); },
                                            [theme] { static_cast<void>(theme->map()); });
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
    const auto meta = state->field->meta.get();
    auto status = state->explicit_status.value_or(
        meta.validating          ? FormValidateStatus::Validating
        : !error.empty()         ? FormValidateStatus::Error
        : !meta.warnings.empty() ? FormValidateStatus::Warning
        : !meta.out_of_date && (meta.touched || (state->has_feedback && state->field->validated))
            ? FormValidateStatus::Success
            : FormValidateStatus::None);
    const auto field = state->field;
    const auto owner = field->owner.lock();
    field->display_status.set(status);
    if (owner && !owner->alive) {
        return;
    }
    state = services_->components().state<FormItemState>(id);
    if (!state || !services_->components().scope(id).active()) {
        return;
    }
    const auto feedback = !error.empty()                             ? error
                          : !state->help.get().empty()               ? state->help.get()
                          : !meta.warnings.empty()                   ? meta.warnings.front()
                          : status == FormValidateStatus::Validating ? state->presentation.locale.validating
                                                                     : String{};
    state->feedback.set(state->custom_help ? error : feedback);
    state->colon.set(state->custom_label && state->presentation.layout == FormLayout::Horizontal &&
                             state->presentation.colon
                         ? String{u8":"}
                         : String{});
    const bool required = state->required_override.value_or(
        state->field->required ||
        std::ranges::any_of(state->field->rules, [](const auto& rule) { return rule.required; }));
    state->required_mark.set(state->presentation.required_mark == FormRequiredMark::Required && required ? String{u8"*"}
                             : state->presentation.required_mark == FormRequiredMark::Optional && !required
                                 ? state->presentation.locale.optional
                                 : String{});
    std::string label{state->label.get().bytes()};
    if (!label.empty() && state->presentation.layout == FormLayout::Horizontal && state->presentation.colon) {
        label += ":";
    }
    state->icon_source.set(IconSource{status == FormValidateStatus::Error        ? IconName::CloseCircleFilled
                                      : status == FormValidateStatus::Warning    ? IconName::ExclamationCircleFilled
                                      : status == FormValidateStatus::Validating ? IconName::LoadingOutlined
                                                                                 : IconName::CheckCircleFilled});
    state->icon_spinning.set(state->has_feedback && !state->no_style && status == FormValidateStatus::Validating);
    state->displayed_label.set(String::from_utf8(label).value());
    if (state->hidden != state->applied_hidden) {
        state->applied_hidden = state->hidden;
        if (services_->components().set_branch_active(id, !state->hidden)) {
            services_->mark_scene_structure_dirty();
        }
        services_->nodes().require(state->node).out_of_flow = state->hidden;
    }
    state->label_color.set(foreground(token.colors.label));
    state->required_color.set(foreground(token.colors.required));
    const auto& map = services_->components().theme_scope(id)->snapshot().map();
    state->feedback_color.set(foreground(status == FormValidateStatus::Error     ? token.colors.error
                                         : status == FormValidateStatus::Warning ? map.color_warning
                                         : status == FormValidateStatus::Success ? map.color_success
                                                                                 : token.colors.help));
    state->font.set({metrics.font_family, 400, false, metrics.font_size, metrics.line_height});
    const bool label_shown =
        !state->no_style && (state->custom_label || !state->label.get().empty()) &&
        (state->presentation.layout != FormLayout::Horizontal ||
         state->presentation.label_col.resolve(services_->responsive_layout().window_width().get()).span != 0);
    for (const auto& [component, shown] :
         {std::pair{state->regions.label, label_shown},
          std::pair{state->regions.feedback, !state->no_style && (state->custom_help || !feedback.empty())},
          std::pair{state->regions.extra, !state->no_style && (state->custom_extra || !state->extra.get().empty())},
          std::pair{state->regions.icon,
                    !state->no_style && state->has_feedback && status != FormValidateStatus::None}}) {
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
    if (state->custom_help) {
        for (const auto& [component, shown] :
             {std::pair{state->feedback_text, !error.empty()}, std::pair{state->help_content, error.empty()}}) {
            const auto node = services_->components().root(component);
            if (services_->components().set_branch_active(component, shown)) {
                services_->mark_scene_structure_dirty();
                geometry = true;
            }
            services_->nodes().require(node).out_of_flow = !shown;
            if (!shown) {
                services_->nodes().require(node).bounds = {};
            }
        }
    }
    if (geometry) {
        services_->layout().set_layout(
            services_->components().root(state->regions.label),
            layout::FlexLayout{layout::FlexDirection::horizontal,
                               metrics.required_gap,
                               {},
                               state->presentation.layout != FormLayout::Inline,
                               false,
                               state->presentation.label_wrap ? layout::FlexWrap::wrap : layout::FlexWrap::no_wrap,
                               state->presentation.label_align == FormLabelAlign::Right ? layout::FlexJustify::end
                                                                                        : layout::FlexJustify::start,
                               layout::FlexAlign::start,
                               0,
                               layout::FlexItemPolicy::sequential,
                               state->presentation.direction == TextDirection::RightToLeft});
        services_->layout().set_layout(services_->components().root(state->regions.control),
                                       layout::BoxLayout{{}, state->presentation.layout != FormLayout::Inline});
        services_->layout().set_layout(
            state->node,
            layout::ComponentLayout{
                [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints constraints) {
                    auto* item = services_->components().state<FormItemState>(id);
                    if (!item || item->hidden) {
                        return runtime::Size{};
                    }
                    const auto& token = services_->components().theme_scope(id)->snapshot().form();
                    const auto& metrics = token.metrics;
                    const auto presentation = item->presentation;
                    const auto width = std::isfinite(constraints.max_width) ? constraints.max_width : 400.0F;
                    const bool label =
                        !services_->nodes().require(services_->components().root(item->regions.label)).out_of_flow;
                    const bool horizontal = presentation.layout != FormLayout::Vertical && !item->no_style;
                    const auto label_col =
                        presentation.label_col.resolve(services_->responsive_layout().window_width().get());
                    const auto wrapper_col =
                        presentation.wrapper_col.resolve(services_->responsive_layout().window_width().get());
                    item->stacked_columns =
                        presentation.layout == FormLayout::Horizontal && label &&
                        label_col.span + label_col.offset + wrapper_col.span + wrapper_col.offset > 24;
                    item->label_width = horizontal && label ? width * float(label_col.span) / 24 : width;
                    const bool columns = presentation.layout == FormLayout::Horizontal && !item->no_style;
                    item->wrapper_x =
                        columns ? width *
                                      float((label && !item->stacked_columns ? label_col.span + label_col.offset : 0) +
                                            wrapper_col.offset) /
                                      24
                                : 0;
                    item->wrapper_width =
                        columns ? std::min(width - item->wrapper_x, width * float(wrapper_col.span) / 24) : width;
                    item->wrapper_width = std::max(0.0F, item->wrapper_width);
                    const auto label_node = services_->components().root(item->regions.label);
                    item->label_height =
                        label
                            ? engine.measure_child(label_node, {0, item->label_width, 0, constraints.max_height}).height
                            : 0;
                    if (presentation.layout == FormLayout::Inline) {
                        const auto label_size =
                            label ? engine.measure_child(label_node, {0, width, 0, constraints.max_height})
                                  : runtime::Size{};
                        item->label_width = label_size.width;
                        item->label_height = label_size.height;
                        item->wrapper_x = label ? item->label_width + metrics.label_gap : 0;
                        float preferred = metrics.font_size * 12;
                        for (const auto child :
                             services_->nodes().require(services_->components().root(item->regions.control)).children) {
                            if (const auto explicit_width = services_->nodes().require(child).external_layout.width) {
                                preferred = std::max(preferred, *explicit_width);
                            }
                        }
                        item->wrapper_width = std::min(std::max(0.0F, width - item->wrapper_x), preferred);
                    }
                    if (!presentation.label_wrap && label) {
                        item->label_height = std::min(item->label_height, metrics.line_height);
                    }
                    const auto icon_node = services_->components().root(item->regions.icon);
                    const auto icon =
                        !services_->nodes().require(icon_node).out_of_flow
                            ? engine.measure_child(icon_node, {0, std::min(item->wrapper_width, metrics.line_height), 0,
                                                               constraints.max_height})
                            : runtime::Size{};
                    item->icon_width = icon.width;
                    item->icon_height = icon.height;
                    const auto suffix = icon.width > 0 ? icon.width + metrics.required_gap : 0;
                    const auto control = engine.measure_child(
                        services_->components().root(item->regions.control),
                        {0, std::max(0.0F, item->wrapper_width - suffix), 0, constraints.max_height});
                    item->control_width = control.width;
                    item->control_row_height = std::max(control.height, icon.height);
                    if (presentation.layout == FormLayout::Inline) {
                        item->wrapper_width = control.width + suffix;
                    }
                    item->control_height = control.height;
                    item->feedback_height =
                        !services_->nodes().require(services_->components().root(item->regions.feedback)).out_of_flow
                            ? engine
                                  .measure_child(services_->components().root(item->regions.feedback),
                                                 {0, item->wrapper_width, 0, constraints.max_height})
                                  .height
                            : 0;
                    item->extra_height =
                        !services_->nodes().require(services_->components().root(item->regions.extra)).out_of_flow
                            ? engine
                                  .measure_child(services_->components().root(item->regions.extra),
                                                 {0, item->wrapper_width, 0, constraints.max_height})
                                  .height
                            : 0;
                    const auto gap = label && (!horizontal || item->stacked_columns) ? metrics.label_gap : 0;
                    const auto top = horizontal && !item->stacked_columns
                                         ? std::max(item->label_height, item->control_row_height)
                                         : item->label_height + gap + item->control_row_height;
                    const auto used_width =
                        presentation.layout == FormLayout::Inline ? item->wrapper_x + item->wrapper_width : width;
                    return constraints.constrain({used_width, top + item->feedback_height + item->extra_height +
                                                                  (item->no_style ? 0 : metrics.item_margin_bottom)});
                },
                [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                    const auto* item = services_->components().state<FormItemState>(id);
                    if (!item || item->hidden) {
                        return;
                    }
                    const auto& metrics = services_->components().theme_scope(id)->snapshot().form().metrics;
                    const bool horizontal =
                        item->presentation.layout != FormLayout::Vertical && !item->no_style && !item->stacked_columns;
                    const bool label =
                        !services_->nodes().require(services_->components().root(item->regions.label)).out_of_flow;
                    const auto label_col =
                        item->presentation.label_col.resolve(services_->responsive_layout().window_width().get());
                    const auto label_x = item->presentation.layout == FormLayout::Horizontal && label
                                             ? bounds.width * float(label_col.offset) / 24
                                             : 0;
                    const auto control_y =
                        bounds.y + (horizontal ? 0 : item->label_height + (label ? metrics.label_gap : 0));
                    const bool rtl = item->presentation.direction == TextDirection::RightToLeft;
                    const bool mirror = rtl && item->presentation.layout != FormLayout::Vertical && !item->no_style;
                    const auto label_left = mirror ? bounds.width - label_x - item->label_width : label_x;
                    const auto control_left =
                        rtl && horizontal ? bounds.width - item->wrapper_x - item->wrapper_width : item->wrapper_x;
                    if (label) {
                        engine.place_child(services_->components().root(item->regions.label),
                                           {bounds.x + label_left, bounds.y, item->label_width, item->label_height},
                                           true);
                    }
                    const auto suffix = item->icon_width > 0 ? item->icon_width + metrics.required_gap : 0;
                    const auto actual_control_left = control_left + (rtl ? suffix : 0);
                    engine.place_child(
                        services_->components().root(item->regions.control),
                        {bounds.x + actual_control_left, control_y, item->control_width, item->control_height}, true);
                    if (item->icon_width > 0) {
                        engine.place_child(
                            services_->components().root(item->regions.icon),
                            {bounds.x + control_left + (rtl ? 0 : item->control_width + metrics.required_gap),
                             control_y + (item->control_row_height - item->icon_height) / 2, item->icon_width,
                             item->icon_height});
                    }
                    auto feedback_y = horizontal ? bounds.y + std::max(item->label_height, item->control_row_height)
                                                 : control_y + item->control_row_height;
                    if (item->feedback_height > 0) {
                        engine.place_child(
                            services_->components().root(item->regions.feedback),
                            {bounds.x + control_left, feedback_y, item->wrapper_width, item->feedback_height}, true);
                    }
                    if (item->extra_height > 0) {
                        engine.place_child(services_->components().root(item->regions.extra),
                                           {bounds.x + control_left, feedback_y + item->feedback_height,
                                            item->wrapper_width, item->extra_height},
                                           true);
                    }
                }});
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
    lifetime_->viewport = viewport;
    lifetime_->clip = clip;
    viewport_ = viewport;
    clip_ = clip;
    const auto forms = forms_;
    for (const auto id : forms) {
        if (auto* form = services_->components().state<FormState>(id)) {
            auto focus = std::move(form->pending_scroll_focus);
            form->pending_scroll_focus = {};
            if (focus) {
                focus();
            }
        }
    }
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

namespace ryn::detail {
void FormComponentHost::mount_list(const FormListProps& props, const FormListItem& render) {
    auto& build = runtime::require_component_build_context();
    auto* form = build.nearest_state<FormState>();
    if (!form || props.name_.empty() || !render) {
        throw std::invalid_argument("FormList needs a Form, path and item slot");
    }
    const auto model = form->model;
    FormData{props.initial_}.validate();
    if (props.reference_ && !props.reference_->list.expired()) {
        throw std::invalid_argument("FormListRef is already bound");
    }
    const auto id = build.mount_component<std::shared_ptr<FormListState>>(std::make_shared<FormListState>());
    const auto list = build.state<std::shared_ptr<FormListState>>(id);
    list->id = id;
    list->model = model;
    list->render = render;
    auto* parent = build.nearest_state<FormListRowContext>();
    list->prefix = [path = props.name_] {
        return path;
    };
    if (parent && parent->list->model.lock() == model) {
        list->prefix = [parent_list = parent->list, row = parent->row, path = props.name_] {
            return parent_list->prefix().append(FormNamePath{row->index.get()}).append(path);
        };
    }
    for (const auto& collection : model->collections) {
        if (collection.active() && collection.path() == list->prefix()) {
            throw std::invalid_argument("FormList paths must be unique");
        }
    }
    for (const auto& item : model->fields()) {
        if (list->prefix().startsWith(item.path)) {
            throw std::invalid_argument("FormList path conflicts with a leaf field");
        }
    }
    for (const auto& callback : props.rules_) {
        FormRule rule;
        rule.validator = [weak = std::weak_ptr{list}, callback](const FormValue&,
                                                                const FormValidationContext& context) {
            const auto list = weak.lock();
            if (!list || !list->alive) {
                return std::vector<FormIssue>{};
            }
            const auto* node = context.values.at(list->prefix());
            const auto* array = node ? std::get_if<FormData::Array>(&node->value) : nullptr;
            return callback(array ? *array : FormData::Array{}, context);
        };
        list->validation->rules.push_back(std::move(rule));
    }
    if (props.async_) {
        list->validation->async = [weak = std::weak_ptr{list}, factory = props.async_](FormValue,
                                                                                       FormValidationContext context) {
            const auto list = weak.lock();
            const auto* node = list ? context.values.at(list->prefix()) : nullptr;
            const auto* array = node ? std::get_if<FormData::Array>(&node->value) : nullptr;
            return factory(array ? *array : FormData::Array{}, std::move(context));
        };
    }
    model->collections.push_back({list->prefix,
                                  [weak = std::weak_ptr{list}] {
                                      const auto list = weak.lock();
                                      return list && list->alive ? list->rows.size() : 0;
                                  },
                                  [this, weak = std::weak_ptr{list}](FormData::Array values) {
                                      const auto list = weak.lock();
                                      if (!list || !list->alive) {
                                          return false;
                                      }
                                      auto rows = list->rows;
                                      if (rows.size() > values.size()) {
                                          rows.resize(values.size());
                                      }
                                      while (rows.size() < values.size()) {
                                          auto row = std::make_shared<FormListRow>();
                                          row->key = ++list->next_key;
                                          row->index.set(rows.size());
                                          row->initial = values[rows.size()];
                                          rows.push_back(row);
                                      }
                                      return update_list(list, std::move(rows));
                                  },
                                  [weak = std::weak_ptr{list}] {
                                      const auto list = weak.lock();
                                      return list && list->alive;
                                  },
                                  {},
                                  list->validation});
    auto initial = props.initial_;
    if (const auto* value = model->initial_values.at(list->prefix())) {
        if (const auto* array = std::get_if<FormData::Array>(&value->value)) {
            initial = *array;
        } else {
            throw std::invalid_argument("FormList initialValues expected an array");
        }
    }
    if (parent && parent->list->model.lock() == model) {
        if (const auto* value = parent->row->initial.at(props.name_)) {
            if (const auto* array = std::get_if<FormData::Array>(&value->value)) {
                initial = *array;
            }
        }
    }
    auto stored = model->stored_values.get();
    if (const auto* value = stored.at(list->prefix()); value && !std::holds_alternative<std::monostate>(value->value)) {
        const auto* array = std::get_if<FormData::Array>(&value->value);
        if (!array) {
            throw std::invalid_argument("Stored FormList value expected an array");
        }
        initial = *array;
    } else {
        stored.set(list->prefix(), FormData{initial});
        model->stored_values.set(std::move(stored));
    }
    for (std::size_t i = 0; i < initial.size(); ++i) {
        auto row = std::make_shared<FormListRow>();
        row->key = ++list->next_key;
        row->index.set(i);
        row->initial = initial[i];
        list->rows.push_back(row);
    }
    auto& services = *services_;
    const auto node = build.root(id);
    services.layout().set_layout(node, layout::FlexLayout{layout::FlexDirection::vertical, 0, {}, true});
    runtime::connect_layout_style(build.scope(id), props.layout_, node, services.nodes(), services.dirty());
    build.on_resource_cleanup(id, [list, &services, node] {
        list->alive = false;
        list->validation->due.reset();
        list->validation->timeout.reset();
        auto cancel = list->validation->session->detach_cancel();
        list->render = {};
        services.layout().remove_layout(node);
        if (const auto model = list->model.lock()) {
            try {
                model->cancel_validation();
            } catch (...) {
            }
        }
        if (cancel) {
            try {
                cancel();
            } catch (...) {
            }
        }
        try {
            list->validation->meta.set({});
        } catch (...) {
        }
    });
    if (props.reference_) {
        props.reference_->list = list;
        props.reference_->update = [this, weak = std::weak_ptr{list}](auto rows) {
            const auto list = weak.lock();
            return list && update_list(list, std::move(rows));
        };
    }
    const auto initial_rows = list->rows;
    const auto reset_list = [this, weak = std::weak_ptr{list}, initial_rows] {
        const auto list = weak.lock();
        if (!list || !list->alive) {
            return;
        }
        auto rows = initial_rows;
        for (auto& row : rows) {
            if (!services_->components().contains(row->component)) {
                auto replacement = std::make_shared<FormListRow>();
                replacement->key = ++list->next_key;
                replacement->initial = row->initial;
                row = replacement;
            }
        }
        list->validation->meta.set({});
        static_cast<void>(update_list(list, std::move(rows)));
        if (const auto model = list->model.lock(); model && model->alive && list->alive) {
            FormData::Array defaults;
            for (const auto& row : initial_rows) {
                defaults.push_back(row->initial);
            }
            auto stored = model->stored_values.get();
            stored.set(list->prefix(), FormData{std::move(defaults)});
            model->stored_values.set(std::move(stored));
        }
    };
    model->reset_hooks.push_back(reset_list);
    model->collections.back().reset = reset_list;
    for (const auto& row : list->rows) {
        mount_list_row(list, row, true);
    }
}

void FormComponentHost::mount_list_row(const std::shared_ptr<FormListState>& list,
                                       const std::shared_ptr<FormListRow>& row, bool initial) {
    const Content content{[this, list, row] {
        auto& build = runtime::require_component_build_context();
        const auto id = build.mount_component<FormListRowContext>(FormListRowContext{list, row});
        row->component = id;
        const auto node = build.root(id);
        auto& services = *services_;
        services.nodes().require(node).external_layout.order = static_cast<int>(row->index.get());
        services.layout().set_layout(node, layout::FlexLayout{layout::FlexDirection::vertical, 0, {}, true});
        build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
        FormListField field;
        field.list_ = list;
        field.row_ = row;
        const auto render = list->render;
        build.mount_slot(id, Content{[render, field] { render(field); }});
    }};
    if (initial) {
        runtime::require_component_build_context().mount_slot(list->id, content);
    } else {
        services_->append_slot(list->id, content);
    }
}

bool FormComponentHost::update_list(const std::shared_ptr<FormListState>& list,
                                    std::vector<std::shared_ptr<FormListRow>> rows) {
    const auto model = list->model.lock();
    if (!list->alive || list->busy || !model || !model->alive) {
        return false;
    }
    const auto old = list->rows;
    std::vector<std::pair<FormNamePath, FormNamePath>> moves;
    const auto prefix = list->prefix();
    // Removed rows are first moved to disjoint temporary indices, so every change is preflighted atomically.
    for (std::size_t i = 0; i < old.size(); ++i) {
        const auto it = std::ranges::find(rows, old[i]);
        const auto next = it == rows.end() ? rows.size() + old.size() + i : std::size_t(it - rows.begin());
        moves.emplace_back(prefix.append(FormNamePath{i}), prefix.append(FormNamePath{next}));
    }
    list->busy = true;
    try {
        batch([&] {
            model->relocate(moves);
            batch([&] {
                for (std::size_t i = 0; i < rows.size(); ++i) {
                    rows[i]->index.set(i);
                }
            });
            list->rows = rows;
            for (const auto& row : old) {
                if (std::ranges::find(rows, row) == rows.end()) {
                    services_->destroy(row->component);
                }
            }
            for (std::size_t i = 0; i < old.size(); ++i) {
                if (std::ranges::find(rows, old[i]) == rows.end()) {
                    model->forget(prefix.append(FormNamePath{rows.size() + old.size() + i}));
                }
            }
            for (const auto& row : rows) {
                if (!list->alive) {
                    break;
                }
                if (!row->component.valid()) {
                    auto stored = model->stored_values.get();
                    const auto path = prefix.append(FormNamePath{row->index.get()});
                    if (const auto* prior = stored.at(path);
                        !prior || std::holds_alternative<std::monostate>(prior->value)) {
                        stored.set(path, row->initial);
                        model->stored_values.set(std::move(stored));
                    }
                    mount_list_row(list, row, false);
                }
                if (services_->components().contains(row->component)) {
                    services_->nodes().require(services_->components().root(row->component)).external_layout.order =
                        static_cast<int>(row->index.get());
                }
            }
            if (list->alive) {
                const auto node = services_->components().root(list->id);
                services_->dirty().invalidate(node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                        runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
                services_->mark_scene_structure_dirty();
            }
            model->topology.set(model->topology.get() + 1);
        });
        list->busy = false;
    } catch (...) {
        list->busy = false;
        // User renderers can mutate external state. Retire this complete list on failure.
        if (list->alive) {
            services_->destroy(list->id);
        }
        throw;
    }
    return list->alive;
}
} // namespace ryn::detail

namespace ryn {
std::uint64_t FormListField::key() const {
    return row_ ? row_->key : 0;
}

std::size_t FormListField::index() const {
    return row_ ? row_->index.get() : 0;
}

Prop<std::size_t> FormListField::position() const {
    return bind([field = *this] { return field.index(); });
}

FormNamePath FormListField::path() const {
    return list_ && row_ ? list_->prefix().append(FormNamePath{index()}) : FormNamePath{};
}

FormFieldMeta FormListRef::meta() const {
    const auto list = state_->list.lock();
    return list && list->alive ? list->validation->meta.get() : FormFieldMeta{};
}

FormListRef::FormListRef() : state_(std::make_shared<detail::FormListRefState>()) {}

bool FormListRef::bound() const {
    const auto list = state_->list.lock();
    return list && list->alive;
}

std::vector<FormListField> FormListRef::fields() const {
    std::vector<FormListField> result;
    const auto list = state_->list.lock();
    if (list && list->alive) {
        for (const auto& row : list->rows) {
            FormListField field;
            field.list_ = list;
            field.row_ = row;
            result.push_back(field);
        }
    }
    return result;
}

bool FormListRef::add(FormData value, std::optional<std::size_t> index) const {
    const auto list = state_->list.lock();
    if (!list || !list->alive || list->busy || index.value_or(list->rows.size()) > list->rows.size()) {
        return false;
    }
    value.validate();
    auto rows = list->rows;
    auto row = std::make_shared<detail::FormListRow>();
    row->key = ++list->next_key;
    row->initial = std::move(value);
    const auto position = index.value_or(rows.size());
    row->index.set(position);
    rows.insert(rows.begin() + position, row);
    return state_->update && state_->update(std::move(rows));
}

bool FormListRef::remove(std::vector<std::size_t> indices) const {
    const auto list = state_->list.lock();
    if (!list || !list->alive || list->busy || indices.empty()) {
        return false;
    }
    std::sort(indices.begin(), indices.end());
    if (indices.back() >= list->rows.size() || std::adjacent_find(indices.begin(), indices.end()) != indices.end()) {
        return false;
    }
    auto rows = list->rows;
    for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
        rows.erase(rows.begin() + *it);
    }
    return state_->update && state_->update(std::move(rows));
}

bool FormListRef::move(std::size_t from, std::size_t to) const {
    const auto list = state_->list.lock();
    if (!list || !list->alive || list->busy || from >= list->rows.size() || to >= list->rows.size()) {
        return false;
    }
    if (from == to) {
        return true;
    }
    auto rows = list->rows;
    const auto row = rows[from];
    rows.erase(rows.begin() + from);
    rows.insert(rows.begin() + to, row);
    return state_->update && state_->update(std::move(rows));
}

void FormList(FormListProps props, FormListItem item) {
    if (!detail::active_forms) {
        throw std::logic_error("FormList requires window component services");
    }
    detail::active_forms->mount_list(props, item);
}
} // namespace ryn

namespace ryn::detail {
void FormComponentHost::on_window_active(bool) {
    on_input_boundary_changed();
}

void FormComponentHost::on_input_boundary_changed() {
    const auto forms = forms_;
    for (const auto id : forms) {
        if (const auto* form = services_->components().state<FormState>(id)) {
            const auto model = form->model;
            model->boundary_changed();
        }
    }
}

std::size_t FormComponentHost::tick_auxiliary(animation::AnimationTime time) {
    std::size_t count{};
    const auto forms = forms_;
    for (const auto id : forms) {
        if (const auto* form = services_->components().state<FormState>(id)) {
            const auto model = form->model;
            count += model->tick(time);
        }
    }
    return count;
}

std::optional<animation::AnimationTime> FormComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> result;
    for (const auto id : forms_) {
        if (const auto* form = services_->components().state<FormState>(id)) {
            const auto deadline = form->model->deadline();
            if (deadline && (!result || *deadline < *result)) {
                result = deadline;
            }
        }
    }
    return result;
}
} // namespace ryn::detail

namespace ryn::detail {
std::shared_ptr<FormFieldState> active_form_control(std::size_t type) {
    auto& build = runtime::require_component_build_context();
    auto* item = build.nearest_state<FormItemState>();
    if (!item || !item->field->registered || item->control_claimed || item->field->initial.index() != type ||
        !build.has_ancestor(item->regions.control) ||
        (private_editor_host == &build.host() && private_editor_item == item->id)) {
        throw std::invalid_argument(
            "FormControl needs one compatible registered FormItem field and an unclaimed control slot");
    }
    if (item->binding_epoch == std::numeric_limits<std::uint64_t>::max()) {
        throw std::length_error("Form native connection epoch exhausted");
    }
    auto connection = std::make_shared<FormControlConnection>();
    connection->lifetime = item->lifetime;
    connection->model = item->field->owner;
    connection->field = item->field;
    connection->form = item->form;
    connection->item = item->id;
    connection->region = item->regions.control;
    connection->registration_generation = item->registration_generation;
    connection->epoch = ++item->binding_epoch;
    item->connection = connection;
    item->field->connection = connection;
    item->control_claimed = true;
    return item->field;
}

std::shared_ptr<FormControlConnection> capture_form_control_connection(const std::shared_ptr<FormFieldState>& field) {
    const auto connection = field ? field->connection.lock() : nullptr;
    if (!connection_current(connection) || connection->field.lock() != field) {
        throw std::invalid_argument("Form binding reservation was superseded");
    }
    return connection;
}

bool form_control_event_admitted(const std::shared_ptr<FormFieldState>& field,
                                 const std::shared_ptr<FormControlConnection>& connection) {
    if (!connection) {
        return !field->direct_eligible || field->direct_eligible();
    }
    if (connection->field.lock() != field || !connection_current(connection)) {
        return false;
    }
    if (connection->connected && (!connection->native.current || !connection->native.current())) {
        return false;
    }
    return item_receives(connection->lifetime.lock(), connection->item) || native_admitted(connection);
}

FormControlMount::FormControlMount(std::shared_ptr<FormControlConnection> connection, std::function<void()> attach)
    : connection_(std::move(connection)), attach_(std::move(attach)) {}

FormControlMount::~FormControlMount() {
    if (!complete_ && connection_ && !connection_->connected) {
        retire_connection(connection_);
    }
}

FormControlMount::operator bool() const noexcept {
    return bool(attach_);
}

void FormControlMount::operator()() const {
    attach_();
    complete_ = true;
}

namespace {
void connect_native_control(const std::shared_ptr<FormFieldState>& field,
                            const std::shared_ptr<FormControlConnection>& connection, FormNativeControl native) {
    auto& build = runtime::require_component_build_context();
    const auto life = connection ? connection->lifetime.lock() : nullptr;
    if (!connection_current(connection) || connection->field.lock() != field || !current_lifetime(life) ||
        &build.host() != &life->window->components() || native.window != life->window || !native.component.valid() ||
        !native.current || !native.current() || !life->window->components().scope(native.component).active() ||
        !descendant(*life->window, native.component, connection->region)) {
        throw std::invalid_argument("Form native receiver must match its exact registration and window");
    }
    for (auto id = std::optional{native.component}; id; id = life->window->components().parent(*id)) {
        if (life->window->components().state<FormItemState>(*id)) {
            if (*id != connection->item) {
                throw std::invalid_argument("Form native receiver crossed a nested Item");
            }
            break;
        }
    }
    if (connection->connected) {
        if (connection->native.component != native.component ||
            connection->native.ref_generation != native.ref_generation) {
            throw std::invalid_argument("A Form binding has one exact native connection");
        }
        return;
    }
    const auto cleanup = [weak = std::weak_ptr{connection}] {
        retire_connection(weak.lock());
    };
    if (!native.connect_cleanup || !native.connect_cleanup(cleanup)) {
        throw std::invalid_argument("Form native ref already has a connection");
    }
    build.scope(native.component).on_cleanup(cleanup);
    connection->native = std::move(native);
    connection->connected = true;
    auto* item = registered_item(life, connection->item);
    item->focus = connection->native.focus;
    if (connection->native.reset) {
        item->reset = connection->native.reset;
    }
    item->geometry = connection->native.geometry;
    field->due.reset();
    field->timeout.reset();
    if (const auto model = field->owner.lock()) {
        model->cancel_validation();
    }
    if (!connection_current(connection)) {
        return;
    }
    item = registered_item(life, connection->item);
    if (connection->native.enter_capable) {
        item->native_enter_claimed = true;
        item->native_enter_claimant = connection->native.component;
    }
}
} // namespace

void attach_form_control(const std::shared_ptr<FormFieldState>& field,
                         const std::shared_ptr<FormControlConnection>& connection, const InputRef& ref) {
    connect_native_control(field, connection, capture_form_native_input(ref));
}

void attach_form_control(const std::shared_ptr<FormFieldState>& field,
                         const std::shared_ptr<FormControlConnection>& connection, const CheckboxRef& ref) {
    connect_native_control(field, connection, capture_form_native_checkbox(ref));
}

void attach_form_control(const std::shared_ptr<FormFieldState>& field,
                         const std::shared_ptr<FormControlConnection>& connection, const SwitchRef& ref) {
    connect_native_control(field, connection, capture_form_native_switch(ref));
}

void attach_form_control(const std::shared_ptr<FormFieldState>& field, const InputRef& ref) {
    auto* item = runtime::require_component_build_context().nearest_state<FormItemState>();
    if (!item || item->field != field) {
        throw std::invalid_argument("FormControl target must belong to its field slot");
    }
    auto callbacks = capture_form_input(ref);
    item->focus = std::move(callbacks.first);
    item->reset = std::move(callbacks.second);
    item->geometry = capture_form_input_geometry(ref);
}

void attach_form_control(const std::shared_ptr<FormFieldState>& field, const CheckboxRef& ref) {
    auto* item = runtime::require_component_build_context().nearest_state<FormItemState>();
    if (!item || item->field != field) {
        throw std::invalid_argument("FormControl target must belong to its field slot");
    }
    item->focus = capture_form_checkbox(ref);
    item->geometry = capture_form_checkbox_geometry(ref);
}

void attach_form_control(const std::shared_ptr<FormFieldState>& field, const SwitchRef& ref) {
    auto* item = runtime::require_component_build_context().nearest_state<FormItemState>();
    if (!item || item->field != field) {
        throw std::invalid_argument("FormControl target must belong to its field slot");
    }
    auto target = capture_form_switch(ref);
    item->focus = std::move(target.first);
    item->geometry = std::move(target.second);
}

void attach_form_control(const std::shared_ptr<FormFieldState>& field, FormControlTarget target) {
    auto* item = runtime::require_component_build_context().nearest_state<FormItemState>();
    if (!item || item->field != field) {
        throw std::invalid_argument("FormControl target must belong to its field slot");
    }
    item->focus = [focus = std::move(target.focus), id = item->id](const FormFocusContext& context) {
        const auto* item = context.window.components().state<FormItemState>(id);
        return focus && item && context.visible(context.window, item->regions.control) && focus();
    };
    item->reset = std::move(target.reset);
    item->geometry = [geometry = std::move(target.geometry)](const FormFocusContext&) {
        return geometry ? geometry() : std::nullopt;
    };
}

FormPresentation active_form_presentation() {
    const auto* item = runtime::require_component_build_context().nearest_state<FormItemState>();
    return item ? item->presentation : FormPresentation{};
}

Binding<FormPresentation> form_control_presentation(const std::shared_ptr<FormFieldState>& field) {
    return bind([field] { return field->presentation.get(); });
}

FormPrivateEditorScope::FormPrivateEditorScope() : previous_(private_editor_item), previous_host_(private_editor_host) {
    auto& build = runtime::require_component_build_context();
    private_editor_host = &build.host();
    const auto* item = build.nearest_state<FormItemState>(true);
    private_editor_item = item ? std::optional{item->id} : std::nullopt;
}

FormPrivateEditorScope::~FormPrivateEditorScope() {
    private_editor_item = previous_;
    private_editor_host = previous_host_;
}

std::function<void()> form_enter_handler(runtime::ComponentId caller) {
    auto& build = runtime::require_component_build_context();
    auto* item = build.nearest_state<FormItemState>(true);
    auto* form = build.nearest_state<FormState>();
    if (!item || !form || item->form != form->id ||
        (private_editor_host == &build.host() && private_editor_item == item->id) ||
        !build.has_ancestor(item->regions.control) || (item->native_enter_claimed && !item->connection)) {
        return {};
    }
    if (item->native_enter_claimed && item->connection && item->connection->connected &&
        item->connection->native.component != caller) {
        return {};
    }
    if (!item->native_enter_claimed) {
        item->native_enter_claimed = true;
        item->native_enter_claimant = caller;
    }
    return [weak = item->lifetime, id = item->id, caller, field = std::weak_ptr{item->field},
            generation = item->registration_generation, connection = item->connection] {
        const auto life = weak.lock();
        const auto* item = registered_item(life, id);
        const auto current = field.lock();
        if (!item || !current || item->field != current || current->generation != generation ||
            item->native_enter_claimant != caller || form_field_disabled(current) || form_field_read_only(current) ||
            !form_control_event_admitted(current, connection)) {
            return;
        }
        if (!item_receives(life, id) && (!caller.valid() || !connection || connection->native.component != caller)) {
            return;
        }
        const auto model = current->owner.lock();
        if (model && (!current->eligible || current->eligible())) {
            static_cast<void>(model->submit());
        }
    };
}

std::function<void()> form_enter_handler() {
    return form_enter_handler(runtime::ComponentId{});
}

std::optional<FormControlTarget> FormComponentHost::field_control(runtime::ComponentId form_id, FormNamePath path) {
    const auto* form = services_->components().state<FormState>(form_id);
    if (!form || !form->model->alive) {
        return std::nullopt;
    }
    const auto model = form->model;
    const auto found = std::ranges::find(model->fields(), path, &FormModel::Registration::path);
    if (found == model->fields().end()) {
        return std::nullopt;
    }
    for (const auto id : items_) {
        const auto* item = registered_item(lifetime_, id);
        if (!item || item->form != form_id || item->field != found->field) {
            continue;
        }
        const auto current = [weak = std::weak_ptr{lifetime_}, model = std::weak_ptr{model}, id,
                              generation = found->generation, epoch = item->binding_epoch]() -> FormItemState* {
            const auto life = weak.lock();
            auto* item = registered_item(life, id);
            return item && item->binding_epoch == epoch && item->registration_generation == generation &&
                           item->field->owner.lock() == model.lock()
                       ? item
                       : nullptr;
        };
        return FormControlTarget{
            [weak = std::weak_ptr{lifetime_}, current] {
                const auto life = weak.lock();
                const auto* item = current();
                if (!item || !item_maintainable(life, item->id) || form_field_disabled(item->field) ||
                    form_field_read_only(item->field)) {
                    return false;
                }
                const auto callback = item_receives(life, item->id) ? item->focus : item->connection->native.focus;
                const bool focused = callback && callback(FormFocusContext{*life->window, life->viewport, life->clip});
                item = current();
                return focused && item && item_maintainable(life, item->id);
            },
            [current] {
                if (const auto* item = current()) {
                    const auto callback = item->reset;
                    if (callback) {
                        callback();
                    }
                }
            },
            [weak = std::weak_ptr{lifetime_}, current]() -> std::optional<FormControlGeometry> {
                const auto life = weak.lock();
                const auto* item = current();
                if (!item || !item_maintainable(life, item->id)) {
                    return std::nullopt;
                }
                const auto callback =
                    item_receives(life, item->id) ? item->geometry : item->connection->native.geometry;
                const auto result =
                    callback ? callback(FormFocusContext{*life->window, life->viewport, life->clip}) : std::nullopt;
                item = current();
                return item && item_maintainable(life, item->id) ? result : std::nullopt;
            }};
    }
    return std::nullopt;
}

bool FormComponentHost::scroll_field(runtime::ComponentId form_id, FormNamePath path, FormScrollOptions options) {
    auto* form = services_->components().state<FormState>(form_id);
    if (!form || !form->model->available() || !form->scroll) {
        return false;
    }
    runtime::ComponentId item_id;
    for (const auto id : items_) {
        const auto* item = registered_item(lifetime_, id);
        if (!item || item->form != form_id) {
            continue;
        }
        const auto found = std::ranges::find(form->model->fields(), item->field, &FormModel::Registration::field);
        if (found != form->model->fields().end() && found->path == path) {
            item_id = id;
            break;
        }
    }
    const auto* item = registered_item(lifetime_, item_id);
    if (!item || !item_maintainable(lifetime_, item_id) || form_field_disabled(item->field) ||
        form_field_read_only(item->field)) {
        return false;
    }
    const auto field_generation = item->registration_generation;
    const auto epoch = item->binding_epoch;
    const auto model = form->model;
    const auto revision = model->revision();
    const auto current = [weak = std::weak_ptr{lifetime_}, model = std::weak_ptr{model}, item_id, field_generation,
                          epoch, revision]() -> FormItemState* {
        const auto life = weak.lock();
        const auto owner = model.lock();
        auto* item = registered_item(life, item_id);
        return owner && owner->available() && owner->revision() == revision && item &&
                       item->registration_generation == field_generation && item->binding_epoch == epoch &&
                       item_maintainable(life, item_id) && !form_field_disabled(item->field) &&
                       !form_field_read_only(item->field)
                   ? item
                   : nullptr;
    };
    const auto geometry_callback =
        item_receives(lifetime_, item_id) ? item->geometry : item->connection->native.geometry;
    const auto geometry =
        geometry_callback ? geometry_callback(FormFocusContext{*services_, viewport_, clip_}) : std::nullopt;
    item = current();
    form = services_->components().state<FormState>(form_id);
    if (!form || !item || !geometry || !geometry->eligible || geometry->bounds.width <= 0 ||
        geometry->bounds.height <= 0) {
        return false;
    }
    const auto generation = ++form->scroll_generation;
    const auto callback = form->scroll;
    const auto delivered = std::make_shared<std::atomic_bool>(false);
    callback({path, *geometry}, [weak = std::weak_ptr{lifetime_}, form_id, options, generation, current,
                                 delivered](bool revealed) {
        const auto life = weak.lock();
        if (!current_lifetime(life) || delivered->exchange(true) || !revealed || !current()) {
            return;
        }
        auto* form = life->window->components().state<FormState>(form_id);
        if (!form || form->scroll_generation != generation) {
            return;
        }
        if (options.focus) {
            form->pending_scroll_focus = [weak, form_id, generation, current] {
                const auto life = weak.lock();
                const auto* item = current();
                if (!current_lifetime(life) || !item) {
                    return;
                }
                const auto* form = life->window->components().state<FormState>(form_id);
                if (!form || form->scroll_generation != generation) {
                    return;
                }
                const auto focus = item_receives(life, item->id) ? item->focus : item->connection->native.focus;
                if (focus) {
                    static_cast<void>(focus(FormFocusContext{*life->window, life->viewport, life->clip}));
                }
            };
            life->window->request_frame();
        }
    });
    return model->alive;
}
} // namespace ryn::detail

namespace ryn {
void FormControl(InputProps props) {
    const auto attach = detail::FormAutoControlAccess::bind(props);
    if (!attach) {
        throw std::invalid_argument("FormControl requires an unclaimed typed field without another explicit binding");
    }
    Input(std::move(props));
    attach();
}

void FormControl(TextAreaProps props) {
    const auto attach = detail::FormAutoControlAccess::bind(props);
    if (!attach) {
        throw std::invalid_argument("FormControl requires an unclaimed typed field without another explicit binding");
    }
    TextArea(std::move(props));
    attach();
}

void FormControl(InputNumberProps props) {
    const auto attach = detail::FormAutoControlAccess::bind(props);
    if (!attach) {
        throw std::invalid_argument("FormControl requires an unclaimed typed field without another explicit binding");
    }
    InputNumber(std::move(props));
    attach();
}

void FormControl(SelectProps props) {
    const auto attach = detail::FormAutoControlAccess::bind(props);
    if (!attach) {
        throw std::invalid_argument("FormControl requires an unclaimed typed field without another explicit binding");
    }
    Select(std::move(props));
    attach();
}

void FormControl(CheckboxProps props, std::optional<CheckboxLabel> label) {
    const auto attach = detail::FormAutoControlAccess::bind(props);
    if (!attach) {
        throw std::invalid_argument("FormControl requires an unclaimed typed field without another explicit binding");
    }
    Checkbox(std::move(props), std::move(label));
    attach();
}

void FormControl(SwitchProps props) {
    const auto attach = detail::FormAutoControlAccess::bind(props);
    if (!attach) {
        throw std::invalid_argument("FormControl requires an unclaimed typed field without another explicit binding");
    }
    Switch(std::move(props));
    attach();
}

std::optional<FormControlTarget> FormRef::getFieldInstance(const FormNamePath& path) const {
    const auto model = detail::resolve_form_model(state_);
    return model && model->alive && model->control ? model->control(path) : std::nullopt;
}

std::vector<FormFieldData> FormRef::getFieldsError(const std::vector<FormNamePath>& paths) const {
    auto fields = getFields();
    std::erase_if(fields, [&](const auto& field) {
        return !paths.empty() && std::ranges::find(paths, field.path) == paths.end();
    });
    for (auto& field : fields) {
        field.value.reset();
    }
    return fields;
}

bool FormRef::scrollToField(const FormNamePath& path, FormScrollOptions options) const {
    const auto model = detail::resolve_form_model(state_);
    return model && model->alive && model->scroll && model->scroll(path, options);
}

bool FormRef::scrollToFirstError(FormScrollOptions options) const {
    const auto model = detail::resolve_form_model(state_);
    if (!model || !model->alive || !model->scroll) {
        return false;
    }
    for (const auto& field : model->fields()) {
        if ((!field.field->error.get().empty() || !field.field->meta.get().errors.empty()) &&
            model->scroll(field.path, options)) {
            return true;
        }
    }
    return false;
}
} // namespace ryn

namespace ryn::detail {
namespace {}

void FormComponentHost::mount_provider(const FormProviderProps& props, const FormProviderContent& content) {
    auto& build = runtime::require_component_build_context();
    const auto provider = std::make_shared<FormProviderState>();
    provider->change = props.change_;
    provider->finish = props.finish_;
    const auto id = build.mount_component<FormProviderContext>(FormProviderContext{provider});
    const auto node = build.root(id);
    auto& services = *services_;
    services.layout().set_layout(node, layout::FlexLayout{layout::FlexDirection::vertical, 0, {}, true});
    build.on_resource_cleanup(id, [provider, node, &services] {
        provider->alive = false;
        provider->forms.clear();
        provider->change = {};
        provider->finish = {};
        services.layout().remove_layout(node);
    });
    build.mount_slot(id, content);
}
} // namespace ryn::detail

namespace ryn {
void FormProvider(FormProviderProps props, FormProviderContent content) {
    if (!detail::active_forms) {
        throw std::logic_error("FormProvider requires window component services");
    }
    detail::active_forms->mount_provider(props, content);
}

FormRef useFormInstance() {
    const auto* form = runtime::require_component_build_context().nearest_state<detail::FormState>();
    if (!form) {
        throw std::logic_error("useFormInstance requires a Form scope");
    }
    FormRef ref;
    ref.state_->model = form->model;
    return ref;
}
} // namespace ryn

namespace ryn::detail {
namespace {
bool automatic_candidate(bool explicit_binding) {
    auto& build = runtime::require_component_build_context();
    auto* item = build.nearest_state<FormItemState>(true);
    const auto* form = build.nearest_state<FormState>();
    if (!item || !form || item->form != form->id ||
        (private_editor_host == &build.host() && private_editor_item == item->id) || !item->field->registered ||
        item->control_claimed || !build.has_ancestor(item->regions.control)) {
        return false;
    }
    if (explicit_binding) {
        item->control_claimed = true;
        return false;
    }
    return true;
}

struct FormContextBinding final {
    std::shared_ptr<FormFieldState> field;

    Prop<InputStatus> status() const {
        return bind([field = field] { return form_field_status(field); });
    }

    Prop<bool> disabled() const {
        return bind([field = field] { return form_field_disabled(field); });
    }

    Prop<bool> readOnly() const {
        return bind([field = field] { return form_field_read_only(field); });
    }

    Prop<bool> uneditable() const {
        return bind([field = field] { return form_field_disabled(field) || form_field_read_only(field); });
    }

    Prop<FormPresentation> presentation() const {
        return field->presentation;
    }
};

std::optional<FormContextBinding> automatic_context() {
    auto& build = runtime::require_component_build_context();
    auto* item = build.nearest_state<FormItemState>(true);
    if (item && !(private_editor_host == &build.host() && private_editor_item == item->id) &&
        !item->field->registered && !item->control_claimed && build.has_ancestor(item->regions.control)) {
        item->control_claimed = true;
        return FormContextBinding{item->field};
    }
    return {};
}

template <class Props, class Binding> void automatic_input(Props& props, const Binding& binding) {
    const auto presentation = binding.presentation();
    props.status(binding.status())
        .disabled(binding.disabled())
        .readOnly(binding.readOnly())
        .size(bind([presentation] { return FormPropBridge::read(presentation).size; }))
        .variant(bind([presentation] { return FormPropBridge::read(presentation).variant; }))
        .direction(bind([presentation] { return FormPropBridge::read(presentation).direction; }));
}
} // namespace

FormControlMount FormAutoControlAccess::bind(InputProps& props) {
    if (const auto context = automatic_context()) {
        const auto disabled = props.common_.disabled_;
        const auto readonly = props.common_.read_only_;
        automatic_input(props, *context);
        props
            .disabled(ryn::bind([disabled, gate = context->disabled()] {
                return FormPropBridge::read(disabled) || FormPropBridge::read(gate);
            }))
            .readOnly(ryn::bind([readonly, gate = context->readOnly()] {
                return FormPropBridge::read(readonly) || FormPropBridge::read(gate);
            }));
        return {};
    }

    if (!automatic_candidate(props.common_.value_.has_value() || bool(props.common_.on_change_))) {
        return {};
    }
    props.common_.default_value_.reset();
    FormControlBinding<String> binding;
    InputRef ref;
    if (props.common_.reference_) {
        ref.state_ = props.common_.reference_;
    }
    const auto disabled = props.common_.disabled_;
    const auto readonly = props.common_.read_only_;
    const auto blur = props.common_.on_blur_;
    props.value(binding.value())
        .onChange(binding.onChange())
        .onBlur([blur, next = binding.onBlur()] {
            if (blur) {
                blur();
            }
            next();
        })
        .ref(ref);
    automatic_input(props, binding);
    props
        .disabled(ryn::bind([disabled, gate = binding.disabled()] {
            return FormPropBridge::read(disabled) || FormPropBridge::read(gate);
        }))
        .readOnly(ryn::bind([readonly, gate = binding.readOnly()] {
            return FormPropBridge::read(readonly) || FormPropBridge::read(gate);
        }));
    return FormControlMount{binding.connection_, [binding, ref] { binding.control(ref); }};
}

FormControlMount FormAutoControlAccess::bind(TextAreaProps& props) {
    if (const auto context = automatic_context()) {
        const auto disabled = props.common_.disabled_;
        const auto readonly = props.common_.read_only_;
        automatic_input(props, *context);
        props
            .disabled(ryn::bind([disabled, gate = context->disabled()] {
                return FormPropBridge::read(disabled) || FormPropBridge::read(gate);
            }))
            .readOnly(ryn::bind([readonly, gate = context->readOnly()] {
                return FormPropBridge::read(readonly) || FormPropBridge::read(gate);
            }));
        return {};
    }

    if (!automatic_candidate(props.common_.value_.has_value() || bool(props.common_.on_change_))) {
        return {};
    }
    props.common_.default_value_.reset();
    const auto original_blur = props.common_.on_blur_;
    const auto local_disabled = props.common_.disabled_;
    const auto local_readonly = props.common_.read_only_;
    FormControlBinding<String> binding;
    InputRef ref;
    if (props.common_.reference_) {
        ref.state_ = props.common_.reference_;
    }
    props.value(binding.value())
        .onChange(binding.onChange())
        .onBlur([original_blur, next = binding.onBlur()] {
            if (original_blur) {
                original_blur();
            }
            next();
        })
        .ref(ref);
    automatic_input(props, binding);
    props
        .disabled(ryn::bind([local_disabled, gate = binding.disabled()] {
            return FormPropBridge::read(local_disabled) || FormPropBridge::read(gate);
        }))
        .readOnly(ryn::bind([local_readonly, gate = binding.readOnly()] {
            return FormPropBridge::read(local_readonly) || FormPropBridge::read(gate);
        }));
    return FormControlMount{binding.connection_, [binding, ref] { binding.control(ref); }};
}

FormControlMount FormAutoControlAccess::bind(InputNumberProps& props) {
    if (const auto context = automatic_context()) {
        const auto disabled = props.input_.common_.disabled_;
        const auto readonly = props.input_.common_.read_only_;
        automatic_input(props, *context);
        props
            .disabled(ryn::bind([disabled, gate = context->disabled()] {
                return FormPropBridge::read(disabled) || FormPropBridge::read(gate);
            }))
            .readOnly(ryn::bind([readonly, gate = context->readOnly()] {
                return FormPropBridge::read(readonly) || FormPropBridge::read(gate);
            }));
        return {};
    }

    if (!automatic_candidate(props.value_.has_value() || props.string_value_.has_value() || bool(props.on_change_) ||
                             bool(props.string_change_))) {
        return {};
    }
    props.default_value_.reset();
    props.string_default_.reset();
    const auto original_blur = props.input_.common_.on_blur_;
    const auto local_disabled = props.input_.common_.disabled_;
    const auto local_readonly = props.input_.common_.read_only_;
    const auto* item = runtime::require_component_build_context().nearest_state<FormItemState>();
    InputRef ref;
    if (props.input_.common_.reference_) {
        ref.state_ = props.input_.common_.reference_;
    }
    if (std::holds_alternative<std::optional<Decimal>>(item->field->initial)) {
        FormControlBinding<InputNumberStringValue> binding;
        props.value(binding.value())
            .onStringChange(binding.onChange())
            .onBlur([original_blur, next = binding.onBlur()] {
                if (original_blur) {
                    original_blur();
                }
                next();
            })
            .ref(ref);
        automatic_input(props, binding);
        props
            .disabled(ryn::bind([local_disabled, gate = binding.disabled()] {
                return FormPropBridge::read(local_disabled) || FormPropBridge::read(gate);
            }))
            .readOnly(ryn::bind([local_readonly, gate = binding.readOnly()] {
                return FormPropBridge::read(local_readonly) || FormPropBridge::read(gate);
            }));
        return FormControlMount{binding.connection_, [binding, ref] { binding.control(ref); }};
    }
    FormControlBinding<InputNumberValue> binding;
    props.value(binding.value())
        .onChange(binding.onChange())
        .onBlur([original_blur, next = binding.onBlur()] {
            if (original_blur) {
                original_blur();
            }
            next();
        })
        .ref(ref);
    automatic_input(props, binding);
    props
        .disabled(ryn::bind([local_disabled, gate = binding.disabled()] {
            return FormPropBridge::read(local_disabled) || FormPropBridge::read(gate);
        }))
        .readOnly(ryn::bind([local_readonly, gate = binding.readOnly()] {
            return FormPropBridge::read(local_readonly) || FormPropBridge::read(gate);
        }));
    return FormControlMount{binding.connection_, [binding, ref] { binding.control(ref); }};
}

FormControlMount FormAutoControlAccess::bind(SelectProps& props) {
    if (const auto context = automatic_context()) {
        const auto disabled = props.disabled_;
        props.status(context->status()).disabled(ryn::bind([disabled, gate = context->uneditable()] {
            return FormPropBridge::read(disabled) || FormPropBridge::read(gate);
        }));
        return {};
    }

    if (!automatic_candidate(props.value_.has_value() || props.values_.has_value() ||
                             props.labeled_values_.has_value() || bool(props.on_change_) ||
                             bool(props.on_values_change_))) {
        return {};
    }
    const auto original_blur = props.on_blur_;
    const auto local_disabled = props.disabled_;
    props.default_value_.reset();
    props.default_values_.reset();
    const auto* item = runtime::require_component_build_context().nearest_state<FormItemState>();
    const auto presentation = Prop<FormPresentation>{item->field->presentation};
    props.size(ryn::bind([presentation] { return FormPropBridge::read(presentation).size; }))
        .variant(ryn::bind([presentation] { return FormPropBridge::read(presentation).variant; }))
        .direction(ryn::bind([presentation] { return FormPropBridge::read(presentation).direction; }));
    const InputRef ref = props.ref_.value_or(InputRef{});
    if (std::holds_alternative<SelectValues>(item->field->initial)) {
        FormControlBinding<SelectValues> binding;
        if (read_prop(props.mode_) == SelectMode::Single) {
            props.mode(SelectMode::Multiple);
        }
        props.values(binding.value())
            .onValuesChange(binding.onChange())
            .status(binding.status())
            .disabled(ryn::bind([local_disabled, gate = binding.uneditable()] {
                return FormPropBridge::read(local_disabled) || FormPropBridge::read(gate);
            }))
            .onBlur([original_blur, next = binding.onBlur()] {
                if (original_blur) {
                    original_blur();
                }
                next();
            })
            .ref(ref);
        return FormControlMount{binding.connection_, [binding, ref] { binding.control(ref); }};
    }
    FormControlBinding<SelectSelection> binding;
    props.value(binding.value())
        .onChange(binding.onChange())
        .status(binding.status())
        .disabled(ryn::bind([local_disabled, gate = binding.uneditable()] {
            return FormPropBridge::read(local_disabled) || FormPropBridge::read(gate);
        }))
        .onBlur([original_blur, next = binding.onBlur()] {
            if (original_blur) {
                original_blur();
            }
            next();
        })
        .ref(ref);
    return FormControlMount{binding.connection_, [binding, ref] { binding.control(ref); }};
}

FormControlMount FormAutoControlAccess::bind(CheckboxProps& props) {
    if (const auto context = automatic_context()) {
        const auto disabled = props.disabled_;
        props.disabled(ryn::bind([disabled, gate = context->uneditable()] {
            return FormPropBridge::read(disabled) || FormPropBridge::read(gate);
        }));
        return {};
    }

    if (!automatic_candidate(props.checked_.has_value() || bool(props.on_change_))) {
        return {};
    }
    if (!props.direction_) {
        const auto* item = runtime::require_component_build_context().nearest_state<FormItemState>();
        const auto context = Prop<FormPresentation>{item->field->presentation};
        props.direction(ryn::bind([context] {
            return FormPropBridge::read(context).direction == TextDirection::RightToLeft
                       ? CheckboxDirection::RightToLeft
                       : CheckboxDirection::LeftToRight;
        }));
    }
    const auto local_disabled = props.disabled_;
    props.default_checked_.reset();
    FormControlBinding<bool> binding;
    const auto ref = props.ref_.value_or(CheckboxRef{});
    props.checked(binding.value())
        .onChange(binding.onChange())
        .disabled(ryn::bind([local_disabled, gate = binding.uneditable()] {
            return FormPropBridge::read(local_disabled) || FormPropBridge::read(gate);
        }))
        .ref(ref);
    return FormControlMount{binding.connection_, [binding, ref] { binding.control(ref); }};
}

FormControlMount FormAutoControlAccess::bind(SwitchProps& props) {
    if (const auto context = automatic_context()) {
        const auto disabled = props.disabled_;
        props.disabled(ryn::bind([disabled, gate = context->uneditable()] {
            return FormPropBridge::read(disabled) || FormPropBridge::read(gate);
        }));
        return {};
    }

    if (!automatic_candidate(props.checked_.has_value() || bool(props.on_change_))) {
        return {};
    }
    const auto* item = runtime::require_component_build_context().nearest_state<FormItemState>();
    const auto context = Prop<FormPresentation>{item->field->presentation};
    const auto direction = props.direction_;
    const auto size = props.size_;
    props
        .direction(ryn::bind([context, direction] {
            return FormPropBridge::read(direction) == SwitchDirection::RightToLeft ||
                           FormPropBridge::read(context).direction == TextDirection::RightToLeft
                       ? SwitchDirection::RightToLeft
                       : SwitchDirection::LeftToRight;
        }))
        .size(ryn::bind([context, size] {
            return FormPropBridge::read(size) == SwitchSize::Small ||
                           FormPropBridge::read(context).size == ControlSize::Small
                       ? SwitchSize::Small
                       : SwitchSize::Middle;
        }));
    const auto local_disabled = props.disabled_;
    props.default_checked_.reset();
    FormControlBinding<bool> binding;
    const auto ref = props.ref_.value_or(SwitchRef{});
    props.checked(binding.value())
        .onChange(binding.onChange())
        .disabled(ryn::bind([local_disabled, gate = binding.uneditable()] {
            return FormPropBridge::read(local_disabled) || FormPropBridge::read(gate);
        }))
        .ref(ref);
    return FormControlMount{binding.connection_, [binding, ref] { binding.control(ref); }};
}
} // namespace ryn::detail

namespace ryn::detail {
namespace {
struct FormErrorListState final {
    std::array<Signal<String>, 2> text{Signal<String>{String{}}, Signal<String>{String{}}};
    std::array<Signal<runtime::SemanticForeground>, 2> colors{Signal<runtime::SemanticForeground>{{}},
                                                              Signal<runtime::SemanticForeground>{{}}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    theme_runtime::Subscription subscription;
};
} // namespace

void FormComponentHost::mount_errors(const FormErrorListProps& props) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<FormErrorListState>();
    auto& state = build.state<FormErrorListState>(id);
    const auto node = build.root(id);
    auto& services = *services_;
    services.layout().set_layout(node, layout::FlexLayout{layout::FlexDirection::vertical, 0, {}, true});
    runtime::connect_layout_style(build.scope(id), props.layout_, node, services.nodes(), services.dirty());
    build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            for (std::size_t i = 0; i < 2; ++i) {
                const auto child = context.mount_component<int>(0);
                const auto root = context.root(child);
                services.layout().set_layout(root, layout::BoxLayout{{}, true});
                context.on_resource_cleanup(child, [&services, root] { services.layout().remove_layout(root); });
                context.mount_slot_with_semantic_text_style(
                    child, Content{[text = state.text[i]] { Text(TextProps{}.content(text)); }}, state.colors[i],
                    state.font);
                const auto prop = i == 0 ? props.errors_ : props.warnings_;
                connect_prop(context.scope(child), prop,
                             [this, child, text = state.text[i]](std::vector<String> values) mutable {
                                 std::string joined;
                                 for (const auto& value : values) {
                                     if (!joined.empty()) {
                                         joined += '\n';
                                     }
                                     joined += value.bytes();
                                 }
                                 text.set(String::from_utf8(joined).value());
                                 if (services_->components().contains(child)) {
                                     services_->components().set_branch_active(child, !joined.empty());
                                     const auto node = services_->components().root(child);
                                     services_->nodes().require(node).out_of_flow = joined.empty();
                                     services_->dirty().invalidate(node, runtime::DirtyFlags::Measure |
                                                                             runtime::DirtyFlags::Layout |
                                                                             runtime::DirtyFlags::Geometry);
                                 }
                             });
            }
        }});
    const auto theme = build.theme_scope();
    const auto refresh = [this, id] {
        auto* state = services_->components().state<FormErrorListState>(id);
        if (!state) {
            return;
        }
        const auto& theme = services_->components().theme_scope(id)->snapshot();
        state->colors[0].set(foreground(theme.form().colors.error));
        state->colors[1].set(foreground(theme.map().color_warning));
        const auto& metrics = theme.form().metrics;
        state->font.set({metrics.font_family, 400, false, metrics.font_size, metrics.line_height});
    };
    state.subscription = theme->capture([refresh](theme_runtime::DirtyPhase) { refresh(); },
                                        [theme] {
                                            static_cast<void>(theme->form_colors());
                                            static_cast<void>(theme->form_metrics());
                                            static_cast<void>(theme->map());
                                        });
    refresh();
}
} // namespace ryn::detail

namespace ryn {
Binding<FormItemStatus> useFormStatus() {
    const auto* item = runtime::require_component_build_context().nearest_state<detail::FormItemState>();
    if (!item) {
        throw std::logic_error("useFormStatus requires a FormItem");
    }
    return bind([field = item->field] {
        const auto meta = detail::form_field_meta(field);
        return FormItemStatus{field->display_status.get(), meta.errors, meta.warnings};
    });
}

void FormErrorList(FormErrorListProps props) {
    if (!detail::active_forms) {
        throw std::logic_error("FormErrorList requires window component services");
    }
    detail::active_forms->mount_errors(props);
}
} // namespace ryn
