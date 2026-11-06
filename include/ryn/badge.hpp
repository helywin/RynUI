#pragma once
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/status_types.hpp>
#include <ryn/string.hpp>
#include <optional>
#include <variant>

namespace ryn {
namespace detail {
class TagBadgeComponentHost;
}
enum class BadgeSize { Default, Small };
enum class RibbonPlacement { Start, End };

struct BadgeRenderer final {
    std::function<void()> render;
};

using BadgeCountValue = std::variant<std::monostate, int, double, String, std::shared_ptr<const BadgeRenderer>>;

struct BadgeContentSlot final {};

using BadgeContent = SlotContent<BadgeContentSlot>;

class BadgeProps final {
public:
    BadgeProps& count(Prop<int> value) {
        count_ = std::move(value);
        string_count_.reset();
        value_count_.reset();
        return *this;
    }

    BadgeProps& count(Prop<String> value) {
        string_count_ = std::move(value);
        count_.reset();
        value_count_.reset();
        return *this;
    }

    template <std::size_t N> BadgeProps& count(const char8_t (&value)[N]) {
        return count(Prop<String>{String{value}});
    }

    BadgeProps& countValue(Prop<BadgeCountValue> value) {
        value_count_ = std::move(value);
        count_.reset();
        string_count_.reset();
        return *this;
    }

    BadgeProps& contentRender(Prop<std::shared_ptr<const BadgeRenderer>> value) {
        content_ = std::move(value);
        return *this;
    }

    BadgeProps& contentRender(std::shared_ptr<const BadgeRenderer> value) {
        return contentRender(Prop<std::shared_ptr<const BadgeRenderer>>{std::move(value)});
    }

    BadgeProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        return *this;
    }

    BadgeProps& visible(Prop<bool> value = true) {
        visible_ = std::move(value);
        return *this;
    }

    BadgeProps& animated(Prop<bool> value = true) {
        animated_ = std::move(value);
        return *this;
    }

    BadgeProps& color(Prop<std::optional<Color>> value) {
        color_ = std::move(value);
        return *this;
    }

    BadgeProps& color(Color value) {
        return color(Prop<std::optional<Color>>{std::optional{value}});
    }

    BadgeProps& overflowCount(Prop<int> value) {
        overflow_count_ = std::move(value);
        return *this;
    }

    BadgeProps& showZero(Prop<bool> value = true) {
        show_zero_ = std::move(value);
        return *this;
    }

    BadgeProps& dot(Prop<bool> value = true) {
        dot_ = std::move(value);
        return *this;
    }

    BadgeProps& status(Prop<Status> value) {
        status_ = std::move(value);
        return *this;
    }

    BadgeProps& text(Prop<String> value) {
        text_ = std::move(value);
        return *this;
    }

    template <std::size_t N> BadgeProps& text(const char8_t (&value)[N]) {
        return text(String{value});
    }

    BadgeProps& offset(Prop<LogicalOffset> value) {
        offset_ = std::move(value);
        return *this;
    }

    BadgeProps& size(Prop<BadgeSize> value) {
        size_ = std::move(value);
        return *this;
    }

    BadgeProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TagBadgeComponentHost;
    std::optional<Prop<String>> string_count_;
    std::optional<Prop<BadgeCountValue>> value_count_;
    Prop<std::shared_ptr<const BadgeRenderer>> content_{nullptr};
    Prop<bool> rtl_{false};
    Prop<bool> visible_{true};
    Prop<bool> animated_{true};
    Prop<std::optional<Color>> color_{std::nullopt};
    std::optional<Prop<int>> count_;
    Prop<int> overflow_count_{99};
    Prop<bool> show_zero_{false};
    Prop<bool> dot_{false};
    Prop<Status> status_{Status::None};
    Prop<String> text_{String{}};
    Prop<LogicalOffset> offset_{LogicalOffset{}};
    Prop<BadgeSize> size_{BadgeSize::Default};
    LayoutStyle layout_;
};

class RibbonProps final {
public:
    RibbonProps& text(Prop<String> value) {
        text_ = std::move(value);
        return *this;
    }

    template <std::size_t N> RibbonProps& text(const char8_t (&value)[N]) {
        return text(String{value});
    }

    RibbonProps& color(Prop<std::optional<Color>> value) {
        color_ = std::move(value);
        return *this;
    }

    RibbonProps& color(Color value) {
        return color(Prop<std::optional<Color>>{std::optional{value}});
    }

    RibbonProps& placement(Prop<RibbonPlacement> value) {
        placement_ = std::move(value);
        return *this;
    }

    RibbonProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        return *this;
    }

    RibbonProps& visible(Prop<bool> value = true) {
        visible_ = std::move(value);
        return *this;
    }

    RibbonProps& textRender(Prop<std::shared_ptr<const BadgeRenderer>> value) {
        text_render_ = std::move(value);
        return *this;
    }

    RibbonProps& contentRender(Prop<std::shared_ptr<const BadgeRenderer>> value) {
        content_ = std::move(value);
        return *this;
    }

    RibbonProps& textRender(std::shared_ptr<const BadgeRenderer> value) {
        return textRender(Prop<std::shared_ptr<const BadgeRenderer>>{std::move(value)});
    }

    RibbonProps& contentRender(std::shared_ptr<const BadgeRenderer> value) {
        return contentRender(Prop<std::shared_ptr<const BadgeRenderer>>{std::move(value)});
    }

    RibbonProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TagBadgeComponentHost;
    Prop<String> text_{String{}};
    Prop<std::optional<Color>> color_{std::nullopt};
    Prop<RibbonPlacement> placement_{RibbonPlacement::End};
    Prop<bool> rtl_{false};
    Prop<bool> visible_{true};
    Prop<std::shared_ptr<const BadgeRenderer>> text_render_{nullptr};
    Prop<std::shared_ptr<const BadgeRenderer>> content_{nullptr};
    LayoutStyle layout_;
};

void BadgeRibbon(RibbonProps props, BadgeContent content);
void Badge(BadgeProps props, std::optional<BadgeContent> content = std::nullopt);
} // namespace ryn
