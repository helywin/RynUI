#pragma once
#include <optional>
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>

namespace ryn {
namespace detail {
class CardComponentHost;
}

enum class CardSize { Default, Small };

enum class CardVariant { Outlined, Borderless };

struct CardContentSlot final {};

struct CardTitleSlot final {};

struct CardExtraSlot final {};

struct CardCoverSlot final {};

struct CardActionsSlot final {};

using CardContent = SlotContent<CardContentSlot>;
using CardTitle = SlotContent<CardTitleSlot>;
using CardExtra = SlotContent<CardExtraSlot>;
using CardCover = SlotContent<CardCoverSlot>;
using CardActions = SlotContent<CardActionsSlot>;

struct CardSlots final {
    std::optional<CardTitle> title;
    std::optional<CardExtra> extra;
    std::optional<CardCover> cover;
    std::optional<CardActions> actions;
};

class CardProps final {
public:
    CardProps& title(Prop<String> value) {
        title_ = std::move(value);
        return *this;
    }

    template <std::size_t N> CardProps& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    CardProps& size(Prop<CardSize> value) {
        size_ = std::move(value);
        return *this;
    }

    CardProps& variant(Prop<CardVariant> value) {
        variant_ = std::move(value);
        return *this;
    }

    CardProps& hoverable(Prop<bool> value = true) {
        hoverable_ = std::move(value);
        return *this;
    }

    CardProps& loading(Prop<bool> value = true) {
        loading_ = std::move(value);
        return *this;
    }

    CardProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::CardComponentHost;
    Prop<String> title_{String{}};
    Prop<CardSize> size_{CardSize::Default};
    Prop<CardVariant> variant_{CardVariant::Outlined};
    Prop<bool> hoverable_{false};
    Prop<bool> loading_{false};
    LayoutStyle layout_;
};

void Card(CardProps props, CardContent content, CardSlots slots = {});
} // namespace ryn
