#include "gallery_demo_inventory.hpp"
#include "ant_design_reference_catalog.hpp"
#include "gallery_page_model.hpp"
#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "gallery_test_inventory.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace rynui::example {
namespace {

std::string quote(std::string_view value) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : value) {
        if (c == '"' || c == '\\') {
            out << '\\' << static_cast<char>(c);
        } else if (c < 32) {
            out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
        } else {
            out << static_cast<char>(c);
        }
    }
    out << '"';
    return out.str();
}

std::string_view family_name(GalleryInputFamily family) {
    constexpr std::array<std::string_view, 5> names{"input", "search", "password", "text-area", "otp"};
    return names.at(static_cast<std::size_t>(family));
}

std::string_view kind_name(GalleryPageKind kind) {
    switch (kind) {
    case GalleryPageKind::overview:
        return "overview";
    case GalleryPageKind::introduction:
        return "introduction";
    case GalleryPageKind::tokens:
        return "tokens";
    case GalleryPageKind::component:
        return "component";
    }
    throw std::logic_error("invalid compiled Gallery page kind");
}

template <typename Range, typename Writer> void array(std::ostream& out, const Range& range, Writer write) {
    out << '[';
    bool first = true;
    for (const auto& item : range) {
        if (!first) {
            out << ',';
        }
        first = false;
        write(item);
    }
    out << ']';
}

} // namespace

std::string gallery_demo_inventory_json() {
    std::ostringstream out;
    out << "{\"schema_version\":1,\"reference_commit\":" << quote(ant_design_reference_commit())
        << ",\"catalog_hash\":" << quote(ant_design_reference_catalog_hash()) << ",\"catalog\":";
    array(out, ant_design_reference_entries(), [&](const auto& entry) {
        out << "{\"identity\":" << quote(entry.identity) << ",\"category\":" << static_cast<int>(entry.category)
            << ",\"source_path\":" << quote(entry.source_path)
            << ",\"status\":" << quote(gallery_support_status_name(entry.support_status)) << '}';
    });
    out << ",\"routes\":[";
    bool first = true;
    const auto route = [&](std::string_view identity, GalleryPageKind kind, std::string_view component,
                           GalleryInputFamily family) {
        if (!GalleryPageModel{}.valid({kind, std::string{component}, family, {}})) {
            throw std::logic_error("inventory route is not accepted by compiled Gallery model");
        }
        if (!first) {
            out << ',';
        }
        first = false;
        out << "{\"identity\":" << quote(identity) << ",\"kind\":" << quote(kind_name(kind))
            << ",\"component\":" << quote(component) << ",\"family\":" << quote(family_name(family)) << '}';
    };
    for (const auto kind : {GalleryPageKind::overview, GalleryPageKind::introduction, GalleryPageKind::tokens}) {
        route(kind_name(kind), kind, {}, GalleryInputFamily::input);
    }
    for (const auto& entry : ant_design_reference_entries()) {
        const auto slug = entry.identity.substr(std::string_view{"ant.component."}.size());
        if (entry.identity == "ant.component.input") {
            for (const auto family :
                 {GalleryInputFamily::input, GalleryInputFamily::search, GalleryInputFamily::password,
                  GalleryInputFamily::text_area, GalleryInputFamily::otp}) {
                route(std::string{slug} + '/' + std::string{family_name(family)}, GalleryPageKind::component,
                      entry.identity, family);
            }
        } else {
            route(slug, GalleryPageKind::component, entry.identity, GalleryInputFamily::input);
        }
    }
    out << "],\"samples\":";
    array(out, gallery_samples(), [&](const auto& sample) {
        out << "{\"identity\":" << quote(sample.identity) << ",\"component\":" << quote(sample.component_identity)
            << ",\"family\":" << quote(family_name(sample.family)) << ",\"kind\":" << quote(kind_name(sample.kind))
            << ",\"source_function\":" << quote(sample.source_function)
            << ",\"public_header\":" << quote(sample.public_header) << '}';
    });
    out << ",\"sources\":";
    array(out, gallery_sample_sources(), [&](const auto& source) {
        out << "{\"identity\":" << quote(source.identity) << ",\"source_path\":" << quote(source.source_path)
            << ",\"source_function\":" << quote(source.source_function)
            << ",\"public_header\":" << quote(source.public_header) << ",\"code\":" << quote(source.cpp) << '}';
    });
    out << ",\"targets\":";
    array(out, gallery_legacy_test_targets(), [&](const auto& target) {
        out << "{\"identity\":" << quote(target.identity) << ",\"sample\":" << quote(target.sample_identity)
            << ",\"alias\":" << quote(target.replacement) << '}';
    });
    out << "}\n";
    return out.str();
}

std::optional<int> run_gallery_demo_inventory(int argc, char** argv, const std::filesystem::path& repository) {
    bool requested = false;
    std::string_view argument;
    for (int index = 1; index < argc; ++index) {
        if (std::string_view{argv[index]}.starts_with("--page-inventory")) {
            requested = true;
            argument = argv[index];
        }
    }
    if (!requested) {
        return std::nullopt;
    }
    if (argc != 2 || !argument.starts_with("--page-inventory=")) {
        throw std::invalid_argument("--page-inventory=<out/file.json> is an exclusive no-window mode");
    }
    const auto root = std::filesystem::weakly_canonical(repository / "out");
    const auto candidate = std::filesystem::path{argument.substr(17)};
    const auto output = std::filesystem::weakly_canonical(candidate.is_absolute() ? candidate : repository / candidate);
    const auto relative = output.lexically_relative(root);
    if (relative.empty() || relative == "." || *relative.begin() == ".." || output.extension() != ".json") {
        throw std::invalid_argument("inventory output must be a JSON file strictly beneath this checkout out/");
    }
    if (std::filesystem::exists(output)) {
        throw std::invalid_argument("inventory output already exists; use a fresh output path");
    }
    std::filesystem::create_directories(output.parent_path());
    std::ofstream stream(output, std::ios::binary);
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream << gallery_demo_inventory_json();
    return 0;
}

} // namespace rynui::example
