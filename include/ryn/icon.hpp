#pragma once

#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/text.hpp>

#include <cstdint>
#include <cstddef>
#include <optional>
#include <utility>

namespace ryn {
namespace detail {
struct IconPropsAccess;
}

enum class IconName : std::uint16_t {
#include <ryn/generated/icon_names.inc>
};

// Values 0-13 preserve the original API; all names and layers are generated
// from third_party/ant-design-icons/manifest.json.
inline constexpr IconName last_bundled_icon = static_cast<IconName>(847);
inline constexpr std::size_t bundled_icon_count = 848;

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
