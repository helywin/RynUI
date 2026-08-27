#include "component/text_component.hpp"

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

    [[nodiscard]] static const Prop<TextTone>& tone(
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

[[nodiscard]] const std::array<float, 4>& tone_color(
    const DefaultThemeSnapshot& theme,
    TextTone tone) noexcept {
    switch (tone) {
    case TextTone::Primary:
        return theme.text.primary;
    case TextTone::Secondary:
        return theme.text.secondary;
    case TextTone::Disabled:
        return theme.text.disabled;
    }
    return theme.text.primary;
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
    std::vector<font::FontIdentity> default_font_chain,
    const DefaultThemeSnapshot& theme)
    : nodes_(&nodes),
      layout_(&layout),
      dirty_(&dirty),
      text_scene_(&text_scene),
      default_font_chain_(std::move(default_font_chain)),
      theme_(&theme),
      components_(nodes) {
    if (default_font_chain_.empty()) {
        throw std::invalid_argument(
            "TextComponentHost requires a resolved Default Theme font chain");
    }
}

TextComponentHost::~TextComponentHost() {
    dispose();
}

void TextComponentHost::mount(const Content& content) {
    ActiveTextHostGuard guard(*this);
    const auto mounted_before = mounted_texts_.size();
    try {
        components_.mount(content);
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
    return true;
}

void TextComponentHost::dispose() noexcept {
    components_.dispose();
    mounted_texts_.clear();
}

bool TextComponentHost::layout_and_synchronize(
    runtime::Size viewport,
    runtime::Rect clip,
    runtime::Point origin,
    float gap) {
    if (!valid_viewport(viewport)
            || !std::isfinite(origin.x)
            || !std::isfinite(origin.y)
            || !std::isfinite(gap)
            || gap < 0.0F) {
        throw std::invalid_argument(
            "Text component viewport, origin, and gap must be finite and valid");
    }

    float cursor_y = origin.y;
    for (const auto mounted : mounted_texts_) {
        if (!components_.contains(mounted.component)
                || !text_scene_->contains(mounted.scene)) {
            continue;
        }
        const auto node = components_.root(mounted.component);
        const auto remaining_width = std::max(0.0F, viewport.width - origin.x);
        const auto remaining_height = std::max(0.0F, viewport.height - cursor_y);
        const auto outer = layout_->layout(
            node,
            {0.0F, remaining_width, 0.0F, remaining_height},
            {origin.x, cursor_y});
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
        cursor_y += outer.height + gap;
    }
    dirty_->clear();
    return true;
}

runtime::ComponentHost& TextComponentHost::components() noexcept {
    return components_;
}

const runtime::ComponentHost& TextComponentHost::components() const noexcept {
    return components_;
}

std::span<const MountedTextComponent>
TextComponentHost::mounted_texts() const noexcept {
    return mounted_texts_;
}

const DefaultThemeSnapshot& TextComponentHost::theme() const noexcept {
    return *theme_;
}

void TextComponentHost::record_mounted_text(
    runtime::ComponentId component,
    TextSceneId scene) {
    mounted_texts_.push_back({component, scene});
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
    const auto initial_tone = read_prop(TextPropsAccess::tone(props));
    const auto scene = host.text_scene_->create(
        node,
        initial_content,
        host.default_font_chain_,
        host.theme_->body.logical_pixel_size,
        {
            host.theme_->body.line_height,
            std::numeric_limits<float>::infinity(),
        });
    build.state<TextComponentState>(component).scene = scene;
    build.on_resource_cleanup(component, [
        layout = host.layout_,
        text_scene = host.text_scene_,
        node,
        scene] {
        static_cast<void>(layout->remove_intrinsic_measure(node));
        static_cast<void>(text_scene->destroy(scene));
    });

    static_cast<void>(host.text_scene_->set_color(
        scene,
        tone_color(*host.theme_, initial_tone)));
    host.layout_->set_intrinsic_measure(
        node,
        host.text_scene_->revisions(scene).content,
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
                text_scene->revisions(scene).content));
            dirty->invalidate(
                node,
                runtime::DirtyFlags::Measure
                    | runtime::DirtyFlags::Layout
                    | runtime::DirtyFlags::Geometry);
        }));
    static_cast<void>(connect_prop(
        scope,
        TextPropsAccess::tone(props),
        [
            text_scene = host.text_scene_,
            dirty = host.dirty_,
            theme = host.theme_,
            scene,
            node](TextTone tone) {
            if (text_scene->set_color(scene, tone_color(*theme, tone))) {
                dirty->invalidate(node, runtime::DirtyFlags::Material);
            }
        }));
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
    host.record_mounted_text(component, scene);
}

} // namespace ryn::detail

namespace ryn {

void Text(TextProps props) {
    detail::mount_text_component(props);
}

} // namespace ryn
