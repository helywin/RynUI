#pragma once
#include <ryn/component.hpp>
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>

namespace ryn {
namespace detail {
class DecorationComponentHost;
}

enum class BorderBeamDirection { Clockwise, Counterclockwise };

struct BorderBeamContentSlot final {};

using BorderBeamContent = SlotContent<BorderBeamContentSlot>;

class BorderBeamProps final {
public:
    BorderBeamProps& duration(Prop<Duration> v) {
        duration_ = std::move(v);
        return *this;
    }

    BorderBeamProps& direction(Prop<BorderBeamDirection> v) {
        direction_ = std::move(v);
        return *this;
    }

    BorderBeamProps& visible(Prop<bool> v) {
        visible_ = std::move(v);
        return *this;
    }

    BorderBeamProps& disabled(Prop<bool> v = true) {
        disabled_ = std::move(v);
        return *this;
    }

    BorderBeamProps& layout(LayoutStyle v) {
        layout_ = std::move(v);
        return *this;
    }

private:
    friend class detail::DecorationComponentHost;
    Prop<Duration> duration_{Duration::milliseconds(6000)};
    Prop<BorderBeamDirection> direction_{BorderBeamDirection::Clockwise};
    Prop<bool> visible_{true};
    Prop<bool> disabled_{false};
    LayoutStyle layout_;
};

void BorderBeam(BorderBeamProps props, BorderBeamContent content);
} // namespace ryn
