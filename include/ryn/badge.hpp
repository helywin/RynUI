#pragma once
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/status_types.hpp>
#include <ryn/string.hpp>
#include <optional>

namespace ryn {
namespace detail {
class TagBadgeComponentHost;
}
enum class BadgeSize { Default, Small };

struct BadgeContentSlot final {};

using BadgeContent = SlotContent<BadgeContentSlot>;

class BadgeProps final {
public:
    BadgeProps& count(Prop<int> value) {
        count_ = std::move(value);
        return *this;
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

void Badge(BadgeProps props, std::optional<BadgeContent> content = std::nullopt);
} // namespace ryn
