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

} // namespace

int main() {
    try {
        test_mount_owner_thread_and_dispose_lifecycle();
        test_reactive_content_tone_and_margin_are_minimal();
        test_shaped_measurement_wrap_resize_and_translation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
