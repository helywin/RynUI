#pragma once
#include <ryn/component.hpp>
#include <ryn/icon.hpp>
#include <ryn/image.hpp>
#include <ryn/breakpoints.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/popover.hpp>
#include <variant>

namespace ryn {
namespace detail {
class AvatarComponentHost;
}

enum class AvatarShape { Circle, Square };
enum class AvatarSize { Small, Medium, Large };
using AvatarSizeValue = std::variant<AvatarSize, float, Responsive<float>>;

struct AvatarIconSlot final {};

struct AvatarContentSlot final {};

using AvatarIcon = SlotContent<AvatarIconSlot>;
using AvatarContent = SlotContent<AvatarContentSlot>;

struct AvatarSlots final {
    std::optional<AvatarIcon> icon;
    std::optional<AvatarContent> content;
    std::optional<AvatarContent> placeholder;
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

    AvatarProps& size(Responsive<float> value) {
        return size(Prop<AvatarSizeValue>{AvatarSizeValue{std::move(value)}});
    }

    AvatarProps& source(Prop<ImageSource> value) {
        source_ = std::move(value);
        return *this;
    }

    AvatarProps& fallback(Prop<ImageSource> value) {
        fallback_ = std::move(value);
        return *this;
    }

    AvatarProps& onError(std::function<bool(const ImageError&)> value) {
        on_error_ = std::move(value);
        return *this;
    }

    AvatarProps& onLoad(std::function<void()> value) {
        on_load_ = std::move(value);
        return *this;
    }

    AvatarProps& breakpoints(Prop<Breakpoints> value) {
        breakpoints_ = std::move(value);
        return *this;
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
    Prop<ImageSource> source_{ImageSource{}};
    Prop<ImageSource> fallback_{ImageSource{}};
    Prop<Breakpoints> breakpoints_{Breakpoints{}};
    std::function<bool(const ImageError&)> on_error_;
    std::function<void()> on_load_;
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
        identity_ = std::make_shared<int>(0);
        icon_ = std::move(value);
        return *this;
    }

    AvatarGroupItem& icon(IconName value) {
        return icon(Prop<std::optional<IconName>>{std::optional{value}});
    }

    AvatarGroupItem& source(Prop<ImageSource> value) {
        identity_ = std::make_shared<int>(0);
        source_ = std::move(value);
        return *this;
    }

    AvatarGroupItem& size(Prop<AvatarSizeValue> value) {
        identity_ = std::make_shared<int>(0);
        size_ = std::move(value);
        return *this;
    }

    AvatarGroupItem& shape(Prop<AvatarShape> value) {
        identity_ = std::make_shared<int>(0);
        shape_ = std::move(value);
        return *this;
    }

    AvatarGroupItem& visible(Prop<bool> value) {
        identity_ = std::make_shared<int>(0);
        visible_ = std::move(value);
        return *this;
    }

    friend bool operator==(const AvatarGroupItem& a, const AvatarGroupItem& b) {
        return a.key_ == b.key_ && a.identity_ == b.identity_;
    }

private:
    std::shared_ptr<const int> identity_{std::make_shared<int>(0)};
    friend class detail::AvatarComponentHost;
    Prop<ImageSource> source_{ImageSource{}};
    std::optional<Prop<AvatarSizeValue>> size_;
    std::optional<Prop<AvatarShape>> shape_;
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

struct AvatarGroupPopoverRenderer final {
    std::function<void()> render;
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

    AvatarGroupProps& size(Responsive<float> value) {
        return size(Prop<AvatarSizeValue>{AvatarSizeValue{std::move(value)}});
    }

    AvatarGroupProps& items(Prop<std::vector<AvatarGroupItem>> value) {
        items_ = std::move(value);
        return *this;
    }

    AvatarGroupProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        return *this;
    }

    AvatarGroupProps& popoverRender(Prop<std::shared_ptr<const AvatarGroupPopoverRenderer>> value) {
        popover_ = std::move(value);
        return *this;
    }

    AvatarGroupProps& popoverRender(std::shared_ptr<const AvatarGroupPopoverRenderer> value) {
        return popoverRender(Prop<std::shared_ptr<const AvatarGroupPopoverRenderer>>{std::move(value)});
    }

    AvatarGroupProps& onOpenChange(std::function<void(bool)> value) {
        on_open_ = std::move(value);
        return *this;
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
    std::optional<Prop<std::vector<AvatarGroupItem>>> items_;
    Prop<bool> rtl_{false};
    Prop<std::shared_ptr<const AvatarGroupPopoverRenderer>> popover_{nullptr};
    std::function<void(bool)> on_open_;
    Prop<AvatarSizeValue> size_{AvatarSizeValue{AvatarSize::Medium}};
    Prop<AvatarShape> shape_{AvatarShape::Circle};
    Prop<AvatarGroupMax> max_{AvatarGroupMax{}};
    Prop<bool> visible_{true};
    LayoutStyle layout_;
};

void Avatar(AvatarProps props = {}, AvatarSlots slots = {});
void AvatarGroup(AvatarGroupProps props, std::vector<AvatarGroupItem> items);
void AvatarGroup(AvatarGroupProps props);
} // namespace ryn
