#pragma once

#include "component/component_scene.hpp"
#include "input/interaction_registry.hpp"
#include "layout/layout_engine.hpp"
#include "runtime/component_host.hpp"
#include "runtime/invalidation.hpp"
#include "text/text_scene_service.hpp"

#include <ryn/text.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace ryn::detail {

using ThemeFontResolver = std::function<std::vector<font::FontIdentity>(
    SystemFontFamily,
    std::uint32_t,
    std::uint32_t)>;

struct MountedTextComponent final {
    runtime::ComponentId component;
    TextSceneId scene;
    std::optional<runtime::SceneFragmentId> fragment;
    std::optional<input::InteractionId> interaction;
    std::vector<graphics::SceneDrawCommand> fragment_commands;
};

class TextComponentHost final {
public:
    TextComponentHost(
        runtime::NodeStore& nodes,
        layout::LayoutEngine& layout,
        runtime::DirtyQueues& dirty,
        TextSceneService& text_scene,
        std::vector<font::FontIdentity> default_font_chain);
    TextComponentHost(
        runtime::NodeStore& nodes,
        layout::LayoutEngine& layout,
        runtime::DirtyQueues& dirty,
        TextSceneService& text_scene,
        ThemeFontResolver font_resolver);
    TextComponentHost(const TextComponentHost&) = delete;
    TextComponentHost& operator=(const TextComponentHost&) = delete;
    ~TextComponentHost();

    void mount(const Content& content);
    bool destroy(runtime::ComponentId id);
    void dispose() noexcept;

    [[nodiscard]] bool layout_and_synchronize(
        runtime::Size viewport,
        runtime::Rect clip,
        runtime::Point origin = {},
        float gap = 0.0F,
        bool clear_dirty = true);
    void attach_component_scene(component::ComponentSceneComposer& composer) noexcept;
    [[nodiscard]] bool synchronize_scene_fragments(
        const std::function<std::optional<input::InteractionId>(
            runtime::ComponentId)>& interaction_for);
    [[nodiscard]] bool layout_performed_last_sync() const noexcept;

    [[nodiscard]] runtime::ComponentHost& components() noexcept;
    [[nodiscard]] const runtime::ComponentHost& components() const noexcept;
    [[nodiscard]] std::span<const MountedTextComponent> mounted_texts() const noexcept;

private:
    friend void mount_text_component(const TextProps& props);

    void record_mounted_text(
        runtime::ComponentId component,
        TextSceneId scene,
        std::optional<runtime::SceneFragmentId> fragment);
    bool apply_typography(
        runtime::ComponentId component,
        runtime::SemanticTypography typography);
    void apply_theme(runtime::ComponentId component);
    void subscribe_theme(runtime::ComponentId component);

    runtime::NodeStore* nodes_;
    layout::LayoutEngine* layout_;
    runtime::DirtyQueues* dirty_;
    TextSceneService* text_scene_;
    ThemeFontResolver font_resolver_;
    component::ComponentSceneComposer* composer_{nullptr};
    runtime::ComponentHost components_;
    std::vector<MountedTextComponent> mounted_texts_;
    runtime::Size layout_viewport_;
    runtime::Point layout_origin_;
    float layout_gap_{0.0F};
    bool layout_snapshot_valid_{false};
    bool layout_performed_last_sync_{false};
};

void mount_text_component(const TextProps& props);

} // namespace ryn::detail
