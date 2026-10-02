#include "component/space_compact.hpp"

#include "layout/flex_distribution.hpp"

#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {

struct SpaceCompactPropsAccess final {
    static const auto& orientation(const SpaceCompactProps& props) {
        return props.orientation_;
    }

    static const auto& direction(const SpaceCompactProps& props) {
        return props.direction_;
    }

    static const auto& size(const SpaceCompactProps& props) {
        return props.size_;
    }

    static bool explicit_size(const SpaceCompactProps& props) {
        return props.explicit_size_;
    }

    static const auto& block(const SpaceCompactProps& props) {
        return props.block_;
    }

    static const auto& layout(const SpaceCompactProps& props) {
        return props.layout_;
    }
};

namespace {
void validate(SpaceOrientation value) {
    if (value != SpaceOrientation::Horizontal && value != SpaceOrientation::Vertical) {
        throw std::invalid_argument("SpaceCompact orientation is invalid");
    }
}

void validate(ControlSize value) {
    if (value != ControlSize::Small && value != ControlSize::Middle && value != ControlSize::Large) {
        throw std::invalid_argument("SpaceCompact size is invalid");
    }
}

bool rtl(FlexDirection value) {
    if (value != FlexDirection::LeftToRight && value != FlexDirection::RightToLeft) {
        throw std::invalid_argument("SpaceCompact direction is invalid");
    }
    return value == FlexDirection::RightToLeft;
}

std::array<bool, 4> corners(const CompactMetadata& metadata) {
    if (metadata.orientation == SpaceOrientation::Vertical) {
        return {metadata.first, metadata.first, metadata.last, metadata.last};
    }
    return metadata.right_to_left ? std::array{metadata.last, metadata.first, metadata.first, metadata.last}
                                  : std::array{metadata.first, metadata.last, metadata.last, metadata.first};
}
} // namespace

CompactContext::CompactContext(runtime::ComponentHost& host, LayoutComponentServices services,
                               runtime::ComponentId component)
    : host_(&host), services_(services), component_(component), node_(host.root(component)) {}

runtime::ComponentId CompactContext::direct_child(runtime::ComponentId component) const {
    for (auto current = component; host_->contains(current);) {
        const auto parent = host_->parent(current);
        if (parent == component_) {
            return current;
        }
        if (!parent) {
            break;
        }
        current = *parent;
    }
    return {};
}

void CompactContext::claim(runtime::ComponentId component) {
    claims_.push_back(component);
}

bool CompactContext::owns_ancestor(const runtime::ComponentBuildContext& build) const {
    return std::any_of(claims_.begin(), claims_.end(), [&](const auto id) { return build.has_ancestor(id); });
}

void CompactContext::attach(runtime::ComponentId component, std::function<void(const CompactMetadata&)> apply,
                            std::function<CompactBorder()> border, component::RetainedSurfaceService* surfaces,
                            bool intrinsic_minimum) {
    if (members_.size() >= component::retained_content_visual_capacity / 4) {
        throw std::length_error("SpaceCompact exceeds 1024 supported members");
    }
    attach_many(
        component, std::move(apply),
        [border = std::move(border)] { return border ? std::vector{border()} : std::vector<CompactBorder>{}; },
        surfaces, intrinsic_minimum);
    if (surfaces) {
        surfaces_ = surfaces;
    }
}

void CompactContext::attach_many(runtime::ComponentId component, std::function<void(const CompactMetadata&)> apply,
                                 std::function<std::vector<CompactBorder>()> borders,
                                 component::RetainedSurfaceService* surfaces, bool intrinsic_minimum) {
    if (members_.size() >= component::retained_content_visual_capacity / 4) {
        throw std::length_error("SpaceCompact exceeds 1024 supported members");
    }
    members_.push_back({component, std::move(apply), std::move(borders), intrinsic_minimum});
    if (surfaces) {
        surfaces_ = surfaces;
    }
}

std::vector<CompactBorder> CompactContext::borders() const {
    std::vector<CompactBorder> result;
    if (!active_) {
        return result;
    }
    for (const auto& member : members_) {
        if (host_->contains(member.component) && member.borders) {
            const auto values = member.borders();
            result.insert(result.end(), values.begin(), values.end());
        }
    }
    return result;
}

std::vector<runtime::ComponentId> CompactContext::flow_children() const {
    std::vector<runtime::ComponentId> result;
    for (const auto child : host_->children(component_)) {
        const auto* nested = host_->state<SpaceCompactState>(child);
        if (!nested || nested->context->has_items()) {
            result.push_back(child);
        }
    }
    return result;
}

bool CompactContext::has_items() const {
    return active_ && !flow_children().empty();
}

void CompactContext::detach(runtime::ComponentId component) {
    if (!active_) {
        return;
    }
    std::erase(claims_, component);
    std::erase_if(members_, [component](const auto& member) { return member.component == component; });
    if (host_->active() && host_->contains(component_) && host_->scope(component_).active()) {
        refresh();
    }
}

void CompactContext::refresh(bool measure) {
    if (!active_ || refreshing_ || !host_->contains(component_)) {
        return;
    }
    refreshing_ = true;
    try {
        const auto children = flow_children();
        for (std::size_t i = 0; i < members_.size(); ++i) {
            auto& member = members_[i];
            if (!host_->contains(member.component)) {
                continue;
            }
            const auto root = direct_child(member.component);
            const auto found = std::find(children.begin(), children.end(), root);
            if (found == children.end()) {
                continue;
            }
            auto value = metadata;
            value.first = found == children.begin() && (i == 0 || direct_child(members_[i - 1].component) != root);
            value.last = found + 1 == children.end() &&
                         (i + 1 == members_.size() || direct_child(members_[i + 1].component) != root);
            value.corners = corners(value);
            for (std::size_t corner = 0; corner < value.corners.size(); ++corner) {
                value.corners[corner] = value.corners[corner] && outer_corners[corner];
            }
            member.apply(value);
        }
        refreshing_ = false;
    } catch (...) {
        refreshing_ = false;
        throw;
    }
    services_.dirty.invalidate_subtree(
        node_, (measure ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout : runtime::DirtyFlags::Placement) |
                   runtime::DirtyFlags::Geometry);
    publish_seams();
}

float CompactContext::overlap(runtime::NodeId left, runtime::NodeId right) const {
    float left_width{};
    float right_width{};
    for (const auto& member : members_) {
        if (!host_->contains(member.component) || !member.borders) {
            continue;
        }
        const auto root = direct_child(member.component);
        if (root.valid()) {
            const auto node = host_->root(root);
            for (const auto& border : member.borders()) {
                if (node == left) {
                    left_width = std::max(left_width, border.width);
                }
                if (node == right) {
                    right_width = std::max(right_width, border.width);
                }
            }
        }
    }
    return std::min(left_width, right_width);
}

runtime::Size CompactContext::measure(layout::LayoutEngine& engine, layout::Constraints constraints) {
    auto& node = services_.nodes.require(node_);
    const bool vertical = metadata.orientation == SpaceOrientation::Vertical;

    struct Item final {
        runtime::NodeId id;
        float main_size;
        float base_main_size;
        float min_main_size;
        float max_main_size;
        float grow;
        float shrink;
        bool frozen{};
    };

    std::vector<Item> items;
    float main{};
    float cross{};
    float joined_extent{};
    node.first_baseline.reset();
    std::optional<runtime::NodeId> previous;
    const layout::Constraints child_constraints{0, constraints.max_width, 0, constraints.max_height};
    for (const auto component : flow_children()) {
        const auto child = host_->root(component);
        const auto& style = services_.nodes.require(child).external_layout;
        const float margin = vertical ? style.margin.top + style.margin.bottom : style.margin.left + style.margin.right;
        float minimum = (vertical ? style.min_height : style.min_width).value_or(0) + margin;
        const float maximum =
            (vertical ? style.max_height : style.max_width).value_or(std::numeric_limits<float>::infinity()) + margin;
        const bool automatic_minimum = !(vertical ? style.min_height : style.min_width) &&
                                       std::any_of(members_.begin(), members_.end(), [component](const auto& member) {
                                           return member.component == component && member.intrinsic_minimum;
                                       });
        std::optional<runtime::Size> natural;
        if (automatic_minimum) {
            natural = engine.measure_child(child, child_constraints);
            minimum = vertical ? natural->height : natural->width;
        }
        const auto basis =
            style.flex_basis ? std::optional{std::clamp(*style.flex_basis + margin, minimum, maximum)} : std::nullopt;
        const auto size = natural && !basis
                              ? *natural
                              : engine.measure_child(child, child_constraints, vertical ? std::nullopt : basis,
                                                     vertical ? basis : std::nullopt);
        if (previous) {
            const float joined =
                std::min(overlap(*previous, child), std::min(main, vertical ? size.height : size.width));
            main -= joined;
            joined_extent += joined;
        }
        const float extent = vertical ? size.height : size.width;
        items.push_back({child, extent, extent, minimum, maximum, style.flex_grow, style.flex_shrink});
        main += vertical ? size.height : size.width;
        cross = std::max(cross, vertical ? size.width : size.height);
        previous = child;
    }
    runtime::Size result = vertical ? runtime::Size{cross, main} : runtime::Size{main, cross};
    if (block && std::isfinite(constraints.max_width)) {
        result.width = constraints.max_width;
    }
    result = constraints.constrain(result);
    layout::distribute_flex_space(std::span{items}, (vertical ? result.height : result.width) - main);
    main = -joined_extent;
    cross = 0;
    for (const auto& item : items) {
        auto size = services_.nodes.require(item.id).measured_size;
        if (std::abs(item.main_size - item.base_main_size) > layout::flex_distribution_epsilon) {
            size = engine.measure_child(item.id, child_constraints,
                                        vertical ? std::nullopt : std::optional{item.main_size},
                                        vertical ? std::optional{item.main_size} : std::nullopt);
        }
        main += vertical ? size.height : size.width;
        cross = std::max(cross, vertical ? size.width : size.height);
        const auto& child = services_.nodes.require(item.id);
        if (!node.first_baseline && child.first_baseline) {
            node.first_baseline = child.external_layout.margin.top + *child.first_baseline;
        }
    }
    result = constraints.constrain(vertical ? runtime::Size{std::max(result.width, cross), main}
                                            : runtime::Size{std::max(result.width, main), cross});
    if (vertical) {
        // Column groups stretch automatic inline widths while preserving explicit
        // width and min/max constraints, just like the shared Flex contract.
        float height{};
        node.first_baseline.reset();
        previous.reset();
        for (const auto component : flow_children()) {
            const auto child = host_->root(component);
            auto& item = services_.nodes.require(child);
            auto size = item.measured_size;
            if (!item.external_layout.width) {
                const float margin = item.external_layout.margin.left + item.external_layout.margin.right;
                const float width =
                    std::clamp(std::max(0.0F, result.width - margin), item.external_layout.min_width.value_or(0),
                               item.external_layout.max_width.value_or(std::numeric_limits<float>::infinity())) +
                    margin;
                size = engine.measure_child(child, {width, width, 0, constraints.max_height});
            }
            if (previous) {
                height -= std::min(overlap(*previous, child), std::min(height, size.height));
            }
            if (!node.first_baseline && item.first_baseline) {
                node.first_baseline = height + item.external_layout.margin.top + *item.first_baseline;
            }
            height += size.height;
            previous = child;
        }
        result.height = std::clamp(height, constraints.min_height, constraints.max_height);
    }
    return result;
}

void CompactContext::place(layout::LayoutEngine& engine, runtime::Rect bounds) {
    const auto children = flow_children();
    const bool vertical = metadata.orientation == SpaceOrientation::Vertical;
    float offset{};
    std::optional<runtime::NodeId> previous;
    for (const auto component : children) {
        const auto child = host_->root(component);
        const auto size = services_.nodes.require(child).measured_size;
        if (previous) {
            offset -= std::min(overlap(*previous, child), std::min(offset, vertical ? size.height : size.width));
        }
        const float x = vertical ? (metadata.right_to_left ? bounds.width - size.width : 0)
                                 : (metadata.right_to_left ? bounds.width - offset - size.width : offset);
        engine.place_child(child, {bounds.x + x, bounds.y + (vertical ? offset : 0), size.width, size.height});
        offset += vertical ? size.height : size.width;
        previous = child;
    }
    publish_seams();
}

void CompactContext::publish_seams() {
    if (!active_ || !surfaces_ || !host_->contains(component_)) {
        return;
    }
    std::vector<graphics::RoundedEffectInstance> effects;
    std::vector<CompactBorder> previous;
    for (const auto& member : members_) {
        if (!member.borders || !host_->contains(member.component)) {
            continue;
        }
        auto current = member.borders();
        if (current.empty()) {
            continue;
        }
        for (const auto& a : previous) {
            for (const auto& b : current) {
                auto a_bounds = a.shape.rect;
                a_bounds.x += a.translation.x;
                a_bounds.y += a.translation.y;
                auto b_bounds = b.shape.rect;
                b_bounds.x += b.translation.x;
                b_bounds.y += b.translation.y;
                const auto seam = graphics::intersect_effect_bounds(a_bounds, b_bounds);
                if (seam.width <= 0 || seam.height <= 0) {
                    continue;
                }
                const auto& winner = a.priority > b.priority ? a : b;
                if (!winner.visible || winner.width <= 0) {
                    continue;
                }
                if (winner.dashed) {
                    for (auto decoration : winner.decorations) {
                        const auto clip =
                            decoration.geometry.ancestor_clip
                                ? graphics::intersect_effect_bounds(seam, decoration.geometry.ancestor_clip->bounds)
                                : seam;
                        if (clip.width <= 0 || clip.height <= 0) {
                            continue;
                        }
                        if (effects.size() >= component::retained_content_visual_capacity) {
                            throw std::length_error("SpaceCompact seam exceeds 4096 effects");
                        }
                        decoration.geometry.ancestor_clip = graphics::EffectClip{effects.size() + 1, clip};
                        effects.push_back(decoration);
                    }
                    continue;
                }
                const auto clip = winner.clip ? graphics::intersect_effect_bounds(seam, winner.clip->bounds) : seam;
                const float width =
                    std::min(winner.width, 0.5F * std::min(winner.shape.rect.width, winner.shape.rect.height));
                if (width <= 0) {
                    continue;
                }
                auto shape = winner.shape;
                shape.rect.x += width;
                shape.rect.y += width;
                shape.rect.width = std::max(0.0F, shape.rect.width - 2 * width);
                shape.rect.height = std::max(0.0F, shape.rect.height - 2 * width);
                shape.radius =
                    std::clamp(shape.radius - width, 0.0F, 0.5F * std::min(shape.rect.width, shape.rect.height));
                const auto outline = graphics::make_corner_outline_effects(
                    shape, winner.corners, width, 0, winner.color, 1, winner.translation,
                    graphics::EffectClip{effects.size() + 1, clip});
                if (effects.size() + outline.size() > component::retained_content_visual_capacity) {
                    throw std::length_error("SpaceCompact seam exceeds 4096 effects");
                }
                effects.insert(effects.end(), outline.begin(), outline.end());
            }
        }
        previous = std::move(current);
    }
    if (!seam_range_.valid() && !effects.empty()) {
        seam_fragment_ = host_->register_scene_fragment(component_, runtime::SceneFragmentPlacement::after_children);
        seam_range_ = surfaces_->create_content_range(seam_fragment_, {});
    }
    if (seam_range_.valid()) {
        surfaces_->update_content_effects(seam_range_, effects);
    }
    seams_ = std::move(effects);
    if (const auto parent = parent_context.lock()) {
        parent->publish_seams();
    }
}

void CompactContext::dispose() noexcept {
    active_ = false;
    members_.clear();
    claims_.clear();
    seams_.clear();
    if (surfaces_ && seam_range_.valid()) {
        try {
            surfaces_->destroy_content_range(seam_range_);
        } catch (...) {
        }
    }
}

std::shared_ptr<CompactContext> nearest_compact(runtime::ComponentBuildContext& build) {
    // The lifetime scope belongs to the current parent, including transparent Theme slots.
    const auto* state = build.nearest_state<SpaceCompactState>(true);
    return state && !state->context->owns_ancestor(build) ? state->context : nullptr;
}

void mount_space_compact(const SpaceCompactProps& props, const SpaceCompactContent& content) {
    auto& build = runtime::require_component_build_context();
    auto& services = require_layout_component_services();
    const auto parent = nearest_compact(build);
    const auto orientation = read_prop(SpaceCompactPropsAccess::orientation(props));
    const auto size = read_prop(SpaceCompactPropsAccess::size(props));
    const auto right_to_left = rtl(read_prop(SpaceCompactPropsAccess::direction(props)));
    validate(orientation);
    validate(size);
    const auto component = build.mount_component<SpaceCompactState>();
    auto& state = build.state<SpaceCompactState>(component);
    const auto context = std::make_shared<CompactContext>(build.host(), services, component);
    state.context = context;
    context->parent_context = parent;
    context->explicit_size = SpaceCompactPropsAccess::explicit_size(props);
    context->metadata = {.size = parent && !context->explicit_size ? parent->metadata.size : size,
                         .orientation = orientation,
                         .right_to_left = right_to_left};
    context->block = read_prop(SpaceCompactPropsAccess::block(props));
    const auto node = build.root(component);
    services.layout.set_layout(
        node, layout::ComponentLayout{
                  [context](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints constraints) {
                      return context->measure(engine, constraints);
                  },
                  [context](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                      context->place(engine, bounds);
                  }});
    build.on_resource_cleanup(component, [context, layout = &services.layout, node] {
        context->dispose();
        static_cast<void>(layout->remove_layout(node));
    });
    runtime::connect_layout_style(build.scope(component), SpaceCompactPropsAccess::layout(props), node, services.nodes,
                                  services.dirty);
    auto& scope = build.scope(component);
    connect_prop(scope, SpaceCompactPropsAccess::orientation(props), [context](SpaceOrientation value) {
        validate(value);
        context->metadata.orientation = value;
        context->refresh();
    });
    connect_prop(scope, SpaceCompactPropsAccess::direction(props), [context](FlexDirection value) {
        context->metadata.right_to_left = rtl(value);
        context->refresh(false);
    });
    if (context->explicit_size || !parent) {
        connect_prop(scope, SpaceCompactPropsAccess::size(props), [context](ControlSize value) {
            validate(value);
            context->metadata.size = value;
            context->refresh();
        });
    }
    connect_prop(scope, SpaceCompactPropsAccess::block(props), [context](bool value) {
        context->block = value;
        context->refresh();
    });
    build.mount_slot(component, content);
    context->refresh();
    if (parent) {
        std::weak_ptr<CompactContext> weak = context;
        parent->attach_many(
            component,
            [weak](const CompactMetadata& value) {
                if (const auto child = weak.lock()) {
                    const bool changed_size = !child->explicit_size && child->metadata.size != value.size;
                    child->outer_corners = value.corners;
                    if (!child->explicit_size) {
                        child->metadata.size = value.size;
                    }
                    child->refresh(changed_size);
                }
            },
            [context] { return context->borders(); }, context->surfaces());
        build.on_resource_cleanup(component, [parent, component] { parent->detach(component); });
    }
}

} // namespace ryn::detail

namespace ryn {
void SpaceCompact(SpaceCompactProps props, SpaceCompactContent content) {
    detail::mount_space_compact(props, content);
}
} // namespace ryn
