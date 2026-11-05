#include "component/segmented_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "component/data_display_retained.hpp"
#include "component/navigation_transition.hpp"
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
    SegmentedValue value;
    std::optional<SegmentedOption> definition;
    std::shared_ptr<Scope> bindings;
    bool visible{true};
    Signal<String> label{String{}};
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    bool disabled{};
    runtime::Size icon_size;
    runtime::Size label_size;
    runtime::Size natural;
    runtime::Size allocated;
};

using SegmentedItemOwner = std::shared_ptr<ItemState>;

SegmentedItemOwner segmented_item(WindowComponentServices& services, runtime::ComponentId id) {
    const auto* slot = services.components().state<SegmentedItemOwner>(id);
    return slot ? *slot : nullptr;
}

struct SegmentedState final {
    runtime::NodeId node;
    input::InteractionId anchor;
    SegmentedValue value{String{}};
    bool controlled{};
    bool rtl{};
    bool visible{true};
    bool animated{true};
    SegmentedShape shape{SegmentedShape::Default};
    bool updating{};
    std::optional<std::vector<SegmentedOption>> pending;
    std::function<void(SegmentedValue)> on_value_change;
    std::unique_ptr<NavigationTransition> motion;
    runtime::Rect pill_from;
    runtime::Rect pill_to;
    runtime::Rect pill;
    std::optional<SegmentedValue> pill_value;
    runtime::Size extent;
    component::RetainedSurfaceId pill_surface;
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

using SegmentedOwner = std::shared_ptr<SegmentedState>;

SegmentedOwner segmented_state(WindowComponentServices& services, runtime::ComponentId id) {
    const auto* slot = services.components().state<SegmentedOwner>(id);
    return slot ? *slot : nullptr;
}

std::size_t option_index(WindowComponentServices& services, const SegmentedState& s, const String& key) {
    for (std::size_t i = 0; i < s.items.size(); ++i) {
        const auto row = segmented_item(services, s.items[i]);
        if (row && row->key == key) {
            return i;
        }
    }
    return s.items.size();
}

void validate_value(const SegmentedValue& value) {
    if (const auto* number = std::get_if<double>(&value); number && !std::isfinite(*number)) {
        throw std::invalid_argument("Segmented numeric value must be finite");
    }
}

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
        if (segmented_item(services, s.items[i])->value == s.value) {
            return i;
        }
    }
    return {};
}

bool enabled(WindowComponentServices& services, const SegmentedState& s, std::size_t i) {
    const auto& row = *segmented_item(services, s.items[i]);
    return s.visible && row.visible && !s.disabled && !row.disabled;
}

std::optional<std::size_t> focused(WindowComponentServices& services, const SegmentedState& s) {
    const auto focus = services.focus().state().focused;
    for (std::size_t i = 0; focus && i < s.items.size(); ++i) {
        const auto& row = *segmented_item(services, s.items[i]);
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
    const auto s = segmented_state(*services_, id);
    if (!s) {
        throw std::out_of_range("Segmented component is stale");
    }
    SegmentedSnapshot result{std::holds_alternative<String>(s->value) ? std::get<String>(s->value) : String{},
                             selected(*services_, *s), s->items};
    result.selected_value = s->value;
    result.pill = s->pill;
    result.motion = s->motion->value();
    return result;
}

void SegmentedComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    run_untracked([&] {
        auto s = segmented_state(*services_, id);
        if (!s || s->updating || !services_->components().scope(id).active()) {
            return;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().segmented();
        const auto& m = t.metrics;
        const float label_height =
            std::max(1.0F, size_value(s->size, m.height_small, m.height, m.height_large) - m.track_padding * 2);
        s->font.set({m.font_family, 400, false, s->size == ControlSize::Large ? m.font_size_large : m.font_size,
                     std::min(label_height, s->size == ControlSize::Large ? m.line_height_large : m.line_height)});
        const auto active = selected(*services_, *s);
        services_->components().set_branch_active(id, s->visible);
        services_->nodes().require(s->node).out_of_flow = !s->visible;
        auto entry = focused(*services_, *s);
        if (!entry && active && enabled(*services_, *s, *active)) {
            entry = active;
        }
        if (!entry) {
            entry = edge(*services_, *s);
        }
        for (std::size_t i = 0; i < s->items.size(); ++i) {
            auto& row = *segmented_item(*services_, s->items[i]);
            const bool eligible = enabled(*services_, *s, i);
            services_->components().set_branch_active(s->items[i], row.visible);
            services_->nodes().require(row.node).out_of_flow = !row.visible;
            services_->interactions().set_eligible(row.interaction, eligible);
            services_->interactions().set_tab_stop(row.interaction, entry == i);
            row.foreground.set(foreground(!eligible     ? t.colors.disabled
                                          : active == i ? t.colors.selected
                                          : s->hovered == i || s->pressed == i || s->keyboard_pressed == i
                                              ? t.colors.hover
                                              : t.colors.item));
        }
        services_->dirty().invalidate(s->node, geometry
                                                   ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                         runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                   : runtime::DirtyFlags::Material);
        const auto items = s->items;
        services_->set_input_blocked(id, !s->visible || s->disabled);
        for (auto item : items) {
            if (!services_->components().contains(id)) {
                return;
            }
            if (const auto row = segmented_item(*services_, item)) {
                services_->set_input_blocked(item, !row->visible || row->disabled);
            }
        }
    });
}

void SegmentedComponentHost::activate(runtime::ComponentId id, std::size_t index) {
    auto s = segmented_state(*services_, id);
    if (!s || index >= s->items.size() || !enabled(*services_, *s, index) ||
        !services_->interactions().component_active(id)) {
        return;
    }
    const auto& row = *segmented_item(*services_, s->items[index]);
    if (!services_->interactions().branch_active(row.interaction) || row.value == s->value) {
        return;
    }
    const auto key = row.value;
    const auto callback = s->on_change;
    const auto typed_callback = s->on_value_change;
    if (!s->controlled) {
        s->value = key;
    }
    refresh(id, false);
    if (callback && std::holds_alternative<String>(key)) {
        callback(std::get<String>(key));
    }
    if (typed_callback && services_->components().contains(id)) {
        typed_callback(key);
    }
}

void SegmentedComponentHost::mount_item(runtime::ComponentId id, std::size_t index,
                                        const std::vector<SegmentedOption>& options, bool initial) {
    auto owner = segmented_state(*services_, id);
    if (!owner) {
        return;
    }
    auto& s = *owner;
    auto& services = *services_;
    const auto content = Content{[&] {
        auto& context = runtime::require_component_build_context();
        const auto option = options[index];
        const auto component = context.mount_component<SegmentedItemOwner>(std::make_shared<ItemState>());
        auto item_owner = context.state<SegmentedItemOwner>(component);
        auto& row = *item_owner;
        context.on_resource_cleanup(component, [weak = std::weak_ptr<ItemState>{item_owner}] {
            if (auto row = weak.lock()) {
                row->bindings.reset();
            }
        });
        row.node = context.root(component);
        row.key = option.key_;
        row.value = option.value_.value_or(SegmentedValue{option.key_});
        row.visible = read_prop(option.visible_);
        row.definition = option;
        row.label.set(read_prop(option.label_));
        row.disabled = read_prop(option.disabled_);
        row.interaction = services.interactions().create(
            {component, row.node, {}, !s.disabled && !row.disabled, true, {}, true, false});
        s.items[index] = component;
        services.layout().set_layout(
            row.node,
            layout::ComponentLayout{
                [this, id, key = option.key_](layout::LayoutEngine& engine, runtime::NodeId,
                                              layout::Constraints limits) {
                    const auto s = segmented_state(*services_, id);
                    const auto index = option_index(*services_, *s, key);
                    return measure_item(id, index, engine, limits);
                },
                [this, id, key = option.key_](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                    const auto s = segmented_state(*services_, id);
                    const auto index = option_index(*services_, *s, key);
                    place_item(id, index, engine, bounds);
                }});
        const auto hit = context.register_scene_fragment(component, runtime::SceneFragmentPlacement::before_children);
        services.scene_composer().set_fragment(hit, {}, row.interaction);
        const auto decoration =
            context.register_scene_fragment(component, runtime::SceneFragmentPlacement::before_children);
        row.decorations = services.surfaces().create_content_range(decoration, {});
        context.on_resource_cleanup(
            component, [&services, node = row.node, interaction = row.interaction, hit, decorations = row.decorations] {
                services.pointer().cancel_interaction(interaction);
                services.focus().cancel_interaction(interaction);
                services.interactions().remove(interaction);
                services.scene_composer().remove_fragment(hit);
                services.surfaces().destroy_content_range(decorations);
                services.layout().remove_layout(node);
            });
        input::InteractionHandlers pointer;
        pointer.target = [this, id, key = option.key_](input::PointerDispatchContext& event) {
            auto current = segmented_state(*services_, id);
            const auto index = current ? option_index(*services_, *current, key) : 0;
            if (!current || index >= current->items.size()) {
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
                const auto& item = *segmented_item(*services_, current->items[index]);
                const auto& node = services_->nodes().require(item.node);
                const auto clip = services_->nodes().content_clip(item.node, current->clip);
                const bool hit =
                    event.press_origin() == item.interaction && current->pressed == index &&
                    inside(graphics::intersect_effect_bounds(translated(node), clip), event.event().x, event.event().y);
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
        focus.state_changed = [this, id, key = option.key_](input::FocusPresentation state) {
            if (auto current = segmented_state(*services_, id)) {
                const auto index = option_index(*services_, *current, key);
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
                    // Crop slot descendants without clipping the item's shadow or focus decorations.
                    services.nodes().require(node).clip_content = true;
                    children.on_resource_cleanup(child, [&services, node] { services.layout().remove_layout(node); });
                    children.mount_slot(child, content);
                    return node;
                };
                if (option.icon_) {
                    row.icon_node = wrapper(Content{[icon = *option.icon_] { SlotContentAccess::function(icon)(); }});
                }
                row.label_node = wrapper(
                    option.content_ ? Content{[label = *option.content_] { SlotContentAccess::function(label)(); }}
                                    : Content{[label = row.label] { Text(TextProps{}.content(label)); }});
            }},
            Prop<runtime::SemanticForeground>{row.foreground}, Prop<runtime::SemanticTypography>{s.font});
    }};
    display_append(services, id, id, content, initial);
}

void SegmentedComponentHost::mount(const SegmentedProps& props, const std::vector<SegmentedOption>& options) {
    validate(read_prop(props.size_));
    if (((props.value_ || props.typed_value_) && (props.default_value_ || props.typed_default_)) ||
        (props.value_ && props.typed_value_) || (props.default_value_ && props.typed_default_)) {
        throw std::invalid_argument("Segmented value and defaultValue are mutually exclusive");
    }
    std::unordered_set<std::string_view> keys;
    for (const auto& option : options) {
        if (option.key_.empty() || !keys.insert(option.key_.bytes()).second) {
            throw std::invalid_argument("Segmented keys must be nonempty and unique");
        }
    }
    std::vector<SegmentedValue> values;
    for (const auto& option : options) {
        const auto value = option.value_.value_or(SegmentedValue{option.key_});
        validate_value(value);
        if (std::ranges::find(values, value) != values.end()) {
            throw std::invalid_argument("Segmented values must be unique");
        }
        values.push_back(value);
    }
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<SegmentedOwner>(std::make_shared<SegmentedState>());
    auto owner = build.state<SegmentedOwner>(id);
    auto& s = *owner;
    s.node = build.root(id);
    s.anchor = display_anchor(services, id);
    s.controlled = props.value_.has_value() || props.typed_value_.has_value();
    s.value = props.typed_value_ ? read_prop(*props.typed_value_)
              : props.value_ ? SegmentedValue{read_prop(*props.value_)}
                             : props.typed_default_.value_or(SegmentedValue{props.default_value_.value_or(String{})});
    validate_value(s.value);
    s.rtl = read_prop(props.rtl_);
    s.visible = read_prop(props.visible_);
    s.animated = read_prop(props.animated_);
    s.shape = read_prop(props.shape_);
    s.on_value_change = props.on_value_change_;
    s.motion = std::make_unique<NavigationTransition>(services, id);
    s.disabled = read_prop(props.disabled_);
    s.block = read_prop(props.block_);
    s.vertical = read_prop(props.vertical_);
    s.size = read_prop(props.size_);
    s.on_change = props.on_change_;
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.decorations = services.surfaces().create_content_range(fragment, {});
    s.pill_surface = services.surfaces().create_content_range(fragment, {});
    build.on_resource_cleanup(id, [&services, id, node = s.node, decorations = s.decorations, pill = s.pill_surface,
                                   weak = std::weak_ptr<SegmentedState>{owner}] {
        services.surfaces().destroy_content_range(pill);
        services.interactions().set_input_blocked(id, false);
        if (auto s = weak.lock()) {
            s->motion.reset();
        }
        services.surfaces().destroy_content_range(decorations);
        services.layout().remove_layout(node);
    });
    s.updating = true;
    s.items.resize(options.size());
    for (std::size_t index = 0; index < options.size(); ++index) {
        mount_item(id, index, options, true);
    }
    s.updating = false;
    if (!s.controlled && !props.default_value_ && !props.typed_default_) {
        for (auto item : s.items) {
            const auto& row = *segmented_item(services, item);
            if (!row.disabled && row.visible) {
                s.value = row.value;
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
            if (auto current = segmented_state(*services_, id)) {
                current->value = std::move(value);
                refresh(id, false);
            }
        });
    }
    connect_prop(scope, props.size_, [this, id](ControlSize value) {
        validate(value);
        if (auto current = segmented_state(*services_, id)) {
            current->size = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.block_, [this, id](bool value) {
        if (auto current = segmented_state(*services_, id)) {
            current->block = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.vertical_, [this, id](bool value) {
        if (auto current = segmented_state(*services_, id)) {
            current->vertical = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.disabled_, [this, id](bool value) {
        if (auto current = segmented_state(*services_, id)) {
            current->disabled = value;
            current->pressed.reset();
            current->keyboard_pressed.reset();
            refresh(id, false);
        }
        reconcile_input(id);
    });
    for (std::size_t index = 0; index < options.size(); ++index) {
        bind_item(id, index);
    }
    if (props.typed_value_) {
        connect_prop(scope, *props.typed_value_, [this, id](SegmentedValue value) {
            validate_value(value);
            if (auto s = segmented_state(*services_, id)) {
                s->value = std::move(value);
                refresh(id, false);
            }
        });
    }
    connect_prop(scope, props.rtl_, [this, id](bool value) {
        if (auto s = segmented_state(*services_, id)) {
            s->rtl = value;
            if (s->motion) {
                s->motion->settle();
            }
            refresh(id, true);
        }
    });
    connect_prop(scope, props.visible_, [this, id](bool value) {
        if (auto s = segmented_state(*services_, id)) {
            s->visible = value;
            if (s->motion) {
                s->motion->settle();
            }
            refresh(id, true);
        }
    });
    connect_prop(scope, props.animated_, [this, id](bool value) {
        if (auto s = segmented_state(*services_, id)) {
            s->animated = value;
            if (s->motion) {
                s->motion->settle();
            }
            refresh(id, true);
        }
    });
    connect_prop(scope, props.shape_, [this, id](SegmentedShape value) {
        if (auto s = segmented_state(*services_, id)) {
            if (value < SegmentedShape::Default || value > SegmentedShape::Round) {
                throw std::invalid_argument("Invalid Segmented shape");
            }
            s->shape = value;
            if (s->motion) {
                s->motion->settle();
            }
            refresh(id, true);
        }
    });
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

void SegmentedComponentHost::bind_item(runtime::ComponentId id, std::size_t index) {
    auto s = segmented_state(*services_, id);
    const auto component = s->items[index];
    auto row = segmented_item(*services_, component);
    row->bindings = std::make_shared<Scope>();
    const auto bindings = row->bindings;
    const auto option = *row->definition;
    connect_prop(*bindings, option.label_, [this, id, component](String value) {
        if (auto row = segmented_item(*services_, component)) {
            row->label.set(std::move(value));
            refresh(id, true);
        }
    });
    connect_prop(*bindings, option.disabled_, [this, id, component](bool value) {
        if (auto row = segmented_item(*services_, component)) {
            row->disabled = value;
            refresh(id, false);
            reconcile_input(id);
        }
    });
    connect_prop(*bindings, option.visible_, [this, id, component](bool value) {
        if (auto row = segmented_item(*services_, component)) {
            row->visible = value;
            if (auto s = segmented_state(*services_, id); s && !s->controlled && !value && row->value == s->value) {
                const auto next = edge(*services_, *s);
                s->value = next ? segmented_item(*services_, s->items[*next])->value : SegmentedValue{String{}};
            }
            refresh(id, true);
            reconcile_input(id);
        }
    });
}

void SegmentedComponentHost::update(runtime::ComponentId id, std::vector<SegmentedOption> options) {
    run_untracked([&] {
        std::vector<String> keys;
        std::vector<SegmentedValue> values;
        for (const auto& option : options) {
            const auto value = option.value_.value_or(SegmentedValue{option.key_});
            validate_value(value);
            if (option.key_.empty() || std::ranges::find(keys, option.key_) != keys.end() ||
                std::ranges::find(values, value) != values.end()) {
                throw std::invalid_argument("Segmented keys and values must be unique");
            }
            keys.push_back(option.key_);
            values.push_back(value);
            static_cast<void>(read_prop(option.disabled_));
            static_cast<void>(read_prop(option.visible_));
            static_cast<void>(read_prop(option.label_));
        }
        auto s = segmented_state(*services_, id);
        if (!s) {
            return;
        }
        if (s->updating) {
            s->pending = std::move(options);
            return;
        }
        s->updating = true;
        try {
            const auto old = s->items;
            s->items.clear();
            s->items.resize(options.size());
            s->hovered.reset();
            s->pressed.reset();
            s->keyboard_pressed.reset();
            for (std::size_t i = 0; i < options.size(); ++i) {
                const auto& option = options[i];
                const auto found = std::ranges::find_if(old, [&](auto item) {
                    const auto row = segmented_item(*services_, item);
                    return row && row->key == option.key_;
                });
                if (found == old.end()) {
                    mount_item(id, i, options, false);
                } else {
                    s->items[i] = *found;
                    auto row = segmented_item(*services_, *found);
                    const auto previous = *row->definition;
                    row->bindings.reset();
                    row->definition = option;
                    row->value = values[i];
                    const auto replace = [&](runtime::NodeId node, const Content& content) {
                        runtime::ComponentId component;
                        for (auto child : services_->components().children(*found)) {
                            if (services_->components().root(child) == node) {
                                component = child;
                                break;
                            }
                        }
                        if (!component.valid()) {
                            throw std::logic_error("Segmented slot region is missing");
                        }
                        display_clear(*services_, id, component);
                        if (services_->components().contains(id)) {
                            display_append_styled(*services_, id, component, content, row->foreground, s->font, false);
                        }
                    };
                    if (previous.content_revision_ != option.content_revision_ ||
                        previous.content_.has_value() != option.content_.has_value()) {
                        replace(row->label_node,
                                option.content_
                                    ? Content{[slot = *option.content_] { SlotContentAccess::function(slot)(); }}
                                    : Content{[label = row->label] { Text(TextProps{}.content(label)); }});
                    }
                    if (!services_->components().contains(id)) {
                        return;
                    }
                    if (previous.icon_revision_ != option.icon_revision_ ||
                        previous.icon_.has_value() != option.icon_.has_value()) {
                        if (row->icon_node) {
                            replace(*row->icon_node, Content{[slot = option.icon_] {
                                if (slot) {
                                    SlotContentAccess::function (*slot)();
                                }
                            }});
                        } else if (option.icon_) {
                            display_append(*services_, id, *found, Content{[&] {
                                row->icon_node = services_->components().root(display_region(
                                    *services_, *found, row->foreground, s->font,
                                    Content{[slot = *option.icon_] { SlotContentAccess::function(slot)(); }}));
                            }},
                                           false);
                        }
                    }
                    if (!services_->components().contains(id)) {
                        return;
                    }
                    if (!option.icon_) {
                        row->icon_node.reset();
                    }
                }
            }
            for (auto item : old) {
                if (std::ranges::find(s->items, item) == s->items.end()) {
                    services_->destroy(item);
                    if (!services_->components().contains(id)) {
                        return;
                    }
                }
            }
            for (std::size_t i = 0; i < s->items.size(); ++i) {
                bind_item(id, i);
            }
            // A retained row still has its old flags until its new bindings run.
            // Resolve removal/visibility fallback from the complete new collection.
            if (!s->controlled && (!selected(*services_, *s) ||
                                   !segmented_item(*services_, s->items[*selected(*services_, *s)])->visible)) {
                const auto first = edge(*services_, *s);
                s->value = first ? segmented_item(*services_, s->items[*first])->value : SegmentedValue{String{}};
            }
            display_reorder(*services_, s->anchor, s->items);
            s->updating = false;
            refresh(id, true);
            if (s->pending && services_->components().contains(id)) {
                auto pending = std::move(*s->pending);
                s->pending.reset();
                update(id, std::move(pending));
            }
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
    });
}

void SegmentedComponentHost::mount(const SegmentedProps& props, const Prop<std::vector<SegmentedOption>>& options) {
    mount(props, read_prop(options));
    const auto id = mounted_.back();
    connect_prop(
        services_->components().scope(id), options,
        [this, id](std::vector<SegmentedOption> value) { update(id, std::move(value)); },
        [](const auto&, const auto&) { return false; });
}

runtime::Size SegmentedComponentHost::measure_item(runtime::ComponentId id, std::size_t index,
                                                   layout::LayoutEngine& engine, layout::Constraints limits) {
    auto& s = *segmented_state(*services_, id);
    auto& row = *segmented_item(*services_, s.items[index]);
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
    auto& s = *segmented_state(*services_, id);
    auto& row = *segmented_item(*services_, s.items[index]);
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
    auto& s = *segmented_state(*services_, id);
    const auto pad = services_->components().theme_scope(id)->snapshot().segmented().metrics.track_padding;
    const float available = std::max(0.0F, limits.max_width - pad * 2);
    float natural_width = 0;
    for (auto item : s.items) {
        auto& row = *segmented_item(*services_, item);
        if (!row.visible) {
            continue;
        }
        row.natural = engine.measure_child(
            row.node, {0, std::numeric_limits<float>::infinity(), 0, std::numeric_limits<float>::infinity()});
        natural_width = s.vertical ? std::max(natural_width, row.natural.width) : natural_width + row.natural.width;
    }
    const float width = s.block && std::isfinite(available) ? available : std::min(natural_width, available);
    float height = 0;
    for (auto item : s.items) {
        auto& row = *segmented_item(*services_, item);
        if (!row.visible) {
            continue;
        }
        const float allocated =
            s.vertical ? width
            : s.block
                ? width / static_cast<float>(std::max<std::size_t>(
                              1, std::ranges::count_if(
                                     s.items, [&](auto item) { return segmented_item(*services_, item)->visible; })))
            : natural_width > 0 ? width * row.natural.width / natural_width
                                : 0;
        row.allocated =
            engine.measure_child(row.node, {allocated, allocated, 0, std::numeric_limits<float>::infinity()});
        height = s.vertical ? height + row.allocated.height : std::max(height, row.allocated.height);
    }
    return limits.constrain({width + pad * 2, height + pad * 2});
}

void SegmentedComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto& s = *segmented_state(*services_, id);
    const auto pad = services_->components().theme_scope(id)->snapshot().segmented().metrics.track_padding;
    const float px = std::min(pad, bounds.width / 2);
    const float py = std::min(pad, bounds.height / 2);
    const float width = std::max(0.0F, bounds.width - px * 2);
    const float height = std::max(0.0F, bounds.height - py * 2);
    float total = 0;
    for (auto item : s.items) {
        const auto& row = *segmented_item(*services_, item);
        if (!row.visible) {
            continue;
        }
        total += s.vertical ? row.allocated.height : row.allocated.width;
    }
    float cursor = 0;
    for (auto item : s.items) {
        auto& row = *segmented_item(*services_, item);
        if (!row.visible) {
            continue;
        }
        const float w =
            s.vertical ? width
            : s.block
                ? width / static_cast<float>(std::max<std::size_t>(
                              1, std::ranges::count_if(
                                     s.items, [&](auto item) { return segmented_item(*services_, item)->visible; })))
                : row.allocated.width;
        const float h = s.vertical ? row.allocated.height * (total > height && total > 0 ? height / total : 1) : height;
        // A parent's cross-axis stretch may exceed our measured width. Reflow at
        // the real allocation, then stretch the actual node rather than only its cached measurement.
        static_cast<void>(engine.measure_child(row.node, layout::Constraints::fixed(w, h)));
        engine.place_child(row.node,
                           {bounds.x + px +
                                (s.vertical ? 0
                                 : s.rtl    ? width - cursor - w
                                            : cursor),
                            bounds.y + py + (s.vertical ? cursor : 0), w, h},
                           true, true);
        cursor += s.vertical ? h : w;
    }
}

bool SegmentedComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    for (auto id : mounted_) {
        auto s = segmented_state(*services_, id);
        if (!s || !services_->interactions().component_active(id)) {
            continue;
        }
        auto target = focused(*services_, *s);
        if (!target) {
            continue;
        }
        const bool activation = event.key == input::Key::enter || event.key == input::Key::space;
        const bool previous = event.key == (s->vertical ? input::Key::up
                                            : s->rtl    ? input::Key::right
                                                        : input::Key::left);
        const bool next = event.key == (s->vertical ? input::Key::down : s->rtl ? input::Key::left : input::Key::right);
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
            const auto interaction = segmented_item(*services_, s->items[*target])->interaction;
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
        auto s = segmented_state(*services_, id);
        if (!s) {
            continue;
        }
        std::optional<std::size_t> hovered;
        if (event.action != input::PointerAction::cancel && services_->interactions().component_active(id)) {
            for (std::size_t i = 0; i < s->items.size(); ++i) {
                if (enabled(*services_, *s, i) && segmented_item(*services_, s->items[i])->interaction == hit) {
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
    const auto mounted = mounted_;
    for (auto id : mounted) {
        reconcile_input(id);
    }
    synchronize_auxiliary_motion();
}

void SegmentedComponentHost::reconcile_input(runtime::ComponentId id) {
    auto s = segmented_state(*services_, id);
    if (!s) {
        return;
    }
    for (std::size_t i = 0; i < s->items.size(); ++i) {
        const auto interaction = segmented_item(*services_, s->items[i])->interaction;
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
        if (auto s = segmented_state(*services_, id)) {
            s->motion->settle();
            s->hovered.reset();
            s->pressed.reset();
            s->keyboard_pressed.reset();
            for (auto item : s->items) {
                services_->pointer().cancel_interaction(segmented_item(*services_, item)->interaction);
            }
            refresh(id, false);
        }
    }
}

void SegmentedComponentHost::synchronize_auxiliary_motion() {
    const auto mounted = mounted_;
    for (auto id : mounted) {
        const auto s = segmented_state(*services_, id);
        if (s && s->motion &&
            (!services_->components().branch_active(id) || !services_->interactions().component_active(id) ||
             !services_->focus().state().window_active ||
             !animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                               services_->motion_preference())
                  .enabled())) {
            s->motion->settle();
        }
    }
}

void SegmentedComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        auto s = segmented_state(*services_, id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        s->clip = services_->component_clip(id, viewport, document_clip);
        const auto& t = services_->components().theme_scope(id)->snapshot().segmented();
        const auto& m = t.metrics;
        const auto& root = services_->nodes().require(s->node);
        const auto root_fill =
            fill(root.bounds,
                 s->shape == SegmentedShape::Round ? root.bounds.height / 2
                                                   : size_value(s->size, m.radius_small, m.radius, m.radius_large),
                 t.colors.track, services_->nodes().content_opacity(s->node), root.translation,
                 {1, services_->nodes().content_clip(s->node, s->clip)});
        static_cast<void>(services_->surfaces().update_content_effects(s->decorations, root_fill));
        const auto active = selected(*services_, *s);
        const auto focus = services_->focus().state();
        runtime::Rect destination{};
        if (active) {
            const auto& row = *segmented_item(*services_, s->items[*active]);
            if (row.visible) {
                destination = services_->nodes().require(row.node).bounds;
            }
        }
        const runtime::Size extent{root.bounds.width, root.bounds.height};
        const bool resized = extent != s->extent;
        s->extent = extent;
        if (destination != s->pill_to || s->pill_value != std::optional{s->value}) {
            const bool selection_change = s->pill_value && s->pill_value != std::optional{s->value};
            s->pill_from = s->pill;
            s->pill_to = destination;
            s->pill_value = s->value;
            s->motion->restart(selection_change && !resized && s->animated && destination.width > 0 &&
                               s->pill_from.width > 0);
        }
        if (resized || !s->visible || !services_->interactions().component_active(id) || !focus.window_active ||
            !animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                              services_->motion_preference())
                 .enabled()) {
            s->motion->settle();
        }
        const float progress = s->motion->value();
        const auto mix = [progress](float a, float b) {
            return a + (b - a) * progress;
        };
        s->pill = {mix(s->pill_from.x, s->pill_to.x), mix(s->pill_from.y, s->pill_to.y),
                   mix(s->pill_from.width, s->pill_to.width), mix(s->pill_from.height, s->pill_to.height)};
        if (progress >= 1) {
            s->pill = s->pill_to;
        }
        const float pill_radius =
            std::min(s->shape == SegmentedShape::Round
                         ? s->pill.height / 2
                         : size_value(s->size, m.item_radius_small, m.item_radius, m.item_radius_large),
                     std::min(s->pill.width, s->pill.height) / 2);
        std::vector<graphics::RoundedEffectInstance> pill_effects;
        const graphics::EffectClip pill_clip{1, services_->nodes().content_clip(s->node, s->clip)};
        if (s->pill.width > 0 && s->pill.height > 0) {
            for (const auto& layer : t.effects.selected_shadow.layers()) {
                pill_effects.push_back(
                    graphics::make_shadow_effect({s->pill, pill_radius}, layer, root.translation, pill_clip));
            }
            const auto background = fill(s->pill, pill_radius, t.colors.selected_background,
                                         services_->nodes().content_opacity(s->node), root.translation, pill_clip);
            pill_effects.insert(pill_effects.end(), background.begin(), background.end());
        }
        static_cast<void>(services_->surfaces().update_content_effects(s->pill_surface, pill_effects));
        for (std::size_t i = 0; i < s->items.size(); ++i) {
            const auto& row = *segmented_item(*services_, s->items[i]);
            const auto& node = services_->nodes().require(row.node);
            const float radius =
                std::min(s->shape == SegmentedShape::Round
                             ? node.bounds.height / 2
                             : size_value(s->size, m.item_radius_small, m.item_radius, m.item_radius_large),
                         std::min(node.bounds.width, node.bounds.height) / 2);
            const auto opacity = services_->nodes().content_opacity(row.node);
            const graphics::EffectClip clip{2, services_->nodes().content_clip(row.node, s->clip)};
            const bool available = enabled(*services_, *s, i);
            const bool chosen = false;
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
void Segmented(SegmentedProps props, Prop<std::vector<SegmentedOption>> options) {
    if (!detail::active_segmented) {
        throw std::logic_error("Segmented requires window component services");
    }
    detail::active_segmented->mount(props, options);
}

void Segmented(SegmentedProps props, std::vector<SegmentedOption> options) {
    if (!detail::active_segmented) {
        throw std::logic_error("Segmented requires window component services");
    }
    detail::active_segmented->mount(props, options);
}
} // namespace ryn
