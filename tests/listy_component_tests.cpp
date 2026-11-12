#include "component/listy_component.hpp"
#include "component/table_component.hpp"
#include "component/select_component.hpp"
#include "component/popover_component.hpp"
#include "support/input_fixture.hpp"
#include <algorithm>
#include <iostream>
#include <cmath>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

std::size_t assertions{};

void check(bool value, const char* message) {
    ++assertions;
    if (!value) {
        throw std::runtime_error(message);
    }
}

std::vector<detail::MountedInputComponent> row_inputs(Fixture& f, runtime::ComponentId viewport) {
    std::vector<detail::MountedInputComponent> result;
    for (const auto& input : f.inputs.mounted_inputs()) {
        for (auto owner = std::optional{input.component}; owner; owner = f.services.components().parent(*owner)) {
            if (*owner == viewport) {
                result.push_back(input);
                break;
            }
        }
    }
    return result;
}

std::vector<ListyItem<String>> records() {
    return {{u8"a", u8"Alpha"}, {u8"b", u8"Beta"}, {u8"c", u8"Gamma"}, {u8"d", u8"Delta"}};
}

void keyed_data_grid_and_lifecycle() {
    Fixture f;
    Signal<std::vector<ListyItem<String>>> items{records()};
    Signal<std::size_t> columns{1};
    Signal<bool> disabled{false};
    Signal<bool> visible{true};
    int runs{};
    ListyRef ref;
    f.services.mount(Content{[&] {
        Listy(ListyProps<String>{}
                  .items(items)
                  .columns(columns)
                  .height(100.0F)
                  .ref(ref)
                  .disabled(disabled)
                  .visible(visible)
                  .layout(LayoutStyle{}.width(dp(320)))
                  .itemRender([&](ListyItemView<String> view) {
                      ++runs;
                      Flex(FlexProps{}.vertical(true), FlexContent{[view] {
                               Text(TextProps{}.content(view.data()));
                               Input(InputProps{}.defaultValue(u8"draft").layout(LayoutStyle{}.width(dp(100))));
                           }});
                  }));
    }});
    f.synchronize();
    const auto id = f.services.listy().mounted().front();
    const auto original = f.services.listy().snapshot(id);
    const auto input = row_inputs(f, original.viewport).front();
    check(runs == 4 && ref.bound() && original.maximum > 0, "Listy did not mount real scrollable rows");
    check(f.services.focus().request_focus(input.interaction, FocusModality::keyboard), "Listy editor focus failed");
    static_cast<void>(f.inputs.editors().require(input.editor).select_all());
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"retained"}, f.inputs.sessions().active()})),
          "Listy editor input failed");
    auto next = items.get();
    std::reverse(next.begin(), next.end());
    next.back().data(String{u8"Updated Alpha"});
    items.set(next);
    columns.set(2);
    f.synchronize();
    const auto grid = f.services.listy().snapshot(id);
    check(runs == 4 && grid.items.back().component == original.items.front().component &&
              grid.items[0].bounds.y == grid.items[1].bounds.y && grid.items[1].bounds.x > grid.items[0].bounds.x,
          "Listy grid/reorder remounted keys or failed actual placement");
    check(f.inputs.editors().require(input.editor).value() == String{u8"retained"}.bytes(), "Listy lost the draft");
    disabled.set(true);
    f.synchronize();
    check(!f.services.focus().state().focused && !f.inputs.sessions().active().valid() &&
              f.services.components().branch_active(grid.viewport) && !ref.scrollTo(0),
          "Listy disabled did not gate input while keeping content");
    disabled.set(false);
    visible.set(false);
    f.synchronize();
    check(!f.services.components().branch_active(id) && runs == 4,
          "Listy visible remounted or retained active content");
    visible.set(true);
    items.set({});
    f.synchronize();
    check(f.services.listy().snapshot(id).items.empty() && row_inputs(f, original.viewport).empty() &&
              f.services.components().branch_active(original.empty),
          "Listy empty state retained removed input");
    items.set(records());
    f.synchronize();
    check(runs == 8 && f.services.listy().snapshot(id).items.size() == 4, "Listy empty recovery failed");
    f.services.dispose();
    check(!ref.bound() && !ref.scrollTo(1) && f.services.interactions().size() == 0 && f.nodes.size() == 0,
          "Listy disposal leaked ref, interactions or layouts");
}

void controlled_paging_and_scroll() {
    Fixture f;
    Signal<std::size_t> page{1};
    Signal<ListyDirection> direction{ListyDirection::Vertical};
    Signal<bool> paginated{true};
    Signal<std::vector<ListyItem<String>>> items{records()};
    ListyRef ref;
    int requests{};
    int runs{};
    f.services.mount(Content{[&] {
        Listy(ListyProps<String>{}
                  .items(items)
                  .current(page)
                  .pageSize(2)
                  .pagination(paginated)
                  .direction(direction)
                  .height(40.0F)
                  .ref(ref)
                  .onChange([&](auto, auto) { ++requests; })
                  .layout(LayoutStyle{}.width(dp(220)))
                  .itemRender([&](ListyItemView<String> view) {
                      ++runs;
                      Input(InputProps{}.defaultValue(view.key).layout(LayoutStyle{}.width(dp(160))));
                  }));
    }});
    f.synchronize();
    const auto id = f.services.listy().mounted().front();
    const auto first = f.services.listy().snapshot(id);
    check(first.items[0].shown && !first.items[2].shown, "Listy paging did not gate rows");
    const auto viewport_bounds = f.nodes.require(f.services.components().root(first.viewport)).bounds;
    const auto pager_bounds = f.nodes.require(f.services.components().root(first.pagination)).bounds;
    const auto section_gap = f.services.components().theme_scope(id)->snapshot().map().size_small;
    check(pager_bounds.y >= viewport_bounds.y + viewport_bounds.height + section_gap,
          "Listy pagination touched the clipped row viewport");
    // The actual Pagination input path must request a page and respect application rejection.
    bool clicked{};
    for (const auto& button : f.buttons.mounted_buttons()) {
        const auto rect = f.nodes.require(f.services.components().root(button.component)).bounds;
        if (rect.width > 0 && rect.height > 0) {
            f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                           rect.x + rect.width / 2, rect.y + rect.height / 2});
            f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary,
                                           rect.x + rect.width / 2, rect.y + rect.height / 2});
            if (requests) {
                clicked = true;
                break;
            }
        }
    }
    check(clicked && f.services.listy().snapshot(id).current == 1,
          "Listy controlled request was missing or changed authority");
    page.set(2);
    f.synchronize();
    check(f.services.listy().snapshot(id).items[2].shown && !f.services.listy().snapshot(id).items[0].shown &&
              runs == 4,
          "Listy page change rebuilt rows or did not update shown rows");
    items.set({records()[0]});
    f.synchronize();
    check(f.services.listy().snapshot(id).current == 1 && page.get() == 2,
          "Listy total shrink changed controlled application state");
    items.set(records());
    paginated.set(false);
    direction.set(ListyDirection::Horizontal);
    f.synchronize();
    check(ref.scrollTo(String{u8"d"}, ListyScrollAlign::End) && f.services.listy().snapshot(id).offset > 0,
          "Listy horizontal scrollTo key failed");
}

void validation_and_failed_renderer() {
    Fixture f;
    Signal<std::vector<ListyItem<String>>> items{records()};
    f.services.mount(Content{[&] {
        Listy(ListyProps<String>{}.items(items).itemRender([](ListyItemView<String> view) {
            Input(InputProps{});
            if (view.key == String{u8"fail"}) {
                throw std::runtime_error("renderer failure");
            }
        }));
    }});
    f.synchronize();
    bool rejected{};
    try {
        items.set({{u8"duplicate", u8"One"}, {u8"duplicate", u8"Two"}});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    const auto viewport = f.services.listy().snapshot(f.services.listy().mounted().front()).viewport;
    check(rejected && row_inputs(f, viewport).size() == 4, "Listy invalid keys mutated live rows");
    items.set({{u8"fail", u8"Failure"}});
    rejected = false;
    try {
        f.synchronize();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected && f.inputs.mounted_inputs().empty() && f.nodes.size() == 0 && f.services.interactions().size() == 0,
          "Listy renderer failure left partial UI or input");
}

void grouped_virtual_measured_retention() {
    Fixture f;
    std::vector<ListyItem<String>> records;
    for (int i = 0; i < 120; ++i) {
        auto key = String::from_utf8(std::to_string(i)).value();
        records.emplace_back(key, key);
    }
    Signal<std::vector<ListyItem<String>>> source{records};
    Signal<bool> virtualized{true};
    Signal<bool> sticky{true};
    Signal<std::size_t> page{1};
    Signal<bool> paginated{false};
    ListyRef ref;
    int renders{};
    int headers{};
    f.services.mount(Content{[&] {
        Listy(ListyProps<String>{}
                  .items(source)
                  .height(100)
                  .virtualized(virtualized)
                  .sticky(sticky)
                  .virtualization({48, 32, 1})
                  .ref(ref)
                  .current(page)
                  .pagination(paginated)
                  .pageSize(20)
                  .layout(LayoutStyle{}.width(dp(320)))
                  .group(
                      {[](const String& value) {
                           return String::from_utf8(std::to_string(std::stoi(std::string{value.bytes()}) / 20)).value();
                       },
                       [&](ListyGroupView<String> group) {
                           ++headers;
                           Text(TextProps{}.content(bind([items = group.items(), key = group.key] {
                               return String::from_utf8(std::string{key.bytes()} + ":" +
                                                        std::to_string(detail::ListyPropBridge::read(items).size()))
                                   .value();
                           })));
                       }})
                  .itemRender([&](ListyItemView<String> view) {
                      ++renders;
                      Input(InputProps{}.defaultValue(view.key).layout(LayoutStyle{}.width(dp(120)).height(
                          dp(std::stoi(std::string{view.key.bytes()}) % 2 ? 52 : 32))));
                  }));
    }});
    f.synchronize();
    const auto id = f.services.listy().mounted().front();
    const auto first = f.services.listy().snapshot(id);
    check(first.items.size() == 120 && first.mounted_items < 10 && headers < 3 && renders == int(first.mounted_items),
          "Listy virtualization eagerly mounted unseen keyed data/groups");
    const auto viewport_box = f.nodes.require(f.services.components().root(first.viewport)).bounds;
    check(f.services.dispatch_scroll(
              {0, -1, viewport_box.x + viewport_box.width / 2, viewport_box.y + viewport_box.height / 2}),
          "Listy actual wheel was not admitted by the native input route");
    f.synchronize();
    check(f.services.listy().snapshot(id).offset > 0, "Listy wheel did not move the real viewport");
    check(ref.scrollTo(0), "Listy wheel return failed");
    f.synchronize();
    check(ref.scrollTo(100000), "virtual end request failed");
    f.synchronize();
    const auto end = f.services.listy().snapshot(id);
    check(std::abs(end.offset - end.maximum) < .01F, "virtual absolute end did not settle after actual measurement");
    check(ref.scrollTo(0), "virtual end return failed");
    f.synchronize();
    const auto editor = row_inputs(f, first.items.front().component).front();
    check(f.services.focus().request_focus(editor.interaction, FocusModality::keyboard), "virtual editor focus failed");
    static_cast<void>(f.inputs.editors().require(editor.editor).select_all());
    check(bool(f.inputs.dispatch(TextCommitted{u8"virtual draft", f.inputs.sessions().active()})),
          "virtual draft failed");
    check(ref.scrollToGroup(u8"4", ListyScrollAlign::Start), "unmounted group was not revealable");
    f.synchronize();
    const auto distant = f.services.listy().snapshot(id);
    check(distant.offset > 1000 && distant.mounted_items < 25 && distant.items[0].shown,
          "virtual scrolling mounted all rows or deactivated focused editor");
    // Blur the pinned editor, then check clipping and sticky geometry after one sync.
    f.services.focus().clear_focus();
    check(ref.scrollTo(u8"82", ListyScrollAlign::Start), "virtual item reveal failed");
    f.synchronize();
    const auto after = f.services.listy().snapshot(id);
    check(!after.items[0].shown && after.items[82].shown, "virtual active interval did not follow scroll");
    check(std::abs(after.offset - after.items[82].bounds.y) < .01F,
          "virtual reveal lost exact Start alignment after measuring previously unseen rows");
    const auto group = std::ranges::find_if(after.groups, [](const auto& g) { return g.key == String{u8"4"}; });
    check(group != after.groups.end() && group->shown, "active sticky group was not mounted");
    const auto& node = f.nodes.require(f.services.components().root(group->component));
    const auto& viewport = f.nodes.require(f.services.components().root(after.viewport));
    check(node.bounds.y + node.translation.y >= viewport.bounds.y &&
              node.bounds.y + node.translation.y < viewport.bounds.y + 20,
          "sticky group did not pin in the same layout synchronization");
    check(ref.scrollTo(0), "virtual return failed");
    f.synchronize();
    check(f.inputs.editors().require(editor.editor).value() == String{u8"virtual draft"}.bytes() &&
              f.services.listy().snapshot(id).items[0].component == first.items[0].component,
          "virtual revisit lost retained keyed editor identity/draft");
    virtualized.set(false);
    f.synchronize();
    check(f.services.listy().snapshot(id).mounted_items == records.size(),
          "Listy virtualized=false did not expose all retained data");
    virtualized.set(true);
    f.synchronize();
    const auto windowed = f.services.listy().snapshot(id);
    check(std::ranges::count_if(windowed.items, [](const auto& row) { return row.shown; }) < 10 &&
              f.inputs.editors().require(editor.editor).value() == String{u8"virtual draft"}.bytes(),
          "Listy virtualization toggle lost its active interval or retained editor draft");
    std::reverse(records.begin(), records.end());
    source.set(records);
    f.synchronize();
    const auto reversed = f.services.listy().snapshot(id);
    check(reversed.groups.front().key == String{u8"5"} &&
              reversed.items.back().component == first.items.front().component,
          "group reorder did not use first-seen group order and retained business keys");
    paginated.set(true);
    page.set(2);
    f.synchronize();
    const auto second_page = f.services.listy().snapshot(id);
    for (const auto& group : second_page.groups) {
        if (group.key != String{u8"4"} && group.component.valid()) {
            check(!f.services.components().branch_active(group.component),
                  "previous page retained an active group header");
        }
    }
    source.set({});
    f.synchronize();
    check(f.services.listy().snapshot(id).groups.empty() && row_inputs(f, first.viewport).empty() &&
              !f.services.components().contains(editor.component) &&
              f.inputs.mounted_inputs().size() == row_inputs(f, second_page.pagination).size(),
          "removed virtual groups leaked their old editors or unrelated input owners");
    f.services.dispose();
    check(!ref.bound() && !ref.scrollToGroup(u8"4") && f.nodes.size() == 0 && f.services.interactions().size() == 0,
          "virtual Listy leaked group ref, node or input resources");
}

void late_nested_virtual_mounts() {
    Fixture f;
    std::vector<ListyItem<String>> outer;
    for (int i = 0; i < 12; ++i) {
        const auto key = String::from_utf8(std::to_string(i)).value();
        outer.emplace_back(key, key);
    }
    f.services.mount(Content{[&] {
        Listy(ListyProps<String>{}
                  .items(outer)
                  .height(120)
                  .virtualized()
                  .virtualization({1000, 32, 0})
                  .itemRender([](ListyItemView<String>) {
                      Listy(ListyProps<String>{}
                                .items(std::vector<ListyItem<String>>{{u8"inner", u8"value"}})
                                .height(20)
                                .itemRender([](ListyItemView<String>) { Input(InputProps{}); }));
                  }));
    }});
    f.synchronize();
    check(f.services.listy().mounted().size() > 2, "measured virtualization did not mount newly exposed rows");
    for (auto id : f.services.listy().mounted()) {
        check(!f.services.listy().snapshot(id).items.empty(),
              "late nested Listy needed a second synchronization to reconcile");
    }
    f.services.dispose();
    check(f.inputs.mounted_inputs().empty() && f.nodes.size() == 0, "late nested Listy cleanup leaked");
}

void table_listy_popup_one_sync(bool outer_table) {
    Fixture f;
    ListyRef ref;
    runtime::ComponentId nested_select;
    const auto select = [&] {
        Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}).open(true).layout(LayoutStyle{}.width(dp(140))));
        nested_select = f.services.select().mounted().back();
    };
    const auto list = [&] {
        std::vector<ListyItem<String>> items;
        for (int i = 0; i < 20; ++i) {
            const auto key = String::from_utf8(std::to_string(i)).value();
            items.emplace_back(key, key);
        }
        Listy(ListyProps<String>{}.items(items).height(160).virtualized().ref(ref).itemRender(
            [&](ListyItemView<String> view) {
                if (view.key == String{u8"0"}) {
                    select();
                } else {
                    Text(TextProps{}.content(view.data()));
                }
            }));
    };
    const auto table = [&] {
        std::vector<String> rows;
        for (int i = 0; i < 20; ++i) {
            rows.push_back(String::from_utf8(std::to_string(i)).value());
        }
        Table(TableProps<String>{}
                  .dataSource(rows)
                  .rowKey([](const String& value) { return value; })
                  .columns({TableColumn<String>{
                      u8"value", u8"Value",
                      [&](TableCell<String> cell) {
                          if (cell.key() == String{u8"0"}) {
                              select();
                          } else {
                              Text(TextProps{}.content(cell.row()));
                          }
                      }}.width(300)})
                  .scrollY(140)
                  .layout(LayoutStyle{}.width(dp(300))));
    };
    f.services.mount(Content{[&] {
        if (outer_table) {
            Table(TableProps<String>{}
                      .dataSource(std::vector<String>{u8"row"})
                      .rowKey([](const String& value) { return value; })
                      .columns({TableColumn<String>{u8"list", u8"List", [&](TableCell<String>) { list(); }}.width(300)})
                      .scrollY(120)
                      .layout(LayoutStyle{}.width(dp(300))));
        } else {
            Listy(ListyProps<String>{}
                      .items(std::vector<ListyItem<String>>{{u8"table", u8"value"}})
                      .height(180)
                      .ref(ref)
                      .layout(LayoutStyle{}.width(dp(320)))
                      .itemRender([&](ListyItemView<String>) { table(); }));
        }
    }});
    f.synchronize();
    const auto table_id = f.services.table().mounted().front();
    const auto select_id = nested_select;
    const auto first = f.services.select().snapshot(select_id);
    const auto body = f.services.table().snapshot(table_id).body;
    check(first.visible, "nested Table/Listy Select did not open");
    check(ref.scrollTo(4), "nested retained list scroll failed");
    check(f.inputs.dispatch(ScrollInputEvent{0, -.25F, body.x + body.width - 2, body.y + 2}),
          "nested Table wheel was not admitted");
    f.synchronize();
    const auto popup = f.services.select().snapshot(select_id);
    const auto& anchor_node = f.nodes.require(f.services.components().root(select_id));
    const auto anchor_y = anchor_node.bounds.y + anchor_node.translation.y;
    const auto gap = f.services.components().theme_scope(select_id)->snapshot().select().metrics.gap;
    const auto expected =
        popup.above ? anchor_y - gap - popup.popup.height : anchor_y + anchor_node.bounds.height + gap;
    if (!(popup.visible && std::abs(popup.popup.y - expected) < .01F &&
          f.services.table().snapshot(table_id).scroll_y > 0)) {
        std::cerr << "nested moved: outer_table=" << outer_table << " visible=" << popup.visible
                  << " popup=" << popup.popup.y << " expected=" << expected
                  << " table_scroll=" << f.services.table().snapshot(table_id).scroll_y
                  << " list_scroll=" << f.services.listy().snapshot(f.services.listy().mounted().front()).offset
                  << '\n';
    }
    check(popup.visible && popup.selector == first.selector && popup.rows == first.rows &&
              std::abs(popup.popup.y - expected) < .01F && f.services.table().snapshot(table_id).scroll_y > 0,
          "nested Table/Listy P consumer lagged behind its anchor in one synchronization");
    const auto list_id = f.services.listy().mounted().front();
    if (outer_table) {
        check(ref.scrollTo(100000), "inner Listy end request failed");
        f.synchronize();
        const auto edge = f.services.listy().snapshot(list_id);
        const auto table_before = f.services.table().snapshot(table_id);
        check(f.inputs.dispatch(ScrollInputEvent{0, -1, table_before.body.x + table_before.body.width - 2,
                                                 table_before.body.y + table_before.body.height / 2}),
              "inner Listy boundary wheel was not handled");
        f.synchronize();
        check(f.services.listy().snapshot(list_id).offset == edge.maximum &&
                  f.services.table().snapshot(table_id).scroll_y > table_before.scroll_y,
              "inner Listy edge did not hand off to the outer Table");
    } else {
        auto table_before = f.services.table().snapshot(table_id);
        const auto outer_before = f.services.listy().snapshot(list_id).offset;
        check(f.inputs.dispatch(ScrollInputEvent{0, -10000, table_before.body.x + table_before.body.width - 2,
                                                 table_before.body.y + 2}),
              "inner Table end wheel was not handled");
        f.synchronize();
        table_before = f.services.table().snapshot(table_id);
        check(table_before.scroll_y == table_before.maximum_y &&
                  f.services.listy().snapshot(list_id).offset == outer_before,
              "inner Table did not own its scroll before reaching the edge");
        check(f.inputs.dispatch(
                  ScrollInputEvent{0, -1, table_before.body.x + table_before.body.width - 2, table_before.body.y + 2}),
              "inner Table boundary wheel was not handled");
        f.synchronize();
        check(f.services.table().snapshot(table_id).scroll_y == table_before.maximum_y &&
                  f.services.listy().snapshot(list_id).offset > outer_before,
              "inner Table edge did not hand off to the outer Listy");
    }
    f.services.dispose();
    check(f.services.select().mounted().empty() && f.nodes.size() == 0 && f.services.interactions().size() == 0,
          "nested Table/Listy/Select cleanup leaked");
}

void grouped_failure_retirement(bool factory_failure) {
    Fixture f;
    Signal<std::vector<ListyItem<String>>> source{std::vector<ListyItem<String>>{{u8"item", u8"initial"}}};
    ListyRef ref;
    InputRef sibling;
    f.services.mount(Content{[&] {
        Listy(ListyProps<String>{}
                  .items(source)
                  .height(80)
                  .virtualized()
                  .sticky()
                  .ref(ref)
                  .group({[](const String&) { return String{u8"group"}; },
                          [&](ListyGroupView<String> view) {
                              Input(InputProps{});
                              if (factory_failure) {
                                  throw std::runtime_error("group factory failure");
                              }
                              Text(TextProps{}.content(bind([items = view.items()] {
                                  const auto values = detail::ListyPropBridge::read(items);
                                  if (values.front() == String{u8"fail"}) {
                                      throw std::runtime_error("group observer failure");
                                  }
                                  return values.front();
                              })));
                          }})
                  .itemRender([](ListyItemView<String>) { Input(InputProps{}); }));
        Input(InputProps{}.ref(sibling));
    }});
    bool failed{};
    try {
        f.synchronize();
        source.set(std::vector<ListyItem<String>>{{u8"item", u8"fail"}});
        f.synchronize();
    } catch (const std::runtime_error&) {
        failed = true;
    }
    f.synchronize();
    check(failed && f.services.listy().mounted().empty() && !ref.bound() && f.inputs.mounted_inputs().size() == 1 &&
              sibling.focus(),
          "failed group factory/observer retained private input or retired its sibling");
    f.services.dispose();
    check(f.nodes.size() == 0 && f.services.interactions().size() == 0 && f.services.surfaces().size() == 0,
          "failed group left logical scene or input resources");
}

void group_theme_domains() {
    Fixture f;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    int factories{};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Listy(ListyProps<String>{}
                            .items(std::vector<ListyItem<String>>{{u8"key", u8"value"}})
                            .height(100)
                            .sticky()
                            .group({[](const String&) { return String{u8"group"}; },
                                    [](ListyGroupView<String> view) { Text(TextProps{}.content(view.key)); }})
                            .itemRender([&](ListyItemView<String> view) {
                                ++factories;
                                Input(InputProps{}.defaultValue(view.key));
                            }));
              }});
    }});
    f.synchronize();
    const auto id = f.services.listy().mounted().front();
    const auto initial = f.services.listy().snapshot(id);
    const auto node = f.services.components().root(id);
    const auto measures = f.nodes.require(node).measure_count;
    ThemeConfig color;
    color.alias.color_background_container = Color::rgba8(30, 45, 60);
    theme.set(color);
    f.synchronize();
    check(factories == 1 && f.nodes.require(node).measure_count == measures &&
              f.services.listy().snapshot(id).groups.front().component == initial.groups.front().component,
          "Listy group background color measured or remounted retained content");
    color.seed.font_size = dp(20);
    theme.set(color);
    f.synchronize();
    check(factories == 1 && f.nodes.require(node).measure_count > measures &&
              f.services.listy().snapshot(id).items.front().component == initial.items.front().component,
          "Listy Theme metrics failed to remeasure retained rows");
    f.services.dispose();
}

void table_listy_direct_popover_one_sync(bool outer_table) {
    Fixture f;
    ListyRef ref;
    runtime::ComponentId popover;
    const auto content = [&] {
        Popover(PopoverProps{}.open(true).trigger(PopoverTriggerMode::Manual).layout(LayoutStyle{}.width(dp(140))),
                PopoverTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Trigger"); }); }},
                PopoverSlots{std::nullopt, PopoverContent{[] { Input(InputProps{}.defaultValue(u8"popup draft")); }}});
        popover = f.services.popover().mounted().back();
    };
    const auto list = [&] {
        std::vector<ListyItem<String>> rows;
        for (int i = 0; i < 20; ++i) {
            const auto key = String::from_utf8(std::to_string(i)).value();
            rows.emplace_back(key, key);
        }
        Listy(ListyProps<String>{}.items(rows).height(160).virtualized().ref(ref).itemRender(
            [&](ListyItemView<String> view) {
                if (view.key == String{u8"0"}) {
                    content();
                } else {
                    Text(TextProps{}.content(view.data()));
                }
            }));
    };
    const auto table = [&] {
        std::vector<String> rows;
        for (int i = 0; i < 20; ++i) {
            rows.push_back(String::from_utf8(std::to_string(i)).value());
        }
        Table(TableProps<String>{}
                  .dataSource(rows)
                  .rowKey([](const String& value) { return value; })
                  .columns({TableColumn<String>{
                      u8"value", u8"Value",
                      [&](TableCell<String> cell) {
                          if (cell.key() == String{u8"0"}) {
                              content();
                          } else {
                              Text(TextProps{}.content(cell.row()));
                          }
                      }}.width(300)})
                  .scrollY(140)
                  .layout(LayoutStyle{}.width(dp(300))));
    };
    f.services.mount(Content{[&] {
        if (outer_table) {
            Table(TableProps<String>{}
                      .dataSource(std::vector<String>{u8"row"})
                      .rowKey([](const String& value) { return value; })
                      .columns({TableColumn<String>{u8"list", u8"List", [&](TableCell<String>) { list(); }}.width(300)})
                      .scrollY(120)
                      .layout(LayoutStyle{}.width(dp(300))));
        } else {
            Listy(ListyProps<String>{}
                      .items(std::vector<ListyItem<String>>{{u8"table", u8"value"}})
                      .height(180)
                      .ref(ref)
                      .layout(LayoutStyle{}.width(dp(320)))
                      .itemRender([&](ListyItemView<String>) { table(); }));
        }
    }});
    f.synchronize();
    const auto before = f.services.popover().snapshot(popover);
    const auto popup_input = row_inputs(f, before.popup).front();
    const auto table_id = f.services.table().mounted().front();
    const auto body = f.services.table().snapshot(table_id).body;
    check(before.geometry.visible && ref.scrollTo(4), "nested retained Popover was not open/scrollable");
    check(f.inputs.dispatch(ScrollInputEvent{0, -.25F, body.x + body.width - 2, body.y + 2}),
          "Popover combination Table wheel rejected");
    f.synchronize();
    const auto view = f.services.popover().snapshot(popover);
    const auto& node = f.nodes.require(f.services.components().root(popover));
    const auto expected = node.bounds.y + node.translation.y;
    if (std::abs(view.geometry.anchor.y - expected) >= .01F) {
        std::cerr << "direct Popover: outer_table=" << outer_table << " anchor=" << view.geometry.anchor.y
                  << " expected=" << expected << '\n';
    }
    check(view.geometry.visible && view.popup == before.popup && std::abs(view.geometry.anchor.y - expected) < .01F &&
              view.geometry.anchor.y < before.geometry.anchor.y &&
              f.inputs.editors().require(popup_input.editor).value() == String{u8"popup draft"}.bytes(),
          "direct Popover lagged behind Table/Listy in the same synchronization");
    f.services.dispose();
    check(f.services.popover().mounted().empty() && f.inputs.mounted_inputs().empty() && f.nodes.size() == 0,
          "nested retained Popover cleanup leaked");
}

} // namespace

int main() {
    try {
        keyed_data_grid_and_lifecycle();
        controlled_paging_and_scroll();
        validation_and_failed_renderer();
        grouped_virtual_measured_retention();
        late_nested_virtual_mounts();
        table_listy_popup_one_sync(true);
        table_listy_popup_one_sync(false);
        grouped_failure_retirement(true);
        grouped_failure_retirement(false);
        group_theme_domains();
        table_listy_direct_popover_one_sync(true);
        table_listy_direct_popover_one_sync(false);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Listy assertions: " << assertions << "\n";
    return 0;
}
