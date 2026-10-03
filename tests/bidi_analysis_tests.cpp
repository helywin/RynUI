#include "text/bidi_analysis.hpp"
#include "support/allocation_probe.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace {
using namespace ryn;
using namespace ryn::text;

void check(bool valid, const char* message) {
    if (!valid) {
        throw std::runtime_error(message);
    }
}

void model_contract() {
    BidiAnalysis model;
    check(!model.assigned() && !model.line_runs(0, 0), "unassigned analysis accepted a range");
    check(model.assign(String{}) && model.paragraphs().size() == 1 && model.line_runs(0, 0)->empty(),
          "empty paragraph");
    check(model.assign(String{u8"abc אבג 123\n\nمرحبا (中文)\n"}), "mixed paragraph");
    const auto size = model.source().size_bytes();
    check(model.paragraphs().size() == 4 && model.paragraphs()[0].base_level == 0 &&
              model.paragraphs()[1].byte_begin == model.paragraphs()[1].content_end &&
              model.paragraphs()[2].base_level == 1 && model.paragraphs()[3].byte_end == size,
          "P1/base direction");
    check(model.level_at(0) == 0 && model.level_at(4) == 1 && !model.level_at(5) && !model.level_at(size),
          "scalar level indexing");
    check(!model.line_runs(1, size) && !model.line_runs(4, 5) && !model.line_runs(size + 1, size + 1) &&
              !model.line_runs(2, 1),
          "illegal line range accepted");
    const auto line = model.line_runs(0, model.paragraphs()[0].content_end);
    check(line && line->size() >= 3 && std::ranges::any_of(*line, [](const auto& run) { return run.right_to_left(); }),
          "mixed visual line runs");
    check(std::ranges::any_of(model.scripts(), [](const auto& run) { return run.unicode_tag == 0x41726162; }) &&
              std::ranges::any_of(model.scripts(), [](const auto& run) { return run.unicode_tag == 0x48656272; }),
          "script tags missing");
    const auto copy = model;
    check(!model.assign(String{u8"bad"}, static_cast<TextDirection>(255)) && model == copy,
          "invalid enum changed source");
    check(model.assign(String{u8"123"}, TextDirection::RightToLeft) && model.paragraphs()[0].base_level == 1 &&
              copy.source().size_bytes() == size && copy.line_runs(0, copy.paragraphs()[0].content_end),
          "copy lifetime");
    BidiAnalysis equal;
    check(equal.assign(String{u8"123"}, TextDirection::RightToLeft) && equal == model, "semantic equality");
    const auto* levels = model.levels().data();
    check(model.assign(String{u8"123"}, TextDirection::RightToLeft) && model.levels().data() == levels,
          "same source rebuilt");
    BidiAnalysis whitespace;
    check(whitespace.assign(String{u8"אבג   xyz"}, TextDirection::RightToLeft), "line whitespace setup");
    const auto short_line = whitespace.line_runs(0, 9);
    check(short_line &&
              std::ranges::any_of(*short_line, [](const auto& run) { return run.byte_end == 9 && run.level == 1; }),
          "line L1 trailing whitespace did not use paragraph level");
    {
        ryn_test::allocation::begin();
        for (int index = 0; index < 1000; ++index) {
            static_cast<void>(copy.source());
            static_cast<void>(copy.paragraphs());
            static_cast<void>(copy.scripts());
            static_cast<void>(copy.level_at(4));
            static_cast<void>(copy.scalar_boundary(4));
            check(copy == copy, "analysis equality");
        }
        check(ryn_test::allocation::end() == 0, "analysis scalar queries allocated");
    }
}

std::string_view trim(std::string_view text) {
    const auto begin = text.find_first_not_of(" \t\r");
    return begin == text.npos ? std::string_view{} : text.substr(begin, text.find_last_not_of(" \t\r") - begin + 1);
}

template <class Callback> void tokens(std::string_view text, Callback callback) {
    while (!(text = trim(text)).empty()) {
        const auto end = text.find_first_of(" \t\r");
        callback(text.substr(0, end));
        text = end == text.npos ? std::string_view{} : text.substr(end + 1);
    }
}

int number(std::string_view text, int base = 10) {
    int value{};
    text = trim(text);
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, base);
    check(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size(), "invalid conformance number");
    return value;
}

std::vector<int> numbers(std::string_view text) {
    std::vector<int> result;
    tokens(text, [&](auto token) { result.push_back(token == "x" ? -1 : number(token)); });
    return result;
}

void append(std::string& text, char32_t codepoint) {
    if (codepoint < 0x80) {
        text.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
        text.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        text.push_back(static_cast<char>(0x80 | (codepoint & 63)));
    } else if (codepoint < 0x10000) {
        text.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        text.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 63)));
        text.push_back(static_cast<char>(0x80 | (codepoint & 63)));
    } else {
        text.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        text.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 63)));
        text.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 63)));
        text.push_back(static_cast<char>(0x80 | (codepoint & 63)));
    }
}

void verify(const std::vector<char32_t>& codepoints, TextDirection direction, int base,
            const std::vector<int>& expected_levels, const std::vector<int>& expected_order) {
    std::string bytes;
    std::vector<std::size_t> offsets;
    for (const auto codepoint : codepoints) {
        offsets.push_back(bytes.size());
        append(bytes, codepoint);
    }
    offsets.push_back(bytes.size());
    BidiAnalysis model;
    check(model.assign(String::from_utf8(bytes).value(), direction), "conformance source rejected");
    check(base < 0 || model.paragraphs()[0].base_level == base, "conformance paragraph level mismatch");
    const auto runs = model.line_runs(0, bytes.size());
    check(runs.has_value() && expected_levels.size() == codepoints.size(), "conformance line absent");
    std::vector<int> order;
    for (const auto& run : *runs) {
        const auto begin = std::lower_bound(offsets.begin(), offsets.end(), run.byte_begin);
        const auto end = std::lower_bound(begin, offsets.end(), run.byte_end);
        check(begin != offsets.end() && *begin == run.byte_begin && end != offsets.end() && *end == run.byte_end,
              "conformance visual run split UTF-8");
        const auto emit = [&](auto offset) {
            const auto index = static_cast<std::size_t>(offset - offsets.begin());
            if (expected_levels[index] >= 0) {
                check(expected_levels[index] == run.level, "conformance line level mismatch");
                order.push_back(static_cast<int>(index));
            }
        };
        if (run.right_to_left()) {
            for (auto offset = end; offset != begin;) {
                emit(--offset);
            }
        } else {
            for (auto offset = begin; offset != end; ++offset) {
                emit(offset);
            }
        }
    }
    check(order == expected_order, "conformance visual order mismatch");
}

std::size_t character_conformance() {
    std::ifstream file{RYNUI_BIDI_CHARACTER_TEST_FILE};
    check(bool(file), "BidiCharacterTest fixture unavailable");
    std::string line;
    std::size_t lines{};
    std::size_t cases{};
    while (std::getline(file, line)) {
        ++lines;
        auto text = trim(std::string_view{line}.substr(0, line.find('#')));
        if (text.empty()) {
            continue;
        }
        std::array<std::string_view, 5> fields;
        for (auto& field : fields) {
            const auto end = text.find(';');
            field = trim(text.substr(0, end));
            text = end == text.npos ? std::string_view{} : text.substr(end + 1);
        }
        std::vector<char32_t> codepoints;
        tokens(fields[0], [&](auto token) { codepoints.push_back(static_cast<char32_t>(number(token, 16))); });
        const auto requested = number(fields[1]);
        try {
            verify(codepoints,
                   requested == 0   ? TextDirection::LeftToRight
                   : requested == 1 ? TextDirection::RightToLeft
                                    : TextDirection::Auto,
                   number(fields[2]), numbers(fields[3]), numbers(fields[4]));
        } catch (const std::exception& error) {
            throw std::runtime_error("BidiCharacterTest line " + std::to_string(lines) + ": " + error.what());
        }
        ++cases;
    }
    check(cases > 90000, "BidiCharacterTest inventory unexpectedly small");
    return cases;
}

char32_t representative(std::string_view name) {
    constexpr std::array<std::pair<std::string_view, char32_t>, 23> types{
        {{"L", U'A'},        {"R", U'א'},        {"AL", U'ا'},       {"EN", U'1'},       {"ES", U'+'},
         {"ET", U'$'},       {"AN", U'١'},       {"CS", U','},       {"NSM", U'\u0300'}, {"BN", U'\u00ad'},
         {"B", U'\u2029'},   {"S", U'\t'},       {"WS", U' '},       {"ON", U'!'},       {"LRE", U'\u202a'},
         {"LRO", U'\u202d'}, {"RLE", U'\u202b'}, {"RLO", U'\u202e'}, {"PDF", U'\u202c'}, {"LRI", U'\u2066'},
         {"RLI", U'\u2067'}, {"FSI", U'\u2068'}, {"PDI", U'\u2069'}}};
    for (const auto& [type, codepoint] : types) {
        if (name == type) {
            return codepoint;
        }
    }
    throw std::runtime_error("unknown conformance bidi type");
}

std::size_t type_conformance() {
    std::ifstream file{RYNUI_BIDI_TEST_FILE};
    check(bool(file), "BidiTest fixture unavailable");
    std::string line;
    std::vector<int> levels;
    std::vector<int> order;
    std::size_t cases{};
    std::size_t lines{};
    while (std::getline(file, line)) {
        ++lines;
        auto text = trim(std::string_view{line}.substr(0, line.find('#')));
        if (text.empty()) {
            continue;
        }
        if (text.starts_with("@Levels:")) {
            levels = numbers(text.substr(8));
            continue;
        }
        if (text.starts_with("@Reorder:")) {
            order = numbers(text.substr(9));
            continue;
        }
        if (text.starts_with('@')) {
            continue;
        }
        const auto separator = text.find(';');
        check(separator != text.npos, "invalid BidiTest data");
        std::vector<char32_t> codepoints;
        tokens(text.substr(0, separator), [&](auto type) { codepoints.push_back(representative(type)); });
        const auto directions = number(text.substr(separator + 1), 16);
        for (int flag = 1; flag <= 4; flag *= 2) {
            if (!(directions & flag)) {
                continue;
            }
            try {
                verify(codepoints,
                       flag == 1   ? TextDirection::Auto
                       : flag == 2 ? TextDirection::LeftToRight
                                   : TextDirection::RightToLeft,
                       -1, levels, order);
            } catch (const std::exception& error) {
                throw std::runtime_error("BidiTest line " + std::to_string(lines) + ": " + error.what());
            }
            ++cases;
        }
    }
    check(cases > 700000, "BidiTest inventory unexpectedly small");
    return cases;
}
} // namespace

int main() {
    try {
        model_contract();
        const auto characters = character_conformance();
        const auto types = type_conformance();
        std::cout << "unicode=17.0.0 bidi_character_cases=" << characters << " bidi_type_cases=" << types << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
