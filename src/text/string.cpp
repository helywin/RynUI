#include <ryn/string.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>

namespace ryn {
namespace {

constexpr std::u8string_view replacement_character = u8"\uFFFD";

struct DecodeResult final {
    bool valid{};
    std::size_t length{};
    std::size_t recovery_length{};
    Utf8Error error{};
};

template <typename Character>
[[nodiscard]] std::uint8_t byte_at(
    std::basic_string_view<Character> value,
    std::size_t offset) noexcept {
    return static_cast<std::uint8_t>(value[offset]);
}

[[nodiscard]] constexpr bool is_continuation(std::uint8_t byte) noexcept {
    return (byte & 0xC0U) == 0x80U;
}

[[nodiscard]] DecodeResult valid_sequence(std::size_t length) noexcept {
    return DecodeResult{true, length, length, {}};
}

[[nodiscard]] DecodeResult invalid_sequence(
    std::size_t byte_offset,
    Utf8ErrorKind kind,
    std::size_t recovery_length) noexcept {
    return DecodeResult{false, 0, std::max<std::size_t>(1, recovery_length),
                        Utf8Error{byte_offset, kind}};
}

template <typename Character>
[[nodiscard]] DecodeResult inspect_sequence(
    std::basic_string_view<Character> value,
    std::size_t offset) noexcept {
    const auto lead = byte_at(value, offset);
    if (lead <= 0x7FU) {
        return valid_sequence(1);
    }
    if (is_continuation(lead)) {
        return invalid_sequence(offset, Utf8ErrorKind::unexpected_continuation, 1);
    }
    if (lead == 0xC0U || lead == 0xC1U) {
        return invalid_sequence(offset, Utf8ErrorKind::overlong_sequence, 1);
    }
    if (lead >= 0xF5U && lead <= 0xF7U) {
        return invalid_sequence(offset, Utf8ErrorKind::code_point_out_of_range, 1);
    }
    if (lead >= 0xF8U) {
        return invalid_sequence(offset, Utf8ErrorKind::invalid_leading_byte, 1);
    }

    std::size_t expected_length = 0;
    if (lead <= 0xDFU) {
        expected_length = 2;
    } else if (lead <= 0xEFU) {
        expected_length = 3;
    } else {
        expected_length = 4;
    }

    for (std::size_t index = 1; index < expected_length; ++index) {
        if (offset + index >= value.size()) {
            return invalid_sequence(
                offset,
                Utf8ErrorKind::truncated_sequence,
                value.size() - offset);
        }
        if (!is_continuation(byte_at(value, offset + index))) {
            return invalid_sequence(
                offset + index,
                Utf8ErrorKind::invalid_continuation,
                index);
        }
    }

    const auto second = byte_at(value, offset + 1);
    if (lead == 0xE0U && second < 0xA0U) {
        return invalid_sequence(offset, Utf8ErrorKind::overlong_sequence, 3);
    }
    if (lead == 0xEDU && second >= 0xA0U) {
        return invalid_sequence(offset, Utf8ErrorKind::surrogate, 3);
    }
    if (lead == 0xF0U && second < 0x90U) {
        return invalid_sequence(offset, Utf8ErrorKind::overlong_sequence, 4);
    }
    if (lead == 0xF4U && second >= 0x90U) {
        return invalid_sequence(offset, Utf8ErrorKind::code_point_out_of_range, 4);
    }

    return valid_sequence(expected_length);
}

template <typename Character>
[[nodiscard]] std::optional<Utf8Error> validate_utf8(
    std::basic_string_view<Character> value) noexcept {
    std::size_t offset = 0;
    while (offset < value.size()) {
        const DecodeResult decoded = inspect_sequence(value, offset);
        if (!decoded.valid) {
            return decoded.error;
        }
        offset += decoded.length;
    }
    return std::nullopt;
}

template <typename Character>
[[nodiscard]] std::u8string copy_utf8(std::basic_string_view<Character> value) {
    std::u8string copy(value.size(), u8'\0');
    if (!value.empty()) {
        std::memcpy(copy.data(), value.data(), value.size());
    }
    return copy;
}

struct RepairStorage final {
    std::u8string value;
    std::size_t replacement_count{};
};

template <typename Character>
[[nodiscard]] RepairStorage repair_utf8(std::basic_string_view<Character> value) {
    RepairStorage repaired;
    repaired.value.reserve(value.size());

    std::size_t offset = 0;
    while (offset < value.size()) {
        const DecodeResult decoded = inspect_sequence(value, offset);
        if (decoded.valid) {
            for (std::size_t index = 0; index < decoded.length; ++index) {
                repaired.value.push_back(
                    static_cast<char8_t>(byte_at(value, offset + index)));
            }
            offset += decoded.length;
            continue;
        }

        repaired.value.append(replacement_character);
        ++repaired.replacement_count;
        offset += std::min(decoded.recovery_length, value.size() - offset);
    }

    return repaired;
}

[[nodiscard]] const char* error_kind_name(Utf8ErrorKind kind) noexcept {
    switch (kind) {
    case Utf8ErrorKind::unexpected_continuation:
        return "unexpected continuation";
    case Utf8ErrorKind::invalid_continuation:
        return "invalid continuation";
    case Utf8ErrorKind::truncated_sequence:
        return "truncated sequence";
    case Utf8ErrorKind::overlong_sequence:
        return "overlong sequence";
    case Utf8ErrorKind::surrogate:
        return "surrogate";
    case Utf8ErrorKind::code_point_out_of_range:
        return "code point out of range";
    case Utf8ErrorKind::invalid_leading_byte:
        return "invalid leading byte";
    }
    return "unknown UTF-8 error";
}

} // namespace

std::string_view StringView::bytes() const noexcept {
    if (value_.empty()) {
        return {};
    }
    return {reinterpret_cast<const char*>(value_.data()), value_.size()};
}

Utf8ParseResult String::from_utf8(std::u8string_view value) {
    if (const auto error = validate_utf8(value)) {
        return Utf8ParseResult{*error};
    }
    return Utf8ParseResult{String{copy_utf8(value), ValidatedUtf8{}}};
}

Utf8ParseResult String::from_utf8(std::string_view bytes) {
    if (const auto error = validate_utf8(bytes)) {
        return Utf8ParseResult{*error};
    }
    return Utf8ParseResult{String{copy_utf8(bytes), ValidatedUtf8{}}};
}

Utf8RepairResult String::from_utf8_lossy(std::u8string_view value) {
    RepairStorage repaired = repair_utf8(value);
    return Utf8RepairResult{
        String{std::move(repaired.value), ValidatedUtf8{}},
        repaired.replacement_count};
}

Utf8RepairResult String::from_utf8_lossy(std::string_view bytes) {
    RepairStorage repaired = repair_utf8(bytes);
    return Utf8RepairResult{
        String{std::move(repaired.value), ValidatedUtf8{}},
        repaired.replacement_count};
}

std::string_view String::bytes() const noexcept {
    if (value_.empty()) {
        return {};
    }
    return {reinterpret_cast<const char*>(value_.data()), value_.size()};
}

StringView String::view() const noexcept {
    return StringView{value_};
}

std::u8string String::copy_literal(const char8_t* literal, std::size_t extent) {
    if (extent == 0 || literal[extent - 1] != u8'\0') {
        throw std::invalid_argument("RynUI UTF-8 literal is not null terminated");
    }

    Utf8ParseResult parsed = from_utf8(
        std::u8string_view{literal, extent - 1});
    if (!parsed) {
        const Utf8Error error = parsed.error();
        throw std::invalid_argument(
            std::string{"Invalid RynUI UTF-8 literal at byte "}
            + std::to_string(error.byte_offset) + ": "
            + error_kind_name(error.kind));
    }
    return std::move(parsed).value().value_;
}

Utf8ParseResult::Utf8ParseResult(String value) : storage_(std::move(value)) {}

Utf8ParseResult::Utf8ParseResult(Utf8Error error) : storage_(error) {}

bool Utf8ParseResult::has_value() const noexcept {
    return std::holds_alternative<String>(storage_);
}

Utf8ParseResult::operator bool() const noexcept {
    return has_value();
}

const String& Utf8ParseResult::value() const& {
    return std::get<String>(storage_);
}

String&& Utf8ParseResult::value() && {
    return std::get<String>(std::move(storage_));
}

const Utf8Error& Utf8ParseResult::error() const& {
    return std::get<Utf8Error>(storage_);
}

} // namespace ryn
