#include "gallery_page_acceptance.hpp"
#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace rynui::example;

int main(int argc, char** argv) {
    try {
        const std::filesystem::path root = RYNUI_GALLERY_SOURCE_ROOT;
        const auto parsed = gallery_page_options(
            std::array<std::string_view, 5>{"--page-acceptance", "--page=input/password", "--page=button",
                                            "--page-width=480", "--page-timeout=12"},
            root);
        if (parsed.pages.size() != 2 || parsed.pages.front().family != GalleryInputFamily::password ||
            parsed.width != 480 || parsed.timeout_seconds != 12) {
            throw std::runtime_error("finite page options did not preserve explicit routes/family/geometry");
        }
        const auto defaults = gallery_page_options(std::array<std::string_view, 1>{"--page-acceptance"}, root);
        const auto visual = gallery_page_options(
            std::array<std::string_view, 3>{"--page-acceptance", "--page-visual", "--page=button"}, root);
        if (!visual.visual || visual.pages.size() != 1) {
            throw std::runtime_error("hidden visual options did not preserve the selected page");
        }
        bool visual_rejected{};
        try {
            static_cast<void>(gallery_page_options(std::array<std::string_view, 1>{"--page-visual"}, root));
        } catch (const std::invalid_argument&) {
            visual_rejected = true;
        }
        if (!visual_rejected) {
            throw std::runtime_error("visual automation accidentally started an interactive product");
        }
        if (defaults.pages.size() != 11 || !defaults.output.lexically_relative(root / "out").native().size()) {
            throw std::runtime_error("default finite matrix is incomplete");
        }
        for (const auto argument :
             {"--unknown", "--page=missing", "--page=button/otp", "--page=button/input", "--page-width=0",
              "--page-timeout=999", "--page-output=README.json", "--page-output=out/../escape.json",
              "--page-output=out/folder", "--sample=missing"}) {
            bool rejected{};
            try {
                static_cast<void>(
                    gallery_page_options(std::array<std::string_view, 2>{"--page-acceptance", argument}, root));
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            if (!rejected) {
                throw std::runtime_error("invalid finite options were accepted");
            }
        }
        bool rejected{};
        try {
            static_cast<void>(gallery_page_options(std::array<std::string_view, 1>{"--page=button"}, root));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        if (!rejected) {
            throw std::runtime_error("automation parameters started an interactive product");
        }
        rejected = false;
        try {
            static_cast<void>(
                gallery_page_options(std::array<std::string_view, 3>{"--page-acceptance", "--page=tokens",
                                                                     "--sample=gallery.sample.tokens.theme"},
                                     root));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        if (!rejected) {
            throw std::runtime_error("an unaddressable built-in sample route was accepted");
        }
        const auto specific =
            gallery_page_options(std::array<std::string_view, 3>{"--page-acceptance", "--page=button",
                                                                 "--sample=gallery.sample.button.basics"},
                                 root);
        if (specific.pages.front().sample_identity != "gallery.sample.button.basics") {
            throw std::runtime_error("sample group route was lost");
        }
        if (argc == 2) {
            const std::string output_argument = "--page-output=" + std::string{argv[1]};
            const auto writer_options =
                gallery_page_options(std::array<std::string_view, 2>{"--page-acceptance", output_argument}, root);
            GalleryPageAcceptanceResult fixture;
            fixture.platform = "linux";
            fixture.preset = "fixture";
            fixture.toolchain = "GNU";
            fixture.configuration = "Release";
            fixture.error = "fixture line\n\"quote\"\t\\";
            write_gallery_page_acceptance(writer_options.output, fixture);
        }
        std::cout << "Gallery finite options passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
