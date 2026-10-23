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
class CollapseComponentHost;
}

struct CollapseTitleSlot final {};

struct CollapseBodySlot final {};

struct CollapseExtraSlot final {};

using CollapseTitle = SlotContent<CollapseTitleSlot>;
using CollapseBody = SlotContent<CollapseBodySlot>;
using CollapseExtra = SlotContent<CollapseExtraSlot>;
using CollapseKeys = std::vector<String>;

class CollapsePanel final {
public:
    CollapsePanel(String key, CollapseTitle title, CollapseBody body)
        : key_(std::move(key)), title_(std::move(title)), body_(std::move(body)) {}

    CollapsePanel& extra(CollapseExtra value) {
        extra_ = std::move(value);
        return *this;
    }

    CollapsePanel& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

private:
    friend class detail::CollapseComponentHost;
    String key_;
    CollapseTitle title_;
    CollapseBody body_;
    std::optional<CollapseExtra> extra_;
    Prop<bool> disabled_{false};
};

class CollapseProps final {
public:
    CollapseProps& activeKeys(Prop<CollapseKeys> value) {
        active_keys_ = std::move(value);
        return *this;
    }

    CollapseProps& defaultActiveKeys(CollapseKeys value) {
        default_keys_ = std::move(value);
        return *this;
    }

    CollapseProps& accordion(Prop<bool> value = true) {
        accordion_ = std::move(value);
        return *this;
    }

    CollapseProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    CollapseProps& onChange(std::function<void(CollapseKeys)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    CollapseProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::CollapseComponentHost;
    std::optional<Prop<CollapseKeys>> active_keys_;
    std::optional<CollapseKeys> default_keys_;
    Prop<bool> accordion_{false};
    Prop<ControlSize> size_{ControlSize::Middle};
    std::function<void(CollapseKeys)> on_change_;
    LayoutStyle layout_;
};

void Collapse(CollapseProps props, std::vector<CollapsePanel> panels);
} // namespace ryn
