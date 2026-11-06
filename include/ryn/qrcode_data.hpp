#pragma once

#include <ryn/design_token.hpp>
#include <ryn/image_source.hpp>
#include <ryn/string.hpp>
#include <variant>

namespace ryn {

enum class QRCodeErrorLevel { L, M, Q, H };

// UTF-8 text/individually optimized text segments, or uninterpreted binary bytes.
struct QRCodeValue final {
    using Value = std::variant<String, std::vector<String>, std::vector<std::byte>>;
    Value value{String{}};
    QRCodeValue() = default;

    QRCodeValue(String v) : value(std::move(v)) {}

    QRCodeValue(std::vector<String> v) : value(std::move(v)) {}

    QRCodeValue(std::vector<std::byte> v) : value(std::move(v)) {}

    template <std::size_t N> QRCodeValue(const char8_t (&v)[N]) : value(String{v}) {}

    friend bool operator==(const QRCodeValue&, const QRCodeValue&) = default;
};

struct QRCodeEncoding final {
    QRCodeErrorLevel error_level{QRCodeErrorLevel::M};
    bool boost_level{true};
    int min_version{1};
    int max_version{40};
    int mask{-1};
    friend bool operator==(QRCodeEncoding, QRCodeEncoding) = default;
};

class QRCodeData final {
public:
    [[nodiscard]] static QRCodeData encode(const QRCodeValue&, QRCodeEncoding = {});

    [[nodiscard]] int version() const noexcept {
        return version_;
    }

    [[nodiscard]] int size() const noexcept {
        return size_;
    }

    [[nodiscard]] int mask() const noexcept {
        return mask_;
    }

    [[nodiscard]] QRCodeErrorLevel error_level() const noexcept {
        return level_;
    }

    [[nodiscard]] bool module(int x, int y) const noexcept;

    [[nodiscard]] std::span<const std::uint8_t> modules() const noexcept {
        return modules_;
    }

private:
    int version_{};
    int size_{};
    int mask_{};
    QRCodeErrorLevel level_{};
    std::vector<std::uint8_t> modules_;
};

struct QRCodePaint final {
    Color foreground{0, 0, 0, 1};
    Color background{1, 1, 1, 1};
    unsigned margin{4};
    // Centered, clipped to the symbol. Straight alpha, nearest-neighbor sampling.
    std::shared_ptr<const ImagePixels> logo;
    float logo_width{40};
    float logo_height{40};
    float size{160};
    bool excavate{true};
    float logo_opacity{1};
};

// Pure CPU export, no IO. Caller owns the bytes and chooses a destination.
// PNG has integer module pitch with centered excess padding, SVG preserves logical geometry.
[[nodiscard]] std::shared_ptr<const ImagePixels> rasterize_qrcode(const QRCodeData&, const QRCodePaint&,
                                                                  std::uint32_t pixel_size);
[[nodiscard]] std::vector<std::byte> export_qrcode_png(const QRCodeData&, const QRCodePaint&, std::uint32_t pixel_size);
[[nodiscard]] std::string export_qrcode_svg(const QRCodeData&, const QRCodePaint&);

} // namespace ryn
