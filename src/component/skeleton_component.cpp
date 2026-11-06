#include "component/skeleton_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/icon.hpp>
#include <algorithm>

namespace ryn::detail {
namespace {
thread_local SkeletonComponentHost* active_skeleton{};

struct SkeletonState final {
    runtime::NodeId node;
    SkeletonModel model;
    SkeletonSnapshot snapshot;
    bool has_content{};
    bool in_view{};
    runtime::SceneFragmentId shimmer_fragment;
    std::vector<graphics::ImageId> shimmer_images;
    std::shared_ptr<const ImagePixels> ramp;
    SkeletonColorToken ramp_colors;
    animation::AnimationTime last_time;
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

bool motion_allowed(WindowComponentServices& services, runtime::ComponentId id) {
    return services.focus().state().window_active && services.components().branch_active(id) &&
           animation::resolve_motion_policy(services.components().theme_scope(id)->snapshot(),
                                            services.motion_preference())
               .enabled();
}

std::shared_ptr<const ImagePixels> shimmer_ramp(const SkeletonColorToken& colors) {
    constexpr std::size_t width = 1024;
    std::array<std::byte, width * 4> bytes;
    for (std::size_t x = 0; x < width; ++x) {
        const float p = static_cast<float>(x) / static_cast<float>(width - 1);
        const float mix = p < .25F || p > .63F ? 0 : p <= .37F ? (p - .25F) / .12F : (.63F - p) / .26F;
        const auto a = colors.background;
        const auto b = colors.gradient_to;
        const float alpha = a.alpha() + (b.alpha() - a.alpha()) * mix;
        const auto channel = [&](float from, float to) {
            return alpha > 0 ? (from * a.alpha() * (1 - mix) + to * b.alpha() * mix) / alpha : 0;
        };
        const std::array values{channel(a.red(), b.red()), channel(a.green(), b.green()), channel(a.blue(), b.blue()),
                                alpha};
        for (std::size_t i = 0; i < 4; ++i) {
            bytes[x * 4 + i] = static_cast<std::byte>(std::lround(std::clamp(values[i], 0.0F, 1.0F) * 255));
        }
    }
    return ImagePixels::copy(width, 1, width * 4, bytes);
}
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

void SkeletonComponentHost::prepare_auxiliary_layout() {
    for (const auto id : mounted_) {
        auto* s = services_->components().state<SkeletonState>(id);
        if (!s) {
            continue;
        }
        const bool present =
            s->snapshot.content.valid() && !services_->components().children(s->snapshot.content).empty();
        if (present == s->has_content) {
            continue;
        }
        s->has_content = present;
        services_->nodes().require(s->node).out_of_flow = !s->snapshot.loading && !present;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                   runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    }
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
    const bool show_content = !loading || s->model.kind == SkeletonKind::Node || s->model.kind == SkeletonKind::Image;
    if (content.valid() && services_->components().set_branch_active(content, show_content)) {
        services_->mark_scene_structure_dirty();
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
    if (content.valid()) {
        // Reconciliation may invoke user blur/popup callbacks and dispose this
        // generation. All state access is complete before entering that boundary.
        services_->set_input_blocked(content, !show_content);
    }
}

runtime::ComponentId SkeletonComponentHost::create(SkeletonModel model, const LayoutStyle& style,
                                                   const std::optional<SkeletonContent>& content, bool loading) {
    validate_skeleton(model);
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<SkeletonState>();
    auto& s = build.state<SkeletonState>(id);
    try {
        s.node = build.root(id);
        s.model = std::move(model);
        s.snapshot.kind = s.model.kind;
        s.snapshot.loading = loading;
        s.shimmer_fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
        build.on_resource_cleanup(id, [this, id, node = s.node] {
            if (const auto* state = services_->components().state<SkeletonState>(id)) {
                for (auto image : state->shimmer_images) {
                    services_->images().destroy(image);
                }
            }
            services_->layout().remove_layout(node);
        });
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
                                 const bool shown = !loading || s.model.kind == SkeletonKind::Node ||
                                                    s.model.kind == SkeletonKind::Image;
                                 services.components().set_branch_active(child, shown);
                                 services.set_input_blocked(child, !shown);
                                 // Reconciliation can synchronously retire this owner and its child.
                                 if (!services.components().contains(id) || !services.components().contains(child)) {
                                     return;
                                 }
                                 context.mount_slot(child, *content);
                                 if (!services.components().contains(id) || !services.components().contains(child)) {
                                     return;
                                 }
                                 s.has_content = !services.components().children(child).empty();
                             }});
        }
        if (!services.components().contains(id)) {
            return id;
        }
        if (s.snapshot.content.valid() && !services.components().contains(s.snapshot.content)) {
            s.snapshot.content = {};
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
                        if ((s.model.kind == SkeletonKind::Image || s.model.kind == SkeletonKind::Node) &&
                            s.has_content) {
                            static_cast<void>(engine.measure_child(services_->components().root(s.snapshot.content),
                                                                   {0, limits.max_width, 0, limits.max_height}));
                        }
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
                    if (s.has_content && (s.model.kind == SkeletonKind::Node || s.model.kind == SkeletonKind::Image)) {
                        const auto child = services_->components().root(s.snapshot.content);
                        const auto size = engine.measure_child(child, {0, r.width, 0, r.height});
                        engine.place_child(child, {r.x + (r.width - size.width) / 2, r.y + (r.height - size.height) / 2,
                                                   size.width, size.height});
                    } else if (!s.snapshot.loading && s.has_content) {
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
    } catch (...) {
        services.retire_failed_component(id);
        throw;
    }
}

template <typename T>
void SkeletonComponentHost::connect(runtime::ComponentId id, const Prop<T>& prop, T SkeletonModel::* member) {
    if (!services_->components().contains(id)) {
        return;
    }
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
    model.active = read_prop(props.active_);
    const auto id = create(std::move(model), props.layout_, content, read_prop(props.loading_));
    if (!services_->components().contains(id)) {
        return;
    }
    connect(id, props.title_, &SkeletonModel::title);
    connect(id, props.paragraph_, &SkeletonModel::paragraph);
    connect(id, props.avatar_, &SkeletonModel::avatar);
    connect(id, props.round_, &SkeletonModel::round);
    connect(id, props.active_, &SkeletonModel::active);
    if (!services_->components().contains(id)) {
        return;
    }
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
    model.active = read_prop(props.active_);
    const auto id = create(std::move(model), props.layout_, {}, true);
    connect(id, props.size_, &SkeletonModel::size);
    connect(id, props.shape_, &SkeletonModel::avatar_shape);
    connect(id, props.active_, &SkeletonModel::active);
}

void SkeletonComponentHost::mount(const SkeletonButtonProps& props) {
    SkeletonModel model;
    model.kind = SkeletonKind::Button;
    model.size = read_prop(props.size_);
    model.button_shape = read_prop(props.shape_);
    model.block = read_prop(props.block_);
    model.active = read_prop(props.active_);
    const auto id = create(std::move(model), props.layout_, {}, true);
    connect(id, props.size_, &SkeletonModel::size);
    connect(id, props.shape_, &SkeletonModel::button_shape);
    connect(id, props.block_, &SkeletonModel::block);
    connect(id, props.active_, &SkeletonModel::active);
}

void SkeletonComponentHost::mount(const SkeletonInputProps& props) {
    SkeletonModel model;
    model.kind = SkeletonKind::Input;
    model.size = read_prop(props.size_);
    model.block = read_prop(props.block_);
    model.active = read_prop(props.active_);
    const auto id = create(std::move(model), props.layout_, {}, true);
    connect(id, props.size_, &SkeletonModel::size);
    connect(id, props.block_, &SkeletonModel::block);
    connect(id, props.active_, &SkeletonModel::active);
}

void SkeletonComponentHost::mount(const SkeletonImageProps& props) {
    SkeletonModel model;
    model.kind = SkeletonKind::Image;
    model.active = read_prop(props.active_);
    const auto theme = runtime::require_component_build_context().theme_scope();
    const auto id =
        create(model, props.layout_, SkeletonContent{[theme] {
                   auto& build = runtime::require_component_build_context();
                   const auto owner = build.capture_context().component_owner.value();
                   build.mount_slot_with_semantic_text_style(
                       owner, Content{[] { Icon(IconProps{}.name(IconName::PictureFilled)); }}, bind([theme] {
                           const auto color = theme->skeleton_colors().colors.image;
                           return runtime::SemanticForeground{color.red(), color.green(), color.blue(), color.alpha()};
                       }),
                       bind([theme] {
                           const auto size = theme->skeleton_metrics().metrics.control_height * 1.5F;
                           return runtime::SemanticTypography{SystemFontFamily::ui_sans, 400, false, size, size};
                       }));
               }},
               true);
    connect(id, props.active_, &SkeletonModel::active);
}

void SkeletonComponentHost::mount(const SkeletonNodeProps& props, const std::optional<SkeletonNodeContent>& content) {
    SkeletonModel model;
    model.kind = SkeletonKind::Node;
    model.active = read_prop(props.active_);
    std::optional<SkeletonContent> body;
    if (content) {
        body = SkeletonContent{[slot = *content] { SlotContentAccess::function(slot)(); }};
    }
    const auto id = create(model, props.layout_, body, true);
    connect(id, props.active_, &SkeletonModel::active);
}

void SkeletonComponentHost::on_window_active(bool value) {
    window_active_ = value;
    synchronize_auxiliary_motion();
}

void SkeletonComponentHost::synchronize_auxiliary_motion() {
    for (auto id : mounted_) {
        if (auto* s = services_->components().state<SkeletonState>(id)) {
            const bool was_animating = s->snapshot.animating;
            s->snapshot.animating = window_active_ && s->model.active && s->snapshot.loading && s->in_view &&
                                    motion_allowed(*services_, id) && services_->nodes().content_opacity(s->node) > 0;
            if (!s->snapshot.animating) {
                s->snapshot.phase = 0;
            }
            if (!s->snapshot.animating || !was_animating) {
                s->last_time = std::max(s->last_time, services_->animation_time());
            }
        }
    }
}

std::size_t SkeletonComponentHost::tick_auxiliary(animation::AnimationTime now) {
    synchronize_auxiliary_motion();
    std::size_t changed{};
    for (auto id : mounted_) {
        auto* s = services_->components().state<SkeletonState>(id);
        if (!s) {
            continue;
        }
        now = std::max(now, s->last_time);
        const auto elapsed = (now - s->last_time).count_microseconds();
        s->last_time = now;
        if (s->snapshot.animating && elapsed > 0) {
            s->snapshot.phase = std::fmod(s->snapshot.phase + static_cast<float>(elapsed % 1400000) / 1400000, 1.0F);
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
            ++changed;
        }
    }
    return changed;
}

std::optional<animation::AnimationTime> SkeletonComponentHost::next_auxiliary_deadline() const {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<SkeletonState>(id);
        if (s && s->snapshot.animating && s->model.active && s->snapshot.loading && window_active_ &&
            motion_allowed(*services_, id) && services_->nodes().content_opacity(s->node) > 0) {
            return std::max(s->last_time, services_->animation_time()) +
                   animation::AnimationDuration::microseconds(16000);
        }
    }
    return {};
}

void SkeletonComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (const auto id : mounted_) {
        auto* s = services_->components().state<SkeletonState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& node = services_->nodes().require(s->node);
        const auto opacity = node.opacity * services_->nodes().content_opacity(s->node);
        const auto& t = services_->components().theme_scope(id)->snapshot().skeleton();
        const auto shapes =
            s->snapshot.loading ? arrange_skeleton(s->model, t.metrics, node.bounds) : std::vector<SkeletonShape>{};
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
        const runtime::Rect translated_bounds{node.bounds.x + node.translation.x, node.bounds.y + node.translation.y,
                                              node.bounds.width, node.bounds.height};
        const auto visible = graphics::intersect_effect_bounds(translated_bounds, clip);
        s->in_view = visible.width > 0 && visible.height > 0;
        const bool was_animating = s->snapshot.animating;
        s->snapshot.animating = window_active_ && s->model.active && s->snapshot.loading && s->in_view &&
                                motion_allowed(*services_, id) && services_->nodes().content_opacity(s->node) > 0;
        if (!s->snapshot.animating) {
            s->snapshot.phase = 0;
        }
        if (!s->snapshot.animating || !was_animating) {
            s->last_time = std::max(s->last_time, services_->animation_time());
        }
        std::vector<graphics::ImageInstance> shimmer;
        if (s->snapshot.animating) {
            if (!s->ramp || s->ramp_colors != t.colors) {
                s->ramp = shimmer_ramp(t.colors);
                s->ramp_colors = t.colors;
            }
            for (const auto& shape : shapes) {
                if (shape.bounds.width <= 0 || shape.bounds.height <= 0) {
                    continue;
                }
                graphics::ImageInstance image;
                image.pixels = s->ramp;
                image.bounds = {shape.bounds.x + node.translation.x, shape.bounds.y + node.translation.y,
                                shape.bounds.width, shape.bounds.height};
                image.clip = clip;
                image.opacity = opacity * services_->nodes().paint_opacity(s->node);
                // Upstream's 400% background moves from right to left; the ramp is immutable.
                const float eased = s->snapshot.phase * s->snapshot.phase * (3 - 2 * s->snapshot.phase);
                image.source_uv = {.75F * (1 - eased), 0, .25F, 1};
                auto pieces = graphics::decompose_rounded_image(image, image.bounds, shape.radius);
                shimmer.insert(shimmer.end(), pieces.begin(), pieces.end());
            }
        }
        while (s->shimmer_images.size() > shimmer.size()) {
            services_->images().destroy(s->shimmer_images.back());
            s->shimmer_images.pop_back();
        }
        std::vector<graphics::SceneDrawCommand> commands;
        s->shimmer_images.reserve(shimmer.size());
        for (std::size_t i = 0; i < shimmer.size(); ++i) {
            if (i == s->shimmer_images.size()) {
                s->shimmer_images.push_back(services_->images().create(shimmer[i]));
            } else {
                services_->images().update(s->shimmer_images[i], shimmer[i]);
            }
            const auto image = s->shimmer_images[i];
            commands.push_back({graphics::SceneDrawKind::image, image.index, 1, image.generation});
        }
        services_->scene_composer().set_fragment(s->shimmer_fragment, commands, {}, clip);
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
            q.opacity = clipped || s->snapshot.animating ? 0 : opacity;
            quads.push_back(q);
            if (clipped && !s->snapshot.animating) {
                auto fill = graphics::make_shadow_effect({shape.bounds, shape.radius},
                                                         ShadowLayer{ShadowKind::outer, {}, 0, 0, c}, node.translation,
                                                         graphics::EffectClip{1, clip});
                fill.material.opacity = opacity;
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

void SkeletonImage(SkeletonImageProps props) {
    if (!detail::active_skeleton) {
        throw std::logic_error("SkeletonImage requires window component services");
    }
    detail::active_skeleton->mount(props);
}

void SkeletonNode(SkeletonNodeProps props) {
    if (!detail::active_skeleton) {
        throw std::logic_error("SkeletonNode requires window component services");
    }
    detail::active_skeleton->mount(props, {});
}

void SkeletonNode(SkeletonNodeProps props, SkeletonNodeContent content) {
    if (!detail::active_skeleton) {
        throw std::logic_error("SkeletonNode requires window component services");
    }
    detail::active_skeleton->mount(props, content);
}
} // namespace ryn
