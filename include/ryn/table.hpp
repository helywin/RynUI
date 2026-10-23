#pragma once
#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace ryn {
enum class TableAlign { Left, Center, Right };

enum class TableSortOrder { None, Ascend, Descend };

struct TableSort final {
    String column_key;
    TableSortOrder order{TableSortOrder::None};
    friend bool operator==(const TableSort&, const TableSort&) = default;
};

struct TableHeaderSlot final {};

struct TableEmptySlot final {};

using TableHeader = SlotContent<TableHeaderSlot>;
using TableEmpty = SlotContent<TableEmptySlot>;

template <typename Row> class TableCell final {
public:
    TableCell(String key, Binding<Row> row) : key_(std::move(key)), row_(std::move(row)) {}

    const String& key() const noexcept {
        return key_;
    }

    Row value() const {
        return row_.get();
    }

    Prop<Row> row() const {
        return row_;
    }

private:
    String key_;
    Binding<Row> row_;
};

template <typename Row> using TableCellContent = std::function<void(TableCell<Row>)>;

namespace detail {
class TableComponentHost;

struct TablePropBridge final {
    template <typename T> static T read(const Prop<T>& prop) {
        if (const auto* value = std::get_if<typename Prop<T>::StaticValue>(&prop.source_)) {
            if constexpr (Prop<T>::indirect_static_value) {
                return **value;
            } else {
                return *value;
            }
        }
        return std::get<Binding<T>>(prop.source_).get();
    }
};

using TableValue = std::shared_ptr<const void>;

struct TableRecord final {
    String key;
    TableValue value;
    bool selection_disabled{};
    friend bool operator==(const TableRecord&, const TableRecord&) = default;
};

struct TableColumnSchema final {
    String key;
    Prop<String> title{String{}};
    std::optional<TableHeader> header;
    std::function<void(String, Binding<TableValue>)> cell;
    std::function<bool(const TableValue&, const TableValue&)> less;
    Prop<float> width{0.0F};
    Prop<TableAlign> align{TableAlign::Left};
    bool sortable{};
};

struct TableOptions final {
    Prop<TableSort> sort{TableSort{}};
    std::function<void(TableSort)> on_sort;
    std::optional<Prop<std::vector<String>>> selected;
    std::function<void(std::vector<String>)> on_selection;
    std::optional<Prop<std::size_t>> current;
    Prop<std::size_t> page_size{10};
    Prop<float> viewport_height{240.0F};
    Prop<ControlSize> size{ControlSize::Large};
    Prop<bool> bordered{false};
    Prop<bool> loading{false};
    Prop<bool> show_header{true};
    LayoutStyle layout;
};

void mount_table(TableOptions, Prop<std::vector<TableRecord>>, std::vector<TableColumnSchema>,
                 std::optional<TableEmpty>);
} // namespace detail

template <typename Row> class TableColumn final {
public:
    TableColumn(String key, String title, TableCellContent<Row> cell)
        : key_(std::move(key)), title_(std::move(title)), cell_(std::move(cell)) {}

    TableColumn& title(Prop<String> value) {
        title_ = std::move(value);
        return *this;
    }

    TableColumn& header(TableHeader value) {
        header_ = std::move(value);
        return *this;
    }

    // Width is in logical pixels; zero distributes the remaining space.
    TableColumn& width(Prop<float> value) {
        width_ = std::move(value);
        return *this;
    }

    TableColumn& align(Prop<TableAlign> value) {
        align_ = std::move(value);
        return *this;
    }

    TableColumn& sortable(bool value = true) {
        sortable_ = value;
        return *this;
    }

    TableColumn& sorter(std::function<bool(const Row&, const Row&)> value) {
        less_ = std::move(value);
        sortable_ = true;
        return *this;
    }

private:
    template <typename> friend class TableProps;
    String key_;
    Prop<String> title_;
    TableCellContent<Row> cell_;
    std::optional<TableHeader> header_;
    Prop<float> width_{0.0F};
    Prop<TableAlign> align_{TableAlign::Left};
    std::function<bool(const Row&, const Row&)> less_;
    bool sortable_{};
};

template <typename Row> struct TableRowSelection final {
    Prop<std::vector<String>> selectedRowKeys{std::vector<String>{}};
    std::function<void(std::vector<String>)> onChange;
    std::function<bool(const Row&)> disabled;
};

template <typename Row> class TableProps final {
public:
    TableProps& dataSource(Prop<std::vector<Row>> value) {
        source_ = std::move(value);
        return *this;
    }

    TableProps& rowKey(std::function<String(const Row&)> value) {
        key_ = std::move(value);
        return *this;
    }

    TableProps& columns(std::vector<TableColumn<Row>> value) {
        columns_ = std::move(value);
        return *this;
    }

    TableProps& sort(Prop<TableSort> value) {
        options_.sort = std::move(value);
        return *this;
    }

    TableProps& onSortChange(std::function<void(TableSort)> value) {
        options_.on_sort = std::move(value);
        return *this;
    }

    TableProps& rowSelection(TableRowSelection<Row> value) {
        options_.selected = std::move(value.selectedRowKeys);
        options_.on_selection = std::move(value.onChange);
        disabled_ = std::move(value.disabled);
        return *this;
    }

    TableProps& current(Prop<std::size_t> value) {
        options_.current = std::move(value);
        return *this;
    }

    TableProps& pageSize(Prop<std::size_t> value) {
        options_.page_size = std::move(value);
        return *this;
    }

    TableProps& scrollY(Prop<float> value) {
        options_.viewport_height = std::move(value);
        return *this;
    }

    TableProps& size(Prop<ControlSize> value) {
        options_.size = std::move(value);
        return *this;
    }

    TableProps& bordered(Prop<bool> value = true) {
        options_.bordered = std::move(value);
        return *this;
    }

    TableProps& loading(Prop<bool> value = true) {
        options_.loading = std::move(value);
        return *this;
    }

    TableProps& showHeader(Prop<bool> value) {
        options_.show_header = std::move(value);
        return *this;
    }

    TableProps& layout(LayoutStyle value) {
        options_.layout = std::move(value);
        return *this;
    }

    void mount(std::optional<TableEmpty> empty) const {
        if (!key_) {
            throw std::invalid_argument("Table requires rowKey");
        }
        std::vector<detail::TableColumnSchema> schema;
        for (const auto& column : columns_) {
            detail::TableColumnSchema c;
            c.key = column.key_;
            c.title = column.title_;
            c.header = column.header_;
            if (!column.cell_) {
                throw std::invalid_argument("Table requires cell content");
            }
            c.cell = [cell = column.cell_](String key, Binding<detail::TableValue> value) {
                cell(TableCell<Row>{std::move(key),
                                    bind([value] { return *std::static_pointer_cast<const Row>(value.get()); })});
            };
            if (column.less_) {
                c.less = [less = column.less_](const detail::TableValue& a, const detail::TableValue& b) {
                    return less(*std::static_pointer_cast<const Row>(a), *std::static_pointer_cast<const Row>(b));
                };
            }
            c.width = column.width_;
            c.align = column.align_;
            c.sortable = column.sortable_;
            schema.push_back(std::move(c));
        }
        auto data = bind([source = source_, key = key_, disabled = disabled_] {
            std::vector<detail::TableRecord> result;
            for (auto& row : detail::TablePropBridge::read(source)) {
                auto value = std::make_shared<const Row>(std::move(row));
                result.push_back({key(*value), value, disabled && disabled(*value)});
            }
            return result;
        });
        detail::mount_table(options_, std::move(data), std::move(schema), std::move(empty));
    }

private:
    Prop<std::vector<Row>> source_{std::vector<Row>{}};
    std::function<String(const Row&)> key_;
    std::function<bool(const Row&)> disabled_;
    std::vector<TableColumn<Row>> columns_;
    detail::TableOptions options_;
};

template <typename Row> void Table(TableProps<Row> props, std::optional<TableEmpty> empty = {}) {
    props.mount(std::move(empty));
}
} // namespace ryn
