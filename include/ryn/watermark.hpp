#pragma once
#include <ryn/component.hpp>
#include <ryn/image.hpp>
#include <ryn/layout_style.hpp>

namespace ryn {
namespace detail {
class DecorationComponentHost;
}

struct WatermarkContentSlot final {};

using WatermarkContent = SlotContent<WatermarkContentSlot>;

struct WatermarkSize final {
    float width{120};
    float height{64};
    friend bool operator==(WatermarkSize, WatermarkSize) = default;
};

struct WatermarkGap final {
    float x{100};
    float y{100};
    friend bool operator==(WatermarkGap, WatermarkGap) = default;
};

struct WatermarkOffset final {
    float x{};
    float y{};
    friend bool operator==(WatermarkOffset, WatermarkOffset) = default;
};

class WatermarkProps final {
public:
    WatermarkProps& content(Prop<String> v) {
        content_ = std::move(v);
        return *this;
    }

    template <std::size_t N> WatermarkProps& content(const char8_t (&v)[N]) {
        return content(String{v});
    }

    WatermarkProps& image(Prop<ImageSource> v) {
        image_ = std::move(v);
        return *this;
    }

    WatermarkProps& size(Prop<WatermarkSize> v) {
        size_ = std::move(v);
        return *this;
    }

    WatermarkProps& gap(Prop<WatermarkGap> v) {
        gap_ = std::move(v);
        return *this;
    }

    WatermarkProps& offset(Prop<WatermarkOffset> v) {
        offset_ = std::move(v);
        return *this;
    }

    WatermarkProps& rotate(Prop<float> v) {
        rotate_ = std::move(v);
        return *this;
    }

    WatermarkProps& visible(Prop<bool> v) {
        visible_ = std::move(v);
        return *this;
    }

    WatermarkProps& disabled(Prop<bool> v = true) {
        disabled_ = std::move(v);
        return *this;
    }

    WatermarkProps& onLoad(std::function<void()> v) {
        load_ = std::move(v);
        return *this;
    }

    WatermarkProps& onError(std::function<void(const ImageError&)> v) {
        error_ = std::move(v);
        return *this;
    }

    WatermarkProps& layout(LayoutStyle v) {
        layout_ = std::move(v);
        return *this;
    }

private:
    friend class detail::DecorationComponentHost;
    Prop<String> content_{String{}};
    Prop<ImageSource> image_{ImageSource{}};
    Prop<WatermarkSize> size_{WatermarkSize{}};
    Prop<WatermarkGap> gap_{WatermarkGap{}};
    std::optional<Prop<WatermarkOffset>> offset_;
    Prop<float> rotate_{-22};
    Prop<bool> visible_{true};
    Prop<bool> disabled_{false};
    std::function<void()> load_;
    std::function<void(const ImageError&)> error_;
    LayoutStyle layout_;
};

void Watermark(WatermarkProps props, WatermarkContent content);
} // namespace ryn
