#pragma once

#include "component/component_scene.hpp"
#include "component/retained_surface_service.hpp"
#include "input/interaction_registry.hpp"
#include "layout/layout_engine.hpp"
#include "runtime/component_host.hpp"
#include "runtime/invalidation.hpp"
#include "text/text_scene_service.hpp"

#include <ryn/text.hpp>
#include <ryn/typography.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace ryn::detail {

using ThemeFontResolver =
    std::function<std::vector<font::FontIdentity>(SystemFontFamily, std::uint32_t, bool, std::uint32_t)>;

struct MountedTextComponent final {
    runtime::ComponentId component;
    TextSceneId scene;
    std::optional<runtime::SceneFragmentId> fragment;
    std::optional<input::InteractionId> interaction;
    std::vector<graphics::SceneDrawCommand> fragment_commands;
};

// Semantics a `Title`/`Text`/`Paragraph` builder carries into the shared text
// host. Declared here so the host can friend the mount entry point.
struct TypographySemantics final {
    enum class Role : std::uint8_t { body, heading, paragraph };

    Role role{Role::body};
    TypographyType type{TypographyType::Default};
    TypographyLevel level{TypographyLevel::H1};
    bool disabled{};
    bool strong{};
    bool italic{};
    bool code{};
    bool keyboard{};
    bool mark{};
    bool underline{};
    bool strikethrough{};

    friend constexpr bool operator==(TypographySemantics, TypographySemantics) = default;
};

struct TextComponentSyncProfile final {
    std::uint64_t layout_nanoseconds{};
    std::uint64_t mounted_loop_nanoseconds{};
    std::uint64_t text_scene_nanoseconds{};
    std::uint64_t layout_calls{};
    std::uint64_t mounted_visited{};
    std::uint64_t mounted_synchronized{};
    std::uint64_t offscreen_skipped{};
};

class TextComponentHost final {
public:
    TextComponentHost(runtime::NodeStore& nodes, layout::LayoutEngine& layout, runtime::DirtyQueues& dirty,
                      TextSceneService& text_scene, std::vector<font::FontIdentity> default_font_chain);
    TextComponentHost(runtime::NodeStore& nodes, layout::LayoutEngine& layout, runtime::DirtyQueues& dirty,
                      TextSceneService& text_scene, ThemeFontResolver font_resolver);
    TextComponentHost(const TextComponentHost&) = delete;
    TextComponentHost& operator=(const TextComponentHost&) = delete;
    ~TextComponentHost();

    void mount(const Content& content);
    bool destroy(runtime::ComponentId id);
    void dispose() noexcept;

    [[nodiscard]] bool layout_and_synchronize(runtime::Size viewport, runtime::Rect clip, runtime::Point origin = {},
                                              float gap = 0.0F, bool clear_dirty = true,
                                              bool unbounded_root_height = false);
    void attach_component_scene(component::ComponentSceneComposer& composer) noexcept;
    void attach_surfaces(component::RetainedSurfaceService& surfaces) noexcept;
    bool set_font_resolver(ThemeFontResolver font_resolver);
    [[nodiscard]] bool synchronize_scene_fragments(
        const std::function<std::optional<input::InteractionId>(runtime::ComponentId)>& interaction_for);
    [[nodiscard]] bool layout_performed_last_sync() const noexcept;

    void set_sync_profiling_enabled(bool enabled) noexcept {
        sync_profiling_enabled_ = enabled;
    }

    void reset_sync_profile() noexcept {
        sync_profile_ = {};
    }

    [[nodiscard]] TextComponentSyncProfile sync_profile() const noexcept {
        return sync_profile_;
    }

    [[nodiscard]] runtime::ComponentHost& components() noexcept;
    [[nodiscard]] const runtime::ComponentHost& components() const noexcept;
    [[nodiscard]] TextSceneService& scene_service() noexcept;
    [[nodiscard]] const TextSceneService& scene_service() const noexcept;
    [[nodiscard]] std::span<const MountedTextComponent> mounted_texts() const noexcept;
    [[nodiscard]] std::vector<font::FontIdentity> resolve_fonts(const runtime::SemanticTypography& typography) const;
    [[nodiscard]] runtime::SemanticTypography resolved_typography(runtime::ComponentId component) const;
    void reserve_ellipsis_inline(runtime::ComponentId component, float width);

private:
    friend void mount_text_component(const TextProps& props, bool icon_font);
    friend void mount_typography_component(const TypographyProps& props, TypographySemantics::Role role,
                                           const Prop<TypographyLevel>& level);

    void record_mounted_text(runtime::ComponentId component, TextSceneId scene,
                             std::optional<runtime::SceneFragmentId> fragment);
    bool apply_typography(runtime::ComponentId component, runtime::SemanticTypography typography);
    void apply_theme(runtime::ComponentId component);
    void subscribe_theme(runtime::ComponentId component);
    void synchronize_decorations(runtime::ComponentId component, runtime::Size viewport, runtime::Rect clip);

    runtime::NodeStore* nodes_;
    layout::LayoutEngine* layout_;
    runtime::DirtyQueues* dirty_;
    TextSceneService* text_scene_;
    ThemeFontResolver font_resolver_;
    component::ComponentSceneComposer* composer_{nullptr};
    component::RetainedSurfaceService* surfaces_{nullptr};
    runtime::ComponentHost components_;
    std::vector<MountedTextComponent> mounted_texts_;
    runtime::Size layout_viewport_;
    runtime::Point layout_origin_;
    float layout_gap_{0.0F};
    bool layout_unbounded_root_height_{false};
    bool layout_snapshot_valid_{false};
    bool layout_performed_last_sync_{false};
    bool sync_profiling_enabled_{};
    TextComponentSyncProfile sync_profile_{};
};

void mount_text_component(const TextProps& props, bool icon_font = false);
void mount_typography_component(const TypographyProps& props, TypographySemantics::Role role,
                                const Prop<TypographyLevel>& level);

} // namespace ryn::detail
