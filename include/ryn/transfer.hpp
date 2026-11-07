#pragma once
#include <ryn/component_locales.hpp>
#include <ryn/checkbox.hpp>
#include <ryn/input.hpp>
#include <ryn/pagination.hpp>
#include <array>

namespace ryn {
namespace detail {
struct ConfigDefaults;
}

namespace detail {
class TransferComponentHost;
}

using TransferKey = std::variant<String, double>;
using TransferKeys = std::vector<TransferKey>;
enum class TransferDirection { Left, Right };
enum class TransferSelection { All, Current, Invert, Clear, RemoveAll, RemoveCurrent };

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
    bool simple{true};
    bool show_less_items{};
    std::vector<std::size_t> page_size_options{10, 20, 50, 100};
    bool controlled_page_size{};
    friend bool operator==(const TransferPagination&, const TransferPagination&) = default;
};

struct TransferSearchOptions final {
    std::optional<String> placeholder;
    std::array<String, 2> default_values;
};

struct TransferSelectAllInfo final {
    TransferDirection direction;
    std::size_t selected_count{};
    std::size_t total_count{};
};

struct TransferActionContext final {
    TransferDirection direction;
    Prop<bool> disabled;
    Prop<String> label;
    Prop<TextDirection> text_direction;
    std::function<void()> activate;
};

struct TransferActionSlot final {};

using TransferAction = SlotContent<TransferActionSlot>;

struct TransferSelectAllContext final {
    TransferDirection direction;
    Prop<std::size_t> selected_count;
    Prop<std::size_t> total_count;
    Prop<String> label;
};

struct TransferSelectAllSlot final {};

using TransferSelectAllLabel = SlotContent<TransferSelectAllSlot>;

struct TransferPageInfo final {
    TransferDirection direction;
    std::size_t page{};
    std::size_t page_size{};
};

struct TransferListContext final {
    TransferDirection direction;
    Prop<std::vector<TransferItem>> filtered_items;
    Prop<TransferKeys> selected_keys;
    Prop<bool> disabled;
    std::function<void(TransferKey, bool)> on_item_select;
    std::function<void(TransferKeys, bool)> on_item_select_all;
    Prop<String> query;
    Prop<TransferLocale> locale;
    Prop<TextDirection> text_direction;
    Prop<bool> show_remove{false};
    std::function<void(TransferKeys)> on_item_remove;
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
        explicit_disabled_ = true;
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

    TransferProps& showSearch(TransferSearchOptions value) {
        show_search_ = true;
        if (value.placeholder) {
            search_placeholder_ = std::move(*value.placeholder);
        }
        search_default_ = std::move(value.default_values);
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

    TransferProps& pagination(bool enabled) {
        pagination_ = TransferPagination{enabled};
        return *this;
    }

    TransferProps& onPageChange(std::function<void(TransferPageInfo)> value) {
        on_page_ = std::move(value);
        return *this;
    }

    TransferProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        explicit_size_ = true;
        return *this;
    }

    TransferProps& status(Prop<InputStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    TransferProps& direction(Prop<TextDirection> value) {
        direction_ = std::move(value);
        explicit_direction_ = true;
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

    TransferProps& titles(Prop<std::array<String, 2>> value) {
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

    TransferProps& locale(Prop<TransferLocale> value) {
        locale_ = std::move(value);
        return *this;
    }

    TransferProps& rowSpacing(Prop<float> value) {
        row_spacing_ = std::move(value);
        return *this;
    }

    TransferProps& operations(Prop<std::array<String, 2>> value) {
        operations_ = std::move(value);
        return *this;
    }

    TransferProps& searchDefaultValue(Prop<std::array<String, 2>> value) {
        search_default_ = std::move(value);
        return *this;
    }

    TransferProps& action(TransferDirection direction, std::function<TransferAction(TransferActionContext)> value) {
        action_slots_[direction == TransferDirection::Right] = std::move(value);
        return *this;
    }

    TransferProps& actions(Prop<std::array<String, 2>> value) {
        return operations(std::move(value));
    }

    TransferProps& actions(std::array<std::function<TransferAction(TransferActionContext)>, 2> value) {
        action_slots_[1] = std::move(value[0]);
        action_slots_[0] = std::move(value[1]);
        return *this;
    }

    TransferProps& additionalActions(std::vector<TransferAction> value) {
        additional_actions_ = std::move(value);
        return *this;
    }

    TransferProps& selectionsIcon(TransferTitle value) {
        selections_icon_ = std::move(value);
        return *this;
    }

    TransferProps& selectAllLabel(TransferDirection direction,
                                  std::function<TransferSelectAllLabel(TransferSelectAllContext)> value) {
        select_all_slots_[direction == TransferDirection::Right] = std::move(value);
        return *this;
    }

    TransferProps& selectAllLabels(std::function<String(TransferSelectAllInfo)> value) {
        select_all_label_ = std::move(value);
        return *this;
    }

    TransferProps& footer(std::function<TransferFooter(TransferListContext)> value) {
        footer_factory_ = std::move(value);
        return *this;
    }

    TransferProps& rowKey(std::function<TransferKey(const TransferItem&)> value) {
        row_key_ = std::move(value);
        return *this;
    }

    TransferProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::ConfigDefaults;
    friend class detail::TransferComponentHost;
    Prop<std::vector<TransferItem>> data_{std::vector<TransferItem>{}};
    Prop<bool> disabled_{false};
    bool explicit_disabled_{};
    Prop<bool> read_only_{false};
    Prop<bool> visible_{true};
    Prop<bool> show_search_{false};
    Prop<bool> select_all_{true};
    Prop<bool> one_way_{false};
    Prop<bool> loading_{false};
    Prop<TransferPagination> pagination_{TransferPagination{}};
    Prop<ControlSize> size_{ControlSize::Middle};
    bool explicit_size_{};
    Prop<InputStatus> status_{InputStatus::Default};
    Prop<TextDirection> direction_{TextDirection::LeftToRight};
    bool explicit_direction_{};
    std::optional<Prop<float>> height_;
    std::optional<Prop<float>> width_;
    std::optional<Prop<String>> search_placeholder_;
    std::optional<Prop<String>> empty_;
    std::optional<Prop<TransferKeys>> target_;
    std::optional<TransferKeys> default_target_;
    std::optional<Prop<TransferKeys>> selected_;
    std::optional<TransferKeys> default_selected_;
    std::optional<Prop<std::array<String, 2>>> titles_;
    std::array<std::optional<TransferTitle>, 2> title_slots_;
    std::array<std::optional<TransferFooter>, 2> footers_;
    std::function<TransferListContent(TransferListContext)> list_;
    std::function<bool(String, const TransferItem&, TransferDirection)> filter_;
    std::function<String(const TransferItem&)> render_;
    std::function<void(TransferChangeInfo)> on_change_;
    std::function<void(TransferSelectInfo)> on_select_;
    std::function<void(TransferDirection, String)> on_search_;
    std::function<void(TransferScrollInfo)> on_scroll_;
    std::function<void(TransferPageInfo)> on_page_;
    std::optional<Prop<TransferLocale>> locale_;
    Prop<float> row_spacing_{0};
    std::optional<Prop<std::array<String, 2>>> operations_;
    Prop<std::array<String, 2>> search_default_{std::array<String, 2>{String{}, String{}}};
    std::array<std::function<TransferAction(TransferActionContext)>, 2> action_slots_;
    std::vector<TransferAction> additional_actions_;
    std::array<std::function<TransferSelectAllLabel(TransferSelectAllContext)>, 2> select_all_slots_;
    std::function<String(TransferSelectAllInfo)> select_all_label_;
    std::optional<TransferTitle> selections_icon_;
    std::function<TransferFooter(TransferListContext)> footer_factory_;
    std::function<TransferKey(const TransferItem&)> row_key_;
    LayoutStyle layout_;
};

void Transfer(TransferProps props);
} // namespace ryn
