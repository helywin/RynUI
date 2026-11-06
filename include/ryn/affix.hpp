#pragma once
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <cstdint>

namespace ryn {
namespace detail {
class DecorationComponentHost;
}

// Native host publishes window-logical viewport geometry and scroll acknowledgements.
// It also positions/translates the retained scroll subtree through normal layout.
// offset invalidates tracking; Affix never applies a second scroll translation.
struct AffixTarget final {
    std::uint64_t identity{};
    float x{};
    float y{};
    float width{};
    float height{};
    float offset{};
    bool attached{true};
    friend bool operator==(AffixTarget, AffixTarget) = default;
};

struct AffixContentSlot final {};

using AffixContent = SlotContent<AffixContentSlot>;

class AffixProps final {
public:
    AffixProps& target(Prop<AffixTarget> v) {
        target_ = std::move(v);
        return *this;
    }

    AffixProps& offsetTop(Prop<float> v) {
        top_ = std::move(v);
        return *this;
    }

    AffixProps& offsetBottom(Prop<float> v) {
        bottom_ = std::move(v);
        return *this;
    }

    AffixProps& visible(Prop<bool> v) {
        visible_ = std::move(v);
        return *this;
    }

    AffixProps& disabled(Prop<bool> v = true) {
        disabled_ = std::move(v);
        return *this;
    }

    AffixProps& onChange(std::function<void(bool)> v) {
        change_ = std::move(v);
        return *this;
    }

    AffixProps& layout(LayoutStyle v) {
        layout_ = std::move(v);
        return *this;
    }

private:
    friend class detail::DecorationComponentHost;
    std::optional<Prop<AffixTarget>> target_;
    std::optional<Prop<float>> top_;
    std::optional<Prop<float>> bottom_;
    Prop<bool> visible_{true};
    Prop<bool> disabled_{false};
    std::function<void(bool)> change_;
    LayoutStyle layout_;
};

void Affix(AffixProps props, AffixContent content);
} // namespace ryn
