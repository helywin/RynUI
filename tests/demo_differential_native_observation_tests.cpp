#include "gallery_demo_scenario.hpp"
#include "gallery_demo_hash.hpp"
#include "component/selection_component.hpp"
#include "support/input_fixture.hpp"
#if defined(RYNUI_DEMO_RECORDING_OBSERVATION)
#include "renderer/common/scene_resources.hpp"
#include "renderer/recording/recording_renderer.hpp"
#endif

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace rynui::example;
using Json = GalleryDemoJson;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <typename Function> void rejects(Function action, const char* message) {
    bool rejected{};
    try {
        action();
    } catch (const std::exception&) {
        rejected = true;
    }
    require(rejected, message);
}

struct Files final {
    std::filesystem::path path =
        std::filesystem::path{RYNUI_GALLERY_SOURCE_ROOT} / "out" /
        ("native-observation-unit-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    Files() {
        require(!std::filesystem::exists(path), "unit output must be fresh");
        std::filesystem::create_directories(path);
    }

    ~Files() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

Json metrics() {
    return Json::Object{
        {"logical_width", 1440}, {"logical_height", 900}, {"pixel_width", 1440}, {"pixel_height", 900}, {"dpr", 1}};
}

Json provenance() {
    return Json::Object{{"source", Json::Object{{"source_hash", std::string(64, 'a')}}},
                        {"binary_sha256", std::string(64, 'b')},
                        {"test_layer", "CPU-mock-not-GPU"}};
}

GalleryDemoCaptureEvidence evidence(std::uint64_t id = 1) {
    return {id,
            id,
            7,
            11,
            1,
            metrics(),
            "BGRA8_UNORM",
            "BGRA",
            "RGBA",
            "premultiplied",
            "native-encoded-SDR",
            true,
            std::string(64, 'a'),
            std::string(64, 'b')};
}

void capture_controls() {
    const Json fixture = Json::Object{{"viewport", Json::Object{{"dpr", "native"}}}};
    const auto validate = [&](const auto& value, std::uint64_t previous = 0) {
        gallery_demo_validate_capture(value, previous, 7, metrics(), fixture, provenance());
    };
    validate(evidence());
    validate(evidence(2), 1); // Same scene, independently completed new capture.
    rejects([&] { validate(evidence(1), 1); }, "old frame/fence was accepted");
    auto value = evidence();
    value.scene_revision = 6;
    rejects([&] { validate(value); }, "old scene was accepted");
    value = evidence();
    value.fence_completed = false;
    rejects([&] { validate(value); }, "save success without fence evidence was accepted");
    value = evidence();
    std::get<Json::Object>(value.metrics.value)["dpr"] = 2;
    rejects([&] { validate(value); }, "wrong actual DPR was accepted");
    value = evidence();
    value.source_channel_order = "RGBA";
    rejects([&] { validate(value); }, "wrong source channel order was accepted");
    value = evidence();
    value.source_hash = std::string(64, 'c');
    rejects([&] { validate(value); }, "source/binary disagreement was accepted");
    value = evidence();
    value.binary_sha256 = std::string(64, 'c');
    rejects([&] { validate(value); }, "wrong actual binary was accepted");
    auto forced_dpr = fixture;
    std::get<Json::Object>(std::get<Json::Object>(forced_dpr.value).at("viewport").value)["dpr"] = 1.25;
    rejects([&] { gallery_demo_validate_capture(evidence(), 0, 7, metrics(), forced_dpr, provenance()); },
            "requested DPR was silently rewritten to native");
}

#if defined(RYNUI_DEMO_RECORDING_OBSERVATION)
void recording_controls() {
    ryn::detail::RecordingRenderer backend;
    ryn::detail::SceneResources resources{backend};
    graphics::GlyphAtlas atlas;
    graphics::GlyphInstanceStore glyphs;
    graphics::OrderedScene scene;
    const ryn::detail::SceneDeviceMetrics first{100, 80, 1.25F};
    require(resources.synchronize({nullptr, atlas, glyphs, nullptr, first, nullptr, &scene}),
            "Recording scene upload failed");
    const auto attachment = resources.attach(scene);
    require(backend.attach_scene(attachment) && attachment.metrics() == first &&
                attachment.scene_revision() == scene.revision(),
            "Recording attachment omitted committed metrics/revision");
    scene.clear(); // Empty scene still has a new draw revision, independent of resource counts.
    require(!backend.valid_attachment(attachment), "old Recording draw revision was accepted");
    const ryn::detail::SceneDeviceMetrics second{100, 80, 2.0F};
    require(resources.synchronize({nullptr, atlas, glyphs, nullptr, second, nullptr, &scene}),
            "Recording metrics update failed");
    const auto updated = resources.attach(scene);
    require(updated.metrics() == second && updated.metrics() != first && backend.attach_scene(updated),
            "same-extent wrong-DPR upload retained old scene metrics");
    resources.retire();
    require(!backend.valid_attachment(updated), "retired Recording upload attachment remained valid");
}
#endif

void source_controls() {
    Files files;
    const auto source = files.path / "owned.cpp";
    std::ofstream(source) << "original source\n";
    const Json manifest = Json::Object{
        {"source_hash", std::string(64, 'a')},
        {"files", Json::Array{Json::Object{{"path", "owned.cpp"}, {"sha256", gallery_demo_file_sha256(source)}}}}};
    gallery_demo_verify_source_files(manifest, files.path);
    std::ofstream(source) << "dirty source, same binary\n";
    rejects([&] { gallery_demo_verify_source_files(manifest, files.path); },
            "dirty source without rebuild was accepted");
}

GalleryDemoScenario font_scenario() {
    GalleryDemoScenario scenario;
    Json::Array files;
    for (const auto& [identity, path] :
         {std::pair{"latin", RYNUI_VALIDATION_LATIN_FONT}, std::pair{"cjk", RYNUI_VALIDATION_CJK_FONT}}) {
        files.emplace_back(Json::Object{{"identity", identity},
                                        {"path", path},
                                        {"sha256", gallery_demo_file_sha256(path)},
                                        {"face_index", 0},
                                        {"weight", 400}});
    }
    auto declarations = files;
    for (auto& declaration : declarations) {
        std::get<Json::Object>(declaration.value).erase("path");
    }
    scenario.font_files = files;
    scenario.definition = Json::Object{{"fixture", Json::Object{{"font_policy", "pinned"}, {"fonts", declarations}}}};
    return scenario;
}

void font_controls() {
    auto scenario = font_scenario();
    gallery_demo_validate_font_chain(scenario);
    auto font = scenario.font_files.array().front();
    std::get<Json::Object>(font.value)["source"] = "pinned-file";
    gallery_demo_validate_font_source(scenario, font, false);
    std::get<Json::Object>(font.value)["source"] = "system-file";
    rejects([&] { gallery_demo_validate_font_source(scenario, font, false); },
            "wrong font-source marker was accepted despite matching bytes");
    std::get<Json::Object>(font.value)["source"] = "pinned-file";
    std::get<Json::Object>(font.value)["face_index"] = 1;
    rejects([&] { gallery_demo_validate_font_source(scenario, font, false); }, "wrong collection face was accepted");
    std::get<Json::Object>(std::get<Json::Array>(scenario.font_files.value).front().value)["sha256"] =
        std::string(64, '0');
    rejects([&] { gallery_demo_validate_font_chain(scenario); }, "wrong pinned file identity was accepted");
}

struct Sink final : animation::AnimationTargetSink {
    void apply(animation::AnimationId, animation::AnimationTargetId, const animation::AnimationValue&,
               animation::AnimationDirtyDomain) override {}
};

void scope_controls() {
    animation::AnimationRuntime runtime;
    Sink sink;
    const auto shell = runtime.create_scope();
    const auto sample = runtime.create_scope();
    const auto play = [&](auto scope) {
        const auto target = runtime.register_target(scope, sink, animation::AnimationValueKind::scalar,
                                                    animation::AnimationDirtyDomain::material);
        return runtime.play(target, 0.0F, 1.0F, {{}, animation::AnimationDuration::microseconds(100000)}, {});
    };
    static_cast<void>(play(shell));
    static_cast<void>(play(sample));
    require(runtime.size(sample) == 1 && runtime.next_deadline(sample), "sample scope deadline missing");
    require(runtime.dispose_scope(sample), "sample disposal failed");
    require(runtime.size(shell) == 1 && runtime.next_deadline(shell), "legal shell animation was retired");
    rejects([&] { static_cast<void>(runtime.next_deadline(sample)); }, "retired scope generation reported idle");
    const auto replacement = runtime.create_scope();
    require(replacement != sample && !runtime.contains(sample), "reused scope accepted old generation");
}

struct Fixture final : ryn_test::input_component::Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    ryn::detail::SelectionComponentHost selections{services};
    GalleryPagesHost pages{surfaces, inputs, frames};

    Fixture() {
        pages.mount({1440, 900});
        pages.dispatch(input::WindowInputEvent{input::WindowInputAction::focus_gained});
    }

    void sync() {
        require(pages.synchronize({1440, 900}) && !pages.snapshot().failed, "real Gallery CPU synchronization failed");
    }
};

void semantic_and_retirement_controls() {
    Files files;
    const auto context = std::make_shared<GalleryDemoSampleContext>();
    context->fixture = Json::Object{{"data", Json::Object{}}};
    GalleryDemoSampleScope sample_scope{context};
    Fixture fixture;
    auto scenario = font_scenario();
    scenario.nonce = "mock-observation-run";
    scenario.identity = "antd665.button.observation-unit.v1";
    scenario.fault = "none";
    scenario.route = {GalleryPageKind::component, "ant.component.button", {}, "gallery.sample.button.basics"};
    auto& definition = std::get<Json::Object>(scenario.definition.value);
    definition["native"] = Json::Object{{"targets", Json::Array{"gallery.state.default"}}};
    auto& configuration = std::get<Json::Object>(definition.at("fixture").value);
    configuration["data"] = Json::Object{};
    configuration["props"] =
        Json::Object{{"controls", Json::Object{{"gallery.state.default", Json::Object{{"component", "Button"}}}}}};
    configuration["parent_policy"] = "accept";
    configuration["viewport"] = Json::Object{{"dpr", "native"}};
    definition["checkpoints"] = Json::Array{Json::Object{{"targets", Json::Array{"gallery.state.default", "sample"}}},
                                            Json::Object{{"targets", Json::Array{}}}};
    scenario.actions = {{"initial", "checkpoint", "sample", "observer", 0, Json::Object{}, Json::Object{}},
                        {"leave", "navigate", "sample", "native-core-dispatch", 1,
                         Json::Object{{"route", "overview"}, {"sample_id", ""}}, Json::Object{}},
                        {"retired", "checkpoint", "sample", "observer", 2, Json::Object{}, Json::Object{}}};
    Sink sink;
    const auto shell = fixture.buttons.animations().create_scope();
    const auto shell_target = fixture.buttons.animations().register_target(
        shell, sink, animation::AnimationValueKind::scalar, animation::AnimationDirtyDomain::material);
    static_cast<void>(fixture.buttons.animations().play(shell_target, 0.0F, 1.0F,
                                                        {{}, animation::AnimationDuration::microseconds(100000)}, {}));
    std::uint64_t capture_id{};
    GalleryDemoNativeActions ports{
        fixture.buttons,
        fixture.inputs,
        fixture.pages,
        context,
        [&] { fixture.sync(); },
        [](auto) {},
        [] { return std::uint64_t{7}; },
        metrics,
        [&](const auto& path) {
            // Mock transport receipt only. No GPU pixels or actual SDL fence are claimed by this test.
            std::ofstream(path, std::ios::binary) << "mock-readback";
            return evidence(++capture_id);
        },
        [&](font::FontIdentity identity) -> Json {
            const auto source = fixture.fonts->source(identity);
            require(source.has_value(), "actual text font generation is stale");
            const auto bytes =
                std::span{reinterpret_cast<const unsigned char*>(source->bytes.data()), source->bytes.size()};
            return Json::Object{
                {"sha256", gallery_demo_sha256(bytes)},
                {"face_index", static_cast<std::int64_t>(source->face_index)},
                {"source", fixture.scene.is_bundled_icon_font(identity) ? "bundled-icon" : "pinned-file"}};
        },
        [&](font::FontIdentity identity, char32_t scalar) {
            return fixture.fonts->glyph_index(identity, scalar).glyph.glyph_id;
        }};
    const auto path = files.path / "observations.jsonl";
    run_gallery_demo_scenario(scenario, path, provenance(), std::move(ports));
    std::ifstream input(path);
    Json initial;
    Json retired;
    for (std::string line; std::getline(input, line);) {
        const auto row = Json::parse(line);
        if (row.at("type").string() == "checkpoint") {
            (row.at("step").string() == "initial" ? initial : retired) = row;
        }
    }
    const auto& target = initial.at("targets").array().front();
    require(target.at("key").string() == "gallery.state.default" &&
                target.at("state").at("key_source").string() == "static-semantic-target",
            "stable target key missing");
    require(std::holds_alternative<std::nullptr_t>(target.at("state").at("selection").value),
            "non-editor default selection was fabricated");
    require(!target.at("text").array().empty() && target.at("tokens").contains("padding_inline"),
            "actual Button text/effective tokens missing");
    const auto& lifetime = retired.at("resources").at("lifecycle");
    require(lifetime.at("retired").boolean() && lifetime.at("live_components").integer() == 0 &&
                lifetime.at("shell_owner_present").boolean() && fixture.buttons.animations().contains(shell),
            "sample owner-local retirement confused legal shell/shared caches with a leak");
    require(lifetime.at("window_deadline_us").number() > 0, "legal shell deadline was hidden");
    require(retired.at("resources").at("readback").at("readback_id").integer() == 2,
            "unchanged scene reused the prior capture receipt");
    require(std::holds_alternative<std::nullptr_t>(retired.at("resources").at("presented").value),
            "mock/readback fabricated presentation knowledge");
}
} // namespace

int main() {
    try {
        capture_controls();
#if defined(RYNUI_DEMO_RECORDING_OBSERVATION)
        recording_controls();
#endif
        source_controls();
        font_controls();
        scope_controls();
        semantic_and_retirement_controls();
        std::cout << "Native observation CPU/mock contracts passed; GPU/readback execution is not covered\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
