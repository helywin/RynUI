#include "component/segmented_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace ryn::detail {
namespace {
thread_local SegmentedComponentHost* active_segmented{};

struct ItemState final {
    runtime::NodeId node;
    runtime::NodeId label_node;
    std::optional<runtime::NodeId> icon_node;
    input::InteractionId interaction;
    component::RetainedSurfaceId decorations;
    String key;
    Signal<String> label{String{}};
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    bool disabled{};
    runtime::Size icon_size;
    runtime::Size label_size;
    runtime::Size natural;
    runtime::Size allocated;
};

struct SegmentedState final {
    runtime::NodeId node;
    String value;
    bool controlled{};
    bool disabled{};
    bool block{};
    bool vertical{};
    ControlSize size{ControlSize::Middle};
    std::vector<runtime::ComponentId> items;
    component::RetainedSurfaceId decorations;
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    std::optional<std::size_t> hovered;
    std::optional<std::size_t> pressed;
    std::optional<std::size_t> keyboard_pressed;
    runtime::Rect clip;
    std::array<theme_runtime::Subscription, 3> subscriptions;
    std::function<void(String)> on_change;
};

void validate(ControlSize size) {
    if (size != ControlSize::Small && size != ControlSize::Middle && size != ControlSize::Large) {
        throw std::invalid_argument("Segmented size is invalid");
    }
}

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

float size_value(ControlSize size, float small, float middle, float large) {
    return size == ControlSize::Small ? small : size == ControlSize::Large ? large : middle;
}

std::optional<std::size_t> selected(WindowComponentServices& services, const SegmentedState& s) {
    for (std::size_t i = 0; i < s.items.size(); ++i) {
        if (services.components().state<ItemState>(s.items[i])->key == s.value) {
            return i;
        }
    }
    return {};
}

bool enabled(WindowComponentServices& services, const SegmentedState& s, std::size_t i) {
    return !s.disabled && !services.components().state<ItemState>(s.items[i])->disabled;
}

std::optional<std::size_t> focused(WindowComponentServices& services, const SegmentedState& s) {
    const auto focus = services.focus().state().focused;
    for (std::size_t i = 0; focus && i < s.items.size(); ++i) {
        const auto& row = *services.components().state<ItemState>(s.items[i]);
        if (row.interaction == *focus && enabled(services, s, i) &&
            services.interactions().branch_active(row.interaction)) {
            return i;
        }
    }
    return {};
}

std::optional<std::size_t> edge(WindowComponentServices& services, const SegmentedState& s, bool last = false) {
    for (std::size_t offset = 0; offset < s.items.size(); ++offset) {
        const auto i = last ? s.items.size() - 1 - offset : offset;
        if (enabled(services, s, i)) {
            return i;
        }
    }
    return {};
}

runtime::Rect translated(const runtime::Node& node) {
    return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
            node.bounds.height};
}

bool inside(runtime::Rect b, float x, float y) {
    return x >= b.x && y >= b.y && x < b.x + b.width && y < b.y + b.height;
}

auto fill(runtime::Rect b, float radius, Color color, float opacity, runtime::Point translation,
          graphics::EffectClip clip) {
    return graphics::make_corner_fill_effects({b, std::min(radius, std::min(b.width, b.height) / 2)},
                                              {true, true, true, true}, color, opacity, translation, clip);
}
} // namespace

SegmentedComponentHost::SegmentedComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

SegmentedComponentHost::~SegmentedComponentHost() {
    services_->detach(*this);
}

void* SegmentedComponentHost::begin_mount() noexcept {
    auto* previous = active_segmented;
    active_segmented = this;
    return previous;
}

void SegmentedComponentHost::end_mount(void* previous) noexcept {
    active_segmented = static_cast<SegmentedComponentHost*>(previous);
}

void SegmentedComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

SegmentedSnapshot SegmentedComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<SegmentedState>(id);
    if (!s) {
        throw std::out_of_range("Segmented component is stale");
    }
    return {s->value, selected(*services_, *s), s->items};
}

void SegmentedComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<SegmentedState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().segmented();
    const auto& m = t.metrics;
    const float label_height =
        std::max(1.0F, size_value(s->size, m.height_small, m.height, m.height_large) - m.track_padding * 2);
    s->font.set({m.font_family, 400, false, s->size == ControlSize::Large ? m.font_size_large : m.font_size,
                 std::min(label_height, s->size == ControlSize::Large ? m.line_height_large : m.line_height)});
    const auto active = selected(*services_, *s);
    auto entry = focused(*services_, *s);
    if (!entry && active && enabled(*services_, *s, *active)) {
        entry = active;
    }
    if (!entry) {
        entry = edge(*services_, *s);
    }
    for (std::size_t i = 0; i < s->items.size(); ++i) {
        auto& row = *services_->components().state<ItemState>(s->items[i]);
        const bool eligible = enabled(*services_, *s, i);
        services_->interactions().set_eligible(row.interaction, eligible);
        services_->interactions().set_tab_stop(row.interaction, entry == i);
        row.foreground.set(foreground(!eligible     ? t.colors.disabled
                                      : active == i ? t.colors.selected
                                      : s->hovered == i || s->pressed == i || s->keyboard_pressed == i
                                          ? t.colors.hover
                                          : t.colors.item));
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
}

void SegmentedComponentHost::activate(runtime::ComponentId id, std::size_t index) {
    auto* s = services_->components().state<SegmentedState>(id);
    if (!s || index >= s->items.size() || !enabled(*services_, *s, index) ||
        !services_->interactions().component_active(id)) {
        return;
    }
    const auto& row = *services_->components().state<ItemState>(s->items[index]);
    if (!services_->interactions().branch_active(row.interaction) || row.key == s->value) {
        return;
    }
    const auto key = row.key;
    const auto callback = s->on_change;
    if (!s->controlled) {
        s->value = key;
    }
    refresh(id, false);
    if (callback) {
        callback(key);
    }
}

void SegmentedComponentHost::mount(const SegmentedProps& props, const std::vector<SegmentedOption>& options) {
    validate(read_prop(props.size_));
    if (props.value_ && props.default_value_) {
        throw std::invalid_argument("Segmented value and defaultValue are mutually exclusive");
    }
    std::unordered_set<std::string_view> keys;
    for (const auto& option : options) {
        if (option.key_.empty() || !keys.insert(option.key_.bytes()).second) {
            throw std::invalid_argument("Segmented keys must be nonempty and unique");
        }
    }
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<SegmentedState>();
    auto& s = build.state<SegmentedState>(id);
    s.node = build.root(id);
    s.controlled = props.value_.has_value();
    s.value = props.value_ ? read_prop(*props.value_) : props.default_value_.value_or(String{});
    s.disabled = read_prop(props.disabled_);
    s.block = read_prop(props.block_);
    s.vertical = read_prop(props.vertical_);
    s.size = read_prop(props.size_);
    s.on_change = props.on_change_;
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.decorations = services.surfaces().create_content_range(fragment, {});
    build.on_resource_cleanup(id, [&services, node = s.node, decorations = s.decorations] {
        services.surfaces().destroy_content_range(decorations);
        services.layout().remove_layout(node);
    });
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            for (std::size_t index = 0; index < options.size(); ++index) {
                const auto& option = options[index];
                const auto component = context.mount_component<ItemState>();
                auto& row = context.state<ItemState>(component);
                row.node = context.root(component);
                row.key = option.key_;
                row.label.set(read_prop(option.label_));
                row.disabled = read_prop(option.disabled_);
                row.interaction = services.interactions().create(
                    {component, row.node, {}, !s.disabled && !row.disabled, true, {}, true, false});
                s.items.push_back(component);
                services.layout().set_layout(
                    row.node,
                    layout::ComponentLayout{
                        [this, id, index](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                            return measure_item(id, index, engine, limits);
                        },
                        [this, id, index](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                            place_item(id, index, engine, bounds);
                        }});
                const auto hit =
                    context.register_scene_fragment(component, runtime::SceneFragmentPlacement::before_children);
                services.scene_composer().set_fragment(hit, {}, row.interaction);
                const auto decoration =
                    context.register_scene_fragment(component, runtime::SceneFragmentPlacement::before_children);
                row.decorations = services.surfaces().create_content_range(decoration, {});
                context.on_resource_cleanup(component, [&services, node = row.node, interaction = row.interaction, hit,
                                                        decorations = row.decorations] {
                    services.pointer().cancel_interaction(interaction);
                    services.focus().cancel_interaction(interaction);
                    services.interactions().remove(interaction);
                    services.scene_composer().remove_fragment(hit);
                    services.surfaces().destroy_content_range(decorations);
                    services.layout().remove_layout(node);
                });
                input::InteractionHandlers pointer;
                pointer.target = [this, id, index](input::PointerDispatchContext& event) {
                    auto* current = services_->components().state<SegmentedState>(id);
                    if (!current) {
                        return;
                    }
                    if (event.kind() == input::PointerEventKind::cancel) {
                        current->pressed.reset();
                        refresh(id, false);
                        return;
                    }
                    if (!enabled(*services_, *current, index) || !services_->interactions().component_active(id)) {
                        return;
                    }
                    if (event.event().button != input::PointerButton::primary) {
                        return;
                    }
                    if (event.kind() == input::PointerEventKind::down) {
                        current->pressed = index;
                        static_cast<void>(event.capture_pointer());
                        event.stop_propagation();
                        refresh(id, false);
                    } else if (event.kind() == input::PointerEventKind::up) {
                        const auto& item = *services_->components().state<ItemState>(current->items[index]);
                        const auto& node = services_->nodes().require(item.node);
                        const auto clip = services_->nodes().content_clip(item.node, current->clip);
                        const bool hit = event.press_origin() == item.interaction && current->pressed == index &&
                                         inside(graphics::intersect_effect_bounds(translated(node), clip),
                                                event.event().x, event.event().y);
                        current->pressed.reset();
                        static_cast<void>(event.release_pointer_capture());
                        event.stop_propagation();
                        refresh(id, false);
                        if (hit) {
                            activate(id, index);
                        }
                    }
                };
                services.interactions().set_handlers(row.interaction, std::move(pointer));
                input::FocusHandlers focus;
                focus.state_changed = [this, id, index](input::FocusPresentation state) {
                    if (auto* current = services_->components().state<SegmentedState>(id)) {
                        if (!state.focused && current->keyboard_pressed == index) {
                            current->keyboard_pressed.reset();
                        }
                        refresh(id, false);
                    }
                };
                services.interactions().set_focus_handlers(row.interaction, std::move(focus));
                context.mount_slot_with_semantic_text_style(
                    component, Content{[&] {
                        auto& children = runtime::require_component_build_context();
                        const auto wrapper = [&](const Content& content) {
                            const auto child = children.mount_component<int>(0);
                            const auto node = children.root(child);
                            // Only the item fills its allocation; slots report their constrained intrinsic size.
                            services.layout().set_layout(node, layout::BoxLayout{});
                            children.on_resource_cleanup(child,
                                                         [&services, node] { services.layout().remove_layout(node); });
                            children.mount_slot(child, content);
                            return node;
                        };
                        if (option.icon_) {
                            row.icon_node =
                                wrapper(Content{[icon = *option.icon_] { SlotContentAccess::function(icon)(); }});
                        }
                        row.label_node =
                            wrapper(option.content_
                                        ? Content{[label = *option.content_] { SlotContentAccess::function(label)(); }}
                                        : Content{[label = row.label] { Text(TextProps{}.content(label)); }});
                    }},
                    Prop<runtime::SemanticForeground>{row.foreground}, Prop<runtime::SemanticTypography>{s.font});
            }
        }});
    if (!props.value_ && !props.default_value_) {
        for (auto item : s.items) {
            const auto& row = *services.components().state<ItemState>(item);
            if (!row.disabled) {
                s.value = row.key;
                break;
            }
        }
    }
    services.layout().set_layout(
        s.node, layout::ComponentLayout{[this, id](layout::LayoutEngine& engine, runtime::NodeId,
                                                   layout::Constraints limits) { return measure(id, engine, limits); },
                                        [this, id](layout::LayoutEngine& engine, runtime::NodeId,
                                                   runtime::Rect bounds) { place(id, engine, bounds); }});
    mounted_.push_back(id);
    auto& scope = build.scope(id);
    if (props.value_) {
        connect_prop(scope, *props.value_, [this, id](String value) {
            if (auto* current = services_->components().state<SegmentedState>(id)) {
                current->value = std::move(value);
                refresh(id, false);
            }
        });
    }
    connect_prop(scope, props.size_, [this, id](ControlSize value) {
        validate(value);
        if (auto* current = services_->components().state<SegmentedState>(id)) {
            current->size = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.block_, [this, id](bool value) {
        if (auto* current = services_->components().state<SegmentedState>(id)) {
            current->block = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.vertical_, [this, id](bool value) {
        if (auto* current = services_->components().state<SegmentedState>(id)) {
            current->vertical = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.disabled_, [this, id](bool value) {
        if (auto* current = services_->components().state<SegmentedState>(id)) {
            current->disabled = value;
            current->pressed.reset();
            current->keyboard_pressed.reset();
            refresh(id, false);
        }
        reconcile_input(id);
    });
    for (std::size_t index = 0; index < options.size(); ++index) {
        connect_prop(scope, options[index].label_, [this, id, index](String value) {
            if (auto* current = services_->components().state<SegmentedState>(id)) {
                services_->components().state<ItemState>(current->items[index])->label.set(std::move(value));
            }
        });
        connect_prop(scope, options[index].disabled_, [this, id, index](bool value) {
            if (auto* current = services_->components().state<SegmentedState>(id)) {
                services_->components().state<ItemState>(current->items[index])->disabled = value;
                refresh(id, false);
            }
            reconcile_input(id);
        });
    }
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::segmented_colors,
                               &theme_runtime::ThemeScope::segmented_metrics,
                               &theme_runtime::ThemeScope::segmented_effects};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { refresh(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    refresh(id, true);
}

runtime::Size SegmentedComponentHost::measure_item(runtime::ComponentId id, std::size_t index,
                                                   layout::LayoutEngine& engine, layout::Constraints limits) {
    auto& s = *services_->components().state<SegmentedState>(id);
    auto& row = *services_->components().state<ItemState>(s.items[index]);
    const auto& m = services_->components().theme_scope(id)->snapshot().segmented().metrics;
    const float padding = s.size == ControlSize::Small ? m.padding_small : m.padding;
    const float available = std::max(0.0F, limits.max_width - padding * 2);
    row.icon_size =
        row.icon_node ? engine.measure_child(*row.icon_node, {0, available, 0, limits.max_height}) : runtime::Size{};
    const float gap = row.icon_node ? std::min(m.icon_gap, std::max(0.0F, available - row.icon_size.width)) : 0;
    row.label_size = engine.measure_child(
        row.label_node, {0, std::max(0.0F, available - row.icon_size.width - gap), 0, limits.max_height});
    const float minimum =
        std::max(0.0F, size_value(s.size, m.height_small, m.height, m.height_large) - m.track_padding * 2);
    return limits.constrain({row.icon_size.width + gap + row.label_size.width + padding * 2,
                             std::max(minimum, std::max(row.icon_size.height, row.label_size.height))});
}

void SegmentedComponentHost::place_item(runtime::ComponentId id, std::size_t index, layout::LayoutEngine& engine,
                                        runtime::Rect bounds) {
    auto& s = *services_->components().state<SegmentedState>(id);
    auto& row = *services_->components().state<ItemState>(s.items[index]);
    const auto& m = services_->components().theme_scope(id)->snapshot().segmented().metrics;
    const float padding = std::min(s.size == ControlSize::Small ? m.padding_small : m.padding, bounds.width / 2);
    const float gap =
        row.icon_node ? std::min(m.icon_gap, std::max(0.0F, bounds.width - padding * 2 - row.icon_size.width)) : 0;
    const float content = row.icon_size.width + gap + row.label_size.width;
    float x = bounds.x + std::max(padding, (bounds.width - content) / 2);
    if (row.icon_node) {
        engine.place_child(*row.icon_node, {x, bounds.y + std::max(0.0F, (bounds.height - row.icon_size.height) / 2),
                                            row.icon_size.width, std::min(bounds.height, row.icon_size.height)});
        x += row.icon_size.width + gap;
    }
    engine.place_child(row.label_node, {x, bounds.y + std::max(0.0F, (bounds.height - row.label_size.height) / 2),
                                        row.label_size.width, std::min(bounds.height, row.label_size.height)});
}

runtime::Size SegmentedComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                              layout::Constraints limits) {
    auto& s = *services_->components().state<SegmentedState>(id);
    const auto pad = services_->components().theme_scope(id)->snapshot().segmented().metrics.track_padding;
    const float available = std::max(0.0F, limits.max_width - pad * 2);
    float natural_width = 0;
    for (auto item : s.items) {
        auto& row = *services_->components().state<ItemState>(item);
        row.natural = engine.measure_child(
            row.node, {0, std::numeric_limits<float>::infinity(), 0, std::numeric_limits<float>::infinity()});
        natural_width = s.vertical ? std::max(natural_width, row.natural.width) : natural_width + row.natural.width;
    }
    const float width = s.block && std::isfinite(available) ? available : std::min(natural_width, available);
    float height = 0;
    for (auto item : s.items) {
        auto& row = *services_->components().state<ItemState>(item);
        const float allocated = s.vertical          ? width
                                : s.block           ? width / static_cast<float>(s.items.size())
                                : natural_width > 0 ? width * row.natural.width / natural_width
                                                    : 0;
        row.allocated =
            engine.measure_child(row.node, {allocated, allocated, 0, std::numeric_limits<float>::infinity()});
        height = s.vertical ? height + row.allocated.height : std::max(height, row.allocated.height);
    }
    return limits.constrain({width + pad * 2, height + pad * 2});
}

void SegmentedComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto& s = *services_->components().state<SegmentedState>(id);
    const auto pad = services_->components().theme_scope(id)->snapshot().segmented().metrics.track_padding;
    const float px = std::min(pad, bounds.width / 2);
    const float py = std::min(pad, bounds.height / 2);
    const float width = std::max(0.0F, bounds.width - px * 2);
    const float height = std::max(0.0F, bounds.height - py * 2);
    float total = 0;
    for (auto item : s.items) {
        const auto& row = *services_->components().state<ItemState>(item);
        total += s.vertical ? row.allocated.height : row.allocated.width;
    }
    float cursor = 0;
    for (auto item : s.items) {
        auto& row = *services_->components().state<ItemState>(item);
        const float w = s.vertical ? width : s.block ? width / static_cast<float>(s.items.size()) : row.allocated.width;
        const float h = s.vertical ? row.allocated.height * (total > height && total > 0 ? height / total : 1) : height;
        // A parent's cross-axis stretch may exceed our measured width. Reflow at
        // the real allocation, then stretch the actual node rather than only its cached measurement.
        static_cast<void>(engine.measure_child(row.node, layout::Constraints::fixed(w, h)));
        engine.place_child(row.node,
                           {bounds.x + px + (s.vertical ? 0 : cursor), bounds.y + py + (s.vertical ? cursor : 0), w, h},
                           true, true);
        cursor += s.vertical ? h : w;
    }
}

bool SegmentedComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<SegmentedState>(id);
        if (!s || !services_->interactions().component_active(id)) {
            continue;
        }
        auto target = focused(*services_, *s);
        if (!target) {
            continue;
        }
        const bool activation = event.key == input::Key::enter || event.key == input::Key::space;
        const bool previous = event.key == (s->vertical ? input::Key::up : input::Key::left);
        const bool next = event.key == (s->vertical ? input::Key::down : input::Key::right);
        const bool navigation = previous || next || event.key == input::Key::home || event.key == input::Key::end;
        if (!activation && !navigation) {
            return false;
        }
        if (event.action == input::KeyAction::up) {
            s->keyboard_pressed.reset();
            refresh(id, false);
            return true;
        }
        if (event.action != input::KeyAction::down || (activation && event.repeat)) {
            return true;
        }
        if (previous || next) {
            for (std::size_t count = 0; count < s->items.size(); ++count) {
                target = (*target + (previous ? s->items.size() - 1 : 1)) % s->items.size();
                if (enabled(*services_, *s, *target)) {
                    break;
                }
            }
        } else if (event.key == input::Key::home || event.key == input::Key::end) {
            target = edge(*services_, *s, event.key == input::Key::end);
        }
        if (target) {
            s->keyboard_pressed = activation ? target : std::nullopt;
            const auto interaction = services_->components().state<ItemState>(s->items[*target])->interaction;
            services_->focus().defer_focus(interaction, input::FocusModality::keyboard);
            activate(id, *target);
        }
        return true;
    }
    return false;
}

void SegmentedComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                              std::optional<input::InteractionId> hit,
                                              std::optional<input::InteractionId>) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<SegmentedState>(id);
        if (!s) {
            continue;
        }
        std::optional<std::size_t> hovered;
        if (event.action != input::PointerAction::cancel && services_->interactions().component_active(id)) {
            for (std::size_t i = 0; i < s->items.size(); ++i) {
                if (enabled(*services_, *s, i) &&
                    services_->components().state<ItemState>(s->items[i])->interaction == hit) {
                    hovered = i;
                }
            }
        }
        if (s->hovered != hovered) {
            s->hovered = hovered;
            refresh(id, false);
        }
    }
}

void SegmentedComponentHost::on_input_boundary_changed() {
    for (auto id : mounted_) {
        reconcile_input(id);
    }
}

void SegmentedComponentHost::reconcile_input(runtime::ComponentId id) {
    auto* s = services_->components().state<SegmentedState>(id);
    if (!s) {
        return;
    }
    for (std::size_t i = 0; i < s->items.size(); ++i) {
        const auto interaction = services_->components().state<ItemState>(s->items[i])->interaction;
        if (!enabled(*services_, *s, i) || !services_->interactions().branch_active(interaction)) {
            if (s->hovered == i) {
                s->hovered.reset();
            }
            if (s->pressed == i) {
                s->pressed.reset();
            }
            if (s->keyboard_pressed == i) {
                s->keyboard_pressed.reset();
            }
            services_->pointer().cancel_interaction(interaction);
            services_->focus().cancel_interaction(interaction);
        }
    }
    refresh(id, false);
}

void SegmentedComponentHost::on_window_active(bool active) {
    if (active) {
        return;
    }
    for (auto id : mounted_) {
        if (auto* s = services_->components().state<SegmentedState>(id)) {
            s->hovered.reset();
            s->pressed.reset();
            s->keyboard_pressed.reset();
            for (auto item : s->items) {
                services_->pointer().cancel_interaction(services_->components().state<ItemState>(item)->interaction);
            }
            refresh(id, false);
        }
    }
}

void SegmentedComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<SegmentedState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        s->clip = services_->component_clip(id, viewport, document_clip);
        const auto& t = services_->components().theme_scope(id)->snapshot().segmented();
        const auto& m = t.metrics;
        const auto& root = services_->nodes().require(s->node);
        const auto root_fill = fill(root.bounds, size_value(s->size, m.radius_small, m.radius, m.radius_large),
                                    t.colors.track, services_->nodes().content_opacity(s->node), root.translation,
                                    {1, services_->nodes().content_clip(s->node, s->clip)});
        static_cast<void>(services_->surfaces().update_content_effects(s->decorations, root_fill));
        const auto active = selected(*services_, *s);
        const auto focus = services_->focus().state();
        for (std::size_t i = 0; i < s->items.size(); ++i) {
            const auto& row = *services_->components().state<ItemState>(s->items[i]);
            const auto& node = services_->nodes().require(row.node);
            const float radius = std::min(size_value(s->size, m.item_radius_small, m.item_radius, m.item_radius_large),
                                          std::min(node.bounds.width, node.bounds.height) / 2);
            const auto opacity = services_->nodes().content_opacity(row.node);
            const graphics::EffectClip clip{2, services_->nodes().content_clip(row.node, s->clip)};
            const bool available = enabled(*services_, *s, i);
            const bool chosen = active == i;
            const bool pressed = available && ((s->pressed == i && s->hovered == i) || s->keyboard_pressed == i);
            std::array<graphics::RoundedEffectInstance, ShadowList::capacity + 5> effects;
            std::size_t count = 0;
            for (const auto& layer : t.effects.selected_shadow.layers()) {
                auto shadow = graphics::make_shadow_effect({node.bounds, radius}, layer, node.translation, clip);
                shadow.material.opacity *= chosen ? opacity : 0;
                effects[count++] = shadow;
            }
            const auto background =
                fill(node.bounds, radius,
                     chosen    ? t.colors.selected_background
                     : pressed ? t.colors.active_background
                               : t.colors.hover_background,
                     chosen || (available && s->hovered == i) || pressed ? opacity : 0, node.translation, clip);
            std::copy(background.begin(), background.end(), effects.begin() + count);
            count += background.size();
            const bool visible = available && services_->interactions().branch_active(row.interaction) &&
                                 focus.focused == row.interaction && focus.focus_visible && t.effects.focus_width > 0;
            effects[count++] = graphics::make_outline_effect(
                {node.bounds, radius}, t.effects.focus_width > 0 ? t.effects.focus_width : 1, t.effects.focus_offset,
                t.colors.focus, visible ? opacity : 0, node.translation, clip);
            static_cast<void>(
                services_->surfaces().update_content_effects(row.decorations, std::span{effects}.first(count)));
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Segmented(SegmentedProps props, std::vector<SegmentedOption> options) {
    if (!detail::active_segmented) {
        throw std::logic_error("Segmented requires window component services");
    }
    detail::active_segmented->mount(props, options);
}
} // namespace ryn
