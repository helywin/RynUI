#include "gallery_demo_protocol.hpp"
#include "gallery_demo_hash.hpp"
#include <stdexcept>

#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view{argv[1]} == "--hash-controls") {
            const auto hash = [](const std::string& text) {
                return rynui::example::gallery_demo_sha256(
                    {reinterpret_cast<const unsigned char*>(text.data()), text.size()});
            };
            if (hash("") != "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" ||
                hash("abc") != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ||
                hash(std::string(1000000, 'a')) != "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") {
                throw std::runtime_error("SHA256 standard vectors failed");
            }
            return 0;
        }
        if (argc == 2 && std::string_view{argv[1]} == "--manifest") {
            std::cout << rynui::example::gallery_demo_native_build_manifest().dump() << '\n';
            return 0;
        }
        if (argc != 2) {
            return 2;
        }
        const auto scenario = rynui::example::read_gallery_demo_scenario(argv[1], RYNUI_GALLERY_SOURCE_ROOT);
        std::cout << scenario.identity << ' ' << scenario.actions.size() << ' ' << scenario.checkpoints.size() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
