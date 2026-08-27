#include "component/text_component.hpp"
#include "runtime/invalidation.hpp"

#include <ryn/rynui.hpp>

#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
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

bool clean(const ryn::runtime::DirtyQueues& dirty) {
    return dirty.layout_roots().empty()
        && dirty.placement_roots().empty()
        && dirty.material_nodes().empty()
        && dirty.transform_nodes().empty()
        && dirty.geometry_nodes().empty()
        && dirty.hit_test_nodes().empty();
}

struct Fixture final {
    Fixture()
        : layout(nodes),
          dirty(nodes, &frames),
          fonts(create_runtime()),
          engine(*fonts),
          scene(*fonts, engine, frames) {
        const auto latin = fonts->load_font_file(
            RYNUI_VALIDATION_LATIN_FONT, 0, 14);
        const auto cjk = fonts->load_font_file(
            RYNUI_VALIDATION_CJK_FONT, 0, 14);
        require(latin && cjk, "Text component fonts failed to load");
        chain = {latin.font, cjk.font};
        host = std::make_unique<ryn::detail::TextComponentHost>(
            nodes,
            layout,
            dirty,
            scene,
            chain);
    }

    static std::unique_ptr<ryn::font::FontRuntime> create_runtime() {
        auto created = ryn::font::FontRuntime::create();
        require(static_cast<bool>(created), "Font Runtime initialization failed");
        return std::move(created.runtime);
    }

    bool layout_texts(float width = 640.0F, float height = 360.0F) {
        return host->layout_and_synchronize(
            {width, height},
            {0.0F, 0.0F, width, height},
            {12.0F, 16.0F},
            4.0F);
    }

    ryn::runtime::NodeStore nodes;
    ryn::layout::LayoutEngine layout;
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::DirtyQueues dirty;
    std::unique_ptr<ryn::font::FontRuntime> fonts;
    ryn::text::TextEngine engine;
    ryn::detail::TextSceneService scene;
    std::vector<ryn::font::FontIdentity> chain;
    std::unique_ptr<ryn::detail::TextComponentHost> host;
};

struct ForegroundProviderState final {};
struct HorizontalProviderState final {};
struct ForegroundContentSlot final {};
using ForegroundContent = ryn::SlotContent<ForegroundContentSlot>;

ryn::runtime::ComponentId mount_foreground_provider(
    ryn::layout::LayoutEngine& layout,
    ryn::Prop<ryn::runtime::SemanticForeground> foreground,
    const ForegroundContent& content) {
    auto& build = ryn::runtime::require_component_build_context();
    const auto component = build.mount_component<ForegroundProviderState>();
    layout.set_layout(build.root(component), ryn::layout::BoxLayout{});
    build.mount_slot_with_semantic_foreground(
        component,
        content,
        std::move(foreground));
    return component;
}

ryn::runtime::SemanticForeground mounted_color(
    const Fixture& fixture,
    std::size_t index) {
    const auto scene = fixture.host->mounted_texts()[index].scene;
    const auto range = fixture.scene.primitive(scene).instances;
    require(range.count != 0, "Text context test produced no glyph instances");
    return fixture.scene.glyph_scene().instances().at(range.first).color;
}

ryn::runtime::ComponentId mount_horizontal_provider(
    ryn::layout::LayoutEngine& layout,
    ryn::layout::HorizontalContentLayout model,
    const ForegroundContent& content) {
    auto& build = ryn::runtime::require_component_build_context();
    const auto component = build.mount_component<HorizontalProviderState>();
    layout.set_layout(build.root(component), model);
    build.mount_slot(component, content);
    return component;
}

bool synchronize_horizontal_texts(
    Fixture& fixture,
    ryn::runtime::ComponentId container) {
    constexpr ryn::runtime::Size viewport{640.0F, 360.0F};
    static_cast<void>(fixture.layout.layout(
        fixture.host->components().root(container),
        {
            0.0F,
            std::numeric_limits<float>::infinity(),
            0.0F,
            viewport.height,
        },
        {12.0F, 16.0F}));
    for (const auto& mounted : fixture.host->mounted_texts()) {
        const auto& node = fixture.nodes.require(
            fixture.scene.node(mounted.scene));
        if (!fixture.scene.synchronize(mounted.scene, {
                {node.bounds.x, node.bounds.y},
                viewport,
                {0.0F, 0.0F, viewport.width, viewport.height},
                node.translation,
                {},
                1.0F,
            })) {
            return false;
        }
    }
    fixture.dirty.clear();
    return true;
}

void test_mount_owner_thread_and_dispose_lifecycle() {
    bool outside_host_diagnosed = false;
    try {
        ryn::Text(u8"outside");
    } catch (const std::logic_error&) {
        outside_host_diagnosed = true;
    }
    require(outside_host_diagnosed,
            "ryn::Text outside a TextComponentHost was accepted");

    Fixture fixture;
    ryn::Signal<ryn::String> content{ryn::String{u8"主文本 中文"}};
    ryn::Signal<ryn::TextTone> tone{ryn::TextTone::Primary};
    fixture.host->mount(ryn::Content{[&] {
        ryn::Text(ryn::TextProps{}.content(content).tone(tone));
        ryn::Text(ryn::TextProps{}
            .content(u8"兄弟 Text")
            .tone(ryn::TextTone::Disabled));
    }});
    require(fixture.host->components().component_count() == 2
                && fixture.host->components().mount_runs() == 1
                && fixture.host->mounted_texts().size() == 2
                && fixture.nodes.size() == 2
                && fixture.scene.size() == 2,
            "first Text mount did not create two stable component records");
    require(fixture.layout_texts(), "mounted Text components did not layout/synchronize");
    const auto first = fixture.host->mounted_texts().front();
    const auto second = fixture.host->mounted_texts().back();
    require(fixture.scene.declaration_order(first.scene)
                    < fixture.scene.declaration_order(second.scene)
                && fixture.nodes.require(fixture.scene.node(first.scene)).bounds.y
                    < fixture.nodes.require(fixture.scene.node(second.scene)).bounds.y,
            "sibling Text declaration or placement order changed");
    const auto disabled_range = fixture.scene.primitive(second.scene).instances;
    require(disabled_range.count != 0
                && fixture.scene.glyph_scene().instances()
                    .at(disabled_range.first).color
                    == fixture.host->theme().text.disabled,
            "disabled Text did not use the Default Theme disabled alias");

    static_cast<void>(fixture.frames.consume_request());
    fixture.dirty.clear();
    const auto sibling_shape = fixture.scene.text_state(second.scene).counters().shape_count;
    require(fixture.host->destroy(first.component)
                && !fixture.scene.contains(first.scene)
                && fixture.scene.contains(second.scene)
                && fixture.nodes.size() == 1,
            "Text component disposal did not remove its Scene range and Node");
    static_cast<void>(fixture.frames.consume_request());
    content.set(ryn::String{u8"销毁后写入"});
    require(!fixture.frames.pending()
                && fixture.scene.text_state(second.scene).counters().shape_count
                    == sibling_shape,
            "disposed Text Prop observer requested a frame or changed its sibling");

    fixture.host->dispose();
    static_cast<void>(fixture.frames.consume_request());
    tone.set(ryn::TextTone::Secondary);
    require(fixture.scene.size() == 0
                && fixture.nodes.size() == 0
                && !fixture.frames.pending(),
            "Text Host dispose left Scene, Node, or Prop callbacks alive");

    Fixture wrong_thread;
    std::exception_ptr error;
    std::thread worker([&] {
        try {
            wrong_thread.host->mount(ryn::Content{[] { ryn::Text(u8"wrong"); }});
        } catch (...) {
            error = std::current_exception();
        }
    });
    worker.join();
    require(error != nullptr
                && wrong_thread.host->components().component_count() == 0
                && wrong_thread.scene.size() == 0,
            "wrong-thread Text mount changed Host state");
}

void test_reactive_content_tone_and_margin_are_minimal() {
    Fixture fixture;
    ryn::Signal<ryn::String> content{ryn::String{u8"状态 A"}};
    ryn::Signal<ryn::TextTone> tone{ryn::TextTone::Primary};
    ryn::Signal<ryn::LogicalLength> margin{ryn::dp(4.0F)};
    fixture.host->mount(ryn::Content{[&] {
        ryn::LayoutStyle style;
        style.width(ryn::dp(120.0F)).margin_left(margin);
        ryn::Text(ryn::TextProps{}
            .content(content)
            .tone(tone)
            .layout(style));
        ryn::Text(u8"稳定兄弟");
    }});
    require(fixture.layout_texts(), "reactive Text fixture did not synchronize");
    const auto target = fixture.host->mounted_texts().front();
    const auto sibling = fixture.host->mounted_texts().back();
    const auto target_initial = fixture.scene.text_state(target.scene).counters();
    const auto sibling_initial = fixture.scene.text_state(sibling.scene).counters();
    fixture.scene.atlas().clear_dirty_regions();
    fixture.scene.glyph_scene().instances().clear_dirty_ranges();
    fixture.dirty.clear();
    static_cast<void>(fixture.frames.consume_request());

    tone.set(ryn::TextTone::Secondary);
    require(fixture.dirty.material_nodes()
                    == std::vector<ryn::runtime::NodeId>{fixture.scene.node(target.scene)}
                && fixture.dirty.layout_roots().empty(),
            "tone Prop update escaped Material invalidation");
    require(fixture.layout_texts(), "tone update did not synchronize");
    const auto range = fixture.scene.primitive(target.scene).instances;
    require(fixture.scene.glyph_scene().instances().at(range.first).color
                    == fixture.host->theme().text.secondary
                && fixture.scene.text_state(target.scene).counters().shape_count
                    == target_initial.shape_count
                && fixture.scene.text_state(target.scene).counters().measure_count
                    == target_initial.measure_count
                && fixture.scene.text_state(sibling.scene).counters().shape_count
                    == sibling_initial.shape_count
                && fixture.scene.atlas().dirty_regions().empty(),
            "secondary tone changed shape, measurement, sibling, or atlas state");
    fixture.scene.glyph_scene().instances().clear_dirty_ranges();

    content.set(ryn::String{u8"状态 B 中文"});
    require(fixture.host->components().mount_runs() == 1
                && fixture.dirty.layout_roots()
                    == std::vector<ryn::runtime::NodeId>{fixture.scene.node(target.scene)},
            "content Prop reran mount or missed target layout invalidation");
    require(fixture.layout_texts(), "content update did not synchronize");
    require(fixture.scene.text_state(target.scene).counters().shape_count
                    == target_initial.shape_count + 1
                && fixture.scene.text_state(target.scene).counters().measure_count
                    == target_initial.measure_count + 1
                && fixture.scene.text_state(sibling.scene).counters().shape_count
                    == sibling_initial.shape_count,
            "content update escaped the target Text state");

    const auto shape_before_margin =
        fixture.scene.text_state(target.scene).counters().shape_count;
    const auto measure_before_margin =
        fixture.scene.text_state(target.scene).counters().measure_count;
    margin.set(ryn::dp(12.0F));
    require(fixture.dirty.placement_roots()
                    == std::vector<ryn::runtime::NodeId>{fixture.scene.node(target.scene)}
                && fixture.dirty.layout_roots().empty(),
            "margin Prop update did not stay in placement");
    require(fixture.layout_texts(), "margin update did not synchronize");
    require(fixture.scene.text_state(target.scene).counters().shape_count
                    == shape_before_margin
                && fixture.scene.text_state(target.scene).counters().measure_count
                    == measure_before_margin
                && fixture.nodes.require(fixture.scene.node(target.scene)).bounds.x == 24.0F,
            "margin update reshaped/remeasured Text or missed Node placement");
}

void test_shaped_measurement_wrap_resize_and_translation() {
    Fixture fixture;
    ryn::Signal<ryn::LogicalLength> width{ryn::auto_length};
    fixture.host->mount(ryn::Content{[&] {
        ryn::LayoutStyle style;
        style.width(width)
            .margin_left(ryn::dp(8.0F))
            .margin_top(ryn::dp(4.0F));
        ryn::Text(ryn::TextProps{}
            .content(u8"中文文本布局测试")
            .layout(style));
    }});
    require(fixture.layout_texts(500.0F, 240.0F),
            "natural-width Text did not synchronize");
    const auto mounted = fixture.host->mounted_texts().front();
    const auto node = fixture.scene.node(mounted.scene);
    const auto& initial_state = fixture.scene.text_state(mounted.scene);
    const auto natural = initial_state.measurement();
    const auto initial_shape = initial_state.counters().shape_count;
    const auto initial_measure = initial_state.counters().measure_count;
    require(near(fixture.nodes.require(node).measured_size.width, natural.width)
                && near(fixture.nodes.require(node).measured_size.height, 22.0F)
                && fixture.nodes.require(node).bounds.x == 20.0F
                && fixture.nodes.require(node).bounds.y == 20.0F,
            "natural Text measurement or margin bounds ignored shaped metrics");

    width.set(ryn::dp(42.0F));
    require(fixture.layout_texts(500.0F, 240.0F),
            "finite-width Text did not synchronize");
    const auto& wrapped_state = fixture.scene.text_state(mounted.scene);
    require(wrapped_state.counters().shape_count == initial_shape
                && wrapped_state.counters().measure_count == initial_measure + 1
                && wrapped_state.measurement().lines.size() > 1
                && fixture.nodes.require(node).measured_size.width == 42.0F
                && fixture.scene.primitive(mounted.scene).instances.count
                    == wrapped_state.shaped().glyphs.size(),
            "finite width reshaped Text or lost cluster-aligned glyphs");

    width.set(ryn::auto_length);
    require(fixture.layout_texts(120.0F, 240.0F),
            "viewport resize did not synchronize Text");
    const auto measure_after_resize =
        fixture.scene.text_state(mounted.scene).counters().measure_count;
    const auto shape_after_resize =
        fixture.scene.text_state(mounted.scene).counters().shape_count;
    require(measure_after_resize == initial_measure + 2
                && shape_after_resize == initial_shape,
            "viewport resize did not remeasure without reshaping");

    ryn::runtime::NodePropertyWriter writer(fixture.nodes, fixture.dirty);
    require(writer.set_translation(node, {9.0F, 6.0F}),
            "Text translation update was ignored");
    fixture.scene.glyph_scene().instances().clear_dirty_ranges();
    fixture.scene.atlas().clear_dirty_regions();
    require(fixture.layout_texts(120.0F, 240.0F),
            "translated Text did not synchronize");
    const auto& translated = fixture.scene.text_state(mounted.scene);
    const auto glyph = fixture.scene.glyph_scene().instances()
        .at(fixture.scene.primitive(mounted.scene).instances.first);
    require(translated.counters().shape_count == shape_after_resize
                && translated.counters().measure_count == measure_after_resize
                && fixture.scene.atlas().dirty_regions().empty()
                && near(glyph.translation_opacity[0], 2.0F * 9.0F / 120.0F)
                && near(glyph.translation_opacity[1], -2.0F * 6.0F / 240.0F),
            "translation reshaped/remeasured Text or missed Glyph geometry");

    Fixture latin;
    latin.host->mount(ryn::Content{[] {
        ryn::Text(ryn::TextProps{}
            .content(u8"Latin words wrap safely")
            .layout(ryn::LayoutStyle{}.width(ryn::dp(64.0F))));
    }});
    require(latin.layout_texts(320.0F, 240.0F),
            "finite-width Latin Text did not synchronize");
    const auto latin_text = latin.host->mounted_texts().front();
    require(latin.scene.text_state(latin_text.scene).measurement().lines.size() > 1
                && latin.scene.text_state(latin_text.scene).counters().shape_count == 1,
            "Latin Text did not wrap at shaped legal boundaries");
}

void test_semantic_foreground_context_is_nested_reactive_and_scoped() {
    Fixture fixture;
    const ryn::runtime::SemanticForeground outer_initial{
        0.10F, 0.20F, 0.30F, 1.0F};
    const ryn::runtime::SemanticForeground outer_next{
        0.70F, 0.20F, 0.10F, 1.0F};
    const ryn::runtime::SemanticForeground inner_initial{
        0.25F, 0.75F, 0.40F, 1.0F};
    const ryn::runtime::SemanticForeground inner_next{
        0.80F, 0.60F, 0.10F, 1.0F};
    ryn::Signal<ryn::runtime::SemanticForeground> outer{outer_initial};
    ryn::Signal<ryn::runtime::SemanticForeground> inner{inner_initial};
    ryn::runtime::ComponentId provider;

    fixture.host->mount(ryn::Content{[&] {
        provider = mount_foreground_provider(
            fixture.layout,
            ryn::Prop<ryn::runtime::SemanticForeground>{outer},
            ForegroundContent{[&] {
                ryn::Text(u8"outer inherited");
                static_cast<void>(mount_foreground_provider(
                    fixture.layout,
                    ryn::Prop<ryn::runtime::SemanticForeground>{inner},
                    ForegroundContent{[] {
                        ryn::Text(u8"inner inherited");
                        ryn::Text(ryn::TextProps{}
                            .content(u8"explicit secondary")
                            .tone(ryn::TextTone::Secondary));
                    }}));
                ryn::Text(u8"outer restored");
            }});
        ryn::Text(u8"outside default");
    }});
    require(fixture.layout_texts(),
            "semantic foreground Texts did not layout/synchronize");
    require(fixture.host->mounted_texts().size() == 5
                && mounted_color(fixture, 0) == outer_initial
                && mounted_color(fixture, 1) == inner_initial
                && mounted_color(fixture, 2) == fixture.host->theme().text.secondary
                && mounted_color(fixture, 3) == outer_initial
                && mounted_color(fixture, 4) == fixture.host->theme().text.primary,
            "nested semantic foreground resolution or explicit tone precedence failed");

    std::array<std::uint64_t, 5> shape_counts{};
    std::array<std::uint64_t, 5> measure_counts{};
    for (std::size_t index = 0; index < fixture.host->mounted_texts().size(); ++index) {
        const auto scene = fixture.host->mounted_texts()[index].scene;
        const auto counters = fixture.scene.text_state(scene).counters();
        shape_counts[index] = counters.shape_count;
        measure_counts[index] = counters.measure_count;
    }
    fixture.dirty.clear();
    fixture.scene.glyph_scene().instances().clear_dirty_ranges();
    static_cast<void>(fixture.frames.consume_request());

    outer.set(outer_next);
    const auto outer_first_node = fixture.scene.node(
        fixture.host->mounted_texts()[0].scene);
    const auto outer_second_node = fixture.scene.node(
        fixture.host->mounted_texts()[3].scene);
    require(fixture.dirty.material_nodes()
                    == std::vector<ryn::runtime::NodeId>{
                        outer_first_node,
                        outer_second_node}
                && fixture.dirty.layout_roots().empty(),
            "semantic foreground update escaped the inherited Glyph Material nodes");
    require(fixture.layout_texts(),
            "reactive semantic foreground did not synchronize");
    require(mounted_color(fixture, 0) == outer_next
                && mounted_color(fixture, 1) == inner_initial
                && mounted_color(fixture, 2) == fixture.host->theme().text.secondary
                && mounted_color(fixture, 3) == outer_next
                && mounted_color(fixture, 4) == fixture.host->theme().text.primary,
            "reactive semantic foreground updated an explicit or unrelated Text");
    for (std::size_t index = 0; index < fixture.host->mounted_texts().size(); ++index) {
        const auto scene = fixture.host->mounted_texts()[index].scene;
        const auto counters = fixture.scene.text_state(scene).counters();
        require(counters.shape_count == shape_counts[index]
                    && counters.measure_count == measure_counts[index],
                "semantic foreground update reshaped or remeasured Text");
    }

    const auto inner_component = fixture.host->mounted_texts()[1].component;
    require(fixture.host->destroy(inner_component),
            "semantic foreground child could not be destroyed");
    fixture.dirty.clear();
    static_cast<void>(fixture.frames.consume_request());
    inner.set(inner_next);
    require(clean(fixture.dirty) && !fixture.frames.pending(),
            "destroyed semantic foreground child retained its subscription");

    require(fixture.host->destroy(provider),
            "semantic foreground provider subtree could not be destroyed");
    fixture.dirty.clear();
    static_cast<void>(fixture.frames.consume_request());
    outer.set(outer_initial);
    require(clean(fixture.dirty) && !fixture.frames.pending()
                && fixture.host->mounted_texts().size() == 1,
            "disposed semantic foreground parent retained child subscriptions");
}

void test_semantic_foreground_context_restores_after_exception() {
    Fixture throwing;
    bool observed = false;
    try {
        throwing.host->mount(ryn::Content{[&] {
            static_cast<void>(mount_foreground_provider(
                throwing.layout,
                ryn::Prop<ryn::runtime::SemanticForeground>{
                    ryn::runtime::SemanticForeground{1.0F, 0.0F, 0.0F, 1.0F}},
                ForegroundContent{[] {
                    ryn::Text(u8"partial");
                    throw std::runtime_error("foreground slot failure");
                }}));
        }});
    } catch (const std::runtime_error&) {
        observed = true;
    }
    require(observed
                && throwing.host->components().component_count() == 0
                && throwing.scene.size() == 0,
            "throwing semantic foreground slot leaked partial component state");

    Fixture recovered;
    recovered.host->mount(ryn::Content{[] { ryn::Text(u8"recovered"); }});
    require(recovered.layout_texts()
                && mounted_color(recovered, 0) == recovered.host->theme().text.primary,
            "semantic foreground build stack was not restored after exception");
}

void test_static_loading_layout_keeps_cjk_text_and_idle_state() {
    Fixture fixture;
    const auto& token = fixture.host->theme().button;
    const ryn::layout::HorizontalContentLayout idle{
        token.middle.control_height,
        token.middle.padding_inline,
        token.border_width,
        token.content_gap,
        false,
        token.loading_indicator_size,
    };
    auto loading = idle;
    loading.loading = true;
    ryn::runtime::ComponentId container;
    fixture.host->mount(ryn::Content{[&] {
        container = mount_horizontal_provider(
            fixture.layout,
            idle,
            ForegroundContent{[] {
                ryn::Text(u8"确定");
                ryn::Text(u8"Stable sibling");
            }});
    }});
    require(synchronize_horizontal_texts(fixture, container),
            "CJK horizontal content did not synchronize");
    const auto first = fixture.host->mounted_texts()[0];
    const auto sibling = fixture.host->mounted_texts()[1];
    const auto first_root = fixture.host->components().root(first.component);
    const auto sibling_root = fixture.host->components().root(sibling.component);
    const auto first_counters = fixture.scene.text_state(first.scene).counters();
    const auto sibling_counters = fixture.scene.text_state(sibling.scene).counters();
    const auto idle_first_bounds = fixture.nodes.require(first_root).bounds;
    static_cast<void>(fixture.frames.consume_request());
    fixture.dirty.clear();

    const auto container_root = fixture.host->components().root(container);
    fixture.layout.set_layout(container_root, loading);
    fixture.dirty.invalidate(
        container_root,
        ryn::runtime::DirtyFlags::Measure
            | ryn::runtime::DirtyFlags::Layout
            | ryn::runtime::DirtyFlags::Geometry);
    require(fixture.frames.consume_request(),
            "loading layout update did not request its one required frame");
    require(synchronize_horizontal_texts(fixture, container),
            "static loading CJK content did not synchronize");
    const auto loading_geometry = fixture.layout.horizontal_content_geometry(
        container_root);
    require(loading_geometry.loading_indicator_bounds.has_value()
                && loading_geometry.loading_indicator_bounds->width
                    == token.loading_indicator_size
                && fixture.host->components().mount_runs() == 1
                && fixture.host->components().root(first.component) == first_root
                && fixture.host->components().root(sibling.component) == sibling_root
                && fixture.nodes.require(first_root).bounds.x
                    > idle_first_bounds.x,
            "static loading geometry remounted content or missed local placement");
    require(fixture.scene.text_state(first.scene).counters().shape_count
                    == first_counters.shape_count
                && fixture.scene.text_state(first.scene).counters().measure_count
                    == first_counters.measure_count
                && fixture.scene.text_state(sibling.scene).counters().shape_count
                    == sibling_counters.shape_count
                && fixture.scene.text_state(sibling.scene).counters().measure_count
                    == sibling_counters.measure_count
                && !fixture.frames.pending(),
            "static loading reshaped unchanged Text or left an animation frame pending");

    fixture.layout.set_layout(container_root, idle);
    fixture.dirty.invalidate(
        container_root,
        ryn::runtime::DirtyFlags::Measure
            | ryn::runtime::DirtyFlags::Layout
            | ryn::runtime::DirtyFlags::Geometry);
    require(fixture.frames.consume_request()
                && synchronize_horizontal_texts(fixture, container)
                && !fixture.layout.horizontal_content_geometry(container_root)
                    .loading_indicator_bounds.has_value()
                && fixture.host->components().root(first.component) == first_root
                && fixture.host->components().root(sibling.component) == sibling_root
                && !fixture.frames.pending(),
            "static loading removal changed content identity or idle state");
}

} // namespace

int main() {
    try {
        test_mount_owner_thread_and_dispose_lifecycle();
        test_reactive_content_tone_and_margin_are_minimal();
        test_shaped_measurement_wrap_resize_and_translation();
        test_semantic_foreground_context_is_nested_reactive_and_scoped();
        test_semantic_foreground_context_restores_after_exception();
        test_static_loading_layout_keeps_cjk_text_and_idle_state();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
