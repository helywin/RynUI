#include "gallery_demo_observation.hpp"
#include "gallery_demo_hash.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

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
    const auto& requested = fixture.at("viewport").at("dpr");
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
    std::set<std::string> identities;
    for (std::size_t i = 0; i < declared.size(); ++i) {
        const auto& expected = declared[i];
        const auto& supplied = scenario.font_files.array()[i];
        if (!hash(supplied.at("sha256").string()) || !same_face(expected, supplied) ||
            expected.at("identity").string() != supplied.at("identity").string() ||
            expected.at("weight").integer() != supplied.at("weight").integer() ||
            supplied.at("face_index").integer() < 0 || supplied.at("weight").integer() < 1 ||
            supplied.at("weight").integer() > 1000 || supplied.at("path").string().empty() ||
            !identities.insert(supplied.at("identity").string()).second) {
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
} // namespace rynui::example
