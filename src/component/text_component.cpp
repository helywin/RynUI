#include "component/text_component.hpp"

#include "component/layout_component_context.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ryn::detail {

struct TextPropsAccess final {
    [[nodiscard]] static const Prop<String>& content(
        const TextProps& props) noexcept {
        return props.content_;
    }

    [[nodiscard]] static const std::optional<Prop<TextTone>>& tone(
        const TextProps& props) noexcept {
        return props.tone_;
    }

    [[nodiscard]] static const LayoutStyle& layout(
        const TextProps& props) noexcept {
        return props.layout_;
    }
};

namespace {

struct TextComponentState final {
    TextSceneId scene;
    TextTone tone{TextTone::Primary};
    bool explicit_tone{};
    bool semantic_foreground{};
    bool semantic_typography{};
    runtime::SemanticTypography resolved_typography;
    theme_runtime::Subscription theme_subscription;
};

thread_local TextComponentHost* active_text_host = nullptr;

class ActiveTextHostGuard final {
public:
    explicit ActiveTextHostGuard(TextComponentHost& host) noexcept
        : previous_(active_text_host) {
        active_text_host = &host;
    }

    ActiveTextHostGuard(const ActiveTextHostGuard&) = delete;
    ActiveTextHostGuard& operator=(const ActiveTextHostGuard&) = delete;

    ~ActiveTextHostGuard() {
        active_text_host = previous_;
    }

private:
    TextComponentHost* previous_;
};

[[nodiscard]] std::array<float, 4> channels(Color color) noexcept {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

[[nodiscard]] Color tone_color(
    const ThemeSnapshot& theme,
    TextTone tone) noexcept {
    switch (tone) {
    case TextTone::Primary:
        return theme.text().color;
    case TextTone::Secondary:
        return theme.alias().color_text_secondary;
    case TextTone::Disabled:
        return theme.alias().color_text_disabled;
    }
    return theme.text().color;
}

void capture_tone_color(
    const std::shared_ptr<theme_runtime::ThemeScope>& theme,
    TextTone tone) {
    switch (tone) {
    case TextTone::Primary:
        static_cast<void>(theme->text_color());
        return;
    case TextTone::Secondary:
        static_cast<void>(theme->text_secondary_color());
        return;
    case TextTone::Disabled:
        static_cast<void>(theme->text_disabled_color());
        return;
    }
}

[[nodiscard]] std::uint64_t intrinsic_revision(
    TextSceneRevisions revisions) noexcept {
    return revisions.content + revisions.layout;
}

[[nodiscard]] bool valid_viewport(runtime::Size viewport) noexcept {
    return std::isfinite(viewport.width)
        && std::isfinite(viewport.height)
        && viewport.width > 0.0F
        && viewport.height > 0.0F;
}

} // namespace

TextComponentHost::TextComponentHost(
    runtime::NodeStore& nodes,
    layout::LayoutEngine& layout,
    runtime::DirtyQueues& dirty,
    TextSceneService& text_scene,
    std::vector<font::FontIdentity> default_font_chain)
    : TextComponentHost(
          nodes,
          layout,
          dirty,
          text_scene,
          [chain = std::move(default_font_chain)](
              SystemFontFamily,
              std::uint32_t,
              std::uint32_t) { return chain; }) {}

TextComponentHost::TextComponentHost(
    runtime::NodeStore& nodes,
    layout::LayoutEngine& layout,
    runtime::DirtyQueues& dirty,
    TextSceneService& text_scene,
    ThemeFontResolver font_resolver)
    : nodes_(&nodes),
      layout_(&layout),
      dirty_(&dirty),
      text_scene_(&text_scene),
      font_resolver_(std::move(font_resolver)),
      components_(nodes) {
    if (!font_resolver_) {
        throw std::invalid_argument(
            "TextComponentHost requires a Theme font resolver");
    }
    if (font_resolver_(SystemFontFamily::ui_sans, 400, 14).empty()) {
        throw std::invalid_argument(
            "TextComponentHost Theme font resolver returned an empty default chain");
    }
}

TextComponentHost::~TextComponentHost() {
    dispose();
}

void TextComponentHost::mount(const Content& content) {
    ActiveTextHostGuard guard(*this);
    LayoutComponentServices services{*nodes_, *layout_, *dirty_};
    ActiveLayoutComponentServices layout_services_guard(services);
    const auto mounted_before = mounted_texts_.size();
    try {
        components_.mount(content);
        layout_snapshot_valid_ = false;
    } catch (...) {
        mounted_texts_.resize(mounted_before);
        throw;
    }
}

bool TextComponentHost::destroy(runtime::ComponentId id) {
    if (!components_.destroy(id)) {
        return false;
    }
    std::erase_if(mounted_texts_, [this](const auto& text) {
        return !components_.contains(text.component);
    });
    layout_snapshot_valid_ = false;
    return true;
}

void TextComponentHost::dispose() noexcept {
    components_.dispose();
    mounted_texts_.clear();
    layout_snapshot_valid_ = false;
}

bool TextComponentHost::layout_and_synchronize(
    runtime::Size viewport,
    runtime::Rect clip,
    runtime::Point origin,
    float gap,
    bool clear_dirty,
    bool unbounded_root_height) {
    if (!valid_viewport(viewport)
            || !std::isfinite(origin.x)
            || !std::isfinite(origin.y)
            || !std::isfinite(gap)
            || gap < 0.0F) {
        throw std::invalid_argument(
            "Text component viewport, origin, and gap must be finite and valid");
    }

    const bool configuration_changed = !layout_snapshot_valid_
        || viewport != layout_viewport_
        || origin != layout_origin_
        || gap != layout_gap_
        || unbounded_root_height != layout_unbounded_root_height_;
    const bool needs_layout = configuration_changed
        || !dirty_->layout_roots().empty()
        || !dirty_->placement_roots().empty();
    layout_performed_last_sync_ = needs_layout;
    if (needs_layout) {
        float cursor_y = origin.y;
        for (const auto component : components_.root_components()) {
            if (!components_.contains(component)) {
                continue;
            }
            const auto node = components_.root(component);
            const auto remaining_width = std::max(0.0F, viewport.width - origin.x);
            const auto remaining_height = unbounded_root_height
                ? std::numeric_limits<float>::infinity()
                : std::max(0.0F, viewport.height - cursor_y);
            const auto outer = layout_->layout(
                node,
                {0.0F, remaining_width, 0.0F, remaining_height},
                {origin.x, cursor_y});
            cursor_y += outer.height + gap;
        }
        layout_viewport_ = viewport;
        layout_origin_ = origin;
        layout_gap_ = gap;
        layout_unbounded_root_height_ = unbounded_root_height;
        layout_snapshot_valid_ = true;
    }

    for (const auto& mounted : mounted_texts_) {
        if (!components_.contains(mounted.component)
                || !text_scene_->contains(mounted.scene)) {
            continue;
        }
        const auto node = components_.root(mounted.component);
        const auto& retained = nodes_->require(node);
        if (!text_scene_->synchronize(mounted.scene, {
                {retained.bounds.x, retained.bounds.y},
                viewport,
                clip,
                retained.translation,
                {},
                1.0F,
            })) {
            return false;
        }
    }
    if (clear_dirty) {
        dirty_->clear();
    }
    return true;
}

void TextComponentHost::attach_component_scene(
    component::ComponentSceneComposer& composer) noexcept {
    composer_ = &composer;
}

bool TextComponentHost::synchronize_scene_fragments(
    const std::function<std::optional<input::InteractionId>(
        runtime::ComponentId)>& interaction_for) {
    if (composer_ == nullptr) {
        return false;
    }
    bool changed = false;
    for (auto& mounted : mounted_texts_) {
        if (!mounted.fragment.has_value()
                || !components_.contains(mounted.component)
                || !text_scene_->contains(mounted.scene)) {
            continue;
        }
        std::vector<graphics::SceneDrawCommand> commands;
        const auto& primitive = text_scene_->primitive(mounted.scene);
        commands.reserve(primitive.draw_ranges.size());
        for (const auto& range : primitive.draw_ranges) {
            commands.push_back({
                graphics::SceneDrawKind::glyph,
                range.instances.first,
                range.instances.count,
                range.atlas_page,
            });
        }
        const auto interaction = interaction_for(mounted.component);
        if (commands == mounted.fragment_commands
                && interaction == mounted.interaction) {
            continue;
        }
        composer_->set_fragment(
            *mounted.fragment,
            commands,
            interaction);
        mounted.fragment_commands = std::move(commands);
        mounted.interaction = interaction;
        changed = true;
    }
    return changed;
}

bool TextComponentHost::layout_performed_last_sync() const noexcept {
    return layout_performed_last_sync_;
}

runtime::ComponentHost& TextComponentHost::components() noexcept {
    return components_;
}

const runtime::ComponentHost& TextComponentHost::components() const noexcept {
    return components_;
}

TextSceneService& TextComponentHost::scene_service() noexcept {
    return *text_scene_;
}

const TextSceneService& TextComponentHost::scene_service() const noexcept {
    return *text_scene_;
}

std::span<const MountedTextComponent>
TextComponentHost::mounted_texts() const noexcept {
    return mounted_texts_;
}

bool TextComponentHost::set_font_resolver(ThemeFontResolver font_resolver) {
    if (!font_resolver
            || font_resolver(SystemFontFamily::ui_sans, 400, 14).empty()) {
        throw std::invalid_argument(
            "Theme font resolver must provide a default UI font chain");
    }

    font_resolver_ = std::move(font_resolver);
    bool changed = false;
    for (const auto& mounted : mounted_texts_) {
        auto* state = components_.state<TextComponentState>(mounted.component);
        if (state == nullptr || !text_scene_->contains(state->scene)) {
            continue;
        }
        const auto& typography = state->resolved_typography;
        auto chain = font_resolver_(
            typography.font_family,
            typography.font_weight,
            static_cast<std::uint32_t>(std::lround(typography.font_size)));
        if (chain.empty()) {
            throw std::runtime_error("Theme font resolver returned an empty chain");
        }
        if (!text_scene_->set_font_chain(state->scene, std::move(chain))) {
            continue;
        }
        const auto node = components_.root(mounted.component);
        static_cast<void>(layout_->set_intrinsic_revision(
            node,
            intrinsic_revision(text_scene_->revisions(state->scene))));
        dirty_->invalidate(
            node,
            runtime::DirtyFlags::Measure
                | runtime::DirtyFlags::Layout
                | runtime::DirtyFlags::Geometry);
        changed = true;
    }
    return changed;
}

void TextComponentHost::record_mounted_text(
    runtime::ComponentId component,
    TextSceneId scene,
    std::optional<runtime::SceneFragmentId> fragment) {
    mounted_texts_.push_back({component, scene, fragment, std::nullopt, {}});
}

bool TextComponentHost::apply_typography(
    runtime::ComponentId component,
    runtime::SemanticTypography typography) {
    auto* state = components_.state<TextComponentState>(component);
    if (state == nullptr || !text_scene_->contains(state->scene)
            || state->resolved_typography == typography) {
        return false;
    }
    auto chain = font_resolver_(
        typography.font_family,
        typography.font_weight,
        static_cast<std::uint32_t>(std::lround(typography.font_size)));
    if (chain.empty()) {
        throw std::runtime_error("Theme font resolver returned an empty chain");
    }
    const bool font_selection_changed =
        state->resolved_typography.font_family != typography.font_family
        || state->resolved_typography.font_weight != typography.font_weight;
    const bool chain_changed = text_scene_->set_font_chain(
        state->scene, std::move(chain));
    if (font_selection_changed && !chain_changed) {
        text_scene_->request_reshape(state->scene);
    }
    bool changed = chain_changed || font_selection_changed;
    changed = text_scene_->set_pixel_size(
        state->scene,
        static_cast<std::uint32_t>(std::lround(typography.font_size))) || changed;
    changed = text_scene_->set_line_height(state->scene, typography.line_height)
        || changed;
    state->resolved_typography = typography;
    if (changed) {
        const auto node = components_.root(component);
        static_cast<void>(layout_->set_intrinsic_revision(
            node,
            intrinsic_revision(text_scene_->revisions(state->scene))));
        dirty_->invalidate(
            node,
            runtime::DirtyFlags::Measure
                | runtime::DirtyFlags::Layout
                | runtime::DirtyFlags::Geometry);
    }
    return changed;
}

void TextComponentHost::apply_theme(runtime::ComponentId component) {
    auto* state = components_.state<TextComponentState>(component);
    if (state == nullptr || !text_scene_->contains(state->scene)) {
        return;
    }
    const auto node = components_.root(component);
    const auto& theme = components_.theme_scope(component)->snapshot();
    bool typography_changed = false;
    if (!state->semantic_typography) {
        const auto& text = theme.text();
        typography_changed = apply_typography(component, {
            text.font_family,
            text.font_weight,
            text.font_size,
            text.line_height,
        });
    }
    if (!state->semantic_foreground) {
        const auto color = state->explicit_tone
            ? tone_color(theme, state->tone)
            : theme.text().color;
        if (text_scene_->set_color(state->scene, channels(color))) {
            dirty_->invalidate(node, runtime::DirtyFlags::Material);
        }
    }
    static_cast<void>(typography_changed);
}

void TextComponentHost::subscribe_theme(runtime::ComponentId component) {
    auto* state = components_.state<TextComponentState>(component);
    if (state == nullptr) {
        return;
    }
    state->theme_subscription.reset();
    const auto theme = components_.theme_scope(component);
    if (state->semantic_foreground && state->semantic_typography) {
        return;
    }
    state->theme_subscription = theme->capture(
        [this, component](theme_runtime::DirtyPhase) {
            apply_theme(component);
        },
        [theme, state] {
            if (!state->semantic_typography) {
                static_cast<void>(theme->text_font_family());
                static_cast<void>(theme->text_font_weight());
                static_cast<void>(theme->text_font_size());
                static_cast<void>(theme->text_line_height());
            }
            if (!state->semantic_foreground) {
                if (state->explicit_tone) {
                    capture_tone_color(theme, state->tone);
                } else {
                    static_cast<void>(theme->text_color());
                }
            }
        });
}

void mount_text_component(const TextProps& props) {
    if (active_text_host == nullptr) {
        throw std::logic_error(
            "ryn::Text can only be declared inside an active TextComponentHost");
    }

    auto& host = *active_text_host;
    auto& build = runtime::require_component_build_context();
    const auto component = build.mount_component<TextComponentState>();
    const auto node = build.root(component);
    host.layout_->set_layout(node, layout::LeafLayout{});

    const auto initial_content = read_prop(TextPropsAccess::content(props));
    const auto& explicit_tone = TextPropsAccess::tone(props);
    const auto& semantic_foreground = build.semantic_foreground();
    const auto& semantic_typography = build.semantic_typography();
    const auto theme_scope = build.theme_scope();
    const auto& snapshot = theme_scope->snapshot();
    const auto initial_typography = semantic_typography.has_value()
        ? read_prop(*semantic_typography)
        : runtime::SemanticTypography{
            snapshot.text().font_family,
            snapshot.text().font_weight,
            snapshot.text().font_size,
            snapshot.text().line_height,
        };
    const auto initial_color = explicit_tone.has_value()
        ? channels(tone_color(snapshot, read_prop(*explicit_tone)))
        : semantic_foreground.has_value()
            ? read_prop(*semantic_foreground)
            : channels(snapshot.text().color);
    auto initial_font_chain = host.font_resolver_(
        initial_typography.font_family,
        initial_typography.font_weight,
        static_cast<std::uint32_t>(std::lround(initial_typography.font_size)));
    if (initial_font_chain.empty()) {
        throw std::runtime_error("Theme font resolver returned an empty chain");
    }
    const auto scene = host.text_scene_->create(
        node,
        initial_content,
        std::move(initial_font_chain),
        static_cast<std::uint32_t>(std::lround(initial_typography.font_size)),
        {
            initial_typography.line_height,
            std::numeric_limits<float>::infinity(),
        });
    const auto fragment = host.composer_ == nullptr
        ? std::optional<runtime::SceneFragmentId>{}
        : std::optional<runtime::SceneFragmentId>{
            build.register_scene_fragment(
                component,
                runtime::SceneFragmentPlacement::before_children)};
    build.state<TextComponentState>(component).scene = scene;
    auto& state = build.state<TextComponentState>(component);
    state.tone = explicit_tone.has_value()
        ? read_prop(*explicit_tone) : TextTone::Primary;
    state.explicit_tone = explicit_tone.has_value();
    state.semantic_foreground = !explicit_tone.has_value()
        && semantic_foreground.has_value();
    state.semantic_typography = semantic_typography.has_value();
    state.resolved_typography = initial_typography;
    build.on_resource_cleanup(component, [
        layout = host.layout_,
        composer = host.composer_,
        text_scene = host.text_scene_,
        node,
        scene,
        fragment] {
        static_cast<void>(layout->remove_intrinsic_measure(node));
        if (composer != nullptr && fragment.has_value()) {
            static_cast<void>(composer->remove_fragment(*fragment));
        }
        static_cast<void>(text_scene->destroy(scene));
    });

    static_cast<void>(host.text_scene_->set_color(
        scene,
        initial_color));
    host.layout_->set_intrinsic_measure(
        node,
        intrinsic_revision(host.text_scene_->revisions(scene)),
        [text_scene = host.text_scene_, scene](layout::Constraints constraints) {
            if (!text_scene->synchronize_measurement(
                    scene, constraints.max_width)) {
                throw std::runtime_error("Text intrinsic measurement failed");
            }
            const auto& measurement = text_scene->text_state(scene).measurement();
            return runtime::Size{measurement.width, measurement.height};
        });

    auto& scope = build.scope(component);
    static_cast<void>(connect_prop(
        scope,
        TextPropsAccess::content(props),
        [
            text_scene = host.text_scene_,
            layout = host.layout_,
            dirty = host.dirty_,
            scene,
            node](String content) {
            if (!text_scene->set_content(scene, std::move(content))) {
                return;
            }
            static_cast<void>(layout->set_intrinsic_revision(
                node,
                intrinsic_revision(text_scene->revisions(scene))));
            dirty->invalidate(
                node,
                runtime::DirtyFlags::Measure
                    | runtime::DirtyFlags::Layout
                    | runtime::DirtyFlags::Geometry);
        }));
    const auto apply_color = [
        text_scene = host.text_scene_,
        dirty = host.dirty_,
        scene,
        node](std::array<float, 4> color) {
        if (text_scene->set_color(scene, color)) {
            dirty->invalidate(node, runtime::DirtyFlags::Material);
        }
    };
    if (explicit_tone.has_value()) {
        static_cast<void>(connect_prop(
            scope,
            *explicit_tone,
            [&host, component, apply_color, theme = theme_scope](TextTone tone) {
                auto* state = host.components_.state<TextComponentState>(component);
                if (state == nullptr) {
                    return;
                }
                state->tone = tone;
                apply_color(channels(tone_color(theme->snapshot(), tone)));
                host.subscribe_theme(component);
            }));
    } else if (semantic_foreground.has_value()) {
        static_cast<void>(connect_prop(
            scope,
            *semantic_foreground,
            apply_color));
    }
    if (semantic_typography.has_value()) {
        static_cast<void>(connect_prop(
            scope,
            *semantic_typography,
            [&host, component](runtime::SemanticTypography typography) {
                static_cast<void>(host.apply_typography(component, typography));
            }));
    }
    host.subscribe_theme(component);
    runtime::connect_layout_style(
        scope,
        TextPropsAccess::layout(props),
        node,
        *host.nodes_,
        *host.dirty_);
    host.dirty_->invalidate(
        node,
        runtime::DirtyFlags::Measure
            | runtime::DirtyFlags::Layout
            | runtime::DirtyFlags::Geometry);
    host.record_mounted_text(component, scene, fragment);
}

} // namespace ryn::detail

namespace ryn {

void Text(TextProps props) {
    detail::mount_text_component(props);
}

} // namespace ryn
