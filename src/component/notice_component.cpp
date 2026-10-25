#include "component/notice_component.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/button.hpp>
#include <ryn/icon.hpp>
#include <algorithm>
#include <array>

namespace ryn::detail {
namespace {
thread_local NoticeComponentHost* active_notices{};

struct HolderState final {
    bool message{};
    std::size_t max_count{8};
    NoticeModel model;
    std::shared_ptr<NoticeBinding> binding{std::make_shared<NoticeBinding>()};
    std::shared_ptr<Scope> max_scope{std::make_shared<Scope>()};
};

struct EntryState final {
    runtime::NodeId node;
    NoticeSnapshot snapshot;
    NoticeTimer timer;
    std::uint64_t revision{};
    std::function<void()> on_close;
    Signal<String> title{String{}};
    Signal<String> description{String{}};
    Signal<IconName> icon{IconName::InfoCircleFilled};
    Signal<runtime::SemanticForeground> text_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> description_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> icon_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> title_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> body_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> icon_font{runtime::SemanticTypography{}};
    std::array<std::shared_ptr<Scope>, 6> bindings;
    std::array<theme_runtime::Subscription, 4> subscriptions;
    std::array<runtime::Size, 3> measured;
    bool rich_description{};
    bool rich_actions{};
    component::RetainedSurfaceId surface;
};

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

const NoticeThemeToken& token(WindowComponentServices& services, runtime::ComponentId id, bool message) {
    const auto& snapshot = services.components().theme_scope(id)->snapshot();
    return message ? snapshot.message() : snapshot.notification();
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId root) {
    for (auto current = std::optional{child}; current && services.components().contains(*current);
         current = services.components().parent(*current)) {
        if (*current == root) {
            return true;
        }
    }
    return false;
}

runtime::ComponentId region_id(const NoticeSnapshot& s, unsigned index) {
    return index == 0 ? s.title : index == 1 ? s.description : s.actions;
}

void set_region(NoticeSnapshot& s, unsigned index, runtime::ComponentId id) {
    if (index == 0) {
        s.title = id;
    } else if (index == 1) {
        s.description = id;
    } else {
        s.actions = id;
    }
}
} // namespace

NoticeComponentHost::NoticeComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

NoticeComponentHost::~NoticeComponentHost() {
    services_->detach(*this);
}

void* NoticeComponentHost::begin_mount() noexcept {
    auto* previous = active_notices;
    active_notices = this;
    return previous;
}

void NoticeComponentHost::end_mount(void* previous) noexcept {
    active_notices = static_cast<NoticeComponentHost*>(previous);
}

void NoticeComponentHost::on_destroy() noexcept {
    std::erase_if(holders_, [this](auto id) { return !services_->components().contains(id); });
    for (auto id : holders_) {
        auto& model = services_->components().state<HolderState>(id)->model;
        std::erase_if(model.entries, [this](const auto& entry) { return !services_->components().contains(entry.id); });
    }
}

void NoticeComponentHost::on_dispose() noexcept {
    holders_.clear();
}

animation::AnimationTime NoticeComponentHost::now() {
    return clock_.observe(services_->animation_time()).effective;
}

std::vector<NoticeEntry> NoticeComponentHost::entries(runtime::ComponentId id) const {
    const auto* s = services_->components().state<HolderState>(id);
    return s ? s->model.entries : std::vector<NoticeEntry>{};
}

NoticeSnapshot NoticeComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<EntryState>(id);
    if (!s) {
        throw std::out_of_range("Stale notice");
    }
    return s->snapshot;
}

bool NoticeComponentHost::visible() const {
    for (auto id : holders_) {
        const auto* s = services_->components().state<HolderState>(id);
        if (s && !s->model.entries.empty() && services_->components().branch_active(id)) {
            return true;
        }
    }
    return false;
}

void NoticeComponentHost::mount(const MessageHolderProps& props) {
    mount_holder(true, props.reference_, props.max_count_);
}

void NoticeComponentHost::mount(const NotificationHolderProps& props) {
    mount_holder(false, props.reference_, props.max_count_);
}

void NoticeComponentHost::mount_holder(bool message, const std::shared_ptr<NoticeRefState>& reference,
                                       const Prop<std::size_t>& max_count) {
    if (!reference) {
        throw std::invalid_argument("Notice holder requires a Ref");
    }
    if (const auto current = reference->binding.lock(); current && current->alive) {
        throw std::invalid_argument("Notice Ref already bound");
    }
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<HolderState>();
    auto& state = build.state<HolderState>(id);
    state.message = message;
    const auto binding = state.binding;
    const auto max_scope = state.max_scope;
    auto& services = *services_;
    const auto node = build.root(id);
    build.scope(id).on_cleanup([binding] { binding->alive = false; });
    services.nodes().require(node).out_of_flow = true;
    services.layout().set_layout(node, layout::LeafLayout{});
    build.on_resource_cleanup(id, [binding, max_scope, &services, node] {
        binding->alive = false;
        max_scope->dispose();
        services.layout().remove_layout(node);
    });
    binding->open = [this, id, weak = std::weak_ptr{binding}](String key, NoticeOptions props, NoticeContent content) {
        const auto alive = weak.lock();
        return alive && alive->alive && open(id, std::move(key), std::move(props), std::move(content));
    };
    binding->close = [this, id, weak = std::weak_ptr{binding}](String key) {
        const auto alive = weak.lock();
        const auto* s = alive && alive->alive ? services_->components().state<HolderState>(id) : nullptr;
        return s && close(id, s->model.find(key));
    };
    binding->clear = [this, id, weak = std::weak_ptr{binding}] {
        const auto alive = weak.lock();
        return alive && alive->alive && clear(id);
    };
    reference->binding = binding;
    holders_.push_back(id);
    connect_prop(*max_scope, max_count, [this, id](std::size_t value) {
        if (!value || value > 1024) {
            throw std::invalid_argument("Notice maxCount must be in [1, 1024]");
        }
        if (auto* s = services_->components().state<HolderState>(id)) {
            s->max_count = value;
            trim(id);
        }
    });
}

runtime::ComponentId NoticeComponentHost::region(runtime::ComponentId id, unsigned index, const Content& content) {
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto child = build.mount_component<int>(0);
    const auto node = build.root(child);
    services.nodes().require(node).clip_content = true;
    services.layout().set_layout(node, layout::FlexLayout{layout::FlexDirection::vertical, 0, {}, true});
    build.on_resource_cleanup(child, [&services, node] { services.layout().remove_layout(node); });
    auto* s = services.components().state<EntryState>(id);
    if (!s) {
        return child;
    }
    const auto color = index == 1 ? s->description_color : s->text_color;
    const auto font = index == 0 ? s->title_font : s->body_font;
    build.mount_slot_with_semantic_text_style(child, content, Prop<runtime::SemanticForeground>{color},
                                              Prop<runtime::SemanticTypography>{font});
    return child;
}

runtime::ComponentId NoticeComponentHost::create(runtime::ComponentId owner, String key) {
    auto& services = *services_;
    runtime::ComponentId id;
    services.append_slot(
        owner, Content{[&, key = std::move(key)] {
            auto& build = runtime::require_component_build_context();
            auto* holder = services.components().state<HolderState>(owner);
            if (!holder) {
                return;
            }
            const bool message = holder->message;
            id = build.mount_component<EntryState>();
            auto& state = build.state<EntryState>(id);
            state.snapshot.owner = owner;
            state.snapshot.message = message;
            state.snapshot.closable = !message;
            state.timer.seconds = message ? 3 : 4.5;
            state.node = build.root(id);
            const auto node = state.node;
            services.nodes().require(node).out_of_flow = true;
            services.nodes().require(node).clip_content = true;
            services.components().set_window_layer(id, token(services, id, message).z_index);
            state.snapshot.interaction = services.interactions().create({id, node, {}, true, false, {}, false, false});
            const auto interaction = state.snapshot.interaction;
            const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
            graphics::QuadInstance initial;
            initial.opacity = 0;
            state.surface = services.surfaces().create(id, node, fragment, interaction, std::span{&initial, 1});
            const auto surface = state.surface;
            build.on_resource_cleanup(id, [&services, node, surface, interaction, id] {
                services.interactions().set_input_blocked(id, false);
                services.interactions().remove(interaction);
                services.surfaces().destroy(surface);
                services.layout().remove_layout(node);
            });
            build.on_resource_cleanup(id, [this, id] {
                if (auto* s = services_->components().state<EntryState>(id)) {
                    for (auto& scope : s->bindings) {
                        if (scope) {
                            scope->dispose();
                        }
                    }
                }
            });
            holder->model.add(key, id);
            build.mount_slot(
                id, Content{[&, id] {
                    auto& child_build = runtime::require_component_build_context();
                    auto* s = services.components().state<EntryState>(id);
                    if (!s) {
                        return;
                    }
                    const auto title = s->title;
                    const auto description = s->description;
                    const auto title_region = region(id, 0, Content{[title] { Text(TextProps{}.content(title)); }});
                    if (!(s = services.components().state<EntryState>(id))) {
                        return;
                    }
                    s->snapshot.title = title_region;
                    const auto desc_region =
                        region(id, 1, Content{[description] { Text(TextProps{}.content(description)); }});
                    if (!(s = services.components().state<EntryState>(id))) {
                        return;
                    }
                    s->snapshot.description = desc_region;
                    s->snapshot.actions = region(id, 2, Content{[] {}});
                    const auto icon_region = child_build.mount_component<int>(0);
                    const auto icon_node = child_build.root(icon_region);
                    services.layout().set_layout(icon_node, layout::BoxLayout{});
                    child_build.on_resource_cleanup(
                        icon_region, [&services, icon_node] { services.layout().remove_layout(icon_node); });
                    s->snapshot.icon = icon_region;
                    const auto icon = s->icon;
                    child_build.mount_slot_with_semantic_text_style(icon_region,
                                                                    Content{[icon] { Icon(IconProps{}.name(icon)); }},
                                                                    Prop<runtime::SemanticForeground>{s->icon_color},
                                                                    Prop<runtime::SemanticTypography>{s->icon_font});
                    const auto close_region = child_build.mount_component<int>(0);
                    const auto close_node = child_build.root(close_region);
                    services.nodes().require(close_node).clip_content = true;
                    services.layout().set_layout(close_node, layout::BoxLayout{{}, true});
                    child_build.on_resource_cleanup(close_region, [&services, close_node, close_region] {
                        services.interactions().set_input_blocked(close_region, false);
                        services.layout().remove_layout(close_node);
                    });
                    s->snapshot.close = close_region;
                    child_build.mount_slot(
                        close_region, Content{[this, id, owner] {
                            Button(ButtonProps{}
                                       .variant(ButtonVariant::Text)
                                       .size(ControlSize::Small)
                                       .onClick([this, id, owner] {
                                           const auto* current = services_->components().state<EntryState>(id);
                                           if (current && current->snapshot.closable &&
                                               services_->interactions().component_active(id)) {
                                               close(owner, id);
                                           }
                                       }),
                                   ButtonContent{[] { Icon(IconProps{}.name(IconName::CloseOutlined)); }});
                        }});
                }});
            if (!services.components().contains(id)) {
                return;
            }
            services.layout().set_layout(
                node,
                layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        auto& s = *services_->components().state<EntryState>(id);
                        const auto& m = token(*services_, id, s.snapshot.message).metrics;
                        const float padding_x = std::min(m.padding_inline, limits.max_width / 2);
                        const float padding_y = std::min(m.padding_block, limits.max_height / 2);
                        const float close_width = s.snapshot.closable ? m.close_size + m.gap : 0;
                        const float leading = m.icon_size + m.gap;
                        const float width = std::max(0.0F, limits.max_width - padding_x * 2 - close_width - leading);
                        const float height = std::max(0.0F, limits.max_height - padding_y * 2);
                        const bool description =
                            !s.snapshot.message && (s.rich_description || !s.description.get().empty());
                        const std::array<bool, 3> shown{true, description, !s.snapshot.message && s.rich_actions};
                        float used = 0;
                        float content_width = 0;
                        for (unsigned i = 0; i < 3; ++i) {
                            s.measured[i] = {};
                            if (!shown[i]) {
                                static_cast<void>(
                                    engine.measure_child(services_->components().root(region_id(s.snapshot, i)),
                                                         layout::Constraints::fixed(0, 0)));
                                continue;
                            }
                            if (used > 0) {
                                used += m.gap;
                            }
                            const auto region = region_id(s.snapshot, i);
                            s.measured[i] = engine.measure_child(services_->components().root(region),
                                                                 {0, width, 0, std::max(0.0F, height - used)});
                            used += s.measured[i].height;
                            content_width = std::max(content_width, s.measured[i].width);
                        }
                        static_cast<void>(
                            engine.measure_child(services_->components().root(s.snapshot.icon),
                                                 layout::Constraints::fixed(std::min(m.icon_size, limits.max_width),
                                                                            std::min(m.icon_size, height))));
                        static_cast<void>(
                            engine.measure_child(services_->components().root(s.snapshot.close),
                                                 layout::Constraints::fixed(
                                                     s.snapshot.closable ? std::min(m.close_size, limits.max_width) : 0,
                                                     std::min(m.close_size, height))));
                        const float wanted_width = s.snapshot.message
                                                       ? content_width + leading + close_width + padding_x * 2
                                                       : limits.max_width;
                        return limits.constrain(
                            {wanted_width,
                             std::max(used, std::max(m.icon_size, s.snapshot.closable ? m.close_size : 0.0F)) +
                                 padding_y * 2});
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect r) {
                        const auto& s = *services_->components().state<EntryState>(id);
                        const auto& m = token(*services_, id, s.snapshot.message).metrics;
                        const float px = std::min(m.padding_inline, r.width / 2);
                        const float py = std::min(m.padding_block, r.height / 2);
                        const float inner_height = std::max(0.0F, r.height - py * 2);
                        const float icon_width = std::min(m.icon_size, std::max(0.0F, r.width - px * 2));
                        const float close_width =
                            s.snapshot.closable ? std::min(m.close_size, std::max(0.0F, r.width - px * 2 - icon_width))
                                                : 0;
                        const float x = r.x + px + icon_width + m.gap;
                        const float width = std::max(0.0F, r.width - px * 2 - icon_width - m.gap -
                                                               (s.snapshot.closable ? close_width + m.gap : 0));
                        float y = r.y + py;
                        for (unsigned i = 0; i < 3; ++i) {
                            const auto size = s.measured[i];
                            engine.place_child(services_->components().root(region_id(s.snapshot, i)),
                                               {x, y, width, size.height}, true, true);
                            if (size.height > 0) {
                                y += size.height + m.gap;
                            }
                        }
                        engine.place_child(services_->components().root(s.snapshot.icon),
                                           {r.x + px, r.y + py, icon_width, std::min(m.icon_size, inner_height)}, true,
                                           true);
                        engine.place_child(services_->components().root(s.snapshot.close),
                                           {r.x + r.width - px - close_width, r.y + py, close_width,
                                            std::min(m.close_size, inner_height)},
                                           true, true);
                    }});
            const auto theme = services.components().theme_scope(id);
            auto* s = services.components().state<EntryState>(id);
            for (unsigned i = 0; i < 4; ++i) {
                s->subscriptions[i] = theme->capture([this, id, i](auto) { refresh(id, i == 1 || i == 3); },
                                                     [theme, message, i] {
                                                         if (message) {
                                                             if (i == 0) {
                                                                 static_cast<void>(theme->message_colors());
                                                             } else if (i == 1) {
                                                                 static_cast<void>(theme->message_metrics());
                                                             } else if (i == 2) {
                                                                 static_cast<void>(theme->message_effects());
                                                             } else {
                                                                 static_cast<void>(theme->message_order());
                                                             }
                                                         } else {
                                                             if (i == 0) {
                                                                 static_cast<void>(theme->notification_colors());
                                                             } else if (i == 1) {
                                                                 static_cast<void>(theme->notification_metrics());
                                                             } else if (i == 2) {
                                                                 static_cast<void>(theme->notification_effects());
                                                             } else {
                                                                 static_cast<void>(theme->notification_order());
                                                             }
                                                         }
                                                     });
            }
        }});
    refresh(id, true);
    return id;
}

template <typename T, typename F>
void NoticeComponentHost::connect(runtime::ComponentId id, std::size_t index, const std::optional<Prop<T>>& prop,
                                  F apply) {
    if (!prop) {
        return;
    }
    auto* s = services_->components().state<EntryState>(id);
    if (!s) {
        return;
    }
    auto connection = std::make_shared<Scope>();
    if (s->bindings[index]) {
        s->bindings[index]->dispose();
    }
    s->bindings[index] = connection;
    connect_prop(*connection, *prop, [this, id, index, weak = std::weak_ptr{connection}, apply](T value) {
        const auto scope = weak.lock();
        const auto* current = services_->components().state<EntryState>(id);
        if (!scope || !scope->active() || !current || !services_->components().scope(id).active() ||
            current->bindings[index] != scope) {
            return;
        }
        apply(std::move(value));
        refresh(id, true);
    });
}

bool NoticeComponentHost::open(runtime::ComponentId owner, String key, NoticeOptions props, NoticeContent content) {
    if (key.empty()) {
        throw std::invalid_argument("Notice key must not be empty");
    }
    auto* holder = services_->components().state<HolderState>(owner);
    if (!holder || !holder->binding->alive || !services_->components().scope(owner).active()) {
        return false;
    }
    auto id = holder->model.find(key);
    const bool created = !id.valid();
    if (created) {
        id = create(owner, key);
    }
    auto* s = services_->components().state<EntryState>(id);
    if (!s) {
        return false;
    }
    const auto revision = ++s->revision;
    const auto current = [this, id, revision] {
        const auto* entry = services_->components().state<EntryState>(id);
        return entry && entry->revision == revision;
    };
    try {
        if (props.on_close) {
            s->on_close = *props.on_close;
        }
        connect(id, 0, props.title, [this, id](String value) {
            if (auto* entry = services_->components().state<EntryState>(id)) {
                auto signal = entry->title;
                signal.set(std::move(value));
            }
        });
        if (!current()) {
            return false;
        }
        connect(id, 1, props.description, [this, id](String value) {
            if (auto* entry = services_->components().state<EntryState>(id)) {
                auto signal = entry->description;
                signal.set(std::move(value));
            }
        });
        if (!current()) {
            return false;
        }
        connect(id, 2, props.status, [this, id](NoticeStatus value) {
            validate_notice_status(value);
            if (auto* entry = services_->components().state<EntryState>(id)) {
                entry->snapshot.status = value;
            }
        });
        if (!current()) {
            return false;
        }
        connect(id, 3, props.placement, [this, id](NotificationPlacement value) {
            validate_notice_placement(value);
            if (auto* entry = services_->components().state<EntryState>(id)) {
                entry->snapshot.placement = value;
            }
        });
        if (!current()) {
            return false;
        }
        connect(id, 4, props.duration, [this, id](double value) {
            if (auto* entry = services_->components().state<EntryState>(id)) {
                entry->timer.reset(now(), value);
            }
        });
        if (!current()) {
            return false;
        }
        connect(id, 5, props.closable, [this, id](bool value) {
            if (auto* entry = services_->components().state<EntryState>(id)) {
                entry->snapshot.closable = value;
            }
        });
        if (!current()) {
            return false;
        }
        const std::array<std::optional<Content>, 3> replacements{content.title, content.description, content.actions};
        for (unsigned index = 0; index < 3; ++index) {
            if (!replacements[index]) {
                continue;
            }
            s = services_->components().state<EntryState>(id);
            const auto old = region_id(s->snapshot, index);
            runtime::ComponentId replacement;
            services_->append_slot(id, Content{[&, index] { replacement = region(id, index, *replacements[index]); }});
            if (!current()) {
                return false;
            }
            s = services_->components().state<EntryState>(id);
            set_region(s->snapshot, index, replacement);
            if (index == 1) {
                s->rich_description = true;
            }
            if (index == 2) {
                s->rich_actions = true;
            }
            services_->destroy(old);
            if (!current()) {
                return false;
            }
        }
        s = services_->components().state<EntryState>(id);
        s->timer.reset(now(), s->timer.seconds);
        refresh(id, true);
        if (!current()) {
            return false;
        }
        trim(owner);
        services_->request_frame();
        return services_->components().contains(id);
    } catch (...) {
        if (created && services_->components().contains(id)) {
            services_->destroy(id);
        }
        throw;
    }
}

bool NoticeComponentHost::close(runtime::ComponentId owner, runtime::ComponentId id) {
    auto* holder = services_->components().state<HolderState>(owner);
    const auto* s = services_->components().state<EntryState>(id);
    if (!holder || !holder->binding->alive || !s || !services_->components().scope(id).active() ||
        s->snapshot.owner != owner) {
        return false;
    }
    const auto callback = s->on_close;
    holder->model.erase(id);
    services_->destroy(id);
    services_->focus().synchronize();
    services_->request_frame();
    if (callback) {
        callback();
    }
    return true;
}

bool NoticeComponentHost::clear(runtime::ComponentId owner) {
    if (!services_->components().contains(owner)) {
        return false;
    }
    const auto batch = entries(owner);
    for (const auto& item : batch) {
        close(owner, item.id);
    }
    return true;
}

void NoticeComponentHost::trim(runtime::ComponentId owner) {
    const auto* holder = services_->components().state<HolderState>(owner);
    if (!holder || holder->model.entries.size() <= holder->max_count) {
        return;
    }
    const auto count = holder->model.entries.size() - holder->max_count;
    const auto batch = holder->model.entries;
    for (std::size_t index = 0; index < count; ++index) {
        close(owner, batch[index].id);
    }
}

void NoticeComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<EntryState>(id);
    if (!s) {
        return;
    }
    const auto& t = token(*services_, id, s->snapshot.message);
    const auto& m = t.metrics;
    const auto status = s->snapshot.status;
    const std::array<IconName, 5> icons{IconName::CheckCircleFilled, IconName::InfoCircleFilled,
                                        IconName::ExclamationCircleFilled, IconName::CloseCircleFilled,
                                        IconName::LoadingOutlined};
    const std::array<Color, 5> colors{t.colors.success, t.colors.info, t.colors.warning, t.colors.error,
                                      t.colors.loading};
    s->icon.set(icons[static_cast<unsigned>(status)]);
    s->text_color.set(foreground(t.colors.text));
    s->description_color.set(foreground(t.colors.description));
    s->icon_color.set(foreground(colors[static_cast<unsigned>(status)]));
    s->title_font.set({m.font_family, m.title_font_weight, false, m.title_font_size, m.title_line_height});
    s->body_font.set({m.font_family, 400, false, m.font_size, m.line_height});
    s->icon_font.set({m.font_family, 400, false, m.icon_size, m.icon_size});
    const auto close_region = s->snapshot.close;
    if (services_->components().set_branch_active(close_region, s->snapshot.closable)) {
        services_->mark_scene_structure_dirty();
    }
    if (services_->components().set_window_layer(id, t.z_index)) {
        services_->mark_scene_structure_dirty();
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
    // This is the only callback boundary. No state is accessed afterwards.
    services_->set_input_blocked(close_region, !s->snapshot.closable);
}

void NoticeComponentHost::position_window_layers(runtime::Size, runtime::Rect clip) {
    for (const auto owner : holders_) {
        const auto* holder = services_->components().state<HolderState>(owner);
        if (!holder) {
            continue;
        }
        std::array<float, 4> offsets{};
        for (const auto& item : holder->model.entries) {
            const auto* s = services_->components().state<EntryState>(item.id);
            if (!s) {
                continue;
            }
            const auto& m = token(*services_, item.id, s->snapshot.message).metrics;
            const auto group = s->snapshot.message ? 0U : static_cast<unsigned>(s->snapshot.placement);
            const float margin = std::min(m.offset, std::min(clip.width, clip.height) / 2);
            const float max_width = std::max(0.0F, clip.width - margin * 2);
            const float max_height = std::max(0.0F, clip.height - margin * 2 - offsets[group]);
            const float width = std::min(m.width, max_width);
            const auto size =
                services_->layout().measure_child(s->node, {s->snapshot.message ? 0.0F : width, width, 0, max_height});
            const bool right = s->snapshot.placement == NotificationPlacement::TopRight ||
                               s->snapshot.placement == NotificationPlacement::BottomRight;
            const bool bottom = s->snapshot.placement == NotificationPlacement::BottomLeft ||
                                s->snapshot.placement == NotificationPlacement::BottomRight;
            const float x = s->snapshot.message ? (clip.width - size.width) / 2
                            : right             ? clip.width - margin - size.width
                                                : margin;
            const float y = !s->snapshot.message && bottom ? clip.height - margin - offsets[group] - size.height
                                                           : margin + offsets[group];
            services_->layout().place_child(s->node,
                                            {clip.x + std::clamp(x, 0.0F, std::max(0.0F, clip.width - size.width)),
                                             clip.y + std::clamp(y, 0.0F, std::max(0.0F, clip.height - size.height)),
                                             size.width, size.height});
            offsets[group] += size.height + m.stack_gap;
        }
    }
}

void NoticeComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (const auto owner : holders_) {
        for (const auto& item : entries(owner)) {
            const auto* s = services_->components().state<EntryState>(item.id);
            if (!s) {
                continue;
            }
            const auto& t = token(*services_, item.id, s->snapshot.message);
            const auto& node = services_->nodes().require(s->node);
            graphics::QuadInstance q;
            q.bounds = {node.bounds.x, node.bounds.y, node.bounds.width, node.bounds.height};
            q.color = foreground(t.colors.background);
            q.corner_radius = std::min(t.metrics.border_radius, std::min(node.bounds.width, node.bounds.height) / 2);
            q.opacity = node.opacity;
            q.translation = {node.translation.x, node.translation.y};
            static_cast<void>(services_->surfaces().update(s->surface, std::span{&q, 1}));
            component::RetainedSurfaceEffects effects;
            effects.shape = {node.bounds.x, node.bounds.y, node.bounds.width, node.bounds.height, q.corner_radius};
            effects.shadows = t.shadow;
            effects.focus_enabled = false;
            effects.translation = node.translation;
            const auto clip = services_->component_clip(item.id, viewport, document_clip);
            effects.ancestor_clip = graphics::EffectClip{1, clip};
            static_cast<void>(services_->surfaces().update_effects(s->surface, effects));
        }
    }
}

std::size_t NoticeComponentHost::tick_auxiliary(animation::AnimationTime time) {
    const auto effective = clock_.observe(time).effective;

    struct Expired final {
        runtime::ComponentId owner;
        runtime::ComponentId id;
        std::uint64_t revision;
    };

    std::vector<Expired> expired;
    for (auto owner : holders_) {
        for (const auto& item : entries(owner)) {
            const auto* s = services_->components().state<EntryState>(item.id);
            if (s && s->timer.deadline && *s->timer.deadline <= effective) {
                expired.push_back({owner, item.id, s->revision});
            }
        }
    }
    std::size_t changed = 0;
    for (const auto& item : expired) {
        const auto* s = services_->components().state<EntryState>(item.id);
        if (s && s->revision == item.revision && s->timer.deadline && *s->timer.deadline <= effective) {
            changed += close(item.owner, item.id);
        }
    }
    return changed;
}

std::optional<animation::AnimationTime> NoticeComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> next;
    for (auto owner : holders_) {
        const auto* holder = services_->components().state<HolderState>(owner);
        if (!holder) {
            continue;
        }
        for (const auto& item : holder->model.entries) {
            const auto* s = services_->components().state<EntryState>(item.id);
            if (s && s->timer.deadline && (!next || *s->timer.deadline < *next)) {
                next = s->timer.deadline;
            }
        }
    }
    return next;
}

bool NoticeComponentHost::consume_scroll_boundary(const input::ScrollInputEvent& event) {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
    if (!record) {
        return false;
    }
    for (auto owner : holders_) {
        for (const auto& item : entries(owner)) {
            if (descendant(*services_, record->component, item.id) &&
                services_->interactions().component_active(item.id)) {
                return true;
            }
        }
    }
    return false;
}
} // namespace ryn::detail

namespace ryn {
void MessageHolder(MessageHolderProps props) {
    if (!detail::active_notices) {
        throw std::logic_error("MessageHolder requires a Window scope");
    }
    detail::active_notices->mount(props);
}

void NotificationHolder(NotificationHolderProps props) {
    if (!detail::active_notices) {
        throw std::logic_error("NotificationHolder requires a Window scope");
    }
    detail::active_notices->mount(props);
}
} // namespace ryn
