#pragma once
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <optional>
#include <vector>

namespace ryn {
namespace detail {
class TimelineComponentHost;
}

enum class TimelineMode { Start, Alternate, End };
enum class TimelinePlacement { Start, End };
enum class TimelineTone { Default, Blue, Red, Green, Gray };

struct TimelineTitleSlot final {};

struct TimelineContentSlot final {};

struct TimelineIconSlot final {};

using TimelineTitle = SlotContent<TimelineTitleSlot>;
using TimelineContent = SlotContent<TimelineContentSlot>;
using TimelineIcon = SlotContent<TimelineIconSlot>;

class TimelineItem final {
public:
    TimelineItem(String key, Prop<String> content) : key_(std::move(key)), content_(std::move(content)) {}

    template <std::size_t N>
    TimelineItem(String key, const char8_t (&content)[N])
        : TimelineItem(std::move(key), Prop<String>{String{content}}) {}

    TimelineItem& title(Prop<String> value) {
        title_ = std::move(value);
        return *this;
    }

    template <std::size_t N> TimelineItem& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    TimelineItem& content(Prop<String> value) {
        content_ = std::move(value);
        return *this;
    }

    template <std::size_t N> TimelineItem& content(const char8_t (&value)[N]) {
        return content(String{value});
    }

    TimelineItem& richTitle(TimelineTitle value) {
        rich_title_ = std::move(value);
        return *this;
    }

    TimelineItem& richContent(TimelineContent value) {
        rich_content_ = std::move(value);
        return *this;
    }

    TimelineItem& icon(TimelineIcon value) {
        icon_ = std::move(value);
        return *this;
    }

    TimelineItem& placement(Prop<std::optional<TimelinePlacement>> value) {
        placement_ = std::move(value);
        return *this;
    }

    TimelineItem& placement(TimelinePlacement value) {
        return placement(std::optional{value});
    }

    TimelineItem& tone(Prop<TimelineTone> value) {
        tone_ = std::move(value);
        return *this;
    }

    TimelineItem& loading(Prop<bool> value = true) {
        loading_ = std::move(value);
        return *this;
    }

    TimelineItem& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TimelineComponentHost;
    String key_;
    Prop<String> title_{String{}};
    Prop<String> content_;
    std::optional<TimelineTitle> rich_title_;
    std::optional<TimelineContent> rich_content_;
    std::optional<TimelineIcon> icon_;
    Prop<std::optional<TimelinePlacement>> placement_{std::nullopt};
    Prop<TimelineTone> tone_{TimelineTone::Default};
    Prop<bool> loading_{false};
    Prop<bool> visible_{true};
};

class TimelineProps final {
public:
    TimelineProps& mode(Prop<TimelineMode> value) {
        mode_ = std::move(value);
        return *this;
    }

    TimelineProps& reverse(Prop<bool> value = true) {
        reverse_ = std::move(value);
        return *this;
    }

    TimelineProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    TimelineProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TimelineComponentHost;
    Prop<TimelineMode> mode_{TimelineMode::Start};
    Prop<bool> reverse_{false};
    Prop<bool> visible_{true};
    LayoutStyle layout_;
};

void Timeline(TimelineProps props, std::vector<TimelineItem> items);
} // namespace ryn
