#pragma once
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/status_types.hpp>
#include <optional>

namespace ryn {
namespace detail {
class TagBadgeComponentHost;
}

struct TagContentSlot final {};

struct TagIconSlot final {};

using TagContent = SlotContent<TagContentSlot>;
using TagIcon = SlotContent<TagIconSlot>;

struct TagSlots final {
    std::optional<TagIcon> icon;
};

class TagProps final {
public:
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
        return *this;
    }

    TagProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
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
    friend class detail::TagBadgeComponentHost;
    Prop<TagColor> color_{TagColor{PresetColor::Default}};
    Prop<bool> bordered_{true};
    Prop<bool> closable_{false};
    Prop<bool> visible_{true};
    Prop<bool> disabled_{false};
    std::function<void()> on_close_;
    LayoutStyle layout_;
};

class CheckableTagProps final {
public:
    CheckableTagProps& checked(Prop<bool> value) {
        checked_ = std::move(value);
        return *this;
    }

    CheckableTagProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
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
    friend class detail::TagBadgeComponentHost;
    Prop<bool> checked_{false};
    Prop<bool> disabled_{false};
    std::function<void(bool)> on_change_;
    LayoutStyle layout_;
};

void Tag(TagProps props, TagContent content, TagSlots slots = {});
void CheckableTag(CheckableTagProps props, TagContent content, TagSlots slots = {});
} // namespace ryn
