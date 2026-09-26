#pragma once

#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/text.hpp>

#include <cstdint>
#include <optional>
#include <utility>

namespace ryn {
namespace detail { struct IconPropsAccess; }

enum class IconName : std::uint8_t {
    EyeOutlined,
    EyeInvisibleOutlined,
    SearchOutlined,
    CloseCircleFilled,
    MenuOutlined,
    SunOutlined,
    MoonOutlined,
    UserOutlined,
    LockOutlined,
};

class IconProps final {
public:
    IconProps& name(Prop<IconName> value) {
        name_ = std::move(value);
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
    std::optional<Prop<TextTone>> tone_;
    Prop<bool> visible_{true};
    LayoutStyle layout_;
};

void Icon(IconProps props);

} // namespace ryn
