#include <ryn/color_picker.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace ryn {
namespace {
void range(double v, double max) {
    if (!std::isfinite(v) || v < 0 || v > max) {
        throw std::invalid_argument("Color channel is outside its finite range");
    }
}

String string(std::string s) {
    return String::from_utf8(s).value();
}

std::string number(double v) {
    std::array<char, 64> buffer{};
    const auto r = std::to_chars(buffer.data(), buffer.data() + buffer.size(), v, std::chars_format::general);
    return {buffer.data(), r.ptr};
}
} // namespace

PickerColor::PickerColor() : rgb_{22, 119, 255, 1} {}

PickerColor::PickerColor(ColorRgb v) : rgb_(v) {
    range(v.r, 255);
    range(v.g, 255);
    range(v.b, 255);
    range(v.a, 1);
    hue_ = hsb().h;
}

PickerColor::PickerColor(Color c)
    : PickerColor(ColorRgb{c.red() * 255.0, c.green() * 255.0, c.blue() * 255.0, c.alpha()}) {}

PickerColor::PickerColor(ColorHsb v) {
    range(v.h, 360);
    range(v.s, 1);
    range(v.b, 1);
    range(v.a, 1);
    hue_ = v.h;
    const auto h = v.h / 60;
    const auto c = v.b * v.s;
    const auto x = c * (1 - std::abs(std::fmod(h, 2) - 1));
    const std::array<ColorRgb, 6> colors{
        {{c, x, 0, v.a}, {x, c, 0, v.a}, {0, c, x, v.a}, {0, x, c, v.a}, {x, 0, c, v.a}, {c, 0, x, v.a}}};
    rgb_ = colors[static_cast<unsigned>(h) % 6];
    const auto m = v.b - c;
    rgb_.r = (rgb_.r + m) * 255;
    rgb_.g = (rgb_.g + m) * 255;
    rgb_.b = (rgb_.b + m) * 255;
    if (rgb_.r != rgb_.g || rgb_.g != rgb_.b) {
        hue_ = hsb().h;
    }
}

ColorHsb PickerColor::hsb() const noexcept {
    const auto hi = std::max({rgb_.r, rgb_.g, rgb_.b});
    const auto lo = std::min({rgb_.r, rgb_.g, rgb_.b});
    const auto d = hi - lo;
    double h = hue_;
    if (d != 0) {
        h = hi == rgb_.r   ? std::fmod((rgb_.g - rgb_.b) / d, 6)
            : hi == rgb_.g ? (rgb_.b - rgb_.r) / d + 2
                           : (rgb_.r - rgb_.g) / d + 4;
        h *= 60;
        if (h < 0) {
            h += 360;
        }
    }
    return {h, hi == 0 ? 0 : d / hi, hi / 255, rgb_.a};
}

Color PickerColor::color() const {
    return Color{static_cast<float>(rgb_.r / 255), static_cast<float>(rgb_.g / 255), static_cast<float>(rgb_.b / 255),
                 static_cast<float>(rgb_.a)};
}

String PickerColor::format(ColorFormat f, bool alpha) const {
    const bool a = alpha && rgb_.a < 1;
    if (f == ColorFormat::Hex) {
        std::array<char, 10> b{};
        std::snprintf(b.data(), b.size(), "#%02x%02x%02x", static_cast<unsigned>(std::lround(rgb_.r)),
                      static_cast<unsigned>(std::lround(rgb_.g)), static_cast<unsigned>(std::lround(rgb_.b)));
        std::string text{b.data()};
        if (a) {
            std::snprintf(b.data(), b.size(), "%02x", static_cast<unsigned>(std::lround(rgb_.a * 255)));
            text += b.data();
        }
        return string(text);
    }
    if (f == ColorFormat::Rgb) {
        return string(std::string{a ? "rgba(" : "rgb("} + number(rgb_.r) + ", " + number(rgb_.g) + ", " +
                      number(rgb_.b) + (a ? ", " + number(rgb_.a) : "") + ")");
    }
    if (f != ColorFormat::Hsb) {
        throw std::invalid_argument("Invalid ColorFormat");
    }
    const auto h = hsb();
    return string(std::string{a ? "hsba(" : "hsb("} + number(h.h) + ", " + number(h.s * 100) + "%, " +
                  number(h.b * 100) + "%" + (a ? ", " + number(h.a) : "") + ")");
}

std::optional<PickerColor> PickerColor::parse(std::string_view text) {
    if (text.empty() || text.size() > 256) {
        return {};
    }
    if (text.front() == '#') {
        text.remove_prefix(1);
        const auto n = text.size();
        if (n != 3 && n != 4 && n != 6 && n != 8) {
            return {};
        }
        std::array<unsigned, 4> c{0, 0, 0, 255};
        for (std::size_t i = 0; i < (n == 3 || n == 4 ? n : n / 2); ++i) {
            const auto digits = text.substr(i * (n < 5 ? 1 : 2), n < 5 ? 1 : 2);
            const auto r = std::from_chars(digits.data(), digits.data() + digits.size(), c[i], 16);
            if (r.ec != std::errc{} || r.ptr != digits.data() + digits.size()) {
                return {};
            }
            if (n < 5) {
                c[i] *= 17;
            }
        }
        return PickerColor{ColorRgb{double(c[0]), double(c[1]), double(c[2]), c[3] / 255.0}};
    }
    const bool hsb = text.starts_with("hsb(") || text.starts_with("hsba(");
    const bool rgb = text.starts_with("rgb(") || text.starts_with("rgba(");
    if ((!hsb && !rgb) || text.back() != ')') {
        return {};
    }
    const bool alpha = text[3] == 'a';
    text.remove_prefix(alpha ? 5 : 4);
    text.remove_suffix(1);
    std::array<double, 4> c{0, 0, 0, 1};
    for (std::size_t i = 0; i < (alpha ? 4U : 3U); ++i) {
        const auto comma = text.find(',');
        auto part = text.substr(0, comma);
        while (!part.empty() && part.front() == ' ') {
            part.remove_prefix(1);
        }
        while (!part.empty() && part.back() == ' ') {
            part.remove_suffix(1);
        }
        const bool percent = !part.empty() && part.back() == '%';
        if (percent) {
            part.remove_suffix(1);
        }
        if (percent != (hsb && (i == 1 || i == 2)) || part.empty()) {
            return {};
        }
        const auto r = std::from_chars(part.data(), part.data() + part.size(), c[i]);
        if (r.ec != std::errc{} || r.ptr != part.data() + part.size() || !std::isfinite(c[i])) {
            return {};
        }
        if ((i + 1 == (alpha ? 4U : 3U)) != (comma == text.npos)) {
            return {};
        }
        if (comma != text.npos) {
            text.remove_prefix(comma + 1);
        }
    }
    try {
        return hsb ? PickerColor{ColorHsb{c[0], c[1] / 100, c[2] / 100, c[3]}}
                   : PickerColor{ColorRgb{c[0], c[1], c[2], c[3]}};
    } catch (const std::invalid_argument&) {
        return {};
    }
}

ColorPickerValue::ColorPickerValue(PickerColor c) : solid_(c) {}

ColorPickerValue::ColorPickerValue(std::vector<ColorStop> stops) : stops_(std::move(stops)) {
    if (stops_.size() < 2 || stops_.size() > 64) {
        throw std::invalid_argument("Gradient requires 2..64 keyed stops");
    }
    std::vector<String> keys;
    for (const auto& s : stops_) {
        range(s.percent, 100);
        if (s.key.empty() || std::ranges::find(keys, s.key) != keys.end()) {
            throw std::invalid_argument("Gradient stop keys must be unique and nonempty");
        }
        keys.push_back(s.key);
    }
    std::stable_sort(stops_.begin(), stops_.end(), [](auto& a, auto& b) { return a.percent < b.percent; });
}

PickerColor ColorPickerValue::sample(double percent) const {
    range(percent, 100);
    if (solid_) {
        return *solid_;
    }
    if (stops_.empty()) {
        return PickerColor{ColorRgb{0, 0, 0, 0}};
    }
    auto next =
        std::upper_bound(stops_.begin(), stops_.end(), percent, [](double p, const auto& s) { return p < s.percent; });
    if (next == stops_.begin()) {
        return next->color;
    }
    if (next == stops_.end()) {
        return stops_.back().color;
    }
    const auto& prev = *(next - 1);
    const auto t = (percent - prev.percent) / (next->percent - prev.percent);
    const auto a = prev.color.rgb();
    const auto b = next->color.rgb();
    return PickerColor{
        ColorRgb{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t}};
}

String ColorPickerValue::format(ColorFormat f, bool alpha) const {
    if (solid_) {
        return solid_->format(f, alpha);
    }
    if (empty()) {
        return {};
    }
    std::string text = "linear-gradient(90deg";
    for (const auto& s : stops_) {
        text += ", ";
        text += s.color.format(f, alpha).bytes();
        text += " " + number(s.percent) + "%";
    }
    return string(text + ")");
}
} // namespace ryn
