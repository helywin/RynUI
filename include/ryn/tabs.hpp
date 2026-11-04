#pragma once
#include <functional>
#include <optional>
#include <vector>
#include <memory>
#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>

namespace ryn {
namespace detail {
class TabsComponentHost;
}

enum class TabsType { Line, Card, EditableCard };

enum class TabsPlacement { Top, Bottom, Left, Right };
enum class TabsEditAction { Add, Remove };
enum class TabsIndicatorAlign { Start, Center, End };

struct TabsIndicator final {
    std::optional<float> size;
    TabsIndicatorAlign align{TabsIndicatorAlign::Center};
    friend bool operator==(const TabsIndicator&, const TabsIndicator&) = default;
};

struct TabLabelSlot final {};

struct TabsExtraSlot final {};

using TabLabel = SlotContent<TabLabelSlot>;
using TabsExtra = SlotContent<TabsExtraSlot>;

struct TabContentSlot final {};

using TabContent = SlotContent<TabContentSlot>;

class TabItem final {
public:
    TabItem(String key, String label, TabContent content)
        : key_(std::move(key)), label_(std::move(label)), content_(std::move(content)) {}

    [[nodiscard]] const String& key() const noexcept {
        return key_;
    }

    TabItem& label(Prop<String> value) {
        label_ = std::move(value);
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    template <std::size_t N> TabItem& label(const char8_t (&value)[N]) {
        return label(String{value});
    }

    TabItem& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    TabItem& richLabel(TabLabel value) {
        rich_label_ = std::move(value);
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    TabItem& closable(Prop<bool> value = true) {
        closable_ = std::move(value);
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    TabItem& forceRender(bool value = true) {
        force_render_ = value;
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    TabItem& destroyOnHidden(std::optional<bool> value) {
        destroy_hidden_ = value;
        revision_ = std::make_shared<const int>(0);
        return *this;
    }

    friend bool operator==(const TabItem& a, const TabItem& b) {
        return a.revision_ == b.revision_;
    }

private:
    friend class detail::TabsComponentHost;
    std::shared_ptr<const int> revision_{std::make_shared<const int>(0)};
    std::optional<TabLabel> rich_label_;
    Prop<bool> closable_{true};
    bool force_render_{};
    std::optional<bool> destroy_hidden_;
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

    TabsProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        return *this;
    }

    TabsProps& lazy(Prop<bool> value = true) {
        lazy_ = std::move(value);
        return *this;
    }

    TabsProps& destroyOnHidden(Prop<bool> value = true) {
        destroy_hidden_ = std::move(value);
        return *this;
    }

    TabsProps& hideAdd(Prop<bool> value = true) {
        hide_add_ = std::move(value);
        return *this;
    }

    TabsProps& animated(Prop<bool> value = true) {
        animated_ = std::move(value);
        return *this;
    }

    TabsProps& indicator(Prop<TabsIndicator> value) {
        indicator_ = std::move(value);
        return *this;
    }

    TabsProps& extraStart(TabsExtra value) {
        extra_start_ = std::move(value);
        return *this;
    }

    TabsProps& extraEnd(TabsExtra value) {
        extra_end_ = std::move(value);
        return *this;
    }

    TabsProps& onEdit(std::function<void(String, TabsEditAction)> value) {
        on_edit_ = std::move(value);
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
    Prop<bool> rtl_{false};
    Prop<bool> lazy_{false};
    Prop<bool> destroy_hidden_{false};
    Prop<bool> hide_add_{false};
    Prop<bool> animated_{true};
    Prop<TabsIndicator> indicator_{TabsIndicator{}};
    std::optional<TabsExtra> extra_start_;
    std::optional<TabsExtra> extra_end_;
    std::function<void(String, TabsEditAction)> on_edit_;
    std::function<void(String)> on_change_;
    std::function<void(String)> on_tab_click_;
    LayoutStyle layout_;
};

void Tabs(TabsProps props, std::vector<TabItem> items);
void Tabs(TabsProps props, Prop<std::vector<TabItem>> items);
} // namespace ryn
