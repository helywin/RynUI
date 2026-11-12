#include "gallery_demo_hash.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace rynui::example {

std::string gallery_demo_sha256(std::span<const unsigned char> bytes) {
    constexpr std::array<std::uint32_t, 64> constants{
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    std::array<std::uint32_t, 8> state{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                       0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    if (bytes.size() > 512 * 1024 * 1024) {
        throw std::invalid_argument("Gallery hash input exceeds 512 MiB");
    }
    std::vector<unsigned char> data(bytes.begin(), bytes.end());
    data.push_back(0x80);
    while (data.size() % 64 != 56) {
        data.push_back(0);
    }
    const auto bits = static_cast<std::uint64_t>(bytes.size()) * 8;
    for (int shift = 56; shift >= 0; shift -= 8) {
        data.push_back(static_cast<unsigned char>(bits >> shift));
    }
    for (std::size_t offset = 0; offset < data.size(); offset += 64) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16; ++i) {
            const auto index = offset + i * 4;
            words[i] = (std::uint32_t{data[index]} << 24) | (std::uint32_t{data[index + 1]} << 16) |
                       (std::uint32_t{data[index + 2]} << 8) | data[index + 3];
        }
        for (std::size_t i = 16; i < 64; ++i) {
            const auto a = words[i - 15];
            const auto b = words[i - 2];
            const auto s0 = std::rotr(a, 7) ^ std::rotr(a, 18) ^ (a >> 3);
            const auto s1 = std::rotr(b, 17) ^ std::rotr(b, 19) ^ (b >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }
        auto work = state;
        for (std::size_t i = 0; i < 64; ++i) {
            const auto s1 = std::rotr(work[4], 6) ^ std::rotr(work[4], 11) ^ std::rotr(work[4], 25);
            const auto choice = (work[4] & work[5]) ^ (~work[4] & work[6]);
            const auto first = work[7] + s1 + choice + constants[i] + words[i];
            const auto s0 = std::rotr(work[0], 2) ^ std::rotr(work[0], 13) ^ std::rotr(work[0], 22);
            const auto majority = (work[0] & work[1]) ^ (work[0] & work[2]) ^ (work[1] & work[2]);
            const auto second = s0 + majority;
            for (std::size_t j = 7; j > 0; --j) {
                work[j] = work[j - 1];
            }
            work[4] += first;
            work[0] = first + second;
        }
        for (std::size_t i = 0; i < state.size(); ++i) {
            state[i] += work[i];
        }
    }
    std::ostringstream result;
    for (const auto word : state) {
        result << std::hex << std::setw(8) << std::setfill('0') << word;
    }
    return result.str();
}

std::string gallery_demo_file_sha256(const std::filesystem::path& path) {
    if (std::filesystem::file_size(path) > 512 * 1024 * 1024) {
        throw std::invalid_argument("Gallery hash input exceeds 512 MiB");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::invalid_argument("Gallery hash input is missing");
    }
    const std::vector<unsigned char> bytes(std::istreambuf_iterator<char>{input}, {});
    if (!input.eof() && input.fail()) {
        throw std::runtime_error("Gallery hash input read failed");
    }
    return gallery_demo_sha256(bytes);
}

} // namespace rynui::example
