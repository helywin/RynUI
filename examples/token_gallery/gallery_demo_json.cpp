#include "gallery_demo_json.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace rynui::example {
namespace {

[[noreturn]] void invalid() {
    throw std::invalid_argument("invalid or out-of-bounds Gallery scenario JSON");
}

void append_utf8(std::string& out, std::uint32_t code) {
    if (code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) {
        invalid();
    }
    if (code < 0x80) {
        out.push_back(static_cast<char>(code));
    } else if (code < 0x800) {
        out.push_back(static_cast<char>(0xc0 | (code >> 6)));
        out.push_back(static_cast<char>(0x80 | (code & 63)));
    } else if (code < 0x10000) {
        out.push_back(static_cast<char>(0xe0 | (code >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 63)));
        out.push_back(static_cast<char>(0x80 | (code & 63)));
    } else {
        out.push_back(static_cast<char>(0xf0 | (code >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code >> 12) & 63)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 63)));
        out.push_back(static_cast<char>(0x80 | (code & 63)));
    }
}

void validate_utf8(std::string_view text) {
    for (std::size_t index = 0; index < text.size();) {
        const auto lead = static_cast<unsigned char>(text[index++]);
        if (lead < 128) {
            continue;
        }
        const int count = lead >= 0xc2 && lead <= 0xdf   ? 1
                          : lead >= 0xe0 && lead <= 0xef ? 2
                          : lead >= 0xf0 && lead <= 0xf4 ? 3
                                                         : -1;
        if (count < 0 || index + static_cast<std::size_t>(count) > text.size()) {
            invalid();
        }
        std::uint32_t code = lead & ((1u << (6 - count)) - 1);
        for (int tail = 0; tail < count; ++tail) {
            const auto byte = static_cast<unsigned char>(text[index++]);
            if ((byte & 0xc0) != 0x80) {
                invalid();
            }
            code = (code << 6) | (byte & 63);
        }
        if ((count == 1 && code < 128) || (count == 2 && code < 2048) || (count == 3 && code < 65536) ||
            code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) {
            invalid();
        }
    }
}

class Reader final {
public:
    explicit Reader(std::string_view text) : text_(text) {
        if (text_.size() > 16 * 1024 * 1024) {
            invalid();
        }
    }

    GalleryDemoJson run() {
        auto result = read(0);
        whitespace();
        if (position_ != text_.size()) {
            invalid();
        }
        return result;
    }

private:
    std::string_view text_;
    std::size_t position_{};
    std::size_t values_{};

    void whitespace() {
        while (position_ < text_.size() && (text_[position_] == ' ' || text_[position_] == '\n' ||
                                            text_[position_] == '\r' || text_[position_] == '\t')) {
            ++position_;
        }
    }

    bool take(char c) {
        whitespace();
        if (position_ < text_.size() && text_[position_] == c) {
            ++position_;
            return true;
        }
        return false;
    }

    std::uint32_t hex() {
        if (position_ + 4 > text_.size()) {
            invalid();
        }
        std::uint32_t code{};
        for (int i = 0; i < 4; ++i) {
            const char c = text_[position_++];
            const int digit = c >= '0' && c <= '9'   ? c - '0'
                              : c >= 'a' && c <= 'f' ? c - 'a' + 10
                              : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                                     : -1;
            if (digit < 0) {
                invalid();
            }
            code = code * 16 + static_cast<std::uint32_t>(digit);
        }
        return code;
    }

    std::string string() {
        if (!take('"')) {
            invalid();
        }
        std::string result;
        while (position_ < text_.size()) {
            const unsigned char c = text_[position_++];
            if (c == '"') {
                validate_utf8(result);
                return result;
            }
            if (c < 32) {
                invalid();
            }
            if (c != '\\') {
                result.push_back(static_cast<char>(c));
            } else {
                if (position_ >= text_.size()) {
                    invalid();
                }
                const char escape = text_[position_++];
                switch (escape) {
                case '"':
                case '\\':
                case '/':
                    result.push_back(escape);
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                case 'u': {
                    auto code = hex();
                    if (code >= 0xd800 && code <= 0xdbff) {
                        if (text_.substr(position_, 2) != "\\u") {
                            invalid();
                        }
                        position_ += 2;
                        const auto low = hex();
                        if (low < 0xdc00 || low > 0xdfff) {
                            invalid();
                        }
                        code = 0x10000 + (code - 0xd800) * 1024 + low - 0xdc00;
                    }
                    append_utf8(result, code);
                    break;
                }
                default:
                    invalid();
                }
            }
            if (result.size() > 1024 * 1024) {
                invalid();
            }
        }
        invalid();
    }

    GalleryDemoJson read(int depth) {
        whitespace();
        if (depth > 48 || ++values_ > 100000 || position_ == text_.size()) {
            invalid();
        }
        if (take('{')) {
            GalleryDemoJson::Object result;
            if (take('}')) {
                return result;
            }
            do {
                auto key = string();
                if (!take(':')) {
                    invalid();
                }
                if (!result.emplace(std::move(key), read(depth + 1)).second) {
                    invalid();
                }
            } while (take(','));
            if (!take('}')) {
                invalid();
            }
            return result;
        }
        if (take('[')) {
            GalleryDemoJson::Array result;
            if (take(']')) {
                return result;
            }
            do {
                result.push_back(read(depth + 1));
            } while (take(','));
            if (!take(']')) {
                invalid();
            }
            return result;
        }
        if (text_[position_] == '"') {
            return string();
        }
        for (const auto literal : {std::string_view{"null"}, std::string_view{"true"}, std::string_view{"false"}}) {
            if (text_.substr(position_, literal.size()) == literal) {
                position_ += literal.size();
                return literal == "null" ? GalleryDemoJson{} : GalleryDemoJson{literal == "true"};
            }
        }
        const auto start = position_;
        if (text_[position_] == '-') {
            ++position_;
        }
        const auto digits = [&] {
            const auto begin = position_;
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
                ++position_;
            }
            if (position_ == begin) {
                invalid();
            }
        };
        const auto integer_start = position_;
        digits();
        if (text_[integer_start] == '0' && position_ - integer_start > 1) {
            invalid();
        }
        bool real = false;
        if (position_ < text_.size() && text_[position_] == '.') {
            real = true;
            ++position_;
            digits();
        }
        if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
            real = true;
            ++position_;
            if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) {
                ++position_;
            }
            digits();
        }
        const auto last = text_.data() + position_;
        if (!real) {
            std::int64_t result{};
            const auto parsed = std::from_chars(text_.data() + start, last, result);
            if (parsed.ec != std::errc{} || parsed.ptr != last) {
                invalid();
            }
            return result;
        }
        double result{};
        const auto parsed = std::from_chars(text_.data() + start, last, result);
        if (parsed.ec != std::errc{} || parsed.ptr != last || !std::isfinite(result)) {
            invalid();
        }
        return result;
    }
};

} // namespace

const GalleryDemoJson& GalleryDemoJson::at(std::string_view key) const {
    const auto found = object().find(key);
    if (found == object().end()) {
        throw std::invalid_argument("missing Gallery scenario field: " + std::string{key});
    }
    return found->second;
}

GalleryDemoJson& GalleryDemoJson::at(std::string_view key) {
    return const_cast<GalleryDemoJson&>(std::as_const(*this).at(key));
}

const GalleryDemoJson::Object& GalleryDemoJson::object() const {
    return std::get<Object>(value);
}

const GalleryDemoJson::Array& GalleryDemoJson::array() const {
    return std::get<Array>(value);
}

const std::string& GalleryDemoJson::string() const {
    return std::get<std::string>(value);
}

double GalleryDemoJson::number() const {
    if (const auto integer = std::get_if<std::int64_t>(&value)) {
        return static_cast<double>(*integer);
    }
    return std::get<double>(value);
}

std::int64_t GalleryDemoJson::integer() const {
    return std::get<std::int64_t>(value);
}

bool GalleryDemoJson::boolean() const {
    return std::get<bool>(value);
}

bool GalleryDemoJson::contains(std::string_view key) const {
    return object().contains(key);
}

std::string GalleryDemoJson::dump() const {
    std::ostringstream out;
    std::visit(
        [&](const auto& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                out << "null";
            } else if constexpr (std::is_same_v<T, bool>) {
                out << (item ? "true" : "false");
            } else if constexpr (std::is_same_v<T, std::string>) {
                validate_utf8(item);
                out << '"';
                for (const unsigned char c : item) {
                    if (c == '"' || c == '\\') {
                        out << '\\' << static_cast<char>(c);
                    } else if (c < 32) {
                        out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
                    } else {
                        out << static_cast<char>(c);
                    }
                }
                out << '"';
            } else if constexpr (std::is_same_v<T, Array> || std::is_same_v<T, Object>) {
                out << (std::is_same_v<T, Array> ? '[' : '{');
                bool first = true;
                for (const auto& child : item) {
                    if (!first) {
                        out << ',';
                    }
                    first = false;
                    if constexpr (std::is_same_v<T, Array>) {
                        out << child.dump();
                    } else {
                        out << GalleryDemoJson{child.first}.dump() << ':' << child.second.dump();
                    }
                }
                out << (std::is_same_v<T, Array> ? ']' : '}');
            } else {
                if constexpr (std::is_same_v<T, double>) {
                    if (!std::isfinite(item)) {
                        invalid();
                    }
                }
                out << std::setprecision(std::numeric_limits<double>::max_digits10) << item;
            }
        },
        value);
    return out.str();
}

GalleryDemoJson GalleryDemoJson::parse(std::string_view input) {
    return Reader{input}.run();
}

void gallery_demo_fields(const GalleryDemoJson& value, std::initializer_list<std::string_view> required) {
    if (value.object().size() != required.size()) {
        throw std::invalid_argument("unknown or missing Gallery scenario field");
    }
    for (const auto field : required) {
        static_cast<void>(value.at(field));
    }
}

} // namespace rynui::example
