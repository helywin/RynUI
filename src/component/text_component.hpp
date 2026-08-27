#pragma once

#include "component/default_theme.hpp"
#include "layout/layout_engine.hpp"
#include "runtime/component_host.hpp"
#include "runtime/invalidation.hpp"
#include "text/text_scene_service.hpp"

#include <ryn/text.hpp>

#include <span>
#include <vector>

namespace ryn::detail {

struct MountedTextComponent final {
    runtime::ComponentId component;
    TextSceneId scene;

    friend constexpr bool operator==(
        MountedTextComponent,
        MountedTextComponent) = default;
};

class TextComponentHost final {
public:
    TextComponentHost(
        runtime::NodeStore& nodes,
        layout::LayoutEngine& layout,
        runtime::DirtyQueues& dirty,
        TextSceneService& text_scene,
        std::vector<font::FontIdentity> default_font_chain,
        const DefaultThemeSnapshot& theme = default_theme_snapshot());
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
        float gap = 0.0F);

    [[nodiscard]] runtime::ComponentHost& components() noexcept;
    [[nodiscard]] const runtime::ComponentHost& components() const noexcept;
    [[nodiscard]] std::span<const MountedTextComponent> mounted_texts() const noexcept;
    [[nodiscard]] const DefaultThemeSnapshot& theme() const noexcept;

private:
    friend void mount_text_component(const TextProps& props);

    void record_mounted_text(
        runtime::ComponentId component,
        TextSceneId scene);

    runtime::NodeStore* nodes_;
    layout::LayoutEngine* layout_;
    runtime::DirtyQueues* dirty_;
    TextSceneService* text_scene_;
    std::vector<font::FontIdentity> default_font_chain_;
    const DefaultThemeSnapshot* theme_;
    runtime::ComponentHost components_;
    std::vector<MountedTextComponent> mounted_texts_;
};

void mount_text_component(const TextProps& props);

} // namespace ryn::detail
