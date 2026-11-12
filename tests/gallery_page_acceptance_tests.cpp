#include <chrono>
#include <fstream>
#include <vector>
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
        const auto scenario_dir = root / "out/demo-differential/cli-options";
        std::filesystem::create_directories(scenario_dir);
        const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto request = scenario_dir / ("request-" + unique + ".json");
        const auto observations = scenario_dir / ("observations-" + unique + ".jsonl");
        std::ofstream(request) << "{}";
        std::vector<std::string> scenario_arguments{"--page-acceptance",
                                                    "--page-visual",
                                                    "--page=button",
                                                    "--sample=gallery.sample.button.basics",
                                                    "--page-scenario=" + request.string(),
                                                    "--page-observations=" + observations.string()};
        const auto parse_arguments = [&](const auto& values) {
            std::vector<std::string_view> views(values.begin(), values.end());
            return gallery_page_options(views, root);
        };
        const auto scenario_options = parse_arguments(scenario_arguments);
        if (scenario_options.scenario != request || scenario_options.observations != observations) {
            throw std::runtime_error("scenario CLI lost its bounded paths");
        }
        for (const auto fault : {"no-acceptance", "no-visual", "no-observations", "duplicate-scenario", "i18n",
                                 "two-pages", "escaping-observations"}) {
            auto arguments = scenario_arguments;
            const std::string_view name{fault};
            if (name == "no-acceptance") {
                arguments.erase(arguments.begin());
            } else if (name == "no-visual") {
                arguments.erase(arguments.begin() + 1);
            } else if (name == "no-observations") {
                arguments.pop_back();
            } else if (name == "duplicate-scenario") {
                arguments.push_back(arguments[4]);
            } else if (name == "i18n") {
                arguments.emplace_back("--page-i18n");
            } else if (name == "two-pages") {
                arguments.emplace_back("--page=input");
            } else {
                arguments.back() = "--page-observations=README.jsonl";
            }
            bool invalid{};
            try {
                static_cast<void>(parse_arguments(arguments));
            } catch (const std::invalid_argument&) {
                invalid = true;
            }
            if (!invalid) {
                throw std::runtime_error("unsafe or ambiguous scenario CLI was accepted");
            }
        }
        std::filesystem::remove(request);
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
