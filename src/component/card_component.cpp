#include "component/card_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
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
    Signal<String> title{String{}};
    Signal<runtime::SemanticForeground> body_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> title_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> extra_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> body_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> title_font{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 4> subscriptions;
    runtime::Size title_size;
    runtime::Size extra_size;
    std::array<runtime::Size, 4> region_sizes;
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
        services_->layout().set_layout(services_->components().root(s->snapshot.body),
                                       layout::BoxLayout{{padding, padding, padding, padding}, true});
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
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
}

void CardComponentHost::mount(const CardProps& props, const CardContent& content, const CardSlots& slots) {
    validate(read_prop(props.size_));
    validate(read_prop(props.variant_));
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<CardState>();
    auto& s = build.state<CardState>(id);
    s.node = build.root(id);
    s.size = read_prop(props.size_);
    s.variant = read_prop(props.variant_);
    s.hoverable = read_prop(props.hoverable_);
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
    services.layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        auto& s = *services_->components().state<CardState>(id);
                        const std::array regions{s.snapshot.header, s.snapshot.cover,
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
                        const std::array regions{s.snapshot.header, s.snapshot.cover,
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
        const auto& token = services_->components().theme_scope(id)->snapshot().card();
        const auto& m = token.metrics;
        const auto region = [&](runtime::ComponentId component) {
            return services_->nodes().require(services_->components().root(component)).bounds;
        };
        s->snapshot.header_bounds = s->snapshot.has_header ? region(s->snapshot.header) : runtime::Rect{};
        s->snapshot.body_bounds = region(s->snapshot.loading ? s->snapshot.placeholder : s->snapshot.body);
        s->snapshot.actions_bounds = s->actions_slot ? region(s->snapshot.actions) : runtime::Rect{};
        const float radius = std::min(m.border_radius, std::min(bounds.width, bounds.height) / 2);
        const auto header = s->snapshot.header_bounds;
        const auto actions = s->snapshot.actions_bounds;
        const auto effective_clip = services_->nodes().content_clip(s->node, clip);
        const auto effect_clip = graphics::EffectClip{1, effective_clip};
        std::array<graphics::RoundedEffectInstance, 33> decorations;
        std::size_t count = 0;
        const auto fill = [&](runtime::Rect rect, Color color, float rounding, std::array<bool, 4> corners,
                              bool visible = true) {
            rounding = std::min(rounding, std::min(rect.width, rect.height) / 2);
            for (auto effect : graphics::make_corner_fill_effects(
                     {rect, rounding}, corners, color, visible ? node.opacity : 0, node.translation, effect_clip)) {
                decorations[count++] = std::move(effect);
            }
        };
        fill(bounds, token.colors.background, radius, {true, true, true, true});
        fill(header, token.colors.header_background, radius, {true, true, false, false}, s->snapshot.has_header);
        fill(actions, token.colors.actions_background, radius, {false, false, true, true}, s->actions_slot);
        fill({header.x, header.y + std::max(0.0F, header.height - m.border_width), header.width,
              s->snapshot.has_header ? std::min(m.border_width, header.height) : 0},
             token.colors.separator, 0, {false, false, false, false}, s->snapshot.has_header);
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
            fill({body.x + padding,
                  body.y + padding + static_cast<float>(index) * (m.loading_line_height + m.loading_gap),
                  std::max(0.0F, body.width - padding * 2) * (index == 2 ? 0.6F : 1.0F),
                  s->snapshot.loading ? height : 0},
                 token.colors.placeholder, 4, {true, true, true, true}, s->snapshot.loading);
        }
        const bool hovered = s->hoverable && s->snapshot.hovered;
        const float border = std::min(m.border_width, std::min(bounds.width, bounds.height) / 2);
        const runtime::Rect inner{bounds.x + border, bounds.y + border, std::max(0.0F, bounds.width - border * 2),
                                  std::max(0.0F, bounds.height - border * 2)};
        decorations[count++] = graphics::make_outline_effect(
            {inner, std::max(0.0F, radius - border)}, border > 0 ? border : 1, 0, token.colors.border,
            border > 0 && s->variant == CardVariant::Outlined && !hovered ? node.opacity : 0, node.translation,
            effect_clip);
        static_cast<void>(
            services_->surfaces().update_content_effects(s->decorations, std::span{decorations}.first(count)));
        component::RetainedSurfaceEffects effects;
        effects.shape = {{bounds.x, bounds.y, bounds.width, bounds.height}, radius};
        effects.shadows = hovered                                 ? token.effects.hover_shadow
                          : s->variant == CardVariant::Borderless ? token.effects.shadow
                                                                  : ShadowList{};
        effects.translation = node.translation;
        effects.shadow_opacity = node.opacity;
        effects.focus_enabled = false;
        effects.ancestor_clip = effect_clip;
        static_cast<void>(services_->surfaces().update_effects(s->surface, effects));
    }
}
} // namespace ryn::detail

namespace ryn {
void Card(CardProps props, CardContent content, CardSlots slots) {
    if (!detail::active_cards) {
        throw std::logic_error("Card requires window component services");
    }
    detail::active_cards->mount(props, content, slots);
}
} // namespace ryn
