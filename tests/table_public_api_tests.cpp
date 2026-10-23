#include <ryn/table.hpp>
#include <ryn/pagination.hpp>
#include <ryn/text.hpp>
#include <type_traits>

struct BusinessRow final {
    ryn::String id;
    ryn::String name;
    double price{};
    friend bool operator==(const BusinessRow&, const BusinessRow&) = default;
};

static_assert(std::is_copy_constructible_v<ryn::TableProps<BusinessRow>>);
static_assert(std::is_same_v<decltype(std::declval<ryn::TableCell<BusinessRow>>().row()), ryn::Prop<BusinessRow>>);

int main() {
    ryn::Signal<std::vector<BusinessRow>> rows{std::vector<BusinessRow>{}};
    ryn::Signal<std::vector<ryn::String>> selected{std::vector<ryn::String>{}};
    ryn::Signal<std::size_t> page{1};
    auto props =
        ryn::TableProps<BusinessRow>{}
            .dataSource(rows)
            .rowKey([](const BusinessRow& row) { return row.id; })
            .columns({ryn::TableColumn<BusinessRow>{
                u8"name", u8"Name",
                [](ryn::TableCell<BusinessRow> cell) {
                    ryn::Text(ryn::TextProps{}.content(ryn::bind([cell] { return cell.value().name; })));
                }}.width(180.0F)
                          .sorter([](const auto& a, const auto& b) { return a.name.bytes() < b.name.bytes(); })})
            .rowSelection({selected, [selected](auto keys) { selected.set(std::move(keys)); }, {}})
            .current(page)
            .pageSize(std::size_t{10})
            .scrollY(200.0F)
            .bordered();
    static_cast<void>(props);
    return 0;
}
