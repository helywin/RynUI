#include "component/app_panel_service.hpp"
#include "component/modal_component.hpp"
#include "component/popconfirm_component.hpp"
#include "component/localized_prop.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/icon.hpp>
#include <algorithm>
#include <cmath>
#include <exception>

namespace ryn::detail {
namespace {
enum class PanelKind { Modal, Drawer, Popconfirm };

template <class T> struct PanelValue final {
    explicit PanelValue(T initial) : value(std::move(initial)) {}

    Signal<T> value;
    std::shared_ptr<Scope> subscription;
    std::uint64_t revision{};
};
} // namespace

struct PanelEntry final {
    std::shared_ptr<PanelServiceContext> context;
    PanelKind kind{};
    runtime::ComponentId root;
    runtime::ComponentId panel;
    runtime::ComponentId body;
    runtime::ComponentId region;
    runtime::ComponentId source;
    std::shared_ptr<PopconfirmAnchorBinding> anchor;
    std::optional<IconName> status_icon;
    Signal<bool> open{false};
    PanelValue<String> title{String{}};
    PanelValue<String> description{String{}};
    PanelValue<String> ok{String{}};
    PanelValue<String> cancel{String{}};
    PanelValue<float> width{520.0F};
    PanelValue<Responsive<float>> width_profile{Responsive<float>{520}};
    Signal<bool> width_profile_active{false};
    PanelValue<float> height{378.0F};
    PanelValue<bool> loading{false};
    std::vector<std::shared_ptr<Scope>> subscriptions;
    std::vector<std::shared_ptr<void>> values;
    std::promise<PanelCloseResult> promise;
    std::shared_future<PanelCloseResult> future{promise.get_future().share()};
    std::function<void()> after_close;
    std::function<void(bool)> after_open;
    std::function<void(bool)> after_presentation;
    std::function<void(ModalCancelReason)> modal_cancel;
    std::function<void(DrawerCancelReason)> drawer_cancel;
    std::function<void()> popconfirm_cancel;
    std::function<void()> accepted;
    std::function<void(ConfirmActionError)> action_error;
    bool retired{};
    bool committed{};
    bool settled{};
    std::uint64_t revision{};
    bool closing{};
    std::optional<PanelCloseResult> pending_close;
    std::uint64_t close_revision{};
};

namespace {
bool live(const std::shared_ptr<PanelEntry>& entry) {
    return entry && AppPanelService::live(entry->context) && !entry->retired;
}

void settle(const std::shared_ptr<PanelEntry>& entry, PanelCloseResult result) noexcept {
    if (!entry->settled) {
        entry->settled = true;
        try {
            entry->promise.set_value(result);
        } catch (...) {
        }
    }
}

WindowComponentServices& services(const std::shared_ptr<PanelEntry>& entry) {
    return *entry->context->binding.lock()->services;
}

void dispose_subscriptions(const std::shared_ptr<PanelEntry>& entry) noexcept {
    for (const auto& subscription : entry->subscriptions) {
        subscription->dispose();
    }
    entry->subscriptions.clear();
}

ConfirmAction::Cancel detach(const std::shared_ptr<PanelEntry>& entry) noexcept {
    // This function is also called synchronously inside owner cleanup. The
    // services object is then still executing its retained teardown.
    const auto owner = entry->context->binding.lock();
    if (!owner || !entry->panel.valid()) {
        return {};
    }
    return entry->kind == PanelKind::Popconfirm ? owner->services->popconfirm().detach_action(entry->panel)
                                                : owner->services->modal().detach_action(entry->panel);
}

void erase(const std::shared_ptr<PanelEntry>& entry) {
    if (const auto owner = entry->context->binding.lock()) {
        std::erase(owner->panels, entry);
    }
    if (entry->anchor && entry->anchor->service.lock() == entry) {
        entry->anchor->service.reset();
    }
}

void finish(const std::shared_ptr<PanelEntry>& entry, PanelCloseResult result, bool notify = true,
            bool retire_resources = true) {
    if (!entry || entry->retired) {
        return;
    }
    entry->retired = true;
    const auto owner = entry->context->binding.lock();
    ++entry->revision;
    auto cancel = detach(entry);
    erase(entry);
    dispose_subscriptions(entry);
    // Animated panels arrive through effective exit completion or hard
    // retirement. Legacy nonanimated panels retain immediate retirement.
    std::exception_ptr error;
    if (retire_resources && owner && !entry->context->captured.owner.expired()) {
        auto& target = *owner->services;
        if (notify) {
            try {
                entry->open.set(false);
            } catch (...) {
                error = std::current_exception();
            }
        }
        try {
            if (!entry->context->captured.owner.expired()) {
                if (notify) {
                    target.destroy(entry->root);
                } else {
                    target.retire_failed_component(entry->root);
                }
                target.request_frame();
            }
        } catch (...) {
            if (!error) {
                error = std::current_exception();
            }
        }
    }
    settle(entry, result);
    ConfirmActionSession::run_cancel(std::move(cancel));
    if (!notify) {
        return;
    }
    if (!owner || !app_context_live(*owner, entry->context->captured)) {
        return;
    }
    // Both notifications run even when the first throws. Internal cleanup and
    // settlement never depend on application callback success.
    const auto call = [&](auto&& callback) {
        try {
            callback();
        } catch (...) {
            if (!error) {
                error = std::current_exception();
            }
        }
    };
    if (result.kind == PanelCloseKind::Cancelled) {
        if (result.modal_cancel && entry->modal_cancel) {
            call([&] { entry->modal_cancel(*result.modal_cancel); });
        } else if (result.drawer_cancel && entry->drawer_cancel) {
            call([&] { entry->drawer_cancel(*result.drawer_cancel); });
        } else if (entry->popconfirm_cancel) {
            call(entry->popconfirm_cancel);
        }
    }
    if (app_context_live(*owner, entry->context->captured) && entry->after_open) {
        call([&] { entry->after_open(false); });
    }
    if (app_context_live(*owner, entry->context->captured) && entry->after_presentation) {
        call([&] { entry->after_presentation(false); });
    }
    if (app_context_live(*owner, entry->context->captured) && entry->after_close) {
        call(entry->after_close);
    }
    if (error) {
        std::rethrow_exception(error);
    }
}

bool begin_panel_close(const std::shared_ptr<PanelEntry>& entry, PanelCloseResult result) {
    if (!live(entry) || entry->closing) {
        return false;
    }
    entry->closing = true;
    entry->pending_close = result;
    const auto revision = ++entry->close_revision;
    ++entry->revision; // Retire an in-flight update candidate, without dropping owner cleanup.
    auto cancel = detach(entry);
    ConfirmActionSession::run_cancel(std::move(cancel));
    if (!live(entry) || entry->close_revision != revision) {
        return true;
    }
    try {
        entry->open.set(false);
        if (live(entry) && entry->close_revision == revision) {
            if (entry->kind == PanelKind::Popconfirm) {
                services(entry).popconfirm().arm_service_close(entry->panel);
            } else {
                services(entry).modal().arm_service_panel_close(entry->panel);
            }
            services(entry).request_frame();
        }
    } catch (...) {
        finish(entry, {PanelCloseKind::Failed}, false);
        throw;
    }
    return true;
}

bool close_entry(const std::shared_ptr<PanelEntry>& entry, PanelCloseResult result) {
    if (!live(entry) || entry->closing) {
        return false;
    }
    if (entry->kind == PanelKind::Drawer ||
        (entry->kind == PanelKind::Modal && services(entry).modal().presentation_enabled(entry->panel)) ||
        (entry->kind == PanelKind::Popconfirm && services(entry).popconfirm().presentation_enabled(entry->panel))) {
        return begin_panel_close(entry, result);
    }
    finish(entry, result);
    return true;
}

void confirm_entry(const std::shared_ptr<PanelEntry>& entry) {
    if (!live(entry)) {
        return;
    }
    const auto accepted = entry->accepted;
    const auto error = entry->action_error;
    if (accepted) {
        try {
            accepted();
        } catch (...) {
            if (live(entry) && error) {
                error({ConfirmActionErrorCode::ProviderThrew, {}});
            }
            return;
        }
    }
    if (live(entry)) {
        close_entry(entry, {PanelCloseKind::Confirmed});
    }
}

template <class T> void connect(const std::shared_ptr<PanelEntry>& entry, PanelValue<T>& field, Prop<T> source) {
    if (!live(entry)) {
        throw std::logic_error("App panel source is retired");
    }
    const auto revision = ++field.revision;
    const auto candidate = std::make_shared<Scope>();
    const auto weak = std::weak_ptr{entry};
    auto* cell = &field;
    try {
        connect_prop(*candidate, source, [weak, cell, revision](T value) {
            const auto entry = weak.lock();
            if (live(entry) && cell->revision == revision) {
                const auto signal = cell->value;
                signal.set(std::move(value));
            }
        });
    } catch (...) {
        if (field.revision == revision) {
            --field.revision;
        }
        candidate->dispose();
        throw;
    }
    if (!live(entry) || field.revision != revision) {
        candidate->dispose();
        return;
    }
    if (field.subscription) {
        field.subscription->dispose();
        std::erase(entry->subscriptions, field.subscription);
    }
    field.subscription = candidate;
    entry->subscriptions.push_back(candidate);
}

template <class T> Prop<T> cache(const std::shared_ptr<PanelEntry>& entry, Prop<T> source, T initial = {}) {
    const auto cell = std::make_shared<PanelValue<T>>(std::move(initial));
    entry->values.push_back(cell);
    connect(entry, *cell, std::move(source));
    return cell->value;
}

runtime::ComponentId region(WindowComponentServices& target, const Content& content, bool candidate = false) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    if (candidate) {
        build.host().set_branch_active(id, false);
    }
    target.layout().set_layout(node, layout::FlexLayout{layout::FlexDirection::vertical, 0, {}, true});
    build.on_resource_cleanup(id, [&target, node] { target.layout().remove_layout(node); });
    build.mount_slot(id, content);
    return id;
}

std::shared_ptr<PanelEntry> create(const std::shared_ptr<PanelServiceContext>& context, PanelKind kind,
                                   std::optional<runtime::ComponentId> parent = {}) {
    if (!AppPanelService::live(context)) {
        throw std::logic_error("App panel requires a live captured App context on its owner thread");
    }
    const auto owner = context->binding.lock();
    const auto entry = std::make_shared<PanelEntry>();
    entry->context = context;
    entry->kind = kind;
    entry->source = parent.value_or(context->captured.component_owner.value_or(owner->owner));
    owner->panels.push_back(entry);
    const auto watch_source = [&](runtime::ComponentId source) {
        if (source == owner->owner || std::ranges::find(owner->panel_sources, source) != owner->panel_sources.end()) {
            return;
        }
        owner->services->components().scope(source).on_cleanup([weak = std::weak_ptr{owner}, source] {
            if (const auto owner = weak.lock()) {
                std::erase(owner->panel_sources, source);
                AppPanelService::retire_owner(owner, source);
            }
        });
        owner->panel_sources.push_back(source);
    };
    watch_source(entry->source);
    if (context->captured.component_owner) {
        watch_source(*context->captured.component_owner);
    }
    return entry;
}

void mount(const std::shared_ptr<PanelEntry>& entry, const Content& content) {
    auto& target = services(entry);
    try {
        target.append_slot(
            entry->source, Content{[&] {
                if (!live(entry)) {
                    return;
                }
                auto& build = runtime::require_component_build_context();
                entry->root = build.mount_component<int>(0);
                const auto node = build.root(entry->root);
                target.nodes().require(node).out_of_flow = true;
                target.layout().set_layout(node, layout::BoxLayout{});
                build.on_resource_cleanup(entry->root, [&target, node] { target.layout().remove_layout(node); });
                build.scope(entry->root).on_cleanup([weak = std::weak_ptr{entry}] {
                    if (const auto entry = weak.lock(); entry && !entry->retired) {
                        // The host already owns this subtree's teardown; do not recursively destroy its scope.
                        finish(entry, {entry->committed ? PanelCloseKind::OwnerDisposed : PanelCloseKind::Failed},
                               false, false);
                    }
                });
                build.mount_slot(entry->root, content);
                if (live(entry) && target.components().contains(entry->root)) {
                    entry->panel = target.components().children(entry->root).back();
                }
            }},
            entry->context->captured);
        if (live(entry) && entry->panel.valid()) {
            entry->committed = true;
            entry->open.set(true);
            if (live(entry)) {
                target.request_frame();
            }
        } else if (!entry->retired) {
            finish(entry, {PanelCloseKind::OwnerDisposed}, false);
        }
    } catch (...) {
        finish(entry, {PanelCloseKind::Failed}, false);
        throw;
    }
}

std::shared_ptr<PanelHandleState> handle(const std::shared_ptr<PanelEntry>& entry) {
    return std::make_shared<PanelHandleState>(entry, entry->future);
}

void replace_body(const std::shared_ptr<PanelEntry>& entry, const Content& content, std::uint64_t revision) {
    if (!live(entry) || !entry->body.valid()) {
        return;
    }
    auto& target = services(entry);
    runtime::ComponentId candidate;
    try {
        target.modal().append_service_body(entry->panel, entry->body, Content{[&] {
                                               candidate = region(target, Content{[content, icon = entry->status_icon] {
                                                                      if (icon) {
                                                                          Icon(IconProps{}.name(*icon));
                                                                      }
                                                                      SlotContentAccess::function(content)();
                                                                  }},
                                                                  true);
                                           }},
                                           entry->context->captured);
    } catch (...) {
        if (candidate.valid() && !entry->context->captured.owner.expired()) {
            target.retire_failed_component(candidate);
        }
        throw;
    }
    if (!live(entry) || entry->revision != revision) {
        if (!entry->context->captured.owner.expired()) {
            target.retire_failed_component(candidate);
        }
        return;
    }
    const auto previous = std::exchange(entry->region, candidate);
    target.components().set_branch_active(candidate, true);
    target.destroy(previous);
    if (live(entry)) {
        target.request_frame();
    }
}

void validate_extent(const std::optional<Prop<float>>& value) {
    if (value) {
        const auto extent = read_prop(*value);
        if (!std::isfinite(extent) || extent <= 0) {
            throw std::invalid_argument("App panel extent must be positive and finite");
        }
    }
}
} // namespace

bool AppPanelService::live(const std::shared_ptr<PanelServiceContext>& context) {
    const auto owner = context ? context->binding.lock() : nullptr;
    return owner && app_context_live(*owner, context->captured);
}

bool AppPanelService::live(const std::shared_ptr<PanelHandleState>& state) {
    const auto entry = state ? state->entry.lock() : nullptr;
    return detail::live(entry) && !entry->closing;
}

ModalHandle AppPanelService::modal(const std::shared_ptr<PanelServiceContext>& context, ModalProps props,
                                   ModalContent content, ModalSlots slots, std::optional<IconName> icon) {
    const auto entry = create(context, PanelKind::Modal);
    entry->status_icon = icon;
    ModalHandle result;
    result.state_ = handle(entry);
    try {
        connect(entry, entry->title, props.title_);
        connect(entry, entry->width, props.width_);
        if (props.responsive_width_) {
            connect(entry, entry->width_profile, *props.responsive_width_);
            entry->width_profile_active.set(true);
            props.responsive_width_ = bind([profile = entry->width_profile.value, active = entry->width_profile_active,
                                            fallback = entry->width.value] {
                return active.get() ? profile.get() : Responsive<float>{fallback.get()};
            });
        }
        const auto locale =
            localized_prop(props.locale_, context->captured.config, [](const LocaleData& data) { return data.common; });
        connect(entry, entry->ok,
                props.ok_text_.value_or(Prop<String>{bind([locale] { return read_prop(locale).confirm; })}));
        connect(entry, entry->cancel,
                props.cancel_text_.value_or(Prop<String>{bind([locale] { return read_prop(locale).cancel; })}));
        entry->after_close = props.after_close_;
        entry->after_open = props.after_open_change_;
        entry->modal_cancel = props.on_cancel_;
        props.action_live_ = [weak = std::weak_ptr{entry}] {
            return detail::live(weak.lock());
        };
        entry->action_error = props.action_error_;
        if (props.action_) {
            entry->accepted = props.on_ok_;
        }
        props.centered_ = cache(entry, props.centered_);
        props.closable_ = cache(entry, props.closable_);
        props.keyboard_ = cache(entry, props.keyboard_);
        props.focus_trap_ = cache(entry, props.focus_trap_);
        props.mask_ = cache(entry, props.mask_);
        props.confirm_loading_ = cache(entry, props.confirm_loading_);
        props.show_cancel_ = cache(entry, props.show_cancel_);
        props.footer_ = cache(entry, props.footer_);
        props.restore_focus_ = cache(entry, props.restore_focus_);
        props.lazy_ = cache(entry, props.lazy_);
        props.destroy_hidden_ = cache(entry, props.destroy_hidden_);
        props.animated_ = cache(entry, props.animated_);
        props.rtl_ = cache(entry, props.rtl_);
        props.draggable_ = cache(entry, props.draggable_);
        props.resizable_ = cache(entry, props.resizable_);
        props.width_ = entry->width.value;
        if (!props.action_ && props.on_ok_) {
            props.action_ = ConfirmAction::synchronous([callback = props.on_ok_] {
                callback();
                return ConfirmActionResult{};
            });
        }
        props.open(entry->open)
            .title(entry->title.value)
            .okText(entry->ok.value)
            .cancelText(entry->cancel.value)
            .afterClose({})
            .afterOpenChange([weak = std::weak_ptr{entry}](bool open) {
                const auto entry = weak.lock();
                if (!detail::live(entry)) {
                    return;
                }
                if (open) {
                    if (!entry->closing && entry->after_open) {
                        entry->after_open(true);
                    }
                } else if (entry->closing && entry->pending_close) {
                    finish(entry, *entry->pending_close);
                }
            })
            .onOk([weak = std::weak_ptr{entry}] {
                if (const auto entry = weak.lock(); detail::live(entry)) {
                    confirm_entry(entry);
                }
            })
            .onCancel([weak = std::weak_ptr{entry}](ModalCancelReason reason) {
                if (const auto entry = weak.lock(); detail::live(entry)) {
                    close_entry(entry, {PanelCloseKind::Cancelled, reason});
                }
            });
        if (icon) {
            props.showCancel(false);
        }
        mount(entry, Content{[entry, props, content, slots, icon] {
                  Modal(props, ModalContent{[entry, content, icon] {
                            auto& target = services(entry);
                            entry->body =
                                runtime::require_component_build_context().capture_context().component_owner.value();
                            entry->region = region(target, Content{[content, icon] {
                                                       if (icon) {
                                                           Icon(IconProps{}.name(*icon));
                                                       }
                                                       SlotContentAccess::function(content)();
                                                   }});
                        }},
                        slots);
              }});
    } catch (...) {
        finish(entry, {PanelCloseKind::Failed}, false);
        throw;
    }
    return result;
}

DrawerHandle AppPanelService::drawer(const std::shared_ptr<PanelServiceContext>& context, DrawerProps props,
                                     DrawerContent content, DrawerSlots slots) {
    const auto entry = create(context, PanelKind::Drawer);
    DrawerHandle result;
    result.state_ = handle(entry);
    try {
        connect(entry, entry->title, props.title_);
        connect(entry, entry->width, props.width_);
        connect(entry, entry->height, props.height_);
        connect(entry, entry->loading, props.loading_);
        entry->after_close = props.after_close_;
        entry->after_open = props.after_open_change_;
        entry->drawer_cancel = props.on_cancel_;
        props.placement_ = cache(entry, props.placement_);
        props.closable_ = cache(entry, props.closable_);
        props.keyboard_ = cache(entry, props.keyboard_);
        props.mask_ = cache(entry, props.mask_);
        props.restore_focus_ = cache(entry, props.restore_focus_);
        props.lazy_ = cache(entry, props.lazy_);
        props.destroy_hidden_ = cache(entry, props.destroy_hidden_);
        props.rtl_ = cache(entry, props.rtl_);
        props.resize_ = cache(entry, props.resize_);
        props.push_ = cache(entry, props.push_);
        props.open(entry->open)
            .title(entry->title.value)
            .width(entry->width.value)
            .height(entry->height.value)
            .loading(entry->loading.value)
            .afterClose({})
            .afterOpenChange([weak = std::weak_ptr{entry}](bool open) {
                const auto entry = weak.lock();
                if (!detail::live(entry)) {
                    return;
                }
                if (open) {
                    if (!entry->closing && entry->after_open) {
                        entry->after_open(true);
                    }
                } else if (entry->closing && entry->pending_close) {
                    // The host has already checked its motion revision. The
                    // entry close revision owns the fixed business result and
                    // remains independent of a reentrant host reconcile pass.
                    const auto revision = entry->close_revision;
                    const auto result = *entry->pending_close;
                    if (detail::live(entry) && entry->closing && entry->close_revision == revision) {
                        finish(entry, result);
                    }
                }
            })
            .onCancel([weak = std::weak_ptr{entry}](DrawerCancelReason reason) {
                if (const auto entry = weak.lock(); detail::live(entry)) {
                    begin_panel_close(entry, {PanelCloseKind::Cancelled, {}, reason});
                }
            });
        mount(entry, Content{[entry, props, content, slots] {
                  Drawer(props, DrawerContent{[entry, content] {
                             auto& target = services(entry);
                             entry->body =
                                 runtime::require_component_build_context().capture_context().component_owner.value();
                             entry->region = region(target, Content{SlotContentAccess::function(content)});
                         }},
                         slots);
              }});
    } catch (...) {
        finish(entry, {PanelCloseKind::Failed}, false);
        throw;
    }
    return result;
}

PopconfirmHandle AppPanelService::popconfirm(const std::shared_ptr<PanelServiceContext>& context,
                                             const PopconfirmRef& ref, PopconfirmProps props, PopconfirmSlots slots) {
    if (!live(context)) {
        throw std::logic_error("Popconfirm service requires a live App");
    }
    if (props.reference_) {
        throw std::invalid_argument("Popconfirm service props cannot rebind its explicit anchor");
    }
    const auto anchor = ref.state_ ? ref.state_->binding.lock() : nullptr;
    if (!anchor || !popconfirm_anchor_live(*anchor) || anchor->context.owner.lock() != context->captured.owner.lock() ||
        anchor->controlled || anchor->open || !anchor->service.expired()) {
        throw std::invalid_argument("Popconfirm service requires a live, free, uncontrolled anchor in its window");
    }
    const auto owner = context->binding.lock();
    auto ancestor = std::optional{anchor->owner};
    while (ancestor && *ancestor != owner->owner) {
        const auto* app = owner->services->components().state<AppState>(*ancestor);
        if (app && app->binding != owner) {
            throw std::invalid_argument("Popconfirm anchor belongs to a nested App");
        }
        ancestor = owner->services->components().parent(*ancestor);
    }
    if (!ancestor) {
        throw std::invalid_argument("Popconfirm anchor belongs to another App");
    }
    const auto overlay = [&](runtime::ComponentId id) -> std::optional<runtime::ComponentId> {
        for (auto current = std::optional{id}; current; current = owner->services->components().parent(*current)) {
            for (const auto& view : owner->services->overlay_stack().ordered()) {
                if (view.root == *current) {
                    return view.owner;
                }
            }
        }
        return {};
    };
    if (context->captured.component_owner) {
        const auto caller_overlay = overlay(*context->captured.component_owner);
        if (caller_overlay && caller_overlay != overlay(anchor->owner)) {
            throw std::invalid_argument("Popconfirm anchor is outside its caller's overlay family");
        }
    }
    const auto entry = create(context, PanelKind::Popconfirm, anchor->owner);
    entry->anchor = anchor;
    anchor->service = entry;
    PopconfirmHandle result;
    result.state_ = handle(entry);
    try {
        connect(entry, entry->title, props.title_);
        connect(entry, entry->description, props.description_);
        const auto locale =
            localized_prop(props.locale_, context->captured.config, [](const LocaleData& data) { return data.common; });
        connect(entry, entry->ok,
                props.ok_text_.value_or(Prop<String>{bind([locale] { return read_prop(locale).confirm; })}));
        connect(entry, entry->cancel,
                props.cancel_text_.value_or(Prop<String>{bind([locale] { return read_prop(locale).cancel; })}));
        if (props.action_) {
            entry->accepted = props.on_confirm_;
        }
        if (!props.action_ && props.on_confirm_) {
            props.action_ = ConfirmAction::synchronous([callback = props.on_confirm_] {
                callback();
                return ConfirmActionResult{};
            });
        }
        entry->popconfirm_cancel = props.on_cancel_;
        props.action_live_ = [weak = std::weak_ptr{entry}] {
            return detail::live(weak.lock());
        };
        entry->after_open = props.on_open_;
        entry->after_presentation = props.after_open_;
        entry->action_error = props.action_error_;
        props.disabled_ = cache(entry, props.disabled_);
        props.show_cancel_ = cache(entry, props.show_cancel_);
        props.loading_ = cache(entry, props.loading_);
        props.placement_ = cache(entry, props.placement_);
        props.arrow_ = cache(entry, props.arrow_);
        props.point_at_center_ = cache(entry, props.point_at_center_);
        props.adjust_ = cache(entry, props.adjust_);
        props.animated_ = cache(entry, props.animated_);
        props.trigger_ = cache(entry, props.trigger_);
        if (props.triggers_) {
            props.triggers_ = cache(entry, *props.triggers_);
        }
        props.direction_ = cache(entry, props.direction_);
        props.enter_ = cache(entry, props.enter_);
        props.leave_ = cache(entry, props.leave_);
        props.afterOpenChange([weak = std::weak_ptr{entry}](bool visible) {
            if (const auto entry = weak.lock(); detail::live(entry)) {
                if (visible) {
                    if (!entry->closing && entry->after_presentation) {
                        entry->after_presentation(true);
                    }
                } else if (entry->closing && entry->pending_close) {
                    finish(entry, *entry->pending_close);
                }
            }
        });
        props.reference_.reset();
        props.default_open_.reset();
        props.service_anchor_ = anchor;
        // Preserve text-vs-slot source declarations while substituting cached live values.
        props.title_ = entry->title.value;
        props.description_ = entry->description.value;
        props.open(entry->open)
            .okText(entry->ok.value)
            .cancelText(entry->cancel.value)
            .onConfirm([weak = std::weak_ptr{entry}] {
                if (const auto entry = weak.lock(); detail::live(entry)) {
                    confirm_entry(entry);
                }
            })
            .onCancel({})
            .onOpenChange([weak = std::weak_ptr{entry}](bool open) {
                if (const auto entry = weak.lock(); detail::live(entry) && !open) {
                    close_entry(entry, {PanelCloseKind::Cancelled});
                }
            });
        mount(entry, Content{[props, slots] { Popconfirm(props, PopconfirmTrigger{[] {}}, slots); }});
    } catch (...) {
        finish(entry, {PanelCloseKind::Failed}, false);
        throw;
    }
    return result;
}

bool AppPanelService::update(const std::shared_ptr<PanelHandleState>& state, ModalUpdate patch) {
    const auto entry = state ? state->entry.lock() : nullptr;
    if (!detail::live(entry) || entry->closing || entry->kind != PanelKind::Modal) {
        return false;
    }
    const auto revision = ++entry->revision;
    const auto current = [&] {
        return detail::live(entry) && entry->revision == revision;
    };
    validate_extent(patch.width);
    if (!current()) {
        return false;
    }
    if (patch.content) {
        replace_body(entry, Content{SlotContentAccess::function(*patch.content)}, revision);
    }
    if (patch.title && current()) {
        connect(entry, entry->title, *patch.title);
    }
    if (patch.ok_text && current()) {
        connect(entry, entry->ok, *patch.ok_text);
    }
    if (patch.cancel_text && current()) {
        connect(entry, entry->cancel, *patch.cancel_text);
    }
    if (patch.width && current()) {
        connect(entry, entry->width, *patch.width);
        if (current()) {
            ++entry->width_profile.revision;
            if (entry->width_profile.subscription) {
                entry->width_profile.subscription->dispose();
                std::erase(entry->subscriptions, entry->width_profile.subscription);
                entry->width_profile.subscription.reset();
            }
            entry->width_profile_active.set(false);
        }
    }
    return current();
}

bool AppPanelService::update(const std::shared_ptr<PanelHandleState>& state, DrawerUpdate patch) {
    const auto entry = state ? state->entry.lock() : nullptr;
    if (!detail::live(entry) || entry->closing || entry->kind != PanelKind::Drawer) {
        return false;
    }
    const auto revision = ++entry->revision;
    const auto current = [&] {
        return detail::live(entry) && !entry->closing && entry->revision == revision;
    };
    validate_extent(patch.width);
    validate_extent(patch.height);
    if (!current()) {
        return false;
    }
    if (patch.content) {
        replace_body(entry, Content{SlotContentAccess::function(*patch.content)}, revision);
    }
    if (patch.title && current()) {
        connect(entry, entry->title, *patch.title);
    }
    if (patch.width && current()) {
        connect(entry, entry->width, *patch.width);
    }
    if (patch.height && current()) {
        connect(entry, entry->height, *patch.height);
    }
    if (patch.loading && current()) {
        connect(entry, entry->loading, *patch.loading);
    }
    return current();
}

bool AppPanelService::update(const std::shared_ptr<PanelHandleState>& state, PopconfirmUpdate patch) {
    const auto entry = state ? state->entry.lock() : nullptr;
    if (!detail::live(entry) || entry->closing || entry->kind != PanelKind::Popconfirm) {
        return false;
    }
    const auto revision = ++entry->revision;
    const auto current = [&] {
        return detail::live(entry) && !entry->closing && entry->revision == revision;
    };
    if (patch.title) {
        connect(entry, entry->title, *patch.title);
    }
    if (patch.description && current()) {
        connect(entry, entry->description, *patch.description);
    }
    if (patch.ok_text && current()) {
        connect(entry, entry->ok, *patch.ok_text);
    }
    if (patch.cancel_text && current()) {
        connect(entry, entry->cancel, *patch.cancel_text);
    }
    return current();
}

bool AppPanelService::close(const std::shared_ptr<PanelHandleState>& state, PanelCloseResult result) {
    const auto entry = state ? state->entry.lock() : nullptr;
    if (!detail::live(entry)) {
        return false;
    }
    if (result.kind == PanelCloseKind::Closed || result.kind == PanelCloseKind::Cancelled) {
        return close_entry(entry, result);
    }
    finish(entry, result);
    return true;
}

std::shared_future<PanelCloseResult> AppPanelService::closed(const std::shared_ptr<PanelHandleState>& state) {
    if (state) {
        return state->future;
    }
    std::promise<PanelCloseResult> promise;
    promise.set_value({PanelCloseKind::OwnerDisposed});
    return promise.get_future().share();
}

void AppPanelService::retire_owner(const std::shared_ptr<AppBinding>& owner,
                                   std::optional<runtime::ComponentId> source) noexcept {
    // Snapshot entries and detach the entire batch before executing *any* user cancel.
    const auto entries = owner->panels;
    std::vector<std::pair<std::shared_ptr<PanelEntry>, ConfirmAction::Cancel>> retired;
    for (const auto& entry : entries) {
        if (!entry->retired &&
            (!source || entry->source == *source || entry->context->captured.component_owner == source)) {
            entry->retired = true;
            ++entry->revision;
            retired.emplace_back(entry, detach(entry));
            erase(entry);
            dispose_subscriptions(entry);
        }
    }
    for (auto& [entry, cancel] : retired) {
        if (!entry->context->captured.owner.expired()) {
            owner->services->retire_failed_component(entry->root);
        }
        settle(entry, {PanelCloseKind::OwnerDisposed});
    }
    for (auto& [entry, cancel] : retired) {
        ConfirmActionSession::run_cancel(std::move(cancel));
    }
}
} // namespace ryn::detail

namespace ryn {
bool ModalServiceRef::bound() const {
    return detail::AppPanelService::live(context_);
}

bool DrawerServiceRef::bound() const {
    return detail::AppPanelService::live(context_);
}

bool PopconfirmServiceRef::bound() const {
    return detail::AppPanelService::live(context_);
}

ModalHandle ModalServiceRef::confirm(ModalProps props, ModalContent content, ModalSlots slots) const {
    return detail::AppPanelService::modal(context_, std::move(props), std::move(content), std::move(slots));
}

ModalHandle ModalServiceRef::info(ModalProps props, ModalContent content, ModalSlots slots) const {
    return detail::AppPanelService::modal(context_, std::move(props), std::move(content), std::move(slots),
                                          IconName::InfoCircleFilled);
}

ModalHandle ModalServiceRef::success(ModalProps props, ModalContent content, ModalSlots slots) const {
    return detail::AppPanelService::modal(context_, std::move(props), std::move(content), std::move(slots),
                                          IconName::CheckCircleFilled);
}

ModalHandle ModalServiceRef::error(ModalProps props, ModalContent content, ModalSlots slots) const {
    return detail::AppPanelService::modal(context_, std::move(props), std::move(content), std::move(slots),
                                          IconName::CloseCircleFilled);
}

ModalHandle ModalServiceRef::warning(ModalProps props, ModalContent content, ModalSlots slots) const {
    return detail::AppPanelService::modal(context_, std::move(props), std::move(content), std::move(slots),
                                          IconName::ExclamationCircleFilled);
}

DrawerHandle DrawerServiceRef::open(DrawerProps props, DrawerContent content, DrawerSlots slots) const {
    return detail::AppPanelService::drawer(context_, std::move(props), std::move(content), std::move(slots));
}

PopconfirmHandle PopconfirmServiceRef::confirm(const PopconfirmRef& ref, PopconfirmProps props,
                                               PopconfirmSlots slots) const {
    return detail::AppPanelService::popconfirm(context_, ref, std::move(props), std::move(slots));
}

bool ModalHandle::bound() const {
    return detail::AppPanelService::live(state_);
}

bool DrawerHandle::bound() const {
    return detail::AppPanelService::live(state_);
}

bool PopconfirmHandle::bound() const {
    return detail::AppPanelService::live(state_);
}

bool ModalHandle::update(ModalUpdate patch) const {
    return detail::AppPanelService::update(state_, std::move(patch));
}

bool DrawerHandle::update(DrawerUpdate patch) const {
    return detail::AppPanelService::update(state_, std::move(patch));
}

bool PopconfirmHandle::update(PopconfirmUpdate patch) const {
    return detail::AppPanelService::update(state_, std::move(patch));
}

bool ModalHandle::close(ModalCancelReason reason) const {
    return detail::AppPanelService::close(state_, {PanelCloseKind::Cancelled, reason});
}

bool DrawerHandle::close(DrawerCancelReason reason) const {
    return detail::AppPanelService::close(state_, {PanelCloseKind::Closed, {}, reason});
}

bool PopconfirmHandle::close() const {
    return detail::AppPanelService::close(state_, {PanelCloseKind::Cancelled});
}

bool ModalHandle::destroy() const {
    return detail::AppPanelService::close(state_, {PanelCloseKind::Destroyed});
}

bool DrawerHandle::destroy() const {
    return detail::AppPanelService::close(state_, {PanelCloseKind::Destroyed});
}

bool PopconfirmHandle::destroy() const {
    return detail::AppPanelService::close(state_, {PanelCloseKind::Destroyed});
}

std::shared_future<PanelCloseResult> ModalHandle::closed() const {
    return detail::AppPanelService::closed(state_);
}

std::shared_future<PanelCloseResult> DrawerHandle::closed() const {
    return detail::AppPanelService::closed(state_);
}

std::shared_future<PanelCloseResult> PopconfirmHandle::closed() const {
    return detail::AppPanelService::closed(state_);
}
} // namespace ryn
