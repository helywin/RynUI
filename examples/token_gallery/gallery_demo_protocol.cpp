#include "gallery_demo_protocol.hpp"
#include "gallery_sample_registry.hpp"
#include "gallery_demo_hash.hpp"
#include "ant_design_reference_catalog.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>

namespace rynui::example {
namespace {
#include "generated_gallery_demo_build.inc"

using Json = GalleryDemoJson;

void identifier(const std::string& value) {
    if (value.empty() || value.size() > 256 || !std::ranges::all_of(value, [](const unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
                   c == ':' || c == '/' || c == '-';
        })) {
        throw std::invalid_argument("invalid Gallery scenario identity");
    }
}

void parameters(const GalleryDemoAction& action) {
    const std::map<std::string_view, std::vector<std::string_view>> fields{
        {"pointer-move", {"x_fraction", "y_fraction"}},
        {"pointer-down", {"button", "x_fraction", "y_fraction"}},
        {"pointer-up", {"button", "x_fraction", "y_fraction"}},
        {"pointer-cancel", {}},
        {"key-down", {"key", "modifiers"}},
        {"key-up", {"key", "modifiers"}},
        {"text-commit", {"text"}},
        {"composition", {"phase", "text", "selection_start", "selection_end", "session"}},
        {"wheel", {"delta_x", "delta_y"}},
        {"external-props", {"values"}},
        {"async-complete", {"request", "result"}},
        {"advance-clock", {"to_us"}},
        {"navigate", {"route", "sample_id"}},
        {"checkpoint", {}},
    };
    const auto found = fields.find(action.kind);
    if (found == fields.end() || found->second.size() != action.parameters.object().size()) {
        throw std::invalid_argument("unknown Gallery scenario action or parameter");
    }
    for (const auto field : found->second) {
        static_cast<void>(action.parameters.at(field));
    }
    if (action.kind.starts_with("pointer-") && action.kind != "pointer-cancel") {
        for (const auto field : {"x_fraction", "y_fraction"}) {
            const auto value = action.parameters.at(field).number();
            if (value < 0 || value > 1) {
                throw std::invalid_argument("pointer target fractions must be in 0..1");
            }
        }
        if (action.kind != "pointer-move") {
            const auto button = action.parameters.at("button").string();
            if (button != "primary" && button != "secondary") {
                throw std::invalid_argument("unknown scenario pointer button");
            }
        }
    }
    if (action.kind.starts_with("key-")) {
        identifier(action.parameters.at("key").string());
        std::set<std::string> unique;
        for (const auto& modifier : action.parameters.at("modifiers").array()) {
            const auto& name = modifier.string();
            if ((name != "control" && name != "shift" && name != "alt" && name != "meta") ||
                !unique.insert(name).second) {
                throw std::invalid_argument("invalid scenario keyboard modifier");
            }
        }
    }
    if (action.kind == "text-commit") {
        static_cast<void>(action.parameters.at("text").string());
    }
    if (action.kind == "wheel") {
        static_cast<void>(action.parameters.at("delta_x").number());
        static_cast<void>(action.parameters.at("delta_y").number());
    }
    if (action.kind == "advance-clock" && action.parameters.at("to_us").integer() != action.time_us) {
        throw std::invalid_argument("scenario clock advance and step timestamp differ");
    }
}

std::filesystem::path owned_file(const std::filesystem::path& path, const std::filesystem::path& repository) {
    const auto file = std::filesystem::weakly_canonical(path.is_absolute() ? path : repository / path);
    const auto out = std::filesystem::weakly_canonical(repository / "out");
    const auto relative = file.lexically_relative(out);
    if (relative.empty() || relative == "." || *relative.begin() == ".." || !std::filesystem::is_regular_file(file) ||
        std::filesystem::file_size(file) > 16 * 1024 * 1024) {
        throw std::invalid_argument("scenario input must be a bounded file inside this checkout out/");
    }
    return file;
}

} // namespace

GalleryDemoScenario read_gallery_demo_scenario(const std::filesystem::path& path,
                                               const std::filesystem::path& repository) {
    std::ifstream input(owned_file(path, repository), std::ios::binary);
    const auto request = Json::parse(std::string{std::istreambuf_iterator<char>{input}, {}});
    gallery_demo_fields(request, {"schema_version", "nonce", "definition", "fault", "font_files"});
    if (request.at("schema_version").integer() != 1) {
        throw std::invalid_argument("unsupported Gallery scenario version");
    }
    GalleryDemoScenario result;
    result.nonce = request.at("nonce").string();
    identifier(result.nonce);
    result.fault = request.at("fault").string();
    if (result.fault != "none" && result.fault != "button-padding-plus-one" && result.fault != "input-reject-commit" &&
        result.fault != "duplicate-callback") {
        throw std::invalid_argument("unknown acceptance-only Gallery fault");
    }
    result.definition = request.at("definition");
    const auto& definition = result.definition;
    gallery_demo_fields(definition,
                        {"schema_version", "identity", "revision", "official", "native", "mapping", "fixture",
                         "profile", "actions", "checkpoints", "oracle", "features", "dependencies"});
    if (definition.at("schema_version").integer() != 1 || definition.at("mapping").string() != "paired" ||
        definition.at("oracle").array().empty()) {
        throw std::invalid_argument("scenario requires version-one paired definition and independent oracle");
    }
    const auto& fixture = definition.at("fixture");
    gallery_demo_fields(fixture, {"data", "props", "slots", "parent_policy", "clock_epoch_us", "seed", "timezone",
                                  "viewport", "container", "theme", "locale", "format_locale", "direction", "fonts",
                                  "font_policy", "motion"});
    const auto policy = fixture.at("parent_policy").string();
    if (policy != "accept" && policy != "reject") {
        throw std::invalid_argument("scripted parent policy has no native execution adapter yet");
    }
    result.identity = definition.at("identity").string();
    identifier(result.identity);
    const auto& native = definition.at("native");
    gallery_demo_fields(native, {"route", "family", "sample_id", "targets", "source_function", "public_header"});
    if (definition.at("official").at("commit").string() != ant_design_reference_commit() ||
        !result.identity.starts_with("antd665.") || !result.identity.ends_with(".v1")) {
        throw std::invalid_argument("scenario official source/version differs from compiled reference");
    }
    const auto route = native.at("route").string();
    result.route.kind = GalleryPageKind::component;
    result.route.component_identity = route.starts_with("ant.component.") ? route : "ant.component." + route;
    if (const auto suffix = result.route.component_identity.find('/'); suffix != std::string::npos) {
        result.route.component_identity.erase(suffix);
    }
    const std::array<std::string_view, 5> families{"input", "search", "password", "text-area", "otp"};
    const auto family = std::ranges::find(families, native.at("family").string());
    if (family == families.end()) {
        throw std::invalid_argument("unknown scenario Input family");
    }
    result.route.family = static_cast<GalleryInputFamily>(family - families.begin());
    result.route.sample_identity = native.at("sample_id").string();
    const auto sample =
        std::ranges::find(gallery_samples(), result.route.sample_identity, &GallerySampleDescriptor::identity);
    if (!GalleryPageModel{}.valid(result.route) || sample == gallery_samples().end() ||
        sample->component_identity != result.route.component_identity || sample->family != result.route.family ||
        sample->source_function != native.at("source_function").string() ||
        sample->public_header != native.at("public_header").string()) {
        throw std::invalid_argument("scenario native route/sample/source identity differs from compiled registry");
    }
    std::set<std::string> steps;
    std::int64_t previous = -1;
    for (const auto& item : definition.at("actions").array()) {
        gallery_demo_fields(item, {"step", "kind", "target", "time_us", "layer", "preconditions", "parameters"});
        GalleryDemoAction action{item.at("step").string(),  item.at("kind").string(),     item.at("target").string(),
                                 item.at("layer").string(), item.at("time_us").integer(), item.at("parameters"),
                                 item.at("preconditions")};
        identifier(action.step);
        identifier(action.target);
        if (action.time_us < 0 || action.time_us < previous || action.time_us > 120000000 ||
            !steps.insert(action.step).second) {
            throw std::invalid_argument("scenario contains duplicate/out-of-order step or timestamp");
        }
        previous = action.time_us;
        parameters(action);
        if (action.kind == "checkpoint") {
            result.checkpoints.push_back(action.step);
        }
        result.actions.push_back(std::move(action));
    }
    if (result.actions.empty() || result.actions.size() > 4096 || result.checkpoints.empty() ||
        result.checkpoints.size() > 256 || definition.at("checkpoints").array().size() != result.checkpoints.size()) {
        throw std::invalid_argument("scenario action/checkpoint set is empty or out of bounds");
    }
    for (std::size_t index = 0; index < result.checkpoints.size(); ++index) {
        const auto& checkpoint = definition.at("checkpoints").array()[index];
        gallery_demo_fields(checkpoint, {"step", "targets", "layers", "rois"});
        if (checkpoint.at("step").string() != result.checkpoints[index]) {
            throw std::invalid_argument("scenario checkpoint action/declaration order differs");
        }
    }
    result.font_files = request.at("font_files");
    if ((fixture.at("font_policy").string() == "pinned" && result.font_files.array().empty()) ||
        result.font_files.array().size() > 16) {
        throw std::invalid_argument("scenario must explicitly configure its test font chain");
    }
    for (const auto& font : result.font_files.array()) {
        gallery_demo_fields(font, {"identity", "path", "sha256", "face_index", "weight"});
        identifier(font.at("identity").string());
        if (font.at("face_index").integer() < 0 || font.at("face_index").integer() > 65535 ||
            font.at("weight").integer() < 1 || font.at("weight").integer() > 1000) {
            throw std::invalid_argument("scenario font face/weight is out of bounds");
        }
        const auto path = owned_file(font.at("path").string(), repository);
        if (gallery_demo_file_sha256(path) != font.at("sha256").string()) {
            throw std::invalid_argument("scenario font source content hash differs");
        }
    }
    return result;
}

GalleryDemoJson gallery_demo_native_build_manifest() {
    return GalleryDemoJson::parse(gallery_demo_build_manifest);
}

} // namespace rynui::example
