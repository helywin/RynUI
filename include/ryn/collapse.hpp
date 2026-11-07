#pragma once
#include <functional>
#include <cstdint>
#include <optional>
#include <vector>
#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>

namespace ryn {
namespace detail {
struct ConfigDefaults;
}

namespace detail {
class CollapseComponentHost;
}

enum class CollapseCollapsible { Header, Icon, Disabled };
enum class CollapseIconPlacement { Start, End };

struct CollapseExpandInfo final {
    String key;
    Binding<bool> active;
};

struct CollapseExpandIconSlot final {};

using CollapseExpandIcon = std::function<SlotContent<CollapseExpandIconSlot>(const CollapseExpandInfo&)>;

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

    CollapsePanel& forceRender(bool value = true) {
        force_render_ = value;
        return *this;
    }

    CollapsePanel& destroyOnHidden(std::optional<bool> value) {
        destroy_hidden_ = value;
        return *this;
    }

    CollapsePanel& showArrow(Prop<bool> value) {
        show_arrow_ = std::move(value);
        return *this;
    }

    CollapsePanel& collapsible(Prop<CollapseCollapsible> value) {
        collapsible_ = std::move(value);
        return *this;
    }

    CollapsePanel& titleRevision(std::uint64_t value) {
        title_revision_ = value;
        return *this;
    }

    CollapsePanel& bodyRevision(std::uint64_t value) {
        body_revision_ = value;
        return *this;
    }

    CollapsePanel& extraRevision(std::uint64_t value) {
        extra_revision_ = value;
        return *this;
    }

private:
    friend class detail::CollapseComponentHost;
    String key_;
    CollapseTitle title_;
    CollapseBody body_;
    std::optional<CollapseExtra> extra_;
    Prop<bool> disabled_{false};
    Prop<bool> show_arrow_{true};
    std::optional<Prop<CollapseCollapsible>> collapsible_;
    bool force_render_{};
    std::optional<bool> destroy_hidden_;
    std::uint64_t title_revision_{};
    std::uint64_t body_revision_{};
    std::uint64_t extra_revision_{};
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
        explicit_size_ = true;
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

    CollapseProps& bordered(Prop<bool> value) {
        bordered_ = std::move(value);
        return *this;
    }

    CollapseProps& ghost(Prop<bool> value = true) {
        ghost_ = std::move(value);
        return *this;
    }

    CollapseProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        explicit_rtl_ = true;
        return *this;
    }

    CollapseProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    CollapseProps& lazy(Prop<bool> value = true) {
        lazy_ = std::move(value);
        return *this;
    }

    CollapseProps& destroyOnHidden(Prop<bool> value = true) {
        destroy_hidden_ = std::move(value);
        return *this;
    }

    CollapseProps& animated(Prop<bool> value = true) {
        animated_ = std::move(value);
        return *this;
    }

    CollapseProps& collapsible(Prop<CollapseCollapsible> value) {
        collapsible_ = std::move(value);
        return *this;
    }

    CollapseProps& expandIconPlacement(Prop<CollapseIconPlacement> value) {
        icon_placement_ = std::move(value);
        return *this;
    }

    CollapseProps& expandIcon(CollapseExpandIcon value) {
        expand_icon_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::ConfigDefaults;
    friend class detail::CollapseComponentHost;
    std::optional<Prop<CollapseKeys>> active_keys_;
    std::optional<CollapseKeys> default_keys_;
    Prop<bool> accordion_{false};
    Prop<ControlSize> size_{ControlSize::Middle};
    bool explicit_size_{};
    std::function<void(CollapseKeys)> on_change_;
    Prop<bool> bordered_{true};
    Prop<bool> ghost_{false};
    Prop<bool> rtl_{false};
    bool explicit_rtl_{};
    Prop<bool> visible_{true};
    Prop<bool> lazy_{false};
    Prop<bool> destroy_hidden_{false};
    Prop<bool> animated_{true};
    Prop<CollapseCollapsible> collapsible_{CollapseCollapsible::Header};
    Prop<CollapseIconPlacement> icon_placement_{CollapseIconPlacement::Start};
    CollapseExpandIcon expand_icon_;
    LayoutStyle layout_;
};

void Collapse(CollapseProps props, std::vector<CollapsePanel> panels);
void Collapse(CollapseProps props, Prop<std::vector<CollapsePanel>> panels);
} // namespace ryn
