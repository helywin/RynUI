#include "renderer/sdl/shader_resources.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <stdexcept>
#include <string>

namespace ryn::detail {

std::span<const std::byte> builtin_shader_bytes(std::string_view filename) {
    std::wstring name{L"RYNUI_SHADER_"};
    for (const char value : filename) {
        if (value == '.') {
            name.push_back(L'_');
        } else if (value >= 'a' && value <= 'z') {
            name.push_back(static_cast<wchar_t>(value - 'a' + 'A'));
        } else {
            name.push_back(static_cast<wchar_t>(static_cast<unsigned char>(value)));
        }
    }
    const auto resource = FindResourceW(nullptr, name.c_str(), MAKEINTRESOURCEW(10));
    if (resource == nullptr) {
        throw std::runtime_error("Missing embedded shader resource: " + std::string(filename));
    }
    const auto size = SizeofResource(nullptr, resource);
    if (size == 0) {
        throw std::runtime_error("Embedded shader resource is empty: " + std::string(filename));
    }
    const auto loaded = LoadResource(nullptr, resource);
    const auto* bytes = loaded != nullptr ? static_cast<const std::byte*>(LockResource(loaded)) : nullptr;
    if (bytes == nullptr) {
        throw std::runtime_error("Unable to load embedded shader resource: " + std::string(filename));
    }
    return {bytes, static_cast<std::size_t>(size)};
}

} // namespace ryn::detail
