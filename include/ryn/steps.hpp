#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <functional>
#include <optional>
#include <vector>
#include <memory>

namespace ryn {
namespace detail {
class StepsComponentHost;
}

enum class StepsStatus { Wait, Process, Finish, Error };
enum class StepsSize { Default, Small };
enum class StepsType { Default, Dot, Inline, Navigation, Panel };

struct StepsTitleSlot final {};

struct StepsSubtitleSlot final {};

struct StepsDescriptionSlot final {};

struct StepsIconSlot final {};

using StepsTitle = SlotContent<StepsTitleSlot>;
using StepsSubtitle = SlotContent<StepsSubtitleSlot>;
using StepsDescription = SlotContent<StepsDescriptionSlot>;
using StepsIcon = SlotContent<StepsIconSlot>;

class StepsItem final {
public:
    StepsItem(String key, String title) : key_(std::move(key)), title_(std::move(title)) {}

    [[nodiscard]] const String& key() const noexcept {
        return key_;
    }

    StepsItem& title(Prop<String> value) {
        title_ = std::move(value);
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    template <std::size_t N> StepsItem& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    StepsItem& subtitle(Prop<String> value) {
        subtitle_ = std::move(value);
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    template <std::size_t N> StepsItem& subtitle(const char8_t (&value)[N]) {
        return subtitle(String{value});
    }

    StepsItem& description(Prop<String> value) {
        description_ = std::move(value);
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    template <std::size_t N> StepsItem& description(const char8_t (&value)[N]) {
        return description(String{value});
    }

    StepsItem& richTitle(StepsTitle value) {
        rich_title_ = std::move(value);
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    StepsItem& richSubtitle(StepsSubtitle value) {
        rich_subtitle_ = std::move(value);
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    StepsItem& richDescription(StepsDescription value) {
        rich_description_ = std::move(value);
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    StepsItem& icon(StepsIcon value) {
        icon_ = std::move(value);
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    StepsItem& status(Prop<std::optional<StepsStatus>> value) {
        status_ = std::move(value);
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    StepsItem& status(StepsStatus value) {
        return status(std::optional{value});
    }

    StepsItem& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    StepsItem& visible(Prop<bool> value) {
        visible_ = std::move(value);
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    friend bool operator==(const StepsItem& a, const StepsItem& b) {
        return a.revision_ == b.revision_;
    }

private:
    friend class detail::StepsComponentHost;
    std::shared_ptr<const int> revision_{std::make_shared<const int>(0)};
    String key_;
    Prop<String> title_;
    Prop<String> subtitle_{String{}};
    Prop<String> description_{String{}};
    std::optional<StepsTitle> rich_title_;
    std::optional<StepsSubtitle> rich_subtitle_;
    std::optional<StepsDescription> rich_description_;
    std::optional<StepsIcon> icon_;
    Prop<std::optional<StepsStatus>> status_{std::nullopt};
    Prop<bool> disabled_{false};
    Prop<bool> visible_{true};
};

class StepsProps final {
public:
    StepsProps& current(Prop<std::size_t> value) {
        current_ = std::move(value);
        return *this;
    }

    StepsProps& defaultCurrent(std::size_t value) {
        default_current_ = value;
        return *this;
    }

    StepsProps& status(Prop<StepsStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    StepsProps& size(Prop<StepsSize> value) {
        size_ = std::move(value);
        return *this;
    }

    StepsProps& vertical(Prop<bool> value = true) {
        vertical_ = std::move(value);
        return *this;
    }

    StepsProps& type(Prop<StepsType> value) {
        type_ = std::move(value);
        return *this;
    }

    StepsProps& responsive(Prop<bool> value = true) {
        responsive_ = std::move(value);
        return *this;
    }

    StepsProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        return *this;
    }

    StepsProps& percent(Prop<std::optional<float>> value) {
        percent_ = std::move(value);
        return *this;
    }

    StepsProps& percent(float value) {
        return percent(std::optional{value});
    }

    StepsProps& maxCount(Prop<std::size_t> value) {
        max_count_ = std::move(value);
        return *this;
    }

    StepsProps& animated(Prop<bool> value = true) {
        animated_ = std::move(value);
        return *this;
    }

    StepsProps& onChange(std::function<void(std::size_t)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    StepsProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::StepsComponentHost;
    std::optional<Prop<std::size_t>> current_;
    std::optional<std::size_t> default_current_;
    Prop<StepsStatus> status_{StepsStatus::Process};
    Prop<StepsSize> size_{StepsSize::Default};
    Prop<bool> vertical_{false};
    Prop<StepsType> type_{StepsType::Default};
    Prop<bool> responsive_{true};
    Prop<bool> rtl_{false};
    Prop<std::optional<float>> percent_{std::nullopt};
    Prop<std::size_t> max_count_{0};
    Prop<bool> animated_{true};
    std::function<void(std::size_t)> on_change_;
    LayoutStyle layout_;
};

void Steps(StepsProps props, std::vector<StepsItem> items);
void Steps(StepsProps props, Prop<std::vector<StepsItem>> items);
} // namespace ryn
