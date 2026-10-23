#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>
#include <functional>
#include <optional>
#include <vector>

namespace ryn {
namespace detail {
class MenuComponentHost;
class DropdownComponentHost;
} // namespace detail

using MenuKeys = std::vector<String>;

enum class MenuMode { Vertical, Inline };

enum class MenuItemKind { Item, Group, Divider, SubMenu };

struct MenuIconSlot final {};

using MenuIcon = SlotContent<MenuIconSlot>;

// Topology and keys are fixed at mount. Each item's presentation remains reactive.
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
        items_ = std::move(value);
        return *this;
    }

    MenuProps& mode(MenuMode value) {
        mode_ = value;
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
    friend class detail::MenuComponentHost;
    friend class detail::DropdownComponentHost;
    std::vector<MenuItem> items_;
    MenuMode mode_{MenuMode::Vertical};
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
