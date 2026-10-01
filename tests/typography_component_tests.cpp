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

} // namespace

int main() {
    try {
        test_public_api_and_heading_levels();
        test_semantic_colours_and_disabled();
        test_emphasis_reaches_the_shape_request();
        test_inline_semantics_reach_the_resolver_and_scale();
        test_reactive_props_stay_local();
        test_theme_update_rescales_headings_without_remount();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
