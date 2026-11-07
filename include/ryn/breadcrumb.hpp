#pragma once

#include <ryn/component.hpp>
#include <ryn/dropdown.hpp>
#include <ryn/text_direction.hpp>
#include <memory>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <functional>
#include <optional>
#include <vector>

namespace ryn {
namespace detail {
struct ConfigDefaults;
}

namespace detail {
class BreadcrumbComponentHost;
}

struct BreadcrumbTitleSlot final {};

struct BreadcrumbIconSlot final {};

struct BreadcrumbSeparatorSlot final {};

using BreadcrumbTitle = SlotContent<BreadcrumbTitleSlot>;
using BreadcrumbIcon = SlotContent<BreadcrumbIconSlot>;
using BreadcrumbSeparator = SlotContent<BreadcrumbSeparatorSlot>;

struct BreadcrumbActivation final {
    String key;
    std::size_t index{};
};

class BreadcrumbItem final {
public:
    BreadcrumbItem(String key, String title) : key_(std::move(key)), title_(std::move(title)) {}

    BreadcrumbItem& title(Prop<String> value) {
        title_ = std::move(value);
        touch();
        return *this;
    }

    template <std::size_t N> BreadcrumbItem& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    BreadcrumbItem& richTitle(BreadcrumbTitle value) {
        rich_title_ = std::make_shared<const BreadcrumbTitle>(std::move(value));
        touch();
        return *this;
    }

    BreadcrumbItem& icon(BreadcrumbIcon value) {
        icon_ = std::make_shared<const BreadcrumbIcon>(std::move(value));
        touch();
        return *this;
    }

    BreadcrumbItem& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        touch();
        return *this;
    }

    BreadcrumbItem& visible(Prop<bool> value) {
        visible_ = std::move(value);
        touch();
        return *this;
    }

    BreadcrumbItem& separator(BreadcrumbSeparator value) {
        separator_ = std::make_shared<const BreadcrumbSeparator>(std::move(value));
        touch();
        return *this;
    }

    BreadcrumbItem& menu(MenuProps value) {
        menu_ = std::make_shared<const MenuProps>(std::move(value));
        touch();
        return *this;
    }

    BreadcrumbItem& dropdownProps(DropdownProps value) {
        dropdown_ = std::make_shared<const DropdownProps>(std::move(value));
        touch();
        return *this;
    }

    BreadcrumbItem& dropdownIcon(BreadcrumbIcon value) {
        dropdown_icon_ = std::make_shared<const BreadcrumbIcon>(std::move(value));
        touch();
        return *this;
    }

    BreadcrumbItem& onActivate(std::function<void(BreadcrumbActivation)> value) {
        on_activate_ = std::move(value);
        touch();
        return *this;
    }

    friend bool operator==(const BreadcrumbItem& a, const BreadcrumbItem& b) {
        return a.key_ == b.key_ && a.revision_ == b.revision_;
    }

private:
    friend class detail::BreadcrumbComponentHost;

    void touch() {
        revision_ = std::make_shared<const int>(0);
    }

    std::shared_ptr<const int> revision_{std::make_shared<const int>(0)};
    std::shared_ptr<const BreadcrumbSeparator> separator_;
    std::shared_ptr<const MenuProps> menu_;
    std::shared_ptr<const DropdownProps> dropdown_;
    std::shared_ptr<const BreadcrumbIcon> dropdown_icon_;
    std::function<void(BreadcrumbActivation)> on_activate_;
    String key_;
    Prop<String> title_;
    std::shared_ptr<const BreadcrumbTitle> rich_title_;
    std::shared_ptr<const BreadcrumbIcon> icon_;
    Prop<bool> disabled_{false};
    Prop<bool> visible_{true};
};

class BreadcrumbProps final {
public:
    BreadcrumbProps& items(Prop<std::vector<BreadcrumbItem>> value) {
        items_ = std::move(value);
        return *this;
    }

    BreadcrumbProps& wrap(Prop<bool> value) {
        wrap_ = std::move(value);
        return *this;
    }

    BreadcrumbProps& direction(Prop<TextDirection> value) {
        direction_ = std::move(value);
        explicit_direction_ = true;
        return *this;
    }

    BreadcrumbProps& separator(BreadcrumbSeparator value) {
        separator_ = std::move(value);
        return *this;
    }

    BreadcrumbProps& onActivate(std::function<void(BreadcrumbActivation)> value) {
        on_activate_ = std::move(value);
        return *this;
    }

    BreadcrumbProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::ConfigDefaults;
    friend class detail::BreadcrumbComponentHost;
    Prop<std::vector<BreadcrumbItem>> items_{std::vector<BreadcrumbItem>{}};
    Prop<bool> wrap_{true};
    Prop<TextDirection> direction_{TextDirection::LeftToRight};
    bool explicit_direction_{};
    std::optional<BreadcrumbSeparator> separator_;
    std::function<void(BreadcrumbActivation)> on_activate_;
    LayoutStyle layout_;
};

void Breadcrumb(BreadcrumbProps props);
void Breadcrumb(BreadcrumbProps props, std::vector<BreadcrumbItem> items);
} // namespace ryn
