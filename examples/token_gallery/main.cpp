#include "token_gallery_definition.hpp"
#include "gallery_demo_inventory.hpp"
#include <iostream>
#include <exception>
#include "gallery_pages_runtime.hpp"
#include <string_view>
#include <array>
#include <algorithm>

int main(int argc, char** argv) {
    try {
        if (const auto result = rynui::example::run_gallery_demo_inventory(argc, argv, RYNUI_GALLERY_SOURCE_ROOT)) {
            return *result;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
    for (int i = 1; i < argc; ++i) {
        if (std::string_view{argv[i]}.starts_with("--page") || std::string_view{argv[i]}.starts_with("--sample")) {
            return rynui::example::run_gallery_pages(argc, argv);
        }
    }
    // Historical finite GPU fixtures remain explicit automation entry points.
    // The interactive product always mounts the component-pages host.
    constexpr std::array<std::string_view, 32> legacy_modes{
        "--animation-acceptance",
        "--bidi-acceptance",
        "--button-acceptance",
        "--card-acceptance",
        "--checkbox-acceptance",
        "--divider-acceptance",
        "--flex-acceptance",
        "--icon-acceptance",
        "--input-acceptance",
        "--input-clear-acceptance",
        "--input-features-acceptance",
        "--modal-acceptance",
        "--otp-acceptance",
        "--password-acceptance",
        "--radio-acceptance",
        "--rounded-acceptance",
        "--scroll-acceptance",
        "--scrollbar-acceptance",
        "--search-acceptance",
        "--select-acceptance",
        "--selection-acceptance",
        "--slider-acceptance",
        "--slider-editing-acceptance",
        "--slider-marks-acceptance",
        "--smoke",
        "--space-acceptance",
        "--switch-acceptance",
        "--tabs-acceptance",
        "--text-area-acceptance",
        "--tooltip-acceptance",
        "--tooltip-content-acceptance",
        "--typography-acceptance",
    };
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (std::ranges::find(legacy_modes, argument) != legacy_modes.end() || argument.starts_with("--snapshot=")) {
            return rynui::example::run_token_gallery(argc, argv, rynui::example::make_token_gallery_definition());
        }
    }
    return rynui::example::run_gallery_pages(argc, argv);
}
