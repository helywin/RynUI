#include "component/exact_decimal.hpp"
#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace ryn::detail {
namespace {
constexpr std::size_t limit = 16384;
}

void ExactDecimal::normalize() {
    const auto first = digits.find_first_not_of('0');
    digits = first == std::string::npos ? "0" : digits.substr(first);
    while (scale && digits.size() > 1 && digits.back() == '0') {
        digits.pop_back();
        --scale;
    }
    if (digits == "0") {
        scale = 0;
        negative = false;
    }
}

std::optional<ExactDecimal> ExactDecimal::parse(std::string_view s, bool finish) {
    if (s.empty() || s.size() > limit + 32) {
        return {};
    }
    ExactDecimal v;
    v.digits.clear();
    if (s.front() == '-' || s.front() == '+') {
        v.negative = s.front() == '-';
        s.remove_prefix(1);
    }
    const auto exponent = s.find_first_of("eE");
    auto mantissa = s.substr(0, exponent);
    bool dot{};
    bool digit{};
    for (char c : mantissa) {
        if (c == '.' && !dot) {
            dot = true;
            continue;
        }
        if (c < '0' || c > '9') {
            return {};
        }
        digit = true;
        v.digits.push_back(c);
        if (dot) {
            ++v.scale;
        }
    }
    if (!digit || (!finish && mantissa.back() == '.')) {
        return {};
    }
    if (exponent != s.npos) {
        auto e = s.substr(exponent + 1);
        bool minus{};
        if (!e.empty() && (e.front() == '+' || e.front() == '-')) {
            minus = e.front() == '-';
            e.remove_prefix(1);
        }
        unsigned n{};
        const auto r = std::from_chars(e.data(), e.data() + e.size(), n);
        if (e.empty() || r.ec != std::errc{} || r.ptr != e.data() + e.size() || n > limit) {
            return {};
        }
        if (minus) {
            v.scale += n;
        } else if (n <= v.scale) {
            v.scale -= n;
        } else {
            v.digits.append(n - v.scale, '0');
            v.scale = 0;
        }
    }
    v.normalize();
    if (v.digits.size() > limit || v.scale > limit) {
        return {};
    }
    v.normalize();
    return v;
}

std::string ExactDecimal::text() const {
    auto s = digits;
    if (scale) {
        if (s.size() <= scale) {
            s.insert(0, scale + 1 - s.size(), '0');
        }
        s.insert(s.size() - scale, 1, '.');
    }
    if (negative) {
        s.insert(s.begin(), '-');
    }
    return s;
}

int ExactDecimal::compare(const ExactDecimal& other) const {
    if (negative != other.negative) {
        return negative ? -1 : 1;
    }
    const auto common = std::max(scale, other.scale);
    auto a = digits == "0" ? digits : digits + std::string(common - scale, '0');
    auto b = other.digits == "0" ? other.digits : other.digits + std::string(common - other.scale, '0');
    const auto c = a.size() != b.size() ? (a.size() < b.size() ? -1 : 1) : a.compare(b);
    return negative ? -c : c;
}

ExactDecimal ExactDecimal::add(ExactDecimal b) const {
    auto a = *this;
    const auto common = std::max(a.scale, b.scale);
    a.digits.append(common - a.scale, '0');
    b.digits.append(common - b.scale, '0');
    a.scale = common;
    b.scale = common;
    const auto n = std::max(a.digits.size(), b.digits.size());
    a.digits.insert(0, n - a.digits.size(), '0');
    b.digits.insert(0, n - b.digits.size(), '0');
    if (a.negative == b.negative) {
        int carry{};
        for (auto i = n; i-- > 0;) {
            const auto sum = a.digits[i] + b.digits[i] - 2 * '0' + carry;
            a.digits[i] = static_cast<char>('0' + sum % 10);
            carry = sum / 10;
        }
        if (carry) {
            a.digits.insert(a.digits.begin(), '1');
        }
    } else {
        if (a.digits < b.digits) {
            std::swap(a, b);
        }
        int borrow{};
        for (auto i = n; i-- > 0;) {
            auto d = a.digits[i] - b.digits[i] - borrow;
            borrow = d < 0;
            if (borrow) {
                d += 10;
            }
            a.digits[i] = static_cast<char>('0' + d);
        }
    }
    a.normalize();
    if (a.digits.size() > limit) {
        throw std::overflow_error("Decimal result exceeds 16384 digits");
    }
    return a;
}

ExactDecimal ExactDecimal::rounded(unsigned precision, int direction) const {
    if (precision >= scale) {
        return *this;
    }
    auto result = *this;
    if (result.digits.size() <= result.scale) {
        result.digits.insert(0, result.scale + 1 - result.digits.size(), '0');
    }
    const auto cut = result.digits.size() - (result.scale - precision);
    const auto tail = result.digits.substr(cut);
    const bool nonzero = tail.find_first_not_of('0') != tail.npos;
    const bool increment = direction == 0 ? tail.front() >= '5' : nonzero && ((direction < 0) == negative);
    result.digits.resize(cut);
    result.scale = precision;
    if (increment) {
        ExactDecimal unit;
        unit.scale = precision;
        unit.digits = "1";
        unit.negative = negative;
        result = result.add(unit);
    }
    result.normalize();
    return result;
}
} // namespace ryn::detail

namespace ryn {
Decimal::Decimal() : text_(u8"0") {}

Decimal::Decimal(std::string_view text) {
    const auto v = detail::ExactDecimal::parse(text);
    if (!v) {
        throw std::invalid_argument("Decimal requires a finite decimal within 16384 digits");
    }
    text_ = String::from_utf8(v->text()).value();
}
} // namespace ryn
