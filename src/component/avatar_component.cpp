#include "component/avatar_component.hpp"
#include "component/popover_component.hpp"
#include "component/image_component.hpp"
#include "component/responsive_layout.hpp"
#include "component/form_control_binding.hpp"
#include "component/retained_slot.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace ryn::detail {
namespace {
thread_local AvatarComponentHost* active_avatar{};
constexpr float infinity = std::numeric_limits<float>::infinity();

struct AvatarState final {
    runtime::NodeId node;
    AvatarSnapshot view;
    component::RetainedSurfaceId border;
    Signal<ImageSource> source{ImageSource{}};
    Signal<ImageSource> fallback{ImageSource{}};
    ImageSource configured_fallback;
    Signal<ThemeConfig> image_theme{ThemeConfig{}};
    Signal<ImageShape> image_shape{ImageShape::Circle};
    Signal<bool> image_visible{true};
    Signal<LogicalLength> image_side{dp(32)};
    std::optional<AvatarContent> placeholder;
    bool image_ready{};
    bool image_failed{};
    bool error_suppressed{};
    std::uint64_t source_revision{};
    std::uint64_t refresh_revision{};
    std::function<bool(const ImageError&)> on_error;
    std::function<void()> on_load;
    Breakpoints breakpoints;
    AvatarSizeValue size{AvatarSize::Medium};
    AvatarShape shape{AvatarShape::Circle};
    float gap{4};
    bool visible{true};
    bool has_icon{};
    bool use_icon{};
    bool rich{};
    bool grouped{};
    Signal<String> text{String{}};
    Signal<runtime::SemanticForeground> color{{1, 1, 1, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> icon_font{runtime::SemanticTypography{}};
    TextSceneId measurement;
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

struct GroupItem final {
    AvatarGroupItemSnapshot view;
    runtime::Rect popup_frame;
    bool visible{true};
    std::shared_ptr<Scope> bindings{std::make_shared<Scope>()};
    Signal<String> text{String{}};
    Signal<ImageSource> source{ImageSource{}};
    Signal<AvatarSizeValue> size{AvatarSizeValue{AvatarSize::Medium}};
    Signal<AvatarShape> shape{AvatarShape::Circle};
    bool own_size{};
    bool own_shape{};
    Signal<bool> inline_visible{false};
    Signal<bool> overflow_visible{false};
    Signal<bool> has_icon{false};
    Signal<IconName> icon{IconName::UserOutlined};
};

struct GroupState final {
    runtime::NodeId node;
    AvatarGroupSnapshot view;
    std::vector<GroupItem> items;
    Signal<AvatarSizeValue> size{AvatarSizeValue{AvatarSize::Medium}};
    Signal<AvatarShape> shape{AvatarShape::Circle};
    Signal<String> overflow_text{String{}};
    Signal<bool> overflow_visible{false};
    Signal<bool> disabled{true};
    Signal<PopoverTriggerMode> trigger{PopoverTriggerMode::Hover};
    Signal<PopoverPlacement> placement{PopoverPlacement::Top};
    AvatarGroupMax max;
    bool visible{true};
    bool rtl{};
    std::uint64_t revision{};
    std::uint64_t request_revision{};
    bool reconciling{};
    std::optional<std::vector<AvatarGroupItem>> pending_items;
    std::shared_ptr<const AvatarGroupPopoverRenderer> renderer;
    runtime::ComponentId custom;
    runtime::ComponentId popup_row;
    float popup_height{};
    float popup_scroll{};
    std::array<theme_runtime::Subscription, 2> subscriptions;
};

void validate_size(const AvatarSizeValue& size) {
    if (const auto* value = std::get_if<float>(&size)) {
        if (!std::isfinite(*value) || *value < 1 || *value > 1e6F) {
            throw std::invalid_argument("Avatar numeric size must be finite and in [1, 1000000]");
        }
    } else if (const auto* responsive = std::get_if<Responsive<float>>(&size)) {
        validate_size(AvatarSizeValue{responsive->fallback()});
        for (const auto& value : responsive->overrides()) {
            if (value) {
                validate_size(AvatarSizeValue{*value});
            }
        }
    } else if (const auto preset = std::get<AvatarSize>(size);
               preset < AvatarSize::Small || preset > AvatarSize::Large) {
        throw std::invalid_argument("Avatar size is invalid");
    }
}

void validate_shape(AvatarShape shape) {
    if (shape != AvatarShape::Circle && shape != AvatarShape::Square) {
        throw std::invalid_argument("Avatar shape is invalid");
    }
}

void validate_gap(float value) {
    if (!std::isfinite(value) || value < 0 || value > 1e6F) {
        throw std::invalid_argument("Avatar gap must be finite and in [0, 1000000]");
    }
}

void validate_max(AvatarGroupMax value) {
    if ((value.count && *value.count > 1024) || value.trigger < PopoverTriggerMode::Manual ||
        value.trigger > PopoverTriggerMode::Click || value.placement < PopoverPlacement::Top ||
        value.placement > PopoverPlacement::RightBottom) {
        throw std::invalid_argument("AvatarGroup max is invalid or count exceeds 1024");
    }
}

struct Metrics final {
    float size;
    float text;
    float icon;
    float radius;
};

Metrics metrics(const AvatarMetricToken& m, const AvatarSizeValue& value, float width = 0,
                const Breakpoints& points = {}) {
    if (const auto* responsive = std::get_if<Responsive<float>>(&value)) {
        return metrics(m, AvatarSizeValue{responsive->resolve(width, points)});
    }
    if (const auto* size = std::get_if<float>(&value)) {
        return {*size, 18, *size / 2, m.border_radius};
    }
    switch (std::get<AvatarSize>(value)) {
    case AvatarSize::Small:
        return {m.container_size_small, m.text_font_size_small, m.icon_font_size_small, m.border_radius_small};
    case AvatarSize::Large:
        return {m.container_size_large, m.text_font_size_large, m.icon_font_size_large, m.border_radius_large};
    default:
        return {m.container_size, m.text_font_size, m.icon_font_size, m.border_radius};
    }
}

runtime::ComponentId region(WindowComponentServices& s) {
    auto& b = runtime::require_component_build_context();
    const auto id = b.mount_component<int>(0);
    const auto node = b.root(id);
    s.nodes().require(node).clip_content = true;
    s.nodes().require(node).clip_decorations = false;
    s.layout().set_layout(
        node, layout::ComponentLayout{[&s](layout::LayoutEngine& e, runtime::NodeId node, layout::Constraints c) {
                                          runtime::Size natural;
                                          for (auto child : s.nodes().require(node).children) {
                                              const auto size = e.measure_child(child, {0, infinity, 0, infinity});
                                              natural.width = std::max(natural.width, size.width);
                                              natural.height = std::max(natural.height, size.height);
                                          }
                                          return c.constrain(natural);
                                      },
                                      [&s](layout::LayoutEngine& e, runtime::NodeId node, runtime::Rect r) {
                                          for (auto child : s.nodes().require(node).children) {
                                              const auto size = s.nodes().require(child).layout_size;
                                              e.place_child(child, {r.x + (r.width - size.width) / 2,
                                                                    r.y + (r.height - size.height) / 2, size.width,
                                                                    size.height});
                                          }
                                      }});
    b.on_resource_cleanup(id, [&s, id, node] {
        s.interactions().set_input_blocked(id, false);
        s.layout().remove_layout(node);
    });
    return id;
}
} // namespace

AvatarComponentHost::AvatarComponentHost(WindowComponentServices& s) : services_(&s) {
    s.attach(*this);
}

AvatarComponentHost::~AvatarComponentHost() {
    services_->detach(*this);
}

void* AvatarComponentHost::begin_mount() noexcept {
    auto* old = active_avatar;
    active_avatar = this;
    return old;
}

void AvatarComponentHost::end_mount(void* old) noexcept {
    active_avatar = static_cast<AvatarComponentHost*>(old);
}

void AvatarComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
    std::erase_if(groups_, [this](auto id) { return !services_->components().contains(id); });
}

void AvatarComponentHost::on_dispose() noexcept {
    mounted_.clear();
    groups_.clear();
}

AvatarSnapshot AvatarComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<AvatarState>(id);
    if (!s) {
        throw std::out_of_range("Avatar component is stale");
    }
    return s->view;
}

AvatarGroupSnapshot AvatarComponentHost::group_snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<GroupState>(id);
    if (!s) {
        throw std::out_of_range("AvatarGroup component is stale");
    }
    auto result = s->view;
    for (const auto& item : s->items) {
        result.items.push_back(item.view);
    }
    return result;
}

void AvatarComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    const auto storage = services_->components().retain_state<AvatarState>(id);
    auto* s = storage.get();
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto revision = ++s->refresh_revision;
    const auto current = [&] {
        return services_->components().contains(id) && s->refresh_revision == revision;
    };
    const auto& t = services_->components().theme_scope(id)->snapshot().avatar();
    s->view.image_visible =
        s->image_ready || (!std::holds_alternative<std::monostate>(s->source.get().value) && !s->image_failed);
    s->image_shape.set(s->shape == AvatarShape::Circle ? ImageShape::Circle : ImageShape::RoundedRectangle);
    if (!current()) {
        return;
    }
    const auto m = metrics(t.metrics, s->size, services_->responsive_layout().window_width().get(), s->breakpoints);
    s->color.set({t.colors.text.red(), t.colors.text.green(), t.colors.text.blue(), t.colors.text.alpha()});
    auto image_theme = s->image_theme.get();
    image_theme.image.tokens.radius = dp(std::min(m.radius, m.size / 2));
    s->image_theme.set(std::move(image_theme));
    if (!current()) {
        return;
    }
    s->icon_font.set({t.metrics.font_family, 400, false, m.icon, m.icon});
    geometry |= services_->components().set_branch_active(id, s->visible);
    services_->nodes().require(s->node).out_of_flow = !s->visible;
    geometry |=
        services_->components().set_branch_active(s->view.icon, s->has_icon && s->use_icon && !s->view.image_visible);
    geometry |= services_->components().set_branch_active(s->view.content,
                                                          (!s->has_icon || !s->use_icon) && !s->view.image_visible);
    s->view.visible = s->visible;
    s->view.icon_visible = s->has_icon && s->use_icon;
    if (geometry) {
        services_->mark_scene_structure_dirty();
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
    // This may synchronously blur a rich child and destroy/rebind this generation.
    const auto image_visible = s->image_visible;
    const bool shown = s->visible;
    services_->set_input_blocked(id, !shown);
    if (!current()) {
        return;
    }
    image_visible.set(shown);
    if (!current()) {
        return;
    }
    s = services_->components().state<AvatarState>(id);
    if (!s) {
        return;
    }
    const auto content = s->view.content;
    const auto icon = s->view.icon;
    const bool image = s->view.image_visible;
    const bool use_icon = s->view.icon_visible;
    const bool rich = s->rich;
    const bool has_icon = s->has_icon;
    services_->set_input_blocked(content, rich && (!shown || image || use_icon));
    if (!services_->components().contains(id)) {
        return;
    }
    services_->set_input_blocked(icon, has_icon && (!shown || image || !use_icon));
}

void AvatarComponentHost::mount(const AvatarProps& props, const AvatarSlots& slots) {
    static_cast<void>(mount_avatar(props, slots, false));
}

void AvatarComponentHost::ensure_image(runtime::ComponentId id) {
    const auto storage = services_->components().retain_state<AvatarState>(id);
    if (!storage || storage->view.image.valid() ||
        std::holds_alternative<std::monostate>(storage->source.get().value)) {
        return;
    }
    auto& s = *storage;
    auto& services = *services_;
    append_retained_slot(services, id, Content{[&] {
                             const auto before = services.image().mounted().size();
                             ImageSlots image_slots;
                             image_slots.placeholder =
                                 s.placeholder ? ImagePlaceholder{SlotContentAccess::function(*s.placeholder)}
                                               : ImagePlaceholder{[] {}};
                             image_slots.error = ImageErrorContent{[] {}};
                             Theme(ThemeProps{}.config(s.image_theme), ThemeContent{[&] {
                                       Image(ImageProps{}
                                                 .source(s.source)
                                                 .fallback(s.fallback)
                                                 .preview(false)
                                                 .visible(s.image_visible)
                                                 .shape(s.image_shape)
                                                 .fit(ImageFit::Cover)
                                                 .layout(LayoutStyle{}.width(s.image_side).height(s.image_side))
                                                 .onLoad([this, id] {
                                                     auto* state = services_->components().state<AvatarState>(id);
                                                     if (!state) {
                                                         return;
                                                     }
                                                     state->image_ready = true;
                                                     state->image_failed = false;
                                                     const auto callback = state->on_load;
                                                     refresh(id, true);
                                                     try {
                                                         if (callback) {
                                                             callback();
                                                         }
                                                     } catch (...) {
                                                         services_->retire_failed_component(id);
                                                         throw;
                                                     }
                                                 })
                                                 .onError([this, id](const ImageError& error) {
                                                     auto* state = services_->components().state<AvatarState>(id);
                                                     if (!state) {
                                                         return;
                                                     }
                                                     const auto revision = state->source_revision;
                                                     const auto callback = state->on_error;
                                                     bool fallback = true;
                                                     try {
                                                         if (callback) {
                                                             fallback = callback(error);
                                                         }
                                                     } catch (...) {
                                                         services_->retire_failed_component(id);
                                                         throw;
                                                     }
                                                     state = services_->components().state<AvatarState>(id);
                                                     if (!state || state->source_revision != revision) {
                                                         return;
                                                     }
                                                     state->image_ready = false;
                                                     state->image_failed = fallback;
                                                     state->error_suppressed = !fallback;
                                                     if (!fallback) {
                                                         const auto signal = state->fallback;
                                                         signal.set(ImageSource{});
                                                         state = services_->components().state<AvatarState>(id);
                                                         if (!state || state->source_revision != revision) {
                                                             return;
                                                         }
                                                     }
                                                     refresh(id, true);
                                                 }),
                                             image_slots);
                                   }});
                             s.view.image = services.image().mounted()[before];
                         }});
}

runtime::ComponentId AvatarComponentHost::mount_avatar(const AvatarProps& p, const AvatarSlots& slots, bool grouped,
                                                       std::optional<Prop<bool>> icon_visible) {
    const auto size = read_prop(p.size_);
    const auto shape = read_prop(p.shape_);
    const auto gap = read_prop(p.gap_);
    validate_size(size);
    validate_shape(shape);
    validate_gap(gap);
    auto& services = *services_;
    auto& b = runtime::require_component_build_context();
    const auto id = b.mount_component<AvatarState>();
    const auto storage = b.host().retain_state<AvatarState>(id);
    auto& s = *storage;
    s.node = b.root(id);
    s.size = size;
    s.shape = shape;
    s.gap = gap;
    s.visible = read_prop(p.visible_);
    s.has_icon = slots.icon.has_value();
    s.use_icon = !icon_visible || read_prop(*icon_visible);
    s.rich = slots.content.has_value();
    s.placeholder = slots.placeholder;
    s.grouped = grouped;
    s.on_error = p.on_error_;
    s.on_load = p.on_load_;
    s.source.set(read_prop(p.source_));
    s.configured_fallback = read_prop(p.fallback_);
    s.fallback.set(s.configured_fallback);
    ThemeConfig image_theme;
    image_theme.image.tokens.background = Color{};
    image_theme.image.tokens.border_width = dp(0);
    s.image_theme.set(image_theme);
    s.breakpoints = read_prop(p.breakpoints_);
    s.text.set(read_prop(p.text_));
    const auto& t = b.theme_scope()->snapshot().avatar();
    const auto m = metrics(t.metrics, size);
    s.font.set({t.metrics.font_family, 400, false, m.text, m.text});
    s.icon_font.set({t.metrics.font_family, 400, false, m.icon, m.icon});
    runtime::connect_layout_style(b.scope(id), p.layout_, s.node, services.nodes(), services.dirty());
    services.nodes().require(s.node).clip_content = true;
    services.nodes().require(s.node).clip_decorations = false;
    b.on_resource_cleanup(id, [&services, id, node = s.node] {
        services.interactions().set_input_blocked(id, false);
        services.layout().remove_layout(node);
    });
    s.measurement =
        services.text().scene_service().create(s.node, s.text.get(), services.text().resolve_fonts(s.font.get()),
                                               static_cast<std::uint32_t>(std::lround(m.text)), {m.text, infinity});
    b.on_resource_cleanup(id,
                          [&services, measure = s.measurement] { services.text().scene_service().destroy(measure); });
    const auto fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.view.surface = services.surfaces().create_content_range(fragment, {});
    b.on_resource_cleanup(
        id, [&services, surface = s.view.surface] { services.surfaces().destroy_content_range(surface); });
    if (grouped) {
        const auto border_fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::after_children);
        s.border = services.surfaces().create_content_range(border_fragment, {});
        b.on_resource_cleanup(id,
                              [&services, border = s.border] { services.surfaces().destroy_content_range(border); });
    }
    b.mount_slot(id, Content{[&] {
                     auto& c = runtime::require_component_build_context();
                     s.view.icon = region(services);
                     c.mount_slot_with_semantic_text_style(
                         s.view.icon, slots.icon ? Content{SlotContentAccess::function(*slots.icon)} : Content{[] {}},
                         s.color, s.icon_font);
                     if (!services.components().contains(id)) {
                         return;
                     }
                     s.view.content = region(services);
                     c.mount_slot_with_semantic_text_style(
                         s.view.content,
                         slots.content ? Content{SlotContentAccess::function(*slots.content)}
                                       : Content{[text = s.text] { Text(TextProps{}.content(text)); }},
                         s.color, s.font);
                     if (!services.components().contains(id)) {
                         return;
                     }
                     ensure_image(id);
                 }});
    if (!services.components().contains(id)) {
        return id;
    }
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) { return measure(id, e, c); },
            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) { place(id, e, r); }});
    mounted_.push_back(id);
    auto& scope = b.scope(id);
    connect_prop(scope, p.source_, [this, id](ImageSource value) {
        auto* state = services_->components().state<AvatarState>(id);
        if (!state || state->source.get() == value) {
            return;
        }
        ++state->source_revision;
        const auto fallback = state->fallback;
        const auto configured = state->configured_fallback;
        fallback.set(configured);
        state = services_->components().state<AvatarState>(id);
        if (!state) {
            return;
        }
        state->image_ready = false;
        state->image_failed = false;
        state->error_suppressed = false;
        const auto signal = state->source;
        refresh(id, true);
        signal.set(std::move(value));
        ensure_image(id);
        refresh(id, true);
    });
    connect_prop(scope, p.fallback_, [this, id](ImageSource value) {
        if (auto* state = services_->components().state<AvatarState>(id)) {
            state->configured_fallback = value;
            state->fallback.set(std::move(value));
        }
    });
    connect_prop(scope, p.breakpoints_, [this, id](Breakpoints value) {
        if (auto* state = services_->components().state<AvatarState>(id)) {
            state->breakpoints = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, Prop<float>{services.responsive_layout().window_width()},
                 [this, id](float) { refresh(id, true); });
    connect_prop(scope, p.size_, [this, id](AvatarSizeValue v) {
        validate_size(v);
        if (auto* s = services_->components().state<AvatarState>(id)) {
            s->size = v;
            refresh(id, true);
        }
    });
    connect_prop(scope, p.shape_, [this, id](AvatarShape v) {
        validate_shape(v);
        if (auto* s = services_->components().state<AvatarState>(id)) {
            s->shape = v;
            refresh(id, true);
        }
    });
    connect_prop(scope, p.gap_, [this, id](float v) {
        validate_gap(v);
        if (auto* s = services_->components().state<AvatarState>(id)) {
            s->gap = v;
            refresh(id, true);
        }
    });
    connect_prop(scope, p.visible_, [this, id](bool v) {
        if (auto* s = services_->components().state<AvatarState>(id)) {
            s->visible = v;
            refresh(id, true);
        }
    });
    connect_prop(scope, p.text_, [this, id](String v) {
        if (auto* s = services_->components().state<AvatarState>(id)) {
            services_->text().scene_service().set_content(s->measurement, v);
            s->text.set(v);
            refresh(id, true);
        }
    });
    if (icon_visible) {
        connect_prop(scope, *icon_visible, [this, id](bool v) {
            if (auto* s = services_->components().state<AvatarState>(id)) {
                s->use_icon = v;
                refresh(id, true);
            }
        });
    }
    const auto theme = b.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::avatar_colors, &theme_runtime::ThemeScope::avatar_metrics};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { refresh(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    refresh(id, true);
    return id;
}

runtime::Size AvatarComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& e, layout::Constraints c) {
    auto& s = *services_->components().state<AvatarState>(id);
    const auto& t = services_->components().theme_scope(id)->snapshot().avatar();
    const auto m = metrics(t.metrics, s.size, services_->responsive_layout().window_width().get(), s.breakpoints);
    const float side = std::max(0.0F, std::min({m.size, c.max_width, c.max_height}));
    const float interior = std::max(0.0F, side - 2 * std::min(t.metrics.border_width, side / 2));
    const float safe =
        s.shape == AvatarShape::Circle
            ? interior / std::sqrt(2.0F)
            : std::max(0.0F, interior - 2 * std::min(m.radius, interior / 2) * (1 - 1 / std::sqrt(2.0F)));
    const float width = std::max(0.0F, std::min(safe, side - 2 * std::min(s.gap, side / 2)));
    s.view.viewport = {0, 0, width, safe};
    s.image_side.set(dp(side));
    if (s.view.image.valid()) {
        static_cast<void>(e.measure_child(services_->components().root(s.view.image), {side, side, side, side}));
    }
    float font = m.text;
    if (!s.rich && !s.view.icon_visible) {
        auto& scene = services_->text().scene_service();
        const auto measured = [&](std::uint32_t pixels) {
            runtime::SemanticTypography typography{t.metrics.font_family, 400, false, static_cast<float>(pixels),
                                                   static_cast<float>(pixels)};
            scene.set_font_chain(s.measurement, services_->text().resolve_fonts(typography));
            scene.set_pixel_size(s.measurement, pixels);
            scene.set_line_height(s.measurement, static_cast<float>(pixels));
            if (!scene.synchronize_measurement(s.measurement, infinity)) {
                throw std::runtime_error("Avatar text measurement failed");
            }
            const auto& natural = scene.text_state(s.measurement).measurement();
            return runtime::Size{natural.width, natural.height};
        };
        const auto maximum = static_cast<std::uint32_t>(std::lround(m.text));
        auto natural = measured(maximum);
        s.view.natural_text_width = natural.width;
        std::uint32_t low = 1;
        std::uint32_t high = maximum;
        std::uint32_t best = 1;
        while (low <= high) {
            const auto middle = low + (high - low) / 2;
            const auto actual = middle == maximum ? natural : measured(middle);
            if (actual.width <= width && actual.height <= safe) {
                best = middle;
                low = middle + 1;
            } else {
                high = middle - 1;
            }
        }
        font = static_cast<float>(best);
    }
    s.font.set({t.metrics.font_family, 400, false, font, font});
    s.view.font_size = font;
    for (auto child : {s.view.content, s.view.icon}) {
        static_cast<void>(e.measure_child(services_->components().root(child), {0, infinity, 0, infinity}));
    }
    return c.constrain({side, side});
}

void AvatarComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& e, runtime::Rect r) {
    auto& s = *services_->components().state<AvatarState>(id);
    auto viewport = s.view.viewport;
    viewport.width = std::min(viewport.width, r.width);
    viewport.height = std::min(viewport.height, r.height);
    viewport.x = r.x + (r.width - viewport.width) / 2;
    viewport.y = r.y + (r.height - viewport.height) / 2;
    s.view.viewport = viewport;
    const auto m = metrics(services_->components().theme_scope(id)->snapshot().avatar().metrics, s.size,
                           services_->responsive_layout().window_width().get(), s.breakpoints);
    const float side = std::min({m.size, r.width, r.height});
    if (s.view.image.valid()) {
        e.place_child(services_->components().root(s.view.image),
                      {r.x + (r.width - side) / 2, r.y + (r.height - side) / 2, side, side}, true, true);
    }
    for (auto child : {s.view.content, s.view.icon}) {
        e.place_child(services_->components().root(child), viewport, true, true);
    }
}

void AvatarComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<AvatarState>(id);
        if (!s) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().avatar();
        const auto& node = services_->nodes().require(s->node);
        std::vector<graphics::RoundedEffectInstance> effects;
        std::vector<graphics::RoundedEffectInstance> border_effects;
        if (services_->components().branch_active(id) && node.bounds.width > 0 && node.bounds.height > 0) {
            const auto m =
                metrics(t.metrics, s->size, services_->responsive_layout().window_width().get(), s->breakpoints);
            const float side = std::min({m.size, node.bounds.width, node.bounds.height});
            const graphics::LogicalRoundedRect shape{{node.bounds.x + (node.bounds.width - side) / 2,
                                                      node.bounds.y + (node.bounds.height - side) / 2, side, side},
                                                     s->shape == AvatarShape::Circle ? side / 2
                                                                                     : std::min(m.radius, side / 2)};
            const auto clip =
                services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
            const float opacity = services_->nodes().content_opacity(s->node);
            const auto fill =
                graphics::make_corner_fill_effects(shape, {true, true, true, true}, t.colors.background, opacity,
                                                   node.translation, graphics::EffectClip{1, clip});
            effects.insert(effects.end(), fill.begin(), fill.end());
            if (s->grouped) {
                const auto border = std::min(t.metrics.border_width, side / 2);
                const graphics::LogicalRoundedRect inner{
                    {shape.rect.x + border, shape.rect.y + border, side - 2 * border, side - 2 * border},
                    std::max(0.0F, shape.radius - border)};
                const auto ring = graphics::make_corner_outline_effects(
                    inner, {true, true, true, true}, border, 0, t.colors.group_border, opacity, node.translation,
                    graphics::EffectClip{1, clip});
                border_effects.insert(border_effects.end(), ring.begin(), ring.end());
            }
        }
        static_cast<void>(services_->surfaces().update_content_effects(s->view.surface, effects));
        if (s->grouped) {
            static_cast<void>(services_->surfaces().update_content_effects(s->border, border_effects));
        }
    }
}

void AvatarComponentHost::refresh_group(runtime::ComponentId id) {
    const auto storage = services_->components().retain_state<GroupState>(id);
    auto* s = services_->components().state<GroupState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const auto revision = s->revision;
    const auto current = [&] {
        return services_->components().contains(id) && s->revision == revision;
    };
    const auto count =
        static_cast<std::size_t>(std::ranges::count_if(s->items, [](const auto& i) { return i.visible; }));
    const auto limit = s->max.count ? static_cast<std::size_t>(*s->max.count) : count;
    s->view.overflow_count = count > limit ? count - limit : 0;
    const bool visible = s->visible && count > 0;
    const bool overflow = visible && s->view.overflow_count > 0;
    std::size_t index = 0;
    for (auto& item : s->items) {
        const bool in = item.visible && index < limit;
        if (item.visible) {
            ++index;
        }
        item.view.inline_visible = visible && in;
        item.view.overflow_visible = visible && item.visible && !in;
        item.inline_visible.set(item.view.inline_visible);
        if (!current()) {
            return;
        }
        item.overflow_visible.set(item.view.overflow_visible && !s->custom.valid());
        if (!current()) {
            return;
        }
    }
    s->overflow_visible.set(overflow);
    if (!current()) {
        return;
    }
    s->overflow_text.set(String::from_utf8("+" + std::to_string(s->view.overflow_count)).value());
    services_->components().set_branch_active(id, visible);
    services_->nodes().require(s->node).out_of_flow = !visible;
    services_->components().set_branch_active(s->view.popover, overflow);
    services_->nodes().require(services_->components().root(s->view.popover)).out_of_flow = !overflow;
    services_->mark_scene_structure_dirty();
    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                               runtime::DirtyFlags::HitTest);
    if (s->popup_row.valid()) {
        services_->dirty().invalidate(services_->components().root(s->popup_row),
                                      runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    }
    // disabled.set closes the popup and may synchronously run a user's focus callback.
    const auto disabled = s->disabled;
    services_->set_input_blocked(id, !visible);
    if (!current()) {
        return;
    }
    disabled.set(!overflow);
}

void AvatarComponentHost::replace_popover(runtime::ComponentId id,
                                          std::shared_ptr<const AvatarGroupPopoverRenderer> renderer) {
    auto* state = services_->components().state<GroupState>(id);
    if (!state || state->renderer == renderer) {
        return;
    }
    const auto revision = ++state->revision;
    const auto old = state->custom;
    runtime::ComponentId replacement;
    try {
        if (renderer) {
            append_retained_slot(*services_, state->popup_row, Content{[&] {
                auto& build = runtime::require_component_build_context();
                replacement = region(*services_);
                build.mount_slot(replacement, Content{[renderer] {
                                     FormPrivateEditorScope private_scope;
                                     if (renderer->render) {
                                         renderer->render();
                                     }
                                 }});
            }});
        }
        state = services_->components().state<GroupState>(id);
        if (!state || state->revision != revision) {
            if (replacement.valid()) {
                services_->destroy(replacement);
            }
            return;
        }
        state->renderer = std::move(renderer);
        state->custom = replacement;
        if (old.valid()) {
            services_->destroy(old);
        }
        refresh_group(id);
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

void AvatarComponentHost::reconcile(runtime::ComponentId id, const std::vector<AvatarGroupItem>& items) {
    const auto storage = services_->components().retain_state<GroupState>(id);
    if (!storage) {
        return;
    }
    const auto request_revision = ++storage->request_revision;
    if (items.size() > 1024) {
        throw std::length_error("AvatarGroup supports at most 1024 items");
    }
    std::set<std::string> keys;
    // Read and validate the entire application update before changing any retained state.
    for (const auto& item : items) {
        if (item.key_.empty() || !keys.insert(std::string(item.key_.view().bytes())).second) {
            throw std::invalid_argument("AvatarGroup keys must be nonempty and unique");
        }
        if (item.size_) {
            validate_size(read_prop(*item.size_));
        }
        if (item.shape_) {
            validate_shape(read_prop(*item.shape_));
        }
        static_cast<void>(read_prop(item.text_));
        static_cast<void>(read_prop(item.source_));
        static_cast<void>(read_prop(item.visible_));
        static_cast<void>(read_prop(item.icon_));
    }
    auto* state = services_->components().state<GroupState>(id);
    if (!state) {
        return;
    }
    if (state->request_revision != request_revision) {
        return;
    }
    if (state->reconciling) {
        state->pending_items = items;
        return;
    }
    const auto revision = ++state->revision;
    state->reconciling = true;
    auto previous = std::move(state->items);
    std::vector<GroupItem> next;
    try {
        for (const auto& item : items) {
            auto found = std::find_if(previous.begin(), previous.end(),
                                      [&](const auto& row) { return row.view.key == item.key_; });
            GroupItem row;
            if (found != previous.end()) {
                row = std::move(*found);
                found->view.key = {};
            }
            row.view.key = item.key_;
            row.bindings = std::make_shared<Scope>();
            row.visible = read_prop(item.visible_);
            row.own_size = item.size_.has_value();
            row.own_shape = item.shape_.has_value();
            row.text.set(read_prop(item.text_));
            row.source.set(read_prop(item.source_));
            if (!services_->components().contains(id) || state->revision != revision) {
                services_->retire_failed_component(id);
                return;
            }
            row.size.set(item.size_ ? read_prop(*item.size_) : state->size.get());
            row.shape.set(item.shape_ ? read_prop(*item.shape_) : state->shape.get());
            const auto icon = read_prop(item.icon_);
            row.icon.set(icon.value_or(IconName::UserOutlined));
            row.has_icon.set(icon.has_value());
            if (!row.view.inline_avatar.valid()) {
                const auto mount = [&](bool popup) {
                    append_retained_slot(*services_, popup ? state->popup_row : id, Content{[&] {
                        AvatarSlots slots;
                        slots.icon = AvatarIcon{[icon = row.icon] { Icon(IconProps{}.name(icon)); }};
                        const auto child = mount_avatar(AvatarProps{}
                                                            .text(row.text)
                                                            .source(row.source)
                                                            .size(row.size)
                                                            .shape(row.shape)
                                                            .visible(popup ? row.overflow_visible : row.inline_visible),
                                                        slots, true, Prop<bool>{row.has_icon});
                        if (popup) {
                            row.view.overflow_avatar = child;
                        } else {
                            row.view.inline_avatar = child;
                        }
                    }});
                };
                mount(false);
                mount(true);
            }
            next.push_back(std::move(row));
        }
        state = services_->components().state<GroupState>(id);
        if (!state || state->revision != revision) {
            services_->retire_failed_component(id);
            return;
        }
        state->items = std::move(next);
        for (const auto& row : previous) {
            if (!row.view.key.empty()) {
                services_->destroy(row.view.inline_avatar);
                services_->destroy(row.view.overflow_avatar);
                if (!services_->components().contains(id)) {
                    return;
                }
            }
        }
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (!services_->components().contains(id) || state->revision != revision) {
                services_->retire_failed_component(id);
                return;
            }
            const auto key = items[i].key_;
            const auto find = [this, id, key]() -> GroupItem* {
                auto* current = services_->components().state<GroupState>(id);
                if (!current) {
                    return nullptr;
                }
                auto it = std::find_if(current->items.begin(), current->items.end(),
                                       [&](auto& row) { return row.view.key == key; });
                return it == current->items.end() ? nullptr : &*it;
            };
            auto bindings = state->items[i].bindings;
            connect_prop(*bindings, items[i].text_, [find](String v) {
                if (auto* row = find()) {
                    row->text.set(std::move(v));
                }
            });
            connect_prop(*bindings, items[i].source_, [find](ImageSource v) {
                if (auto* row = find()) {
                    row->source.set(std::move(v));
                }
            });
            connect_prop(*bindings, items[i].visible_, [this, id, find](bool v) {
                if (auto* row = find()) {
                    if (row->visible != v) {
                        if (auto* state = services_->components().state<GroupState>(id)) {
                            ++state->revision;
                        }
                    }
                    row->visible = v;
                    refresh_group(id);
                }
            });
            connect_prop(*bindings, items[i].icon_, [this, id, find](std::optional<IconName> v) {
                if (auto* row = find()) {
                    row->icon.set(v.value_or(IconName::UserOutlined));
                    row->has_icon.set(v.has_value());
                    refresh_group(id);
                }
            });
            if (items[i].size_) {
                connect_prop(*bindings, *items[i].size_, [find](AvatarSizeValue v) {
                    validate_size(v);
                    if (auto* row = find()) {
                        row->size.set(v);
                    }
                });
            }
            if (items[i].shape_) {
                connect_prop(*bindings, *items[i].shape_, [find](AvatarShape v) {
                    validate_shape(v);
                    if (auto* row = find()) {
                        row->shape.set(v);
                    }
                });
            }
        }
        std::vector<runtime::ComponentId> order;
        for (const auto& row : state->items) {
            order.push_back(row.view.inline_avatar);
        }
        order.push_back(state->view.popover);
        services_->components().reorder_children(id, order);
        order.clear();
        for (const auto& row : state->items) {
            order.push_back(row.view.overflow_avatar);
        }
        if (state->custom.valid()) {
            order.push_back(state->custom);
        }
        services_->components().reorder_children(state->popup_row, order);
        refresh_group(id);
        state = services_->components().state<GroupState>(id);
        if (state) {
            state->reconciling = false;
            auto pending = std::move(state->pending_items);
            state->pending_items.reset();
            if (pending) {
                reconcile(id, *pending);
            }
        }
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

void AvatarComponentHost::mount(const AvatarGroupProps& p, const std::vector<AvatarGroupItem>& items) {
    validate_size(read_prop(p.size_));
    validate_shape(read_prop(p.shape_));
    validate_max(read_prop(p.max_));
    auto& services = *services_;
    auto& b = runtime::require_component_build_context();
    const auto id = b.mount_component<GroupState>();
    const auto storage = b.host().retain_state<GroupState>(id);
    auto& s = *storage;
    s.node = b.root(id);
    s.visible = read_prop(p.visible_);
    s.rtl = read_prop(p.rtl_);
    s.max = read_prop(p.max_);
    s.size.set(read_prop(p.size_));
    s.shape.set(read_prop(p.shape_));
    s.trigger.set(s.max.trigger);
    s.placement.set(s.max.placement);
    services.nodes().require(s.node).clip_content = true;
    services.nodes().require(s.node).clip_decorations = false;
    runtime::connect_layout_style(b.scope(id), p.layout_, s.node, services.nodes(), services.dirty());
    b.on_resource_cleanup(id, [&services, id, node = s.node] {
        services.interactions().set_input_blocked(id, false);
        services.layout().remove_layout(node);
    });
    b.mount_slot(
        id, Content{[&] {
            const auto before = services.popover().mounted().size();
            PopoverSlots slots;
            slots.content = PopoverContent{[&] {
                FormPrivateEditorScope private_scope;
                auto& c = runtime::require_component_build_context();
                s.popup_row = region(services);
                services.layout().set_layout(
                    c.root(s.popup_row),
                    layout::ComponentLayout{[this, id](layout::LayoutEngine& e, runtime::NodeId,
                                                       layout::Constraints c) { return measure_group(id, e, c, true); },
                                            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) {
                                                place_group(id, e, r, true);
                                            }});
            }};
            Popover(PopoverProps{}
                        .disabled(s.disabled)
                        .trigger(s.trigger)
                        .placement(s.placement)
                        .onOpenChange([this, id, callback = p.on_open_](bool value) {
                            try {
                                if (callback) {
                                    callback(value);
                                }
                            } catch (...) {
                                services_->retire_failed_component(id);
                                throw;
                            }
                        }),
                    PopoverTrigger{[&] {
                        s.view.overflow = mount_avatar(
                            AvatarProps{}.text(s.overflow_text).size(s.size).shape(s.shape).visible(s.overflow_visible),
                            {}, true);
                    }},
                    slots);
            s.view.popover = services.popover().mounted()[before];
        }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{[this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) {
                                            return measure_group(id, e, c, false);
                                        },
                                        [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) {
                                            place_group(id, e, r, false);
                                        }});
    groups_.push_back(id);
    auto& scope = b.scope(id);
    connect_prop(scope, p.visible_, [this, id](bool v) {
        if (auto* s = services_->components().state<GroupState>(id)) {
            ++s->revision;
            s->visible = v;
            refresh_group(id);
        }
    });
    connect_prop(scope, p.rtl_, [this, id](bool v) {
        if (auto* s = services_->components().state<GroupState>(id)) {
            ++s->revision;
            s->rtl = v;
            refresh_group(id);
        }
    });
    connect_prop(scope, p.size_, [this, id](AvatarSizeValue v) {
        validate_size(v);
        const auto storage = services_->components().retain_state<GroupState>(id);
        if (auto* s = storage.get()) {
            const auto revision = ++s->revision;
            const auto current = [&] {
                return services_->components().contains(id) && s->revision == revision;
            };
            s->size.set(v);
            if (!current()) {
                return;
            }
            for (auto& row : s->items) {
                if (!row.own_size) {
                    row.size.set(v);
                    if (!current()) {
                        return;
                    }
                }
            }
            refresh_group(id);
        }
    });
    connect_prop(scope, p.shape_, [this, id](AvatarShape v) {
        validate_shape(v);
        const auto storage = services_->components().retain_state<GroupState>(id);
        if (auto* s = storage.get()) {
            const auto revision = ++s->revision;
            const auto current = [&] {
                return services_->components().contains(id) && s->revision == revision;
            };
            s->shape.set(v);
            if (!current()) {
                return;
            }
            for (auto& row : s->items) {
                if (!row.own_shape) {
                    row.shape.set(v);
                    if (!current()) {
                        return;
                    }
                }
            }
            refresh_group(id);
        }
    });
    connect_prop(scope, p.max_, [this, id](AvatarGroupMax v) {
        validate_max(v);
        if (auto* s = services_->components().state<GroupState>(id)) {
            s->max = v;
            const auto revision = ++s->revision;
            const auto trigger = s->trigger;
            const auto placement = s->placement;
            trigger.set(v.trigger);
            s = services_->components().state<GroupState>(id);
            if (!s || s->revision != revision) {
                return;
            }
            placement.set(v.placement);
            s = services_->components().state<GroupState>(id);
            if (!s || s->revision != revision) {
                return;
            }
            refresh_group(id);
        }
    });
    if (p.items_) {
        connect_prop(
            scope, *p.items_, [this, id](std::vector<AvatarGroupItem> v) { reconcile(id, v); },
            [](const auto&, const auto&) { return false; });
    } else {
        reconcile(id, items);
    }
    connect_prop(scope, p.popover_, [this, id](std::shared_ptr<const AvatarGroupPopoverRenderer> v) {
        replace_popover(id, std::move(v));
    });
    const auto theme = b.theme_scope();
    s.subscriptions[0] = theme->capture([this, id](auto) { refresh_group(id); },
                                        [theme] { static_cast<void>(theme->avatar_metrics()); });
    refresh_group(id);
}

runtime::Size AvatarComponentHost::measure_group(runtime::ComponentId id, layout::LayoutEngine& e,
                                                 layout::Constraints c, bool popup) {
    auto& s = *services_->components().state<GroupState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().avatar().metrics;
    if (popup) {
        if (s.custom.valid()) {
            return e.measure_child(services_->components().root(s.custom), c);
        }
        float x = 0;
        float y = 0;
        float row_height = 0;
        float width = 0;
        for (auto& item : s.items) {
            if (!item.view.overflow_visible) {
                continue;
            }
            const auto size = e.measure_child(services_->components().root(item.view.overflow_avatar),
                                              {0, c.max_width, 0, c.max_height});
            if (x > 0 && x + m.group_space + size.width > c.max_width) {
                y += row_height + m.group_space;
                x = 0;
                row_height = 0;
            }
            if (x > 0) {
                x += m.group_space;
            }
            item.popup_frame = {x, y, size.width, size.height};
            x += size.width;
            width = std::max(width, x);
            row_height = std::max(row_height, size.height);
        }
        s.popup_height = y + row_height;
        return c.constrain({width, s.popup_height});
    }
    float width = 0;
    float height = 0;
    std::size_t count = 0;
    const auto add = [&](runtime::ComponentId child) {
        const auto size = e.measure_child(services_->components().root(child), {0, infinity, 0, c.max_height});
        if (count++) {
            width = std::max(0.0F, width + m.group_overlapping);
        }
        width += size.width;
        height = std::max(height, size.height);
    };
    for (const auto& item : s.items) {
        if (item.view.inline_visible) {
            add(item.view.inline_avatar);
        }
    }
    if (s.view.overflow_count > 0) {
        add(s.view.popover);
    }
    return c.constrain({width, height});
}

void AvatarComponentHost::place_group(runtime::ComponentId id, layout::LayoutEngine& e, runtime::Rect r, bool popup) {
    auto& s = *services_->components().state<GroupState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().avatar().metrics;
    if (popup) {
        if (s.custom.valid()) {
            e.place_child(services_->components().root(s.custom), r);
            return;
        }
        s.popup_scroll = std::clamp(s.popup_scroll, 0.0F, std::max(0.0F, s.popup_height - r.height));
        for (const auto& item : s.items) {
            if (item.view.overflow_visible) {
                const auto frame = item.popup_frame;
                e.place_child(services_->components().root(item.view.overflow_avatar),
                              {r.x + frame.x, r.y + frame.y - s.popup_scroll, frame.width, frame.height});
            }
        }
        return;
    }
    float x = 0;
    std::size_t count = 0;
    const auto add = [&](runtime::ComponentId child) {
        const auto node = services_->components().root(child);
        const auto size = services_->nodes().require(node).layout_size;
        if (count++) {
            x = std::max(0.0F, x + m.group_overlapping);
        }
        e.place_child(node, {s.rtl ? r.x + r.width - x - size.width : r.x + x, r.y + (r.height - size.height) / 2,
                             size.width, size.height});
        x += size.width;
    };
    for (const auto& item : s.items) {
        if (item.view.inline_visible) {
            add(item.view.inline_avatar);
        }
    }
    if (s.view.overflow_count > 0) {
        add(s.view.popover);
    }
}

bool AvatarComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    auto layer = target ? std::optional{target->component} : std::nullopt;
    while (layer && !services_->components().is_window_layer_root(*layer)) {
        layer = services_->components().parent(*layer);
    }
    if (!layer) {
        return false;
    }
    for (auto id : groups_) {
        auto* s = services_->components().state<GroupState>(id);
        if (!s) {
            continue;
        }
        const auto popup = services_->popover().snapshot(s->view.popover);
        if (popup.popup != *layer || !popup.geometry.visible ||
            !services_->interactions().component_active(popup.popup)) {
            continue;
        }
        const auto node = services_->components().root(s->popup_row);
        const auto& row = services_->nodes().require(node);
        const auto clip = services_->nodes().content_clip(node, popup.geometry.bounds);
        if (event.x < clip.x || event.x >= clip.x + clip.width || event.y < clip.y || event.y >= clip.y + clip.height) {
            return false;
        }
        const float next = std::clamp(s->popup_scroll - event.delta_y * 32.0F, 0.0F,
                                      std::max(0.0F, s->popup_height - row.bounds.height));
        if (next != s->popup_scroll) {
            s->popup_scroll = next;
            services_->dirty().invalidate(node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry |
                                                    runtime::DirtyFlags::HitTest);
        }
        return true;
    }
    return false;
}
} // namespace ryn::detail

namespace ryn {
void Avatar(AvatarProps p, AvatarSlots slots) {
    if (!detail::active_avatar) {
        throw std::logic_error("Avatar requires window component services");
    }
    detail::active_avatar->mount(p, slots);
}

void AvatarGroup(AvatarGroupProps p) {
    AvatarGroup(std::move(p), {});
}

void AvatarGroup(AvatarGroupProps p, std::vector<AvatarGroupItem> items) {
    if (!detail::active_avatar) {
        throw std::logic_error("AvatarGroup requires window component services");
    }
    detail::active_avatar->mount(p, items);
}
} // namespace ryn
