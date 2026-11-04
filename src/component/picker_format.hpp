#pragma once
#include <ryn/picker_common.hpp>
#include <cctype>
#include <stdexcept>
#include <string_view>

namespace ryn::detail {
struct FormatPart final {
    std::string text;
    bool token{};
};

inline std::vector<FormatPart> tokenize_picker_format(std::string_view format, std::string_view symbols) {
    if (format.empty()) {
        throw std::invalid_argument("Picker format cannot be empty");
    }
    std::vector<FormatPart> result;
    for (std::size_t i = 0; i < format.size();) {
        if (format[i] == '[') {
            const auto end = format.find(']', i + 1);
            if (end == std::string_view::npos) {
                throw std::invalid_argument("Unterminated picker format literal");
            }
            result.push_back({std::string{format.substr(i + 1, end - i - 1)}, false});
            i = end + 1;
        } else if (symbols.find(format[i]) != std::string_view::npos) {
            const auto start = i++;
            while (i < format.size() && format[i] == format[start]) {
                ++i;
            }
            result.push_back({std::string{format.substr(start, i - start)}, true});
        } else {
            if (std::isalpha(static_cast<unsigned char>(format[i]))) {
                throw std::invalid_argument("Unsupported picker format token; quote literals with [..]");
            }
            result.push_back({std::string(1, format[i++]), false});
        }
    }
    return result;
}

inline std::pair<std::string_view, std::string_view> split_date_time_format(std::string_view format) {
    bool literal{};
    for (std::size_t i = 0; i < format.size(); ++i) {
        if (format[i] == '[') {
            literal = true;
        } else if (format[i] == ']') {
            literal = false;
        } else if (!literal && (format[i] == 'H' || format[i] == 'h')) {
            return {format.substr(0, i), format.substr(i)};
        }
    }
    throw std::invalid_argument("Date-time format requires date and hour fields");
}

inline std::string picker_number(int value, int width) {
    if (value < 0) {
        return "-" + picker_number(-value, width);
    }
    auto text = std::to_string(value);
    if (text.size() < static_cast<std::size_t>(width)) {
        text.insert(0, static_cast<std::size_t>(width) - text.size(), '0');
    }
    return text;
}

inline std::optional<int> picker_read_number(std::string_view text, std::size_t& offset, int min, int max) {
    const auto start = offset;
    int result{};
    while (offset < text.size() && offset - start < static_cast<std::size_t>(max) && text[offset] >= '0' &&
           text[offset] <= '9') {
        result = result * 10 + text[offset++] - '0';
    }
    return offset - start >= static_cast<std::size_t>(min) ? std::optional{result} : std::nullopt;
}

inline bool picker_read_literal(std::string_view text, std::size_t& offset, std::string_view literal) {
    if (text.substr(offset, literal.size()) != literal) {
        return false;
    }
    offset += literal.size();
    return true;
}
} // namespace ryn::detail
