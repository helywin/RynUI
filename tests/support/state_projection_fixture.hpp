#pragma once
#include "component/selection_component.hpp"
#include "component/table_component.hpp"
#include "component/tree_component.hpp"
#include "support/timeline_fixture.hpp"
#include <functional>
#include <utility>

namespace projection_test {
using namespace ryn;
using namespace ryn::input;
using timeline_test::bounds;
using timeline_test::check;
using timeline_test::descendant;

struct Fixture final : ryn_test::input_component::Fixture {
    detail::SelectionComponentHost selections{services};
};

inline void sync(Fixture& f) {
    check(f.services.layout_and_synchronize({640, 600}, {0, 0, 640, 600}), "projection fixture synchronization failed");
}

inline String key(std::size_t i) {
    return String::from_utf8("row-" + std::to_string(i)).value();
}

inline std::vector<TreeItem> tree_items(std::size_t count) {
    std::vector<TreeItem> items;
    items.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        items.emplace_back(key(i), String{u8"Tree row"});
    }
    return items;
}

inline runtime::ComponentId mount_tree(Fixture& f, std::size_t count) {
    f.services.mount(
        Content{[&] { Tree(TreeProps{}.layout(LayoutStyle{}.width(dp(320)).height(dp(180))), tree_items(count)); }});
    sync(f);
    return f.services.tree().mounted().back();
}

inline void hover(Fixture& f, const detail::TreeRowSnapshot& row) {
    const auto r = bounds(f, row.title);
    const runtime::Point point{r.x + r.width / 2, r.y + r.height / 2};
    check(f.services.hit_test().hit_test(point) == row.interaction, "Tree hover fixture missed the actual row");
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::move, PointerButton::none, point.x, point.y});
}

// Keep the driver usable with the old baseline: comparator/composer checks observe
// real work, while private counters absent there are reported as unavailable.
struct TreeCounts final {
    bool available{};
    std::uint64_t projected{};
    std::uint64_t presented{};
};

template <class Host> TreeCounts tree_counts(const Host& host, runtime::ComponentId id) {
    if constexpr (requires { host.work(id); }) {
        const auto work = host.work(id);
        return {true, work.projected_rows, work.presented_rows};
    } else {
        return {};
    }
}

struct TableCounts final {
    bool available{};
    std::uint64_t copied{};
    std::uint64_t sorts{};
    std::uint64_t visited{};
};

template <class Host> TableCounts table_counts(const Host& host, runtime::ComponentId id) {
    if constexpr (requires { host.work(id); }) {
        const auto work = host.work(id);
        return {true, work.copied_records, work.sort_passes, work.visited_rows};
    } else {
        return {};
    }
}

struct Record final {
    String key;
    int rank{};
    String label{u8"Record"};
    bool disabled{};
    friend bool operator==(const Record&, const Record&) = default;
};

inline std::vector<Record> table_records(std::size_t count) {
    std::vector<Record> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        result.push_back({key(i), static_cast<int>(count - i)});
    }
    return result;
}

struct TableFixture final {
    Fixture f;
    Signal<std::vector<Record>> source;
    Signal<TableSort> sort{TableSort{u8"value", TableSortOrder::Ascend}};
    Signal<std::vector<String>> selected{std::vector<String>{}};
    Signal<std::size_t> page{1};
    Signal<std::size_t> page_size{10};
    Signal<bool> bordered{false};
    std::uint64_t comparisons{};
    std::function<void()> compare_callback;
    std::function<void(TableCell<Record>)> cell_callback;
    runtime::ComponentId id;

    explicit TableFixture(std::size_t count) : source(table_records(count)) {}

    void mount() {
        f.services.mount(Content{[&] {
            Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                     Table(TableProps<Record>{}
                               .dataSource(source)
                               .rowKey([](const Record& value) { return value.key; })
                               .columns({TableColumn<Record>{
                                   u8"value", u8"Value",
                                   [this](TableCell<Record> cell) {
                                       if (cell_callback) {
                                           cell_callback(cell);
                                       } else {
                                           Text(TextProps{}.content(bind([cell] { return cell.value().label; })));
                                       }
                                   }}.sorter([this](const Record& a, const Record& b) {
                                   ++comparisons;
                                   if (compare_callback) {
                                       // One-shot reentry avoids manufacturing an endlessly changing authority.
                                       const auto callback = std::exchange(compare_callback, {});
                                       callback();
                                   }
                                   return a.rank < b.rank;
                               })})
                               .sort(sort)
                               .rowSelection({selected, [this](auto value) { selected.set(std::move(value)); },
                                              [](const Record& value) { return value.disabled; }})
                               .current(page)
                               .pageSize(page_size)
                               .bordered(bordered)
                               .scrollY(180.0F));
                 }});
        }});
        sync(f);
        id = f.services.table().mounted().back();
    }
};
} // namespace projection_test
