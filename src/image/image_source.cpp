#include <ryn/image_source.hpp>

#include <stdexcept>
#include <limits>

#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_MAX_DIMENSIONS 16384
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace ryn {
namespace {
std::size_t checked_size(std::uint32_t width, std::uint32_t height, std::size_t pitch, ImageLimits limits) {
    const auto row = std::uint64_t(width) * 4;
    if (!width || !height || width > limits.maximum_dimension || height > limits.maximum_dimension || pitch < row ||
        row > limits.maximum_decoded_bytes ||
        std::uint64_t(height - 1) > (limits.maximum_decoded_bytes - row) / pitch) {
        throw std::length_error("Invalid or oversized RGBA image extent/stride");
    }
    return std::size_t(height - 1) * pitch + std::size_t(row);
}
} // namespace

std::shared_ptr<const ImagePixels> ImagePixels::copy(std::uint32_t width, std::uint32_t height, std::size_t pitch,
                                                     std::span<const std::byte> bytes, ImageLimits limits) {
    const auto size = checked_size(width, height, pitch, limits);
    if (bytes.size() < size) {
        throw std::invalid_argument("RGBA source bytes are shorter than extent/stride");
    }
    auto result = std::shared_ptr<ImagePixels>(new ImagePixels);
    result->width_ = width;
    result->height_ = height;
    result->row_stride_ = pitch;
    result->bytes_.assign(bytes.begin(), bytes.begin() + size);
    return result;
}

std::shared_ptr<const EncodedImage> EncodedImage::copy(std::span<const std::byte> bytes, ImageLimits limits) {
    if (bytes.size() > limits.maximum_encoded_bytes || bytes.size() > std::size_t(std::numeric_limits<int>::max())) {
        throw std::length_error("Encoded image exceeds input limit");
    }
    auto result = std::shared_ptr<EncodedImage>(new EncodedImage);
    result->bytes_.assign(bytes.begin(), bytes.end());
    return result;
}

ImageDecodeResult decode_image(const EncodedImage& image, ImageLimits limits) {
    const auto bytes = image.bytes();
    if (bytes.empty()) {
        return {{}, {ImageErrorCode::empty, "Empty encoded image"}};
    }
    if (bytes.size() > limits.maximum_encoded_bytes || bytes.size() > std::size_t(std::numeric_limits<int>::max())) {
        return {{}, {ImageErrorCode::limits, "Encoded image exceeds input limit"}};
    }
    const auto* data = reinterpret_cast<const stbi_uc*>(bytes.data());
    const auto size = static_cast<int>(bytes.size());
    const bool png = bytes.size() >= 8 && data[0] == 137 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G';
    const bool jpeg = bytes.size() >= 2 && data[0] == 255 && data[1] == 216;
    const bool bmp = bytes.size() >= 2 && data[0] == 'B' && data[1] == 'M';
    if (!png && !jpeg && !bmp) {
        return {{}, {ImageErrorCode::unsupported_format, "Supported formats: PNG, JPEG, BMP"}};
    }
    int width{};
    int height{};
    int channels{};
    if (!stbi_info_from_memory(data, size, &width, &height, &channels) || width <= 0 || height <= 0) {
        return {{}, {ImageErrorCode::malformed, "Invalid image header"}};
    }
    try {
        const auto count = checked_size(width, height, std::size_t(width) * 4, limits);
        const auto expected_width = width;
        const auto expected_height = height;
        stbi_set_unpremultiply_on_load_thread(1);
        stbi_convert_iphone_png_to_rgb_thread(1);
        std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decoded(
            stbi_load_from_memory(data, size, &width, &height, &channels, 4), &stbi_image_free);
        if (!decoded) {
            return {{}, {ImageErrorCode::malformed, "Image decoder rejected input"}};
        }
        if (width != expected_width || height != expected_height) {
            return {{}, {ImageErrorCode::malformed, "Decoder extent changed after preflight"}};
        }
        return {ImagePixels::copy(width, height, std::size_t(width) * 4,
                                  {reinterpret_cast<const std::byte*>(decoded.get()), count}, limits),
                {}};
    } catch (const std::length_error& error) {
        return {{}, {ImageErrorCode::limits, error.what()}};
    }
}
} // namespace ryn
