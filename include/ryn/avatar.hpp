#pragma once
#include <ryn/component.hpp>
#include <ryn/icon.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/popover.hpp>
#include <variant>

namespace ryn {
namespace detail {
class AvatarComponentHost;
}

enum class AvatarShape { Circle, Square };
enum class AvatarSize { Small, Medium, Large };
using AvatarSizeValue = std::variant<AvatarSize, float>;

struct AvatarIconSlot final {};

struct AvatarContentSlot final {};

using AvatarIcon = SlotContent<AvatarIconSlot>;
using AvatarContent = SlotContent<AvatarContentSlot>;

struct AvatarSlots final {
    std::optional<AvatarIcon> icon;
    std::optional<AvatarContent> content;
};

class AvatarProps final {
public:
    AvatarProps& text(Prop<String> value) {
        text_ = std::move(value);
        return *this;
    }

    template <std::size_t N> AvatarProps& text(const char8_t (&value)[N]) {
        return text(String{value});
    }

    AvatarProps& size(Prop<AvatarSizeValue> value) {
        size_ = std::move(value);
        return *this;
    }

    AvatarProps& size(AvatarSize value) {
        return size(Prop<AvatarSizeValue>{AvatarSizeValue{value}});
    }

    AvatarProps& size(float value) {
        return size(Prop<AvatarSizeValue>{AvatarSizeValue{value}});
    }

    AvatarProps& shape(Prop<AvatarShape> value) {
        shape_ = std::move(value);
        return *this;
    }

    AvatarProps& gap(Prop<float> value) {
        gap_ = std::move(value);
        return *this;
    }

    AvatarProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    AvatarProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::AvatarComponentHost;
    Prop<String> text_{String{}};
    Prop<AvatarSizeValue> size_{AvatarSizeValue{AvatarSize::Medium}};
    Prop<AvatarShape> shape_{AvatarShape::Circle};
    Prop<float> gap_{4};
    Prop<bool> visible_{true};
    LayoutStyle layout_;
};

class AvatarGroupItem final {
public:
    AvatarGroupItem(String key, Prop<String> text) : key_(std::move(key)), text_(std::move(text)) {}

    template <std::size_t N>
    AvatarGroupItem(String key, const char8_t (&text)[N])
        : AvatarGroupItem(std::move(key), Prop<String>{String{text}}) {}

    AvatarGroupItem& icon(Prop<std::optional<IconName>> value) {
        icon_ = std::move(value);
        return *this;
    }

    AvatarGroupItem& icon(IconName value) {
        return icon(Prop<std::optional<IconName>>{std::optional{value}});
    }

    AvatarGroupItem& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

private:
    friend class detail::AvatarComponentHost;
    String key_;
    Prop<String> text_;
    Prop<std::optional<IconName>> icon_{std::nullopt};
    Prop<bool> visible_{true};
};

struct AvatarGroupMax final {
    std::optional<std::uint32_t> count;
    PopoverTriggerMode trigger{PopoverTriggerMode::Hover};
    PopoverPlacement placement{PopoverPlacement::Top};
    friend bool operator==(const AvatarGroupMax&, const AvatarGroupMax&) = default;
};

class AvatarGroupProps final {
public:
    AvatarGroupProps& size(Prop<AvatarSizeValue> value) {
        size_ = std::move(value);
        return *this;
    }

    AvatarGroupProps& size(AvatarSize value) {
        return size(Prop<AvatarSizeValue>{AvatarSizeValue{value}});
    }

    AvatarGroupProps& size(float value) {
        return size(Prop<AvatarSizeValue>{AvatarSizeValue{value}});
    }

    AvatarGroupProps& shape(Prop<AvatarShape> value) {
        shape_ = std::move(value);
        return *this;
    }

    AvatarGroupProps& max(Prop<AvatarGroupMax> value) {
        max_ = std::move(value);
        return *this;
    }

    AvatarGroupProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    AvatarGroupProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::AvatarComponentHost;
    Prop<AvatarSizeValue> size_{AvatarSizeValue{AvatarSize::Medium}};
    Prop<AvatarShape> shape_{AvatarShape::Circle};
    Prop<AvatarGroupMax> max_{AvatarGroupMax{}};
    Prop<bool> visible_{true};
    LayoutStyle layout_;
};

void Avatar(AvatarProps props = {}, AvatarSlots slots = {});
void AvatarGroup(AvatarGroupProps props, std::vector<AvatarGroupItem> items);
} // namespace ryn
