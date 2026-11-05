#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ryn {

struct ImageLimits final {
    std::uint32_t maximum_dimension{16384};
    std::size_t maximum_decoded_bytes{256U * 1024U * 1024U};
    std::size_t maximum_encoded_bytes{64U * 1024U * 1024U};
};

// Top-left origin, RGBA8 straight alpha, encoded RGB values (no ICC/gamma conversion).
// JPEG EXIF orientation is not applied; applications normalize metadata before supplying pixels.
class ImagePixels final {
public:
    [[nodiscard]] static std::shared_ptr<const ImagePixels> copy(std::uint32_t width, std::uint32_t height,
                                                                 std::size_t row_stride,
                                                                 std::span<const std::byte> bytes,
                                                                 ImageLimits limits = {});

    [[nodiscard]] std::uint32_t width() const noexcept {
        return width_;
    }

    [[nodiscard]] std::uint32_t height() const noexcept {
        return height_;
    }

    [[nodiscard]] std::size_t row_stride() const noexcept {
        return row_stride_;
    }

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
        return bytes_;
    }

private:
    std::uint32_t width_{};
    std::uint32_t height_{};
    std::size_t row_stride_{};
    std::vector<std::byte> bytes_;
};

class EncodedImage final {
public:
    [[nodiscard]] static std::shared_ptr<const EncodedImage> copy(std::span<const std::byte> bytes,
                                                                  ImageLimits limits = {});

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
        return bytes_;
    }

private:
    std::vector<std::byte> bytes_;
};

enum class ImageErrorCode { none, empty, unsupported_format, malformed, limits, loader_failure, cancelled };

struct ImageError final {
    ImageErrorCode code{};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return code != ImageErrorCode::none;
    }

    friend bool operator==(const ImageError&, const ImageError&) = default;
};

struct ImageDecodeResult final {
    std::shared_ptr<const ImagePixels> pixels;
    ImageError error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return bool(pixels);
    }
};

// PNG (8/16-bit normalized to 8), JPEG and BMP only. No IO, platform or renderer dependency.
[[nodiscard]] ImageDecodeResult decode_image(const EncodedImage& image, ImageLimits limits = {});

} // namespace ryn
