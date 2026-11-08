#include "component/component_scene.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/recording/recording_renderer.hpp"
#include <ryn/component.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::graphics;
using namespace ryn::detail;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <class F> void rejects(F action) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("Invalid composition was accepted");
}

struct SlotTag {};

using Slot = SlotContent<SlotTag>;

void geometry(runtime::NodeStore& nodes, runtime::NodeId id, runtime::Rect rect) {
    auto& n = nodes.require(id);
    n.bounds = rect;
    n.place_generation = 1;
}

void hit_and_real_traversal() {
    runtime::NodeStore nodes;
    runtime::ComponentHost host(nodes);
    input::InteractionRegistry interactions(host, nodes);
    input::HitTestSnapshot hit(interactions, nodes);
    component::ComponentSceneComposer composer(host, interactions, hit);
    runtime::ComponentId parent;
    runtime::ComponentId child;
    runtime::ComponentId popup;
    host.mount(Content{[&] {
        auto& b = runtime::require_component_build_context();
        parent = b.mount_component<int>(0);
        b.mount_slot(parent, Slot{[&] {
                         child = runtime::require_component_build_context().mount_component<int>(0);
                         popup = runtime::require_component_build_context().mount_component<int>(0);
                     }});
    }});
    const auto root = host.root(parent);
    geometry(nodes, root, {10, 10, 40, 40});
    geometry(nodes, host.root(child), {10, 10, 40, 40});
    geometry(nodes, host.root(popup), {10, 10, 40, 40});
    nodes.require(root).rounded_content_radius = 20;
    host.set_window_layer(popup, 10);
    nodes.require(host.root(popup)).clip_boundary = true;
    const auto p = interactions.create({parent, root, {}, true, false, {}});
    const auto c = interactions.create({child, host.root(child), p, true, false, {}});
    const auto pop = interactions.create({popup, host.root(popup), p, true, false, {}});
    const auto shadow = host.register_scene_fragment(parent, runtime::SceneFragmentPlacement::before_children);
    const auto fill = host.register_scene_fragment(parent, runtime::SceneFragmentPlacement::before_children);
    const auto content = host.register_scene_fragment(child, runtime::SceneFragmentPlacement::before_children);
    const auto layer = host.register_scene_fragment(popup, runtime::SceneFragmentPlacement::before_children);
    const std::array a{SceneDrawCommand{SceneDrawKind::quad, 7, 1}};
    const std::array b{SceneDrawCommand{SceneDrawKind::quad, 8, 1}};
    composer.set_fragment(shadow, a, p);
    composer.set_fragment_composition(shadow, true);
    composer.set_fragment(fill, b);
    composer.set_fragment(content, a, c);
    composer.set_fragment(layer, b, pop);
    composer.rebuild({0, 0, 100, 100});
    const auto& scene = composer.ordered_scene();
    require(scene.commands().size() == 4 && scene.commands()[1].first_instance == 8, "barrier merged real draw ranges");
    const auto& ops = scene.composition()->operations;
    require(ops[0].kind == SceneCompositionOpKind::DrawRange &&
                ops[1].kind == SceneCompositionOpKind::BeginMaskedGroup &&
                ops[4].kind == SceneCompositionOpKind::EndMaskedGroup &&
                ops.back().kind == SceneCompositionOpKind::DrawRange,
            "root shadow/group/escaped popup order differs");
    require(hit.hit_test({10.1F, 10.1F}) == pop, "window popup did not escape rounded hit clip");
    host.set_branch_active(popup, false);
    composer.rebuild({0, 0, 100, 100});
    require(!hit.hit_test({10.1F, 10.1F}) && hit.hit_test({30, 10}) == c,
            "rounded corner/fringe hit or arc boundary differs");
    nodes.require(root).translation = {5, 3};
    const std::array dirty{root};
    hit.refresh(dirty);
    require(!hit.hit_test({30, 10}) && hit.hit_test({35, 13}) == c, "rounded hit refresh lost translation");
    const auto normalized = runtime::normalized_content_clip({{0, 0, 4, 2}, 500});
    require(normalized.radius == 1 && !runtime::content_clip_contains(normalized, {4, 1}),
            "radius clamp/half-open edge");
    rejects([] { (void)runtime::normalized_content_clip({{0, 0, 2, 2}, -1}); });
    composer.rebuild({0, 0, 100, 100});
    const auto previous = std::vector<SceneDrawCommand>{composer.ordered_scene().commands().begin(),
                                                        composer.ordered_scene().commands().end()};
    nodes.require(root).rounded_content_radius = -1;
    rejects([&] { composer.rebuild({0, 0, 100, 100}); });
    require(std::ranges::equal(previous, composer.ordered_scene().commands()) && hit.hit_test({35, 13}) == c,
            "failed CPU clip build published paint without matching hit geometry");
    nodes.require(root).rounded_content_radius = 20;
    OrderedScene visible;
    composer.build_visible_scene(nodes, {0, 0, 1, 1}, visible);
    require(visible.composition() && std::ranges::equal(visible.commands(), composer.ordered_scene().commands()),
            "visible culling broke balanced program or shadow support");
}

struct Fixture {
    RecordingRenderer backend;
    GlyphAtlas atlas;
    GlyphInstanceStore glyphs;
    RoundedEffectStore effects;
    ImageStore images;
    SceneResources resources;
    OrderedScene scene;

    explicit Fixture(SceneBackendCapabilities caps = recording_scene_capabilities())
        : backend(caps), resources(backend) {
        (void)effects.add(make_shadow_effect({{3, 3, 10, 10}, 2}, {ShadowKind::outer, {}, 0, 0, Color{1, 0, 0, .5F}}));
        (void)effects.add(make_shadow_effect({{5, 5, 10, 10}, 2}, {ShadowKind::outer, {}, 0, 0, Color{0, 1, 0, .5F}}));
        scene.append_command({SceneDrawKind::rounded_effect, 1, 1});
        program();
    }

    void program(bool blur = false) {
        auto p = std::make_shared<SceneCompositionProgram>();
        p->scene_revision = scene.revision();
        p->requests.push_back({{1, 1}, {{2.25F, 1.5F, 20, 16}, 3}, {0, 0, 32, 24}});
        if (blur) {
            p->requests.push_back({{2, 1}, {{0, 0, 32, 24}, 0}, {0, 0, 32, 24}, true, 2, {0, 0, 0, 0}, 1});
        }
        p->operations = {{SceneCompositionOpKind::BeginMaskedGroup, 0, 0, 0},
                         {SceneCompositionOpKind::DrawRange, 0, 1},
                         {SceneCompositionOpKind::EndMaskedGroup, 0, 0, 0}};
        if (blur) {
            p->operations.push_back({SceneCompositionOpKind::Backdrop, 0, 0, 1});
        }
        scene.set_composition(p);
    }

    bool sync() {
        return resources.synchronize({nullptr, atlas, glyphs, &effects, {40, 30, 1.25F}, &images, &scene});
    }

    void draw() {
        require(backend.attach_scene(resources.attach(scene)), "composition attachment rejected");
        require(backend.submit_frame({}) == runtime::FrameSubmissionResult::submitted, "composition submit failed");
    }
};

void resources_and_failures() {
    Fixture f;
    require(f.sync(), "initial sync");
    f.draw();
    require(f.backend.draws().size() == 1 && f.backend.draws()[0].command.first_instance == 1 &&
                f.backend.draws()[0].instance_bytes.size() == 112,
            "nonzero first real bytes lost");
    const auto packed = pack_rounded_effect_instance(f.effects.packed_instances()[1], {40, 30, 1.25F});
    require(std::memcmp(&packed, f.backend.draws()[0].instance_bytes.data(), 112) == 0,
            "group crop repacked full-window instance");
    const auto old = f.resources.attach(f.scene);
    f.backend.fail_next(RecordingFailure::commit);
    require(!f.sync() && !f.backend.valid_attachment(old), "commit failure did not invalidate whole attachment");
    f.glyphs.clear_dirty_ranges();
    require(f.sync(), "zero-new-dirty retry");
    f.draw();
    f.program(true);
    for (const auto failure : {RecordingFailure::begin, RecordingFailure::create, RecordingFailure::upload,
                               RecordingFailure::upload_exception, RecordingFailure::create_exception}) {
        f.backend.fail_next(failure);
        bool failed = false;
        try {
            failed = !f.sync();
        } catch (const std::exception&) {
            failed = true;
        }
        require(failed, "injected stage was not reached");
        f.glyphs.clear_dirty_ranges();
        require(f.sync(), "complete retry after injected failure");
    }
    f.program(true);
    require(f.sync(), "backdrop plan sync");
    f.draw();
    const auto attachment = f.resources.attach(f.scene);
    const auto& plan = attachment.composition()->plan;
    double sum = plan.weights[0].value[0];
    for (std::size_t i = 1; i < plan.weights.size(); ++i) {
        sum += 2 * plan.weights[i].value[0];
    }
    require(std::abs(sum - 1) < 1e-6 && plan.requests[1].weight_count == 9, "Gaussian normalization/DPI radius");
    const auto& record = f.backend.composition_records().back();
    require(record.source && record.filtered && record.source != record.target && record.filtered != record.target,
            "snapshot/filter target alias");
    f.backend.set_auto_complete_frames(false);
    f.draw();
    require(f.sync(), "second in-flight frame");
    f.draw();
    rejects([&] { f.sync(); });
    f.backend.complete_frames();
    require(f.sync(), "fence completion did not release capacity");
    const auto stale = f.resources.attach(f.scene);
    f.backend.reset_device();
    require(!f.backend.valid_attachment(stale) && f.sync(), "epoch recovery retained old handles");
}

void actual_capacity_includes_old_resize() {
    Fixture seed;
    auto caps = recording_scene_capabilities();
    const auto old_plan = pack_composition(seed.scene, {40, 30, 1.25F}, caps);
    const auto new_plan = pack_composition(seed.scene, {80, 30, 1.25F}, caps);
    caps.maximum_composition_bytes = old_plan.allocated_bytes + new_plan.allocated_bytes - 1;
    Fixture f(caps);
    f.backend.set_auto_complete_frames(false);
    require(f.sync(), "initial capacity fixture");
    f.draw();
    const auto commits = f.backend.counters().commits;
    rejects([&] {
        (void)f.resources.synchronize({nullptr, f.atlas, f.glyphs, &f.effects, {80, 30, 1.25F}, &f.images, &f.scene});
    });
    require(f.resources.next_deadline() == animation::AnimationTime::microseconds(16000),
            "fence retry is not a finite delayed ticket");
    require(f.backend.counters().commits == commits, "resize ignored old in-flight allocation or reached upload begin");
    f.backend.complete_frames();
    require(f.resources.synchronize({nullptr, f.atlas, f.glyphs, &f.effects, {80, 30, 1.25F}, &f.images, &f.scene}),
            "completed old capacity was not released for resized target");
    require(!f.resources.next_deadline(), "completed fence retained a resource deadline");
}

void unsupported_and_cold() {
    auto caps = recording_scene_capabilities();
    caps.masked_groups = false;
    Fixture unsupported(caps);
    const auto unsupported_baseline = unsupported.backend.live_resources();
    rejects([&] { unsupported.sync(); });
    require(unsupported.backend.live_resources() == unsupported_baseline && unsupported.backend.counters().commits == 0,
            "unsupported live request reached upload begin/create");
    Fixture f;
    const auto unbalanced_baseline = f.backend.live_resources();
    auto p = std::make_shared<SceneCompositionProgram>(*f.scene.composition());
    p->operations.pop_back();
    f.scene.set_composition(p);
    rejects([&] { f.sync(); });
    require(f.backend.live_resources() == unbalanced_baseline, "unbalanced program allocated resources");
    Fixture cold;
    cold.scene.set_composition({});
    require(cold.sync(), "cold scene sync");
    cold.draw();
    require(!cold.resources.attach(cold.scene).composition() && cold.backend.composition_records().empty(),
            "cold scene allocated a composition frame");
    const auto bytes = cold.backend.counters().uploaded_bytes;
    require(cold.sync() && cold.backend.counters().uploaded_bytes == bytes, "cold idle added uploads");
}

void target_reentry_and_alias_preflight() {
    Fixture f;
    auto p = std::make_shared<SceneCompositionProgram>(*f.scene.composition());
    p->operations.push_back({SceneCompositionOpKind::BeginMaskedGroup, 0, 0, 0});
    p->operations.push_back({SceneCompositionOpKind::EndMaskedGroup, 0, 0, 0});
    f.scene.set_composition(p);
    const auto plan = pack_composition(f.scene, {40, 30, 1.25F}, f.backend.capabilities());
    require(plan.targets.size() == 2, "sequential group reentry overcounted actual target storage");
    p = std::make_shared<SceneCompositionProgram>(*f.scene.composition());
    p->operations.insert(p->operations.begin() + 1, {SceneCompositionOpKind::BeginMaskedGroup, 0, 0, 0});
    p->operations.insert(p->operations.begin() + 3, {SceneCompositionOpKind::EndMaskedGroup, 0, 0, 0});
    f.scene.set_composition(p);
    const auto baseline = f.backend.live_resources();
    rejects([&] { f.sync(); });
    require(f.backend.live_resources() == baseline && f.backend.counters().commits == 0,
            "aliased nested request reached resource creation/upload");
}
} // namespace

int main() {
    try {
        hit_and_real_traversal();
        resources_and_failures();
        actual_capacity_includes_old_resize();
        unsupported_and_cold();
        target_reentry_and_alias_preflight();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    std::cout << "scene composition source contracts passed\n";
}
