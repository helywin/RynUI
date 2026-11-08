#include "component/config_defaults.hpp"
#include "component/card_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/flex.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local CardComponentHost* active_cards{};

struct CardState final {
    runtime::NodeId node;
    input::InteractionId interaction;
    component::RetainedSurfaceId surface;
    component::RetainedSurfaceId decorations;
    CardSnapshot snapshot;
    CardSize size{CardSize::Default};
    CardVariant variant{CardVariant::Outlined};
    bool hoverable{};
    bool title_slot{};
    bool extra_slot{};
    bool cover_slot{};
    bool actions_slot{};
    bool grid{};
    bool animated{true};
    bool clip_content{};
    runtime::SceneFragmentId shadow_fragment;
    runtime::SceneFragmentId decoration_fragment;
    std::optional<runtime::RoundedContentClip> last_mask;
    std::optional<runtime::Rect> last_mask_clip;
    std::size_t columns{3};
    std::vector<runtime::ComponentId> grid_children;
    std::vector<runtime::Size> grid_sizes;
    std::optional<animation::AnimationTime> last;
    runtime::Rect clip;
    float pulse{1};
    Signal<String> title{String{}};
    Signal<runtime::SemanticForeground> body_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> title_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> extra_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> body_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> title_font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 4> subscriptions;
    runtime::Size title_size;
    runtime::Size extra_size;
    std::array<runtime::Size, 5> region_sizes;
};

struct MetaState final {
    runtime::NodeId node;
    runtime::ComponentId avatar;
    runtime::ComponentId section;
    runtime::ComponentId title;
    runtime::ComponentId description;
    bool rich_title{};
    bool rich_description{};
    Signal<String> title_text{String{}};
    Signal<String> description_text{String{}};
    Signal<runtime::SemanticForeground> title_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> description_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> title_font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 3> subscriptions;
};

runtime::SemanticForeground foreground(Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

void validate(CardSize size) {
    if (size != CardSize::Default && size != CardSize::Small) {
        throw std::invalid_argument("Card size is invalid");
    }
}

void validate(CardVariant variant) {
    if (variant != CardVariant::Outlined && variant != CardVariant::Borderless) {
        throw std::invalid_argument("Card variant is invalid");
    }
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId ancestor) {
    for (auto current = std::optional{child}; current && services.components().contains(*current);
         current = services.components().parent(*current)) {
        if (*current == ancestor) {
            return true;
        }
    }
    return false;
}
} // namespace

CardComponentHost::CardComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

CardComponentHost::~CardComponentHost() {
    services_->detach(*this);
}

void* CardComponentHost::begin_mount() noexcept {
    auto* previous = active_cards;
    active_cards = this;
    return previous;
}

void CardComponentHost::end_mount(void* previous) noexcept {
    active_cards = static_cast<CardComponentHost*>(previous);
}

void CardComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

CardSnapshot CardComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* state = services_->components().state<CardState>(id);
    if (!state) {
        throw std::out_of_range("Card component is stale");
    }
    return state->snapshot;
}

void CardComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<CardState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto& token = services_->components().theme_scope(id)->snapshot().card();
    const auto& fonts = token.typography;
    const bool small = s->size == CardSize::Small;
    s->body_color.set(foreground(token.colors.text));
    s->title_color.set(foreground(token.colors.header_text));
    s->extra_color.set(foreground(token.colors.extra));
    s->body_font.set({fonts.font_family, fonts.font_weight, false, fonts.font_size, fonts.line_height});
    s->title_font.set({fonts.font_family, fonts.header_font_weight, false,
                       small ? fonts.header_font_size_small : fonts.header_font_size,
                       small ? fonts.header_line_height_small : fonts.header_line_height});
    s->snapshot.has_header = s->title_slot || s->extra_slot || !s->title.get().empty();
    for (const auto& [component, shown] :
         {std::pair{s->snapshot.header, s->snapshot.has_header}, std::pair{s->snapshot.cover, s->cover_slot},
          std::pair{s->snapshot.body, !s->snapshot.loading}, std::pair{s->snapshot.placeholder, s->snapshot.loading},
          std::pair{s->snapshot.actions, s->actions_slot}}) {
        if (services_->components().set_branch_active(component, shown)) {
            services_->mark_scene_structure_dirty();
        }
    }
    if (geometry) {
        const auto padding = small ? token.metrics.body_padding_small : token.metrics.body_padding;
        if (s->grid_children.empty()) {
            services_->layout().set_layout(services_->components().root(s->snapshot.body),
                                           layout::BoxLayout{{padding, padding, padding, padding}, true});
        }
        services_->layout().set_layout(
            services_->components().root(s->snapshot.actions),
            layout::FlexLayout{layout::FlexDirection::horizontal,
                               0,
                               {0, token.metrics.actions_padding, 0, token.metrics.actions_padding},
                               true,
                               false,
                               layout::FlexWrap::no_wrap,
                               layout::FlexJustify::center,
                               layout::FlexAlign::center});
        services_->layout().set_layout(
            services_->components().root(s->snapshot.placeholder),
            layout::LeafLayout{{padding * 2 + 160,
                                padding * 2 + token.metrics.loading_line_height * 3 + token.metrics.loading_gap * 2}});
    }
    const auto& mask_token = services_->components().theme_scope(id)->snapshot().card();
    const bool masked = !s->grid && (s->clip_content || s->snapshot.loading);
    auto& mask_node = services_->nodes().require(s->node);
    const std::optional<float> requested_radius =
        masked ? std::optional{mask_token.metrics.border_radius} : std::nullopt;
    if (mask_node.rounded_content_radius != requested_radius) {
        mask_node.rounded_content_radius = requested_radius;
        services_->scene_composer().invalidate_composition();
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
}

void CardComponentHost::mount(const CardProps& props, const CardContent& content, const CardSlots& slots, bool grid) {
    validate(read_prop(props.size_));
    validate(read_prop(props.variant_));
    if (read_prop(props.grid_columns_) == 0) {
        throw std::invalid_argument("Card gridColumns must be positive");
    }
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<CardState>();
    auto& s = build.state<CardState>(id);
    s.node = build.root(id);
    s.size = read_prop(props.size_);
    s.variant = read_prop(props.variant_);
    s.hoverable = read_prop(props.hoverable_);
    s.grid = grid;
    s.columns = read_prop(props.grid_columns_);
    s.animated = read_prop(props.animated_);
    s.clip_content = read_prop(props.clip_content_);
    s.snapshot.loading = read_prop(props.loading_);
    s.title.set(read_prop(props.title_));
    s.title_slot = slots.title.has_value();
    s.extra_slot = slots.extra.has_value();
    s.cover_slot = slots.cover.has_value();
    s.actions_slot = slots.actions.has_value();
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    s.interaction = services.interactions().create({id, s.node, {}, true, false, {}, true, false});
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    graphics::QuadInstance initial;
    initial.opacity = 0;
    component::RetainedSurfaceEffects effects;
    effects.focus_enabled = false;
    s.surface = services.surfaces().create(id, s.node, fragment, s.interaction, std::span{&initial, 1}, effects);
    const auto decoration_fragment =
        build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.decorations = services.surfaces().create_content_range(decoration_fragment, {});
    s.shadow_fragment = fragment;
    s.decoration_fragment = decoration_fragment;
    services.scene_composer().set_fragment_composition(fragment, true);
    build.on_resource_cleanup(
        id, [&services, decorations = s.decorations] { services.surfaces().destroy_content_range(decorations); });
    build.on_resource_cleanup(id, [&services, node = s.node, hit = s.interaction, surface = s.surface] {
        services.pointer().cancel_interaction(hit);
        services.interactions().remove(hit);
        services.surfaces().destroy(surface);
        services.layout().remove_layout(node);
    });
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            const auto region = [&](runtime::ComponentId parent, const Content& body,
                                    const Signal<runtime::SemanticForeground>& color,
                                    const Signal<runtime::SemanticTypography>& font) {
                runtime::ComponentId result;
                context.mount_slot(parent, Content{[&] {
                                       auto& children = runtime::require_component_build_context();
                                       result = children.mount_component<int>(0);
                                       const auto node = children.root(result);
                                       services.layout().set_layout(node, layout::BoxLayout{{}, true});
                                       children.on_resource_cleanup(
                                           result, [&services, node] { services.layout().remove_layout(node); });
                                       children.mount_slot_with_semantic_text_style(
                                           result, body, Prop<runtime::SemanticForeground>{color},
                                           Prop<runtime::SemanticTypography>{font});
                                   }});
                return result;
            };
            s.snapshot.header = context.mount_component<int>(0);
            const auto header_node = context.root(s.snapshot.header);
            context.on_resource_cleanup(s.snapshot.header,
                                        [&services, header_node] { services.layout().remove_layout(header_node); });
            s.snapshot.title = region(s.snapshot.header, Content{[title = slots.title, signal = s.title] {
                                          if (title) {
                                              SlotContentAccess::function (*title)();
                                          } else {
                                              Text(TextProps{}.content(signal));
                                          }
                                      }},
                                      s.title_color, s.title_font);
            if (!slots.title) {
                const auto& title_text = services.text().mounted_texts().back();
                services.text().scene_service().set_ellipsis(title_text.scene, {1, String{u8"…"}, false, 0});
                const auto revisions = services.text().scene_service().revisions(title_text.scene);
                services.layout().set_intrinsic_revision(services.components().root(title_text.component),
                                                         revisions.content + revisions.layout);
            }
            s.snapshot.extra = region(s.snapshot.header, Content{[extra = slots.extra] {
                                          if (extra) {
                                              SlotContentAccess::function (*extra)();
                                          }
                                      }},
                                      s.extra_color, s.body_font);
            services.layout().set_layout(context.root(s.snapshot.title), layout::BoxLayout{});
            services.layout().set_layout(context.root(s.snapshot.extra), layout::BoxLayout{});
            services.layout().set_layout(
                header_node,
                layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        auto& s = *services_->components().state<CardState>(id);
                        const auto& token = services_->components().theme_scope(id)->snapshot().card();
                        const auto& metrics = token.metrics;
                        const bool small = s.size == CardSize::Small;
                        const float padding = std::min(small ? metrics.header_padding_small : metrics.header_padding,
                                                       limits.max_width / 2);
                        const float available = std::max(0.0F, limits.max_width - padding * 2);
                        s.extra_size = s.extra_slot
                                           ? engine.measure_child(services_->components().root(s.snapshot.extra),
                                                                  {0, available, 0, limits.max_height})
                                           : runtime::Size{};
                        const float gap =
                            s.extra_size.width > 0
                                ? std::min(metrics.header_gap, std::max(0.0F, available - s.extra_size.width))
                                : 0;
                        s.title_size = engine.measure_child(
                            services_->components().root(s.snapshot.title),
                            {0, std::max(0.0F, available - s.extra_size.width - gap), 0, limits.max_height});
                        return limits.constrain({std::isfinite(limits.max_width)
                                                     ? limits.max_width
                                                     : s.title_size.width + s.extra_size.width + gap + padding * 2,
                                                 std::max(small ? metrics.header_height_small : metrics.header_height,
                                                          std::max(s.title_size.height, s.extra_size.height))});
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                        auto& s = *services_->components().state<CardState>(id);
                        const auto& metrics = services_->components().theme_scope(id)->snapshot().card().metrics;
                        const float padding =
                            std::min(s.size == CardSize::Small ? metrics.header_padding_small : metrics.header_padding,
                                     bounds.width / 2);
                        const float available = std::max(0.0F, bounds.width - padding * 2);
                        const float extra = std::min(available, s.extra_size.width);
                        const float gap =
                            extra > 0 ? std::min(metrics.header_gap, std::max(0.0F, available - extra)) : 0;
                        engine.place_child(services_->components().root(s.snapshot.title),
                                           {bounds.x + padding, bounds.y + (bounds.height - s.title_size.height) / 2,
                                            std::max(0.0F, available - extra - gap), s.title_size.height});
                        if (s.extra_slot) {
                            engine.place_child(services_->components().root(s.snapshot.extra),
                                               {bounds.x + bounds.width - padding - extra,
                                                bounds.y + (bounds.height - s.extra_size.height) / 2, extra,
                                                s.extra_size.height});
                        }
                    }});
            s.snapshot.tabs =
                region(id, Content{[&props] {
                           if (props.tabs_) {
                               auto options = props.tab_props_.value_or(TabsProps{}.size(bind([size = props.size_] {
                                   return read_prop(size) == CardSize::Small ? ControlSize::Small : ControlSize::Large;
                               })));
                               if (props.tab_extra_) {
                                   options.extraEnd(*props.tab_extra_);
                               }
                               if (props.active_tab_) {
                                   options.activeKey(*props.active_tab_);
                               }
                               if (props.default_tab_) {
                                   options.defaultActiveKey(*props.default_tab_);
                               }
                               if (props.on_tab_change_) {
                                   options.onChange(props.on_tab_change_);
                               }
                               Tabs(std::move(options), *props.tabs_);
                           }
                       }},
                       s.body_color, s.body_font);
            s.snapshot.cover = region(id, Content{[cover = slots.cover] {
                                          if (cover) {
                                              SlotContentAccess::function (*cover)();
                                          }
                                      }},
                                      s.body_color, s.body_font);
            s.snapshot.body =
                region(id, Content{[content] { SlotContentAccess::function(content)(); }}, s.body_color, s.body_font);
            s.snapshot.placeholder = context.mount_component<int>(0);
            const auto placeholder_node = context.root(s.snapshot.placeholder);
            context.on_resource_cleanup(s.snapshot.placeholder, [&services, placeholder_node] {
                services.layout().remove_layout(placeholder_node);
            });
            s.snapshot.actions = region(id, Content{[actions = slots.actions] {
                                            if (actions) {
                                                SlotContentAccess::function (*actions)();
                                            }
                                        }},
                                        s.body_color, s.body_font);
        }});
    for (auto child : services.components().children(s.snapshot.body)) {
        if (const auto* cell = services.components().state<CardState>(child); cell && cell->grid) {
            s.grid_children.push_back(child);
        }
    }
    if (!s.grid_children.empty()) {
        s.grid_children = services.components().children(s.snapshot.body);
        s.grid_sizes.resize(s.grid_children.size());
        services.layout().set_layout(
            services.components().root(s.snapshot.body),
            layout::ComponentLayout{
                [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                    auto& s = *services_->components().state<CardState>(id);
                    const float width = std::isfinite(limits.max_width)
                                            ? limits.max_width / static_cast<float>(s.columns)
                                            : std::numeric_limits<float>::infinity();
                    float height{};
                    float row_height{};
                    float measured_width{};
                    for (std::size_t i = 0; i < s.grid_children.size(); ++i) {
                        s.grid_sizes[i] = engine.measure_child(services_->components().root(s.grid_children[i]),
                                                               {0, width, 0, limits.max_height});
                        measured_width =
                            std::max(measured_width, s.grid_sizes[i].width * static_cast<float>(s.columns));
                        row_height = std::max(row_height, s.grid_sizes[i].height);
                        if ((i + 1) % s.columns == 0) {
                            height += row_height;
                            row_height = 0;
                        }
                    }
                    return limits.constrain(
                        {std::isfinite(limits.max_width) ? limits.max_width : measured_width, height + row_height});
                },
                [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                    auto& s = *services_->components().state<CardState>(id);
                    const float width = bounds.width / static_cast<float>(s.columns);
                    float y = bounds.y;
                    for (std::size_t i = 0; i < s.grid_children.size();) {
                        const auto end = std::min(s.grid_children.size(), i + s.columns);
                        float height{};
                        for (auto j = i; j < end; ++j) {
                            height = std::max(height, s.grid_sizes[j].height);
                        }
                        for (auto j = i; j < end; ++j) {
                            engine.place_child(services_->components().root(s.grid_children[j]),
                                               {bounds.x + static_cast<float>(j - i) * width, y, width, height});
                        }
                        y += height;
                        i = end;
                    }
                }});
    }
    services.layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        auto& s = *services_->components().state<CardState>(id);
                        const std::array regions{s.snapshot.header, s.snapshot.tabs, s.snapshot.cover,
                                                 s.snapshot.loading ? s.snapshot.placeholder : s.snapshot.body,
                                                 s.snapshot.actions};
                        float height = 0;
                        float width = 0;
                        for (std::size_t index = 0; index < regions.size(); ++index) {
                            auto& size = s.region_sizes[index];
                            size = services_->components().branch_active(regions[index])
                                       ? engine.measure_child(
                                             services_->components().root(regions[index]),
                                             {0, limits.max_width, 0, std::max(0.0F, limits.max_height - height)})
                                       : runtime::Size{};
                            height += size.height;
                            width = std::max(width, size.width);
                        }
                        return limits.constrain({std::isfinite(limits.max_width) ? limits.max_width : width, height});
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                        auto& s = *services_->components().state<CardState>(id);
                        const std::array regions{s.snapshot.header, s.snapshot.tabs, s.snapshot.cover,
                                                 s.snapshot.loading ? s.snapshot.placeholder : s.snapshot.body,
                                                 s.snapshot.actions};
                        float y = bounds.y;
                        for (std::size_t index = 0; index < regions.size(); ++index) {
                            if (services_->components().branch_active(regions[index])) {
                                engine.place_child(services_->components().root(regions[index]),
                                                   {bounds.x, y, bounds.width, s.region_sizes[index].height});
                            }
                            y += s.region_sizes[index].height;
                        }
                    }});
    mounted_.push_back(id);
    refresh(id, true);
    auto& scope = build.scope(id);
    connect_prop(scope, props.title_, [this, id](String title) {
        if (auto* s = services_->components().state<CardState>(id)) {
            s->title.set(std::move(title));
            refresh(id, true);
        }
    });
    connect_prop(scope, props.size_, [this, id](CardSize size) {
        validate(size);
        if (auto* s = services_->components().state<CardState>(id)) {
            s->size = size;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.variant_, [this, id](CardVariant variant) {
        validate(variant);
        if (auto* s = services_->components().state<CardState>(id)) {
            s->variant = variant;
            refresh(id, false);
        }
    });
    connect_prop(scope, props.hoverable_, [this, id](bool hoverable) {
        if (auto* s = services_->components().state<CardState>(id)) {
            s->hoverable = hoverable;
            refresh(id, false);
        }
    });
    connect_prop(scope, props.loading_, [this, id](bool loading) {
        if (auto* s = services_->components().state<CardState>(id)) {
            s->snapshot.loading = loading;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.clip_content_, [this, id](bool value) {
        if (auto* s = services_->components().state<CardState>(id)) {
            s->clip_content = value;
            refresh(id, false);
        }
    });
    connect_prop(scope, props.animated_, [this, id](bool value) {
        if (auto* s = services_->components().state<CardState>(id)) {
            s->animated = value;
            refresh(id, false);
        }
    });
    connect_prop(scope, props.grid_columns_, [this, id](std::size_t value) {
        if (value == 0) {
            throw std::invalid_argument("Card gridColumns must be positive");
        }
        if (auto* s = services_->components().state<CardState>(id)) {
            s->columns = value;
            refresh(id, true);
        }
    });
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::card_colors, &theme_runtime::ThemeScope::card_metrics,
                               &theme_runtime::ThemeScope::card_typography, &theme_runtime::ThemeScope::card_effects};
    for (std::size_t index = 0; index < accessors.size(); ++index) {
        s.subscriptions[index] = theme->capture(
            [this, id, geometry = index == 1 || index == 2](theme_runtime::DirtyPhase) { refresh(id, geometry); },
            [theme, accessor = accessors[index]] { static_cast<void>((theme.get()->*accessor)()); });
    }
}

void CardComponentHost::on_pointer_input(const input::PointerInputEvent& event, std::optional<input::InteractionId> hit,
                                         std::optional<input::InteractionId>) {
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    for (auto id : mounted_) {
        auto* s = services_->components().state<CardState>(id);
        if (!s) {
            continue;
        }
        const bool hovered = event.action != input::PointerAction::cancel && target &&
                             services_->interactions().component_active(id) &&
                             descendant(*services_, target->component, id);
        if (s->snapshot.hovered != hovered) {
            s->snapshot.hovered = hovered;
            refresh(id, false);
        }
    }
}

void CardComponentHost::on_window_active(bool active) {
    if (active) {
        return;
    }
    for (auto id : mounted_) {
        if (auto* s = services_->components().state<CardState>(id); s && s->snapshot.hovered) {
            s->snapshot.hovered = false;
            refresh(id, false);
        }
    }
}

void CardComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<CardState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto clip = services_->component_clip(id, viewport, document_clip);
        const auto& node = services_->nodes().require(s->node);
        const auto bounds = node.bounds;
        s->clip = graphics::intersect_effect_bounds(
            {bounds.x + node.translation.x, bounds.y + node.translation.y, bounds.width, bounds.height}, clip);
        const auto& token = services_->components().theme_scope(id)->snapshot().card();
        const auto& m = token.metrics;
        const auto region = [&](runtime::ComponentId component) {
            return services_->nodes().require(services_->components().root(component)).bounds;
        };
        s->snapshot.header_bounds = s->snapshot.has_header ? region(s->snapshot.header) : runtime::Rect{};
        const auto tabs = region(s->snapshot.tabs);
        if (tabs.height > 0) {
            if (!s->snapshot.has_header) {
                s->snapshot.header_bounds = tabs;
            } else {
                s->snapshot.header_bounds.height += tabs.height;
            }
        }
        s->snapshot.body_bounds = region(s->snapshot.loading ? s->snapshot.placeholder : s->snapshot.body);
        s->snapshot.actions_bounds = s->actions_slot ? region(s->snapshot.actions) : runtime::Rect{};
        const float radius = s->grid ? 0 : std::min(m.border_radius, std::min(bounds.width, bounds.height) / 2);
        const auto header = s->snapshot.header_bounds;
        const auto actions = s->snapshot.actions_bounds;
        const auto effective_clip = services_->nodes().content_clip(s->node, clip);
        const auto effect_clip = graphics::EffectClip{1, effective_clip};
        const bool masked = node.rounded_content_radius.has_value();
        const auto current_mask =
            masked ? std::optional{services_->nodes().rounded_content_clip(s->node)} : std::nullopt;
        const auto current_mask_clip = masked ? std::optional{effective_clip} : std::nullopt;
        if (s->last_mask != current_mask || s->last_mask_clip != current_mask_clip) {
            s->last_mask = current_mask;
            s->last_mask_clip = current_mask_clip;
            services_->scene_composer().invalidate_composition();
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::HitTest);
        }
        services_->scene_composer().set_fragment_composition(s->decoration_fragment, false,
                                                             masked ? graphics::DrawCoverageRole::GroupOwnsOuterCoverage
                                                                    : graphics::DrawCoverageRole::Normal);
        std::array<graphics::RoundedEffectInstance, 33 + ShadowList::capacity> decorations;
        std::size_t count = 0;
        const auto fill = [&](runtime::Rect rect, Color color, float rounding, std::array<bool, 4> corners,
                              bool visible = true) {
            rounding = std::min(rounding, std::min(rect.width, rect.height) / 2);
            for (auto effect : graphics::make_corner_fill_effects(
                     {rect, rounding}, corners, color, visible ? node.opacity : 0, node.translation, effect_clip)) {
                decorations[count++] = std::move(effect);
            }
        };
        const auto surface_fill = [&](runtime::Rect band, Color color, bool visible = true) {
            if (!masked) {
                return;
            }
            runtime::Rect translated{band.x + node.translation.x, band.y + node.translation.y, band.width, band.height};
            // Internal bands have hard shared boundaries; their outside Card
            // edge remains the group's single AA boundary, including its guard.
            const float left = band.x == bounds.x ? effective_clip.x : translated.x;
            const float top = band.y == bounds.y ? effective_clip.y : translated.y;
            const float right = band.x + band.width == bounds.x + bounds.width ? effective_clip.x + effective_clip.width
                                                                               : translated.x + translated.width;
            const float bottom = band.y + band.height == bounds.y + bounds.height
                                     ? effective_clip.y + effective_clip.height
                                     : translated.y + translated.height;
            const auto band_clip = graphics::EffectClip{
                1, graphics::intersect_effect_bounds(
                       {left, top, std::max(0.0F, right - left), std::max(0.0F, bottom - top)}, effective_clip)};
            auto effect = graphics::make_shadow_effect({bounds, radius}, {ShadowKind::outer, {}, 0, 0, color},
                                                       node.translation, band_clip);
            effect.material.opacity = visible ? node.opacity : 0;
            decorations[count++] = std::move(effect);
        };
        if (masked) {
            surface_fill(bounds, token.colors.background);
        } else {
            fill(bounds, token.colors.background, radius, {true, true, true, true});
        }
        const bool has_header = s->snapshot.has_header || tabs.height > 0;
        if (masked) {
            surface_fill(header, token.colors.header_background, has_header);
            surface_fill(actions, token.colors.actions_background, s->actions_slot);
        } else {
            fill(header, token.colors.header_background, radius, {true, true, false, false}, has_header);
            fill(actions, token.colors.actions_background, radius, {false, false, true, true}, s->actions_slot);
        }
        fill({header.x, header.y + std::max(0.0F, header.height - m.border_width), header.width,
              has_header ? std::min(m.border_width, header.height) : 0},
             token.colors.separator, 0, {false, false, false, false}, has_header);
        fill({actions.x, actions.y, actions.width, s->actions_slot ? std::min(m.border_width, actions.height) : 0},
             token.colors.separator, 0, {false, false, false, false}, s->actions_slot);
        const auto body = s->snapshot.body_bounds;
        const float padding = std::min(s->size == CardSize::Small ? m.body_padding_small : m.body_padding,
                                       std::min(body.width, body.height) / 2);
        for (std::size_t index = 0; index < 3; ++index) {
            const float height =
                std::min(m.loading_line_height,
                         std::max(0.0F, body.height - padding * 2 -
                                            static_cast<float>(index) * (m.loading_line_height + m.loading_gap)));
            const auto placeholder = token.colors.placeholder;
            const auto animated_color =
                Color(placeholder.red(), placeholder.green(), placeholder.blue(), placeholder.alpha() * s->pulse);
            fill({body.x + padding,
                  body.y + padding + static_cast<float>(index) * (m.loading_line_height + m.loading_gap),
                  std::max(0.0F, body.width - padding * 2) * (index == 2 ? 0.6F : 1.0F),
                  s->snapshot.loading ? height : 0},
                 animated_color, 4, {true, true, true, true}, s->snapshot.loading);
        }
        const bool hovered = s->hoverable && s->snapshot.hovered;
        const float border = std::min(m.border_width, std::min(bounds.width, bounds.height) / 2);
        const runtime::Rect inner{bounds.x + border, bounds.y + border, std::max(0.0F, bounds.width - border * 2),
                                  std::max(0.0F, bounds.height - border * 2)};
        const float border_opacity = border > 0 && s->variant == CardVariant::Outlined && !hovered ? node.opacity : 0;
        if (masked) {
            // An inset band keeps the existing nonnegative outline-offset
            // contract. The group supplies its outer coverage exactly once.
            auto effect =
                graphics::make_shadow_effect({bounds, radius}, {ShadowKind::inset, {}, 0, border, token.colors.border},
                                             node.translation, effect_clip);
            effect.material.opacity = border_opacity;
            decorations[count++] = std::move(effect);
        } else {
            decorations[count++] =
                graphics::make_outline_effect({inner, std::max(0.0F, radius - border)}, border > 0 ? border : 1, 0,
                                              token.colors.border, border_opacity, node.translation, effect_clip);
        }
        const auto shadow_layers = hovered                                 ? token.effects.hover_shadow
                                   : s->variant == CardVariant::Borderless ? token.effects.shadow
                                                                           : ShadowList{};
        std::array<ShadowLayer, ShadowList::capacity> outer_layers;
        std::size_t outer_count = 0;
        for (const auto& layer : shadow_layers.layers()) {
            if (masked && layer.kind == ShadowKind::inset) {
                auto effect = graphics::make_shadow_effect({bounds, radius}, layer, node.translation, effect_clip);
                effect.material.opacity = node.opacity;
                decorations[count++] = std::move(effect);
            } else {
                outer_layers[outer_count++] = layer;
            }
        }
        static_cast<void>(
            services_->surfaces().update_content_effects(s->decorations, std::span{decorations}.first(count)));
        component::RetainedSurfaceEffects effects;
        effects.shape = {{bounds.x, bounds.y, bounds.width, bounds.height}, radius};
        effects.shadows = ShadowList{std::span{outer_layers}.first(outer_count)};
        effects.translation = node.translation;
        effects.shadow_opacity = node.opacity;
        effects.focus_enabled = false;
        effects.ancestor_clip = effect_clip;
        static_cast<void>(services_->surfaces().update_effects(s->surface, effects));
    }
}

std::size_t CardComponentHost::tick_auxiliary(animation::AnimationTime time) {
    std::size_t count{};
    for (auto id : mounted_) {
        auto* s = services_->components().state<CardState>(id);
        if (!s) {
            continue;
        }
        s->last = s->last ? std::max(*s->last, time) : time;
        float pulse = 1;
        const auto bounds = services_->nodes().require(s->node).bounds;
        const bool enabled = s->snapshot.loading && s->animated && services_->focus().state().window_active &&
                             services_->interactions().component_active(id) && s->clip.width > 0 &&
                             s->clip.height > 0 &&
                             services_->motion_preference() != animation::MotionPreference::reduced &&
                             services_->components().theme_scope(id)->snapshot().map().motion;
        if (enabled && bounds.width > 0 && bounds.height > 0) {
            const auto micros = s->last->count_microseconds();
            pulse = 0.7F + 0.3F * std::cos(static_cast<float>(micros % 1400000) / 1400000.0F * 6.2831853F);
        }
        if (s->pulse != pulse) {
            s->pulse = pulse;
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
            ++count;
        }
    }
    return count;
}

std::optional<animation::AnimationTime> CardComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> result;
    for (auto id : mounted_) {
        const auto* s = services_->components().state<CardState>(id);
        if (!s || !s->last || !s->snapshot.loading || !s->animated || !services_->focus().state().window_active ||
            !services_->interactions().component_active(id) || s->clip.width <= 0 || s->clip.height <= 0 ||
            services_->motion_preference() == animation::MotionPreference::reduced ||
            !services_->components().theme_scope(id)->snapshot().map().motion) {
            continue;
        }
        const auto deadline = *s->last + animation::AnimationDuration::microseconds(16667);
        if (!result || deadline < *result) {
            result = deadline;
        }
    }
    return result;
}

void CardComponentHost::mount(const CardGridProps& props, const CardGridContent& content) {
    mount(CardProps{}.hoverable(props.hoverable_).layout(props.layout_),
          CardContent{[content] { SlotContentAccess::function(content)(); }}, {}, true);
}

void CardComponentHost::mount(const CardMetaProps& props, const CardMetaSlots& slots) {
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<MetaState>();
    auto& s = build.state<MetaState>(id);
    s.node = build.root(id);
    s.title_text.set(read_prop(props.title_));
    s.description_text.set(read_prop(props.description_));
    s.rich_title = slots.title.has_value();
    s.rich_description = slots.description.has_value();
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            const auto region = [&](runtime::ComponentId parent, Content content, auto color, auto font) {
                runtime::ComponentId result;
                context.mount_slot(
                    parent, Content{[&] {
                        auto& child = runtime::require_component_build_context();
                        result = child.mount_component<int>(0);
                        const auto node = child.root(result);
                        services.layout().set_layout(node, layout::BoxLayout{});
                        child.on_resource_cleanup(result, [&services, node] { services.layout().remove_layout(node); });
                        child.mount_slot_with_semantic_text_style(result, content,
                                                                  Prop<runtime::SemanticForeground>{color},
                                                                  Prop<runtime::SemanticTypography>{font});
                    }});
                return result;
            };
            s.avatar = region(id, Content{[avatar = slots.avatar] {
                                  if (avatar) {
                                      SlotContentAccess::function (*avatar)();
                                  }
                              }},
                              s.title_color, s.font);
            services.components().set_branch_active(s.avatar, slots.avatar.has_value());
            s.section = context.mount_component<int>(0);
            const auto section_node = context.root(s.section);
            context.on_resource_cleanup(s.section,
                                        [&services, section_node] { services.layout().remove_layout(section_node); });
            s.title = region(s.section, Content{[slot = slots.title, title = s.title_text] {
                                 if (slot) {
                                     SlotContentAccess::function (*slot)();
                                 } else {
                                     Text(TextProps{}.content(title));
                                 }
                             }},
                             s.title_color, s.title_font);
            s.description = region(s.section, Content{[slot = slots.description, text = s.description_text] {
                                       if (slot) {
                                           SlotContentAccess::function (*slot)();
                                       } else {
                                           Text(TextProps{}.content(text));
                                       }
                                   }},
                                   s.description_color, s.font);
        }});
    const auto refresh_meta = [this, id](bool geometry) {
        auto* s = services_->components().state<MetaState>(id);
        if (!s) {
            return;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().card();
        const auto& type = token.typography;
        s->title_color.set(foreground(token.colors.header_text));
        s->description_color.set(foreground(token.colors.meta_description));
        s->font.set({type.font_family, type.font_weight, false, type.font_size, type.line_height});
        s->title_font.set(
            {type.font_family, type.header_font_weight, false, type.header_font_size, type.header_line_height});
        const bool title = s->rich_title || !s->title_text.get().empty();
        const bool description = s->rich_description || !s->description_text.get().empty();
        for (const auto& [part, active] : {std::pair{s->title, title}, std::pair{s->description, description},
                                           std::pair{s->section, title || description}}) {
            if (services_->components().set_branch_active(part, active)) {
                services_->mark_scene_structure_dirty();
            }
        }
        if (geometry) {
            const auto& m = token.metrics;
            services_->layout().set_layout(s->node, layout::FlexLayout{layout::FlexDirection::horizontal,
                                                                       m.header_gap,
                                                                       {0, m.meta_margin_block, 0, m.meta_margin_block},
                                                                       true,
                                                                       false,
                                                                       layout::FlexWrap::no_wrap,
                                                                       layout::FlexJustify::start,
                                                                       layout::FlexAlign::start});
            services_->layout().set_layout(
                services_->components().root(s->section),
                layout::FlexLayout{layout::FlexDirection::vertical, m.meta_gap, {}, true, false});
        }
        services_->dirty().invalidate(s->node, geometry
                                                   ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                         runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                   : runtime::DirtyFlags::Material);
    };
    auto& scope = build.scope(id);
    connect_prop(scope, props.title_, [this, id, refresh_meta](String text) {
        if (auto* s = services_->components().state<MetaState>(id)) {
            s->title_text.set(std::move(text));
            refresh_meta(true);
        }
    });
    connect_prop(scope, props.description_, [this, id, refresh_meta](String text) {
        if (auto* s = services_->components().state<MetaState>(id)) {
            s->description_text.set(std::move(text));
            refresh_meta(true);
        }
    });
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::card_colors, &theme_runtime::ThemeScope::card_metrics,
                               &theme_runtime::ThemeScope::card_typography};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] = theme->capture([refresh_meta, geometry = i != 0](auto) { refresh_meta(geometry); },
                                            [theme, accessor = accessors[i]] { (void)(theme.get()->*accessor)(); });
    }
    refresh_meta(true);
}
} // namespace ryn::detail

namespace ryn {
void CardGrid(CardGridProps props, CardGridContent content) {
    if (!detail::active_cards) {
        throw std::logic_error("CardGrid requires window component services");
    }
    detail::active_cards->mount(props, content);
}

void CardMeta(CardMetaProps props, CardMetaSlots slots) {
    if (!detail::active_cards) {
        throw std::logic_error("CardMeta requires window component services");
    }
    detail::active_cards->mount(props, slots);
}

void Card(CardProps props, CardContent content, CardSlots slots) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_cards) {
        throw std::logic_error("Card requires window component services");
    }
    detail::active_cards->mount(props, content, slots);
}
} // namespace ryn
