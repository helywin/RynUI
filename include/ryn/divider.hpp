#pragma once
#include <optional>
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>

namespace ryn {
namespace detail {
class DividerComponentHost;
}
enum class DividerType { Horizontal, Vertical };
enum class DividerOrientation { Left, Center, Right };

struct DividerOrientationMargin final {
    enum class Source { Theme, None, Ratio };
    Source source{Source::Theme};
    float ratio{};

    static constexpr DividerOrientationMargin theme() {
        return {};
    }

    static constexpr DividerOrientationMargin none() {
        return {Source::None, 0};
    }

    static constexpr DividerOrientationMargin fraction(float ratio) {
        return {Source::Ratio, ratio};
    }

    friend bool operator==(const DividerOrientationMargin&, const DividerOrientationMargin&) = default;
};

struct DividerTextSlot final {};

using DividerText = SlotContent<DividerTextSlot>;

class DividerProps final {
public:
    DividerProps& type(Prop<DividerType> value) {
        type_ = std::move(value);
        return *this;
    }

    DividerProps& orientation(Prop<DividerOrientation> value) {
        orientation_ = std::move(value);
        return *this;
    }

    DividerProps& orientationMargin(Prop<DividerOrientationMargin> value) {
        margin_ = std::move(value);
        return *this;
    }

    DividerProps& dashed(Prop<bool> value) {
        dashed_ = std::move(value);
        return *this;
    }

    DividerProps& plain(Prop<bool> value) {
        plain_ = std::move(value);
        return *this;
    }

    DividerProps& content(Prop<String> value) {
        content_ = std::move(value);
        return *this;
    }

    template <std::size_t N> DividerProps& content(const char8_t (&value)[N]) {
        return content(String{value});
    }

    DividerProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::DividerComponentHost;
    Prop<DividerType> type_{DividerType::Horizontal};
    Prop<DividerOrientation> orientation_{DividerOrientation::Center};
    Prop<DividerOrientationMargin> margin_{DividerOrientationMargin{}};
    Prop<bool> dashed_{false};
    Prop<bool> plain_{false};
    Prop<String> content_{String{}};
    LayoutStyle layout_;
};

void Divider(DividerProps props = {});
void Divider(DividerProps props, DividerText text);
} // namespace ryn
