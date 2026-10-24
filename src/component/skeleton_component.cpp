#include "component/skeleton_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>

namespace ryn::detail {
namespace {
thread_local SkeletonComponentHost* active_skeleton{};

struct SkeletonState final {
    runtime::NodeId node;
    SkeletonModel model;
    SkeletonSnapshot snapshot;
    bool has_content{};
    std::array<theme_runtime::Subscription, 2> subscriptions;
};
} // namespace

SkeletonComponentHost::SkeletonComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

SkeletonComponentHost::~SkeletonComponentHost() {
    services_->detach(*this);
}

void* SkeletonComponentHost::begin_mount() noexcept {
    auto* previous = active_skeleton;
    active_skeleton = this;
    return previous;
}

void SkeletonComponentHost::end_mount(void* previous) noexcept {
    active_skeleton = static_cast<SkeletonComponentHost*>(previous);
}

void SkeletonComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void SkeletonComponentHost::on_dispose() noexcept {
    mounted_.clear();
}

SkeletonSnapshot SkeletonComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<SkeletonState>(id);
    if (!s) {
        throw std::out_of_range("Skeleton component is stale");
    }
    return s->snapshot;
}

void SkeletonComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    const auto* s = services_->components().state<SkeletonState>(id);
    if (!s) {
        return;
    }
    const auto content = s->snapshot.content;
    const auto loading = s->snapshot.loading;
    services_->nodes().require(s->node).out_of_flow = !loading && !s->has_content;
    if (content.valid() && services_->components().set_branch_active(content, !loading)) {
        services_->mark_scene_structure_dirty();
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
    if (content.valid()) {
        // Reconciliation may invoke user blur/popup callbacks and dispose this
        // generation. All state access is complete before entering that boundary.
        services_->set_input_blocked(content, loading);
    }
}

runtime::ComponentId SkeletonComponentHost::create(SkeletonModel model, const LayoutStyle& style,
                                                   const std::optional<SkeletonContent>& content, bool loading) {
    validate_skeleton(model);
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<SkeletonState>();
    auto& s = build.state<SkeletonState>(id);
    s.node = build.root(id);
    s.model = std::move(model);
    s.snapshot.kind = s.model.kind;
    s.snapshot.loading = loading;
    s.has_content = content.has_value();
    services.nodes().require(s.node).clip_content = true;
    runtime::connect_layout_style(build.scope(id), style, s.node, services.nodes(), services.dirty());
    if (content) {
        build.mount_slot(id, Content{[&] {
                             auto& context = runtime::require_component_build_context();
                             const auto child = context.mount_component<int>(0);
                             s.snapshot.content = child;
                             const auto node = context.root(child);
                             services.layout().set_layout(node, layout::BoxLayout{});
                             context.on_resource_cleanup(child, [&services, node, child] {
                                 // Registry-only removal is safe after children have been
                                 // disposed and removes exactly this generational owner.
                                 services.interactions().set_input_blocked(child, false);
                                 services.layout().remove_layout(node);
                             });
                             services.components().set_branch_active(child, !loading);
                             services.set_input_blocked(child, loading);
                             context.mount_slot(child, *content);
                         }});
    }
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.snapshot.surface = services.surfaces().create_content_range(fragment, {});
    build.on_resource_cleanup(id, [&services, node = s.node, surface = s.snapshot.surface] {
        services.surfaces().destroy_content_range(surface);
        services.layout().remove_layout(node);
    });
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                auto& s = *services_->components().state<SkeletonState>(id);
                if (s.snapshot.loading) {
                    return measure_skeleton(
                        s.model, services_->components().theme_scope(id)->snapshot().skeleton().metrics, limits);
                }
                return s.has_content
                           ? limits.constrain(engine.measure_child(services_->components().root(s.snapshot.content),
                                                                   {0, limits.max_width, 0, limits.max_height}))
                           : runtime::Size{};
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect r) {
                const auto& s = *services_->components().state<SkeletonState>(id);
                if (!s.snapshot.loading && s.has_content) {
                    engine.place_child(services_->components().root(s.snapshot.content), r, true, true);
                }
            }});
    mounted_.push_back(id);
    const auto theme = build.theme_scope();
    s.subscriptions[0] = theme->capture([this, id](auto) { refresh(id, false); },
                                        [theme] { static_cast<void>(theme->skeleton_colors()); });
    s.subscriptions[1] = theme->capture([this, id](auto) { refresh(id, true); },
                                        [theme] { static_cast<void>(theme->skeleton_metrics()); });
    return id;
}

template <typename T>
void SkeletonComponentHost::connect(runtime::ComponentId id, const Prop<T>& prop, T SkeletonModel::* member) {
    connect_prop(services_->components().scope(id), prop, [this, id, member](T value) {
        if (auto* s = services_->components().state<SkeletonState>(id)) {
            auto model = s->model;
            model.*member = std::move(value);
            validate_skeleton(model);
            s->model = std::move(model);
            refresh(id, true);
        }
    });
}

void SkeletonComponentHost::mount(const SkeletonProps& props, const std::optional<SkeletonContent>& content) {
    SkeletonModel model;
    model.title = read_prop(props.title_);
    model.paragraph = read_prop(props.paragraph_);
    model.avatar = read_prop(props.avatar_);
    model.round = read_prop(props.round_);
    const auto id = create(std::move(model), props.layout_, content, read_prop(props.loading_));
    connect(id, props.title_, &SkeletonModel::title);
    connect(id, props.paragraph_, &SkeletonModel::paragraph);
    connect(id, props.avatar_, &SkeletonModel::avatar);
    connect(id, props.round_, &SkeletonModel::round);
    connect_prop(services_->components().scope(id), props.loading_, [this, id](bool value) {
        if (auto* s = services_->components().state<SkeletonState>(id)) {
            s->snapshot.loading = value;
            refresh(id, true);
        }
    });
}

void SkeletonComponentHost::mount(const SkeletonAvatarProps& props) {
    SkeletonModel model;
    model.kind = SkeletonKind::Avatar;
    model.size = read_prop(props.size_);
    model.avatar_shape = read_prop(props.shape_);
    const auto id = create(std::move(model), props.layout_, {}, true);
    connect(id, props.size_, &SkeletonModel::size);
    connect(id, props.shape_, &SkeletonModel::avatar_shape);
}

void SkeletonComponentHost::mount(const SkeletonButtonProps& props) {
    SkeletonModel model;
    model.kind = SkeletonKind::Button;
    model.size = read_prop(props.size_);
    model.button_shape = read_prop(props.shape_);
    model.block = read_prop(props.block_);
    const auto id = create(std::move(model), props.layout_, {}, true);
    connect(id, props.size_, &SkeletonModel::size);
    connect(id, props.shape_, &SkeletonModel::button_shape);
    connect(id, props.block_, &SkeletonModel::block);
}

void SkeletonComponentHost::mount(const SkeletonInputProps& props) {
    SkeletonModel model;
    model.kind = SkeletonKind::Input;
    model.size = read_prop(props.size_);
    model.block = read_prop(props.block_);
    const auto id = create(std::move(model), props.layout_, {}, true);
    connect(id, props.size_, &SkeletonModel::size);
    connect(id, props.block_, &SkeletonModel::block);
}

void SkeletonComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (const auto id : mounted_) {
        const auto* s = services_->components().state<SkeletonState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& node = services_->nodes().require(s->node);
        const auto& t = services_->components().theme_scope(id)->snapshot().skeleton();
        const auto shapes =
            s->snapshot.loading ? arrange_skeleton(s->model, t.metrics, node.bounds) : std::vector<SkeletonShape>{};
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
        bool clipped = false;
        for (const auto& shape : shapes) {
            const runtime::Rect translated{shape.bounds.x + node.translation.x, shape.bounds.y + node.translation.y,
                                           shape.bounds.width, shape.bounds.height};
            clipped = clipped || graphics::intersect_effect_bounds(translated, clip) != translated;
        }
        std::vector<graphics::QuadInstance> quads;
        std::vector<graphics::RoundedEffectInstance> effects;
        quads.reserve(shapes.size());
        for (const auto& shape : shapes) {
            graphics::QuadInstance q;
            q.bounds = {shape.bounds.x, shape.bounds.y, shape.bounds.width, shape.bounds.height};
            const auto c = t.colors.background;
            q.color = {c.red(), c.green(), c.blue(), c.alpha()};
            q.corner_radius = shape.radius;
            q.translation = {node.translation.x, node.translation.y};
            q.opacity = clipped ? 0 : node.opacity;
            quads.push_back(q);
            if (clipped) {
                auto fill = graphics::make_shadow_effect({shape.bounds, shape.radius},
                                                         ShadowLayer{ShadowKind::outer, {}, 0, 0, c}, node.translation,
                                                         graphics::EffectClip{1, clip});
                fill.material.opacity = node.opacity;
                effects.push_back(fill);
            }
        }
        static_cast<void>(services_->surfaces().update_content_range(s->snapshot.surface, quads));
        static_cast<void>(services_->surfaces().update_content_effects(s->snapshot.surface, effects));
    }
}
} // namespace ryn::detail

namespace ryn {
void Skeleton(SkeletonProps props) {
    if (!detail::active_skeleton) {
        throw std::logic_error("Skeleton requires window component services");
    }
    detail::active_skeleton->mount(props, {});
}

void Skeleton(SkeletonProps props, SkeletonContent content) {
    if (!detail::active_skeleton) {
        throw std::logic_error("Skeleton requires window component services");
    }
    detail::active_skeleton->mount(props, content);
}

void SkeletonAvatar(SkeletonAvatarProps props) {
    if (!detail::active_skeleton) {
        throw std::logic_error("SkeletonAvatar requires window component services");
    }
    detail::active_skeleton->mount(props);
}

void SkeletonButton(SkeletonButtonProps props) {
    if (!detail::active_skeleton) {
        throw std::logic_error("SkeletonButton requires window component services");
    }
    detail::active_skeleton->mount(props);
}

void SkeletonInput(SkeletonInputProps props) {
    if (!detail::active_skeleton) {
        throw std::logic_error("SkeletonInput requires window component services");
    }
    detail::active_skeleton->mount(props);
}
} // namespace ryn
