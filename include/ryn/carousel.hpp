#pragma once
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/text_direction.hpp>
#include <ryn/design_token.hpp>
#include <ryn/string.hpp>
#include <functional>
#include <memory>
#include <vector>
#include <optional>

namespace ryn {
namespace detail {
struct ConfigDefaults;
}

enum class CarouselEffect { ScrollX, Fade };
enum class CarouselDotPlacement { Top, Bottom, Start, End };
enum class CarouselChangeSource { Arrow, Dot, Keyboard, Drag, Ref, Autoplay, Property, Items };

namespace detail {
class CarouselComponentHost;
struct CarouselRefState;
} // namespace detail

class CarouselRef final {
public:
    CarouselRef();
    [[nodiscard]] bool goTo(std::size_t slide, bool dont_animate = false) const;
    [[nodiscard]] bool next() const;
    [[nodiscard]] bool prev() const;
    [[nodiscard]] bool play() const;
    [[nodiscard]] bool pause() const;
    [[nodiscard]] bool focus() const;

private:
    friend class detail::CarouselComponentHost;
    std::shared_ptr<detail::CarouselRefState> state_;
};

struct CarouselContentSlot final {};

struct CarouselDotSlot final {};

struct CarouselArrowSlot final {};

using CarouselContent = SlotContent<CarouselContentSlot>;
using CarouselDot = SlotContent<CarouselDotSlot>;
using CarouselArrow = SlotContent<CarouselArrowSlot>;

class CarouselSlide final {
public:
    CarouselSlide(String key, CarouselContent content)
        : key_(std::move(key)), content_(std::make_shared<const CarouselContent>(std::move(content))) {}

    const String& key() const noexcept {
        return key_;
    }

    friend bool operator==(const CarouselSlide&, const CarouselSlide&) = default;

private:
    friend class detail::CarouselComponentHost;
    String key_;
    std::shared_ptr<const CarouselContent> content_;
};

struct CarouselDotInfo final {
    std::size_t index{};
    String key;
    bool active{};
    float progress{};
    friend bool operator==(const CarouselDotInfo&, const CarouselDotInfo&) = default;
};

struct CarouselArrowInfo final {
    bool next{};
    bool disabled{};
    friend bool operator==(const CarouselArrowInfo&, const CarouselArrowInfo&) = default;
};

using CarouselDotRenderer = std::function<CarouselDot(Binding<CarouselDotInfo>, std::function<void()>)>;
using CarouselArrowRenderer = std::function<CarouselArrow(Binding<CarouselArrowInfo>, std::function<void()>)>;

class CarouselProps final {
public:
    CarouselProps& slides(std::vector<CarouselSlide> v) {
        return slides(Prop<std::vector<CarouselSlide>>{std::move(v)});
    }

    CarouselProps& slides(Prop<std::vector<CarouselSlide>> v) {
        slides_ = std::move(v);
        return *this;
    }

    CarouselProps& current(Prop<std::size_t> v) {
        current_ = std::move(v);
        return *this;
    }

    CarouselProps& defaultCurrent(std::size_t v) {
        default_current_ = v;
        return *this;
    }

    CarouselProps& initialSlide(std::size_t v) {
        return defaultCurrent(v);
    }

    CarouselProps& ref(CarouselRef v) {
        ref_ = std::move(v);
        return *this;
    }

    CarouselProps& effect(Prop<CarouselEffect> v) {
        effect_ = std::move(v);
        return *this;
    }

    CarouselProps& easing(Prop<CubicBezier> v) {
        easing_ = std::move(v);
        return *this;
    }

    CarouselProps& infinite(Prop<bool> v = true) {
        infinite_ = std::move(v);
        return *this;
    }

    CarouselProps& adaptiveHeight(Prop<bool> v = true) {
        adaptive_height_ = std::move(v);
        return *this;
    }

    CarouselProps& arrows(Prop<bool> v = true) {
        arrows_ = std::move(v);
        return *this;
    }

    CarouselProps& dots(Prop<bool> v = true) {
        dots_ = std::move(v);
        return *this;
    }

    CarouselProps& dotPlacement(Prop<CarouselDotPlacement> v) {
        placement_ = std::move(v);
        return *this;
    }

    CarouselProps& dotDuration(Prop<bool> v = true) {
        progress_ = std::move(v);
        return *this;
    }

    CarouselProps& vertical(Prop<bool> v = true) {
        vertical_ = std::move(v);
        return *this;
    }

    CarouselProps& direction(Prop<TextDirection> v) {
        direction_ = std::move(v);
        explicit_direction_ = true;
        return *this;
    }

    CarouselProps& draggable(Prop<bool> v = true) {
        draggable_ = std::move(v);
        return *this;
    }

    CarouselProps& swipe(Prop<bool> v = true) {
        swipe_ = std::move(v);
        return *this;
    }

    CarouselProps& touchThreshold(Prop<float> v) {
        threshold_ = std::move(v);
        return *this;
    }

    CarouselProps& autoplay(Prop<bool> v = true) {
        autoplay_ = std::move(v);
        return *this;
    }

    CarouselProps& autoplaySpeed(Prop<Duration> v) {
        autoplay_speed_ = std::move(v);
        return *this;
    }

    CarouselProps& speed(Prop<Duration> v) {
        speed_ = std::move(v);
        return *this;
    }

    CarouselProps& pauseOnHover(Prop<bool> v = true) {
        pause_hover_ = std::move(v);
        return *this;
    }

    CarouselProps& pauseOnFocus(Prop<bool> v = true) {
        pause_focus_ = std::move(v);
        return *this;
    }

    CarouselProps& waitForAnimate(Prop<bool> v = true) {
        wait_ = std::move(v);
        return *this;
    }

    CarouselProps& animated(Prop<bool> v = true) {
        animated_ = std::move(v);
        return *this;
    }

    CarouselProps& visible(Prop<bool> v = true) {
        visible_ = std::move(v);
        return *this;
    }

    CarouselProps& disabled(Prop<bool> v = true) {
        disabled_ = std::move(v);
        return *this;
    }

    CarouselProps& beforeChange(std::function<void(std::size_t, std::size_t)> v) {
        before_ = std::move(v);
        return *this;
    }

    CarouselProps& afterChange(std::function<void(std::size_t)> v) {
        after_ = std::move(v);
        return *this;
    }

    CarouselProps& onCurrentChange(std::function<void(std::size_t, CarouselChangeSource)> v) {
        request_ = std::move(v);
        return *this;
    }

    CarouselProps& customPaging(CarouselDotRenderer v) {
        dot_ = std::move(v);
        return *this;
    }

    CarouselProps& prevArrow(CarouselArrowRenderer v) {
        prev_ = std::move(v);
        return *this;
    }

    CarouselProps& nextArrow(CarouselArrowRenderer v) {
        next_ = std::move(v);
        return *this;
    }

    CarouselProps& layout(LayoutStyle v) {
        layout_ = std::move(v);
        return *this;
    }

private:
    friend struct detail::ConfigDefaults;
    friend class detail::CarouselComponentHost;
    Prop<std::vector<CarouselSlide>> slides_{std::vector<CarouselSlide>{}};
    std::optional<Prop<std::size_t>> current_;
    std::size_t default_current_{};
    CarouselRef ref_;
    Prop<CarouselEffect> effect_{CarouselEffect::ScrollX};
    Prop<CubicBezier> easing_{CubicBezier{0.25F, 0.1F, 0.25F, 1}};
    Prop<bool> infinite_{true};
    Prop<bool> adaptive_height_{false};
    Prop<bool> arrows_{false};
    Prop<bool> dots_{true};
    Prop<CarouselDotPlacement> placement_{CarouselDotPlacement::Bottom};
    Prop<bool> progress_{false};
    std::optional<Prop<bool>> vertical_;
    Prop<TextDirection> direction_{TextDirection::LeftToRight};
    bool explicit_direction_{};
    Prop<bool> draggable_{false};
    Prop<bool> swipe_{true};
    Prop<float> threshold_{5};
    Prop<bool> autoplay_{false};
    Prop<Duration> autoplay_speed_{Duration::milliseconds(3000)};
    Prop<Duration> speed_{Duration::milliseconds(500)};
    Prop<bool> pause_hover_{true};
    Prop<bool> pause_focus_{true};
    Prop<bool> wait_{false};
    Prop<bool> animated_{true};
    Prop<bool> visible_{true};
    Prop<bool> disabled_{false};
    std::function<void(std::size_t, std::size_t)> before_;
    std::function<void(std::size_t)> after_;
    std::function<void(std::size_t, CarouselChangeSource)> request_;
    CarouselDotRenderer dot_;
    CarouselArrowRenderer prev_;
    CarouselArrowRenderer next_;
    LayoutStyle layout_;
};

void Carousel(CarouselProps props);
} // namespace ryn
