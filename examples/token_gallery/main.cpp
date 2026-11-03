#include "token_gallery_definition.hpp"
#include "gallery_pages_runtime.hpp"
#include <string_view>

int main(int argc, char** argv) {
    // Historical finite GPU fixtures remain explicit automation entry points.
    // The interactive product always mounts the component-pages host.
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument != "--page-acceptance" &&
            (argument.ends_with("-acceptance") || argument == "--smoke" || argument.starts_with("--snapshot-"))) {
            return rynui::example::run_token_gallery(argc, argv, rynui::example::make_token_gallery_definition());
        }
    }
    return rynui::example::run_gallery_pages(argc, argv);
}
