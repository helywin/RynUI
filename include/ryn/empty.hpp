#pragma once
#include <ryn/component.hpp>
#include <ryn/component_locales.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <ryn/image.hpp>
#include <optional>

namespace ryn {
namespace detail {
class EmptyResultComponentHost;
}

enum class EmptyImage { Default, Compact, None };

struct EmptyImageSlot final {};

struct EmptyExtraSlot final {};

struct EmptyDescriptionSlot final {};

using EmptyImageContent = SlotContent<EmptyImageSlot>;
using EmptyExtra = SlotContent<EmptyExtraSlot>;
using EmptyDescription = SlotContent<EmptyDescriptionSlot>;

struct EmptySlots final {
    std::optional<EmptyImageContent> image;
    std::optional<EmptyExtra> extra;
    std::optional<EmptyDescription> description;
};

// Share unchanged regions across updates; replacing one identity rebuilds only that region.
struct EmptyDynamicContent final {
    std::shared_ptr<const EmptyImageContent> image;
    std::shared_ptr<const EmptyExtra> extra;
    std::shared_ptr<const EmptyDescription> description;
    friend bool operator==(const EmptyDynamicContent&, const EmptyDynamicContent&) = default;
};

class EmptyProps final {
public:
    EmptyProps& locale(Prop<CommonLocale> value) {
        locale_ = std::move(value);
        return *this;
    }

    EmptyProps& illustration(Prop<bool> value = true) {
        illustration_ = std::move(value);
        return *this;
    }

    EmptyProps& source(Prop<ImageSource> value) {
        source_ = std::move(value);
        return *this;
    }

    EmptyProps& dynamicContent(Prop<EmptyDynamicContent> value) {
        dynamic_ = std::move(value);
        return *this;
    }

    EmptyProps& image(Prop<EmptyImage> value) {
        image_ = std::move(value);
        return *this;
    }

    EmptyProps& description(Prop<String> value) {
        description_ = std::move(value);
        return *this;
    }

    template <std::size_t N> EmptyProps& description(const char8_t (&value)[N]) {
        return description(String{value});
    }

    EmptyProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::EmptyResultComponentHost;
    Prop<EmptyImage> image_{EmptyImage::Default};
    std::optional<Prop<String>> description_;
    std::optional<Prop<CommonLocale>> locale_;
    std::optional<Prop<ImageSource>> source_;
    std::optional<Prop<bool>> illustration_;
    std::optional<Prop<EmptyDynamicContent>> dynamic_;
    LayoutStyle layout_;
};

void Empty(EmptyProps props = {}, EmptySlots slots = {});
} // namespace ryn
