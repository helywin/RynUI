#include "component/table_component.hpp"
#include "component/selection_component.hpp"
#include "support/navigation_fixture.hpp"
#include <ryn/input.hpp>
#include <ryn/table.hpp>
#include <ryn/text.hpp>

namespace {
using namespace ryn_test::navigation;

struct TableFixture final : ryn_test::navigation::Fixture {
    detail::SelectionComponentHost selection{services};
};

struct Row final {
    String key;
    int value{};
    std::vector<Row> children;
    friend bool operator==(const Row&, const Row&) = default;
};

std::vector<Row> rows(std::size_t count) {
    std::vector<Row> result;
    for (std::size_t i = 0; i < count; ++i) {
        result.push_back({String::from_utf8(std::to_string(i)).value(), static_cast<int>(i), {}});
    }
    return result;
}

void virtual_rows() {
    TableFixture f;
    Signal<std::vector<Row>> data{rows(10000)};
    std::size_t mounts = 0;
    f.services.mount(Content{[&] {
        Table(TableProps<Row>{}
                  .dataSource(data)
                  .rowKey([](const Row& row) { return row.key; })
                  .columns({TableColumn<Row>{u8"edit", u8"Editor",
                                             [&](TableCell<Row> cell) {
                                                 ++mounts;
                                                 Input(InputProps{}.defaultValue(cell.key()));
                                             }}
                                .width(400)
                                .fixed(TableFixed::Start),
                            TableColumn<Row>{u8"value", u8"Value",
                                             [](TableCell<Row> cell) {
                                                 Text(TextProps{}.content(bind([cell] { return cell.key(); })));
                                             }}
                                .width(500)})
                  .scrollY(160)
                  .virtualRows({40, 100, 2}));
    }});
    sync(f, 640);
    const auto id = f.services.table().mounted().front();
    const auto first = f.services.table().snapshot(id);
    check(mounts <= 10 && first.rows.size() <= 10 && first.projected_rows == 10000,
          "virtual Table eagerly constructed the data source");
    const auto original = first.rows.front();
    const auto body = first.body;
    check(f.services.dispatch_scroll({0, -10, body.x + 450, body.y + 40}), "virtual Table did not consume real wheel");
    sync(f);
    const auto after = f.services.table().snapshot(id);
    check(after.scroll_y > first.scroll_y && mounts < 30, "virtual wheel did not realize a bounded window");
    check(f.services.dispatch_scroll({0, 10, body.x + 450, body.y + 40}), "virtual Table reverse wheel failed");
    sync(f);
    const auto restored = f.services.table().snapshot(id);
    const auto row = std::ranges::find_if(restored.rows, [&](const auto& value) { return value.key == original.key; });
    check(row != restored.rows.end() && row->component == original.component && row->cells == original.cells,
          "virtual cache replaced a retained editor");
    const auto scrollbar = restored.scrollbars[1];
    const auto thumb = bounds(f, scrollbar);
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                   thumb.x + thumb.width / 2, thumb.y + thumb.height / 2});
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::move, PointerButton::none, thumb.x + thumb.width / 2, thumb.y + 50});
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, thumb.x + thumb.width / 2, thumb.y + 50});
    sync(f);
    check(f.services.table().snapshot(id).scroll_y > 0, "scrollbar did not route native pointer capture");
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "scrollbar capture leaked");
    f.services.dispose();
    check(f.services.components().component_count() == 0 && f.services.interactions().size() == 0,
          "virtual Table cleanup leaked");
}

void tree_filters() {
    TableFixture f;
    Signal<std::vector<Row>> data{std::vector<Row>{{u8"parent", 1, {{u8"child", 2, {}}}}, {u8"other", 3, {}}}};
    Signal<std::vector<String>> expanded{std::vector<String>{}};
    Signal<std::vector<String>> filtered{std::vector<String>{}};
    bool accept = false;
    int requests = 0;
    int details = 0;
    TableExpandable<Row> expansion;
    expansion.children = [](const Row& row) {
        return row.children;
    };
    expansion.expandedRowKeys = expanded;
    expansion.onExpandedRowsChange = [&](auto value) {
        ++requests;
        if (accept) {
            expanded.set(std::move(value));
        }
    };
    expansion.expandedRowRender = [&](TableCell<Row>) {
        ++details;
        Input(InputProps{}.defaultValue(u8"draft"));
    };
    f.services.mount(Content{[&] {
        Table(TableProps<Row>{}
                  .dataSource(data)
                  .rowKey([](const Row& row) { return row.key; })
                  .columns({TableColumn<Row>{
                      u8"value", u8"Value",
                      [](TableCell<Row> row) { Text(TextProps{}.content(bind([row] { return row.value().key; }))); }}
                                .filters({{u8"even", u8"Even"}})
                                .filteredValue(filtered)
                                .onFilter([](const String&, const Row& row) { return row.value % 2 == 0; })
                                .onFilterChange([filtered](auto keys) { filtered.set(std::move(keys)); })})
                  .expandable(expansion)
                  .scrollY(300));
    }});
    sync(f);
    const auto id = f.services.table().mounted().front();
    check(f.services.table().snapshot(id).visible_keys == std::vector<String>{u8"parent", u8"other"},
          "collapsed tree exposed a child");
    // Native expansion button is the first button below the first row.
    const auto owner = f.services.table().snapshot(id).rows.front().component;
    runtime::ComponentId button;
    for (const auto& b : f.buttons.mounted_buttons()) {
        if (descendant(f, b.component, owner)) {
            button = b.component;
            break;
        }
    }
    click(f, button);
    check(requests == 1 && f.services.table().snapshot(id).expanded_keys.empty(),
          "controlled expansion refusal was ignored");
    accept = true;
    click(f, button);
    check(f.services.table().snapshot(id).visible_keys == std::vector<String>{u8"parent", u8"child", u8"other"} &&
              details > 0,
          "accepted native expansion did not expose tree/detail content");
    const auto before = f.services.table().snapshot(id).rows;
    filtered.set({u8"even"});
    sync(f);
    check(f.services.table().snapshot(id).visible_keys == std::vector<String>{u8"parent", u8"child"},
          "filter did not retain a matching descendant's ancestor path");
    const auto after = f.services.table().snapshot(id).rows;
    check(before[0].component == after[0].component && before[0].cells == after[0].cells,
          "filter replaced retained cells");
    expanded.set({});
    sync(f);
    expanded.set({u8"parent"});
    sync(f);
    check(details == 1, "re-expansion rebuilt a retained rich detail editor");
    f.services.dispose();
    check(f.inputs.mounted_inputs().empty() && f.services.interactions().size() == 0, "tree/filter cleanup leaked");
}

void fixed_clips_and_callbacks() {
    TableFixture f;
    f.services.mount(Content{[&] {
        Table(
            TableProps<Row>{}
                .dataSource(rows(3))
                .rowKey([](const Row& row) { return row.key; })
                .columns(
                    {TableColumn<Row>{u8"start", u8"Start", [](auto) { Input(InputProps{}.defaultValue(u8"fixed")); }}
                         .width(120)
                         .fixed(TableFixed::Start),
                     TableColumn<Row>{u8"middle", u8"Middle",
                                      [](auto) { Input(InputProps{}.defaultValue(u8"scrolling rich content")); }}
                         .width(400),
                     TableColumn<Row>{u8"end", u8"End", [](auto) { Text(u8"End"); }}.width(80).fixed(TableFixed::End)})
                .scrollY(140));
    }});
    sync(f, 320);
    const auto id = f.services.table().mounted().front();
    const auto before = f.services.table().snapshot(id);
    const auto start = bounds(f, before.rows.front().cells.front());
    const auto end = bounds(f, before.rows.front().cells.back());
    check(f.services.dispatch_scroll({4, 0, 180, before.body.y + 10}), "fixed Table horizontal wheel did not move");
    sync(f, 320);
    check(bounds(f, before.rows.front().cells.front()) == start && bounds(f, before.rows.front().cells.back()) == end,
          "fixed columns drifted with the scrollable cells");
    const auto middle_id = before.rows.front().cells[1];
    const auto middle = bounds(f, middle_id);
    const auto middle_clip = f.nodes.content_clip(f.services.components().root(middle_id), {0, 0, 320, 800});
    if (middle.width != 400 || middle_clip.x < start.x + start.width || middle_clip.x + middle_clip.width > end.x) {
        std::cerr << "Table fixed clip: middle=" << middle.x << ',' << middle.width << " clip=" << middle_clip.x << ','
                  << middle_clip.width << " start=" << start.x << ',' << start.width << " end=" << end.x << ','
                  << end.width << '\n';
    }
    check(middle.width == 400 && middle_clip.x >= start.x + start.width && middle_clip.x + middle_clip.width <= end.x,
          "scrolling cell did not crop its actual viewport between fixed columns");
    check(f.services.layout_and_synchronize({320, 800}, {20, 10, 260, 180}), "clipped Table layout failed");
    surfaces_and_hits_inside(f, {20, 10, 260, 180});
    f.services.dispose();
    for (bool throwing : {false, true}) {
        TableFixture callback_fixture;
        TableExpandable<Row> expansion;
        expansion.expandedRowRender = [](auto) {
            Input(InputProps{}.defaultValue(u8"detail"));
        };
        expansion.onExpandedRowsChange = [&](auto) {
            if (throwing) {
                throw std::runtime_error("expected Table callback failure");
            }
            callback_fixture.services.dispose();
        };
        callback_fixture.services.mount(Content{[&] {
            Table(TableProps<Row>{}
                      .dataSource(rows(1))
                      .rowKey([](const Row& row) { return row.key; })
                      .columns({TableColumn<Row>{u8"key", u8"Key", [](auto) { Text(u8"row"); }}})
                      .expandable(expansion));
        }});
        sync(callback_fixture);
        const auto owner = callback_fixture.services.table().mounted().front();
        const auto row = callback_fixture.services.table().snapshot(owner).rows.front().component;
        runtime::ComponentId action;
        for (const auto& button : callback_fixture.buttons.mounted_buttons()) {
            if (descendant(callback_fixture, button.component, row)) {
                action = button.component;
                break;
            }
        }
        bool threw = false;
        try {
            click(callback_fixture, action);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw == throwing && !callback_fixture.services.components().contains(owner),
              "Table callback failure/destruction did not retire its exact owner");
        callback_fixture.services.dispose();
        check(callback_fixture.services.interactions().size() == 0 && callback_fixture.inputs.mounted_inputs().empty(),
              "failed Table callback leaked interactions/editor state");
    }
}
} // namespace

int main() {
    try {
        virtual_rows();
        tree_filters();
        fixed_clips_and_callbacks();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
