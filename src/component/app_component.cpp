#include "component/app_component.hpp"
#include "component/notice_model.hpp"
#include "runtime/layout_style_adapter.hpp"

namespace ryn::detail {
namespace {
thread_local AppComponentHost* active_apps{};

std::shared_ptr<AppBinding> binding(const std::shared_ptr<AppRefState>& state) {
    if (!state || !state->context) {
        return {};
    }
    const auto result = state->binding.lock();
    return result && app_context_live(*result, *state->context) ? result : nullptr;
}

std::shared_ptr<AppRefState> copy_state(const std::shared_ptr<AppRefState>& state) {
    // Unbound props and their original ref share the first binding. A copy made
    // after binding keeps that generation even if the original ref is rebound.
    return state && state->assigned ? std::make_shared<AppRefState>(*state) : state;
}
} // namespace

bool app_context_live(const AppBinding& binding, const runtime::CapturedComponentContext& context) {
    if (binding.thread != std::this_thread::get_id() || !binding.alive) {
        return false;
    }
    const auto owner = binding.context.owner.lock();
    if (!owner || context.owner.lock() != owner || !context.theme || !context.config || !context.config->active()) {
        return false;
    }
    auto& host = binding.services->components();
    return host.active() && host.contains(binding.owner) && host.scope(binding.owner).active() &&
           (!context.component_owner ||
            (host.contains(*context.component_owner) && host.scope(*context.component_owner).active()));
}

AppComponentHost::AppComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

AppComponentHost::~AppComponentHost() {
    services_->detach(*this);
}

void* AppComponentHost::begin_mount() noexcept {
    return std::exchange(active_apps, this);
}

void AppComponentHost::end_mount(void* previous) noexcept {
    active_apps = static_cast<AppComponentHost*>(previous);
}

void AppComponentHost::mount(const AppProps& props, const AppContent& content) {
    if (props.reference_) {
        const auto previous = props.reference_->binding.lock();
        if (previous && previous->alive) {
            throw std::invalid_argument("AppRef is already bound to a live App");
        }
    }
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<AppState>();
    const auto binding = build.state<AppState>(id).binding;
    auto& services = *services_;
    binding->services = &services;
    binding->owner = id;
    binding->context = services.components().capture_context(id);
    build.scope(id).on_cleanup([binding] { binding->alive = false; });
    const auto node = build.root(id);
    services.layout().set_layout(node, layout::FlexLayout{layout::FlexDirection::vertical, 0, {}, true});
    build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
    runtime::connect_layout_style(build.scope(id), props.layout_, node, services.nodes(), services.dirty());
    if (!binding->alive || !services.components().contains(id) || !services.components().scope(id).active()) {
        return;
    }
    if (props.reference_) {
        props.reference_->binding = binding;
        props.reference_->context = binding->context;
        props.reference_->assigned = true;
    }
    build.mount_slot(id, Content{[&, binding] {
                         MessageHolder(MessageHolderProps{}.ref(binding->message).maxCount(props.message_max_count_));
                         NotificationHolder(NotificationHolderProps{}
                                                .ref(binding->notification)
                                                .maxCount(props.notification_max_count_)
                                                .stack(props.notification_stack_));
                         auto& children = runtime::require_component_build_context();
                         children.mount_slot(id, content);
                     }});
}
} // namespace ryn::detail

namespace ryn {
AppRef::AppRef() : state_(std::make_shared<detail::AppRefState>()) {}

AppRef::AppRef(const AppRef& other) : state_(detail::copy_state(other.state_)) {}

AppRef& AppRef::operator=(const AppRef& other) {
    state_ = detail::copy_state(other.state_);
    return *this;
}

bool AppRef::bound() const {
    return bool(detail::binding(state_));
}

MessageRef AppRef::message() const {
    MessageRef result;
    if (const auto target = detail::binding(state_)) {
        result.state_->binding = target->message.state_->binding;
        result.state_->context = state_->context;
        result.state_->live = [weak = std::weak_ptr{target}, context = *state_->context] {
            const auto owner = weak.lock();
            return owner && detail::app_context_live(*owner, context);
        };
    }
    return result;
}

NotificationRef AppRef::notification() const {
    NotificationRef result;
    if (const auto target = detail::binding(state_)) {
        result.state_->binding = target->notification.state_->binding;
        result.state_->context = state_->context;
        result.state_->live = [weak = std::weak_ptr{target}, context = *state_->context] {
            const auto owner = weak.lock();
            return owner && detail::app_context_live(*owner, context);
        };
    }
    return result;
}

void App(AppProps props, AppContent content) {
    if (!detail::active_apps) {
        throw std::logic_error("App requires window component services");
    }
    detail::active_apps->mount(props, content);
}

AppRef useApp() {
    auto& build = runtime::require_component_build_context();
    const auto* owner = build.nearest_state<detail::AppState>();
    if (!owner) {
        throw std::logic_error("useApp requires an App scope");
    }
    AppRef result;
    result.state_->binding = owner->binding;
    result.state_->context = build.capture_context();
    result.state_->assigned = true;
    return result;
}
} // namespace ryn
