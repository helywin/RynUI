#pragma once
#include <ryn/input.hpp>
#include <vector>

namespace ryn {
namespace detail {
struct OTPPropsAccess;
struct OTPRefState;
} // namespace detail
enum class OTPDirection { LeftToRight, RightToLeft };

struct OTPMask final {
    bool enabled{};
    String glyph{u8"•"};
    friend bool operator==(const OTPMask&, const OTPMask&) = default;
};

class OTPRef final {
public:
    OTPRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] bool focus() const;
    [[nodiscard]] bool blur() const;

private:
    friend class OTPProps;
    std::shared_ptr<detail::OTPRefState> state_;
};

struct OTPSeparatorSlot final {};

using OTPSeparatorContent = SlotContent<OTPSeparatorSlot>;
using OTPSeparator = std::function<std::optional<OTPSeparatorContent>(std::size_t)>;

class OTPProps final {
public:
    OTPProps& value(Prop<String> value) {
        value_ = std::move(value);
        return *this;
    }

    template <std::size_t N> OTPProps& value(const char8_t (&value)[N]) {
        return this->value(String{value});
    }

    OTPProps& defaultValue(String value) {
        default_value_ = std::move(value);
        return *this;
    }

    template <std::size_t N> OTPProps& defaultValue(const char8_t (&value)[N]) {
        return defaultValue(String{value});
    }

    OTPProps& length(Prop<std::size_t> value) {
        length_ = std::move(value);
        return *this;
    }

    OTPProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    OTPProps& variant(Prop<InputVariant> value) {
        variant_ = std::move(value);
        return *this;
    }

    OTPProps& status(Prop<InputStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    OTPProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    OTPProps& readOnly(Prop<bool> value) {
        read_only_ = std::move(value);
        return *this;
    }

    OTPProps& direction(Prop<OTPDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    OTPProps& mask(Prop<OTPMask> value) {
        mask_ = std::move(value);
        return *this;
    }

    OTPProps& purpose(Prop<InputPurpose> value) {
        purpose_ = std::move(value);
        return *this;
    }

    OTPProps& capitalization(Prop<InputCapitalization> value) {
        capitalization_ = std::move(value);
        return *this;
    }

    OTPProps& autocorrect(Prop<bool> value) {
        autocorrect_ = std::move(value);
        return *this;
    }

    OTPProps& mask(bool enabled = true) {
        return mask(OTPMask{enabled});
    }

    OTPProps& mask(String glyph) {
        return mask(OTPMask{true, std::move(glyph)});
    }

    template <std::size_t N> OTPProps& mask(const char8_t (&value)[N]) {
        return mask(String{value});
    }

    OTPProps& formatter(std::function<String(String)> callback) {
        formatter_ = std::move(callback);
        return *this;
    }

    OTPProps& onInput(std::function<void(const std::vector<String>&)> callback) {
        on_input_ = std::move(callback);
        return *this;
    }

    OTPProps& onChange(std::function<void(String)> callback) {
        on_change_ = std::move(callback);
        return *this;
    }

    OTPProps& onFocus(std::function<void(std::size_t)> callback) {
        on_focus_ = std::move(callback);
        return *this;
    }

    OTPProps& onBlur(std::function<void(std::size_t)> callback) {
        on_blur_ = std::move(callback);
        return *this;
    }

    OTPProps& ref(const OTPRef& ref) {
        reference_ = ref.state_;
        return *this;
    }

    OTPProps& autoFocus(bool enabled = true) {
        auto_focus_ = enabled;
        return *this;
    }

    OTPProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::OTPPropsAccess;
    std::optional<Prop<String>> value_;
    std::optional<String> default_value_;
    Prop<std::size_t> length_{6};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<InputVariant> variant_{InputVariant::Outlined};
    Prop<InputStatus> status_{InputStatus::Default};
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    Prop<OTPDirection> direction_{OTPDirection::LeftToRight};
    Prop<OTPMask> mask_{OTPMask{}};
    Prop<InputPurpose> purpose_{InputPurpose::Number};
    Prop<InputCapitalization> capitalization_{InputCapitalization::None};
    Prop<bool> autocorrect_{false};
    std::function<String(String)> formatter_;
    std::function<void(const std::vector<String>&)> on_input_;
    std::function<void(String)> on_change_;
    std::function<void(std::size_t)> on_focus_;
    std::function<void(std::size_t)> on_blur_;
    std::shared_ptr<detail::OTPRefState> reference_;
    bool auto_focus_{};
    LayoutStyle layout_;
};

void OTP(OTPProps props = {}, std::optional<OTPSeparator> separator = {});
} // namespace ryn
