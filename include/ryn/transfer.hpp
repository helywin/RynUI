#pragma once
#include <ryn/checkbox.hpp>
#include <ryn/input.hpp>
#include <ryn/pagination.hpp>
#include <array>

namespace ryn {
namespace detail {
class TransferComponentHost;
}

using TransferKey = std::variant<String, double>;
using TransferKeys = std::vector<TransferKey>;
enum class TransferDirection { Left, Right };
enum class TransferSelection { All, Current, Invert, Clear };

struct TransferItemSlot final {};

using TransferItemContent = SlotContent<TransferItemSlot>;

struct TransferItem final {
    TransferKey key{String{}};
    String title;
    String description;
    bool disabled{};
    std::optional<TransferItemContent> content;

    friend bool operator==(const TransferItem& a, const TransferItem& b) {
        return a.key == b.key && a.title == b.title && a.description == b.description && a.disabled == b.disabled &&
               a.content.has_value() == b.content.has_value();
    }
};

struct TransferChangeInfo final {
    TransferKeys target_keys;
    TransferDirection direction;
    TransferKeys moved_keys;
};

struct TransferSelectInfo final {
    TransferKeys source_selected_keys;
    TransferKeys target_selected_keys;
};

struct TransferScrollInfo final {
    TransferDirection direction;
    float offset;
    float delta;
};

struct TransferPagination final {
    bool enabled{};
    std::size_t page_size{10};
    bool show_size_changer{};
    friend bool operator==(const TransferPagination&, const TransferPagination&) = default;
};

struct TransferListContext final {
    TransferDirection direction;
    Prop<std::vector<TransferItem>> filtered_items;
    Prop<TransferKeys> selected_keys;
    Prop<bool> disabled;
    std::function<void(TransferKey, bool)> on_item_select;
    std::function<void(TransferKeys, bool)> on_item_select_all;
};

struct TransferListSlot final {};

using TransferListContent = SlotContent<TransferListSlot>;

struct TransferFooterSlot final {};

using TransferFooter = SlotContent<TransferFooterSlot>;

struct TransferTitleSlot final {};

using TransferTitle = SlotContent<TransferTitleSlot>;

class TransferProps final {
public:
    TransferProps& dataSource(Prop<std::vector<TransferItem>> value) {
        data_ = std::move(value);
        return *this;
    }

    TransferProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    TransferProps& readOnly(Prop<bool> value) {
        read_only_ = std::move(value);
        return *this;
    }

    TransferProps& visible(Prop<bool> value) {
        visible_ = std::move(value);
        return *this;
    }

    TransferProps& showSearch(Prop<bool> value) {
        show_search_ = std::move(value);
        return *this;
    }

    TransferProps& showSelectAll(Prop<bool> value) {
        select_all_ = std::move(value);
        return *this;
    }

    TransferProps& oneWay(Prop<bool> value) {
        one_way_ = std::move(value);
        return *this;
    }

    TransferProps& loading(Prop<bool> value) {
        loading_ = std::move(value);
        return *this;
    }

    TransferProps& pagination(Prop<TransferPagination> value) {
        pagination_ = std::move(value);
        return *this;
    }

    TransferProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    TransferProps& status(Prop<InputStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    TransferProps& direction(Prop<TextDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    TransferProps& listHeight(Prop<float> value) {
        height_ = std::move(value);
        return *this;
    }

    TransferProps& listWidth(Prop<float> value) {
        width_ = std::move(value);
        return *this;
    }

    TransferProps& searchPlaceholder(Prop<String> value) {
        search_placeholder_ = std::move(value);
        return *this;
    }

    TransferProps& notFoundContent(Prop<String> value) {
        empty_ = std::move(value);
        return *this;
    }

    TransferProps& targetKeys(Prop<TransferKeys> value) {
        target_ = std::move(value);
        return *this;
    }

    TransferProps& defaultTargetKeys(TransferKeys value) {
        default_target_ = std::move(value);
        return *this;
    }

    TransferProps& selectedKeys(Prop<TransferKeys> value) {
        selected_ = std::move(value);
        return *this;
    }

    TransferProps& defaultSelectedKeys(TransferKeys value) {
        default_selected_ = std::move(value);
        return *this;
    }

    TransferProps& titles(std::array<String, 2> value) {
        titles_ = std::move(value);
        return *this;
    }

    TransferProps& title(TransferDirection side, TransferTitle value) {
        title_slots_[side == TransferDirection::Right] = std::move(value);
        return *this;
    }

    TransferProps& footer(TransferDirection side, TransferFooter value) {
        footers_[side == TransferDirection::Right] = std::move(value);
        return *this;
    }

    TransferProps& renderList(std::function<TransferListContent(TransferListContext)> value) {
        list_ = std::move(value);
        return *this;
    }

    TransferProps& filterOption(std::function<bool(String, const TransferItem&, TransferDirection)> value) {
        filter_ = std::move(value);
        return *this;
    }

    TransferProps& render(std::function<String(const TransferItem&)> value) {
        render_ = std::move(value);
        return *this;
    }

    TransferProps& onChange(std::function<void(TransferChangeInfo)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    TransferProps& onSelectChange(std::function<void(TransferSelectInfo)> value) {
        on_select_ = std::move(value);
        return *this;
    }

    TransferProps& onSearch(std::function<void(TransferDirection, String)> value) {
        on_search_ = std::move(value);
        return *this;
    }

    TransferProps& onScroll(std::function<void(TransferScrollInfo)> value) {
        on_scroll_ = std::move(value);
        return *this;
    }

    TransferProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TransferComponentHost;
    Prop<std::vector<TransferItem>> data_{std::vector<TransferItem>{}};
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    Prop<bool> visible_{true};
    Prop<bool> show_search_{false};
    Prop<bool> select_all_{true};
    Prop<bool> one_way_{false};
    Prop<bool> loading_{false};
    Prop<TransferPagination> pagination_{TransferPagination{}};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<InputStatus> status_{InputStatus::Default};
    Prop<TextDirection> direction_{TextDirection::LeftToRight};
    Prop<float> height_{200};
    Prop<float> width_{220};
    Prop<String> search_placeholder_{String{u8"Search here"}};
    Prop<String> empty_{String{u8"No data"}};
    std::optional<Prop<TransferKeys>> target_;
    std::optional<TransferKeys> default_target_;
    std::optional<Prop<TransferKeys>> selected_;
    std::optional<TransferKeys> default_selected_;
    std::array<String, 2> titles_{String{u8"Source"}, String{u8"Target"}};
    std::array<std::optional<TransferTitle>, 2> title_slots_;
    std::array<std::optional<TransferFooter>, 2> footers_;
    std::function<TransferListContent(TransferListContext)> list_;
    std::function<bool(String, const TransferItem&, TransferDirection)> filter_;
    std::function<String(const TransferItem&)> render_;
    std::function<void(TransferChangeInfo)> on_change_;
    std::function<void(TransferSelectInfo)> on_select_;
    std::function<void(TransferDirection, String)> on_search_;
    std::function<void(TransferScrollInfo)> on_scroll_;
    LayoutStyle layout_;
};

void Transfer(TransferProps props);
} // namespace ryn
