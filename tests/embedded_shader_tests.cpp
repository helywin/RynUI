#include "renderer/sdl/shader_resources.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            throw std::runtime_error("Expected the compiled shader directory");
        }
        const std::filesystem::path directory{argv[1]};
        std::size_t total_bytes{};
        for (const auto name : std::array{"quad", "glyph", "rounded_effect"}) {
            for (const auto stage : std::array{"vertex", "fragment"}) {
                for (const auto format : std::array{"dxil", "spv"}) {
                    const auto filename = std::string{name} + "." + stage + "." + format;
                    std::ifstream input{directory / filename, std::ios::binary};
                    if (!input) {
                        throw std::runtime_error("Missing comparison input: " + filename);
                    }
                    const std::vector<char> expected{std::istreambuf_iterator<char>{input}, {}};
                    const auto actual = ryn::detail::builtin_shader_bytes(filename);
                    if (expected.empty() || actual.size() != expected.size() ||
                        !std::equal(actual.begin(), actual.end(), expected.begin(), [](std::byte a, char b) {
                            return std::to_integer<unsigned char>(a) == static_cast<unsigned char>(b);
                        })) {
                        throw std::runtime_error("Embedded shader differs from input: " + filename);
                    }
                    if (ryn::detail::builtin_shader_bytes(filename).data() != actual.data()) {
                        throw std::runtime_error("Embedded shader storage is not stable");
                    }
                    total_bytes += actual.size();
                }
            }
        }
        bool rejected{};
        try {
            static_cast<void>(ryn::detail::builtin_shader_bytes("missing.vertex.dxil"));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        if (!rejected) {
            throw std::runtime_error("Missing embedded shader was accepted");
        }
        std::cout << "embedded_shaders=12 bytes=" << total_bytes << " comparison=exact\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
