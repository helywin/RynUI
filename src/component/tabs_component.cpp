#include "component/tabs_component.hpp"
#include "component/tabs_model.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local TabsComponentHost* active_tabs{};

struct TabHeaderState final {
    runtime::NodeId node;
    runtime::NodeId text_node;
    TextSceneId scene;
    input::InteractionId interaction;
    component::RetainedSurfaceId decorations;
    Signal<String> label{String{}};
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    float natural_width{};
    std::uint64_t natural_revision{};
    float width{};
    float offset{};
};

struct TabArrowState final {
    runtime::NodeId node;
    runtime::NodeId icon;
    input::InteractionId interaction;
    component::RetainedSurfaceId decorations;
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    runtime::Size icon_size;
};

struct TabsState final {
    explicit TabsState(TabsModel value) : model(std::move(value)) {}

    TabsModel model;
    runtime::NodeId node;
    runtime::ComponentId navigation;
    runtime::ComponentId viewport;
    std::array<runtime::ComponentId, 2> arrows;
    std::vector<runtime::ComponentId> headers;
    std::vector<runtime::ComponentId> panels;
    component::RetainedSurfaceId decorations;
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    ControlSize size{ControlSize::Middle};
    TabsType type{TabsType::Line};
    TabsPlacement placement{TabsPlacement::Top};
    bool centered{};
    bool overflow{};
    bool reveal{true};
    std::optional<std::size_t> hovered;
    std::optional<std::size_t> pressed;
    std::optional<std::size_t> keyboard_pressed;
    std::optional<std::size_t> arrow_hovered;
    float header_height{};
    float arrow_width{};
    float viewport_width{};
    float content_width{};
    float scroll{};
    float maximum_scroll{};
    float scroll_scale{1};
    float gap{};
    runtime::Rect navigation_bounds;
    runtime::Rect viewport_bounds;
    runtime::Rect clip;
    std::array<theme_runtime::Subscription, 4> subscriptions;
    std::function<void(String)> on_change;
    std::function<void(String)> on_click;
};

void validate(ControlSize value) {
    if (value != ControlSize::Small && value != ControlSize::Middle && value != ControlSize::Large) {
        throw std::invalid_argument("Tabs size is invalid");
    }
}

void validate(TabsType value) {
    if (value != TabsType::Line && value != TabsType::Card) {
        throw std::invalid_argument("Tabs type is invalid");
    }
}

void validate(TabsPlacement value) {
    if (value != TabsPlacement::Top && value != TabsPlacement::Bottom) {
        throw std::invalid_argument("Tabs placement is invalid");
    }
}

runtime::SemanticForeground foreground(Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

runtime::SemanticTypography font(const TabsThemeToken& token, ControlSize size) {
    const auto& f = token.typography;
    return {f.font_family, f.font_weight, false,
            size == ControlSize::Small   ? f.font_size_small
            : size == ControlSize::Large ? f.font_size_large
                                         : f.font_size,
            size == ControlSize::Small   ? f.line_height_small
            : size == ControlSize::Large ? f.line_height_large
                                         : f.line_height};
}

float line_padding(const TabsMetricToken& m, ControlSize size) {
    return size == ControlSize::Small   ? m.line_padding_small
           : size == ControlSize::Large ? m.line_padding_large
                                        : m.line_padding;
}

float card_padding(const TabsMetricToken& m, ControlSize size) {
    return size == ControlSize::Small   ? m.card_padding_small
           : size == ControlSize::Large ? m.card_padding_large
                                        : m.card_padding;
}

float card_height(const TabsMetricToken& m, ControlSize size) {
    return size == ControlSize::Small   ? m.card_height_small
           : size == ControlSize::Large ? m.card_height_large
                                        : m.card_height;
}

std::optional<std::size_t> focused_header(WindowComponentServices& services, const TabsState& s) {
    const auto focused = services.focus().state().focused;
    for (std::size_t index = 0; focused && index < s.headers.size(); ++index) {
        const auto* row = services.components().state<TabHeaderState>(s.headers[index]);
        if (row && row->interaction == *focused && !s.model.options()[index].disabled) {
            return index;
        }
    }
    return {};
}

bool inside(runtime::Rect bounds, float x, float y) {
    return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height;
}

runtime::Rect translated(runtime::Rect bounds, runtime::Point offset) {
    bounds.x += offset.x;
    bounds.y += offset.y;
    return bounds;
}

void translate_subtree(runtime::NodeId node, runtime::Point value, WindowComponentServices& services) {
    if (services.nodes().require(node).clip_boundary) {
        return;
    }
    runtime::NodePropertyWriter writer{services.nodes(), services.dirty()};
    writer.set_translation(node, value);
    for (auto child : services.nodes().require(node).children) {
        translate_subtree(child, value, services);
    }
}

auto fill(runtime::Rect rect, float radius, std::array<bool, 4> corners, Color color, float opacity,
          runtime::Point translation, graphics::EffectClip clip) {
    radius = std::min(radius, std::min(rect.width, rect.height) / 2);
    return graphics::make_corner_fill_effects({rect, radius}, corners, color, opacity, translation, clip);
}
} // namespace

TabsComponentHost::TabsComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

TabsComponentHost::~TabsComponentHost() {
    services_->detach(*this);
}

void* TabsComponentHost::begin_mount() noexcept {
    auto* previous = active_tabs;
    active_tabs = this;
    return previous;
}

void TabsComponentHost::end_mount(void* previous) noexcept {
    active_tabs = static_cast<TabsComponentHost*>(previous);
}

void TabsComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

TabsSnapshot TabsComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<TabsState>(id);
    if (!s) {
        throw std::out_of_range("Tabs component is stale");
    }
    const auto translation = services_->nodes().require(s->node).translation;
    return {s->model.value(),
            s->model.active(),
            s->headers,
            s->panels,
            s->arrows[0],
            s->arrows[1],
            graphics::intersect_effect_bounds(translated(s->viewport_bounds, translation), s->clip),
            s->scroll,
            s->maximum_scroll,
            s->overflow};
}

void TabsComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<TabsState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto& token = services_->components().theme_scope(id)->snapshot().tabs();
    const auto typography = font(token, s->size);
    s->font.set(typography);
    const auto active = s->model.active();
    auto entry = focused_header(*services_, *s);
    if (!entry && active && !s->model.options()[*active].disabled) {
        entry = active;
    }
    if (!entry) {
        entry = s->model.first_enabled();
    }
    for (std::size_t index = 0; index < s->headers.size(); ++index) {
        auto& row = *services_->components().state<TabHeaderState>(s->headers[index]);
        const bool disabled = s->model.options()[index].disabled;
        services_->interactions().set_eligible(row.interaction, !disabled);
        services_->interactions().set_tab_stop(row.interaction, entry == index);
        const bool pressed = (s->pressed == index && s->hovered == index) || s->keyboard_pressed == index;
        row.foreground.set(foreground(disabled              ? token.colors.item_disabled
                                      : pressed             ? token.colors.item_active
                                      : active == index     ? token.colors.item_selected
                                      : s->hovered == index ? token.colors.item_hover
                                                            : token.colors.item));
        if (services_->components().set_branch_active(s->panels[index], active == index)) {
            services_->mark_scene_structure_dirty();
        }
        if (geometry) {
            const float vertical =
                s->type == TabsType::Line
                    ? line_padding(token.metrics, s->size)
                    : std::max(0.0F, (card_height(token.metrics, s->size) - typography.line_height) / 2);
            const float horizontal = s->type == TabsType::Card ? card_padding(token.metrics, s->size) : 0;
            services_->layout().set_layout(row.node,
                                           layout::BoxLayout{{horizontal, vertical, horizontal, vertical}, true});
        }
    }
    for (std::size_t index = 0; index < 2; ++index) {
        auto* arrow = services_->components().state<TabArrowState>(s->arrows[index]);
        if (!arrow) {
            continue;
        }
        const bool available = index == 0 ? s->scroll > 0 : s->scroll < s->maximum_scroll;
        arrow->foreground.set(foreground(!available                  ? token.colors.item_disabled
                                         : s->arrow_hovered == index ? token.colors.item_hover
                                                                     : token.colors.item));
    }
    if (geometry) {
        s->reveal = true;
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
}

void TabsComponentHost::activate(runtime::ComponentId id, std::size_t index) {
    auto* s = services_->components().state<TabsState>(id);
    if (!s || !services_->components().scope(id).active() || index >= s->headers.size() ||
        s->model.options()[index].disabled) {
        return;
    }
    const auto before = s->model.active();
    const auto key = s->model.options()[index].key;
    auto changed = s->model.request(index);
    const auto click = s->on_click;
    const auto change = s->on_change;
    s->reveal = true;
    refresh(id, before != s->model.active());
    if (click) {
        click(key);
    }
    if (!services_->components().contains(id) || !services_->components().scope(id).active()) {
        return;
    }
    if (changed && change) {
        change(std::move(*changed));
    }
}

void TabsComponentHost::scroll_to(runtime::ComponentId id, float value) {
    auto* s = services_->components().state<TabsState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto next = std::clamp(std::round(value * s->scroll_scale) / s->scroll_scale, 0.0F, s->maximum_scroll);
    if (next == s->scroll) {
        return;
    }
    s->scroll = next;
    s->reveal = false;
    refresh(id, false);
    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
}

void TabsComponentHost::mount(const TabsProps& props, const std::vector<TabItem>& items) {
    validate(read_prop(props.type_));
    validate(read_prop(props.size_));
    validate(read_prop(props.placement_));
    std::vector<TabOption> options;
    options.reserve(items.size());
    for (const auto& item : items) {
        options.push_back({item.key_, read_prop(item.disabled_)});
    }
    TabsModel model{std::move(options), props.active_key_ ? std::optional{read_prop(*props.active_key_)} : std::nullopt,
                    props.default_key_};
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<TabsState>(std::move(model));
    auto& s = build.state<TabsState>(id);
    s.node = build.root(id);
    s.type = read_prop(props.type_);
    s.size = read_prop(props.size_);
    s.placement = read_prop(props.placement_);
    s.centered = read_prop(props.centered_);
    s.on_change = props.on_change_;
    s.on_click = props.on_tab_click_;
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    const auto root_fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.decorations = services.surfaces().create_content_range(root_fragment, {});
    build.on_resource_cleanup(id, [&services, node = s.node, decorations = s.decorations] {
        services.surfaces().destroy_content_range(decorations);
        services.layout().remove_layout(node);
    });
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            const auto leaf = [&] {
                const auto component = context.mount_component<int>(0);
                const auto node = context.root(component);
                services.layout().set_layout(node, layout::LeafLayout{});
                context.on_resource_cleanup(component, [&services, node] { services.layout().remove_layout(node); });
                return component;
            };
            s.navigation = leaf();
            context.mount_slot(
                s.navigation, Content{[&] {
                    auto& navigation = runtime::require_component_build_context();
                    const auto arrow = [&](std::size_t index) {
                        const auto component = navigation.mount_component<TabArrowState>();
                        auto& row = navigation.state<TabArrowState>(component);
                        row.node = navigation.root(component);
                        services.layout().set_layout(row.node, layout::LeafLayout{});
                        row.interaction =
                            services.interactions().create({component, row.node, {}, true, false, {}, false, false});
                        input::InteractionHandlers handlers;
                        handlers.target = [this, id, index](input::PointerDispatchContext& event) {
                            auto* s = services_->components().state<TabsState>(id);
                            if (!s) {
                                return;
                            }
                            const auto* arrow = services_->components().state<TabArrowState>(s->arrows[index]);
                            if (!arrow) {
                                return;
                            }
                            if (event.kind() == input::PointerEventKind::down &&
                                event.event().button == input::PointerButton::primary) {
                                static_cast<void>(event.capture_pointer());
                                event.stop_propagation();
                            }
                            if (event.kind() == input::PointerEventKind::up &&
                                event.event().button == input::PointerButton::primary) {
                                const auto interaction = arrow->interaction;
                                static_cast<void>(event.release_pointer_capture());
                                event.stop_propagation();
                                s = services_->components().state<TabsState>(id);
                                if (s && event.press_origin() == interaction) {
                                    const auto& node = services_->nodes().require(arrow->node);
                                    if (inside(translated(node.bounds, node.translation), event.event().x,
                                               event.event().y)) {
                                        scroll_to(id, s->scroll + (index == 0 ? -1 : 1) *
                                                                      std::max(1.0F, s->viewport_width * 0.8F));
                                    }
                                }
                            }
                        };
                        services.interactions().set_handlers(row.interaction, std::move(handlers));
                        const auto hit = navigation.register_scene_fragment(
                            component, runtime::SceneFragmentPlacement::before_children);
                        services.scene_composer().set_fragment(hit, {}, row.interaction);
                        const auto decoration = navigation.register_scene_fragment(
                            component, runtime::SceneFragmentPlacement::before_children);
                        row.decorations = services.surfaces().create_content_range(decoration, {});
                        navigation.on_resource_cleanup(component,
                                                       [&services, node = row.node, interaction = row.interaction, hit,
                                                        decorations = row.decorations] {
                                                           services.pointer().cancel_interaction(interaction);
                                                           services.interactions().remove(interaction);
                                                           services.scene_composer().remove_fragment(hit);
                                                           services.surfaces().destroy_content_range(decorations);
                                                           services.layout().remove_layout(node);
                                                       });
                        navigation.mount_slot_with_semantic_text_style(
                            component, Content{[index] {
                                Icon(IconProps{}.name(index == 0 ? IconName::LeftOutlined : IconName::RightOutlined));
                            }},
                            Prop<runtime::SemanticForeground>{row.foreground},
                            Prop<runtime::SemanticTypography>{s.font});
                        row.icon = services.nodes().require(row.node).children.front();
                        s.arrows[index] = component;
                    };
                    arrow(0);
                    s.viewport = navigation.mount_component<int>(0);
                    const auto viewport_node = navigation.root(s.viewport);
                    services.layout().set_layout(viewport_node, layout::LeafLayout{});
                    services.nodes().require(viewport_node).clip_content = true;
                    navigation.on_resource_cleanup(
                        s.viewport, [&services, viewport_node] { services.layout().remove_layout(viewport_node); });
                    navigation.mount_slot(
                        s.viewport, Content{[&] {
                            auto& viewport = runtime::require_component_build_context();
                            for (std::size_t index = 0; index < items.size(); ++index) {
                                const auto component = viewport.mount_component<TabHeaderState>();
                                auto& row = viewport.state<TabHeaderState>(component);
                                row.node = viewport.root(component);
                                const auto label = read_prop(items[index].label_);
                                row.label.set(label.empty() ? items[index].key_ : label);
                                services.layout().set_layout(row.node, layout::BoxLayout{});
                                row.interaction = services.interactions().create(
                                    {component, row.node, {}, true, true, {}, true, false});
                                input::InteractionHandlers handlers;
                                handlers.target = [this, id, index](input::PointerDispatchContext& event) {
                                    auto* s = services_->components().state<TabsState>(id);
                                    if (s && event.kind() == input::PointerEventKind::cancel) {
                                        s->pressed.reset();
                                        refresh(id, false);
                                        return;
                                    }
                                    if (!s || s->model.options()[index].disabled) {
                                        return;
                                    }
                                    auto* row = services_->components().state<TabHeaderState>(s->headers[index]);
                                    if (!row) {
                                        return;
                                    }
                                    if (event.kind() == input::PointerEventKind::down &&
                                        event.event().button == input::PointerButton::primary) {
                                        static_cast<void>(event.capture_pointer());
                                        event.stop_propagation();
                                        if (auto* current = services_->components().state<TabsState>(id)) {
                                            current->pressed = index;
                                            refresh(id, false);
                                        }
                                    }
                                    if (event.kind() == input::PointerEventKind::up &&
                                        event.event().button == input::PointerButton::primary) {
                                        const auto interaction = row->interaction;
                                        const auto& node = services_->nodes().require(row->node);
                                        const auto point = translated(node.bounds, node.translation);
                                        const auto clip = services_->nodes().content_clip(row->node, s->clip);
                                        const bool hit = event.press_origin() == interaction &&
                                                         inside(graphics::intersect_effect_bounds(point, clip),
                                                                event.event().x, event.event().y);
                                        static_cast<void>(event.release_pointer_capture());
                                        event.stop_propagation();
                                        if (auto* current = services_->components().state<TabsState>(id)) {
                                            current->pressed.reset();
                                            refresh(id, false);
                                        }
                                        if (hit) {
                                            activate(id, index);
                                        }
                                    }
                                };
                                services.interactions().set_handlers(row.interaction, std::move(handlers));
                                input::FocusHandlers focus;
                                focus.state_changed = [this, id, index](input::FocusPresentation presentation) {
                                    if (auto* s = services_->components().state<TabsState>(id)) {
                                        if (presentation.focused) {
                                            s->reveal = true;
                                        } else if (s->keyboard_pressed == index) {
                                            s->keyboard_pressed.reset();
                                        }
                                        refresh(id, false);
                                    }
                                };
                                services.interactions().set_focus_handlers(row.interaction, std::move(focus));
                                const auto hit = viewport.register_scene_fragment(
                                    component, runtime::SceneFragmentPlacement::before_children);
                                services.scene_composer().set_fragment(hit, {}, row.interaction);
                                const auto decoration = viewport.register_scene_fragment(
                                    component, runtime::SceneFragmentPlacement::before_children);
                                row.decorations = services.surfaces().create_content_range(decoration, {});
                                viewport.on_resource_cleanup(component,
                                                             [&services, node = row.node, interaction = row.interaction,
                                                              hit, decorations = row.decorations] {
                                                                 services.pointer().cancel_interaction(interaction);
                                                                 services.interactions().remove(interaction);
                                                                 services.scene_composer().remove_fragment(hit);
                                                                 services.surfaces().destroy_content_range(decorations);
                                                                 services.layout().remove_layout(node);
                                                             });
                                viewport.mount_slot_with_semantic_text_style(
                                    component, Content{[label = row.label] { Text(TextProps{}.content(label)); }},
                                    Prop<runtime::SemanticForeground>{row.foreground},
                                    Prop<runtime::SemanticTypography>{s.font});
                                const auto text = services.text().mounted_texts().back();
                                row.text_node = services.components().root(text.component);
                                row.scene = text.scene;
                                services.text().scene_service().set_ellipsis(row.scene, {1, String{u8"…"}, false, 0});
                                const auto revisions = services.text().scene_service().revisions(row.scene);
                                services.layout().set_intrinsic_revision(row.text_node,
                                                                         revisions.content + revisions.layout);
                                s.headers.push_back(component);
                            }
                        }});
                    arrow(1);
                }});
            for (std::size_t index = 0; index < items.size(); ++index) {
                const auto& item = items[index];
                const auto panel = context.mount_component<int>(0);
                const auto node = context.root(panel);
                services.layout().set_layout(node, layout::BoxLayout{{}, true});
                services.components().set_branch_active(panel, s.model.active() == index);
                context.on_resource_cleanup(panel, [&services, node] { services.layout().remove_layout(node); });
                context.mount_slot(panel,
                                   Content{[content = item.content_] { SlotContentAccess::function(content)(); }});
                s.panels.push_back(panel);
            }
        }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints constraints) {
                        return measure(id, engine, constraints);
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                        place(id, engine, bounds);
                    }});
    mounted_.push_back(id);
    refresh(id, true);
    auto& scope = build.scope(id);
    if (props.active_key_) {
        connect_prop(scope, *props.active_key_, [this, id](String key) {
            if (auto* s = services_->components().state<TabsState>(id)) {
                s->model.reconcile(std::move(key));
                refresh(id, true);
            }
        });
    }
    connect_prop(scope, props.type_, [this, id](TabsType value) {
        validate(value);
        if (auto* s = services_->components().state<TabsState>(id)) {
            s->type = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.size_, [this, id](ControlSize value) {
        validate(value);
        if (auto* s = services_->components().state<TabsState>(id)) {
            s->size = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.placement_, [this, id](TabsPlacement value) {
        validate(value);
        if (auto* s = services_->components().state<TabsState>(id)) {
            s->placement = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.centered_, [this, id](bool value) {
        if (auto* s = services_->components().state<TabsState>(id)) {
            s->centered = value;
            refresh(id, true);
        }
    });
    for (std::size_t index = 0; index < items.size(); ++index) {
        connect_prop(scope, items[index].label_, [this, id, index](String value) {
            if (auto* s = services_->components().state<TabsState>(id)) {
                auto& row = *services_->components().state<TabHeaderState>(s->headers[index]);
                row.label.set(value.empty() ? s->model.options()[index].key : std::move(value));
                refresh(id, true);
            }
        });
        connect_prop(scope, items[index].disabled_, [this, id, index](bool value) {
            if (auto* s = services_->components().state<TabsState>(id)) {
                s->model.set_disabled(index, value);
                refresh(id, false);
            }
        });
    }
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::tabs_colors, &theme_runtime::ThemeScope::tabs_metrics,
                               &theme_runtime::ThemeScope::tabs_typography, &theme_runtime::ThemeScope::tabs_effects};
    for (std::size_t index = 0; index < accessors.size(); ++index) {
        s.subscriptions[index] = theme->capture(
            [this, id, geometry = index == 1 || index == 2](theme_runtime::DirtyPhase) { refresh(id, geometry); },
            [theme, accessor = accessors[index]] { static_cast<void>((theme.get()->*accessor)()); });
    }
}

runtime::Size TabsComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                         layout::Constraints limits) {
    auto& s = *services_->components().state<TabsState>(id);
    const auto& token = services_->components().theme_scope(id)->snapshot().tabs();
    const auto& m = token.metrics;
    const auto f = font(token, s.size);
    const float horizontal = s.type == TabsType::Card ? card_padding(m, s.size) : 0;
    const float gutter = s.type == TabsType::Card ? m.card_gutter : m.horizontal_gutter;
    float natural_width = m.focus_inset * 2;
    for (auto header : s.headers) {
        auto& row = *services_->components().state<TabHeaderState>(header);
        auto& state = services_->text().scene_service().text_state(row.scene);
        if (state.revision() != row.natural_revision) {
            row.natural_width =
                engine.measure_child(row.text_node, {0, std::numeric_limits<float>::infinity(), 0, f.line_height})
                    .width;
        }
        natural_width += row.natural_width + horizontal * 2;
    }
    if (!s.headers.empty()) {
        natural_width += static_cast<float>(s.headers.size() - 1) * gutter;
        const auto& row = *services_->components().state<TabHeaderState>(s.headers.front());
        const auto scale =
            services_->text().scene_service().text_state(row.scene).shaped().default_metrics.display_scale;
        s.scroll_scale = std::isfinite(scale) && scale > 0 ? scale : 1;
    }
    const float preferred = s.headers.empty()          ? 0
                            : s.type == TabsType::Card ? card_height(m, s.size)
                                                       : f.line_height + line_padding(m, s.size) * 2;
    s.header_height = std::min(preferred, limits.max_height);
    const auto active = s.model.active();
    s.gap = active ? std::min(m.body_gap, std::max(0.0F, limits.max_height - s.header_height)) : 0;
    const auto panel = active ? engine.measure_child(services_->components().root(s.panels[*active]),
                                                     {0, limits.max_width, 0,
                                                      std::max(0.0F, limits.max_height - s.header_height - s.gap)})
                              : runtime::Size{};
    const float width = std::isfinite(limits.max_width)
                            ? limits.max_width
                            : std::max(limits.min_width, std::max(natural_width, panel.width));
    const auto content_width = [&](float viewport_width) {
        float result = m.focus_inset * 2;
        for (auto header : s.headers) {
            const auto& row = *services_->components().state<TabHeaderState>(header);
            result += std::min(row.natural_width + horizontal * 2, std::max(0.0F, viewport_width - m.focus_inset * 2));
        }
        if (!s.headers.empty()) {
            result += static_cast<float>(s.headers.size() - 1) * gutter;
        }
        return result;
    };
    s.overflow = !s.headers.empty() && content_width(width) > width + 0.001F;
    s.arrow_width = s.overflow ? std::min(m.scroll_button_width, width / 2) : 0;
    const float viewport_width = std::max(0.0F, width - s.arrow_width * 2);
    if (s.viewport_width != viewport_width) {
        s.reveal = true;
    }
    s.viewport_width = viewport_width;
    s.content_width = content_width(viewport_width);
    s.maximum_scroll = std::ceil(std::max(0.0F, s.content_width - viewport_width) * s.scroll_scale) / s.scroll_scale;
    s.scroll = std::clamp(s.scroll, 0.0F, s.maximum_scroll);
    for (auto arrow_id : s.arrows) {
        if (services_->components().set_branch_active(arrow_id, s.overflow)) {
            services_->mark_scene_structure_dirty();
        }
    }
    const float start =
        m.focus_inset + (!s.overflow && s.centered ? std::max(0.0F, (viewport_width - s.content_width) / 2) : 0);
    float offset = start;
    for (auto header : s.headers) {
        auto& row = *services_->components().state<TabHeaderState>(header);
        row.width = std::min(row.natural_width + horizontal * 2, std::max(0.0F, viewport_width - m.focus_inset * 2));
        row.offset = offset;
        static_cast<void>(engine.measure_child(row.node, layout::Constraints::fixed(row.width, s.header_height)));
        row.natural_revision = services_->text().scene_service().text_state(row.scene).revision();
        offset += row.width + gutter;
    }
    static_cast<void>(engine.measure_child(services_->components().root(s.navigation),
                                           layout::Constraints::fixed(width, s.header_height)));
    static_cast<void>(engine.measure_child(services_->components().root(s.viewport),
                                           layout::Constraints::fixed(viewport_width, s.header_height)));
    for (auto arrow_id : s.arrows) {
        auto& arrow = *services_->components().state<TabArrowState>(arrow_id);
        static_cast<void>(engine.measure_child(arrow.node, layout::Constraints::fixed(s.arrow_width, s.header_height)));
        arrow.icon_size = engine.measure_child(arrow.icon, {0, s.arrow_width, 0, s.header_height});
    }
    return limits.constrain({width, s.header_height + s.gap + panel.height});
}

void TabsComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto& s = *services_->components().state<TabsState>(id);
    const auto header_height = std::min(bounds.height, s.header_height);
    const float y = s.placement == TabsPlacement::Top ? bounds.y : bounds.y + bounds.height - header_height;
    s.navigation_bounds = {bounds.x, y, bounds.width, header_height};
    engine.place_child(services_->components().root(s.navigation), s.navigation_bounds);
    s.viewport_bounds = {bounds.x + s.arrow_width, y, s.viewport_width, header_height};
    engine.place_child(services_->components().root(s.viewport), s.viewport_bounds);
    for (auto header : s.headers) {
        const auto& row = *services_->components().state<TabHeaderState>(header);
        engine.place_child(row.node, {s.viewport_bounds.x + row.offset, y, row.width, header_height});
    }
    for (std::size_t index = 0; index < 2; ++index) {
        const auto& arrow = *services_->components().state<TabArrowState>(s.arrows[index]);
        const runtime::Rect arrow_bounds{index == 0 ? bounds.x : bounds.x + bounds.width - s.arrow_width, y,
                                         s.arrow_width, header_height};
        engine.place_child(arrow.node, arrow_bounds);
        engine.place_child(arrow.icon, {arrow_bounds.x + (s.arrow_width - arrow.icon_size.width) / 2,
                                        y + (header_height - arrow.icon_size.height) / 2, arrow.icon_size.width,
                                        arrow.icon_size.height});
    }
    if (const auto active = s.model.active()) {
        const float gap = std::min(s.gap, std::max(0.0F, bounds.height - header_height));
        engine.place_child(services_->components().root(s.panels[*active]),
                           {bounds.x, s.placement == TabsPlacement::Top ? y + header_height + gap : bounds.y,
                            bounds.width, std::max(0.0F, bounds.height - header_height - gap)});
    }
}

void TabsComponentHost::position_window_layers(runtime::Size, runtime::Rect) {
    // This post-layout hook runs before text synchronization; navigation is not a window layer.
    for (auto id : mounted_) {
        auto* s = services_->components().state<TabsState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        if (s->reveal) {
            auto target = focused_header(*services_, *s);
            if (!target) {
                target = s->model.active();
            }
            if (target) {
                const auto& row = *services_->components().state<TabHeaderState>(s->headers[*target]);
                const auto inset = services_->components().theme_scope(id)->snapshot().tabs().metrics.focus_inset;
                if (row.offset - inset < s->scroll) {
                    s->scroll = std::floor((row.offset - inset) * s->scroll_scale) / s->scroll_scale;
                }
                if (row.offset + row.width + inset > s->scroll + s->viewport_width) {
                    s->scroll = std::ceil((row.offset + row.width + inset - s->viewport_width) * s->scroll_scale) /
                                s->scroll_scale;
                }
                s->scroll = std::clamp(s->scroll, 0.0F, s->maximum_scroll);
            }
            s->reveal = false;
        }
        const auto translation = services_->nodes().require(s->node).translation;
        for (auto header : s->headers) {
            translate_subtree(services_->components().root(header), {translation.x - s->scroll, translation.y},
                              *services_);
        }
        for (auto arrow : s->arrows) {
            translate_subtree(services_->components().root(arrow), translation, *services_);
        }
    }
}

bool TabsComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<TabsState>(id);
        if (!s) {
            continue;
        }
        const auto focused = focused_header(*services_, *s);
        if (!focused) {
            continue;
        }
        const bool activation = event.key == input::Key::enter || event.key == input::Key::space;
        const bool navigation = event.key == input::Key::left || event.key == input::Key::right ||
                                event.key == input::Key::home || event.key == input::Key::end;
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
        auto target = focused;
        if (event.key == input::Key::left || event.key == input::Key::right) {
            target = s->model.next_enabled(focused, event.key == input::Key::left ? -1 : 1);
        } else if (event.key == input::Key::home) {
            target = s->model.first_enabled();
        } else if (event.key == input::Key::end) {
            target = s->model.last_enabled();
        }
        if (target) {
            const auto hit = services_->components().state<TabHeaderState>(s->headers[*target])->interaction;
            services_->focus().defer_focus(hit, input::FocusModality::keyboard);
            if (auto* current = services_->components().state<TabsState>(id)) {
                current->keyboard_pressed = activation ? target : std::nullopt;
            }
            activate(id, *target);
        }
        return true;
    }
    return false;
}

void TabsComponentHost::on_pointer_input(const input::PointerInputEvent& event, std::optional<input::InteractionId> hit,
                                         std::optional<input::InteractionId>) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<TabsState>(id);
        if (!s) {
            continue;
        }
        std::optional<std::size_t> hovered;
        std::optional<std::size_t> arrow_hovered;
        if (event.action != input::PointerAction::cancel && services_->components().branch_active(id)) {
            for (std::size_t index = 0; index < s->headers.size(); ++index) {
                if (services_->components().state<TabHeaderState>(s->headers[index])->interaction == hit) {
                    hovered = index;
                }
            }
            for (std::size_t index = 0; index < 2; ++index) {
                if (services_->components().state<TabArrowState>(s->arrows[index])->interaction == hit) {
                    arrow_hovered = index;
                }
            }
        }
        if (s->hovered != hovered || s->arrow_hovered != arrow_hovered) {
            s->hovered = hovered;
            s->arrow_hovered = arrow_hovered;
            refresh(id, false);
        }
    }
}

bool TabsComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    for (auto it = mounted_.rbegin(); it != mounted_.rend(); ++it) {
        auto* s = services_->components().state<TabsState>(*it);
        if (!s || !s->overflow || !services_->components().branch_active(*it)) {
            continue;
        }
        const auto world = translated(s->navigation_bounds, services_->nodes().require(s->node).translation);
        const auto clipped = graphics::intersect_effect_bounds(
            world, services_->nodes().content_clip(services_->components().root(s->navigation), s->clip));
        if (!inside(clipped, event.x, event.y)) {
            continue;
        }
        const auto delta = event.delta_x != 0 ? event.delta_x : -event.delta_y;
        const auto step = services_->components().theme_scope(*it)->snapshot().tabs().metrics.scroll_button_width * 2;
        scroll_to(*it, s->scroll + delta * step);
        return true;
    }
    return false;
}

void TabsComponentHost::on_window_active(bool active) {
    if (active) {
        return;
    }
    for (auto id : mounted_) {
        auto* s = services_->components().state<TabsState>(id);
        if (!s) {
            continue;
        }
        s->pressed.reset();
        s->hovered.reset();
        s->keyboard_pressed.reset();
        s->arrow_hovered.reset();
        for (auto header : s->headers) {
            services_->pointer().cancel_interaction(services_->components().state<TabHeaderState>(header)->interaction);
        }
        for (auto arrow : s->arrows) {
            services_->pointer().cancel_interaction(services_->components().state<TabArrowState>(arrow)->interaction);
        }
        refresh(id, false);
    }
}

void TabsComponentHost::synchronize_auxiliary_geometry(runtime::Size, runtime::Rect clip) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<TabsState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        s->clip = clip;
        const auto& token = services_->components().theme_scope(id)->snapshot().tabs();
        const auto& m = token.metrics;
        const auto translation = services_->nodes().require(s->node).translation;
        const auto opacity = services_->nodes().content_opacity(s->node);
        const auto nav_clip =
            graphics::EffectClip{1, services_->nodes().content_clip(services_->components().root(s->navigation), clip)};
        const auto viewport_clip =
            graphics::EffectClip{2, services_->nodes().content_clip(services_->components().root(s->viewport), clip)};
        const auto active = s->model.active();
        const float border = std::min(m.border_width, s->navigation_bounds.height);
        const float rail_y = s->placement == TabsPlacement::Top
                                 ? s->navigation_bounds.y + s->navigation_bounds.height - border
                                 : s->navigation_bounds.y;
        std::array<graphics::RoundedEffectInstance, 8> root_effects;
        const auto rail =
            fill({s->navigation_bounds.x, rail_y, s->navigation_bounds.width, border}, 0, {false, false, false, false},
                 token.colors.rail, s->headers.empty() ? 0 : opacity, translation, nav_clip);
        std::copy(rail.begin(), rail.end(), root_effects.begin());
        runtime::Rect indicator;
        runtime::Point indicator_translation = translation;
        if (active) {
            const auto& row = *services_->components().state<TabHeaderState>(s->headers[*active]);
            const auto& text = services_->nodes().require(row.text_node);
            const float height = std::min(m.indicator_width, s->navigation_bounds.height);
            indicator = {text.bounds.x,
                         s->placement == TabsPlacement::Top
                             ? s->navigation_bounds.y + s->navigation_bounds.height - height
                             : s->navigation_bounds.y,
                         text.bounds.width, height};
            indicator_translation = services_->nodes().require(row.node).translation;
        }
        const auto ink = fill(indicator, 0, {false, false, false, false}, token.colors.indicator,
                              active && s->type == TabsType::Line ? opacity : 0, indicator_translation, viewport_clip);
        std::copy(ink.begin(), ink.end(), root_effects.begin() + 4);
        static_cast<void>(services_->surfaces().update_content_effects(s->decorations, root_effects));
        for (std::size_t index = 0; index < s->headers.size(); ++index) {
            const auto& row = *services_->components().state<TabHeaderState>(s->headers[index]);
            const auto& node = services_->nodes().require(row.node);
            const auto bounds = node.bounds;
            const bool card = s->type == TabsType::Card;
            const bool selected = active == index;
            const auto row_opacity = services_->nodes().content_opacity(row.node);
            const std::array corners{s->placement == TabsPlacement::Top, s->placement == TabsPlacement::Top,
                                     s->placement == TabsPlacement::Bottom, s->placement == TabsPlacement::Bottom};
            std::array<graphics::RoundedEffectInstance, 13> effects;
            const auto background =
                fill(bounds, m.border_radius, corners,
                     selected ? token.colors.card_selected_background : token.colors.card_background,
                     card ? row_opacity : 0, node.translation, viewport_clip);
            std::copy(background.begin(), background.end(), effects.begin());
            const float width = std::min(m.border_width, std::min(bounds.width, bounds.height) / 2);
            const runtime::Rect inner{bounds.x + width, bounds.y + width, std::max(0.0F, bounds.width - width * 2),
                                      std::max(0.0F, bounds.height - width * 2)};
            const float radius =
                std::min(std::max(0.0F, m.border_radius - width), std::min(inner.width, inner.height) / 2);
            const auto outlines =
                graphics::make_corner_outline_effects({inner, radius}, corners, width, 0, token.colors.card_border,
                                                      card ? row_opacity : 0, node.translation, viewport_clip);
            std::copy(outlines.begin(), outlines.end(), effects.begin() + 4);
            const auto patch = fill({bounds.x + width,
                                     s->placement == TabsPlacement::Top ? bounds.y + bounds.height - width : bounds.y,
                                     std::max(0.0F, bounds.width - width * 2), width},
                                    0, {false, false, false, false}, token.colors.card_selected_background,
                                    card && selected ? row_opacity : 0, node.translation, viewport_clip);
            std::copy(patch.begin(), patch.end(), effects.begin() + 8);
            const auto focus_bounds = services_->nodes().require(row.text_node).bounds;
            const float focus_radius =
                std::min(token.effects.focus_radius, std::min(focus_bounds.width, focus_bounds.height) / 2);
            const auto focus = services_->focus().state();
            const bool focused = focus.focus_visible && focus.focused == row.interaction &&
                                 token.effects.focus_width > 0 && focus_bounds.width > 0 && focus_bounds.height > 0;
            effects[12] = graphics::make_outline_effect({focus_bounds, focus_radius},
                                                        token.effects.focus_width > 0 ? token.effects.focus_width : 1,
                                                        token.effects.focus_offset, token.colors.focus_outline,
                                                        focused ? row_opacity : 0, node.translation, viewport_clip);
            static_cast<void>(services_->surfaces().update_content_effects(row.decorations, effects));
        }
        for (std::size_t index = 0; index < 2; ++index) {
            auto& arrow = *services_->components().state<TabArrowState>(s->arrows[index]);
            const auto& node = services_->nodes().require(arrow.node);
            const bool available = index == 0 ? s->scroll > 0 : s->scroll < s->maximum_scroll;
            arrow.foreground.set(foreground(!available                  ? token.colors.item_disabled
                                            : s->arrow_hovered == index ? token.colors.item_hover
                                                                        : token.colors.item));
            const auto background =
                fill(node.bounds, m.border_radius, {true, true, true, true}, token.colors.card_background,
                     s->overflow && available && s->arrow_hovered == index ? opacity : 0, node.translation, nav_clip);
            static_cast<void>(services_->surfaces().update_content_effects(arrow.decorations, background));
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Tabs(TabsProps props, std::vector<TabItem> items) {
    if (!detail::active_tabs) {
        throw std::logic_error("Tabs requires window component services");
    }
    detail::active_tabs->mount(props, items);
}
} // namespace ryn
