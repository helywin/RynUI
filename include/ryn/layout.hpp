#pragma once

#include <ryn/breakpoints.hpp>
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <functional>
#include <optional>
#include <utility>

namespace ryn {
namespace detail {
class ShellLayoutComponentHost;
}

enum class SiderTheme { Light, Dark };
using SiderBreakpoint = Breakpoint;
enum class SiderCollapseType { ClickTrigger, Responsive };
enum class SiderTriggerMode { Default, Hidden };

struct LayoutContentSlot final {};

struct HeaderContentSlot final {};

struct FooterContentSlot final {};

struct ContentBodySlot final {};

struct SiderContentSlot final {};

struct SiderTriggerSlot final {};

using LayoutContent = SlotContent<LayoutContentSlot>;
using HeaderContent = SlotContent<HeaderContentSlot>;
using FooterContent = SlotContent<FooterContentSlot>;
using ContentBody = SlotContent<ContentBodySlot>;
using SiderContent = SlotContent<SiderContentSlot>;
using SiderTrigger = SlotContent<SiderTriggerSlot>;

class LayoutProps final {
public:
    LayoutProps& hasSider(Prop<bool> value) {
        has_sider_ = std::move(value);
        return *this;
    }

    LayoutProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::ShellLayoutComponentHost;
    std::optional<Prop<bool>> has_sider_;
    LayoutStyle layout_;
};

class HeaderProps final {
public:
    HeaderProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::ShellLayoutComponentHost;
    LayoutStyle layout_;
};

class FooterProps final {
public:
    FooterProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::ShellLayoutComponentHost;
    LayoutStyle layout_;
};

class ContentProps final {
public:
    ContentProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::ShellLayoutComponentHost;
    LayoutStyle layout_;
};

class SiderProps final {
public:
    SiderProps& width(Prop<LogicalLength> value) {
        width_ = std::move(value);
        return *this;
    }

    SiderProps& collapsedWidth(Prop<LogicalLength> value) {
        collapsed_width_ = std::move(value);
        return *this;
    }

    SiderProps& collapsed(Prop<bool> value) {
        collapsed_ = std::move(value);
        return *this;
    }

    SiderProps& defaultCollapsed(bool value) {
        default_collapsed_ = value;
        return *this;
    }

    SiderProps& collapsible(Prop<bool> value = true) {
        collapsible_ = std::move(value);
        return *this;
    }

    SiderProps& theme(Prop<SiderTheme> value) {
        theme_ = std::move(value);
        return *this;
    }

    SiderProps& breakpoint(Prop<std::optional<SiderBreakpoint>> value) {
        breakpoint_ = std::move(value);
        return *this;
    }

    SiderProps& breakpoint(SiderBreakpoint value) {
        return breakpoint(std::optional{value});
    }

    SiderProps& reverseArrow(Prop<bool> value = true) {
        reverse_arrow_ = std::move(value);
        return *this;
    }

    SiderProps& trigger(SiderTrigger value) {
        trigger_ = std::move(value);
        trigger_mode_ = SiderTriggerMode::Default;
        return *this;
    }

    SiderProps& trigger(SiderTriggerMode value) {
        trigger_mode_ = value;
        trigger_.reset();
        return *this;
    }

    SiderProps& onBreakpoint(std::function<void(bool)> value) {
        on_breakpoint_ = std::move(value);
        return *this;
    }

    SiderProps& onCollapse(std::function<void(bool, SiderCollapseType)> value) {
        on_collapse_ = std::move(value);
        return *this;
    }

    SiderProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::ShellLayoutComponentHost;
    Prop<LogicalLength> width_{dp(200)};
    Prop<LogicalLength> collapsed_width_{dp(80)};
    std::optional<Prop<bool>> collapsed_;
    std::optional<bool> default_collapsed_;
    Prop<bool> collapsible_{false};
    Prop<SiderTheme> theme_{SiderTheme::Dark};
    Prop<std::optional<SiderBreakpoint>> breakpoint_{std::optional<SiderBreakpoint>{}};
    Prop<bool> reverse_arrow_{false};
    std::optional<SiderTrigger> trigger_;
    SiderTriggerMode trigger_mode_{SiderTriggerMode::Default};
    std::function<void(bool)> on_breakpoint_;
    std::function<void(bool, SiderCollapseType)> on_collapse_;
    LayoutStyle layout_;
};

void Layout(LayoutProps props, LayoutContent content);
void Header(HeaderProps props, HeaderContent content);
void Footer(FooterProps props, FooterContent content);
void ContentArea(ContentProps props, ContentBody content);
void Sider(SiderProps props, SiderContent content);

} // namespace ryn
