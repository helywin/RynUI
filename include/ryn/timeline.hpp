#pragma once
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <optional>
#include <cstdint>
#include <ryn/design_token.hpp>
#include <vector>

namespace ryn {
namespace detail {
struct ConfigDefaults;
}

namespace detail {
class TimelineComponentHost;
}

enum class TimelineOrientation { Vertical, Horizontal };
enum class TimelineVariant { Outlined, Filled };

struct TimelineTitleSpan final {
    float value{0.5F};
    bool logical{};

    static TimelineTitleSpan fraction(float value) {
        return {value, false};
    }

    static TimelineTitleSpan logicalWidth(float value) {
        return {value, true};
    }

    static TimelineTitleSpan columns(float value) {
        return {value / 24.0F, false};
    }

    friend bool operator==(const TimelineTitleSpan&, const TimelineTitleSpan&) = default;
};

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

    TimelineItem& color(Prop<Color> value) {
        color_ = std::move(value);
        return *this;
    }

    TimelineItem& titleRevision(std::uint64_t value) {
        title_revision_ = value;
        return *this;
    }

    TimelineItem& contentRevision(std::uint64_t value) {
        content_revision_ = value;
        return *this;
    }

    TimelineItem& iconRevision(std::uint64_t value) {
        icon_revision_ = value;
        return *this;
    }

private:
    friend class detail::TimelineComponentHost;
    String key_;
    std::optional<Prop<Color>> color_;
    std::uint64_t title_revision_{};
    std::uint64_t content_revision_{};
    std::uint64_t icon_revision_{};
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

    TimelineProps& orientation(Prop<TimelineOrientation> value) {
        orientation_ = std::move(value);
        return *this;
    }

    TimelineProps& variant(Prop<TimelineVariant> value) {
        variant_ = std::move(value);
        return *this;
    }

    TimelineProps& titleSpan(Prop<TimelineTitleSpan> value) {
        title_span_ = std::move(value);
        return *this;
    }

    TimelineProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        explicit_rtl_ = true;
        return *this;
    }

private:
    friend struct detail::ConfigDefaults;
    friend class detail::TimelineComponentHost;
    Prop<TimelineOrientation> orientation_{TimelineOrientation::Vertical};
    Prop<TimelineVariant> variant_{TimelineVariant::Outlined};
    Prop<TimelineTitleSpan> title_span_{TimelineTitleSpan{}};
    Prop<bool> rtl_{false};
    bool explicit_rtl_{};
    Prop<TimelineMode> mode_{TimelineMode::Start};
    Prop<bool> reverse_{false};
    Prop<bool> visible_{true};
    LayoutStyle layout_;
};

void Timeline(TimelineProps props, std::vector<TimelineItem> items);
void Timeline(TimelineProps props, Prop<std::vector<TimelineItem>> items);
} // namespace ryn
