#pragma once

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

} // namespace ryn
