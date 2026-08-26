#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace ryn {

enum class Utf8ErrorKind {
    unexpected_continuation,
    invalid_continuation,
    truncated_sequence,
    overlong_sequence,
    surrogate,
    code_point_out_of_range,
    invalid_leading_byte,
};

struct Utf8Error final {
    std::size_t byte_offset{};
    Utf8ErrorKind kind{Utf8ErrorKind::invalid_leading_byte};

    friend constexpr bool operator==(const Utf8Error&, const Utf8Error&) = default;
};

class Utf8ParseResult;
struct Utf8RepairResult;

class StringView final {
public:
    constexpr StringView() noexcept = default;

    [[nodiscard]] constexpr bool empty() const noexcept {
        return value_.empty();
    }

    [[nodiscard]] constexpr std::size_t size_bytes() const noexcept {
        return value_.size();
    }

    [[nodiscard]] constexpr const char8_t* data() const noexcept {
        return value_.data();
    }

    [[nodiscard]] constexpr std::u8string_view utf8() const noexcept {
        return value_;
    }

    [[nodiscard]] std::string_view bytes() const noexcept;

    friend constexpr bool operator==(const StringView&, const StringView&) = default;

private:
    friend class String;

    explicit constexpr StringView(std::u8string_view value) noexcept : value_(value) {}

    std::u8string_view value_{};
};

class String final {
public:
    String() = default;

    template <std::size_t N>
    String(const char8_t (&literal)[N]) : value_(copy_literal(literal, N)) {
        static_assert(N > 0);
    }

    [[nodiscard]] static Utf8ParseResult from_utf8(std::u8string_view value);
    [[nodiscard]] static Utf8ParseResult from_utf8(std::string_view bytes);
    [[nodiscard]] static Utf8RepairResult from_utf8_lossy(std::u8string_view value);
    [[nodiscard]] static Utf8RepairResult from_utf8_lossy(std::string_view bytes);

    [[nodiscard]] bool empty() const noexcept {
        return value_.empty();
    }

    [[nodiscard]] std::size_t size_bytes() const noexcept {
        return value_.size();
    }

    [[nodiscard]] const char8_t* data() const noexcept {
        return value_.data();
    }

    [[nodiscard]] std::u8string_view utf8() const noexcept {
        return value_;
    }

    [[nodiscard]] std::string_view bytes() const noexcept;
    [[nodiscard]] StringView view() const noexcept;

    friend bool operator==(const String&, const String&) = default;

private:
    struct ValidatedUtf8 final {};

    explicit String(std::u8string value, ValidatedUtf8) noexcept
        : value_(std::move(value)) {}

    [[nodiscard]] static std::u8string copy_literal(
        const char8_t* literal,
        std::size_t extent);

    std::u8string value_;
};

class Utf8ParseResult final {
public:
    [[nodiscard]] bool has_value() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

    [[nodiscard]] const String& value() const&;
    [[nodiscard]] String&& value() &&;
    [[nodiscard]] const Utf8Error& error() const&;

private:
    friend class String;

    explicit Utf8ParseResult(String value);
    explicit Utf8ParseResult(Utf8Error error);

    std::variant<String, Utf8Error> storage_;
};

struct Utf8RepairResult final {
    String value;
    std::size_t replacement_count{};
};

} // namespace ryn
