#pragma once
#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/text.hpp>
#include <functional>
#include <optional>
#include <vector>

namespace ryn {
namespace detail {
class PaginationComponentHost;
}

enum class PaginationAlign { Start, Center, End };
enum class PaginationItemType { Page, Previous, Next, JumpPrevious, JumpNext };

struct PaginationRange final {
    std::size_t first{};
    std::size_t last{};
};

// Invoked once per retained button. Read these bindings in the mounted slot.
struct PaginationItemContext final {
    Binding<std::size_t> page;
    Binding<PaginationItemType> type;
    Binding<String> label;
    Binding<bool> active;
};

struct PaginationLocale final {
    String items_per_page{u8" / page"};
    String page_separator{u8"/ "};
    String jump_placeholder{u8"Go to"};
    friend bool operator==(const PaginationLocale&, const PaginationLocale&) = default;
};

class PaginationProps final {
public:
    PaginationProps& locale(Prop<PaginationLocale> value) {
        locale_ = std::move(value);
        return *this;
    }

    PaginationProps& direction(Prop<TextDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    PaginationProps& showLessItems(Prop<bool> value) {
        less_ = std::move(value);
        return *this;
    }

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

    PaginationProps& defaultPageSize(std::size_t value) {
        default_page_size_ = value;
        return *this;
    }

    PaginationProps& showSizeChanger(Prop<bool> value = true) {
        show_size_changer_ = std::move(value);
        return *this;
    }

    PaginationProps& pageSizeOptions(Prop<std::vector<std::size_t>> value) {
        page_size_options_ = std::move(value);
        return *this;
    }

    PaginationProps& showQuickJumper(Prop<bool> value = true) {
        quick_ = std::move(value);
        return *this;
    }

    PaginationProps& simple(Prop<bool> value = true) {
        simple_ = std::move(value);
        return *this;
    }

    PaginationProps& readOnly(Prop<bool> value = true) {
        read_only_ = std::move(value);
        return *this;
    }

    PaginationProps& responsive(Prop<bool> value = true) {
        responsive_ = std::move(value);
        return *this;
    }

    PaginationProps& align(Prop<PaginationAlign> value) {
        align_ = std::move(value);
        return *this;
    }

    PaginationProps& showTotal(std::function<String(std::size_t, PaginationRange)> value) {
        total_render_ = std::move(value);
        return *this;
    }

    PaginationProps& itemRender(std::function<void(PaginationItemContext)> value) {
        item_render_ = std::move(value);
        return *this;
    }

    PaginationProps& onShowSizeChange(std::function<void(std::size_t, std::size_t)> value) {
        on_size_change_ = std::move(value);
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
    Prop<PaginationLocale> locale_{PaginationLocale{}};
    Prop<TextDirection> direction_{TextDirection::LeftToRight};
    Prop<bool> less_{false};
    Prop<std::size_t> total_{0};
    std::optional<Prop<std::size_t>> current_;
    std::optional<std::size_t> default_current_;
    std::optional<Prop<std::size_t>> page_size_;
    std::optional<std::size_t> default_page_size_;
    std::optional<Prop<bool>> show_size_changer_;
    Prop<std::vector<std::size_t>> page_size_options_{std::vector<std::size_t>{10, 20, 50, 100}};
    Prop<bool> quick_{false};
    Prop<bool> simple_{false};
    Prop<bool> read_only_{false};
    Prop<bool> responsive_{false};
    Prop<PaginationAlign> align_{PaginationAlign::Start};
    std::function<String(std::size_t, PaginationRange)> total_render_;
    std::function<void(PaginationItemContext)> item_render_;
    std::function<void(std::size_t, std::size_t)> on_size_change_;
    Prop<bool> disabled_{false};
    Prop<bool> hide_single_{false};
    Prop<ControlSize> size_{ControlSize::Middle};
    std::function<void(std::size_t, std::size_t)> on_change_;
    LayoutStyle layout_;
};

void Pagination(PaginationProps props = {});
} // namespace ryn
