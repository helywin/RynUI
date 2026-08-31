#include "reference_surface.hpp"

#include "font/font_runtime.hpp"
#include "runtime/invalidation.hpp"
#include "text/text_engine.hpp"
#include "text/text_scene_service.hpp"

#include <ryn/rynui.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float actual, float expected, float tolerance = 0.001F) {
    return std::fabs(actual - expected) <= tolerance;
}

std::array<float, 4> channels(ryn::Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

struct Fixture final {
    Fixture()
        : layout(nodes),
          dirty(nodes, &frames),
          fonts(create_runtime()),
          engine(*fonts),
          text_scene(*fonts, engine, frames) {
        const auto latin = fonts->load_font_file(
            RYNUI_VALIDATION_LATIN_FONT, 0, 14);
        const auto cjk = fonts->load_font_file(
            RYNUI_VALIDATION_CJK_FONT, 0, 14);
        require(latin && cjk, "ReferenceSurface fonts failed to load");
        chain = {latin.font, cjk.font};
        chains.emplace(14, chain);
        application = std::make_unique<ryn::detail::ButtonComponentHost>(
            nodes,
            layout,
            dirty,
            text_scene,
            [this](ryn::SystemFontFamily, std::uint32_t, std::uint32_t pixel_size) {
                return resolve_fonts(pixel_size);
            },
            frames);
        surfaces = std::make_unique<rynui::example::ReferenceSurfaceHost>(
            *application);
    }

    static std::unique_ptr<ryn::font::FontRuntime> create_runtime() {
        auto created = ryn::font::FontRuntime::create();
        require(static_cast<bool>(created),
                "ReferenceSurface Font Runtime initialization failed");
        return std::move(created.runtime);
    }

    std::vector<ryn::font::FontIdentity> resolve_fonts(
        std::uint32_t pixel_size) {
        if (const auto found = chains.find(pixel_size); found != chains.end()) {
            return found->second;
        }
        const auto latin = fonts->load_font_file(
            RYNUI_VALIDATION_LATIN_FONT, 0, pixel_size);
        const auto cjk = fonts->load_font_file(
            RYNUI_VALIDATION_CJK_FONT, 0, pixel_size);
        if (!latin || !cjk) {
            return {};
        }
        auto resolved = std::vector<ryn::font::FontIdentity>{latin.font, cjk.font};
        chains.emplace(pixel_size, resolved);
        return resolved;
    }

    bool synchronize(
        ryn::runtime::Rect clip = {5.0F, 6.0F, 500.0F, 300.0F}) {
        return surfaces->layout_and_synchronize(
            {640.0F, 360.0F}, clip, {20.0F, 20.0F}, 8.0F);
    }

    const ryn::graphics::QuadInstance& layer(
        const rynui::example::MountedReferenceSurface& mounted,
        rynui::example::ReferenceSurfaceVisualLayer layer) const {
        const auto range = application->button_scene().visual_range(mounted.scene);
        return application->button_scene().instances().at(
            range.first + static_cast<std::uint32_t>(layer));
    }

    ryn::runtime::NodeStore nodes;
    ryn::layout::LayoutEngine layout;
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::DirtyQueues dirty;
    std::unique_ptr<ryn::font::FontRuntime> fonts;
    ryn::text::TextEngine engine;
    ryn::detail::TextSceneService text_scene;
    std::vector<ryn::font::FontIdentity> chain;
    std::map<std::uint32_t, std::vector<ryn::font::FontIdentity>> chains;
    std::unique_ptr<ryn::detail::ButtonComponentHost> application;
    std::unique_ptr<rynui::example::ReferenceSurfaceHost> surfaces;
};

void test_typed_mount_retained_scene_and_non_interaction() {
    bool outside_host_rejected = false;
    try {
        rynui::example::ReferenceSurface({}, [] {});
    } catch (const std::logic_error&) {
        outside_host_rejected = true;
    }
    require(outside_host_rejected,
            "ReferenceSurface outside its Gallery host was accepted");

    Fixture fixture;
    ryn::Signal<rynui::example::GallerySupportStatus> status{
        rynui::example::GallerySupportStatus::partial};
    ryn::Signal<std::optional<ryn::Color>> swatch{
        std::optional<ryn::Color>{ryn::Color::rgba8(114, 46, 209)}};
    ryn::Signal<bool> elevated{true};
    ryn::Signal<ryn::ThemeConfig> theme{ryn::ThemeConfig{}};
    int content_runs = 0;
    fixture.surfaces->mount(ryn::Content{[&] {
        ryn::Theme(
            ryn::ThemeProps{}.config(theme),
            ryn::ThemeContent{[&] {
                rynui::example::ReferenceSurface(
                    rynui::example::ReferenceSurfaceProps{}
                        .status(status)
                        .swatch(swatch)
                        .elevated(elevated)
                        .layout(ryn::LayoutStyle{}
                            .width(ryn::dp(260.0F))
                            .min_height(ryn::dp(96.0F))),
                    [&] {
                        ++content_runs;
                        ryn::Text(u8"中文 Latin reference");
                        rynui::example::ReferenceSurface(
                            rynui::example::ReferenceSurfaceProps{}
                                .status(
                                    rynui::example::GallerySupportStatus::implemented)
                                .layout(ryn::LayoutStyle{}
                                    .width(ryn::dp(180.0F))),
                            [&] {
                                ++content_runs;
                                ryn::Text(u8"Nested surface");
                            });
                    });
                rynui::example::ReferenceSurface(
                    rynui::example::ReferenceSurfaceProps{}
                        .status(rynui::example::GallerySupportStatus::planned)
                        .layout(ryn::LayoutStyle{}.width(ryn::dp(220.0F))),
                    [&] { ++content_runs; });
            }});
    }});
    require(fixture.synchronize(),
            "ReferenceSurface retained fixture did not synchronize");

    const auto mounted = fixture.surfaces->mounted_surfaces();
    require(content_runs == 3 && mounted.size() == 3
                && fixture.application->components().component_count() == 8
                && fixture.application->text().mounted_texts().size() == 5
                && fixture.application->mounted_buttons().empty()
                && fixture.application->interactions().size() == 0
                && fixture.application->button_scene().size() == 3,
            "ReferenceSurface mount leaked Button or Interaction semantics");

    const auto outer_it = std::ranges::find_if(mounted, [&](const auto& item) {
        return fixture.surfaces->snapshot(item.component).elevated;
    });
    const auto nested_it = std::ranges::find_if(mounted, [&](const auto& item) {
        return fixture.surfaces->snapshot(item.component).status
            == rynui::example::GallerySupportStatus::implemented;
    });
    require(outer_it != mounted.end() && nested_it != mounted.end(),
            "ReferenceSurface generation-checked records were not discoverable");
    const auto outer = *outer_it;
    const auto nested = *nested_it;
    const auto outer_snapshot = fixture.surfaces->snapshot(outer.component);
    require(outer_snapshot.visual_range.count
                    == rynui::example::reference_surface_visual_layer_count
                && fixture.layer(
                    outer,
                    rynui::example::ReferenceSurfaceVisualLayer::swatch).opacity == 1.0F
                && fixture.layer(
                    outer,
                    rynui::example::ReferenceSurfaceVisualLayer::status_badge).color
                    == channels(ryn::resolve_theme().map().color_warning)
                && fixture.application->button_scene()
                    .shadow_effects(outer.scene).size() == 3
                && fixture.application->rounded_effects().live_count() == 3,
            "ReferenceSurface Theme visuals, swatch, badge, or shadow drifted");
    require(fixture.application->scene_composer().interaction_order().empty(),
            "ReferenceSurface entered the scene Interaction order");
    const auto commands = fixture.application->scene_composer()
        .ordered_scene().commands();
    require(!commands.empty()
                && commands.front().kind
                    == ryn::graphics::SceneDrawKind::rounded_effect
                && std::ranges::any_of(commands, [](const auto& command) {
                    return command.kind == ryn::graphics::SceneDrawKind::quad;
                })
                && std::ranges::any_of(commands, [](const auto& command) {
                    return command.kind == ryn::graphics::SceneDrawKind::glyph;
                }),
            "ReferenceSurface scene order omitted effect, quad, or Text content");

    const auto first_shadow = fixture.application->button_scene()
        .shadow_effects(outer.scene).front();
    ryn::runtime::NodePropertyWriter writer(
        fixture.nodes, fixture.dirty);
    require(writer.set_translation(outer.node, {7.0F, 5.0F})
                && fixture.synchronize(),
            "ReferenceSurface translation did not synchronize");
    const auto& translated = fixture.layer(
        outer,
        rynui::example::ReferenceSurfaceVisualLayer::background);
    const auto& shadow = fixture.application->rounded_effects().at(first_shadow);
    require(near(translated.translation[0], 14.0F / 640.0F)
                && near(translated.translation[1], -10.0F / 360.0F)
                && shadow.geometry.translation == ryn::runtime::Point{7.0F, 5.0F}
                && shadow.geometry.ancestor_clip.has_value()
                && shadow.geometry.ancestor_clip->bounds
                    == ryn::runtime::Rect{5.0F, 6.0F, 500.0F, 300.0F},
            "ReferenceSurface translation or ancestor clip diverged");

    fixture.application->pointer().dispatch({
        ryn::input::PointerIdentity::mouse(),
        ryn::input::PointerAction::move,
        ryn::input::PointerButton::none,
        fixture.nodes.require(outer.node).bounds.x + 10.0F,
        fixture.nodes.require(outer.node).bounds.y + 10.0F,
    });
    fixture.application->pointer().dispatch({
        ryn::input::PointerIdentity::mouse(),
        ryn::input::PointerAction::down,
        ryn::input::PointerButton::primary,
        fixture.nodes.require(outer.node).bounds.x + 10.0F,
        fixture.nodes.require(outer.node).bounds.y + 10.0F,
    });
    for (const auto key : {ryn::input::Key::tab, ryn::input::Key::enter,
                            ryn::input::Key::space}) {
        fixture.application->focus().dispatch({
            key,
            ryn::input::KeyAction::down,
            ryn::input::KeyModifier::none,
            false,
        });
    }
    const auto pointer = fixture.application->pointer().state(
        ryn::input::PointerIdentity::mouse());
    require(fixture.application->pointer().diagnostics().routes_dispatched == 0
                && fixture.application->focus().diagnostics().focus_changes == 0
                && !fixture.application->focus().state().focused.has_value()
                && pointer.has_value() && !pointer->capture.has_value()
                && fixture.application->interactions().size() == 0,
            "ReferenceSurface reacted to pointer or keyboard input");

    const auto stable_component_count = fixture.application->components()
        .component_count();
    const auto stable_scene = outer.scene;
    const auto stable_range = outer_snapshot.visual_range;
    const auto content_text = fixture.application->text().mounted_texts()[1].scene;
    const auto shape_count = fixture.text_scene.text_state(content_text)
        .counters().shape_count;
    fixture.dirty.clear();
    auto dark = ryn::ThemeConfig{};
    dark.algorithms = {ryn::ThemeAlgorithm::Dark};
    require(theme.set(dark), "ReferenceSurface Theme update was ignored");
    require(fixture.dirty.hit_test_nodes().empty()
                && fixture.synchronize(),
            "ReferenceSurface Theme update refreshed HitTest or failed");
    require(content_runs == 3
                && fixture.application->components().component_count()
                    == stable_component_count
                && fixture.surfaces->snapshot(outer.component).scene == stable_scene
                && fixture.surfaces->snapshot(outer.component).visual_range == stable_range
                && fixture.text_scene.text_state(content_text).counters().shape_count
                    == shape_count
                && fixture.layer(
                    outer,
                    rynui::example::ReferenceSurfaceVisualLayer::background).color
                    == channels(ryn::resolve_theme(dark)
                        .alias().color_background_container),
            "ReferenceSurface Theme update reran content or rebuilt retained identity");

    fixture.dirty.clear();
    require(swatch.set(std::nullopt)
                && elevated.set(false)
                && fixture.dirty.hit_test_nodes().empty()
                && fixture.synchronize(),
            "ReferenceSurface reactive material update failed");
    require(status.set(rynui::example::GallerySupportStatus::deprecated)
                && fixture.synchronize(),
            "ReferenceSurface status label update failed");
    require(content_runs == 3
                && fixture.surfaces->snapshot(outer.component).scene == stable_scene
                && fixture.layer(
                    outer,
                    rynui::example::ReferenceSurfaceVisualLayer::swatch).opacity == 0.0F
                && fixture.application->button_scene()
                    .shadow_effects(outer.scene).empty()
                && fixture.application->rounded_effects().live_count() == 0
                && fixture.application->interactions().size() == 0,
            "ReferenceSurface status/swatch/shadow update changed interaction or identity");

    const auto stale_scene = nested.scene;
    require(fixture.surfaces->destroy(nested.component),
            "ReferenceSurface destroy failed");
    bool stale_rejected = false;
    try {
        static_cast<void>(fixture.application->button_scene()
            .visual_range(stale_scene));
    } catch (const std::out_of_range&) {
        stale_rejected = true;
    }
    require(stale_rejected,
            "ReferenceSurface stale scene generation was not rejected");

    Fixture cascade;
    cascade.surfaces->mount(ryn::Content{[] {
        rynui::example::ReferenceSurface(
            rynui::example::ReferenceSurfaceProps{}
                .status(rynui::example::GallerySupportStatus::partial),
            [] {
                rynui::example::ReferenceSurface(
                    rynui::example::ReferenceSurfaceProps{}
                        .status(
                            rynui::example::GallerySupportStatus::implemented),
                    [] { ryn::Text(u8"Cascade child"); });
            });
    }});
    require(cascade.synchronize(),
            "nested ReferenceSurface cascade did not synchronize");
    const auto cascade_outer_it = std::ranges::find_if(
        cascade.surfaces->mounted_surfaces(),
        [&](const auto& item) {
            return cascade.surfaces->snapshot(item.component).status
                == rynui::example::GallerySupportStatus::partial;
        });
    require(cascade_outer_it != cascade.surfaces->mounted_surfaces().end(),
            "outer ReferenceSurface cascade record was not found");
    const auto cascade_outer = *cascade_outer_it;
    require(cascade.surfaces->destroy(cascade_outer.component),
            "parent ReferenceSurface destroy failed");
    require(cascade.surfaces->mounted_surfaces().empty(),
            "parent ReferenceSurface destroy retained nested records");
    require(cascade.application->button_scene().size() == 0,
            "parent ReferenceSurface destroy retained nested scenes");
    require(cascade.application->text().mounted_texts().empty(),
            "parent ReferenceSurface destroy retained nested Text records");
    require(cascade.application->interactions().size() == 0,
            "parent ReferenceSurface destroy retained interactions");
}

} // namespace

int main() {
    try {
        test_typed_mount_retained_scene_and_non_interaction();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
