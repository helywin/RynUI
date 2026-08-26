#include <ryn/string.hpp>

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

using Utf8Literal = const char8_t (&)[4];
using NarrowLiteral = const char (&)[4];

static_assert(std::is_convertible_v<Utf8Literal, ryn::String>);
static_assert(!std::is_convertible_v<NarrowLiteral, ryn::String>);
static_assert(!std::is_convertible_v<const char8_t*, ryn::String>);
static_assert(std::is_trivially_copyable_v<ryn::StringView>);

template <typename Value>
concept HasSubscript = requires(const Value& value) {
    value[std::size_t{}];
};

static_assert(!HasSubscript<ryn::String>);
static_assert(!HasSubscript<ryn::StringView>);

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string bytes(std::initializer_list<unsigned int> values) {
    std::string result;
    result.reserve(values.size());
    for (const unsigned int value : values) {
        result.push_back(static_cast<char>(value));
    }
    return result;
}

void require_error(
    std::string_view input,
    ryn::Utf8ErrorKind expected_kind,
    std::size_t expected_offset,
    const char* message) {
    const auto parsed = ryn::String::from_utf8(input);
    require(!parsed, message);
    require(parsed.error().kind == expected_kind, "unexpected UTF-8 error kind");
    require(parsed.error().byte_offset == expected_offset,
            "unexpected UTF-8 error offset");
}

void test_valid_values_and_views() {
    ryn::String empty;
    require(empty.empty(), "default String is not empty");
    require(empty.size_bytes() == 0, "empty String has bytes");

    const auto parsed_empty = ryn::String::from_utf8(std::string_view{});
    require(parsed_empty.has_value(), "strict parser rejected empty UTF-8");
    require(parsed_empty.value().empty(), "strict empty UTF-8 produced content");

    ryn::String title = u8"设备监控";
    constexpr std::u8string_view expected = u8"设备监控";
    require(title.utf8() == expected, "CJK literal bytes changed");
    require(title.size_bytes() == expected.size(), "CJK byte length changed");

    const ryn::StringView view = title.view();
    require(view.data() == title.data(), "StringView did not borrow String storage");
    require(view.size_bytes() == title.size_bytes(), "StringView length changed");
    require(view.utf8() == expected, "StringView UTF-8 data changed");
    require(view.bytes() == title.bytes(), "byte adapter data changed");

    const auto runtime = ryn::String::from_utf8(std::u8string_view{u8"RynUI 中文"});
    require(runtime.has_value(), "valid char8_t runtime input failed");
    require(runtime.value().utf8() == u8"RynUI 中文", "runtime UTF-8 changed");

    const auto byte_runtime = ryn::String::from_utf8(std::string_view{"RynUI"});
    require(byte_runtime.has_value(), "valid char byte input failed");
    require(byte_runtime.value().bytes() == "RynUI", "char byte adapter changed data");
}

void test_strict_errors() {
    require_error(bytes({0xE2, 0x82}),
                  ryn::Utf8ErrorKind::truncated_sequence,
                  0,
                  "truncated sequence was accepted");
    require_error(bytes({0xE2, 0x28, 0xA1}),
                  ryn::Utf8ErrorKind::invalid_continuation,
                  1,
                  "invalid continuation was accepted");
    require_error(bytes({0x80}),
                  ryn::Utf8ErrorKind::unexpected_continuation,
                  0,
                  "unexpected continuation was accepted");
    require_error(bytes({0xC0, 0xAF}),
                  ryn::Utf8ErrorKind::overlong_sequence,
                  0,
                  "overlong sequence was accepted");
    require_error(bytes({0xED, 0xA0, 0x80}),
                  ryn::Utf8ErrorKind::surrogate,
                  0,
                  "UTF-8 surrogate was accepted");
    require_error(bytes({0xF4, 0x90, 0x80, 0x80}),
                  ryn::Utf8ErrorKind::code_point_out_of_range,
                  0,
                  "out-of-range code point was accepted");
    require_error(bytes({0xFF}),
                  ryn::Utf8ErrorKind::invalid_leading_byte,
                  0,
                  "invalid leading byte was accepted");
}

void test_lossy_repairs() {
    const std::string malformed = bytes({0xE2, 0x28, 0xA1});
    const auto repaired = ryn::String::from_utf8_lossy(malformed);
    require(repaired.replacement_count == 2,
            "lossy repair returned the wrong replacement count");
    require(repaired.value.utf8() == u8"�(�",
            "lossy repair returned the wrong normalized UTF-8");

    const auto truncated = ryn::String::from_utf8_lossy(bytes({0xE2, 0x82}));
    require(truncated.replacement_count == 1,
            "truncated sequence was not repaired as one invalid subsequence");
    require(truncated.value.utf8() == u8"�",
            "truncated sequence repair returned unexpected bytes");

    const auto valid = ryn::String::from_utf8_lossy(std::u8string_view{u8"abc中文"});
    require(valid.replacement_count == 0, "valid UTF-8 was reported as repaired");
    require(valid.value.utf8() == u8"abc中文", "valid lossy input changed");
}

} // namespace

int main() {
    try {
        test_valid_values_and_views();
        test_strict_errors();
        test_lossy_repairs();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
