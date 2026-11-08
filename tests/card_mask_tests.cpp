#include "component/card_component.hpp"
#include "component/image_component.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/common/composition_packing.hpp"
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

void retained_card_mask() {
    ryn_test::input_component::Fixture f;
    Signal<bool> clipped{false};
    Signal<bool> loading{false};
    std::array<std::byte, 16> rgba;
    rgba.fill(std::byte{255});
    const auto pixels = ImagePixels::copy(2, 2, 8, rgba);
    int runs = 0;
    f.services.mount(Content{[&] {
        Card(CardProps{}.clipContent(clipped).loading(loading).animated(false).layout(
                 LayoutStyle{}.width(dp(240)).height(dp(180))),
             CardContent{[&] {
                 ++runs;
                 Input(InputProps{}.defaultValue(u8"draft"));
                 Image(ImageProps{}.source(ImageSource{pixels}).layout(LayoutStyle{}.width(dp(40)).height(dp(40))));
             }});
    }});
    f.synchronize();
    require(!f.services.scene_composer().ordered_scene().composition(), "ordinary Card changed default overflow");
    const auto card = f.services.card().mounted().front();
    const auto input = f.inputs.mounted_inputs().front();
    require(f.services.focus().request_focus(input.interaction, input::FocusModality::keyboard), "input focus");
    auto& editor = f.inputs.editors().require(input.editor);
    (void)editor.set_value("dirty draft");
    (void)editor.select({8, 2});
    (void)f.inputs.dispatch(input::CompositionChanged{String{u8"候选"}, {0, 1}, f.inputs.sessions().active()});
    const auto selection = editor.selection();
    const auto session = f.inputs.sessions().active();
    const auto revision = editor.revision();
    RecordingRenderer renderer;
    SceneResources resources(renderer);
    for (const float scale : {.25F, .75F, 1.0F, 1.25F, 1.5F, 2.0F}) {
        clipped.set(true);
        f.services.set_display_scale(scale);
        f.synchronize();
        const auto& scene = f.services.scene_composer().ordered_scene();
        require(scene.composition() && runs == 1 && f.inputs.mounted_inputs().front().editor == input.editor &&
                    editor.selection() == selection && editor.revision() == revision && editor.composition().active &&
                    f.inputs.sessions().active() == session && f.inputs.focused(input.component),
                "mask/DPI update remounted content or changed focus/draft/selection/preedit");
        const auto& program = *scene.composition();
        const auto root = f.services.components().root(card);
        require(program.requests.front().owner == root && f.nodes.require(root).rounded_content_radius,
                "Card did not publish a retained CPU mask");
        bool inside = false;
        std::array<bool, 4> seen{};
        for (const auto op : program.operations) {
            if (op.kind == SceneCompositionOpKind::BeginMaskedGroup) {
                inside = true;
            }
            if (op.kind == SceneCompositionOpKind::EndMaskedGroup) {
                inside = false;
            }
            if (inside && op.kind == SceneCompositionOpKind::DrawRange) {
                for (std::uint32_t i = op.first; i < op.first + op.count; ++i) {
                    seen[static_cast<unsigned>(scene.commands()[i].kind)] = true;
                }
            }
        }
        require(seen[0] && seen[1] && seen[2] && seen[3], "Card group omitted a real Quad/Glyph/Effect/Image store");
        const SceneDeviceMetrics metrics{static_cast<std::uint32_t>(320 * scale),
                                         static_cast<std::uint32_t>(240 * scale), scale};
        require(resources.synchronize({&f.services.surfaces().instances(), f.scene.atlas(),
                                       f.scene.glyph_scene().instances(), &f.services.surfaces().effects(), metrics,
                                       &f.services.images(), &scene}),
                "Card group resource transaction");
        require(renderer.attach_scene(resources.attach(scene)) &&
                    renderer.submit_frame({}) == runtime::FrameSubmissionResult::submitted,
                "Card group real Recording interpreter");
        require(renderer.draws().size() == scene.commands().size(), "Card group lost actual draws");
        const auto bounds = f.nodes.rounded_content_clip(root).bounds;
        const auto records = f.services.hit_test().records();
        const auto entry = std::find_if(records.begin(), records.end(),
                                        [&](const auto& r) { return r.interaction == input.interaction; });
        require(entry != records.end() && entry->rounded_clips && !entry->rounded_clips->empty() &&
                    !runtime::content_clip_contains(entry->rounded_clips->front(), {bounds.x, bounds.y}),
                "rounded hit chain missing");
    }
    clipped.set(false);
    f.synchronize();
    require(!f.services.scene_composer().ordered_scene().composition() && editor.composition().active,
            "mask disable retained compositor or canceled preedit");
    loading.set(true);
    f.synchronize();
    require(bool(f.services.scene_composer().ordered_scene().composition()), "loading did not enable rounded mask");
    f.services.dispose();
    require(f.nodes.size() == 0 && f.services.interactions().size() == 0, "Card mask teardown leaked input/owner");
}

void ancestor_clip_expansion() {
    ryn_test::input_component::Fixture f;
    runtime::ComponentId ancestor;
    runtime::ComponentId card;
    float height = 120;
    f.services.mount(Content{[&] {
        auto& build = runtime::require_component_build_context();
        ancestor = build.mount_component<int>(0);
        const auto node = build.root(ancestor);
        f.nodes.require(node).clip_content = true;
        build.on_resource_cleanup(ancestor, [&f, node] { f.layout.remove_layout(node); });
        build.mount_slot(ancestor, Content{[&] {
                             Card(CardProps{}.clipContent(true).layout(LayoutStyle{}.width(dp(240)).height(dp(180))),
                                  CardContent{[] { Text(TextProps{}.content(u8"Retained clip")); }});
                         }});
        card = f.services.card().mounted().front();
        const auto child = build.root(card);
        f.layout.set_layout(node, layout::ComponentLayout{
                                      [&, child](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints c) {
                                          (void)engine.measure_child(child, {240, 240, 180, 180});
                                          return c.constrain({240, height});
                                      },
                                      [child](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect r) {
                                          engine.place_child(child, {r.x, r.y, 240, 180});
                                      }});
    }});
    f.synchronize();
    const auto root = f.services.components().root(card);
    const auto bounds = f.nodes.require(root).bounds;
    const auto commands = f.services.scene_composer().ordered_scene().commands();
    const std::vector<SceneDrawCommand> original_commands(commands.begin(), commands.end());
    std::uint32_t previous_crop{};
    for (const float extent : {120.0F, 150.0F, 179.0F}) {
        height = extent;
        f.dirty.invalidate(f.services.components().root(ancestor),
                           runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout | runtime::DirtyFlags::HitTest);
        f.synchronize();
        const auto& scene = f.services.scene_composer().ordered_scene();
        require(f.nodes.require(root).bounds == bounds && std::ranges::equal(scene.commands(), original_commands),
                "ancestor clip test changed Card geometry or retained draw topology");
        const auto& request = scene.composition()->requests.front();
        require(request.owner == root && request.hard_clip.height == extent,
                "Card retained stale ancestor clip after expansion");
        const auto plan = pack_composition(scene, {400, 300, 1.25F}, recording_scene_capabilities());
        const auto crop = plan.requests.front().crop.height;
        require(crop > previous_crop, "Card GPU crop did not expand with ancestor clip");
        previous_crop = crop;
    }
    f.services.dispose();
}
} // namespace

int main() {
    try {
        retained_card_mask();
        ancestor_clip_expansion();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
