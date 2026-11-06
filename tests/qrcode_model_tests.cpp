#include <ryn/qrcode_data.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

using namespace ryn;

namespace {
int checks{};

void expect(bool ok, const char* message) {
    ++checks;
    if (!ok) {
        throw std::runtime_error(message);
    }
}

template <typename F> void rejects(F fn) {
    bool threw{};
    try {
        fn();
    } catch (const std::exception&) {
        threw = true;
    }
    expect(threw, "Invalid input was accepted");
}

std::uint64_t fingerprint(const QRCodeData& d) {
    std::uint64_t h = 14695981039346656037ULL;
    for (auto m : d.modules()) {
        h = (h ^ m) * 1099511628211ULL;
    }
    return h;
}

void write(const std::filesystem::path& p, std::span<const std::byte> bytes) {
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!f) {
        throw std::runtime_error("Failed to write test artifact");
    }
}

void tests(const std::filesystem::path& dir) {
    // Independent Segno 1.6.6 ISO Model 2 vectors: no boost, exact version/EC/mask.
    struct Vector {
        String value;
        int version;
        QRCodeErrorLevel ec;
        int mask;
        std::uint64_t hash;
    };

    const Vector vectors[] = {
        {u8"HELLO WORLD", 1, QRCodeErrorLevel::Q, 0, 0x84d342a69849001ULL},
        {u8"123456789012345678901234567890", 7, QRCodeErrorLevel::M, 4, 0xc2d1cc0154f40e5dULL},
        {u8"VERSION FORTY", 40, QRCodeErrorLevel::H, 7, 0x3b41cefd55b0bfd5ULL},
    };
    for (const auto& v : vectors) {
        auto d = QRCodeData::encode(v.value, {v.ec, false, v.version, v.version, v.mask});
        expect(d.version() == v.version && d.size() == 17 + 4 * v.version && d.mask() == v.mask &&
                   d.error_level() == v.ec,
               "Encoding metadata changed");
        expect(fingerprint(d) == v.hash, "Independent full-matrix vector differs");
        expect(d.module(0, 0) && !d.module(-1, 0) && !d.module(d.size(), 0), "Module bounds failed");
    }
    auto d = QRCodeData::encode(u8"HELLO WORLD", {QRCodeErrorLevel::L, false});
    expect(d.error_level() == QRCodeErrorLevel::L, "Explicit EC changed without boost");
    expect(QRCodeData::encode(u8"HELLO WORLD", {QRCodeErrorLevel::L, true}).error_level() == QRCodeErrorLevel::Q,
           "EC boost failed");
    const auto segments = QRCodeData::encode(std::vector<String>{u8"12345678901234567890", u8"lowercase"});
    expect(segments.size() >= 21, "Text segments failed");
    std::vector<std::byte> bytes(2953, std::byte{0x41});
    expect(QRCodeData::encode(bytes, {QRCodeErrorLevel::L, false}).version() == 40, "Maximum binary capacity failed");
    bytes.push_back(std::byte{0});
    rejects([&] { (void)QRCodeData::encode(bytes); });
    rejects([] { (void)QRCodeData::encode(QRCodeValue{}); });
    rejects([] { (void)QRCodeData::encode(u8"x", {QRCodeErrorLevel::M, true, 0, 40, -1}); });
    rejects([] { (void)QRCodeData::encode(u8"x", {QRCodeErrorLevel::M, true, 40, 1, -1}); });
    rejects([] { (void)QRCodeData::encode(u8"x", {QRCodeErrorLevel::M, true, 1, 40, 8}); });
    rejects([] { (void)QRCodeData::encode(u8"x", {static_cast<QRCodeErrorLevel>(9)}); });
    rejects([] { (void)QRCodeData::encode(std::vector<String>(129, String{u8"x"})); });
    expect(!String::from_utf8(std::string_view("\xc0\xaf", 2)), "Malformed UTF8 entered typed payload");
    QRCodePaint p;
    const auto pixels = rasterize_qrcode(d, p, 299);
    expect(pixels->width() == 299 && pixels->row_stride() == 1196, "Export metrics invalid");
    const unsigned pitch = 299U / (d.size() + 8U);
    const unsigned origin = (299U - pitch * (d.size() + 8U)) / 2 + 4 * pitch;
    for (unsigned y = 0; y < 299; ++y) {
        for (unsigned x = 0; x < 299; ++x) {
            const bool dark = x >= origin && y >= origin && x < origin + pitch * d.size() &&
                              y < origin + pitch * d.size() && d.module((x - origin) / pitch, (y - origin) / pitch);
            const auto pixel = pixels->bytes().subspan((y * 299U + x) * 4, 4);
            expect(pixel[0] == (dark ? std::byte{0} : std::byte{255}) && pixel[3] == std::byte{255},
                   "Actual module pixels or quiet zone differ");
        }
    }
    rejects([&] { (void)rasterize_qrcode(d, p, 20); });
    rejects([&] { (void)rasterize_qrcode(d, p, 4097); });
    p.size = std::numeric_limits<float>::quiet_NaN();
    rejects([&] { (void)export_qrcode_svg(d, p); });
    p.size = 160;
    p.foreground = p.background;
    rejects([&] { (void)export_qrcode_png(d, p, 290); });
    p.foreground = {0, 0, 0, 1};
    const std::array<std::byte, 4> red{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{128}};
    p.logo = ImagePixels::copy(1, 1, 4, red);
    p.logo_width = 16;
    p.logo_height = 16;
    auto logo = rasterize_qrcode(d, p, 290);
    const auto middle = logo->bytes().subspan((145 * 290 + 145) * 4, 4);
    expect(middle[0] == std::byte{255} && middle[1] == std::byte{127} && middle[3] == std::byte{255},
           "Logo straight alpha/excavation failed");
    const auto svg = export_qrcode_svg(d, p);
    expect(svg.find("data:image/png;base64,") != std::string::npos &&
               svg.find("clip-rule=\"evenodd\"") != std::string::npos,
           "SVG lost actual logo or excavation");
    if (!dir.empty()) {
        std::filesystem::create_directories(dir);
        p.logo.reset();
        const QRCodeValue values[] = {
            u8"RynUI independent decode 107", u8"原生二维码 UTF-8 🌏",
            std::vector<std::byte>{std::byte{65}, std::byte{0}, std::byte{66}, std::byte{127}}};
        for (int i = 0; i < 3; ++i) {
            const auto q = QRCodeData::encode(values[i], {QRCodeErrorLevel::H, false});
            const auto png = export_qrcode_png(q, p, 464);
            write(dir / (std::to_string(i) + ".png"), png);
            const auto svg = export_qrcode_svg(q, p);
            write(dir / (std::to_string(i) + ".svg"), std::as_bytes(std::span(svg)));
            expect(png == export_qrcode_png(q, p, 464), "PNG is not deterministic");
        }
        p.logo = ImagePixels::copy(1, 1, 4, red);
        const auto q = QRCodeData::encode(u8"RynUI independent decode 107", {QRCodeErrorLevel::H, false});
        write(dir / "logo.png", export_qrcode_png(q, p, 464));
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        tests(argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path{});
        std::cout << checks << " checks passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
