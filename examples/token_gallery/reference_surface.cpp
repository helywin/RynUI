#include "reference_surface.hpp"

#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <ryn/text.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace rynui::example::detail {

struct ReferenceSurfacePropsAccess final {
    [[nodiscard]] static const ryn::Prop<GallerySupportStatus>& status(
        const ReferenceSurfaceProps& props) noexcept {
        return props.status_;
    }

    [[nodiscard]] static const ryn::Prop<std::optional<ryn::Color>>& swatch(
        const ReferenceSurfaceProps& props) noexcept {
        return props.swatch_;
    }

    [[nodiscard]] static const ryn::Prop<bool>& elevated(
        const ReferenceSurfaceProps& props) noexcept {
        return props.elevated_;
    }

    [[nodiscard]] static const ryn::Prop<bool>& visible(
        const ReferenceSurfaceProps& props) noexcept {
        return props.visible_;
    }

    [[nodiscard]] static const ryn::LayoutStyle& layout(
        const ReferenceSurfaceProps& props) noexcept {
        return props.layout_;
    }
};

struct ReferenceSurfaceComponentState final {
    ryn::runtime::ComponentId component;
    ryn::runtime::NodeId node;
    ryn::runtime::SceneFragmentId fragment;
    ryn::component::ButtonSceneId scene;
    GallerySupportStatus status{GallerySupportStatus::planned};
    std::optional<ryn::Color> swatch;
    bool elevated{};
    bool visible{true};
    ReferenceSurfaceVisualData visuals;
    ryn::component::ButtonEffectData effects;
    ryn::Signal<ryn::String> status_label{ryn::String{u8"规划中"}};
    ryn::theme_runtime::Subscription theme_subscription;
};

} // namespace rynui::example::detail

namespace rynui::example {
namespace {

thread_local ReferenceSurfaceHost* active_reference_surface_host = nullptr;

class ActiveReferenceSurfaceHost final {
public:
    explicit ActiveReferenceSurfaceHost(ReferenceSurfaceHost& host) noexcept
        : previous_(active_reference_surface_host) {
        active_reference_surface_host = &host;
    }

    ~ActiveReferenceSurfaceHost() {
        active_reference_surface_host = previous_;
    }

private:
    ReferenceSurfaceHost* previous_;
};

void validate_status(GallerySupportStatus status) {
    switch (status) {
    case GallerySupportStatus::implemented:
    case GallerySupportStatus::partial:
    case GallerySupportStatus::planned:
    case GallerySupportStatus::web_only:
    case GallerySupportStatus::deprecated:
    case GallerySupportStatus::out_of_scope:
        return;
    }
    throw std::invalid_argument("ReferenceSurface support status is invalid");
}

ryn::String status_label(GallerySupportStatus status) {
    switch (status) {
    case GallerySupportStatus::implemented:
        return ryn::String{u8"已实现"};
    case GallerySupportStatus::partial:
        return ryn::String{u8"部分支持"};
    case GallerySupportStatus::planned:
        return ryn::String{u8"规划中"};
    case GallerySupportStatus::web_only:
        return ryn::String{u8"仅 Web"};
    case GallerySupportStatus::deprecated:
        return ryn::String{u8"已弃用"};
    case GallerySupportStatus::out_of_scope:
        return ryn::String{u8"不在范围"};
    }
    throw std::invalid_argument("ReferenceSurface support status is invalid");
}

ryn::Color status_color(
    GallerySupportStatus status,
    const ryn::ThemeSnapshot& theme) {
    switch (status) {
    case GallerySupportStatus::implemented:
        return theme.map().color_success;
    case GallerySupportStatus::partial:
        return theme.map().color_warning;
    case GallerySupportStatus::planned:
        return theme.map().color_primary_border;
    case GallerySupportStatus::web_only:
        return theme.alias().color_text_secondary;
    case GallerySupportStatus::deprecated:
        return theme.map().color_error;
    case GallerySupportStatus::out_of_scope:
        return theme.alias().color_text_disabled;
    }
    throw std::invalid_argument("ReferenceSurface support status is invalid");
}

std::array<float, 4> channels(ryn::Color color) noexcept {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

std::array<float, 4> clip_rect(
    ryn::runtime::Rect bounds,
    ryn::runtime::Size viewport) {
    if (!std::isfinite(viewport.width) || !std::isfinite(viewport.height)
            || viewport.width <= 0.0F || viewport.height <= 0.0F) {
        throw std::invalid_argument(
            "ReferenceSurface viewport must be finite and positive");
    }
    return {
        -1.0F + 2.0F * bounds.x / viewport.width,
        1.0F - 2.0F * bounds.y / viewport.height,
        2.0F * bounds.width / viewport.width,
        -2.0F * bounds.height / viewport.height,
    };
}

float normalized_radius(ryn::runtime::Rect bounds, float radius) noexcept {
    const float extent = std::min(bounds.width, bounds.height);
    return extent > 0.0F
        ? std::clamp(radius / extent, 0.0F, 0.5F)
        : 0.0F;
}

ryn::graphics::QuadInstance make_quad(
    ryn::runtime::Rect bounds,
    ryn::runtime::Size viewport,
    ryn::Color color,
    float opacity,
    float radius,
    ryn::runtime::Point translation) {
    return {
        clip_rect(bounds, viewport),
        channels(color),
        opacity,
        normalized_radius(bounds, radius),
        {
            2.0F * translation.x / viewport.width,
            -2.0F * translation.y / viewport.height,
        },
    };
}

float logical_radius(ryn::runtime::Rect bounds, float radius) noexcept {
    return std::clamp(
        radius,
        0.0F,
        0.5F * std::min(bounds.width, bounds.height));
}

void refresh_material(
    ReferenceSurfaceHost& host,
    detail::ReferenceSurfaceComponentState& state) {
    const auto& theme = host.application().components()
        .theme_scope(state.component)->snapshot();
    state.visuals[static_cast<std::size_t>(
        ReferenceSurfaceVisualLayer::border)].color =
            channels(theme.alias().color_border_secondary);
    state.visuals[static_cast<std::size_t>(
        ReferenceSurfaceVisualLayer::border)].opacity = state.visible ? 1.0F : 0.0F;
    state.visuals[static_cast<std::size_t>(
        ReferenceSurfaceVisualLayer::background)].color =
            channels(theme.alias().color_background_container);
    state.visuals[static_cast<std::size_t>(
        ReferenceSurfaceVisualLayer::background)].opacity = state.visible ? 1.0F : 0.0F;
    state.visuals[static_cast<std::size_t>(
        ReferenceSurfaceVisualLayer::swatch)].color =
            channels(state.swatch.value_or(theme.alias().color_background_container));
    state.visuals[static_cast<std::size_t>(
        ReferenceSurfaceVisualLayer::swatch)].opacity =
            state.visible && state.swatch.has_value() ? 1.0F : 0.0F;
    state.visuals[static_cast<std::size_t>(
        ReferenceSurfaceVisualLayer::status_badge)].color =
            channels(status_color(state.status, theme));
    state.visuals[static_cast<std::size_t>(
        ReferenceSurfaceVisualLayer::status_badge)].opacity = state.visible ? 1.0F : 0.0F;

    state.effects.shadows = state.elevated && state.visible
        ? theme.alias().box_shadow_tertiary : ryn::ShadowList{};
    state.effects.shadow_opacity = state.elevated && state.visible ? 1.0F : 0.0F;
    state.effects.focus_enabled = false;
    state.effects.focus_opacity = 0.0F;
    if (state.scene.valid()) {
        static_cast<void>(host.application().button_scene().update_surface(
            state.scene, state.visuals));
        static_cast<void>(host.application().button_scene().update_effects(
            state.scene, state.effects));
        host.application().dirty().invalidate(
            state.node, ryn::runtime::DirtyFlags::Material);
    }
}

void apply_status(
    ReferenceSurfaceHost& host,
    ryn::runtime::ComponentId component,
    GallerySupportStatus status) {
    validate_status(status);
    auto* state = host.find_state(component);
    if (state == nullptr || state->status == status) {
        return;
    }
    state->status = status;
    static_cast<void>(state->status_label.set(status_label(status)));
    refresh_material(host, *state);
}

void apply_swatch(
    ReferenceSurfaceHost& host,
    ryn::runtime::ComponentId component,
    std::optional<ryn::Color> swatch) {
    auto* state = host.find_state(component);
    if (state == nullptr || state->swatch == swatch) {
        return;
    }
    state->swatch = swatch;
    refresh_material(host, *state);
}

void apply_elevated(
    ReferenceSurfaceHost& host,
    ryn::runtime::ComponentId component,
    bool elevated) {
    auto* state = host.find_state(component);
    if (state == nullptr || state->elevated == elevated) {
        return;
    }
    state->elevated = elevated;
    refresh_material(host, *state);
}

void apply_visible(
    ReferenceSurfaceHost& host,
    ryn::runtime::ComponentId component,
    bool visible) {
    auto* state = host.find_state(component);
    if (state == nullptr || state->visible == visible) {
        return;
    }
    state->visible = visible;
    refresh_material(host, *state);
    auto& components = host.application().components();
    auto& text_host = host.application().text();
    auto& scenes = text_host.scene_service();
    for (const auto& mounted : text_host.mounted_texts()) {
        auto current = std::optional<ryn::runtime::ComponentId>{mounted.component};
        bool descendant = false;
        while (current.has_value()) {
            if (*current == component) {
                descendant = true;
                break;
            }
            current = components.parent(*current);
        }
        if (!descendant) {
            continue;
        }
        if (scenes.set_opacity(mounted.scene, visible ? 1.0F : 0.0F)) {
            host.application().dirty().invalidate(
                scenes.node(mounted.scene), ryn::runtime::DirtyFlags::Material);
        }
    }
}

} // namespace

ReferenceSurfaceHost::ReferenceSurfaceHost(
    ryn::detail::ButtonComponentHost& application)
    : application_(&application) {
    application_->attach_auxiliary(*this);
}

ReferenceSurfaceHost::~ReferenceSurfaceHost() {
    while (!mounted_.empty()) {
        const auto component = mounted_.back().component;
        try {
            if (!application_->destroy(component)) {
                mounted_.pop_back();
            } else {
                std::erase_if(mounted_, [component](const auto& mounted) {
                    return mounted.component == component;
                });
            }
        } catch (...) {
            mounted_.pop_back();
        }
    }
    application_->detach_auxiliary(*this);
}

void ReferenceSurfaceHost::mount(const ryn::Content& content) {
    ActiveReferenceSurfaceHost guard(*this);
    application_->mount(content);
}

bool ReferenceSurfaceHost::destroy(ryn::runtime::ComponentId component) {
    if (!application_->destroy(component)) {
        return false;
    }
    std::erase_if(mounted_, [this](const auto& mounted) {
        return !application_->components().contains(mounted.component);
    });
    return true;
}

bool ReferenceSurfaceHost::layout_and_synchronize(
    ryn::runtime::Size viewport,
    ryn::runtime::Rect clip,
    ryn::runtime::Point origin,
    float gap) {
    return application_->layout_and_synchronize(
        viewport, clip, origin, gap);
}

std::span<const MountedReferenceSurface>
ReferenceSurfaceHost::mounted_surfaces() const noexcept {
    return mounted_;
}

ReferenceSurfaceSnapshot ReferenceSurfaceHost::snapshot(
    ryn::runtime::ComponentId component) const {
    const auto* state = find_state(component);
    if (state == nullptr) {
        throw std::out_of_range(
            "ReferenceSurface component is stale or invalid");
    }
    return {
        state->status,
        state->swatch,
        state->elevated,
        state->visible,
        state->scene,
        application_->button_scene().visual_range(state->scene),
    };
}

ryn::detail::ButtonComponentHost& ReferenceSurfaceHost::application() noexcept {
    return *application_;
}

void ReferenceSurfaceHost::synchronize_auxiliary_geometry(
    ryn::runtime::Size viewport,
    ryn::runtime::Rect clip) {
    for (const auto& mounted : mounted_) {
        auto* state = find_state(mounted.component);
        if (state == nullptr) {
            continue;
        }
        const auto& theme = application_->components()
            .theme_scope(state->component)->snapshot();
        const auto& node = application_->nodes().require(state->node);
        const float border_width = theme.seed().line_width;
        const float radius = theme.map().border_radius;
        const ryn::runtime::Rect background{
            node.bounds.x + border_width,
            node.bounds.y + border_width,
            std::max(0.0F, node.bounds.width - 2.0F * border_width),
            std::max(0.0F, node.bounds.height - 2.0F * border_width),
        };
        const ryn::runtime::Rect swatch{
            node.bounds.x + 12.0F,
            std::max(node.bounds.y + 12.0F, node.bounds.y + node.bounds.height - 28.0F),
            state->swatch.has_value() ? 16.0F : 0.0F,
            state->swatch.has_value() ? 16.0F : 0.0F,
        };
        const ryn::runtime::Rect badge{
            std::max(node.bounds.x + 12.0F, node.bounds.x + node.bounds.width - 20.0F),
            node.bounds.y + 12.0F,
            8.0F,
            8.0F,
        };
        state->visuals[static_cast<std::size_t>(
            ReferenceSurfaceVisualLayer::border)] = make_quad(
                node.bounds,
                viewport,
                theme.alias().color_border_secondary,
                1.0F,
                radius,
                node.translation);
        state->visuals[static_cast<std::size_t>(
            ReferenceSurfaceVisualLayer::background)] = make_quad(
                background,
                viewport,
                theme.alias().color_background_container,
                1.0F,
                std::max(0.0F, radius - border_width),
                node.translation);
        state->visuals[static_cast<std::size_t>(
            ReferenceSurfaceVisualLayer::swatch)] = make_quad(
                swatch,
                viewport,
                state->swatch.value_or(theme.alias().color_background_container),
                state->swatch.has_value() ? 1.0F : 0.0F,
                4.0F,
                node.translation);
        state->visuals[static_cast<std::size_t>(
            ReferenceSurfaceVisualLayer::status_badge)] = make_quad(
                badge,
                viewport,
                status_color(state->status, theme),
                1.0F,
                4.0F,
                node.translation);
        state->effects.shape = {
            node.bounds,
            logical_radius(node.bounds, radius),
        };
        state->effects.translation = node.translation;
        state->effects.ancestor_clip = ryn::graphics::EffectClip{
            (static_cast<std::uint64_t>(state->component.index) << 32U)
                | state->component.generation,
            clip,
        };
        static_cast<void>(application_->button_scene().update_surface(
            state->scene, state->visuals));
        static_cast<void>(application_->button_scene().update_effects(
            state->scene, state->effects));
    }
}

detail::ReferenceSurfaceComponentState* ReferenceSurfaceHost::find_state(
    ryn::runtime::ComponentId component) noexcept {
    return application_->components()
        .state<detail::ReferenceSurfaceComponentState>(component);
}

const detail::ReferenceSurfaceComponentState* ReferenceSurfaceHost::find_state(
    ryn::runtime::ComponentId component) const noexcept {
    return application_->components()
        .state<detail::ReferenceSurfaceComponentState>(component);
}

void ReferenceSurfaceHost::record_mounted(
    MountedReferenceSurface mounted) {
    mounted_.push_back(std::move(mounted));
}

void ReferenceSurface(
    ReferenceSurfaceProps props,
    ReferenceSurfaceContent content) {
    if (active_reference_surface_host == nullptr) {
        throw std::logic_error(
            "ReferenceSurface can only be declared inside an active Gallery host");
    }
    auto& host = *active_reference_surface_host;
    auto& build = ryn::runtime::require_component_build_context();
    const auto initial_status = ryn::detail::read_prop(
        detail::ReferenceSurfacePropsAccess::status(props));
    validate_status(initial_status);
    const auto initial_swatch = ryn::detail::read_prop(
        detail::ReferenceSurfacePropsAccess::swatch(props));
    const bool initial_elevated = ryn::detail::read_prop(
        detail::ReferenceSurfacePropsAccess::elevated(props));
    const bool initial_visible = ryn::detail::read_prop(
        detail::ReferenceSurfacePropsAccess::visible(props));

    const auto component = build.mount_component<
        detail::ReferenceSurfaceComponentState>();
    auto& state = build.state<detail::ReferenceSurfaceComponentState>(component);
    state.component = component;
    state.node = build.root(component);
    state.status = initial_status;
    state.swatch = initial_swatch;
    state.elevated = initial_elevated;
    state.visible = initial_visible;
    static_cast<void>(state.status_label.set(status_label(initial_status)));

    ryn::layout::FlexLayout model;
    model.direction = ryn::layout::FlexDirection::vertical;
    model.main_gap = 4.0F;
    model.cross_gap = 4.0F;
    model.padding = {12.0F, 12.0F, 12.0F, 12.0F};
    model.item_policy = ryn::layout::FlexItemPolicy::sequential;
    host.application().layout().set_layout(state.node, model);
    build.on_resource_cleanup(component, [
        layout = &host.application().layout(),
        node = state.node] {
        static_cast<void>(layout->remove_layout(node));
    });
    ryn::runtime::connect_layout_style(
        build.scope(component),
        detail::ReferenceSurfacePropsAccess::layout(props),
        state.node,
        host.application().nodes(),
        host.application().dirty());

    state.fragment = build.register_scene_fragment(
        component,
        ryn::runtime::SceneFragmentPlacement::before_children);
    refresh_material(host, state);
    state.scene = host.application().button_scene().create_surface(
        component,
        state.node,
        state.fragment,
        state.visuals,
        state.effects);
    build.on_resource_cleanup(component, [
        scenes = &host.application().button_scene(),
        scene = state.scene] {
        static_cast<void>(scenes->destroy(scene));
    });

    const auto theme = build.theme_scope();
    state.theme_subscription = theme->capture(
        [&host, component](ryn::theme_runtime::DirtyPhase) {
            if (auto* current = host.find_state(component)) {
                refresh_material(host, *current);
            }
        },
        [theme] {
            static_cast<void>(theme->alias());
            static_cast<void>(theme->map());
        });

    auto& scope = build.scope(component);
    static_cast<void>(ryn::detail::connect_prop(
        scope,
        detail::ReferenceSurfacePropsAccess::status(props),
        [&host, component](GallerySupportStatus value) {
            apply_status(host, component, value);
        }));
    static_cast<void>(ryn::detail::connect_prop(
        scope,
        detail::ReferenceSurfacePropsAccess::swatch(props),
        [&host, component](std::optional<ryn::Color> value) {
            apply_swatch(host, component, value);
        }));
    static_cast<void>(ryn::detail::connect_prop(
        scope,
        detail::ReferenceSurfacePropsAccess::elevated(props),
        [&host, component](bool value) {
            apply_elevated(host, component, value);
        }));
    static_cast<void>(ryn::detail::connect_prop(
        scope,
        detail::ReferenceSurfacePropsAccess::visible(props),
        [&host, component](bool value) {
            apply_visible(host, component, value);
        }));

    const ReferenceSurfaceContent children{[&state, &content] {
        ryn::Text(
            ryn::TextProps{}
                .content(state.status_label)
                .tone(ryn::TextTone::Secondary));
        ryn::detail::SlotContentAccess::function(content)();
    }};
    build.mount_slot(component, children);
    if (!state.visible) {
        state.visible = true;
        apply_visible(host, component, false);
    }
    host.application().dirty().invalidate_subtree(
        state.node,
        ryn::runtime::DirtyFlags::Measure
            | ryn::runtime::DirtyFlags::Layout
            | ryn::runtime::DirtyFlags::Geometry
            | ryn::runtime::DirtyFlags::Material);
    host.record_mounted({component, state.node, state.scene, state.fragment});
}

} // namespace rynui::example
