#include "gallery_page_acceptance.hpp"
#include "gallery_sample_registry.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace rynui::example {
namespace {
int integer(std::string_view text, int minimum, int maximum) {
    int value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value < minimum || value > maximum) {
        throw std::invalid_argument("Gallery page numeric argument is outside its allowed range");
    }
    return value;
}

std::string json_string(std::string_view value) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : value) {
        switch (c) {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (c < 0x20) {
                out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << unsigned(c) << std::dec;
            } else {
                out << static_cast<char>(c);
            }
        }
    }
    out << '"';
    return out.str();
}

GalleryPageRoute route(std::string_view value) {
    if (value == "overview") {
        return {};
    }
    if (value == "tokens") {
        return {GalleryPageKind::tokens};
    }
    if (value == "introduction") {
        return {GalleryPageKind::introduction};
    }
    GalleryPageRoute result{GalleryPageKind::component};
    const auto separator = value.find('/');
    const auto identity = value.substr(0, separator);
    result.component_identity =
        identity.starts_with("ant.component.") ? std::string{identity} : "ant.component." + std::string{identity};
    if (separator != std::string_view::npos) {
        if (result.component_identity != "ant.component.input") {
            throw std::invalid_argument("Only the Input page accepts a family suffix");
        }
        const auto family = value.substr(separator + 1);
        constexpr std::array<std::string_view, 5> names{"input", "search", "password", "text-area", "otp"};
        const auto found = std::ranges::find(names, family);
        if (found == names.end()) {
            throw std::invalid_argument("Unknown Gallery Input family");
        }
        result.family = static_cast<GalleryInputFamily>(found - names.begin());
    }
    if (!GalleryPageModel{}.valid(result)) {
        throw std::invalid_argument("Unknown Gallery page route");
    }
    return result;
}
} // namespace

GalleryPageOptions gallery_page_options(std::span<const std::string_view> arguments,
                                        const std::filesystem::path& repository) {
    GalleryPageOptions result;
    std::filesystem::path output = repository / "out/gallery-pages/page-acceptance.json";
    bool acceptance_argument{};
    for (const auto argument : arguments) {
        if (argument == "--page-acceptance") {
            result.acceptance = true;
        } else if (argument == "--page-visual") {
            result.visual = true;
            acceptance_argument = true;
        } else if (argument == "--page-i18n") {
            result.i18n = true;
            acceptance_argument = true;
        } else if (argument.starts_with("--locale=")) {
            result.locale = ryn::LocaleId{argument.substr(9)};
        } else if (argument.starts_with("--format-locale=")) {
            result.format_locale = ryn::LocaleId{argument.substr(16)};
        } else if (argument.starts_with("--direction=")) {
            const auto direction = argument.substr(12);
            if (direction == "ltr") {
                result.direction = ryn::TextDirection::LeftToRight;
            } else if (direction == "rtl") {
                result.direction = ryn::TextDirection::RightToLeft;
            } else if (direction == "auto") {
                result.direction = ryn::TextDirection::Auto;
            } else {
                throw std::invalid_argument("--direction requires ltr, rtl or auto");
            }
        } else if (argument == "--motion-disabled") {
            result.motion_disabled = true;
        } else if (argument.starts_with("--page=")) {
            result.pages.push_back(route(argument.substr(7)));
            acceptance_argument = true;
        } else if (argument.starts_with("--page-output=")) {
            output = std::filesystem::path{argument.substr(14)};
            acceptance_argument = true;
        } else if (argument.starts_with("--page-width=")) {
            result.width = integer(argument.substr(13), 360, 3840);
            acceptance_argument = true;
        } else if (argument.starts_with("--page-timeout=")) {
            result.timeout_seconds = integer(argument.substr(15), 1, 120);
            acceptance_argument = true;
        } else if (argument.starts_with("--sample=")) {
            if (result.pages.empty()) {
                throw std::invalid_argument("--sample requires a preceding --page");
            }
            auto& page = result.pages.back();
            page.sample_identity = argument.substr(9);
            const auto found = std::ranges::find_if(gallery_samples(), [&page](const auto& sample) {
                return sample.identity == page.sample_identity && sample.kind == page.kind &&
                       sample.component_identity == page.component_identity && sample.family == page.family;
            });
            if (found == gallery_samples().end() || !GalleryPageModel{}.valid(page)) {
                throw std::invalid_argument("--sample does not belong to this page/family");
            }
            acceptance_argument = true;
        } else {
            throw std::invalid_argument("Unknown Gallery argument: " + std::string{argument});
        }
    }
    if (!result.acceptance && acceptance_argument) {
        throw std::invalid_argument("Page automation arguments require --page-acceptance");
    }
    if (result.i18n && !result.visual) {
        throw std::invalid_argument("--page-i18n requires --page-acceptance --page-visual");
    }
    if (result.acceptance) {
        const auto base = std::filesystem::weakly_canonical(repository / "out");
        const auto resolved = std::filesystem::weakly_canonical(output.is_absolute() ? output : repository / output);
        const auto relative = resolved.lexically_relative(base);
        if (relative.empty() || relative == "." || *relative.begin() == ".." || resolved.extension() != ".json") {
            throw std::invalid_argument("--page-output must be a JSON file inside this repository's out directory");
        }
        result.output = resolved;
        if (result.pages.empty()) {
            for (const auto name : {"overview", "input", "select", "time-picker", "tree-select", "modal", "upload",
                                    "input/password", "input/otp", "button", "tokens"}) {
                result.pages.push_back(route(name));
            }
        }
    }
    return result;
}

void write_gallery_page_acceptance(const std::filesystem::path& output, const GalleryPageAcceptanceResult& r) {
    if (output.empty()) {
        return;
    }
    std::filesystem::create_directories(output.parent_path());
    std::ofstream out(output, std::ios::binary | std::ios::trunc);
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out << "{\n  \"schema_version\": 1,\n";
    for (const auto& [name, value] :
         std::array{std::pair{"platform", r.platform}, std::pair{"preset", r.preset},
                    std::pair{"configuration", r.configuration}, std::pair{"toolchain", r.toolchain},
                    std::pair{"backend", r.backend}, std::pair{"shader", r.shader},
                    std::pair{"font_source", r.font_source}, std::pair{"error", r.error}}) {
        out << "  " << json_string(name) << ": " << json_string(value) << ",\n";
    }
    out << std::boolalpha << "  \"system_fonts\": " << r.system_fonts
        << ",\n  \"host_input_area\": " << r.host_input_area << ",\n  \"host_cursor_sync\": " << r.host_cursor_sync
        << ",\n  \"checks\": " << r.checks << ",\n  \"submitted_frames\": " << r.submitted_frames
        << ",\n  \"cases\": " << r.cases << ",\n  \"passed\": " << r.passed << "\n}\n";
}
} // namespace rynui::example
