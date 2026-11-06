#include <ryn/qrcode_data.hpp>
#include <qrcodegen.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace ryn {
namespace {
using Segment = qrcodegen::QrSegment;
using Code = qrcodegen::QrCode;
constexpr std::size_t max_payload = 7089;

void validate(const QRCodeData& data, const QRCodePaint& p) {
    if (data.size() < 21 || p.margin > 64 || !std::isfinite(p.size) || p.size <= 0 || p.size > 16384 ||
        !std::isfinite(p.logo_width) || !std::isfinite(p.logo_height) || p.logo_width < 0 || p.logo_height < 0 ||
        p.logo_width > p.size || p.logo_height > p.size || !std::isfinite(p.logo_opacity) || p.logo_opacity < 0 ||
        p.logo_opacity > 1) {
        throw std::invalid_argument("Invalid QRCode paint geometry");
    }
    if (p.foreground == p.background || p.foreground.alpha() == 0) {
        throw std::invalid_argument("QRCode requires distinct colors and a visible foreground");
    }
}

std::uint8_t channel(float x) {
    return static_cast<std::uint8_t>(std::lround(x * 255));
}

void put_color(std::span<std::byte> pixel, Color c) {
    pixel[0] = std::byte{channel(c.red())};
    pixel[1] = std::byte{channel(c.green())};
    pixel[2] = std::byte{channel(c.blue())};
    pixel[3] = std::byte{channel(c.alpha())};
}

void blend(std::span<std::byte> dst, std::span<const std::byte> src, float opacity) {
    const float a = std::to_integer<unsigned>(src[3]) / 255.F * opacity;
    const float b = std::to_integer<unsigned>(dst[3]) / 255.F;
    const float total = a + b * (1 - a);
    for (int i = 0; i < 3; ++i) {
        const auto value =
            total > 0
                ? (std::to_integer<unsigned>(src[i]) * a + std::to_integer<unsigned>(dst[i]) * b * (1 - a)) / total
                : 0;
        dst[i] = std::byte{static_cast<std::uint8_t>(std::lround(value))};
    }
    dst[3] = std::byte{channel(total)};
}

void be32(std::vector<std::byte>& out, std::uint32_t v) {
    for (int shift : {24, 16, 8, 0}) {
        out.push_back(std::byte((v >> shift) & 255));
    }
}

void chunk(std::vector<std::byte>& out, const char* name, std::span<const std::byte> data) {
    be32(out, static_cast<std::uint32_t>(data.size()));
    const auto first = out.size();
    for (int i = 0; i < 4; ++i) {
        out.push_back(std::byte(name[i]));
    }
    out.insert(out.end(), data.begin(), data.end());
    std::uint32_t crc = 0xffffffffU;
    for (std::size_t i = first; i < out.size(); ++i) {
        crc ^= std::to_integer<std::uint8_t>(out[i]);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((0U - (crc & 1U)) & 0xedb88320U);
        }
    }
    be32(out, crc ^ 0xffffffffU);
}

std::vector<std::byte> png(const ImagePixels& pixels) {
    std::vector<std::byte> raw;
    raw.reserve((std::size_t(pixels.width()) * 4 + 1) * pixels.height());
    for (std::uint32_t y = 0; y < pixels.height(); ++y) {
        raw.push_back(std::byte{0});
        const auto row = pixels.bytes().subspan(y * pixels.row_stride(), std::size_t(pixels.width()) * 4);
        raw.insert(raw.end(), row.begin(), row.end());
    }
    std::vector<std::byte> z{std::byte{0x78}, std::byte{0x01}};
    for (std::size_t i = 0; i < raw.size();) {
        const auto n = static_cast<std::uint16_t>(std::min<std::size_t>(65535, raw.size() - i));
        z.push_back(std::byte(i + n == raw.size() ? 1 : 0));
        z.push_back(std::byte(n & 255));
        z.push_back(std::byte(n >> 8));
        z.push_back(std::byte((~n) & 255));
        z.push_back(std::byte((~n >> 8) & 255));
        z.insert(z.end(), raw.begin() + i, raw.begin() + i + n);
        i += n;
    }
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (auto v : raw) {
        a = (a + std::to_integer<unsigned>(v)) % 65521;
        b = (b + a) % 65521;
    }
    be32(z, (b << 16) | a);
    std::vector<std::byte> out;
    for (unsigned v : {137, 80, 78, 71, 13, 10, 26, 10}) {
        out.push_back(std::byte(v));
    }
    std::vector<std::byte> header;
    be32(header, pixels.width());
    be32(header, pixels.height());
    for (unsigned v : {8, 6, 0, 0, 0}) {
        header.push_back(std::byte(v));
    }
    chunk(out, "IHDR", header);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});
    return out;
}

std::string base64(std::span<const std::byte> data) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    for (std::size_t i = 0; i < data.size(); i += 3) {
        const auto n = std::min<std::size_t>(3, data.size() - i);
        unsigned v = std::to_integer<unsigned>(data[i]) << 16;
        if (n > 1) {
            v |= std::to_integer<unsigned>(data[i + 1]) << 8;
        }
        if (n > 2) {
            v |= std::to_integer<unsigned>(data[i + 2]);
        }
        result += alphabet[(v >> 18) & 63];
        result += alphabet[(v >> 12) & 63];
        result += n > 1 ? alphabet[(v >> 6) & 63] : '=';
        result += n > 2 ? alphabet[v & 63] : '=';
    }
    return result;
}

std::string rgb(Color c) {
    return "rgb(" + std::to_string(channel(c.red())) + "," + std::to_string(channel(c.green())) + "," +
           std::to_string(channel(c.blue())) + ")";
}
} // namespace

QRCodeData QRCodeData::encode(const QRCodeValue& value, QRCodeEncoding options) {
    if (options.error_level < QRCodeErrorLevel::L || options.error_level > QRCodeErrorLevel::H ||
        options.min_version < 1 || options.max_version > 40 || options.min_version > options.max_version ||
        options.mask < -1 || options.mask > 7) {
        throw std::invalid_argument("Invalid QRCode encoding options");
    }
    std::vector<Segment> segments;
    if (const auto* binary = std::get_if<std::vector<std::byte>>(&value.value)) {
        if (binary->empty() || binary->size() > 2953) {
            throw std::length_error("QRCode binary payload must be 1..2953 bytes");
        }
        std::vector<std::uint8_t> bytes;
        for (auto v : *binary) {
            bytes.push_back(std::to_integer<std::uint8_t>(v));
        }
        segments.push_back(Segment::makeBytes(bytes));
    } else {
        std::vector<String> text;
        if (const auto* s = std::get_if<String>(&value.value)) {
            text.push_back(*s);
        } else {
            text = std::get<std::vector<String>>(value.value);
        }
        if (text.empty() || text.size() > 128) {
            throw std::length_error("QRCode requires 1..128 text segments");
        }
        std::size_t total = 0;
        bool unicode = false;
        for (const auto& s : text) {
            if (s.size_bytes() > max_payload - total) {
                throw std::length_error("QRCode payload limit exceeded");
            }
            total += s.size_bytes();
            for (auto c : s.bytes()) {
                unicode |= static_cast<unsigned char>(c) >= 128;
            }
        }
        if (!total) {
            throw std::invalid_argument("QRCode payload is empty");
        }
        if (unicode) {
            segments.push_back(Segment::makeEci(26));
        }
        for (const auto& s : text) {
            const std::string bytes(s.bytes());
            if (bytes.find('\0') != std::string::npos) {
                segments.push_back(Segment::makeBytes(std::vector<std::uint8_t>(bytes.begin(), bytes.end())));
            } else {
                auto parts = Segment::makeSegments(bytes.c_str());
                segments.insert(segments.end(), parts.begin(), parts.end());
            }
        }
    }
    try {
        const auto qr = Code::encodeSegments(segments, static_cast<Code::Ecc>(options.error_level), options.min_version,
                                             options.max_version, options.mask, options.boost_level);
        QRCodeData result;
        result.version_ = qr.getVersion();
        result.size_ = qr.getSize();
        result.mask_ = qr.getMask();
        result.level_ = static_cast<QRCodeErrorLevel>(qr.getErrorCorrectionLevel());
        result.modules_.reserve(std::size_t(result.size_) * result.size_);
        for (int y = 0; y < result.size_; ++y) {
            for (int x = 0; x < result.size_; ++x) {
                result.modules_.push_back(qr.getModule(x, y));
            }
        }
        return result;
    } catch (const qrcodegen::data_too_long& e) {
        throw std::length_error(e.what());
    }
}

bool QRCodeData::module(int x, int y) const noexcept {
    return x >= 0 && y >= 0 && x < size_ && y < size_ && modules_[std::size_t(y) * size_ + x] != 0;
}

std::shared_ptr<const ImagePixels> rasterize_qrcode(const QRCodeData& data, const QRCodePaint& p, std::uint32_t size) {
    validate(data, p);
    const unsigned count = data.size() + 2 * p.margin;
    if (size < count || size > 4096) {
        throw std::length_error("QRCode export must fit modules at integer pitch (up to 4096 pixels)");
    }
    const auto pitch = size / count;
    const auto start = (size - pitch * count) / 2;
    const auto symbol = pitch * data.size();
    const auto origin = start + pitch * p.margin;
    const auto lw = std::min<double>(symbol, p.logo_width / p.size * (pitch * count));
    const auto lh = std::min<double>(symbol, p.logo_height / p.size * (pitch * count));
    const double lx = origin + (symbol - lw) / 2;
    const double ly = origin + (symbol - lh) / 2;
    std::vector<std::byte> bytes(std::size_t(size) * size * 4);
    for (std::uint32_t y = 0; y < size; ++y) {
        for (std::uint32_t x = 0; x < size; ++x) {
            auto pixel = std::span(bytes).subspan((std::size_t(y) * size + x) * 4, 4);
            bool dark = x >= origin && y >= origin && x < origin + symbol && y < origin + symbol &&
                        data.module((x - origin) / pitch, (y - origin) / pitch);
            const bool logo =
                p.logo && lw > 0 && lh > 0 && x + .5 >= lx && x + .5 < lx + lw && y + .5 >= ly && y + .5 < ly + lh;
            if (logo && p.excavate) {
                dark = false;
            }
            put_color(pixel, dark ? p.foreground : p.background);
            if (logo) {
                const auto sx =
                    std::min(p.logo->width() - 1, static_cast<std::uint32_t>((x + .5 - lx) / lw * p.logo->width()));
                const auto sy =
                    std::min(p.logo->height() - 1, static_cast<std::uint32_t>((y + .5 - ly) / lh * p.logo->height()));
                blend(pixel, p.logo->bytes().subspan(sy * p.logo->row_stride() + sx * 4, 4), p.logo_opacity);
            }
        }
    }
    return ImagePixels::copy(size, size, std::size_t(size) * 4, bytes);
}

std::vector<std::byte> export_qrcode_png(const QRCodeData& data, const QRCodePaint& p, std::uint32_t size) {
    return png(*rasterize_qrcode(data, p, size));
}

std::string export_qrcode_svg(const QRCodeData& data, const QRCodePaint& p) {
    validate(data, p);
    const double count = data.size() + 2 * p.margin;
    const double lw = std::min<double>(data.size(), p.logo_width / p.size * count);
    const double lh = std::min<double>(data.size(), p.logo_height / p.size * count);
    const double lx = p.margin + (data.size() - lw) / 2;
    const double ly = p.margin + (data.size() - lh) / 2;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(9);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << p.size << "\" height=\"" << p.size
        << "\" viewBox=\"0 0 " << count << ' ' << count << "\" shape-rendering=\"crispEdges\">";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"" << rgb(p.background) << "\" fill-opacity=\""
        << p.background.alpha() << "\"/>";
    if (p.logo && p.excavate && lw > 0 && lh > 0) {
        out << "<defs><clipPath id=\"qr\"><path clip-rule=\"evenodd\" d=\"M0 0H" << count << 'V' << count << "H0Z M"
            << lx << ' ' << ly << 'v' << lh << 'h' << lw << 'v' << -lh << "Z\"/></clipPath></defs>";
    }
    out << "<path fill=\"" << rgb(p.foreground) << "\" fill-opacity=\"" << p.foreground.alpha() << '"';
    if (p.logo && p.excavate && lw > 0 && lh > 0) {
        out << " clip-path=\"url(#qr)\"";
    }
    out << " d=\"";
    for (int y = 0; y < data.size(); ++y) {
        for (int x = 0; x < data.size();) {
            if (!data.module(x, y)) {
                ++x;
                continue;
            }
            const int start = x;
            while (data.module(x, y)) {
                ++x;
            }
            out << 'M' << start + p.margin << ' ' << y + p.margin << 'h' << x - start << "v1h" << start - x << 'Z';
        }
    }
    out << "\"/>";
    if (p.logo && lw > 0 && lh > 0) {
        out << "<image x=\"" << lx << "\" y=\"" << ly << "\" width=\"" << lw << "\" height=\"" << lh
            << "\" preserveAspectRatio=\"none\" image-rendering=\"pixelated\" opacity=\"" << p.logo_opacity
            << "\" href=\"data:image/png;base64," << base64(png(*p.logo)) << "\"/>";
    }
    out << "</svg>";
    return out.str();
}
} // namespace ryn
