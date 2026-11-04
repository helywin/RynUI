#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <cmath>
#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ryn {
namespace detail {
class SplitterComponentHost;
}

enum class SplitterOrientation { Horizontal, Vertical };
enum class SplitterDirection { LeftToRight, RightToLeft };
enum class SplitterIconVisibility { Auto, Always, Hidden };

class SplitterSize final {
public:
    explicit SplitterSize(LogicalLength value) : value_(value.value()) {
        if (value.is_auto() || !std::isfinite(value_) || value_ < 0) {
            throw std::invalid_argument("Splitter size must be a finite non-negative logical length");
        }
    }

    [[nodiscard]] static SplitterSize percent(float value) {
        if (!std::isfinite(value) || value < 0 || value > 100) {
            throw std::invalid_argument("Splitter percentage must be within [0, 100]");
        }
        auto result = SplitterSize{dp(value)};
        result.percentage_ = true;
        return result;
    }

    [[nodiscard]] float value() const noexcept {
        return value_;
    }

    [[nodiscard]] bool is_percentage() const noexcept {
        return percentage_;
    }

    [[nodiscard]] float resolve(float extent) const noexcept {
        return percentage_ ? extent * (value_ / 100) : value_;
    }

    friend bool operator==(SplitterSize, SplitterSize) = default;

private:
    float value_{};
    bool percentage_{};
};

struct SplitterCollapsible final {
    bool start{};
    bool end{};
    SplitterIconVisibility show_icon{SplitterIconVisibility::Auto};
    friend bool operator==(SplitterCollapsible, SplitterCollapsible) = default;
};

struct SplitterPanelContentSlot final {};

struct SplitterDraggerIconSlot final {};

struct SplitterStartIconSlot final {};

struct SplitterEndIconSlot final {};

using SplitterPanelContent = SlotContent<SplitterPanelContentSlot>;
using SplitterDraggerIcon = SlotContent<SplitterDraggerIconSlot>;
using SplitterStartIcon = SlotContent<SplitterStartIconSlot>;
using SplitterEndIcon = SlotContent<SplitterEndIconSlot>;
using SplitterSizes = std::vector<float>;
using SplitterCollapsed = std::vector<bool>;

class SplitterPanel final {
public:
    explicit SplitterPanel(SplitterPanelContent content) : content_(std::move(content)) {}

    SplitterPanel& size(Prop<SplitterSize> value) {
        size_ = std::move(value);
        return *this;
    }

    SplitterPanel& defaultSize(SplitterSize value) {
        default_size_ = value;
        return *this;
    }

    SplitterPanel& min(Prop<SplitterSize> value) {
        min_ = std::move(value);
        return *this;
    }

    SplitterPanel& max(Prop<SplitterSize> value) {
        max_ = std::move(value);
        return *this;
    }

    SplitterPanel& resizable(Prop<bool> value = true) {
        resizable_ = std::move(value);
        return *this;
    }

    SplitterPanel& collapsible(Prop<SplitterCollapsible> value) {
        collapsible_ = std::move(value);
        return *this;
    }

    SplitterPanel& collapsible(bool value = true) {
        return collapsible(SplitterCollapsible{value, value});
    }

    SplitterPanel& destroyOnHidden(Prop<bool> value = true) {
        destroy_on_hidden_ = std::move(value);
        return *this;
    }

private:
    friend class detail::SplitterComponentHost;
    SplitterPanelContent content_;
    std::optional<Prop<SplitterSize>> size_;
    std::optional<SplitterSize> default_size_;
    Prop<SplitterSize> min_{SplitterSize{dp(0)}};
    Prop<SplitterSize> max_{SplitterSize::percent(100)};
    Prop<bool> resizable_{true};
    Prop<SplitterCollapsible> collapsible_{SplitterCollapsible{}};
    std::optional<Prop<bool>> destroy_on_hidden_;
};

class SplitterProps final {
public:
    SplitterProps& orientation(Prop<SplitterOrientation> value) {
        orientation_ = std::move(value);
        return *this;
    }

    SplitterProps& vertical(Prop<bool> value = true) {
        vertical_ = std::move(value);
        return *this;
    }

    SplitterProps& direction(Prop<SplitterDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    SplitterProps& lazy(Prop<bool> value = true) {
        lazy_ = std::move(value);
        return *this;
    }

    SplitterProps& destroyOnHidden(Prop<bool> value = true) {
        destroy_on_hidden_ = std::move(value);
        return *this;
    }

    SplitterProps& collapseMotion(Prop<bool> value = true) {
        collapse_motion_ = std::move(value);
        return *this;
    }

    SplitterProps& draggerIcon(SplitterDraggerIcon value) {
        dragger_icon_ = std::move(value);
        return *this;
    }

    SplitterProps& startIcon(SplitterStartIcon value) {
        start_icon_ = std::move(value);
        return *this;
    }

    SplitterProps& endIcon(SplitterEndIcon value) {
        end_icon_ = std::move(value);
        return *this;
    }

    SplitterProps& onResizeStart(std::function<void(SplitterSizes)> value) {
        on_resize_start_ = std::move(value);
        return *this;
    }

    SplitterProps& onResize(std::function<void(SplitterSizes)> value) {
        on_resize_ = std::move(value);
        return *this;
    }

    SplitterProps& onResizeEnd(std::function<void(SplitterSizes)> value) {
        on_resize_end_ = std::move(value);
        return *this;
    }

    SplitterProps& onCollapse(std::function<void(SplitterCollapsed, SplitterSizes)> value) {
        on_collapse_ = std::move(value);
        return *this;
    }

    SplitterProps& onDraggerDoubleClick(std::function<void(std::size_t)> value) {
        on_double_click_ = std::move(value);
        return *this;
    }

    SplitterProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::SplitterComponentHost;
    std::optional<Prop<SplitterOrientation>> orientation_;
    Prop<bool> vertical_{false};
    Prop<SplitterDirection> direction_{SplitterDirection::LeftToRight};
    Prop<bool> lazy_{false};
    Prop<bool> destroy_on_hidden_{false};
    Prop<bool> collapse_motion_{false};
    std::optional<SplitterDraggerIcon> dragger_icon_;
    std::optional<SplitterStartIcon> start_icon_;
    std::optional<SplitterEndIcon> end_icon_;
    std::function<void(SplitterSizes)> on_resize_start_;
    std::function<void(SplitterSizes)> on_resize_;
    std::function<void(SplitterSizes)> on_resize_end_;
    std::function<void(SplitterCollapsed, SplitterSizes)> on_collapse_;
    std::function<void(std::size_t)> on_double_click_;
    LayoutStyle layout_;
};

void Splitter(SplitterProps props, std::vector<SplitterPanel> panels);

} // namespace ryn
