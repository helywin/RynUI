#pragma once
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/status_types.hpp>
#include <ryn/string.hpp>
#include <optional>

namespace ryn {
namespace detail {
struct ConfigDefaults;
}

namespace detail {
class TagBadgeComponentHost;
}

enum class TagVariant { Outlined, Filled, Solid };

struct TagCloseEvent final {
    void preventDefault() noexcept {
        prevented = true;
    }

    bool prevented{};
};

struct TagContentSlot final {};

struct TagIconSlot final {};

using TagContent = SlotContent<TagContentSlot>;
using TagIcon = SlotContent<TagIconSlot>;

struct TagSlots final {
    std::optional<TagIcon> icon;
    std::optional<TagIcon> close;
};

class TagProps final {
public:
    TagProps& variant(Prop<TagVariant> value) {
        variant_ = std::move(value);
        return *this;
    }

    TagProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        explicit_rtl_ = true;
        return *this;
    }

    TagProps& animated(Prop<bool> value = true) {
        animated_ = std::move(value);
        return *this;
    }

    TagProps& destroyOnClose(bool value = true) {
        destroy_on_close_ = value;
        return *this;
    }

    TagProps& onClose(std::function<void(TagCloseEvent&)> value) {
        on_close_event_ = std::move(value);
        return *this;
    }

    TagProps& color(Prop<TagColor> value) {
        color_ = std::move(value);
        return *this;
    }

    TagProps& bordered(Prop<bool> value = true) {
        bordered_ = std::move(value);
        return *this;
    }

    TagProps& color(PresetColor value) {
        return color(Prop<TagColor>{TagColor{value}});
    }

    TagProps& color(Status value) {
        return color(Prop<TagColor>{TagColor{value}});
    }

    TagProps& color(Color value) {
        return color(Prop<TagColor>{TagColor{value}});
    }

    TagProps& closable(Prop<bool> value = true) {
        closable_ = std::move(value);
        return *this;
    }

    TagProps& visible(Prop<bool> value = true) {
        visible_ = std::move(value);
        controlled_visible_ = true;
        return *this;
    }

    TagProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        explicit_disabled_ = true;
        return *this;
    }

    TagProps& onClose(std::function<void()> value) {
        on_close_ = std::move(value);
        return *this;
    }

    TagProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::ConfigDefaults;
    friend class detail::TagBadgeComponentHost;
    Prop<TagVariant> variant_{TagVariant::Outlined};
    Prop<bool> rtl_{false};
    bool explicit_rtl_{};
    Prop<bool> animated_{true};
    bool destroy_on_close_{};
    bool controlled_visible_{};
    std::function<void(TagCloseEvent&)> on_close_event_;
    Prop<TagColor> color_{TagColor{PresetColor::Default}};
    Prop<bool> bordered_{true};
    Prop<bool> closable_{false};
    Prop<bool> visible_{true};
    Prop<bool> disabled_{false};
    bool explicit_disabled_{};
    std::function<void()> on_close_;
    LayoutStyle layout_;
};

class CheckableTagProps final {
public:
    CheckableTagProps& checked(Prop<bool> value) {
        checked_ = std::move(value);
        controlled_checked_ = true;
        return *this;
    }

    CheckableTagProps& defaultChecked(bool value = true) {
        default_checked_ = value;
        controlled_checked_ = false;
        return *this;
    }

    CheckableTagProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        explicit_rtl_ = true;
        return *this;
    }

    CheckableTagProps& animated(Prop<bool> value = true) {
        animated_ = std::move(value);
        return *this;
    }

    CheckableTagProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        explicit_disabled_ = true;
        return *this;
    }

    CheckableTagProps& onChange(std::function<void(bool)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    CheckableTagProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::ConfigDefaults;
    friend class detail::TagBadgeComponentHost;
    bool controlled_checked_{true};
    bool default_checked_{};
    Prop<bool> rtl_{false};
    bool explicit_rtl_{};
    Prop<bool> animated_{true};
    Prop<bool> checked_{false};
    Prop<bool> disabled_{false};
    bool explicit_disabled_{};
    std::function<void(bool)> on_change_;
    LayoutStyle layout_;
};

struct TagItemRenderer final {
    std::function<void()> render;
};

struct TagGroupItem final {
    String key;
    String text;
    TagColor color{PresetColor::Default};
    bool disabled{};
    bool closable{};
    std::shared_ptr<const TagItemRenderer> content;
    std::shared_ptr<const TagItemRenderer> icon;
    friend bool operator==(const TagGroupItem&, const TagGroupItem&) = default;
};

using TagValues = std::vector<String>;

class TagGroupProps final {
public:
    TagGroupProps& items(Prop<std::vector<TagGroupItem>> value) {
        items_ = std::move(value);
        return *this;
    }

    TagGroupProps& value(Prop<TagValues> value) {
        value_ = std::move(value);
        return *this;
    }

    TagGroupProps& defaultValue(TagValues value) {
        default_value_ = std::move(value);
        return *this;
    }

    TagGroupProps& multiple(bool value = true) {
        multiple_ = value;
        return *this;
    }

    TagGroupProps& checkable(bool value = true) {
        checkable_ = value;
        return *this;
    }

    TagGroupProps& draggable(Prop<bool> value = true) {
        draggable_ = std::move(value);
        return *this;
    }

    TagGroupProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        explicit_disabled_ = true;
        return *this;
    }

    TagGroupProps& visible(Prop<bool> value = true) {
        visible_ = std::move(value);
        return *this;
    }

    TagGroupProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        explicit_rtl_ = true;
        return *this;
    }

    TagGroupProps& onReorder(std::function<void(TagValues)> value) {
        on_reorder_ = std::move(value);
        return *this;
    }

    TagGroupProps& onChange(std::function<void(TagValues)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    TagGroupProps& onClose(std::function<void(String, TagCloseEvent&)> value) {
        on_close_ = std::move(value);
        return *this;
    }

    TagGroupProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::ConfigDefaults;
    friend class detail::TagBadgeComponentHost;
    Prop<std::vector<TagGroupItem>> items_{std::vector<TagGroupItem>{}};
    std::optional<Prop<TagValues>> value_;
    TagValues default_value_;
    bool multiple_{};
    bool checkable_{};
    Prop<bool> draggable_{false};
    Prop<bool> disabled_{false};
    bool explicit_disabled_{};
    Prop<bool> visible_{true};
    Prop<bool> rtl_{false};
    bool explicit_rtl_{};
    std::function<void(TagValues)> on_reorder_;
    std::function<void(TagValues)> on_change_;
    std::function<void(String, TagCloseEvent&)> on_close_;
    LayoutStyle layout_;
};

void TagGroup(TagGroupProps props);
void CheckableTagGroup(TagGroupProps props);
void Tag(TagProps props, TagContent content, TagSlots slots = {});
void CheckableTag(CheckableTagProps props, TagContent content, TagSlots slots = {});
} // namespace ryn
