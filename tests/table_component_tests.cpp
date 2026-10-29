#include "component/table_component.hpp"
#include "component/pagination_component.hpp"
#include "component/modal_component.hpp"
#include "component/selection_component.hpp"
#include "component/select_component.hpp"
#include "support/input_fixture.hpp"
#include "support/bundled_icon_checks.hpp"
#include <iostream>
#include <map>

namespace {
using namespace ryn;
using namespace ryn::input;

struct Fixture final : ryn_test::input_component::Fixture {
    detail::SelectionComponentHost selections{services};
};

struct Row final {
    String id;
    String name;
    int amount{};
    bool locked{};
    friend bool operator==(const Row&, const Row&) = default;
};

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void sync(Fixture& f) {
    f.synchronize(640, {0, 0, 640, 240});
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    const auto& n = f.nodes.require(f.services.components().root(id));
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto b = bounds(f, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, b.x + b.width / 2, b.y + b.height / 2});
    }
    sync(f);
}

std::vector<Row> data() {
    return {{u8"a", u8"Alpha", 30, false},
            {u8"b", u8"Beta", 10, false},
            {u8"c", u8"Gamma", 10, true},
            {u8"d", u8"Delta", 20, false}};
}

std::vector<TableColumn<Row>> columns(std::map<std::string, int>* runs = nullptr) {
    return {TableColumn<Row>{u8"name", u8"Name",
                             [runs](TableCell<Row> cell) {
                                 if (runs) {
                                     ++(*runs)[std::string{cell.key().bytes()}];
                                 }
                                 Text(TextProps{}.content(bind([cell] { return cell.value().name; })));
                             }}
                .width(150.0F),
            TableColumn<Row>{u8"amount", u8"Amount",
                             [](TableCell<Row> cell) {
                                 Text(TextProps{}.content(bind([cell] {
                                     return String::from_utf8(std::to_string(cell.value().amount)).value();
                                 })));
                             }}
                .width(120.0F)
                .align(TableAlign::Right)
                .sorter([](const Row& a, const Row& b) { return a.amount < b.amount; })};
}

void retained_business_rows() {
    Fixture f;
    Signal<std::vector<Row>> source{data()};
    std::map<std::string, int> runs;
    int outside{};
    int header_runs{};
    Signal<String> header_label{String{u8"Customer"}};
    f.services.mount(Content{[&] {
        auto schema = columns(&runs);
        schema[0].header(TableHeader{[&] {
            ++header_runs;
            Text(TextProps{}.content(header_label));
        }});
        Table(TableProps<Row>{}
                  .dataSource(source)
                  .rowKey([](const Row& row) { return row.id; })
                  .columns(schema)
                  .scrollY(100.0F)
                  .bordered());
        ++outside;
        Text(u8"Sibling");
    }});
    sync(f);
    const auto id = f.services.table().mounted()[0];
    auto original = f.services.table().snapshot(id);
    check(original.visible_keys == std::vector<String>{u8"a", u8"b", u8"c", u8"d"} && runs.size() == 4,
          "initial business rows");
    const auto count = f.services.components().component_count();
    const auto amount_cell = original.rows.front().cells[1];
    const auto amount_region = f.services.components().children(amount_cell).front();
    const auto cell_bounds = bounds(f, amount_cell);
    const auto content_bounds = bounds(f, amount_region);
    check(std::abs(content_bounds.x + content_bounds.width - (cell_bounds.x + cell_bounds.width - 16)) < 0.01F,
          "column right alignment");
    auto next = data();
    next[0].name = String{u8"Alpha / 更新"};
    next[0].amount = 42;
    std::swap(next[0], next[3]);
    source.set(next);
    header_label.set(String{u8"客户 / Updated"});
    sync(f);
    auto s = f.services.table().snapshot(id);
    check(s.rows.back().component == original.rows.front().component &&
              s.rows.back().cells == original.rows.front().cells && runs["a"] == 1 && runs["b"] == 1 && outside == 1 &&
              header_runs == 1 && f.services.components().component_count() == count,
          "data update/reorder remounted cell");
    bool fresh_text{};
    for (const auto& text : f.services.text().mounted_texts()) {
        if (f.scene.text_state(text.scene).content().bytes() == String{u8"Alpha / 更新"}.bytes()) {
            fresh_text = true;
        }
    }
    check(fresh_text, "reactive cell did not display updated data");
    next.erase(next.begin() + 1);
    next.push_back({u8"e", u8"Epsilon", 5, false});
    source.set(next);
    sync(f);
    s = f.services.table().snapshot(id);
    check(!f.services.components().contains(original.rows[1].component) && runs["e"] == 1 && runs["a"] == 1 &&
              s.rows.size() == 4,
          "row add/remove cleanup");
    const auto valid = s;
    next[0].id = next[1].id;
    bool rejected{};
    try {
        source.set(next);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && f.services.table().snapshot(id).rows[0].component == valid.rows[0].component &&
              f.services.table().snapshot(id).rows.size() == valid.rows.size(),
          "duplicate row keys changed retained structure");
    source.set({});
    sync(f);
    check(f.services.table().snapshot(id).rows.empty() && f.services.table().snapshot(id).visible_keys.empty(),
          "empty data");
    check(f.services.destroy(id), "Table destroy");
    sync(f);
    check(f.services.table().mounted().empty() && f.services.interactions().size() == 0 &&
              f.services.components().component_count() == 1 && f.scene.size() == 1,
          "Table leaked resources or destroyed sibling");
}

void controlled_sort_selection_pages() {
    Fixture f;
    Signal<std::vector<Row>> source{data()};
    Signal<TableSort> sort{TableSort{}};
    Signal<std::vector<String>> selected{std::vector<String>{}};
    Signal<std::size_t> page{1};
    Signal<bool> loading{false};
    bool accept{};
    std::vector<TableSort> sorts;
    std::vector<std::vector<String>> selections;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 Table(TableProps<Row>{}
                           .dataSource(source)
                           .rowKey([](const auto& row) { return row.id; })
                           .columns(columns())
                           .sort(sort)
                           .onSortChange([&](TableSort next) {
                               sorts.push_back(next);
                               if (accept) {
                                   sort.set(next);
                               }
                           })
                           .rowSelection({selected,
                                          [&](auto next) {
                                              selections.push_back(next);
                                              if (accept) {
                                                  selected.set(next);
                                              }
                                          },
                                          [](const Row& row) { return row.locked; }})
                           .current(page)
                           .pageSize(std::size_t{2})
                           .loading(loading)
                           .size(ControlSize::Small)
                           .scrollY(88.0F));
                 Pagination(PaginationProps{}
                                .total(std::size_t{4})
                                .current(page)
                                .pageSize(std::size_t{2})
                                .onChange([page](auto next, auto) { page.set(next); }));
             }});
    }});
    sync(f);
    const auto id = f.services.table().mounted()[0];
    auto s = f.services.table().snapshot(id);
    const auto sort_icon =
        ryn_test::require_bundled_icon(f.services, s.sort_buttons[1], IconName::ColumnHeightOutlined);
    click(f, s.sort_buttons[1]);
    check(sorts.size() == 1 && f.services.table().snapshot(id).sort.order == TableSortOrder::None,
          "sort controlled refusal");
    check(ryn_test::require_bundled_icon(f.services, s.sort_buttons[1], IconName::ColumnHeightOutlined) == sort_icon,
          "rejected Table sort changed the retained icon");
    click(f, s.rows[0].checkbox);
    check(selections == std::vector<std::vector<String>>{{u8"a"}} && selected.get().empty(),
          "selection controlled refusal");
    accept = true;
    click(f, s.sort_buttons[1]);
    s = f.services.table().snapshot(id);
    check(s.visible_keys == std::vector<String>{u8"b", u8"c"}, "stable ascending sort/page");
    check(ryn_test::require_bundled_icon(f.services, s.sort_buttons[1], IconName::ArrowUpOutlined) == sort_icon,
          "ascending Table sort did not use the retained bundled icon");
    check(bounds(f, s.rows[1].component).y == s.body.y && bounds(f, s.rows[2].component).y > s.body.y,
          "sorted rows were not placed in display order");
    click(f, s.select_all);
    check(selected.get() == std::vector<String>{u8"b"}, "selectAll included disabled row");
    check(f.selections.snapshot(s.rows[1].checkbox).checked && !f.selections.snapshot(s.rows[2].checkbox).checked &&
              f.selections.snapshot(s.rows[2].checkbox).disabled,
          "controlled checkbox presentation did not match selected/disabled rows");
    const auto pager = f.services.pagination().mounted()[0];
    click(f, f.services.pagination().snapshot(pager).items.back());
    s = f.services.table().snapshot(id);
    check(page.get() == 2 && s.current == 2 && s.visible_keys == std::vector<String>{u8"d", u8"a"},
          "composed pagination");
    click(f, s.select_all);
    check(selected.get() == std::vector<String>{u8"b", u8"d", u8"a"}, "selection did not preserve other page");
    loading.set(true);
    sync(f);
    const auto selection_count = selections.size();
    const auto sort_count = sorts.size();
    click(f, s.select_all);
    click(f, s.sort_buttons[1]);
    check(selections.size() == selection_count && sorts.size() == sort_count && f.services.table().snapshot(id).loading,
          "loading controls still active");
    loading.set(false);
    sync(f);
    click(f, s.sort_buttons[1]);
    check(f.services.table().snapshot(id).sort.order == TableSortOrder::Descend &&
              f.services.table().snapshot(id).visible_keys == std::vector<String>{u8"b", u8"c"},
          "descending sort/page");
    check(ryn_test::require_bundled_icon(f.services, s.sort_buttons[1], IconName::ArrowDownOutlined) == sort_icon,
          "descending Table sort did not use the retained bundled icon");
    click(f, s.sort_buttons[1]);
    check(f.services.table().snapshot(id).sort.order == TableSortOrder::None, "sort cycle did not clear");
    check(ryn_test::require_bundled_icon(f.services, s.sort_buttons[1], IconName::ColumnHeightOutlined) == sort_icon,
          "cleared Table sort did not restore the retained bundled icon");
}

void scroll_theme_and_nested_cleanup() {
    Fixture f;
    Signal<ThemeConfig> config{ThemeConfig{}};
    Signal<std::vector<Row>> source{data()};
    std::map<std::string, int> runs;
    Signal<float> name_width{240.0F};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  auto schema = columns(&runs);
                  schema[0].width(name_width);
                  schema[1].width(180.0F);
                  schema.emplace_back(u8"action", u8"Action",
                                      [](TableCell<Row> cell) { Input(InputProps{}.defaultValue(cell.key())); });
                  schema.back().width(160.0F);
                  Table(TableProps<Row>{}
                            .dataSource(source)
                            .rowKey([](const Row& row) { return row.id; })
                            .columns(schema)
                            .layout(LayoutStyle{}.width(dp(320)).height(dp(230)))
                            .scrollY(80.0F));
              }});
    }});
    sync(f);
    const auto id = f.services.table().mounted()[0];
    auto s = f.services.table().snapshot(id);
    check(s.maximum_x == 260 && s.maximum_y > 0 && s.body.height == 80, "finite viewport geometry");
    const auto first = s.rows[0].cells;
    const auto alpha_text = f.services.text().mounted_texts().front().scene;
    const auto shapes = f.scene.text_state(alpha_text).counters().shape_count;
    check(f.inputs.dispatch(ScrollInputEvent{100, -100, s.body.x + 10, s.body.y + 10}),
          "Table down wheel was not consumed");
    sync(f);
    s = f.services.table().snapshot(id);
    check(s.scroll_x == s.maximum_x && s.scroll_y == s.maximum_y && runs["a"] == 1 && s.rows[0].cells == first,
          "scroll clamp or retained identity");
    check(std::abs(bounds(f, s.rows[0].component).y - (s.body.y - s.scroll_y)) < 0.001F,
          "scroll did not translate retained row geometry");
    check(f.scene.text_state(alpha_text).counters().shape_count == shapes, "scroll reshaped retained text");
    check(f.inputs.dispatch(ScrollInputEvent{0, 100, s.body.x + 10, s.body.y + 10}), "Table up wheel was not consumed");
    sync(f);
    check(f.services.table().snapshot(id).scroll_y == 0, "positive Y wheel did not return body to top");
    name_width.set(150.0F);
    sync(f);
    s = f.services.table().snapshot(id);
    check(s.maximum_x == 170 && s.scroll_x == 170 && s.widths.front() == 150 &&
              bounds(f, s.rows[0].cells[0]).width == 150 && runs["a"] == 1,
          "reactive column width did not clamp scroll");
    const auto hit = f.services.hit_test().hit_test({s.body.x + 10, s.body.y - 1});
    check(!hit || f.services.interactions().require(*hit).component != f.inputs.mounted_inputs()[0].component,
          "body hit escaped clip");
    const auto root_node = f.services.components().root(id);
    const auto before = f.nodes.require(root_node).measure_count;
    ThemeConfig color;
    color.table.tokens.border = Color::rgba8(255, 0, 0);
    config.set(color);
    sync(f);
    check(runs["a"] == 1 && f.nodes.require(root_node).measure_count == before,
          "Table color update measured/remounted content");
    color.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
    color.table.tokens.padding_block = dp(10);
    config.set(color);
    sync(f);
    check(runs["a"] == 1 && f.inputs.mounted_inputs().size() == 4, "Table theme remounted nested Input");
    const auto focused_input = f.inputs.mounted_inputs().back();
    check(f.services.focus().request_focus(focused_input.interaction, FocusModality::keyboard), "Table cell focus");
    sync(f);
    const auto focused_bounds = bounds(f, focused_input.component);
    s = f.services.table().snapshot(id);
    check(focused_bounds.y >= s.body.y && focused_bounds.y + focused_bounds.height <= s.body.y + s.body.height + 0.01F,
          "focused row did not reveal inside finite viewport");
    check(f.inputs.sessions().active().valid(), "nested Input session not started");
    auto remaining = data();
    remaining.pop_back();
    source.set(remaining);
    sync(f);
    check(f.inputs.mounted_inputs().size() == 3 && !f.inputs.sessions().active().valid(),
          "removed focused row leaked input session");
    check(f.services.destroy(id), "nested Table destroy");
    sync(f);
    check(f.inputs.mounted_inputs().empty() && f.services.table().mounted().empty() &&
              f.services.interactions().size() == 0 && f.scene.size() == 0 && f.services.surfaces().size() == 0,
          "nested Table resources leaked");
}

void retained_select_popup_wheel_and_anchor() {
    Fixture f;
    Signal<std::vector<Row>> source{data()};
    int cell_runs{};
    std::vector<SelectOption> options;
    for (int i = 0; i < 20; ++i) {
        options.push_back({i, String::from_utf8(std::to_string(i)).value()});
    }
    f.services.mount(Content{[&] {
        Table(TableProps<Row>{}
                  .dataSource(source)
                  .rowKey([](const Row& row) { return row.id; })
                  .columns({TableColumn<Row>{
                      u8"value", u8"Value",
                      [&](TableCell<Row> cell) {
                          ++cell_runs;
                          if (cell.key() == String{u8"a"}) {
                              Select(SelectProps{}.options(options).open(true).listHeight(64.0F).layout(
                                  LayoutStyle{}.width(dp(200))));
                          } else {
                              Text(TextProps{}.content(bind([cell] { return cell.value().name; })));
                          }
                      }}.width(400.0F)})
                  .layout(LayoutStyle{}.width(dp(400)))
                  .scrollY(180.0F));
    }});
    sync(f);
    const auto table = f.services.table().mounted().front();
    const auto select = f.services.select().mounted().front();
    const auto original = f.services.select().snapshot(select);
    auto next = data();
    next[0].amount = 99;
    source.set(next);
    sync(f);
    auto menu = f.services.select().snapshot(select);
    const auto body = f.services.table().snapshot(table).body;
    check(menu.visible && !menu.above && menu.popup.y + 10 < body.y + body.height &&
              menu.selector == original.selector && menu.rows == original.rows && cell_runs == 4,
          "Select popup regression requires retained overlapping popup/body");
    check(f.inputs.dispatch(ScrollInputEvent{0, -1, menu.popup.x + 10, menu.popup.y + 10}), "popup wheel unhandled");
    sync(f);
    check(f.services.select().snapshot(select).scroll > menu.scroll && f.services.table().snapshot(table).scroll_y == 0,
          "Table consumed Select popup wheel");
    const auto anchor = bounds(f, select);
    check(f.inputs.dispatch(ScrollInputEvent{0, -0.5F, body.x + body.width - 5, body.y + 10}), "body wheel unhandled");
    sync(f);
    menu = f.services.select().snapshot(select);
    const auto moved_anchor = bounds(f, select);
    const auto gap = f.services.components().theme_scope(select)->snapshot().select().metrics.gap;
    check(moved_anchor.y < anchor.y && std::abs(menu.popup.y - (moved_anchor.y + moved_anchor.height + gap)) < 0.01F,
          "Select popup did not follow Table transform in the same synchronization");
    check(f.inputs.dispatch(ScrollInputEvent{0, 0.5F, body.x + body.width - 5, body.y + 10}),
          "body up wheel unhandled");
    sync(f);
    check(f.services.table().snapshot(table).scroll_y == 0 &&
              std::abs(f.services.select().snapshot(select).popup.y - original.popup.y) < 0.01F,
          "Select popup anchor did not return with body");
    f.services.destroy(table);
    sync(f);
    check(f.services.select().mounted().empty() && f.services.interactions().size() == 0,
          "retained Select popup leaked after Table cleanup");
}

void callbacks_can_destroy_table() {
    for (bool selection : {false, true}) {
        Fixture f;
        runtime::ComponentId id;
        f.services.mount(Content{[&] {
            auto props = TableProps<Row>{}
                             .dataSource(data())
                             .rowKey([](const Row& row) { return row.id; })
                             .columns(columns())
                             .scrollY(150.0F);
            if (selection) {
                props.rowSelection({std::vector<String>{}, [&](auto) { f.services.destroy(id); }, {}});
            } else {
                props.onSortChange([&](auto) { f.services.destroy(id); });
            }
            Table(props);
        }});
        sync(f);
        id = f.services.table().mounted()[0];
        const auto s = f.services.table().snapshot(id);
        click(f, selection ? s.select_all : s.sort_buttons[1]);
        check(f.services.table().mounted().empty() && f.services.interactions().size() == 0 && f.scene.size() == 0,
              "Table destruction callback leaked");
    }
}

void inactive_new_rows_do_not_steal_focus() {
    Fixture f;
    Signal<std::vector<Row>> source{std::vector<Row>{{u8"a", u8"Active", 1, false}}};
    Signal<String> incidental{String{u8"One-time setup"}};
    int key_reads{};
    f.services.mount(Content{[&] {
        Table(TableProps<Row>{}
                  .dataSource(source)
                  .rowKey([&](const Row& row) {
                      ++key_reads;
                      return row.id;
                  })
                  .columns({TableColumn<Row>{u8"editor", u8"Editor",
                                             [incidental](TableCell<Row> cell) {
                                                 static_cast<void>(incidental.get());
                                                 Input(InputProps{}.defaultValue(cell.value().name).autoFocus());
                                             }}})
                  .current(std::size_t{1})
                  .pageSize(std::size_t{1}),
              TableEmpty{[] { Input(InputProps{}.defaultValue(u8"Empty editor").autoFocus()); }});
    }});
    sync(f);
    const auto original = f.inputs.mounted_inputs().front();
    check(f.services.focus().state().focused == original.interaction, "active row autofocus");
    const auto starts = f.platform.starts;
    const auto session = f.inputs.sessions().active();
    source.set({{u8"a", u8"Active", 1, false}, {u8"b", u8"Hidden", 2, false}});
    sync(f);
    check(f.services.focus().state().focused == original.interaction && f.inputs.sessions().active() == session &&
              f.platform.starts == starts,
          "inactive new row stole active editor focus/session");
    check(f.inputs.mounted_inputs().size() == 3, "inactive row/empty content was not retained");
    source.set({{u8"a", u8"Updated", 3, false}, {u8"b", u8"Updated hidden", 4, false}});
    sync(f);
    const auto reads = key_reads;
    incidental.set(String{u8"Changed independently"});
    sync(f);
    check(key_reads == reads && f.services.focus().state().focused == original.interaction,
          "one-time cell reads contaminated data source dependencies");
}

void retained_row_tab_order() {
    Fixture f;
    Signal<std::vector<Row>> source{std::vector<Row>{{u8"a", u8"Alpha", 1, false}, {u8"b", u8"Beta", 2, false}}};
    int cell_runs{};
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 Input(InputProps{}.defaultValue(u8"Before"));
                 Table(TableProps<Row>{}
                           .dataSource(source)
                           .rowKey([](const Row& row) { return row.id; })
                           .columns({TableColumn<Row>{u8"editor", u8"Editor",
                                                      [&](TableCell<Row> cell) {
                                                          ++cell_runs;
                                                          Input(InputProps{}.defaultValue(cell.key()));
                                                      }}})
                           .scrollY(100.0F));
                 Input(InputProps{}.defaultValue(u8"After"));
             }});
    }});
    sync(f);
    const auto table = f.services.table().mounted().front();
    const auto original = f.services.table().snapshot(table);
    const auto before = f.inputs.mounted_inputs()[0].interaction;
    const auto a = f.inputs.mounted_inputs()[1].interaction;
    const auto b = f.inputs.mounted_inputs()[2].interaction;
    const auto after = f.inputs.mounted_inputs()[3].interaction;
    const auto expect_tabs = [&](std::vector<InteractionId> expected, const char* message) {
        check(f.services.focus().request_focus(before, FocusModality::keyboard), "focus preceding control");
        for (auto interaction : expected) {
            f.services.focus().dispatch(KeyboardInputEvent{Key::tab, KeyAction::down});
            f.services.focus().dispatch(KeyboardInputEvent{Key::tab, KeyAction::up});
            sync(f);
            check(f.services.focus().state().focused == interaction, message);
        }
    };
    expect_tabs({a, b, after}, "initial Input Tab order");
    check(f.services.focus().request_focus(a, FocusModality::keyboard), "focus retained row before reorder");
    const auto session = f.inputs.sessions().active();
    source.set({{u8"b", u8"Beta", 2, false}, {u8"a", u8"Alpha", 1, false}});
    sync(f);
    const auto reordered = f.services.table().snapshot(table);
    check(reordered.rows[0].component == original.rows[1].component &&
              reordered.rows[1].component == original.rows[0].component && cell_runs == 2 &&
              f.services.focus().state().focused == a && f.inputs.sessions().active() == session,
          "row reorder replaced identity or focused editor session");
    expect_tabs({b, a, after}, "A/B to B/A did not update actual Tab order");
    source.set({{u8"b", u8"Beta", 2, false}, {u8"c", u8"New", 3, false}, {u8"a", u8"Alpha", 1, false}});
    sync(f);
    const auto c = f.inputs.mounted_inputs().back().interaction;
    check(cell_runs == 3 && f.services.interactions().find(a) && f.services.interactions().find(b),
          "insertion rebuilt retained Inputs");
    expect_tabs({b, c, a, after}, "inserted row Input was not before external following control");
    source.set({{u8"c", u8"New", 3, false}});
    sync(f);
    expect_tabs({c, after}, "removed row remained in Tab order");
    f.services.destroy(table);
    sync(f);
    expect_tabs({after}, "Table cleanup damaged external Input Tab order");
    check(f.inputs.mounted_inputs().size() == 2 && !f.services.interactions().find(c), "row interaction cleanup");
}

void varying_height_cells_stretch_and_center() {
    Fixture f;
    Signal<std::vector<Row>> source{std::vector<Row>{{u8"a", u8"Short", 1, false}}};
    f.services.mount(Content{[&] {
        Table(TableProps<Row>{}
                  .dataSource(source)
                  .rowKey([](const Row& row) { return row.id; })
                  .columns({TableColumn<Row>{u8"short", u8"Short",
                                             [](TableCell<Row> cell) {
                                                 Text(TextProps{}.content(bind([cell] { return cell.value().name; })));
                                             }}
                                .width(180.0F),
                            TableColumn<Row>{u8"multiline", u8"Multiline",
                                             [](TableCell<Row>) { Text(u8"Line 1\nLine 2\nLine 3"); }}
                                .width(220.0F)})
                  .rowSelection({std::vector<String>{}, [](auto) {}, {}})
                  .scrollY(180.0F));
    }});
    sync(f);
    const auto table = f.services.table().mounted().front();
    const auto row = f.services.table().snapshot(table).rows.front();
    const auto full = bounds(f, row.component);
    const auto short_region = f.services.components().children(row.cells[1]).front();
    const auto tall_region = f.services.components().children(row.cells[2]).front();
    const auto short_bounds = bounds(f, f.services.components().children(short_region).front());
    const auto tall_bounds = bounds(f, f.services.components().children(tall_region).front());
    const auto checkbox = bounds(f, row.checkbox);
    check(tall_bounds.height > short_bounds.height + 20, "multiline fixture did not create unequal cell heights");
    for (auto cell : row.cells) {
        const auto cell_bounds = bounds(f, cell);
        check(cell_bounds.y == full.y && cell_bounds.height == full.height,
              "short Table cell did not stretch to full row height");
    }
    for (const auto content : {short_bounds, tall_bounds, checkbox}) {
        check(std::abs(content.y + content.height / 2 - (full.y + full.height / 2)) < 0.01F,
              "Table Checkbox/Text content was not vertically centered in full row");
    }
}

void modal_table_scroll_and_select_anchor() {
    Fixture f;
    int cell_runs{};
    std::vector<SelectOption> options;
    for (int i = 0; i < 20; ++i) {
        options.push_back({i, String::from_utf8(std::to_string(i)).value()});
    }
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open().title(u8"Orders").width(460).footer(ModalFooterMode::None), ModalContent{[&] {
                  Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                           Table(TableProps<Row>{}
                                     .dataSource(data())
                                     .rowKey([](const Row& row) { return row.id; })
                                     .columns({TableColumn<Row>{
                                         u8"value", u8"Value",
                                         [&](TableCell<Row> cell) {
                                             ++cell_runs;
                                             if (cell.key() == String{u8"a"}) {
                                                 Select(
                                                     SelectProps{}.options(options).open(true).listHeight(64.0F).layout(
                                                         LayoutStyle{}.width(dp(200))));
                                             } else {
                                                 Text(TextProps{}.content(bind([cell] { return cell.value().name; })));
                                             }
                                         }}})
                                     .scrollY(80.0F));
                           Text(u8"Tail\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20");
                       }});
              }});
    }});
    const auto synchronize = [&] {
        check(f.services.layout_and_synchronize({640, 480}, {0, 0, 640, 480}), "Modal Table synchronization");
    };
    synchronize();
    check(f.services.table().snapshot(f.services.table().mounted().front()).body.height == 80,
          "Modal Table body clip was not positioned in the first synchronization");
    synchronize();
    const auto modal = f.services.modal().mounted().front();
    const auto table = f.services.table().mounted().front();
    const auto select = f.services.select().mounted().front();
    const auto original = f.services.table().snapshot(table);
    const auto original_menu = f.services.select().snapshot(select);
    const auto dialog = f.services.modal().snapshot(modal);
    check(dialog.visible && dialog.maximum_scroll > 0 && original.maximum_y > 0 && original.body.height == 80 &&
              original.scroll_y == 0 && dialog.scroll == 0 && original_menu.visible && !original_menu.above,
          "Modal regression requires independent scrollable Modal/Table and open child Select");
    const auto row = original.rows.front().component;
    const auto row_node = f.services.components().root(row);
    const auto first_bounds = bounds(f, row);
    const auto check_anchor = [&] {
        const auto menu = f.services.select().snapshot(select);
        const auto anchor = bounds(f, select);
        const auto gap = f.services.components().theme_scope(select)->snapshot().select().metrics.gap;
        const auto y = menu.above ? anchor.y - gap - menu.popup.height : anchor.y + anchor.height + gap;
        check(std::abs(menu.popup.y - y) < 0.01F && menu.selector == original_menu.selector &&
                  menu.rows == original_menu.rows && cell_runs == 4,
              "Modal/Table child Select lost retained identity or anchor tracking");
    };
    const auto wheel_x = original.body.x + original.body.width - 5;
    const auto wheel_y = original.body.y + 10;
    check(f.inputs.dispatch(ScrollInputEvent{0, -0.5F, wheel_x, wheel_y}), "Modal Table down wheel unhandled");
    synchronize();
    const auto scrolled = f.services.table().snapshot(table);
    check(scrolled.scroll_y > 0 && f.services.modal().snapshot(modal).scroll == dialog.scroll,
          "Modal consumed scrollable Table body wheel");
    const auto check_row = [&] {
        const auto table_translation = f.nodes.require(f.services.components().root(table)).translation;
        check(std::abs(bounds(f, row).y - (first_bounds.y - scrolled.scroll_y)) < 0.01F &&
                  std::abs(f.nodes.require(row_node).translation.y - (table_translation.y - scrolled.scroll_y)) < 0.01F,
              "Modal positioning overwrote Table row translation");
        const auto clip = f.nodes.content_clip(row_node, {0, 0, 640, 480});
        check(clip.y >= original.body.y && clip.y + clip.height <= original.body.y + original.body.height &&
                  bounds(f, row).y < clip.y,
              "scrolled Modal Table row escaped body clipping");
        const auto hit = f.services.hit_test().hit_test({wheel_x, original.body.y - 1});
        check(!hit || f.services.interactions().require(*hit).component != row, "Modal Table row hit escaped clip");
    };
    check_row();
    check_anchor();
    synchronize();
    check_row();
    check_anchor();
    auto menu = f.services.select().snapshot(select);
    check(menu.popup.y + 10 >= original.body.y && menu.popup.y + 10 < original.body.y + original.body.height,
          "child popup wheel must overlap Table body");
    check(f.inputs.dispatch(ScrollInputEvent{0, -1, menu.popup.x + 10, menu.popup.y + 10}),
          "Modal child popup wheel unhandled");
    synchronize();
    check(f.services.select().snapshot(select).scroll > menu.scroll &&
              f.services.table().snapshot(table).scroll_y == scrolled.scroll_y &&
              f.services.modal().snapshot(modal).scroll == dialog.scroll,
          "Table/Modal stole child Select popup wheel");
    check(f.inputs.dispatch(ScrollInputEvent{0, 0.5F, wheel_x, wheel_y}), "Modal Table up wheel unhandled");
    synchronize();
    check(f.services.table().snapshot(table).scroll_y == 0 && bounds(f, row).y == first_bounds.y,
          "Modal Table up wheel failed to restore row");
    check_anchor();
    check(f.inputs.dispatch(ScrollInputEvent{0, -1, dialog.body_bounds.x + dialog.body_bounds.width - 5,
                                             dialog.body_bounds.y + dialog.body_bounds.height - 5}),
          "outer Modal body wheel unhandled");
    synchronize();
    const auto outer_scroll = f.services.modal().snapshot(modal).scroll;
    check(outer_scroll > 0 && f.services.table().snapshot(table).scroll_y == 0 &&
              std::abs(bounds(f, row).y - (first_bounds.y - outer_scroll)) < 0.01F,
          "Modal outer scroll did not propagate through retained Table");
    check_anchor();
    f.services.destroy(modal);
    synchronize();
    check(f.services.table().mounted().empty() && f.services.select().mounted().empty() &&
              f.services.interactions().size() == 0,
          "Modal Table/Select cleanup leaked interactions");
}

void empty_body_horizontal_wheel() {
    Fixture f;
    Signal<std::vector<Row>> source{std::vector<Row>{}};
    Signal<float> height{96};
    f.services.mount(Content{[&] {
        Table(TableProps<Row>{}
                  .dataSource(source)
                  .rowKey([](const Row& row) { return row.id; })
                  .columns({TableColumn<Row>{u8"name", u8"Name", [](TableCell<Row>) { Text(u8"Row"); }}.width(600)})
                  .scrollY(height)
                  .layout(LayoutStyle{}.width(dp(300))));
    }});
    sync(f);
    const auto table = f.services.table().mounted().front();
    const auto original = f.services.table().snapshot(table);
    const float x = original.body.x + 10;
    const float y = original.body.y + 10;
    check(original.maximum_x == 300 && original.rows.empty(), "empty Table horizontal wheel fixture");
    check(f.services.dispatch_scroll({1, 0, x, y}), "empty Table body horizontal wheel was unhandled");
    sync(f);
    check(f.services.table().snapshot(table).scroll_x == 32, "empty Table body did not scroll horizontally");
    const auto background = f.services.hit_test().hit_test({x, y});
    check(background && !f.services.interactions().require(*background).focusable,
          "empty Table body lacks a nonfocusable background");
    check(!f.services.focus().request_focus(*background, FocusModality::keyboard),
          "Table body background accepted keyboard focus");
    f.services.focus().dispatch(KeyboardInputEvent{Key::tab, KeyAction::down});
    f.services.focus().dispatch(KeyboardInputEvent{Key::tab, KeyAction::up});
    check(!f.services.focus().state().focused, "empty Table body added a Tab stop");
    source.set(data());
    sync(f);
    const auto row_hit = f.services.hit_test().hit_test({x, y});
    check(row_hit && *row_hit != *background, "Table body background stole row hit priority");
    source.set({});
    sync(f);
    check(f.services.hit_test().hit_test({x, y}) == background, "Table body background identity changed with rows");
    f.synchronize(640, {0, 0, 640, y - 1});
    check(!f.services.hit_test().hit_test({x, y}) && !f.services.dispatch_scroll({1, 0, x, y}),
          "empty Table body background escaped document clipping");
    sync(f);
    check(f.services.hit_test().hit_test({x, y}) == background, "Table body background identity changed with clipping");
    height.set(0);
    sync(f);
    check(!f.services.hit_test().hit_test({x, y}) && !f.services.dispatch_scroll({1, 0, x, y}),
          "zero-height Table body background remained eligible");
    height.set(96);
    sync(f);
    check(f.services.hit_test().hit_test({x, y}) == background, "Table body background identity changed with layout");
    f.services.destroy(table);
    sync(f);
    check(!f.services.interactions().find(*background) && f.services.interactions().size() == 0 &&
              f.services.surfaces().size() == 0 && !f.services.hit_test().hit_test({x, y}),
          "empty Table body background cleanup leaked");
}
} // namespace

int main() {
    try {
        empty_body_horizontal_wheel();
        retained_business_rows();
        controlled_sort_selection_pages();
        scroll_theme_and_nested_cleanup();
        retained_select_popup_wheel_and_anchor();
        callbacks_can_destroy_table();
        inactive_new_rows_do_not_steal_focus();
        retained_row_tab_order();
        varying_height_cells_stretch_and_center();
        modal_table_scroll_and_select_anchor();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
