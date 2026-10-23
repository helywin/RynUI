#pragma once
#include <functional>
#include <optional>
#include <vector>
#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>

namespace ryn {
namespace detail {
class TabsComponentHost;
}

enum class TabsType { Line, Card };

enum class TabsPlacement { Top, Bottom };

struct TabContentSlot final {};

using TabContent = SlotContent<TabContentSlot>;

class TabItem final {
public:
    TabItem(String key, String label, TabContent content)
        : key_(std::move(key)), label_(std::move(label)), content_(std::move(content)) {}

    TabItem& label(Prop<String> value) {
        label_ = std::move(value);
        return *this;
    }

    template <std::size_t N> TabItem& label(const char8_t (&value)[N]) {
        return label(String{value});
    }

    TabItem& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TabsComponentHost;
    String key_;
    Prop<String> label_;
    TabContent content_;
    Prop<bool> disabled_{false};
};

class TabsProps final {
public:
    TabsProps& activeKey(Prop<String> value) {
        active_key_ = std::move(value);
        return *this;
    }

    template <std::size_t N> TabsProps& activeKey(const char8_t (&value)[N]) {
        return activeKey(String{value});
    }

    TabsProps& defaultActiveKey(String value) {
        default_key_ = std::move(value);
        return *this;
    }

    TabsProps& type(Prop<TabsType> value) {
        type_ = std::move(value);
        return *this;
    }

    TabsProps& placement(Prop<TabsPlacement> value) {
        placement_ = std::move(value);
        return *this;
    }

    TabsProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    TabsProps& centered(Prop<bool> value = true) {
        centered_ = std::move(value);
        return *this;
    }

    TabsProps& onChange(std::function<void(String)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    TabsProps& onTabClick(std::function<void(String)> value) {
        on_tab_click_ = std::move(value);
        return *this;
    }

    TabsProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TabsComponentHost;
    std::optional<Prop<String>> active_key_;
    std::optional<String> default_key_;
    Prop<TabsType> type_{TabsType::Line};
    Prop<TabsPlacement> placement_{TabsPlacement::Top};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<bool> centered_{false};
    std::function<void(String)> on_change_;
    std::function<void(String)> on_tab_click_;
    LayoutStyle layout_;
};

void Tabs(TabsProps props, std::vector<TabItem> items);
} // namespace ryn
