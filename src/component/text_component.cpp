#include "component/text_component.hpp"

#include "component/layout_component_context.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <ryn/icon.hpp>
#include <ryn/typography.hpp>

#include <algorithm>
#include <array>
#include <chrono>
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

struct IconPropsAccess final {
    [[nodiscard]] static const Prop<IconName>& name(const IconProps& props) noexcept {
        return props.name_;
    }
    [[nodiscard]] static const std::optional<Prop<TextTone>>& tone(
        const IconProps& props) noexcept {
        return props.tone_;
    }
    [[nodiscard]] static const Prop<bool>& visible(const IconProps& props) noexcept {
        return props.visible_;
    }
    [[nodiscard]] static const LayoutStyle& layout(const IconProps& props) noexcept {
        return props.layout_;
    }
};

// Semantics a `Title`/`Text`/`Paragraph` builder carries into the shared text
// host. Keeping them separate from `TextTone` lets the theme subscription
// resolve the semantic colour without a second colour path.
struct TypographyPropsAccess final {
    [[nodiscard]] static const Prop<String>& content(
        const TypographyProps& props) noexcept {
        return props.content_;
    }
    [[nodiscard]] static const std::optional<Prop<TypographyType>>& type(
        const TypographyProps& props) noexcept {
        return props.type_;
    }
    [[nodiscard]] static const std::optional<Prop<bool>>& disabled(
        const TypographyProps& props) noexcept {
        return props.disabled_;
    }
    [[nodiscard]] static const std::optional<Prop<bool>>& strong(
        const TypographyProps& props) noexcept {
        return props.strong_;
    }
    [[nodiscard]] static const std::optional<Prop<bool>>& code(
        const TypographyProps& props) noexcept {
        return props.code_;
    }
    [[nodiscard]] static const std::optional<Prop<bool>>& keyboard(
        const TypographyProps& props) noexcept {
        return props.keyboard_;
    }
    [[nodiscard]] static const std::optional<Prop<bool>>& mark(
        const TypographyProps& props) noexcept {
        return props.mark_;
    }
    [[nodiscard]] static const std::optional<Prop<bool>>& italic(
        const TypographyProps& props) noexcept {
        return props.italic_;
    }
    [[nodiscard]] static const std::optional<Prop<bool>>& underline(
        const TypographyProps& props) noexcept {
        return props.underline_;
    }
    [[nodiscard]] static const std::optional<Prop<bool>>& strikethrough(
        const TypographyProps& props) noexcept {
        return props.strikethrough_;
    }
    [[nodiscard]] static const LayoutStyle& layout(
        const TypographyProps& props) noexcept {
        return props.layout_;
    }
    [[nodiscard]] static const TypographyProps& base(
        const TitleProps& props) noexcept {
        return props.typography_;
    }
    [[nodiscard]] static TypographyLevel level(const TitleProps& props) noexcept {
        return props.level_;
    }
};

[[nodiscard]] String icon_content(IconName name) {
    const auto index = static_cast<std::uint32_t>(name);
    if (index > static_cast<std::uint32_t>(last_bundled_icon)) {
        throw std::invalid_argument("Icon name is outside the bundled catalog");
    }
    const char32_t codepoint = 0xE000 + index;
    const std::array<char, 3> bytes{
        static_cast<char>(0xE0 | (codepoint >> 12)),
        static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)),
        static_cast<char>(0x80 | (codepoint & 0x3F)),
    };
    return std::move(String::from_utf8(
        std::string_view(bytes.data(), bytes.size()))).value();
}

namespace {

struct TextComponentState final {
    TextSceneId scene;
    TextTone tone{TextTone::Primary};
    bool explicit_tone{};
    bool semantic_foreground{};
    bool semantic_typography{};
    bool icon_font{};
    // Typography builder state. `semantic` is present only for components
    // declared through `Title`/`Text`/`Paragraph`; the plain `Text`/`Icon`
    // builders leave it empty so their existing behaviour is unchanged.
    std::optional<TypographySemantics> typography;
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

// Semantic `TypographyType` colours come from the Typography Component Token
// group, which is separate from the `TextTone` alias colours.
[[nodiscard]] Color typography_color(
    const ThemeSnapshot& theme,
    TypographyType type) noexcept {
    switch (type) {
    case TypographyType::Default:
        return theme.typography().colors.text;
    case TypographyType::Secondary:
        return theme.typography().colors.description;
    case TypographyType::Success:
        return theme.typography().colors.success;
    case TypographyType::Warning:
        return theme.typography().colors.warning;
    case TypographyType::Danger:
        return theme.typography().colors.error;
    }
    return theme.typography().colors.text;
}

// A semantic typography component resolves its foreground through `TextTone` so
// the existing colour path and theme subscription keep working unchanged.
[[nodiscard]] TextTone semantic_tone(const TypographySemantics& semantics) noexcept {
    if (semantics.disabled) {
        return TextTone::Disabled;
    }
    if (semantics.type == TypographyType::Default) {
        return TextTone::Primary;
    }
    if (semantics.type == TypographyType::Secondary) {
        return TextTone::Secondary;
    }
    // success/warning/danger carry their own colour, so `tone_color` is bypassed
    // and `typography_color` answers instead.
    return TextTone::Primary;
}

[[nodiscard]] bool tone_uses_typography_color(
    const TypographySemantics& semantics) noexcept {
    return !semantics.disabled && semantics.type != TypographyType::Default
        && semantics.type != TypographyType::Secondary;
}

[[nodiscard]] Color semantic_color(
    const ThemeSnapshot& theme,
    const TextComponentState& state) noexcept {
    if (state.typography.has_value() && tone_uses_typography_color(*state.typography)) {
        return typography_color(theme, state.typography->type);
    }
    if (state.typography.has_value()) {
        return state.typography->type == TypographyType::Secondary
            ? theme.typography().colors.description
            : (state.typography->disabled
                ? theme.typography().colors.disabled
                : theme.typography().colors.text);
    }
    return state.explicit_tone ? tone_color(theme, state.tone) : theme.text().color;
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

// Records exactly the token reads that decide the resolved foreground, so the
// theme subscription fires for that colour and nothing else.
void capture_semantic_color(
    const std::shared_ptr<theme_runtime::ThemeScope>& theme,
    const TextComponentState& state) {
    if (state.typography.has_value()) {
        switch (state.typography->type) {
        case TypographyType::Secondary:
            static_cast<void>(theme->typography_colors());
            return;
        case TypographyType::Success:
        case TypographyType::Warning:
        case TypographyType::Danger:
            if (!state.typography->disabled) {
                static_cast<void>(theme->typography_colors());
                return;
            }
            break;
        case TypographyType::Default:
            break;
        }
        if (state.typography->disabled) {
            static_cast<void>(theme->text_disabled_color());
            return;
        }
        static_cast<void>(theme->typography_colors());
        return;
    }
    if (state.explicit_tone) {
        capture_tone_color(theme, state.tone);
    } else {
        static_cast<void>(theme->text_color());
    }
}

// Resolves the shape a semantic component should render with. Heading levels use
// the per-level tokens; body and paragraph use the base tokens with the
// emphasis variants applied. `code`/`keyboard` switch to the code font family
// and scale by the inline token, which is what makes an inline run monospace at
// a smaller size than its surrounding text.
[[nodiscard]] runtime::SemanticTypography resolve_semantic_typography(
    const ThemeSnapshot& theme,
    const TypographySemantics& semantics) {
    const auto& typography = theme.typography();
    runtime::SemanticTypography resolved;
    resolved.font_family = (semantics.code || semantics.keyboard)
        ? typography.font_family_code
        : typography.font_family;
    resolved.font_weight = semantics.strong
        ? typography.font_weight_strong : typography.font_weight;
    resolved.italic = semantics.italic;
    if (semantics.role == TypographySemantics::Role::heading) {
        const auto& heading = typography.heading(semantics.level);
        resolved.font_size = heading.font_size;
        resolved.line_height = heading.line_height;
    } else {
        resolved.font_size = typography.base_font_size;
        resolved.line_height = typography.base_line_height;
    }
    // Inline code and keyboard render at a fraction of the surrounding size; the
    // line box keeps the surrounding line height so an inline run does not
    // disturb the paragraph rhythm.
    if (semantics.code) {
        resolved.font_size *= typography.code.font_scale;
    } else if (semantics.keyboard) {
        resolved.font_size *= typography.keyboard.font_scale;
    }
    return resolved;
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
              bool,
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
    if (font_resolver_(SystemFontFamily::ui_sans, 400, false, 14).empty()) {
        throw std::invalid_argument(
            "TextComponentHost Theme font resolver returned an empty default chain");
    }
}

TextComponentHost::~TextComponentHost() {
    dispose();
}

std::vector<font::FontIdentity> TextComponentHost::resolve_fonts(
    const runtime::SemanticTypography& typography) const {
    auto chain = font_resolver_(
        typography.font_family,
        typography.font_weight,
        typography.italic,
        static_cast<std::uint32_t>(std::lround(typography.font_size)));
    if(chain.empty()) throw std::runtime_error("Theme font resolver returned an empty chain");
    return chain;
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
        const auto layout_started = sync_profiling_enabled_
            ? std::chrono::steady_clock::now()
            : std::chrono::steady_clock::time_point{};
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
        if (sync_profiling_enabled_) {
            ++sync_profile_.layout_calls;
            sync_profile_.layout_nanoseconds += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - layout_started).count());
        }
    }

    const auto loop_started = sync_profiling_enabled_
        ? std::chrono::steady_clock::now()
        : std::chrono::steady_clock::time_point{};
    const auto synchronize_mounted = [&]() {
        for (const auto& mounted : mounted_texts_) {
            if (sync_profiling_enabled_) ++sync_profile_.mounted_visited;
            if (!components_.contains(mounted.component)
                    || !text_scene_->contains(mounted.scene)) {
                continue;
            }
            const auto node = components_.root(mounted.component);
            const auto& retained = nodes_->require(node);
            constexpr float visual_overflow = 32.0F;
            const float left = retained.bounds.x + retained.translation.x;
            const float top = retained.bounds.y + retained.translation.y;
            if (retained.bounds.width <= 0.0F
                    || retained.bounds.height <= 0.0F
                    || left >= clip.x + clip.width + visual_overflow
                    || left + retained.bounds.width <= clip.x - visual_overflow
                    || top >= clip.y + clip.height + visual_overflow
                    || top + retained.bounds.height <= clip.y - visual_overflow) {
                if (sync_profiling_enabled_) ++sync_profile_.offscreen_skipped;
                continue;
            }
            const auto phase_residual = text_scene_->set_phase_preserving_scroll_translation(
                mounted.scene, retained.translation);
            const auto sync_started = sync_profiling_enabled_
                ? std::chrono::steady_clock::now()
                : std::chrono::steady_clock::time_point{};
            const bool synchronized = text_scene_->synchronize(mounted.scene, {
                    {retained.bounds.x, retained.bounds.y},
                    viewport,
                    clip,
                    phase_residual,
                    {},
                    1.0F,
                });
            if (sync_profiling_enabled_) {
                ++sync_profile_.mounted_synchronized;
                sync_profile_.text_scene_nanoseconds += static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - sync_started).count());
            }
            if (!synchronized) {
                return false;
            }
        }
        return true;
    };
    text_scene_->begin_ordered_scene_batch();
    try {
        const bool synchronized = synchronize_mounted();
        text_scene_->finish_ordered_scene_batch();
        if (!synchronized) {
            return false;
        }
    } catch (...) {
        text_scene_->cancel_ordered_scene_batch();
        throw;
    }
    if (sync_profiling_enabled_) {
        sync_profile_.mounted_loop_nanoseconds += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - loop_started).count());
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
        const auto& primitive = text_scene_->primitive(mounted.scene);
        const auto interaction = interaction_for(mounted.component);
        bool same = interaction == mounted.interaction
            && primitive.draw_ranges.size() == mounted.fragment_commands.size();
        for(std::size_t index = 0; same && index < primitive.draw_ranges.size(); ++index) {
            const auto& range = primitive.draw_ranges[index];
            same = mounted.fragment_commands[index] == graphics::SceneDrawCommand{
                graphics::SceneDrawKind::glyph, range.instances.first,
                range.instances.count, range.atlas_page};
        }
        if(same) continue;
        std::vector<graphics::SceneDrawCommand> commands;
        commands.reserve(primitive.draw_ranges.size());
        for (const auto& range : primitive.draw_ranges) {
            commands.push_back({
                graphics::SceneDrawKind::glyph,
                range.instances.first,
                range.instances.count,
                range.atlas_page,
            });
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
            || font_resolver(SystemFontFamily::ui_sans, 400, false, 14).empty()) {
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
        typography.italic,
        static_cast<std::uint32_t>(std::lround(typography.font_size)));
        if (chain.empty()) {
            throw std::runtime_error("Theme font resolver returned an empty chain");
        }
        if (state->icon_font) {
            chain = {text_scene_->icon_font(
                chain.front(), static_cast<std::uint32_t>(std::lround(typography.font_size)))};
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
        typography.italic,
        static_cast<std::uint32_t>(std::lround(typography.font_size)));
    if (chain.empty()) {
        throw std::runtime_error("Theme font resolver returned an empty chain");
    }
    if (state->icon_font) {
        chain = {text_scene_->icon_font(
            chain.front(), static_cast<std::uint32_t>(std::lround(typography.font_size)))};
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
    if (state->typography.has_value()) {
        // A semantic component always re-resolves its shape from the Typography
        // Component Token group, so a theme update scales headings and applies
        // the emphasis variants without a remount.
        typography_changed = apply_typography(component,
            resolve_semantic_typography(theme, *state->typography));
    } else if (!state->semantic_typography) {
        const auto& text = theme.text();
        typography_changed = apply_typography(component, {
            text.font_family,
            text.font_weight,
            false,
            text.font_size,
            text.line_height,
        });
    }
    if (state->typography.has_value()) {
        if (text_scene_->set_color(state->scene, channels(semantic_color(theme, *state)))) {
            dirty_->invalidate(node, runtime::DirtyFlags::Material);
        }
    } else if (!state->semantic_foreground) {
        const auto color = state->explicit_tone
            ? tone_color(theme, state->tone)
            : theme.text().color;
        if (text_scene_->set_color(state->scene, channels(color))) {
            dirty_->invalidate(node, runtime::DirtyFlags::Material);
        }
    }
}

void TextComponentHost::subscribe_theme(runtime::ComponentId component) {
    auto* state = components_.state<TextComponentState>(component);
    if (state == nullptr) {
        return;
    }
    state->theme_subscription.reset();
    const auto theme = components_.theme_scope(component);
    const bool semantic = state->typography.has_value();
    if (!semantic && state->semantic_foreground && state->semantic_typography) {
        return;
    }
    state->theme_subscription = theme->capture(
        [this, component](theme_runtime::DirtyPhase) {
            apply_theme(component);
        },
        [theme, state, semantic] {
            if (semantic) {
                // A semantic component resolves size and weight from the
                // Typography Component Token group. The inline token group is a
                // separate identity, so a change to the code or keyboard scale
                // only reaches a component that captured it here.
                static_cast<void>(theme->typography_headings());
                static_cast<void>(theme->typography_fonts());
                static_cast<void>(theme->typography_base_typography());
                static_cast<void>(theme->typography_inline_code());
                static_cast<void>(theme->typography_inline_keyboard());
            } else if (!state->semantic_typography) {
                // The plain `Text` builder keeps deriving its shape from the Text
                // token group, so those identities must stay captured.
                static_cast<void>(theme->text_font_family());
                static_cast<void>(theme->text_font_weight());
                static_cast<void>(theme->text_font_size());
                static_cast<void>(theme->text_line_height());
            }
            if (semantic || !state->semantic_foreground) {
                capture_semantic_color(theme, *state);
            }
        });
}

void mount_text_component(const TextProps& props, bool icon_font) {
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
            false,
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
        initial_typography.italic,
        static_cast<std::uint32_t>(std::lround(initial_typography.font_size)));
    if (initial_font_chain.empty()) {
        throw std::runtime_error("Theme font resolver returned an empty chain");
    }
    if (icon_font) {
        initial_font_chain = {host.text_scene_->icon_font(initial_font_chain.front(),
            static_cast<std::uint32_t>(std::lround(initial_typography.font_size)))};
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
    state.icon_font = icon_font;
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

// Semantic `Title`/`Text`/`Paragraph` mount into the same host as plain `Text`
// so font-chain resolution, scene lifetime, theme subscription and dirty
// invalidation are shared instead of duplicated.
void mount_typography_component(
    const TypographyProps& props,
    TypographySemantics::Role role,
    TypographyLevel level) {
    if (active_text_host == nullptr) {
        throw std::logic_error(
            "Typography components can only be declared inside an active "
            "TextComponentHost");
    }

    auto& host = *active_text_host;
    auto& build = runtime::require_component_build_context();
    const auto component = build.mount_component<TextComponentState>();
    const auto node = build.root(component);
    host.layout_->set_layout(node, layout::LeafLayout{});

    const auto theme_scope = build.theme_scope();
    auto& state = build.state<TextComponentState>(component);
    state.typography = TypographySemantics{.role = role, .level = level};
    const auto content = read_prop(TypographyPropsAccess::content(props));
    // Resolve once and reuse: `create` must already carry the semantic font size
    // and line height, otherwise the first frame would rasterize at a wrong
    // pixel size before the theme application runs.
    const auto initial_typography =
        resolve_semantic_typography(theme_scope->snapshot(), *state.typography);
    const auto chain = host.resolve_fonts(initial_typography);
    const auto scene = host.text_scene_->create(
        node,
        content,
        chain,
        static_cast<std::uint32_t>(std::lround(initial_typography.font_size)),
        {
            initial_typography.line_height,
            std::numeric_limits<float>::infinity(),
        });
    state.scene = scene;
    state.resolved_typography = initial_typography;
    state.explicit_tone = true;
    state.tone = semantic_tone(*state.typography);
    const auto fragment = host.composer_ == nullptr
        ? std::optional<runtime::SceneFragmentId>{}
        : std::optional<runtime::SceneFragmentId>{
            build.register_scene_fragment(
                component,
                runtime::SceneFragmentPlacement::before_children)};
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
    host.layout_->set_intrinsic_measure(
        node,
        intrinsic_revision(host.text_scene_->revisions(scene)),
        [text_scene = host.text_scene_, scene](layout::Constraints constraints) {
            if (!text_scene->synchronize_measurement(
                    scene, constraints.max_width)) {
                throw std::runtime_error("Typography intrinsic measurement failed");
            }
            const auto& measurement = text_scene->text_state(scene).measurement();
            return runtime::Size{measurement.width, measurement.height};
        });

    auto& scope = build.scope(component);
    // Content and layout first, then the initial theme application, so the first
    // frame already carries the semantic colour and the resolved face.
    static_cast<void>(connect_prop(
        scope,
        TypographyPropsAccess::content(props),
        [
            text_scene = host.text_scene_,
            layout = host.layout_,
            dirty = host.dirty_,
            scene,
            node](String value) {
            if (!text_scene->set_content(scene, std::move(value))) {
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
    runtime::connect_layout_style(
        scope,
        TypographyPropsAccess::layout(props),
        node,
        *host.nodes_,
        *host.dirty_);

    const auto update_semantics = [&host, component](auto apply) {
        auto* current = host.components_.state<TextComponentState>(component);
        if (current == nullptr || !current->typography.has_value()) {
            return;
        }
        apply(*current->typography);
        host.subscribe_theme(component);
        host.apply_theme(component);
    };
    const auto connect_semantic = [&](
            const std::optional<Prop<bool>>& prop,
            bool TypographySemantics::*member) {
        if (!prop.has_value()) {
            return;
        }
        static_cast<void>(connect_prop(scope, *prop,
            [update_semantics, member](bool value) {
                update_semantics([member, value](TypographySemantics& semantics) {
                    semantics.*member = value;
                });
            }));
    };
    connect_semantic(TypographyPropsAccess::strong(props),
        &TypographySemantics::strong);
    connect_semantic(TypographyPropsAccess::italic(props),
        &TypographySemantics::italic);
    connect_semantic(TypographyPropsAccess::code(props),
        &TypographySemantics::code);
    connect_semantic(TypographyPropsAccess::keyboard(props),
        &TypographySemantics::keyboard);
    connect_semantic(TypographyPropsAccess::mark(props),
        &TypographySemantics::mark);
    connect_semantic(TypographyPropsAccess::underline(props),
        &TypographySemantics::underline);
    connect_semantic(TypographyPropsAccess::strikethrough(props),
        &TypographySemantics::strikethrough);
    connect_semantic(TypographyPropsAccess::disabled(props),
        &TypographySemantics::disabled);
    if (const auto& type = TypographyPropsAccess::type(props); type.has_value()) {
        static_cast<void>(connect_prop(scope, *type,
            [update_semantics](TypographyType value) {
                update_semantics([value](TypographySemantics& semantics) {
                    semantics.type = value;
                });
            }));
    }

    // Apply the initial shape and colour, then keep both in sync with the theme.
    static_cast<void>(host.apply_typography(component, initial_typography));
    static_cast<void>(host.text_scene_->set_color(scene,
        channels(semantic_color(theme_scope->snapshot(), state))));
    host.subscribe_theme(component);
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
    detail::mount_text_component(props, false);
}

void Icon(IconProps props) {
    TextProps text;
    const auto name = detail::IconPropsAccess::name(props);
    const auto visible = detail::IconPropsAccess::visible(props);
    text.content(bind([name, visible] {
        return detail::read_prop(visible)
            ? detail::icon_content(detail::read_prop(name)) : String{};
    }));
    if (const auto& tone = detail::IconPropsAccess::tone(props)) text.tone(*tone);
    text.layout(detail::IconPropsAccess::layout(props));
    detail::mount_text_component(text, true);
}

void Title(TitleProps props) {
    detail::mount_typography_component(
        detail::TypographyPropsAccess::base(props),
        detail::TypographySemantics::Role::heading,
        detail::TypographyPropsAccess::level(props));
}

void Text(TypographyProps props) {
    detail::mount_typography_component(props,
        detail::TypographySemantics::Role::body, TypographyLevel::H1);
}

void Paragraph(TypographyProps props) {
    detail::mount_typography_component(props,
        detail::TypographySemantics::Role::paragraph, TypographyLevel::H1);
}

} // namespace ryn
