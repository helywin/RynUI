#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>
#include <ryn/text_direction.hpp>
#include <functional>
#include <optional>
#include <vector>

namespace ryn {
namespace detail {
struct ConfigDefaults;
}

namespace detail {
class MenuComponentHost;
class DropdownComponentHost;
} // namespace detail

using MenuKeys = std::vector<String>;

enum class MenuMode { Vertical, Inline, Horizontal };

enum class MenuTheme { Inherit, Light, Dark };

enum class MenuSubMenuTrigger { Hover, Click };

enum class MenuItemKind { Item, Group, Divider, SubMenu };

struct MenuIconSlot final {};

using MenuIcon = SlotContent<MenuIconSlot>;

struct MenuLabelSlot final {};

using MenuLabel = SlotContent<MenuLabelSlot>;

struct MenuExtraSlot final {};

using MenuExtra = SlotContent<MenuExtraSlot>;

// Keys retain row/slot identity across reactive topology updates. Slot factories run once per key.
struct MenuItem final {
    MenuItem() = default;

    MenuItem(String key, Prop<String> label, Prop<bool> disabled = false, Prop<bool> danger = false,
             MenuItemKind kind = MenuItemKind::Item, std::vector<MenuItem> children = {},
             std::optional<MenuIcon> icon = {})
        : key(std::move(key)), label(std::move(label)), disabled(std::move(disabled)), danger(std::move(danger)),
          kind(kind), children(std::move(children)), icon(std::move(icon)) {}

    MenuItem(String key, String label, Prop<bool> disabled = false, Prop<bool> danger = false,
             MenuItemKind kind = MenuItemKind::Item, std::vector<MenuItem> children = {},
             std::optional<MenuIcon> icon = {})
        : MenuItem(std::move(key), Prop<String>{std::move(label)}, std::move(disabled), std::move(danger), kind,
                   std::move(children), std::move(icon)) {}

    String key;
    Prop<String> label{String{}};
    Prop<bool> disabled{false};
    Prop<bool> danger{false};
    MenuItemKind kind{MenuItemKind::Item};
    std::vector<MenuItem> children;
    std::optional<MenuIcon> icon;
    std::optional<MenuLabel> content;
    std::optional<MenuExtra> extra;
};

struct MenuClickInfo final {
    String key;
    MenuKeys key_path; // Leaf first, then containing submenu keys.
};

struct MenuSelectInfo final {
    String key;
    MenuKeys selected_keys;
    bool selected{};
};

class MenuProps final {
public:
    MenuProps& items(std::vector<MenuItem> value) {
        dynamic_items_.reset();
        items_ = std::move(value);
        return *this;
    }

    MenuProps& items(std::initializer_list<MenuItem> value) {
        return items(std::vector<MenuItem>{value});
    }

    MenuProps& items(Prop<std::vector<MenuItem>> value) {
        dynamic_items_ = std::move(value);
        return *this;
    }

    MenuProps& mode(Prop<MenuMode> value) {
        mode_ = value;
        return *this;
    }

    MenuProps& theme(Prop<MenuTheme> value) {
        theme_ = std::move(value);
        return *this;
    }

    MenuProps& direction(Prop<TextDirection> value) {
        direction_ = std::move(value);
        explicit_direction_ = true;
        return *this;
    }

    MenuProps& inlineCollapsed(Prop<bool> value = true) {
        collapsed_ = std::move(value);
        return *this;
    }

    MenuProps& triggerSubMenuAction(Prop<MenuSubMenuTrigger> value) {
        submenu_trigger_ = std::move(value);
        return *this;
    }

    MenuProps& subMenuOpenDelay(Prop<float> seconds) {
        open_delay_ = std::move(seconds);
        return *this;
    }

    MenuProps& subMenuCloseDelay(Prop<float> seconds) {
        close_delay_ = std::move(seconds);
        return *this;
    }

    MenuProps& selectedKeys(Prop<MenuKeys> value) {
        selected_ = std::move(value);
        return *this;
    }

    MenuProps& defaultSelectedKeys(MenuKeys value) {
        default_selected_ = std::move(value);
        return *this;
    }

    MenuProps& openKeys(Prop<MenuKeys> value) {
        open_ = std::move(value);
        return *this;
    }

    MenuProps& defaultOpenKeys(MenuKeys value) {
        default_open_ = std::move(value);
        return *this;
    }

    MenuProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    MenuProps& selectable(Prop<bool> value) {
        selectable_ = std::move(value);
        explicit_selectable_ = true;
        return *this;
    }

    MenuProps& multiple(Prop<bool> value = true) {
        multiple_ = std::move(value);
        return *this;
    }

    MenuProps& onClick(std::function<void(MenuClickInfo)> value) {
        on_click_ = std::move(value);
        return *this;
    }

    MenuProps& onSelect(std::function<void(MenuSelectInfo)> value) {
        on_select_ = std::move(value);
        return *this;
    }

    MenuProps& onOpenChange(std::function<void(MenuKeys)> value) {
        on_open_ = std::move(value);
        return *this;
    }

    MenuProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::ConfigDefaults;
    friend class detail::MenuComponentHost;
    friend class detail::DropdownComponentHost;
    std::vector<MenuItem> items_;
    std::optional<Prop<std::vector<MenuItem>>> dynamic_items_;
    Prop<MenuMode> mode_{MenuMode::Vertical};
    Prop<MenuTheme> theme_{MenuTheme::Inherit};
    Prop<TextDirection> direction_{TextDirection::LeftToRight};
    bool explicit_direction_{};
    Prop<bool> collapsed_{false};
    Prop<MenuSubMenuTrigger> submenu_trigger_{MenuSubMenuTrigger::Hover};
    Prop<float> open_delay_{0.0F};
    Prop<float> close_delay_{0.1F};
    std::optional<Prop<MenuKeys>> selected_;
    std::optional<MenuKeys> default_selected_;
    std::optional<Prop<MenuKeys>> open_;
    std::optional<MenuKeys> default_open_;
    Prop<bool> disabled_{false};
    Prop<bool> selectable_{true};
    bool explicit_selectable_{};
    Prop<bool> multiple_{false};
    std::function<void(MenuClickInfo)> on_click_;
    std::function<void(MenuSelectInfo)> on_select_;
    std::function<void(MenuKeys)> on_open_;
    LayoutStyle layout_;
};

void Menu(MenuProps props);
} // namespace ryn
