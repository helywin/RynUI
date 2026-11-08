#include "renderer/common/scene_resources.hpp"
#include "renderer/recording/recording_renderer.hpp"
#include "support/input_fixture.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::detail;
using namespace ryn::graphics;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void last_image_and_weight_retry() {
    ryn_test::input_component::Fixture f;
    f.services.mount(Content{[] { Input(InputProps{}.defaultValue(u8"Actual glyph bytes and selection")); }});
    f.synchronize();
    RecordingRenderer renderer;
    SceneResources resources(renderer);
    ImageStore images;
    std::array<std::byte, 16> bytes;
    bytes.fill(std::byte{255});
    ImageInstance image{ImagePixels::copy(2, 2, 8, bytes), {0, 0, 10, 10}, {0, 0, 320, 240}, {}};
    const auto id = images.create(image);
    OrderedScene scene = f.services.scene_composer().ordered_scene();
    scene.barrier();
    scene.append_command({SceneDrawKind::image, id.index, 1, id.generation});
    const auto sync = [&] {
        return resources.synchronize({&f.services.surfaces().instances(),
                                      f.scene.atlas(),
                                      f.scene.glyph_scene().instances(),
                                      &f.services.rounded_effects(),
                                      {320, 240, 1},
                                      &images,
                                      &scene});
    };
    const auto draw = [&] {
        require(renderer.attach_scene(resources.attach(scene)) &&
                    renderer.submit_frame({}) == runtime::FrameSubmissionResult::submitted,
                "real four-store draw");
    };
    require(sync(), "cold four-store sync");
    draw();
    const auto cold_resources = renderer.live_resources();
    auto program = std::make_shared<SceneCompositionProgram>();
    program->scene_revision = scene.revision();
    const runtime::Rect full{0, 0, 320, 240};
    program->requests = {{{1, 1}, {{2, 2, 100, 80}, 8}, full}, {{2, 1}, {full, 0}, full, true, 4, {0, 0, 0, .2F}, 1}};
    program->operations = {{SceneCompositionOpKind::BeginMaskedGroup, 0, 0, 0},
                           {SceneCompositionOpKind::DrawRange, 0, static_cast<std::uint32_t>(scene.commands().size())},
                           {SceneCompositionOpKind::EndMaskedGroup, 0, 0, 0},
                           {SceneCompositionOpKind::Backdrop, 0, 0, 1}};
    scene.set_composition(program);
    require(sync(), "composition four-store sync");
    draw();
    std::array<bool, 4> seen{};
    for (const auto& d : renderer.draws()) {
        seen[static_cast<unsigned>(d.command.kind)] = true;
        require(!d.instance_bytes.empty(), "empty real primitive bytes");
    }
    require(seen[0] && seen[1] && seen[2] && seen[3], "failure fixture omitted a primitive resource");
    const auto old = resources.attach(scene);
    bytes[0] = std::byte{17};
    image.pixels = ImagePixels::copy(2, 2, 8, bytes);
    require(images.update(id, image), "Image update");
    // Existing primitive dirty queues are empty: the two Image uploads precede
    // the independent weight upload. Fail its final instance buffer, not a mock.
    renderer.fail_next(RecordingFailure::upload, 1);
    bool failed = false;
    try {
        failed = !sync();
    } catch (const std::exception&) {
        failed = true;
    }
    require(failed && !renderer.valid_attachment(old), "last Image failure kept attachment");
    f.scene.glyph_scene().instances().clear_dirty_ranges();
    f.services.surfaces().instances().clear_dirty_ranges();
    f.services.rounded_effects().clear_dirty_ranges();
    const auto before = renderer.counters().uploads;
    require(sync() && renderer.counters().uploads >= before + 6,
            "zero-new-dirty retry omitted Quad/Glyph/Effect/Image/weights");
    draw();
    const auto committed = resources.attach(scene);
    renderer.fail_next(RecordingFailure::upload); // Only weights are dirty now.
    failed = false;
    try {
        failed = !sync();
    } catch (const std::exception&) {
        failed = true;
    }
    require(failed && !renderer.valid_attachment(committed), "last weight failure kept old attachment");
    f.scene.glyph_scene().instances().clear_dirty_ranges();
    f.services.surfaces().instances().clear_dirty_ranges();
    f.services.rounded_effects().clear_dirty_ranges();
    const auto retry_before = renderer.counters().uploads;
    require(sync() && renderer.counters().uploads >= retry_before + 6,
            "weight failure did not retry old and new resources");
    draw();
    scene.set_composition({});
    const auto cold_bytes = renderer.counters().uploaded_bytes;
    require(sync() && !resources.attach(scene).composition() && renderer.live_resources() == cold_resources &&
                renderer.counters().uploaded_bytes == cold_bytes,
            "disabled composition did not return to cold resources/bytes");
}
} // namespace

int main() {
    try {
        last_image_and_weight_retry();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
