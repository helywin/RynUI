#include "component/divider_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local DividerComponentHost* active_divider{};

struct DividerState {
    runtime::ComponentId component;
    runtime::ComponentId label;
    runtime::NodeId node;
    component::RetainedSurfaceId range;
    DividerType type{DividerType::Horizontal};
    DividerOrientation orientation{DividerOrientation::Center};
    DividerDirection direction{DividerDirection::LeftToRight};
    DividerVariant variant{DividerVariant::Solid};
    std::optional<ControlSize> size;
    DividerOrientationMargin orientation_margin;
    bool dashed{};
    bool plain{};
    bool slot{};
    bool content_nonempty{};
    bool inherited_line_height{};
    float current_line_height{22};
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
    DividerSnapshot geometry;
    runtime::Size label_size;
    theme_runtime::Subscription colors;
    theme_runtime::Subscription metrics;
    theme_runtime::Subscription fonts;
};

void validate(DividerOrientationMargin value) {
    if (value.source != DividerOrientationMargin::Source::Theme &&
        value.source != DividerOrientationMargin::Source::None &&
        value.source != DividerOrientationMargin::Source::Ratio &&
        value.source != DividerOrientationMargin::Source::Length) {
        throw std::invalid_argument("Divider orientation margin source is invalid");
    }
    if (value.source == DividerOrientationMargin::Source::Ratio &&
        (!std::isfinite(value.ratio) || value.ratio < 0 || value.ratio > 1)) {
        throw std::invalid_argument("Divider orientation margin ratio must be in [0,1]");
    }
    if (value.source == DividerOrientationMargin::Source::Length &&
        (!std::isfinite(value.logical_length) || value.logical_length < 0)) {
        throw std::invalid_argument("Divider orientation margin length must be finite and non-negative");
    }
}

void validate(DividerType value) {
    if (value != DividerType::Horizontal && value != DividerType::Vertical) {
        throw std::invalid_argument("Divider type is invalid");
    }
}

void validate(DividerOrientation value) {
    if (value != DividerOrientation::Left && value != DividerOrientation::Center &&
        value != DividerOrientation::Right && value != DividerOrientation::Start && value != DividerOrientation::End) {
        throw std::invalid_argument("Divider orientation is invalid");
    }
}

void validate(DividerDirection value) {
    if (value != DividerDirection::LeftToRight && value != DividerDirection::RightToLeft) {
        throw std::invalid_argument("Divider direction is invalid");
    }
}

void validate(DividerVariant value) {
    if (value != DividerVariant::Solid && value != DividerVariant::Dashed && value != DividerVariant::Dotted) {
        throw std::invalid_argument("Divider variant is invalid");
    }
}

void validate(ControlSize value) {
    if (value != ControlSize::Small && value != ControlSize::Middle && value != ControlSize::Large) {
        throw std::invalid_argument("Divider size is invalid");
    }
}

DividerVariant effective_variant(const DividerState& state) {
    if (state.variant == DividerVariant::Dotted) {
        return DividerVariant::Dotted;
    }
    return state.dashed || state.variant == DividerVariant::Dashed ? DividerVariant::Dashed : DividerVariant::Solid;
}

DividerOrientation physical_orientation(const DividerState& state) {
    if (state.orientation == DividerOrientation::Start) {
        return state.direction == DividerDirection::LeftToRight ? DividerOrientation::Left : DividerOrientation::Right;
    }
    if (state.orientation == DividerOrientation::End) {
        return state.direction == DividerDirection::LeftToRight ? DividerOrientation::Right : DividerOrientation::Left;
    }
    return state.orientation;
}
} // namespace

DividerComponentHost::DividerComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

DividerComponentHost::~DividerComponentHost() {
    services_->detach(*this);
}

void* DividerComponentHost::begin_mount() noexcept {
    auto* previous = active_divider;
    active_divider = this;
    return previous;
}

void DividerComponentHost::end_mount(void* previous) noexcept {
    active_divider = static_cast<DividerComponentHost*>(previous);
}

void DividerComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

DividerSnapshot DividerComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<DividerState>(id);
    if (!s) {
        throw std::out_of_range("Divider component is stale");
    }
    return s->geometry;
}

void DividerComponentHost::update(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<DividerState>(id);
    if (!s) {
        return;
    }
    const auto& theme = services_->components().theme_scope(id)->snapshot();
    const auto& token = theme.divider();
    const auto color = s->plain ? token.colors.plain_text : token.colors.text;
    s->foreground.set({color.red(), color.green(), color.blue(), color.alpha()});
    const auto& typography = theme.typography();
    const float size = s->plain ? token.typography.plain_font_size : token.typography.text_font_size;
    s->typography.set({typography.font_family,
                       s->plain ? token.typography.plain_font_weight : token.typography.text_font_weight, false, size,
                       size * typography.base_line_height / typography.base_font_size});
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry
                                                    : runtime::DirtyFlags::Material);
}

void DividerComponentHost::mount(const DividerProps& props, const std::optional<DividerText>& text) {
    validate(read_prop(props.type_));
    validate(read_prop(props.orientation_));
    validate(read_prop(props.direction_));
    validate(read_prop(props.variant_));
    validate(read_prop(props.margin_));
    if (props.size_) {
        validate(read_prop(*props.size_));
    }
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<DividerState>();
    auto& s = build.state<DividerState>(id);
    s.component = id;
    s.node = build.root(id);
    s.slot = text.has_value();
    s.type = read_prop(props.type_);
    s.orientation = read_prop(props.orientation_);
    s.direction = read_prop(props.direction_);
    s.variant = read_prop(props.variant_);
    if (props.size_) {
        s.size = read_prop(*props.size_);
    }
    s.orientation_margin = read_prop(props.margin_);
    validate(s.orientation_margin);
    s.plain = read_prop(props.plain_);
    s.dashed = read_prop(props.dashed_);
    s.content_nonempty = !read_prop(props.content_).empty();
    s.current_line_height = build.semantic_typography() ? read_prop(*build.semantic_typography()).line_height
                                                        : build.theme_scope()->snapshot().typography().base_line_height;
    s.inherited_line_height = build.semantic_typography().has_value();
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.range = services.surfaces().create_content_range(fragment, {});
    build.on_resource_cleanup(id, [&services, range = s.range, node = s.node] {
        services.surfaces().destroy_content_range(range);
        services.layout().remove_layout(node);
    });
    update(id, true);
    build.mount_slot(id, Content{[&services, &s, text, content = props.content_] {
                         auto& context = runtime::require_component_build_context();
                         s.label = context.mount_component<int>(0);
                         services.layout().set_layout(context.root(s.label), layout::BoxLayout{});
                         context.on_resource_cleanup(s.label, [&services, node = context.root(s.label)] {
                             services.layout().remove_layout(node);
                         });
                         context.mount_slot_with_semantic_text_style(s.label, Content{[text, content] {
                                                                         if (text) {
                                                                             SlotContentAccess::function (*text)();
                                                                         } else {
                                                                             ryn::Text(TextProps{}.content(content));
                                                                         }
                                                                     }},
                                                                     Prop<runtime::SemanticForeground>{s.foreground},
                                                                     Prop<runtime::SemanticTypography>{s.typography});
                     }});
    if (!text) {
        const auto& mounted = services.text().mounted_texts().back();
        services.text().scene_service().set_ellipsis(mounted.scene, {1, String{u8"…"}, false, 0});
        const auto revisions = services.text().scene_service().revisions(mounted.scene);
        services.layout().set_intrinsic_revision(services.components().root(mounted.component),
                                                 revisions.content + revisions.layout);
    }
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                auto& s = *services_->components().state<DividerState>(id);
                const auto& token = services_->components().theme_scope(id)->snapshot().divider();
                s.geometry = {};
                s.geometry.vertical = s.type == DividerType::Vertical;
                s.geometry.variant = effective_variant(s);
                s.geometry.dashed = s.geometry.variant == DividerVariant::Dashed;
                s.geometry.size = s.size.value_or(ControlSize::Large);
                s.geometry.orientation = s.orientation;
                s.geometry.direction = s.direction;
                s.geometry.plain = s.plain;
                s.geometry.line_width = token.metrics.line_width;
                if (s.geometry.vertical) {
                    if (services_->components().set_branch_active(s.label, false)) {
                        services_->mark_scene_structure_dirty();
                    }
                    s.geometry.margin = token.metrics.vertical_margin_inline;
                    s.geometry.left = {s.geometry.margin, -0.06F * s.current_line_height, token.metrics.line_width,
                                       0.9F * s.current_line_height};
                    return limits.constrain(
                        {2 * s.geometry.margin + token.metrics.line_width, 0.9F * s.current_line_height});
                }
                const bool potential_label = s.slot || s.content_nonempty;
                const auto orientation = physical_orientation(s);
                float padding = token.metrics.text_padding_inline;
                const bool no_margin = s.orientation_margin.source == DividerOrientationMargin::Source::None;
                const bool length_margin = s.orientation_margin.source == DividerOrientationMargin::Source::Length &&
                                           orientation != DividerOrientation::Center;
                float margin_length = length_margin ? s.orientation_margin.logical_length : 0;
                if (no_margin && orientation != DividerOrientation::Center) {
                    padding = 0;
                }
                if (std::isfinite(limits.max_width)) {
                    margin_length = std::min(margin_length, limits.max_width);
                    padding =
                        std::min(padding, length_margin ? limits.max_width - margin_length : limits.max_width * 0.5F);
                }
                const float horizontal_insets = length_margin ? margin_length + padding : 2 * padding;
                s.label_size = potential_label
                                   ? engine.measure_child(services_->components().root(s.label),
                                                          {0, std::max(0.0F, limits.max_width - horizontal_insets), 0,
                                                           limits.max_height})
                                   : runtime::Size{};
                const bool label = potential_label && (s.content_nonempty || s.label_size.width > 0);
                if (services_->components().set_branch_active(s.label, label)) {
                    services_->mark_scene_structure_dirty();
                }
                s.geometry.has_label = label;
                s.geometry.margin = label ? token.metrics.horizontal_with_text_margin : token.metrics.horizontal_margin;
                if (s.size == ControlSize::Small) {
                    s.geometry.margin = token.metrics.small_horizontal_margin;
                } else if (s.size == ControlSize::Middle) {
                    s.geometry.margin = token.metrics.middle_horizontal_margin;
                }
                const float width = std::isfinite(limits.max_width)
                                        ? limits.max_width
                                        : s.label_size.width + horizontal_insets + 2 * token.metrics.line_width;
                const float content_height =
                    label ? std::max(token.metrics.line_width, s.label_size.height) : token.metrics.line_width;
                const float height = content_height + 2 * s.geometry.margin;
                const float y = s.geometry.margin + (content_height - token.metrics.line_width) * 0.5F;
                if (!label) {
                    s.geometry.left = {0, y, width, token.metrics.line_width};
                    return limits.constrain({width, height});
                }
                const float occupied = std::min(width, s.label_size.width + horizontal_insets);
                const float remaining = std::max(0.0F, width - occupied);
                float left = remaining * 0.5F;
                if (orientation != DividerOrientation::Center) {
                    float ratio = s.orientation_margin.source == DividerOrientationMargin::Source::Ratio
                                      ? s.orientation_margin.ratio
                                      : token.metrics.orientation_margin;
                    if (no_margin || length_margin) {
                        ratio = 0;
                    }
                    const float edge = std::clamp(width * ratio, 0.0F, remaining);
                    left = orientation == DividerOrientation::Left ? edge : remaining - edge;
                }
                s.geometry.left = {0, y, left, token.metrics.line_width};
                s.geometry.right = {left + occupied, y, remaining - left, token.metrics.line_width};
                const float label_inset =
                    length_margin && orientation == DividerOrientation::Left ? margin_length : padding;
                s.geometry.label = {left + label_inset, s.geometry.margin,
                                    std::min(s.label_size.width, std::max(0.0F, width - horizontal_insets)),
                                    s.label_size.height};
                return limits.constrain({width, height});
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                auto& s = *services_->components().state<DividerState>(id);
                if (!s.geometry.has_label) {
                    return;
                }
                const auto label = s.geometry.label;
                engine.place_child(services_->components().root(s.label),
                                   {bounds.x + label.x, bounds.y + label.y, label.width, label.height});
            }});
    mounted_.push_back(id);
    auto& scope = build.scope(id);
    connect_prop(scope, props.type_, [this, id](DividerType value) {
        validate(value);
        if (auto* s = services_->components().state<DividerState>(id)) {
            s->type = value;
            update(id, true);
        }
    });
    connect_prop(scope, props.orientation_, [this, id](DividerOrientation value) {
        validate(value);
        if (auto* s = services_->components().state<DividerState>(id)) {
            s->orientation = value;
            update(id, true);
        }
    });
    connect_prop(scope, props.margin_, [this, id](DividerOrientationMargin value) {
        validate(value);
        if (auto* s = services_->components().state<DividerState>(id)) {
            s->orientation_margin = value;
            update(id, true);
        }
    });
    connect_prop(scope, props.dashed_, [this, id](bool value) {
        if (auto* s = services_->components().state<DividerState>(id)) {
            s->dashed = value;
            s->geometry.variant = effective_variant(*s);
            s->geometry.dashed = s->geometry.variant == DividerVariant::Dashed;
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry);
        }
    });
    connect_prop(scope, props.variant_, [this, id](DividerVariant value) {
        validate(value);
        if (auto* s = services_->components().state<DividerState>(id)) {
            s->variant = value;
            s->geometry.variant = effective_variant(*s);
            s->geometry.dashed = s->geometry.variant == DividerVariant::Dashed;
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry);
        }
    });
    connect_prop(scope, props.direction_, [this, id](DividerDirection value) {
        validate(value);
        if (auto* s = services_->components().state<DividerState>(id)) {
            s->direction = value;
            update(id, true);
        }
    });
    if (props.size_) {
        connect_prop(scope, *props.size_, [this, id](ControlSize value) {
            validate(value);
            if (auto* s = services_->components().state<DividerState>(id)) {
                s->size = value;
                update(id, true);
            }
        });
    }
    connect_prop(scope, props.plain_, [this, id](bool value) {
        if (auto* s = services_->components().state<DividerState>(id)) {
            s->plain = value;
            update(id, true);
        }
    });
    connect_prop(scope, props.content_, [this, id](String value) {
        if (auto* s = services_->components().state<DividerState>(id)) {
            s->content_nonempty = !value.empty();
            update(id, true);
        }
    });
    if (build.semantic_typography()) {
        connect_prop(scope, *build.semantic_typography(), [this, id](runtime::SemanticTypography value) {
            if (auto* s = services_->components().state<DividerState>(id)) {
                s->current_line_height = value.line_height;
                update(id, true);
            }
        });
    }
    const auto theme = build.theme_scope();
    s.colors = theme->capture([this, id](theme_runtime::DirtyPhase) { update(id, false); },
                              [theme] { (void)theme->divider_colors(); });
    s.metrics = theme->capture([this, id](theme_runtime::DirtyPhase) { update(id, true); },
                               [theme] { (void)theme->divider_metrics(); });
    s.fonts = theme->capture(
        [this, id](theme_runtime::DirtyPhase) {
            if (auto* s = services_->components().state<DividerState>(id); s && !s->inherited_line_height) {
                s->current_line_height =
                    services_->components().theme_scope(id)->snapshot().typography().base_line_height;
            }
            update(id, true);
        },
        [theme] {
            (void)theme->divider_typography();
            (void)theme->typography_fonts();
            (void)theme->typography_base_typography();
        });
}

void DividerComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    for (auto id : mounted_) {
        auto* s = services_->components().state<DividerState>(id);
        if (!s) {
            continue;
        }
        const auto& node = services_->nodes().require(s->node);
        const auto color = services_->components().theme_scope(id)->snapshot().divider().colors.line;
        std::vector<graphics::QuadInstance> quads;
        std::vector<graphics::RoundedEffectInstance> effects;
        const auto variant = effective_variant(*s);
        const float diameter = s->geometry.line_width;
        const float segment_length = variant == DividerVariant::Dashed ? 3 * diameter : diameter;
        const auto count_segments = [&](runtime::Rect rect) -> std::size_t {
            if (rect.width <= 0 || rect.height <= 0 || segment_length <= 0) {
                return 0;
            }
            if (variant == DividerVariant::Solid) {
                return 1;
            }
            const double length = s->geometry.vertical ? rect.height : rect.width;
            const double count = std::ceil(length / (2.0 * segment_length));
            if (!std::isfinite(count) || count > component::retained_content_visual_capacity) {
                throw std::length_error("Divider decoration exceeds the primitive limit");
            }
            return static_cast<std::size_t>(count);
        };
        const auto left_count = count_segments(s->geometry.left);
        const auto right_count = count_segments(s->geometry.right);
        if (left_count + right_count > component::retained_content_visual_capacity) {
            throw std::length_error("Divider decoration exceeds the primitive limit");
        }
        const auto append = [&](runtime::Rect rect) {
            rect.x += node.bounds.x + node.translation.x;
            rect.y += node.bounds.y + node.translation.y;
            const float right = std::min(rect.x + rect.width, clip.x + clip.width);
            const float bottom = std::min(rect.y + rect.height, clip.y + clip.height);
            rect.x = std::max(rect.x, clip.x);
            rect.y = std::max(rect.y, clip.y);
            rect.width = std::max(0.0F, right - rect.x);
            rect.height = std::max(0.0F, bottom - rect.y);
            if (rect.width <= 0 || rect.height <= 0) {
                return;
            }
            graphics::QuadInstance quad;
            quad.bounds = {rect.x, rect.y, rect.width, rect.height};
            quad.color = {color.red(), color.green(), color.blue(), color.alpha()};
            quad.opacity = node.opacity;
            quads.push_back(quad);
        };
        const auto rail = [&](runtime::Rect rect, std::size_t count, std::uint64_t clip_identity) {
            if (variant == DividerVariant::Solid) {
                append(rect);
                return;
            }
            const float length = s->geometry.vertical ? rect.height : rect.width;
            if (count == 0) {
                return;
            }
            const runtime::Rect world_rail{rect.x + node.bounds.x + node.translation.x,
                                           rect.y + node.bounds.y + node.translation.y, rect.width, rect.height};
            const float clip_x = std::max(clip.x, world_rail.x);
            const float clip_y = std::max(clip.y, world_rail.y);
            const runtime::Rect rail_clip{
                clip_x, clip_y, std::max(0.0F, std::min(clip.x + clip.width, world_rail.x + world_rail.width) - clip_x),
                std::max(0.0F, std::min(clip.y + clip.height, world_rail.y + world_rail.height) - clip_y)};
            for (std::size_t index = 0; index < count; ++index) {
                const auto offset = static_cast<float>(index * (2.0 * segment_length));
                auto segment = rect;
                if (s->geometry.vertical) {
                    segment.y += offset;
                    segment.height = std::min(segment_length, length - offset);
                } else {
                    segment.x += offset;
                    segment.width = std::min(segment_length, length - offset);
                }
                if (variant == DividerVariant::Dotted) {
                    if (rail_clip.width <= 0 || rail_clip.height <= 0) {
                        continue;
                    }
                    segment.width = diameter;
                    segment.height = diameter;
                    segment.x += node.bounds.x;
                    segment.y += node.bounds.y;
                    graphics::RoundedEffectInstance dot;
                    dot.geometry.shape = {segment, diameter * 0.5F};
                    dot.geometry.translation = node.translation;
                    dot.geometry.ancestor_clip = graphics::EffectClip{clip_identity, rail_clip};
                    dot.material = {color, node.opacity, true};
                    effects.push_back(dot);
                } else {
                    append(segment);
                }
            }
        };
        rail(s->geometry.left, left_count, 1);
        rail(s->geometry.right, right_count, 2);
        (void)services_->surfaces().update_content_range(s->range, quads);
        (void)services_->surfaces().update_content_effects(s->range, effects);
    }
}
} // namespace ryn::detail

namespace ryn {
void Divider(DividerProps props) {
    if (!detail::active_divider) {
        throw std::logic_error("Divider requires window component services");
    }
    detail::active_divider->mount(props, {});
}

void Divider(DividerProps props, DividerText text) {
    if (!detail::active_divider) {
        throw std::logic_error("Divider requires window component services");
    }
    detail::active_divider->mount(props, text);
}
} // namespace ryn
