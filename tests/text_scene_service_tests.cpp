#include "renderer/sdl/glyph_gpu_resources.hpp"
#include "text/text_scene_service.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class RecordingGpuApi final : public ryn::detail::GlyphGpuApi {
public:
    void* create_glyph_sampler() override { return handle(next_++); }
    void* create_glyph_texture(std::uint32_t, std::uint32_t) override {
        return handle(next_++);
    }
    void* create_glyph_buffer(std::size_t) override { return handle(next_++); }
    bool upload_glyph_texture(
        void*,
        const ryn::detail::GlyphTextureUpload&) override {
        ++texture_uploads;
        return true;
    }
    bool upload_glyph_buffer(
        void*,
        std::size_t,
        std::span<const std::byte>) override {
        ++buffer_uploads;
        return true;
    }
    void release_glyph_buffer(void*) noexcept override {}
    void release_glyph_texture(void*) noexcept override {}
    void release_glyph_sampler(void*) noexcept override {}
    const char* glyph_gpu_error() const noexcept override { return ""; }

    static void* handle(std::uintptr_t value) {
        return reinterpret_cast<void*>(value);
    }

    std::uintptr_t next_{1};
    std::size_t texture_uploads{};
    std::size_t buffer_uploads{};
};

struct Fixture final {
    Fixture()
        : fonts(create_runtime()),
          engine(*fonts),
          service(*fonts, engine, frames) {
        const auto latin = fonts->load_font_file(
            RYNUI_VALIDATION_LATIN_FONT, 0, 14);
        const auto cjk = fonts->load_font_file(
            RYNUI_VALIDATION_CJK_FONT, 0, 14);
        require(latin && cjk, "Text scene service fonts failed to load");
        chain = {latin.font, cjk.font};
    }

    static std::unique_ptr<ryn::font::FontRuntime> create_runtime() {
        auto created = ryn::font::FontRuntime::create();
        require(static_cast<bool>(created), "Font Runtime initialization failed");
        return std::move(created.runtime);
    }

    ryn::detail::TextSceneId create(ryn::String content) {
        return service.create(
            nodes.create_root(),
            std::move(content),
            chain,
            14,
            {20.0F, std::numeric_limits<float>::infinity()});
    }

    static ryn::graphics::GlyphPlacement placement(float x) {
        return {
            {x, 24.0F},
            {640.0F, 360.0F},
            {0.0F, 0.0F, 640.0F, 360.0F},
            {0.0F, 0.0F},
            {},
            1.0F,
        };
    }

    ryn::runtime::NodeStore nodes;
    std::unique_ptr<ryn::font::FontRuntime> fonts;
    ryn::text::TextEngine engine;
    ryn::runtime::FrameRequestState frames;
    ryn::detail::TextSceneService service;
    std::vector<ryn::font::FontIdentity> chain;
};

void test_multiple_texts_share_font_atlas_scene_and_upload_plan() {
    Fixture fixture;
    const auto first = fixture.create(ryn::String{u8"Shared 中"});
    const auto second = fixture.create(ryn::String{u8"Shared 中"});
    require(fixture.service.size() == 2
                && fixture.service.declaration_order(first) == 0
                && fixture.service.declaration_order(second) == 1,
            "Text records did not preserve declaration order");
    require(fixture.service.set_placement(first, Fixture::placement(16.0F))
                && fixture.service.set_placement(second, Fixture::placement(180.0F)),
            "initial Text placements were ignored");

    RecordingGpuApi api;
    ryn::detail::GlyphGpuResources resources(api);
    require(fixture.service.synchronize(first),
            "first shared Text did not synchronize");
    const auto rasterizations = fixture.fonts->counters().rasterizations;
    const auto entries = fixture.service.atlas().entry_count();
    require(rasterizations != 0
                && entries != 0
                && !fixture.service.atlas().dirty_regions().empty(),
            "first Text did not populate the shared atlas");
    resources.synchronize(
        fixture.service.atlas(),
        fixture.service.glyph_scene().instances());
    const auto texture_uploads = api.texture_uploads;

    require(fixture.service.synchronize(second),
            "second shared Text did not synchronize");
    require(fixture.fonts->counters().rasterizations == rasterizations
                && fixture.service.atlas().entry_count() == entries
                && fixture.service.atlas().dirty_regions().empty(),
            "second Text repeated shared glyph raster or atlas work");
    resources.synchronize(
        fixture.service.atlas(),
        fixture.service.glyph_scene().instances());
    require(api.texture_uploads == texture_uploads,
            "second Text repeated a texture upload for shared glyphs");

    const auto& first_primitive = fixture.service.primitive(first);
    const auto& second_primitive = fixture.service.primitive(second);
    require(first_primitive.instances.first == 0
                && first_primitive.instances.count != 0
                && second_primitive.instances.first
                    == first_primitive.instances.count
                && second_primitive.instances.count
                    == first_primitive.instances.count,
            "shared Text instance ranges lost declaration order");
    require(fixture.service.node(first) != fixture.service.node(second)
                && fixture.service.ordered_scene().commands().front().first_instance == 0,
            "Text records lost independent Node or ordered Scene state");
}

void test_destroy_compacts_ranges_and_rejects_stale_generations() {
    Fixture fixture;
    const auto first = fixture.create(ryn::String{u8"A"});
    const auto middle = fixture.create(ryn::String{u8"B"});
    const auto last = fixture.create(ryn::String{u8"C"});
    static_cast<void>(fixture.service.set_placement(first, Fixture::placement(10.0F)));
    static_cast<void>(fixture.service.set_placement(middle, Fixture::placement(30.0F)));
    static_cast<void>(fixture.service.set_placement(last, Fixture::placement(50.0F)));
    require(fixture.service.synchronize_all(),
            "three Text records did not synchronize");
    const auto first_range = fixture.service.primitive(first).instances;
    const auto middle_range = fixture.service.primitive(middle).instances;
    const auto last_range = fixture.service.primitive(last).instances;
    require(first_range.count == 1 && middle_range.count == 1 && last_range.count == 1,
            "single-glyph Text fixtures did not create one instance each");
    const auto first_uv = fixture.service.glyph_scene().instances()
        .at(first_range.first).uv_rect;
    const auto last_uv = fixture.service.glyph_scene().instances()
        .at(last_range.first).uv_rect;
    fixture.service.glyph_scene().instances().clear_dirty_ranges();

    require(fixture.service.destroy(middle)
                && !fixture.service.contains(middle)
                && fixture.service.contains(first)
                && fixture.service.contains(last),
            "destroying the middle Text damaged surviving identities");
    require(fixture.service.primitive(first).instances == first_range
                && fixture.service.primitive(last).instances
                    == ryn::graphics::GlyphInstanceRange{1, 1}
                && fixture.service.glyph_scene().instances().at(1).uv_rect == last_uv,
            "middle Text deletion did not compact and remap the trailing range");
    require(fixture.service.glyph_scene().instances().geometry_dirty_ranges().size() == 1
                && fixture.service.glyph_scene().instances().geometry_dirty_ranges().front()
                    == ryn::graphics::GlyphInstanceRange{1, 1},
            "middle Text deletion dirtied the wrong compacted range");
    fixture.service.glyph_scene().instances().clear_dirty_ranges();

    require(fixture.service.destroy(first)
                && fixture.service.primitive(last).instances
                    == ryn::graphics::GlyphInstanceRange{0, 1}
                && fixture.service.glyph_scene().instances().at(0).uv_rect == last_uv
                && first_uv != last_uv,
            "first Text deletion did not preserve the last Text UV and identity");
    require(fixture.service.glyph_scene().instances().geometry_dirty_ranges().size() == 1
                && fixture.service.glyph_scene().instances().geometry_dirty_ranges().front()
                    == ryn::graphics::GlyphInstanceRange{0, 1},
            "first Text deletion did not dirty the shifted surviving range");
    fixture.service.glyph_scene().instances().clear_dirty_ranges();

    require(fixture.service.destroy(last)
                && fixture.service.size() == 0
                && fixture.service.glyph_scene().instances().size() == 0
                && fixture.service.ordered_scene().commands().empty(),
            "last Text deletion did not empty the shared Scene");
    const auto replacement = fixture.create(ryn::String{u8"D"});
    require(replacement.index == last.index
                && replacement.generation != last.generation
                && !fixture.service.destroy(last)
                && fixture.service.contains(replacement),
            "Text slot reuse accepted a stale generation");
}

void test_dirty_paths_remain_per_text_and_sparse() {
    Fixture fixture;
    const auto first = fixture.create(ryn::String{u8"A"});
    const auto middle = fixture.create(ryn::String{u8"B"});
    const auto last = fixture.create(ryn::String{u8"C"});
    static_cast<void>(fixture.service.set_placement(first, Fixture::placement(10.0F)));
    static_cast<void>(fixture.service.set_placement(middle, Fixture::placement(30.0F)));
    static_cast<void>(fixture.service.set_placement(last, Fixture::placement(50.0F)));
    require(fixture.service.synchronize_all(),
            "dirty-path Text fixtures did not synchronize");
    fixture.service.atlas().clear_dirty_regions();
    fixture.service.glyph_scene().instances().clear_dirty_ranges();
    const auto first_initial = fixture.service.text_state(first).counters();
    const auto middle_initial = fixture.service.text_state(middle).counters();
    const auto last_initial = fixture.service.text_state(last).counters();

    require(fixture.service.set_color(first, {1.0F, 0.0F, 0.0F, 1.0F})
                && fixture.service.set_color(last, {0.0F, 0.0F, 1.0F, 1.0F})
                && fixture.service.synchronize(first)
                && fixture.service.synchronize(last),
            "non-adjacent Text tone updates failed");
    const auto material_ranges =
        fixture.service.glyph_scene().instances().material_dirty_ranges();
    require(material_ranges.size() == 2
                && material_ranges[0] == fixture.service.primitive(first).instances
                && material_ranges[1] == fixture.service.primitive(last).instances,
            "non-adjacent tone updates expanded into a broad upload range");
    require(fixture.service.text_state(first).counters().shape_count
                    == first_initial.shape_count
                && fixture.service.text_state(last).counters().measure_count
                    == last_initial.measure_count
                && fixture.service.text_state(middle).counters().shape_count
                    == middle_initial.shape_count
                && fixture.service.record_counters(first).material_updates == 1
                && fixture.service.record_counters(last).material_updates == 1
                && fixture.service.atlas().dirty_regions().empty(),
            "tone update escaped the target Material ranges");
    fixture.service.glyph_scene().instances().clear_dirty_ranges();

    auto moved = Fixture::placement(30.0F);
    moved.translation_pixels = {7.0F, 3.0F};
    require(fixture.service.set_placement(middle, moved)
                && fixture.service.synchronize(middle),
            "Text placement update failed");
    require(fixture.service.glyph_scene().instances().geometry_dirty_ranges().size() == 1
                && fixture.service.glyph_scene().instances().geometry_dirty_ranges().front()
                    == fixture.service.primitive(middle).instances
                && fixture.service.record_counters(middle).geometry_updates == 1
                && fixture.service.text_state(middle).counters().shape_count
                    == middle_initial.shape_count
                && fixture.service.text_state(middle).counters().measure_count
                    == middle_initial.measure_count,
            "placement update escaped geometry-only work");
    fixture.service.glyph_scene().instances().clear_dirty_ranges();

    require(fixture.service.set_width_constraint(first, 8.0F)
                && fixture.service.synchronize(first),
            "Text constraint update failed");
    require(fixture.service.text_state(first).counters().shape_count
                    == first_initial.shape_count
                && fixture.service.text_state(first).counters().measure_count
                    == first_initial.measure_count + 1
                && fixture.service.record_counters(first).instance_rebuilds == 1
                && fixture.service.glyph_scene().instances()
                    .geometry_dirty_ranges().empty()
                && fixture.service.atlas().dirty_regions().empty(),
            "constraint update escaped measure/layout-only work");
    require(fixture.service.synchronize(first, Fixture::placement(10.0F))
                && fixture.service.record_counters(first).geometry_updates == 1
                && fixture.service.text_state(first).counters().measure_count
                    == first_initial.measure_count + 1,
            "post-constraint placement did not update geometry without remeasure");
    fixture.service.glyph_scene().instances().clear_dirty_ranges();

    const auto rasterizations = fixture.fonts->counters().rasterizations;
    require(fixture.service.set_content(last, ryn::String{u8"D"})
                && fixture.service.synchronize(last),
            "Text content update failed");
    require(fixture.service.text_state(last).counters().shape_count
                    == last_initial.shape_count + 1
                && fixture.service.text_state(last).counters().measure_count
                    == last_initial.measure_count + 1
                && fixture.fonts->counters().rasterizations > rasterizations
                && !fixture.service.atlas().dirty_regions().empty()
                && fixture.service.text_state(middle).counters().shape_count
                    == middle_initial.shape_count,
            "content update did not stay on the target shape/measure/atlas path");
    require(fixture.service.revisions(first).layout == 2
                && fixture.service.revisions(middle).placement == 2
                && fixture.service.revisions(last).content == 2
                && fixture.service.revisions(first).tone == 2
                && fixture.service.revisions(last).tone == 2,
            "per-Text content, tone, constraint, or placement revision was lost");
}

void test_scroll_translation_preserves_glyphs() {
    Fixture fixture;
    const auto source = fixture.create(ryn::String{u8"Scrolling retained text"});
    const auto view = fixture.service.create_view(source, fixture.nodes.create_root());
    auto placement = Fixture::placement(10);
    require(fixture.service.synchronize(source, placement)
        && fixture.service.synchronize(view, placement), "scroll fixture failed");
    const auto initial = fixture.service.primitive(view).instances;
    const auto glyph = fixture.service.glyph_scene().instances().at(initial.first);
    const auto rebuilds = fixture.service.counters().ordered_scene_rebuilds;
    const auto rasterizations = fixture.fonts->counters().rasterizations;
    for (int index = 0; index < 20000; ++index) {
        fixture.service.glyph_scene().instances().clear_dirty_ranges();
        const float scroll = static_cast<float>(index % 200);
        fixture.service.set_scroll_translation(view, {-scroll, 0});
        require(fixture.service.synchronize(view), "scroll synchronization failed");
        const auto& instance = fixture.service.glyph_scene().instances().at(initial.first);
        require(instance.position_size == glyph.position_size && instance.uv_rect == glyph.uv_rect
            && instance.clip_bounds == glyph.clip_bounds
            && instance.translation_opacity[0] == -2.0F * scroll / placement.viewport_pixels.width,
            "scroll changed glyph shape, raster phase, or clip");
    }
    require(fixture.service.primitive(view).instances == initial
        && fixture.service.counters().ordered_scene_rebuilds == rebuilds
        && fixture.fonts->counters().rasterizations == rasterizations
        && fixture.service.text_state(source).counters().shape_count == 1,
        "scroll rebuilt retained text or atlas");
    placement.clip_pixels.width *= 0.5F;
    require(fixture.service.synchronize(view, placement), "scroll clip update failed");
    const auto offset = fixture.service.glyph_scene().instances().at(initial.first).translation_opacity[0];
    fixture.service.set_content(source, ryn::String{u8"Replacement scrolling content"});
    require(fixture.service.synchronize(view), "scroll source replacement failed");
    require(fixture.service.glyph_scene().instances().at(fixture.service.primitive(view).instances.first)
        .translation_opacity[0] == offset, "content replacement lost scroll translation");
    bool rejected{};
    try { fixture.service.set_scroll_translation(view, {std::numeric_limits<float>::infinity(), 0}); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "nonfinite scroll accepted");
}

void test_shared_shape_views() {
    Fixture fixture;
    const auto source = fixture.create(ryn::String{u8"Shared 中"});
    const auto selected = fixture.service.create_view(source, fixture.nodes.create_root());
    const auto placeholder = fixture.service.create_view(source, fixture.nodes.create_root());
    const std::array<float, 4> red{1, 0, 0, 1}, blue{0, 0, 1, 1};
    fixture.service.set_color(source, blue);
    fixture.service.set_color(selected, red);
    fixture.service.set_opacity(placeholder, 0);
    for(const auto id : {source, selected, placeholder})
        require(fixture.service.synchronize(id, Fixture::placement(10)), "shared view synchronization failed");
    const auto& read = std::as_const(fixture.service);
    require(&read.text_state(source) == &read.text_state(selected)
        && read.text_state(source).counters().shape_count == 1, "text views duplicated shaping");
    const auto& instances = read.glyph_scene().instances();
    require(instances.at(read.primitive(source).instances.first).color == blue
        && instances.at(read.primitive(selected).instances.first).color == red
        && instances.at(read.primitive(placeholder).instances.first).translation_opacity[2] == 0,
        "text views shared mutable material");
    const auto source_range = read.primitive(source).instances;
    fixture.service.set_opacity(selected, 0.5F);
    require(fixture.service.synchronize(selected), "selected view material failed");
    require(read.primitive(source).instances == source_range && read.text_state(source).counters().shape_count == 1,
        "view material reshaped source");
    bool rejected{};
    try { fixture.service.set_content(selected, ryn::String{u8"illegal"}); } catch(const std::logic_error&) { rejected = true; }
    require(rejected, "view changed shared text");
    fixture.service.set_content(source, ryn::String{u8"Changed longer text 中文"});
    require(fixture.service.synchronize(selected) && fixture.service.synchronize(source)
        && fixture.service.synchronize(placeholder), "views did not follow source revision");
    require(read.text_state(source).counters().shape_count == 2
        && read.primitive(selected).instances.count == read.primitive(source).instances.count,
        "view source update duplicated shape or lost glyph range");
    require(fixture.service.destroy(source) && fixture.service.synchronize(selected), "view lost owned shaping lifetime");
    const auto replacement = fixture.create(ryn::String{u8"New owner"});
    require(source.index == replacement.index && source.generation != replacement.generation
        && read.text_state(selected).content().bytes() != read.text_state(replacement).content().bytes(),
        "source reuse replaced existing view ownership");
    require(fixture.service.destroy(selected) && fixture.service.destroy(placeholder), "view cleanup failed");
}

} // namespace

int main() {
    try {
        test_multiple_texts_share_font_atlas_scene_and_upload_plan();
        test_destroy_compacts_ranges_and_rejects_stale_generations();
        test_dirty_paths_remain_per_text_and_sparse();
        test_shared_shape_views();
        test_scroll_translation_preserves_glyphs();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
