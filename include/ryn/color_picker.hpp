#pragma once
#include <ryn/button.hpp>
#include <ryn/input_types.hpp>
#include <ryn/text_direction.hpp>
#include <ryn/popover.hpp>
#include <ryn/component_locales.hpp>
#include <span>
#include <vector>

namespace ryn {
namespace detail {
struct ConfigDefaults;
}

namespace detail {
class ColorPickerComponentHost;
}
enum class ColorFormat { Hex, Rgb, Hsb };
enum class ColorPickerMode { Single, Gradient };

struct ColorRgb final {
    double r{};
    double g{};
    double b{};
    double a{1};
    friend bool operator==(const ColorRgb&, const ColorRgb&) = default;
};

struct ColorHsb final {
    double h{};
    double s{};
    double b{};
    double a{1};
    friend bool operator==(const ColorHsb&, const ColorHsb&) = default;
};

class PickerColor final {
public:
    PickerColor();
    explicit PickerColor(ColorRgb);
    explicit PickerColor(ColorHsb);
    explicit PickerColor(Color);
    [[nodiscard]] static std::optional<PickerColor> parse(std::string_view);

    [[nodiscard]] ColorRgb rgb() const noexcept {
        return rgb_;
    }

    [[nodiscard]] ColorHsb hsb() const noexcept;
    [[nodiscard]] Color color() const;
    [[nodiscard]] String format(ColorFormat, bool alpha = true) const;
    friend bool operator==(const PickerColor&, const PickerColor&) = default;

private:
    ColorRgb rgb_;
    double hue_{};
};

struct ColorStop final {
    String key;
    PickerColor color;
    double percent{};
    friend bool operator==(const ColorStop&, const ColorStop&) = default;
};

class ColorPickerValue final {
public:
    ColorPickerValue() = default;
    ColorPickerValue(PickerColor);
    explicit ColorPickerValue(std::vector<ColorStop>);

    [[nodiscard]] bool empty() const noexcept {
        return !solid_ && stops_.empty();
    }

    [[nodiscard]] bool gradient() const noexcept {
        return !stops_.empty();
    }

    [[nodiscard]] std::optional<PickerColor> solid() const noexcept {
        return solid_;
    }

    [[nodiscard]] std::span<const ColorStop> stops() const noexcept {
        return stops_;
    }

    [[nodiscard]] PickerColor sample(double percent) const;
    [[nodiscard]] String format(ColorFormat = ColorFormat::Hex, bool alpha = true) const;
    friend bool operator==(const ColorPickerValue&, const ColorPickerValue&) = default;

private:
    std::optional<PickerColor> solid_;
    std::vector<ColorStop> stops_;
};

struct ColorPreset final {
    String key;
    String label;
    std::vector<ColorPickerValue> colors;
    bool default_open{true};
    friend bool operator==(const ColorPreset&, const ColorPreset&) = default;
};

struct ColorPickerTriggerSlot final {};

struct ColorPickerPanelSlot final {};

struct ColorPickerPresetsSlot final {};

struct ColorPickerExtraSlot final {};

using ColorPickerTrigger = SlotContent<ColorPickerTriggerSlot>;
using ColorPickerPanel = SlotContent<ColorPickerPanelSlot>;
using ColorPickerPresets = SlotContent<ColorPickerPresetsSlot>;
using ColorPickerExtra = SlotContent<ColorPickerExtraSlot>;

struct ColorPickerPanelParts final {
    ColorPickerPanel picker;
    ColorPickerPresets presets;
    void renderPicker() const;
    void renderPresets() const;
};

struct ColorPickerTextContext final {
    Prop<ColorPickerValue> value;
    Prop<String> text;
    Prop<ColorFormat> format;
};

struct ColorPickerSlots final {
    std::optional<ColorPickerTrigger> trigger;
    std::function<void(ColorPickerPanelParts)> panel_render;
    std::function<void(ColorPickerTextContext)> text_render;
    std::optional<ColorPickerExtra> extra;
};

class ColorPickerProps final {
public:
    ColorPickerProps& locale(Prop<ColorPickerLocale> value) {
        locale_ = std::move(value);
        return *this;
    }

    ColorPickerProps& direction(Prop<TextDirection> value) {
        direction_ = std::move(value);
        explicit_direction_ = true;
        return *this;
    }

    ColorPickerProps& value(Prop<ColorPickerValue> v) {
        value_ = std::move(v);
        return *this;
    }

    ColorPickerProps& defaultValue(ColorPickerValue v) {
        initial_ = std::move(v);
        return *this;
    }

    ColorPickerProps& format(Prop<ColorFormat> v) {
        format_ = std::move(v);
        return *this;
    }

    ColorPickerProps& defaultFormat(ColorFormat v) {
        initial_format_ = v;
        return *this;
    }

    ColorPickerProps& modes(Prop<std::vector<ColorPickerMode>> v) {
        modes_ = std::move(v);
        return *this;
    }

    ColorPickerProps& mode(ColorPickerMode v) {
        modes_ = std::vector<ColorPickerMode>{v};
        return *this;
    }

    ColorPickerProps& presets(Prop<std::vector<ColorPreset>> v) {
        presets_ = std::move(v);
        return *this;
    }

    ColorPickerProps& disabled(Prop<bool> v) {
        disabled_ = std::move(v);
        explicit_disabled_ = true;
        return *this;
    }

    ColorPickerProps& disabledAlpha(Prop<bool> v) {
        disabled_alpha_ = std::move(v);
        return *this;
    }

    ColorPickerProps& disabledFormat(Prop<bool> v) {
        disabled_format_ = std::move(v);
        return *this;
    }

    ColorPickerProps& allowClear(Prop<bool> v) {
        clear_ = std::move(v);
        return *this;
    }

    ColorPickerProps& showText(Prop<bool> v) {
        show_text_ = std::move(v);
        return *this;
    }

    ColorPickerProps& size(Prop<ControlSize> v) {
        size_ = std::move(v);
        explicit_size_ = true;
        return *this;
    }

    ColorPickerProps& status(Prop<InputStatus> v) {
        status_ = std::move(v);
        return *this;
    }

    ColorPickerProps& open(Prop<bool> v) {
        open_ = std::move(v);
        return *this;
    }

    ColorPickerProps& defaultOpen(bool v) {
        initial_open_ = v;
        return *this;
    }

    ColorPickerProps& trigger(Prop<PopoverTriggerMode> v) {
        trigger_ = std::move(v);
        return *this;
    }

    ColorPickerProps& placement(Prop<PopoverPlacement> v) {
        placement_ = std::move(v);
        return *this;
    }

    ColorPickerProps& arrow(Prop<bool> v) {
        arrow_ = std::move(v);
        return *this;
    }

    ColorPickerProps& autoAdjustOverflow(Prop<bool> v) {
        adjust_ = std::move(v);
        return *this;
    }

    ColorPickerProps& onChange(std::function<void(ColorPickerValue, String)> v) {
        change_ = std::move(v);
        return *this;
    }

    ColorPickerProps& onChangeComplete(std::function<void(ColorPickerValue)> v) {
        complete_ = std::move(v);
        return *this;
    }

    ColorPickerProps& onClear(std::function<void()> v) {
        on_clear_ = std::move(v);
        return *this;
    }

    ColorPickerProps& onFormatChange(std::function<void(ColorFormat)> v) {
        format_change_ = std::move(v);
        return *this;
    }

    ColorPickerProps& onOpenChange(std::function<void(bool)> v) {
        open_change_ = std::move(v);
        return *this;
    }

    ColorPickerProps& layout(LayoutStyle v) {
        layout_ = std::move(v);
        return *this;
    }

private:
    friend struct detail::ConfigDefaults;
    friend class detail::ColorPickerComponentHost;
    std::optional<Prop<ColorPickerLocale>> locale_;
    Prop<TextDirection> direction_{TextDirection::LeftToRight};
    bool explicit_direction_{};
    std::optional<Prop<ColorPickerValue>> value_;
    std::optional<ColorPickerValue> initial_;
    std::optional<Prop<ColorFormat>> format_;
    std::optional<ColorFormat> initial_format_;
    Prop<std::vector<ColorPickerMode>> modes_{std::vector<ColorPickerMode>{ColorPickerMode::Single}};
    Prop<std::vector<ColorPreset>> presets_{std::vector<ColorPreset>{}};
    Prop<bool> disabled_{false};
    bool explicit_disabled_{};
    Prop<bool> disabled_alpha_{false};
    Prop<bool> disabled_format_{false};
    Prop<bool> clear_{false};
    Prop<bool> show_text_{false};
    Prop<ControlSize> size_{ControlSize::Middle};
    bool explicit_size_{};
    Prop<InputStatus> status_{InputStatus::Default};
    std::optional<Prop<bool>> open_;
    std::optional<bool> initial_open_;
    Prop<PopoverTriggerMode> trigger_{PopoverTriggerMode::Click};
    Prop<PopoverPlacement> placement_{PopoverPlacement::BottomLeft};
    Prop<bool> arrow_{false};
    Prop<bool> adjust_{true};
    std::function<void(ColorPickerValue, String)> change_;
    std::function<void(ColorPickerValue)> complete_;
    std::function<void()> on_clear_;
    std::function<void(ColorFormat)> format_change_;
    std::function<void(bool)> open_change_;
    LayoutStyle layout_;
};

void ColorPicker(ColorPickerProps, ColorPickerSlots = {});
} // namespace ryn
