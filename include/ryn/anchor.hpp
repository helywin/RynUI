#pragma once

#include <ryn/component.hpp>
#include <ryn/float_button.hpp>
#include <ryn/layout_style.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace ryn {
namespace detail {
class AnchorComponentHost;
}

// Same logical units and host acknowledgement as FloatButtonBackTop.
using AnchorScrollState = FloatButtonScrollState;

struct AnchorScrollViewport final {
    float top{}; // Logical window coordinate of a nested scroll viewport.
    float height{};
    friend bool operator==(const AnchorScrollViewport&, const AnchorScrollViewport&) = default;
};
enum class AnchorDirection { Vertical, Horizontal };

struct AnchorTitleSlot final {};

using AnchorTitle = SlotContent<AnchorTitleSlot>;

class AnchorItem final {
public:
    AnchorItem(String key, String title, Prop<float> position)
        : key_(std::move(key)), title_(std::move(title)), position_(std::move(position)) {}

    AnchorItem& title(Prop<String> value) {
        title_ = std::move(value);
        touch();
        return *this;
    }

    template <std::size_t N> AnchorItem& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    AnchorItem& richTitle(AnchorTitle value) {
        rich_title_ = std::make_shared<const AnchorTitle>(std::move(value));
        touch();
        return *this;
    }

    AnchorItem& position(Prop<float> value) {
        position_ = std::move(value);
        touch();
        return *this;
    }

    AnchorItem& targetOffset(Prop<float> value) {
        target_offset_ = std::move(value);
        touch();
        return *this;
    }

    AnchorItem& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        touch();
        return *this;
    }

    AnchorItem& visible(Prop<bool> value) {
        visible_ = std::move(value);
        touch();
        return *this;
    }

    AnchorItem& children(std::vector<AnchorItem> value) {
        children_ = std::move(value);
        touch();
        return *this;
    }

    friend bool operator==(const AnchorItem& a, const AnchorItem& b) {
        return a.key_ == b.key_ && a.revision_ == b.revision_;
    }

private:
    friend class detail::AnchorComponentHost;

    void touch() {
        revision_ = std::make_shared<const int>(0);
    }

    String key_;
    Prop<String> title_;
    Prop<float> position_;
    std::optional<Prop<float>> target_offset_;
    Prop<bool> disabled_{false};
    Prop<bool> visible_{true};
    std::shared_ptr<const AnchorTitle> rich_title_;
    std::vector<AnchorItem> children_;
    std::shared_ptr<const int> revision_{std::make_shared<const int>(0)};
};

struct AnchorClickEvent final {
    String key;
    std::size_t index{}; // Depth-first index in the current topology.
    float offset{};
    bool default_prevented{};

    void preventScroll() noexcept {
        default_prevented = true;
    }
};

class AnchorProps final {
public:
    AnchorProps& items(Prop<std::vector<AnchorItem>> value) {
        items_ = std::move(value);
        return *this;
    }

    AnchorProps& scroll(Prop<AnchorScrollState> value) {
        scroll_ = std::move(value);
        return *this;
    }

    AnchorProps& scrollViewport(Prop<AnchorScrollViewport> value) {
        scroll_viewport_ = std::move(value);
        return *this;
    }

    AnchorProps& onScrollTo(std::function<void(float)> value) {
        on_scroll_ = std::move(value);
        return *this;
    }

    AnchorProps& direction(Prop<AnchorDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    AnchorProps& offsetTop(Prop<float> value) {
        offset_top_ = std::move(value);
        return *this;
    }

    AnchorProps& targetOffset(Prop<float> value) {
        target_offset_ = std::move(value);
        return *this;
    }

    AnchorProps& bounds(Prop<float> value) {
        bounds_ = std::move(value);
        return *this;
    }

    AnchorProps& affix(Prop<bool> value) {
        affix_ = std::move(value);
        return *this;
    }

    AnchorProps& showInkInFixed(Prop<bool> value) {
        ink_ = std::move(value);
        return *this;
    }

    AnchorProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    AnchorProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    AnchorProps& duration(Prop<Duration> value) {
        duration_ = std::move(value);
        return *this;
    }

    AnchorProps& activeKey(Prop<String> value) {
        active_ = std::move(value);
        return *this;
    }

    AnchorProps& onClick(std::function<void(AnchorClickEvent&)> value) {
        on_click_ = std::move(value);
        return *this;
    }

    AnchorProps& onChange(std::function<void(String)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    AnchorProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::AnchorComponentHost;
    Prop<std::vector<AnchorItem>> items_{std::vector<AnchorItem>{}};
    Prop<AnchorScrollState> scroll_{AnchorScrollState{}};
    std::function<void(float)> on_scroll_;
    std::optional<Prop<AnchorScrollViewport>> scroll_viewport_;
    Prop<AnchorDirection> direction_{AnchorDirection::Vertical};
    Prop<float> offset_top_{0};
    std::optional<Prop<float>> target_offset_;
    Prop<float> bounds_{5};
    Prop<bool> affix_{true};
    Prop<bool> ink_{false};
    Prop<bool> disabled_{false};
    Prop<bool> visible_{true};
    Prop<Duration> duration_{Duration::milliseconds(450)};
    std::optional<Prop<String>> active_;
    std::function<void(AnchorClickEvent&)> on_click_;
    std::function<void(String)> on_change_;
    LayoutStyle layout_;
};

void Anchor(AnchorProps props);
} // namespace ryn
