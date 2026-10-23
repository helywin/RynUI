#pragma once
#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/layout_style.hpp>
#include <functional>
#include <optional>

namespace ryn {
namespace detail {
class PaginationComponentHost;
}

class PaginationProps final {
public:
    PaginationProps& total(Prop<std::size_t> value) {
        total_ = std::move(value);
        return *this;
    }

    PaginationProps& current(Prop<std::size_t> value) {
        current_ = std::move(value);
        return *this;
    }

    PaginationProps& defaultCurrent(std::size_t value) {
        default_current_ = value;
        return *this;
    }

    PaginationProps& pageSize(Prop<std::size_t> value) {
        page_size_ = std::move(value);
        return *this;
    }

    PaginationProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    PaginationProps& hideOnSinglePage(Prop<bool> value = true) {
        hide_single_ = std::move(value);
        return *this;
    }

    PaginationProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    PaginationProps& onChange(std::function<void(std::size_t, std::size_t)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    PaginationProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::PaginationComponentHost;
    Prop<std::size_t> total_{0};
    std::optional<Prop<std::size_t>> current_;
    std::optional<std::size_t> default_current_;
    Prop<std::size_t> page_size_{10};
    Prop<bool> disabled_{false};
    Prop<bool> hide_single_{false};
    Prop<ControlSize> size_{ControlSize::Middle};
    std::function<void(std::size_t, std::size_t)> on_change_;
    LayoutStyle layout_;
};

void Pagination(PaginationProps props = {});
} // namespace ryn
