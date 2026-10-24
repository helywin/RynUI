#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <optional>
#include <vector>

namespace ryn {
namespace detail {
class DescriptionsComponentHost;
}

enum class DescriptionsSize { Default, Middle, Small };

enum class DescriptionsLabelPlacement { Horizontal, Vertical };

struct DescriptionsLabelSlot final {};

struct DescriptionsContentSlot final {};

struct DescriptionsTitleSlot final {};

struct DescriptionsExtraSlot final {};

using DescriptionsLabel = SlotContent<DescriptionsLabelSlot>;
using DescriptionsContent = SlotContent<DescriptionsContentSlot>;
using DescriptionsTitle = SlotContent<DescriptionsTitleSlot>;
using DescriptionsExtra = SlotContent<DescriptionsExtraSlot>;

class DescriptionsItem final {
public:
    DescriptionsItem(String key, DescriptionsLabel label, DescriptionsContent content)
        : key_(std::move(key)), label_(std::move(label)), content_(std::move(content)) {}

    DescriptionsItem& span(Prop<int> value) {
        span_ = std::move(value);
        return *this;
    }

    DescriptionsItem& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

private:
    friend class detail::DescriptionsComponentHost;
    String key_;
    DescriptionsLabel label_;
    DescriptionsContent content_;
    Prop<int> span_{1};
    Prop<bool> visible_{true};
};

class DescriptionsProps final {
public:
    DescriptionsProps& column(Prop<int> value) {
        column_ = std::move(value);
        return *this;
    }

    DescriptionsProps& bordered(Prop<bool> value = true) {
        bordered_ = std::move(value);
        return *this;
    }

    DescriptionsProps& labelPlacement(Prop<DescriptionsLabelPlacement> value) {
        placement_ = std::move(value);
        return *this;
    }

    DescriptionsProps& size(Prop<DescriptionsSize> value) {
        size_ = std::move(value);
        return *this;
    }

    DescriptionsProps& colon(Prop<bool> value) {
        colon_ = std::move(value);
        return *this;
    }

    DescriptionsProps& title(DescriptionsTitle value) {
        title_ = std::move(value);
        return *this;
    }

    DescriptionsProps& extra(DescriptionsExtra value) {
        extra_ = std::move(value);
        return *this;
    }

    DescriptionsProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::DescriptionsComponentHost;
    Prop<int> column_{3};
    Prop<bool> bordered_{false};
    Prop<DescriptionsLabelPlacement> placement_{DescriptionsLabelPlacement::Horizontal};
    Prop<DescriptionsSize> size_{DescriptionsSize::Default};
    Prop<bool> colon_{true};
    std::optional<DescriptionsTitle> title_;
    std::optional<DescriptionsExtra> extra_;
    LayoutStyle layout_;
};

void Descriptions(DescriptionsProps props, std::vector<DescriptionsItem> items);
} // namespace ryn
