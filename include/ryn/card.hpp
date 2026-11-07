#pragma once
#include <optional>
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <ryn/tabs.hpp>

namespace ryn {
namespace detail {
struct ConfigDefaults;
}

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
    CardProps& tabList(Prop<std::vector<TabItem>> value) {
        tabs_ = std::move(value);
        return *this;
    }

    CardProps& tabProps(TabsProps value) {
        tab_props_ = std::move(value);
        return *this;
    }

    CardProps& activeTabKey(Prop<String> value) {
        active_tab_ = std::move(value);
        return *this;
    }

    CardProps& defaultActiveTabKey(String value) {
        default_tab_ = std::move(value);
        return *this;
    }

    CardProps& onTabChange(std::function<void(String)> value) {
        on_tab_change_ = std::move(value);
        return *this;
    }

    CardProps& tabBarExtraContent(TabsExtra value) {
        tab_extra_ = std::move(value);
        return *this;
    }

    CardProps& gridColumns(Prop<std::size_t> value) {
        grid_columns_ = std::move(value);
        return *this;
    }

    CardProps& animated(Prop<bool> value = true) {
        animated_ = std::move(value);
        return *this;
    }

    CardProps& title(Prop<String> value) {
        title_ = std::move(value);
        return *this;
    }

    template <std::size_t N> CardProps& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    CardProps& size(Prop<CardSize> value) {
        size_ = std::move(value);
        explicit_size_ = true;
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
    friend struct detail::ConfigDefaults;
    friend class detail::CardComponentHost;
    Prop<String> title_{String{}};
    Prop<CardSize> size_{CardSize::Default};
    bool explicit_size_{};
    Prop<CardVariant> variant_{CardVariant::Outlined};
    Prop<bool> hoverable_{false};
    Prop<bool> loading_{false};
    Prop<bool> animated_{true};
    Prop<std::size_t> grid_columns_{3};
    std::optional<Prop<std::vector<TabItem>>> tabs_;
    std::optional<TabsProps> tab_props_;
    std::optional<TabsExtra> tab_extra_;
    std::optional<Prop<String>> active_tab_;
    std::optional<String> default_tab_;
    std::function<void(String)> on_tab_change_;
    LayoutStyle layout_;
};

void Card(CardProps props, CardContent content, CardSlots slots = {});

struct CardGridSlot final {};

using CardGridContent = SlotContent<CardGridSlot>;

class CardGridProps final {
public:
    CardGridProps& hoverable(Prop<bool> value = true) {
        hoverable_ = std::move(value);
        return *this;
    }

    CardGridProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::CardComponentHost;
    Prop<bool> hoverable_{true};
    LayoutStyle layout_;
};

struct CardMetaAvatarSlot final {};

struct CardMetaTitleSlot final {};

struct CardMetaDescriptionSlot final {};

using CardMetaAvatar = SlotContent<CardMetaAvatarSlot>;
using CardMetaTitle = SlotContent<CardMetaTitleSlot>;
using CardMetaDescription = SlotContent<CardMetaDescriptionSlot>;

struct CardMetaSlots final {
    std::optional<CardMetaAvatar> avatar;
    std::optional<CardMetaTitle> title;
    std::optional<CardMetaDescription> description;
};

class CardMetaProps final {
public:
    CardMetaProps& title(Prop<String> value) {
        title_ = std::move(value);
        return *this;
    }

    template <std::size_t N> CardMetaProps& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    CardMetaProps& description(Prop<String> value) {
        description_ = std::move(value);
        return *this;
    }

    template <std::size_t N> CardMetaProps& description(const char8_t (&value)[N]) {
        return description(String{value});
    }

    CardMetaProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::CardComponentHost;
    Prop<String> title_{String{}};
    Prop<String> description_{String{}};
    LayoutStyle layout_;
};

void CardGrid(CardGridProps props, CardGridContent content);
void CardMeta(CardMetaProps props = {}, CardMetaSlots slots = {});
} // namespace ryn
