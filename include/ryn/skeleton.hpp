#pragma once
#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/layout_style.hpp>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>

namespace ryn {
namespace detail {
class SkeletonComponentHost;
}

class SkeletonWidth final {
public:
    SkeletonWidth() = default;

    [[nodiscard]] static SkeletonWidth fraction(float value) {
        if (!std::isfinite(value) || value < 0 || value > 1) {
            throw std::invalid_argument("Skeleton width fraction must be in [0, 1]");
        }
        SkeletonWidth result;
        result.fraction_ = value;
        return result;
    }

    [[nodiscard]] std::optional<float> fraction() const noexcept {
        return fraction_;
    }

    friend bool operator==(const SkeletonWidth&, const SkeletonWidth&) = default;

private:
    std::optional<float> fraction_;
};

enum class SkeletonAvatarShape { Circle, Square };

enum class SkeletonButtonShape { Default, Circle, Round, Square };

struct SkeletonTitle final {
    bool enabled{true};
    SkeletonWidth width;
    friend bool operator==(const SkeletonTitle&, const SkeletonTitle&) = default;
};

struct SkeletonParagraph final {
    bool enabled{true};
    std::optional<std::size_t> rows;
    SkeletonWidth last_line_width;
    std::vector<SkeletonWidth> widths;
    friend bool operator==(const SkeletonParagraph&, const SkeletonParagraph&) = default;
};

struct SkeletonAvatarOptions final {
    bool enabled{};
    ControlSize size{ControlSize::Large};
    std::optional<SkeletonAvatarShape> shape;
    friend bool operator==(const SkeletonAvatarOptions&, const SkeletonAvatarOptions&) = default;
};

struct SkeletonContentSlot final {};

using SkeletonContent = SlotContent<SkeletonContentSlot>;

class SkeletonProps final {
public:
    SkeletonProps& loading(Prop<bool> value) {
        loading_ = std::move(value);
        return *this;
    }

    SkeletonProps& title(Prop<SkeletonTitle> value) {
        title_ = std::move(value);
        return *this;
    }

    SkeletonProps& title(bool enabled) {
        return title(SkeletonTitle{enabled});
    }

    SkeletonProps& paragraph(Prop<SkeletonParagraph> value) {
        paragraph_ = std::move(value);
        return *this;
    }

    SkeletonProps& paragraph(bool enabled) {
        return paragraph(SkeletonParagraph{enabled});
    }

    SkeletonProps& avatar(Prop<SkeletonAvatarOptions> value) {
        avatar_ = std::move(value);
        return *this;
    }

    SkeletonProps& avatar(bool enabled) {
        return avatar(SkeletonAvatarOptions{enabled});
    }

    SkeletonProps& round(Prop<bool> value) {
        round_ = std::move(value);
        return *this;
    }

    SkeletonProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::SkeletonComponentHost;
    Prop<bool> loading_{true};
    Prop<SkeletonTitle> title_{SkeletonTitle{}};
    Prop<SkeletonParagraph> paragraph_{SkeletonParagraph{}};
    Prop<SkeletonAvatarOptions> avatar_{SkeletonAvatarOptions{}};
    Prop<bool> round_{false};
    LayoutStyle layout_;
};

class SkeletonAvatarProps final {
public:
    SkeletonAvatarProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    SkeletonAvatarProps& shape(Prop<SkeletonAvatarShape> value) {
        shape_ = std::move(value);
        return *this;
    }

    SkeletonAvatarProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::SkeletonComponentHost;
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<SkeletonAvatarShape> shape_{SkeletonAvatarShape::Circle};
    LayoutStyle layout_;
};

class SkeletonButtonProps final {
public:
    SkeletonButtonProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    SkeletonButtonProps& shape(Prop<SkeletonButtonShape> value) {
        shape_ = std::move(value);
        return *this;
    }

    SkeletonButtonProps& block(Prop<bool> value) {
        block_ = std::move(value);
        return *this;
    }

    SkeletonButtonProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::SkeletonComponentHost;
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<SkeletonButtonShape> shape_{SkeletonButtonShape::Default};
    Prop<bool> block_{false};
    LayoutStyle layout_;
};

class SkeletonInputProps final {
public:
    SkeletonInputProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    SkeletonInputProps& block(Prop<bool> value) {
        block_ = std::move(value);
        return *this;
    }

    SkeletonInputProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::SkeletonComponentHost;
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<bool> block_{false};
    LayoutStyle layout_;
};

void Skeleton(SkeletonProps props = {});
void Skeleton(SkeletonProps props, SkeletonContent content);
void SkeletonAvatar(SkeletonAvatarProps props = {});
void SkeletonButton(SkeletonButtonProps props = {});
void SkeletonInput(SkeletonInputProps props = {});
} // namespace ryn
