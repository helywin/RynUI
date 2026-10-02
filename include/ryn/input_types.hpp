#pragma once

#include <ryn/string.hpp>
#include <cstddef>
#include <optional>

namespace ryn {

enum class InputPurpose { Text, Name, Email, Username, Number };

enum class InputCapitalization { None, Sentences, Words, Letters };

enum class InputFocusCursor { Keep, Start, End, All };

struct InputFocusOptions final {
    InputFocusCursor cursor{InputFocusCursor::Keep};
    friend constexpr bool operator==(InputFocusOptions, InputFocusOptions) = default;
};

enum class InputStatus { Default, Warning, Error };

enum class InputVariant { Outlined, Filled, Borderless, Underlined };

enum class InputCountUnit { Scalar, Grapheme };

struct InputCountOptions final {
    std::optional<std::size_t> max;
    InputCountUnit unit{InputCountUnit::Scalar};
    friend constexpr bool operator==(InputCountOptions, InputCountOptions) = default;
};

struct InputCountInfo final {
    String value;
    std::size_t count{};
    std::optional<std::size_t> max;
    bool exceeded{};
};

} // namespace ryn
