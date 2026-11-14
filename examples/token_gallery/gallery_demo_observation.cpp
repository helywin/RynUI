#include "gallery_demo_observation.hpp"
#include "gallery_demo_hash.hpp"
#include "component/button_component.hpp"
#include "component/flex_component.hpp"
#include "component/space_component.hpp"
#include "icons/bundled_icon_catalog.hpp"
#include "theme/theme_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <tuple>

namespace rynui::example {
namespace {
using Json = GalleryDemoJson;

bool hash(const std::string& value) {
    return value.size() == 64 &&
           std::ranges::all_of(value, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

bool same_face(const Json& left, const Json& right) {
    return left.at("sha256").string() == right.at("sha256").string() &&
           left.at("face_index").integer() == right.at("face_index").integer();
}
} // namespace

void gallery_demo_validate_capture(const GalleryDemoCaptureEvidence& evidence, std::uint64_t previous_readback,
                                   std::uint64_t revision, const Json& metrics, const Json& fixture,
                                   const Json& provenance) {
    if (evidence.readback_id <= previous_readback || !evidence.fence_id || !evidence.fence_completed ||
        !evidence.renderer_owner || !evidence.device_epoch || evidence.scene_revision != revision) {
        throw std::runtime_error("checkpoint has stale/missing renderer frame or completed fence evidence");
    }
    for (const auto field : {"pixel_width", "pixel_height", "dpr", "logical_width", "logical_height"}) {
        if (evidence.metrics.at(field).number() != metrics.at(field).number()) {
            throw std::runtime_error("checkpoint readback and actual window metrics differ");
        }
    }
    const auto dpr = metrics.at("dpr").number();
    if (!std::isfinite(dpr) || dpr <= 0 || metrics.at("pixel_width").integer() <= 0 ||
        metrics.at("pixel_height").integer() <= 0 ||
        std::abs(metrics.at("logical_width").number() * dpr - metrics.at("pixel_width").number()) > .01 ||
        std::abs(metrics.at("logical_height").number() * dpr - metrics.at("pixel_height").number()) > .01) {
        throw std::runtime_error("checkpoint metrics have inconsistent logical/physical extent or DPR");
    }
    const auto& viewport = fixture.at("viewport");
    for (const auto axis : {"width", "height"}) {
        if (viewport.contains(axis) &&
            std::abs(metrics.at(std::string{"logical_"} + axis).number() - viewport.at(axis).number()) > .001) {
            throw std::runtime_error("checkpoint actual logical viewport differs from declared fixture");
        }
    }
    const auto& requested = viewport.at("dpr");
    if ((std::holds_alternative<std::string>(requested.value) && requested.string() != "native") ||
        (!std::holds_alternative<std::string>(requested.value) && requested.number() != dpr)) {
        throw std::runtime_error("requested DPR differs from actual native DPR");
    }
    if ((evidence.framebuffer_format != "RGBA8_UNORM" && evidence.framebuffer_format != "BGRA8_UNORM") ||
        (evidence.source_channel_order != "RGBA" && evidence.source_channel_order != "BGRA") ||
        evidence.channel_order != "RGBA" || evidence.alpha != "premultiplied" ||
        (evidence.framebuffer_format == "BGRA8_UNORM") != (evidence.source_channel_order == "BGRA") ||
        evidence.color_profile != "native-encoded-SDR") {
        throw std::runtime_error("checkpoint has unsupported or unknown framebuffer encoding");
    }
    if (!hash(evidence.source_hash) || !hash(evidence.binary_sha256) ||
        evidence.source_hash != provenance.at("source").at("source_hash").string() ||
        evidence.binary_sha256 != provenance.at("binary_sha256").string()) {
        throw std::runtime_error("checkpoint source/binary provenance differs from hello");
    }
}

void gallery_demo_validate_font_chain(const GalleryDemoScenario& scenario) {
    const auto& fixture = scenario.definition.at("fixture");
    const auto policy = fixture.at("font_policy").string();
    if (policy != "pinned" && policy != "system") {
        throw std::invalid_argument("native font policy is unknown");
    }
    if (policy == "system") {
        return; // The host resolves its ordinary system chain; files are not injected.
    }
    const auto& declared = fixture.at("fonts").array();
    if (declared.empty() || declared.size() != scenario.font_files.array().size()) {
        throw std::invalid_argument("pinned font declarations and explicit font files differ");
    }
    std::set<std::tuple<std::string, std::int64_t, std::int64_t>> faces;
    for (std::size_t i = 0; i < declared.size(); ++i) {
        const auto& expected = declared[i];
        const auto& supplied = scenario.font_files.array()[i];
        if (!hash(supplied.at("sha256").string()) || !same_face(expected, supplied) ||
            expected.at("identity").string() != supplied.at("identity").string() ||
            expected.at("weight").integer() != supplied.at("weight").integer() ||
            supplied.at("face_index").integer() < 0 || supplied.at("weight").integer() < 1 ||
            supplied.at("weight").integer() > 1000 || supplied.at("path").string().empty() ||
            !faces
                 .emplace(supplied.at("identity").string(), supplied.at("face_index").integer(),
                          supplied.at("weight").integer())
                 .second) {
            throw std::invalid_argument("pinned font identity/hash/face/weight/file mismatch");
        }
    }
}

void gallery_demo_validate_font_source(const GalleryDemoScenario& scenario, const Json& font, bool icon) {
    if (!hash(font.at("sha256").string()) || font.at("face_index").integer() < 0) {
        throw std::runtime_error("actual font source bytes/face are unknown");
    }
    if (icon) {
        if (font.at("source").string() != "bundled-icon") {
            throw std::runtime_error("icon text is not using the actual bundled icon font");
        }
        return;
    }
    const auto policy = scenario.definition.at("fixture").at("font_policy").string();
    if (policy == "pinned") {
        if (font.at("source").string() != "pinned-file" ||
            std::ranges::none_of(scenario.font_files.array(),
                                 [&](const auto& file) { return same_face(file, font); })) {
            throw std::runtime_error("pinned text used an undeclared actual face or incorrect font source marker");
        }
    } else if (font.at("source").string() != "system-file" ||
               std::ranges::none_of(scenario.definition.at("fixture").at("fonts").array(),
                                    [&](const auto& file) { return same_face(file, font); })) {
        throw std::runtime_error("system font profile used a custom/bundled/unobservable text face");
    }
}

void gallery_demo_verify_source_files(const Json& manifest, const std::filesystem::path& repository) {
    if (!hash(manifest.at("source_hash").string()) || manifest.at("files").array().empty()) {
        throw std::runtime_error("embedded native source closure is absent");
    }
    const auto root = std::filesystem::weakly_canonical(repository);
    std::string previous;
    for (const auto& file : manifest.at("files").array()) {
        const auto name = file.at("path").string();
        const auto path = std::filesystem::weakly_canonical(root / name);
        const auto relative = path.lexically_relative(root);
        if (name.empty() || name <= previous || relative.empty() || *relative.begin() == ".." ||
            !hash(file.at("sha256").string()) || gallery_demo_file_sha256(path) != file.at("sha256").string()) {
            throw std::runtime_error("native source changed without rebuilding the binary closure: " + name);
        }
        previous = name;
    }
}

namespace {
using namespace ryn;

Json retained_text(detail::ButtonComponentHost& app, runtime::ComponentId id) {
    for (const auto& text : app.text().mounted_texts()) {
        if (text.component != id) {
            continue;
        }
        try {
            const auto icon = app.text().icon_snapshot(id);
            if (const auto name = icon.source.bundled_name()) {
                return Json::Object{{"icon", std::string{detail::bundled_icon_entry(*name).name}}};
            }
        } catch (const std::out_of_range&) {
        }
        return std::string{std::as_const(app.text().scene_service()).text_state(text.scene).content().bytes()};
    }
    return {};
}

Json flex_props(const layout::FlexLayout& model) {
    const char* align = model.align == layout::FlexAlign::center     ? "center"
                        : model.align == layout::FlexAlign::end      ? "end"
                        : model.align == layout::FlexAlign::stretch  ? "stretch"
                        : model.align == layout::FlexAlign::baseline ? "baseline"
                                                                     : "start";
    return Json::Object{{"vertical", model.direction == layout::FlexDirection::vertical},
                        {"gap", model.main_gap},
                        {"cross_gap", model.cross_gap},
                        {"wrap", model.wrap != layout::FlexWrap::no_wrap},
                        {"align", align}};
}

std::string hex_color(Color color) {
    const char* digits = "0123456789abcdef";
    std::string value{"#"};
    for (const auto channel : {color.red(), color.green(), color.blue()}) {
        const auto byte = static_cast<unsigned>(std::lround(channel * 255));
        value.push_back(digits[(byte >> 4) & 15]);
        value.push_back(digits[byte & 15]);
    }
    return value;
}
} // namespace

Json gallery_demo_retained_content(detail::ButtonComponentHost& app, runtime::ComponentId root,
                                   std::optional<runtime::ComponentId> exclude) {
    Json::Array result;
    const auto visit = [&](auto&& self, runtime::ComponentId id) -> void {
        if (exclude == id || !app.components().contains(id)) {
            return;
        }
        const auto text = retained_text(app, id);
        if (!std::holds_alternative<std::nullptr_t>(text.value)) {
            result.push_back(text);
        }
        for (const auto child : app.components().children(id)) {
            self(self, child);
        }
    };
    visit(visit, root);
    return result;
}

Json gallery_demo_retained_tree(detail::ButtonComponentHost& app, std::span<const runtime::ComponentId> roots,
                                const std::vector<std::pair<std::string, runtime::ComponentId>>& targets,
                                std::optional<runtime::ComponentId> origin,
                                std::span<const std::pair<std::string, runtime::ComponentId>> feedback_targets) {
    const auto visit = [&](auto&& self, runtime::ComponentId id) -> Json {
        for (const auto& [identity, target] : feedback_targets) {
            if (id == target) {
                const auto content = retained_text(app, id);
                if (!std::holds_alternative<std::string>(content.value)) {
                    throw std::runtime_error("authored feedback target is not retained text");
                }
                return Json::Object{{"component", "source-feedback"}, {"identity", identity}, {"content", content}};
            }
        }
        if (origin == id) {
            return Json::Object{{"component", "origin"}};
        }
        for (const auto& [name, target] : targets) {
            if (id == target) {
                return Json::Object{{"component", "control"}, {"target", name}};
            }
        }
        Json::Array children;
        for (const auto child : app.components().children(id)) {
            children.push_back(self(self, child));
        }
        const auto text = retained_text(app, id);
        if (std::holds_alternative<std::string>(text.value)) {
            return Json::Object{{"component", "Typography.Text"}, {"content", text}};
        }
        const auto& store = std::as_const(app.components());
        if (const auto* state = store.state<detail::SpaceComponentState>(id)) {
            Json::Object result{{"component", "Space"}, {"props", flex_props(state->model)}, {"children", children}};
            if (const auto width = app.nodes().require(state->node).external_layout.width) {
                result.emplace("width", *width);
            }
            return result;
        }
        if (const auto* state = store.state<detail::FlexComponentState>(id)) {
            return Json::Object{{"component", "Flex"}, {"props", flex_props(state->model)}, {"children", children}};
        }
        return Json::Object{{"component", "retained"}, {"children", children}};
    };
    Json::Array result;
    for (const auto root : roots) {
        result.push_back(visit(visit, root));
    }
    Json host;
    if (!roots.empty()) {
        if (const auto parent = app.components().parent(roots.front())) {
            if (const auto* state = std::as_const(app.components()).state<detail::FlexComponentState>(*parent)) {
                host = flex_props(state->model);
            }
        }
    }
    return Json::Object{
        {"source", "retained component tree and applied layout models"}, {"host", host}, {"roots", result}};
}

Json gallery_demo_applied_theme(detail::ButtonComponentHost& app, runtime::ComponentId id) {
    const auto& scope = *app.components().theme_scope(id);
    const auto& snapshot = scope.snapshot();
    Json::Array algorithms;
    for (const auto algorithm : snapshot.algorithms()) {
        algorithms.emplace_back(algorithm == ThemeAlgorithm::Dark      ? "dark"
                                : algorithm == ThemeAlgorithm::Compact ? "compact"
                                                                       : "default");
    }
    const auto& config = scope.applied_config();
    Json::Object select{{"algorithm", config.select.algorithm}};
    if (config.select.seed.color_primary) {
        select.emplace("colorPrimary", hex_color(*config.select.seed.color_primary));
    }
    if (config.select.tokens.option_height) {
        select.emplace("optionHeight", config.select.tokens.option_height->value());
    }
    return Json::Object{{"source", "retained ThemeScope applied config and resolved snapshot"},
                        {"algorithms", algorithms},
                        {"colorBgContainer", hex_color(snapshot.alias().color_background_container)},
                        {"Select", select},
                        {"select_option_height", snapshot.select().metrics.option_height}};
}
} // namespace rynui::example
