#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <optional>
#include <cstdint>
#include <ryn/breakpoints.hpp>
#include <vector>

namespace ryn {
namespace detail {
class DescriptionsComponentHost;
}

struct DescriptionsLocale final {
    String separator{u8":"};
    friend bool operator==(const DescriptionsLocale&, const DescriptionsLocale&) = default;
};

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

    DescriptionsItem& span(Prop<Responsive<int>> value) {
        responsive_span_ = std::move(value);
        return *this;
    }

    DescriptionsItem& fill(Prop<bool> value = true) {
        fill_ = std::move(value);
        return *this;
    }

    DescriptionsItem& labelRevision(std::uint64_t value) {
        label_revision_ = value;
        return *this;
    }

    DescriptionsItem& contentRevision(std::uint64_t value) {
        content_revision_ = value;
        return *this;
    }

private:
    friend class detail::DescriptionsComponentHost;
    String key_;
    DescriptionsLabel label_;
    DescriptionsContent content_;
    Prop<int> span_{1};
    std::optional<Prop<Responsive<int>>> responsive_span_;
    Prop<bool> fill_{false};
    std::uint64_t label_revision_{};
    std::uint64_t content_revision_{};
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

    DescriptionsProps& column(Prop<Responsive<int>> value) {
        responsive_column_ = std::move(value);
        return *this;
    }

    DescriptionsProps& breakpoints(Prop<Breakpoints> value) {
        breakpoints_ = std::move(value);
        return *this;
    }

    DescriptionsProps& locale(Prop<DescriptionsLocale> value) {
        locale_ = std::move(value);
        return *this;
    }

    DescriptionsProps& rtl(Prop<bool> value = true) {
        rtl_ = std::move(value);
        return *this;
    }

    DescriptionsProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    DescriptionsProps& titleContent(Prop<DescriptionsTitle> value) {
        dynamic_title_ = std::move(value);
        return *this;
    }

    DescriptionsProps& extraContent(Prop<DescriptionsExtra> value) {
        dynamic_extra_ = std::move(value);
        return *this;
    }

private:
    friend class detail::DescriptionsComponentHost;
    Prop<int> column_{3};
    std::optional<Prop<Responsive<int>>> responsive_column_;
    Prop<Breakpoints> breakpoints_{Breakpoints{}};
    Prop<DescriptionsLocale> locale_{DescriptionsLocale{}};
    Prop<bool> rtl_{false};
    Prop<bool> visible_{true};
    std::optional<Prop<DescriptionsTitle>> dynamic_title_;
    std::optional<Prop<DescriptionsExtra>> dynamic_extra_;
    Prop<bool> bordered_{false};
    Prop<DescriptionsLabelPlacement> placement_{DescriptionsLabelPlacement::Horizontal};
    Prop<DescriptionsSize> size_{DescriptionsSize::Default};
    Prop<bool> colon_{true};
    std::optional<DescriptionsTitle> title_;
    std::optional<DescriptionsExtra> extra_;
    LayoutStyle layout_;
};

void Descriptions(DescriptionsProps props, std::vector<DescriptionsItem> items);
void Descriptions(DescriptionsProps props, Prop<std::vector<DescriptionsItem>> items);
} // namespace ryn
