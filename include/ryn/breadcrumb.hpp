#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <functional>
#include <optional>
#include <vector>

namespace ryn {
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
        return *this;
    }

    template <std::size_t N> BreadcrumbItem& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    BreadcrumbItem& richTitle(BreadcrumbTitle value) {
        rich_title_ = std::move(value);
        return *this;
    }

    BreadcrumbItem& icon(BreadcrumbIcon value) {
        icon_ = std::move(value);
        return *this;
    }

    BreadcrumbItem& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    BreadcrumbItem& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

private:
    friend class detail::BreadcrumbComponentHost;
    String key_;
    Prop<String> title_;
    std::optional<BreadcrumbTitle> rich_title_;
    std::optional<BreadcrumbIcon> icon_;
    Prop<bool> disabled_{false};
    Prop<bool> visible_{true};
};

class BreadcrumbProps final {
public:
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
    friend class detail::BreadcrumbComponentHost;
    std::optional<BreadcrumbSeparator> separator_;
    std::function<void(BreadcrumbActivation)> on_activate_;
    LayoutStyle layout_;
};

void Breadcrumb(BreadcrumbProps props, std::vector<BreadcrumbItem> items);
} // namespace ryn
