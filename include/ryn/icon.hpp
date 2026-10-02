#pragma once

#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/text.hpp>

#include <cstdint>
#include <cstddef>
#include <optional>
#include <memory>
#include <span>
#include <variant>
#include <vector>
#include <utility>

namespace ryn {
namespace detail {
struct IconPropsAccess;
struct IconSourceAccess;
struct IconVectorData;
} // namespace detail

enum class IconName : std::uint16_t {
#include <ryn/generated/icon_names.inc>
};

// Values 0-13 preserve the original API; all names and layers are generated
// from third_party/ant-design-icons/manifest.json.
inline constexpr IconName last_bundled_icon = static_cast<IconName>(847);
inline constexpr std::size_t bundled_icon_count = 848;

struct IconPoint final {
    float x{};
    float y{};
    friend bool operator==(IconPoint, IconPoint) = default;
};

struct IconViewBox final {
    float x{};
    float y{};
    float width{};
    float height{};
    friend bool operator==(IconViewBox, IconViewBox) = default;
};

struct IconMove final {
    IconPoint to;
};

struct IconLine final {
    IconPoint to;
};

struct IconQuadratic final {
    IconPoint control;
    IconPoint to;
};

struct IconCubic final {
    IconPoint control1;
    IconPoint control2;
    IconPoint to;
};

struct IconClose final {};

using IconPathCommand = std::variant<IconMove, IconLine, IconQuadratic, IconCubic, IconClose>;

enum class IconColorRole : std::uint8_t { Primary, Secondary };

struct IconPath final {
    IconColorRole color{IconColorRole::Primary};
    std::vector<IconPathCommand> commands;
    float opacity{1};
};

class IconVector final {
public:
    IconVector(IconViewBox view_box, std::vector<IconPath> paths);
    [[nodiscard]] IconViewBox view_box() const noexcept;
    [[nodiscard]] std::span<const IconPath> paths() const noexcept;
    friend bool operator==(const IconVector&, const IconVector&) = default;

private:
    friend class IconSource;
    std::shared_ptr<const detail::IconVectorData> data_;
};

class IconSource final {
public:
    IconSource() = default;

    IconSource(IconName name) : name_(name) {}

    IconSource(IconVector vector) : name_(std::nullopt), vector_(std::move(vector.data_)) {}

    [[nodiscard]] std::optional<IconName> bundled_name() const noexcept {
        return name_;
    }

    friend bool operator==(const IconSource&, const IconSource&) = default;

private:
    friend struct detail::IconSourceAccess;
    std::optional<IconName> name_{IconName::EyeOutlined};
    std::shared_ptr<const detail::IconVectorData> vector_;
};

struct IconTwoToneColor final {
    Color primary;
    std::optional<Color> secondary;

    friend bool operator==(const IconTwoToneColor&, const IconTwoToneColor&) = default;
};

class IconProps final {
public:
    IconProps& name(Prop<IconName> value) {
        name_ = std::move(value);
        source_.reset();
        return *this;
    }

    IconProps& source(Prop<IconSource> value) {
        source_ = std::move(value);
        return *this;
    }

    IconProps& twoToneColor(Prop<IconTwoToneColor> value) {
        two_tone_color_ = std::move(value);
        return *this;
    }

    IconProps& rotate(Prop<float> value) {
        rotate_ = std::move(value);
        return *this;
    }

    IconProps& spin(Prop<bool> value) {
        spin_ = std::move(value);
        return *this;
    }

    IconProps& tone(Prop<TextTone> value) {
        tone_ = std::move(value);
        return *this;
    }

    IconProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    IconProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::IconPropsAccess;
    Prop<IconName> name_{IconName::EyeOutlined};
    std::optional<Prop<IconSource>> source_;
    std::optional<Prop<IconTwoToneColor>> two_tone_color_;
    Prop<float> rotate_{0};
    Prop<bool> spin_{false};
    std::optional<Prop<TextTone>> tone_;
    Prop<bool> visible_{true};
    LayoutStyle layout_;
};

void Icon(IconProps props);

} // namespace ryn
