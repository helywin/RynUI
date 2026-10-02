#include "component/space_addon.hpp"

#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "theme/semantic_background.hpp"

#include <algorithm>
#include <stdexcept>

namespace ryn::detail {

struct SpaceAddonPropsAccess final {
    static const auto& size(const SpaceAddonProps& props) {
        return props.size_;
    }

    static const auto& variant(const SpaceAddonProps& props) {
        return props.variant_;
    }

    static const auto& status(const SpaceAddonProps& props) {
        return props.status_;
    }

    static const auto& disabled(const SpaceAddonProps& props) {
        return props.disabled_;
    }

    static const auto& layout(const SpaceAddonProps& props) {
        return props.layout_;
    }
};

namespace {
thread_local SpaceAddonHost* active_addon{};

struct AddonState final {
    runtime::NodeId node;
    component::RetainedSurfaceId range;
    SpaceAddonSnapshot visual;
    bool explicit_size{};
    std::weak_ptr<CompactContext> compact;
    std::optional<layout::HorizontalContentLayout> layout;
    std::optional<graphics::EffectClip> clip;
    float radius{};
    Signal<runtime::SemanticForeground> foreground{runtime::SemanticForeground{}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
    theme_runtime::Subscription theme;
};

void validate(ControlSize value) {
    if (value != ControlSize::Small && value != ControlSize::Middle && value != ControlSize::Large) {
        throw std::invalid_argument("SpaceAddon size is invalid");
    }
}

void validate(InputVariant value) {
    if (value != InputVariant::Outlined && value != InputVariant::Filled && value != InputVariant::Borderless &&
        value != InputVariant::Underlined) {
        throw std::invalid_argument("SpaceAddon variant is invalid");
    }
}

void validate(InputStatus value) {
    if (value != InputStatus::Default && value != InputStatus::Warning && value != InputStatus::Error) {
        throw std::invalid_argument("SpaceAddon status is invalid");
    }
}
} // namespace

SpaceAddonHost::SpaceAddonHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

SpaceAddonHost::~SpaceAddonHost() {
    services_->detach(*this);
}

void* SpaceAddonHost::begin_mount() noexcept {
    const auto previous = active_addon;
    active_addon = this;
    return previous;
}

void SpaceAddonHost::end_mount(void* previous) noexcept {
    active_addon = static_cast<SpaceAddonHost*>(previous);
}

void SpaceAddonHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

SpaceAddonSnapshot SpaceAddonHost::snapshot(runtime::ComponentId id) const {
    const auto* state = services_->components().state<AddonState>(id);
    if (!state) {
        throw std::out_of_range("SpaceAddon component is stale");
    }
    return state->visual;
}

void SpaceAddonHost::update(runtime::ComponentId id) {
    auto* state = services_->components().state<AddonState>(id);
    if (!state) {
        return;
    }
    auto& visual = state->visual;
    const auto& theme = services_->components().theme_scope(id)->snapshot();
    const auto& map = theme.map();
    const auto& alias = theme.alias();
    const bool small = visual.size == ControlSize::Small;
    const bool large = visual.size == ControlSize::Large;
    const bool bordered = visual.variant == InputVariant::Outlined || visual.variant == InputVariant::Filled;
    visual.border_width = bordered ? theme.seed().line_width : 0;
    state->radius = small ? map.border_radius_small : large ? map.border_radius_large : map.border_radius;
    visual.shape.radius = std::min(state->radius, .5F * std::min(visual.shape.rect.width, visual.shape.rect.height));
    visual.fill = bordered ? alias.color_background_container_disabled : Color{0, 0, 0, 0};
    visual.border = visual.variant == InputVariant::Filled && !visual.disabled ? Color{0, 0, 0, 0} : alias.color_border;
    visual.foreground = visual.disabled ? alias.color_text_disabled : alias.color_text;
    if (visual.status != InputStatus::Default) {
        const auto color = visual.status == InputStatus::Error ? map.color_error : map.color_warning;
        if (!visual.disabled) {
            visual.foreground = color;
        }
        if (visual.variant == InputVariant::Outlined) {
            visual.border = color;
        }
        if (visual.variant == InputVariant::Filled && !visual.disabled) {
            visual.fill = semantic_status_background(theme, visual.status);
        }
    }
    const float font = small ? map.font_size_small : large ? map.font_size_large : map.font_size;
    const float line = font * (small ? map.line_height_small : large ? map.line_height_large : map.line_height);
    state->foreground.set(
        {visual.foreground.red(), visual.foreground.green(), visual.foreground.blue(), visual.foreground.alpha()});
    state->typography.set({theme.typography().font_family, 400, false, font, line});
    layout::HorizontalContentLayout model;
    model.control_height = small ? map.control_height_small : large ? map.control_height_large : map.control_height;
    model.padding_inline = small ? map.size_xs : map.size_small;
    model.border_width = visual.border_width;
    model.gap = 0;
    if (state->layout != model) {
        state->layout = model;
        services_->layout().set_layout(state->node, model);
        services_->dirty().invalidate(state->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout);
    }
    services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Material);
}

void SpaceAddonHost::mount(const SpaceAddonProps& props, const SpaceAddonContent& content) {
    const auto variant = read_prop(SpaceAddonPropsAccess::variant(props));
    const auto status = read_prop(SpaceAddonPropsAccess::status(props));
    validate(variant);
    validate(status);
    const auto& size_prop = SpaceAddonPropsAccess::size(props);
    if (size_prop) {
        validate(read_prop(*size_prop));
    }
    auto& build = runtime::require_component_build_context();
    const auto compact = nearest_compact(build);
    const auto id = build.mount_component<AddonState>();
    auto& state = build.state<AddonState>(id);
    state.node = build.root(id);
    state.explicit_size = size_prop.has_value();
    state.compact = compact;
    state.visual.size = size_prop ? read_prop(*size_prop) : compact ? compact->metadata.size : ControlSize::Middle;
    state.visual.variant = variant;
    state.visual.status = status;
    state.visual.disabled = read_prop(SpaceAddonPropsAccess::disabled(props));
    if (compact) {
        compact->claim(id);
        build.on_resource_cleanup(id, [compact, id] { compact->detach(id); });
    }
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    state.range = services_->surfaces().create_content_range(fragment, {});
    build.on_resource_cleanup(id, [this, node = state.node, range = state.range] {
        services_->surfaces().destroy_content_range(range);
        static_cast<void>(services_->layout().remove_layout(node));
    });
    runtime::connect_layout_style(build.scope(id), SpaceAddonPropsAccess::layout(props), state.node, services_->nodes(),
                                  services_->dirty());
    update(id);
    build.mount_slot_with_semantic_text_style(id, content, Prop<runtime::SemanticForeground>{state.foreground},
                                              Prop<runtime::SemanticTypography>{state.typography});
    mounted_.push_back(id);
    auto& scope = build.scope(id);
    if (size_prop) {
        connect_prop(scope, *size_prop, [this, id](ControlSize value) {
            validate(value);
            if (auto* state = services_->components().state<AddonState>(id)) {
                state->visual.size = value;
                update(id);
            }
        });
    }
    connect_prop(scope, SpaceAddonPropsAccess::variant(props), [this, id](InputVariant value) {
        validate(value);
        if (auto* state = services_->components().state<AddonState>(id)) {
            state->visual.variant = value;
            update(id);
            if (const auto compact = state->compact.lock()) {
                compact->refresh();
            }
        }
    });
    connect_prop(scope, SpaceAddonPropsAccess::status(props), [this, id](InputStatus value) {
        validate(value);
        if (auto* state = services_->components().state<AddonState>(id)) {
            state->visual.status = value;
            update(id);
        }
    });
    connect_prop(scope, SpaceAddonPropsAccess::disabled(props), [this, id](bool value) {
        if (auto* state = services_->components().state<AddonState>(id)) {
            state->visual.disabled = value;
            update(id);
        }
    });
    const auto theme = build.theme_scope();
    state.theme = theme->capture([this, id](theme_runtime::DirtyPhase) { update(id); },
                                 [theme] {
                                     static_cast<void>(theme->map());
                                     static_cast<void>(theme->alias());
                                     static_cast<void>(theme->line_width());
                                     static_cast<void>(theme->typography_fonts());
                                 });
    if (compact) {
        compact->attach(
            id,
            [this, id](const CompactMetadata& value) {
                if (auto* state = services_->components().state<AddonState>(id)) {
                    state->visual.corners = value.corners;
                    if (!state->explicit_size) {
                        state->visual.size = value.size;
                    }
                    update(id);
                }
            },
            [this, id] {
                const auto* state = services_->components().state<AddonState>(id);
                if (!state) {
                    return CompactBorder{};
                }
                const auto& node = services_->nodes().require(state->node);
                return CompactBorder{.shape = state->visual.shape,
                                     .corners = state->visual.corners,
                                     .color = state->visual.border,
                                     .width = state->visual.border_width,
                                     .priority = state->visual.disabled ? 0 : 2,
                                     .visible = services_->components().branch_active(id),
                                     .translation = node.translation,
                                     .clip = state->clip};
            },
            &services_->surfaces(), true);
    }
}

void SpaceAddonHost::synchronize_auxiliary_geometry(runtime::Size, runtime::Rect clip) {
    for (const auto id : mounted_) {
        auto* state = services_->components().state<AddonState>(id);
        if (!state || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& node = services_->nodes().require(state->node);
        auto& visual = state->visual;
        visual.shape.rect = node.bounds;
        visual.shape.radius = std::min(state->radius, .5F * std::min(node.bounds.width, node.bounds.height));
        const float width = std::min(visual.border_width, .5F * std::min(node.bounds.width, node.bounds.height));
        const graphics::LogicalRoundedRect inner{{node.bounds.x + width, node.bounds.y + width,
                                                  std::max(0.0F, node.bounds.width - 2 * width),
                                                  std::max(0.0F, node.bounds.height - 2 * width)},
                                                 std::max(0.0F, visual.shape.radius - width)};
        const auto clipping = graphics::EffectClip{1, clip};
        state->clip = clipping;
        const auto fill = graphics::make_corner_fill_effects(visual.shape, visual.corners, visual.fill, 1,
                                                             node.translation, clipping);
        const auto border = graphics::make_corner_outline_effects(inner, visual.corners, width, 0, visual.border,
                                                                  width > 0 ? 1.0F : 0.0F, node.translation, clipping);
        std::array<graphics::RoundedEffectInstance, 8> effects;
        std::copy(fill.begin(), fill.end(), effects.begin());
        std::copy(border.begin(), border.end(), effects.begin() + 4);
        services_->surfaces().update_content_effects(state->range, effects);
        if (const auto compact = state->compact.lock()) {
            compact->publish_seams();
        }
    }
}

} // namespace ryn::detail

namespace ryn {
void SpaceAddon(SpaceAddonProps props, SpaceAddonContent content) {
    if (!detail::active_addon) {
        throw std::logic_error("SpaceAddon requires an active component Host");
    }
    detail::active_addon->mount(props, content);
}
} // namespace ryn
