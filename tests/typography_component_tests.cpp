#include "component/text_component.hpp"
#include "runtime/invalidation.hpp"

#include <ryn/rynui.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float left, float right) {
    return std::abs(left - right) < 0.0001F;
}

struct FontRequest final {
    ryn::SystemFontFamily family{ryn::SystemFontFamily::ui_sans};
    std::uint32_t weight{400};
    bool italic{};
    std::uint32_t pixel_size{};
};

struct Fixture final {
    Fixture()
        : layout(nodes),
          dirty(nodes, &frames),
          fonts(create_runtime()),
          engine(*fonts),
          scene(*fonts, engine, frames) {
        const auto latin = fonts->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, 14);
        const auto cjk = fonts->load_font_file(RYNUI_VALIDATION_CJK_FONT, 0, 14);
        require(latin && cjk, "Typography component fonts failed to load");
        chain = {latin.font, cjk.font};
        chains.emplace(14, chain);
        host = std::make_unique<ryn::detail::TextComponentHost>(
            nodes,
            layout,
            dirty,
            scene,
            [this](ryn::SystemFontFamily family, std::uint32_t weight, bool italic,
                   std::uint32_t pixel_size) {
                requests.push_back(FontRequest{family, weight, italic, pixel_size});
                return resolve_fonts(pixel_size);
            });
    }

    ~Fixture() { host->dispose(); }

    void enable_surfaces() {
        interactions = std::make_unique<ryn::input::InteractionRegistry>(host->components(), nodes);
        hit_test = std::make_unique<ryn::input::HitTestSnapshot>(*interactions, nodes);
        composer = std::make_unique<ryn::component::ComponentSceneComposer>(host->components(), *interactions, *hit_test);
        surfaces = std::make_unique<ryn::component::RetainedSurfaceService>(host->components(), nodes, *composer);
        host->attach_component_scene(*composer);
        host->attach_surfaces(*surfaces);
    }

    static std::unique_ptr<ryn::font::FontRuntime> create_runtime() {
        auto created = ryn::font::FontRuntime::create();
        require(static_cast<bool>(created), "Font Runtime initialization failed");
        return std::move(created.runtime);
    }

    std::vector<ryn::font::FontIdentity> resolve_fonts(std::uint32_t pixel_size) {
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

    bool layout_texts(float width = 640.0F, float height = 360.0F) {
        return host->layout_and_synchronize(
            {width, height},
            {0.0F, 0.0F, width, height},
            {12.0F, 16.0F},
            4.0F);
    }

    ryn::runtime::SemanticForeground mounted_color(std::size_t index) const {
        const auto& instances = scene.glyph_scene().instances().instances();
        const auto primitive = scene.primitive(host->mounted_texts()[index].scene);
        const auto& instance = instances[primitive.instances.first];
        return {instance.color[0], instance.color[1], instance.color[2],
            instance.color[3]};
    }

    ryn::text::TextState& text_state(std::size_t index) {
        return scene.text_state(host->mounted_texts()[index].scene);
    }

    ryn::runtime::NodeStore nodes;
    ryn::layout::LayoutEngine layout;
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::DirtyQueues dirty;
    std::unique_ptr<ryn::font::FontRuntime> fonts;
    ryn::text::TextEngine engine;
    ryn::detail::TextSceneService scene;
    std::vector<ryn::font::FontIdentity> chain;
    std::map<std::uint32_t, std::vector<ryn::font::FontIdentity>> chains;
    // Every resolver call, so a test can assert which family, weight and slant
    // the component actually asked for.
    std::vector<FontRequest> requests;
    std::unique_ptr<ryn::detail::TextComponentHost> host;
    std::unique_ptr<ryn::input::InteractionRegistry> interactions;
    std::unique_ptr<ryn::input::HitTestSnapshot> hit_test;
    std::unique_ptr<ryn::component::ComponentSceneComposer> composer;
    std::unique_ptr<ryn::component::RetainedSurfaceService> surfaces;

    [[nodiscard]] bool requested(ryn::SystemFontFamily family, std::uint32_t weight,
                                  bool italic) const {
        return std::ranges::any_of(requests, [&](const FontRequest& request) {
            return request.family == family && request.weight == weight
                && request.italic == italic;
        });
    }
};

[[nodiscard]] ryn::runtime::SemanticForeground channels(ryn::Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

void test_public_api_and_heading_levels() {
    Fixture fixture;
    // The same text at every level isolates the typography from the content, so
    // each measurement difference is attributable to the level token alone.
    fixture.host->mount(ryn::Content{[] {
        ryn::Title(ryn::TitleProps{}.content(u8"Heading").level(ryn::TypographyLevel::H1));
        ryn::Title(ryn::TitleProps{}.content(u8"Heading").level(ryn::TypographyLevel::H2));
        ryn::Title(ryn::TitleProps{}.content(u8"Heading").level(ryn::TypographyLevel::H3));
        ryn::Title(ryn::TitleProps{}.content(u8"Heading").level(ryn::TypographyLevel::H4));
        ryn::Title(ryn::TitleProps{}.content(u8"Heading").level(ryn::TypographyLevel::H5));
        ryn::Text(ryn::TypographyProps{}.content(u8"Body 中文"));
        ryn::Paragraph(ryn::TypographyProps{}.content(u8"Paragraph copy"));
    }});
    require(fixture.host->mounted_texts().size() == 7,
            "Typography declarations did not mount one component each");
    require(fixture.layout_texts(), "Typography fixture did not synchronize");

    const auto& typography = ryn::resolve_theme().typography();
    // Levels 1..5 use the locked reference chain 38/30/24/20/16 at base 14.
    const std::array<float, 5> expected_sizes{38.0F, 30.0F, 24.0F, 20.0F, 16.0F};
    for (std::size_t index = 0; index < expected_sizes.size(); ++index) {
        require(near(typography.headings[index].font_size, expected_sizes[index]),
                "Typography heading token drifted from the reference chain");
        // A larger level must produce a taller line box for identical text.
        if (index + 1 < expected_sizes.size()) {
            require(fixture.text_state(index).measurement().content_bounds.bottom
                        > fixture.text_state(index + 1).measurement().content_bounds.bottom,
                    "a larger heading level did not measure taller than the next level");
        }
    }
    // Body and paragraph share the base typography, so they measure identically
    // apart from their content.
    require(near(typography.base_font_size, 14.0F)
                && near(typography.base_line_height, 22.0F),
            "Typography base typography drifted from the locked baseline");
}

void test_semantic_colours_and_disabled() {
    Fixture fixture;
    fixture.host->mount(ryn::Content{[] {
        ryn::Text(ryn::TypographyProps{}.content(u8"Default"));
        ryn::Text(ryn::TypographyProps{}.content(u8"Secondary")
            .type(ryn::TypographyType::Secondary));
        ryn::Text(ryn::TypographyProps{}.content(u8"Success")
            .type(ryn::TypographyType::Success));
        ryn::Text(ryn::TypographyProps{}.content(u8"Warning")
            .type(ryn::TypographyType::Warning));
        ryn::Text(ryn::TypographyProps{}.content(u8"Danger")
            .type(ryn::TypographyType::Danger));
        ryn::Text(ryn::TypographyProps{}.content(u8"Disabled").disabled(true));
    }});
    require(fixture.layout_texts(), "semantic Typography fixture did not synchronize");

    const auto& colors = ryn::resolve_theme().typography().colors;
    require(fixture.mounted_color(0) == channels(colors.text),
            "default Typography text did not use the Typography text colour");
    require(fixture.mounted_color(1) == channels(colors.description),
            "secondary Typography text did not use the description colour");
    require(fixture.mounted_color(2) == channels(colors.success),
            "success Typography text did not use the success colour");
    require(fixture.mounted_color(3) == channels(colors.warning),
            "warning Typography text did not use the warning colour");
    require(fixture.mounted_color(4) == channels(colors.error),
            "danger Typography text did not use the error colour");
    require(fixture.mounted_color(5) == channels(colors.disabled),
            "disabled Typography text did not use the disabled colour");
}

void test_emphasis_reaches_the_shape_request() {
    Fixture fixture;
    ryn::Signal<bool> bold{false};
    fixture.host->mount(ryn::Content{[&] {
        ryn::Text(ryn::TypographyProps{}.content(u8"Regular"));
        ryn::Text(ryn::TypographyProps{}.content(u8"Emphasis").strong(bold));
    }});
    require(fixture.layout_texts(), "emphasis Typography fixture did not synchronize");
    const auto theme = ryn::resolve_theme();
    require(theme.typography().font_weight == 400
                && theme.typography().font_weight_strong == 600,
            "Typography weights drifted from the locked baseline");

    const auto& state = fixture.text_state(1);
    const auto component_count = fixture.host->components().component_count();
    const auto target_node = fixture.scene.node(fixture.host->mounted_texts()[1].scene);
    const auto shape_before = state.counters().shape_count;
    fixture.dirty.clear();

    // Flipping emphasis re-resolves the face, which requires a reshape of that
    // component only. The fixture resolver ignores weight, so the assertion is
    // about the request reaching the shape path, not about visible weight.
    require(bold.set(true), "Typography strong Signal did not propagate");
    require(!fixture.dirty.layout_roots().empty()
                && fixture.dirty.layout_roots().front() == target_node,
            "emphasis change did not request a reshape of its own component");
    require(fixture.layout_texts(), "emphasis update did not synchronize");
    require(fixture.host->components().component_count() == component_count
                && fixture.text_state(1).counters().shape_count > shape_before,
            "emphasis change did not reshape or remounted the component");
}

void test_inline_semantics_reach_the_resolver_and_scale() {
    Fixture fixture;
    fixture.host->mount(ryn::Content{[] {
        ryn::Text(ryn::TypographyProps{}.content(u8"Plain"));
        ryn::Text(ryn::TypographyProps{}.content(u8"Strong").strong(true));
        ryn::Text(ryn::TypographyProps{}.content(u8"Italic").italic(true));
        ryn::Text(ryn::TypographyProps{}.content(u8"Code").code(true));
        ryn::Text(ryn::TypographyProps{}.content(u8"Key").keyboard(true));
    }});
    require(fixture.layout_texts(), "inline Typography fixture did not synchronize");

    const auto& typography = ryn::resolve_theme().typography();
    // Every inline variant must reach the resolver with its own request.
    require(fixture.requested(ryn::SystemFontFamily::ui_sans, 400, false),
            "plain Typography text did not request the UI family at regular weight");
    require(fixture.requested(ryn::SystemFontFamily::ui_sans,
                typography.font_weight_strong, false),
            "strong Typography text did not request the strong weight");
    require(fixture.requested(ryn::SystemFontFamily::ui_sans, 400, true),
            "italic Typography text did not request a slanted face");
    require(fixture.requested(ryn::SystemFontFamily::ui_monospace, 400, false),
            "code and keyboard Typography text did not request the code family");

    // `code` and `keyboard` scale by their inline token instead of the base size.
    const auto expected_code =
        static_cast<std::uint32_t>(std::lround(
            typography.base_font_size * typography.code.font_scale));
    const auto expected_keyboard =
        static_cast<std::uint32_t>(std::lround(
            typography.base_font_size * typography.keyboard.font_scale));
    require(expected_code != expected_keyboard,
            "code and keyboard inline scales are not distinct");
    require(std::ranges::any_of(fixture.requests, [&](const FontRequest& request) {
                return request.family == ryn::SystemFontFamily::ui_monospace
                    && request.pixel_size == expected_code;
            })
                && std::ranges::any_of(fixture.requests, [&](const FontRequest& request) {
                    return request.family == ryn::SystemFontFamily::ui_monospace
                        && request.pixel_size == expected_keyboard;
                }),
            "code or keyboard did not resolve at its inline token size");
    require(fixture.text_state(3).shaped().default_metrics.logical_pixel_size == expected_code
        && fixture.text_state(4).shaped().default_metrics.logical_pixel_size == expected_keyboard,
        "inline font requests were correct but final shapes reverted to body size");
}

void test_inline_token_change_reaches_the_shape() {
    Fixture fixture;
    ryn::Signal<ryn::ThemeConfig> config{ryn::ThemeConfig{}};
    fixture.host->mount(ryn::Content{[&] {
        ryn::Theme(ryn::ThemeProps{}.config(config), ryn::ThemeContent{[&] {
            ryn::Text(ryn::TypographyProps{}.content(u8"Code").code(true));
        }});
        ryn::Text(ryn::TypographyProps{}.content(u8"Stable sibling"));
    }});
    require(fixture.layout_texts(), "inline token fixture did not synchronize");
    const auto target_node = fixture.scene.node(fixture.host->mounted_texts()[0].scene);
    const auto component_count = fixture.host->components().component_count();
    const auto sibling_shape_before = fixture.text_state(1).counters().shape_count;
    const auto code_requests_before = fixture.requests.size();
    fixture.dirty.clear();

    // The inline token group is its own TokenIdentity, so without an explicit
    // capture a scale change would silently not reach this component.
    auto scaled = ryn::ThemeConfig{};
    scaled.typography.tokens.code.font_scale = 0.5F;
    require(config.set(scaled), "inline code token update was suppressed");
    require(fixture.dirty.layout_roots() == std::vector<ryn::runtime::NodeId>{target_node},
            "inline code font scale did not invalidate the code component layout");
    require(fixture.layout_texts(), "inline code token update did not synchronize");

    const auto expected = static_cast<std::uint32_t>(std::lround(
        ryn::resolve_theme(scaled).typography().base_font_size * 0.5F));
    const auto resolved_scaled = std::ranges::any_of(
        std::span{fixture.requests}.subspan(code_requests_before),
        [&](const FontRequest& request) {
            return request.family == ryn::SystemFontFamily::ui_monospace
                && request.pixel_size == expected;
        });
    require(resolved_scaled,
            "inline code font scale did not change the resolved code font size");
    require(fixture.host->components().component_count() == component_count,
            "inline code token update remounted the component");
    require(fixture.text_state(1).counters().shape_count == sibling_shape_before,
            "inline code token update reshaped an unrelated sibling");
}

void test_reactive_heading_level_keeps_identity() {
    Fixture fixture;
    ryn::Signal<ryn::TypographyLevel> level{ryn::TypographyLevel::H1};
    fixture.host->mount(ryn::Content{[&] {
        ryn::Title(ryn::TitleProps{}.content(u8"Heading").level(level));
        ryn::Text(ryn::TypographyProps{}.content(u8"Stable sibling"));
    }});
    require(fixture.layout_texts(), "reactive level fixture did not synchronize");
    const auto target_node = fixture.scene.node(fixture.host->mounted_texts()[0].scene);
    const auto target_component = fixture.host->mounted_texts()[0].component;
    const auto component_count = fixture.host->components().component_count();
    const auto sibling_shape_before = fixture.text_state(1).counters().shape_count;
    const auto bottom_before = fixture.text_state(0).measurement().content_bounds.bottom;
    fixture.dirty.clear();

    // The spec requires a level change after mount to keep the component
    // identity, so the node, component id and component count must all survive.
    require(level.set(ryn::TypographyLevel::H4),
            "reactive level Signal did not propagate");
    require(fixture.dirty.layout_roots() == std::vector<ryn::runtime::NodeId>{target_node},
            "level change did not invalidate exactly the heading layout");
    require(fixture.layout_texts(), "level change did not synchronize");
    require(fixture.host->components().component_count() == component_count
                && fixture.host->mounted_texts()[0].component == target_component
                && fixture.scene.node(fixture.host->mounted_texts()[0].scene) == target_node,
            "level change changed the heading component identity");
    const auto& typography = ryn::resolve_theme().typography();
    require(fixture.text_state(0).measurement().content_bounds.bottom < bottom_before
                && fixture.text_state(0).measurement().content_bounds.bottom
                    > typography.headings[4].font_size * 0.5F,
            "level change did not re-resolve the heading tokens");
    require(fixture.text_state(1).counters().shape_count == sibling_shape_before,
            "level change reshaped an unrelated sibling");
}

void test_reactive_props_stay_local() {
    Fixture fixture;
    ryn::Signal<ryn::String> content{ryn::String{u8"First"}};
    ryn::Signal<ryn::TypographyType> type{ryn::TypographyType::Default};
    fixture.host->mount(ryn::Content{[&] {
        ryn::Text(ryn::TypographyProps{}.content(content).type(type));
        ryn::Text(ryn::TypographyProps{}.content(u8"Sibling"));
    }});
    require(fixture.layout_texts(), "reactive Typography fixture did not synchronize");
    const auto target_node = fixture.scene.node(fixture.host->mounted_texts()[0].scene);
    const auto sibling_shape_before = fixture.text_state(1).counters().shape_count;
    const auto component_count = fixture.host->components().component_count();
    fixture.dirty.clear();

    require(content.set(ryn::String{u8"Second 中文"}),
            "Typography content Signal did not propagate");
    require(fixture.dirty.layout_roots() == std::vector<ryn::runtime::NodeId>{target_node}
                && fixture.host->components().component_count() == component_count,
            "Typography content update remounted a component or missed its node");
    require(fixture.layout_texts(), "Typography content update did not synchronize");
    require(fixture.text_state(1).counters().shape_count == sibling_shape_before,
            "Typography content update reshaped an unrelated sibling");

    fixture.dirty.clear();
    const auto& colors = ryn::resolve_theme().typography().colors;
    require(type.set(ryn::TypographyType::Danger),
            "Typography type Signal did not propagate");
    require(fixture.dirty.material_nodes() == std::vector<ryn::runtime::NodeId>{target_node}
                && fixture.dirty.layout_roots().empty(),
            "Typography type change was not a material-only invalidation");
    require(fixture.layout_texts(), "Typography type update did not synchronize");
    require(fixture.mounted_color(0) == channels(colors.error),
            "Typography type update did not reach the rendered colour");
}

void test_theme_update_rescales_headings_without_remount() {
    Fixture fixture;
    ryn::Signal<ryn::ThemeConfig> config{ryn::ThemeConfig{}};
    fixture.host->mount(ryn::Content{[&] {
        ryn::Theme(ryn::ThemeProps{}.config(config), ryn::ThemeContent{[&] {
            ryn::Title(ryn::TitleProps{}.content(u8"Themed heading")
                .level(ryn::TypographyLevel::H1));
        }});
        ryn::Text(ryn::TypographyProps{}.content(u8"Stable sibling"));
    }});
    require(fixture.layout_texts(), "themed Typography fixture did not synchronize");
    const auto target_node = fixture.scene.node(fixture.host->mounted_texts()[0].scene);
    const auto component_count = fixture.host->components().component_count();
    const auto sibling_shape_before = fixture.text_state(1).counters().shape_count;
    const auto heading_baseline_before =
        fixture.text_state(0).measurement().lines.front().baseline;
    const auto heading_bottom_before =
        fixture.text_state(0).measurement().content_bounds.bottom;
    fixture.dirty.clear();

    auto compact = ryn::ThemeConfig{};
    compact.algorithms = {ryn::ThemeAlgorithm::Compact};
    require(config.set(compact), "Typography Theme update was suppressed");
    require(fixture.dirty.layout_roots() == std::vector<ryn::runtime::NodeId>{target_node},
            "Typography Theme update did not invalidate the heading layout");
    require(fixture.layout_texts(), "Typography Theme update did not synchronize");
    // Compact lowers the base size, so the heading level scales down and its
    // baseline and line box both move up.
    const auto& compact_lines = fixture.text_state(0).measurement().lines;
    require(fixture.text_state(0).measurement().content_bounds.bottom
                    < heading_bottom_before
                && compact_lines.front().baseline < heading_baseline_before,
            "heading did not rescale from the Typography Component Token group");
    require(fixture.host->components().component_count() == component_count,
            "Typography Theme update remounted the component");
    require(fixture.text_state(1).counters().shape_count == sibling_shape_before,
            "Typography Theme update reshaped an unrelated sibling");
}

void test_decoration_layers_metrics_reflow_and_material_updates() {
    Fixture fixture;
    fixture.enable_surfaces();
    ryn::Signal<bool> underline{false};
    ryn::Signal<ryn::ThemeConfig> config{ryn::ThemeConfig{}};
    fixture.host->mount(ryn::Content{[&] {
        ryn::Theme(ryn::ThemeProps{}.config(config), ryn::ThemeContent{[&] {
            ryn::Paragraph(ryn::TypographyProps{}.content(u8"Decorated text")
                .code(true).mark(true).underline(underline).strikethrough(true));
        }});
    }});
    require(fixture.layout_texts(), "decoration layout failed");
    require(fixture.host->synchronize_scene_fragments([](auto) {
        return std::optional<ryn::input::InteractionId>{}; }), "glyph fragment was not published");
    static_cast<void>(fixture.surfaces->compact_effects({0, 0, 640, 360}));
    fixture.composer->rebuild({0, 0, 640, 360});
    auto commands = fixture.composer->ordered_scene().commands();
    require(commands.size() == 4 && commands[0].kind == ryn::graphics::SceneDrawKind::quad
        && commands[1].kind == ryn::graphics::SceneDrawKind::rounded_effect
        && commands[2].kind == ryn::graphics::SceneDrawKind::glyph
        && commands[3].kind == ryn::graphics::SceneDrawKind::quad
        && commands[0].instance_count == 2 && commands[3].instance_count == 1,
        "decorations were not layered before and after glyphs");
    const auto shape_before = fixture.text_state(0).counters().shape_count;
    const auto measure_before = fixture.text_state(0).counters().measure_count;
    require(underline.set(true) && fixture.layout_texts(), "reactive underline did not synchronize");
    fixture.composer->rebuild({0, 0, 640, 360});
    commands = fixture.composer->ordered_scene().commands();
    require(commands.back().instance_count == 2, "reactive underline did not use a persistent layer");
    const auto& text = fixture.text_state(0);
    const auto& node = fixture.nodes.require(fixture.scene.node(fixture.host->mounted_texts()[0].scene));
    const auto& token = ryn::resolve_theme().typography().code;
    const auto size = text.shaped().default_metrics.logical_pixel_size;
    const float glyph_top = node.bounds.y + token.padding_block_start_em * (14 * token.font_scale) + token.border_width;
    const float expected_y = glyph_top + text.measurement().lines[0].baseline
        - text.shaped().default_metrics.underline_position * (14 * token.font_scale);
    const auto& quad = fixture.surfaces->instances().at(commands.back().first_instance);
    require(near((1 - quad.clip_rect[1]) * 180, expected_y)
        && near(-quad.clip_rect[3] * 180,
            text.shaped().default_metrics.underline_thickness * (14 * token.font_scale)),
        "underline does not follow the font's em-relative position and thickness");
    require(size == 12 && text.counters().shape_count == shape_before
        && text.counters().measure_count == measure_before,
        "decoration flags reshaped or remeasured text");
    auto changed = ryn::ThemeConfig{};
    changed.typography.tokens.code.background = ryn::Color::rgba8(50, 60, 70);
    fixture.surfaces->instances().clear_dirty_ranges();
    fixture.dirty.clear();
    require(config.set(changed) && fixture.dirty.layout_roots().empty()
        && fixture.layout_texts(), "decoration color requested layout");
    require(fixture.text_state(0).counters().shape_count == shape_before
        && fixture.text_state(0).counters().measure_count == measure_before
        && fixture.surfaces->instances().geometry_dirty_ranges().empty()
        && !fixture.surfaces->instances().material_dirty_ranges().empty(),
        "decoration color was not a material-only update");
    const auto component = fixture.host->mounted_texts()[0].component;
    require(fixture.host->destroy(component) && fixture.surfaces->size() == 0
        && fixture.surfaces->instances().size() == 0, "decoration teardown leaked ranges");
}

void test_multiline_decoration_ranges_translation_and_clip() {
    Fixture fixture;
    fixture.enable_surfaces();
    ryn::Signal<ryn::String> content{ryn::String{u8"One\nTwo\nThree\nFour\nFive\nSix\nSeven\nEight\nNine\nTen"}};
    fixture.host->mount(ryn::Content{[&] {
        ryn::Paragraph(ryn::TypographyProps{}.content(content).underline(true).strikethrough(true));
    }});
    require(fixture.layout_texts(), "multiline decorations failed");
    require(fixture.surfaces->instances().size() == 20, "multiline decorations retained the 16-quad surface limit");
    const auto count = fixture.host->components().component_count();
    require(content.set(ryn::String{u8"One line"}) && fixture.layout_texts()
        && fixture.surfaces->instances().size() == 2
        && fixture.host->components().component_count() == count,
        "decoration reflow did not resize ranges while preserving identity");
    auto& node = fixture.nodes.require(fixture.scene.node(fixture.host->mounted_texts()[0].scene));
    const auto before = fixture.surfaces->instances().instances()[0].clip_rect;
    node.translation = {5, 7};
    require(fixture.layout_texts(), "translated decoration layout failed");
    const auto after = fixture.surfaces->instances().instances()[0].clip_rect;
    require(near(after[0] - before[0], 10.0F / 640)
        && near(after[1] - before[1], -14.0F / 360), "decorations did not follow scroll translation");
    require(fixture.host->layout_and_synchronize({640, 360}, {20, 0, 10, 360}, {12, 16}, 4),
        "clipped decoration synchronization failed");
    for (const auto& q : fixture.surfaces->instances().instances()) {
        const auto x = (q.clip_rect[0] + 1) * 320;
        const auto right = x + q.clip_rect[2] * 320;
        require(x >= 19.999F && right <= 30.001F, "decorations escaped the clip");
    }
}

void test_disabled_secondary_uses_component_color_and_subscription() {
    Fixture fixture;
    ryn::Signal<ryn::ThemeConfig> config{ryn::ThemeConfig{}};
    fixture.host->mount(ryn::Content{[&] {
        ryn::Theme(ryn::ThemeProps{}.config(config), ryn::ThemeContent{[&] {
            ryn::Text(ryn::TypographyProps{}.content(u8"Disabled secondary")
                .type(ryn::TypographyType::Secondary).disabled(true));
        }});
    }});
    require(fixture.layout_texts(), "disabled fixture failed");
    require(fixture.mounted_color(0) == channels(ryn::resolve_theme().typography().colors.disabled),
        "secondary overrode disabled precedence");
    auto changed = ryn::ThemeConfig{};
    changed.typography.tokens.disabled = ryn::Color::rgba8(70, 80, 90);
    require(config.set(changed) && fixture.layout_texts()
        && fixture.mounted_color(0) == channels(*changed.typography.tokens.disabled),
        "disabled text missed its Typography component token subscription");
}

void test_decoration_geometry_at_display_scales() {
    Fixture fixture;
    fixture.enable_surfaces();
    fixture.host->mount(ryn::Content{[] {
        ryn::Title(ryn::TitleProps{}.content(u8"Title").underline(true).strikethrough(true));
    }});
    const auto identity = fixture.host->mounted_texts()[0].component;
    for (const float scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
        fixture.host->set_font_resolver([&](auto, auto, auto, std::uint32_t size) {
            auto loaded = fixture.fonts->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0,
                ryn::font::FontRasterConfig{size, scale});
            require(static_cast<bool>(loaded), "scaled font failed to load");
            return std::vector{loaded.font};
        });
        require(fixture.layout_texts(), "scaled decoration layout failed");
        const auto& text = fixture.text_state(0);
        const auto metrics = text.shaped().default_metrics;
        const auto& node = fixture.nodes.require(fixture.scene.node(fixture.host->mounted_texts()[0].scene));
        const float top = node.bounds.y + 1.2F * 38;
        const auto& quad = fixture.surfaces->instances().instances()[0];
        require(near((1 - quad.clip_rect[1]) * 180,
            top + text.measurement().lines[0].baseline - metrics.underline_position * 38)
            && near(-quad.clip_rect[3] * 180, metrics.underline_thickness * 38)
            && fixture.host->mounted_texts()[0].component == identity,
            "display scale changed decoration units or component identity");
        require(near(node.bounds.height, 38 * (1.2F + 0.5F) + 38 * 1.4F),
            "heading margins do not follow the Typography component tokens");
    }
}

} // namespace

int main() {
    try {
        test_public_api_and_heading_levels();
        test_semantic_colours_and_disabled();
        test_emphasis_reaches_the_shape_request();
        test_inline_semantics_reach_the_resolver_and_scale();
        test_inline_token_change_reaches_the_shape();
        test_reactive_heading_level_keeps_identity();
        test_reactive_props_stay_local();
        test_theme_update_rescales_headings_without_remount();
        test_decoration_layers_metrics_reflow_and_material_updates();
        test_multiline_decoration_ranges_translation_and_clip();
        test_disabled_secondary_uses_component_color_and_subscription();
        test_decoration_geometry_at_display_scales();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
