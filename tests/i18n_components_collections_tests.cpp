#include "component/descriptions_component.hpp"
#include "component/pagination_component.hpp"
#include "component/selection_component.hpp"
#include "component/select_component.hpp"
#include "component/table_component.hpp"
#include "component/transfer_component.hpp"
#include "runtime/prop_connection.hpp"
#include "support/i18n_test.hpp"
#include "support/input_fixture.hpp"

#include <limits>

namespace {
using namespace ryn;
using namespace ryn::input;
using ryn_test::i18n::require;

struct Fixture final : ryn_test::input_component::Fixture {
    detail::SelectionComponentHost selections{services};

    ~Fixture() {
        // Retire controls before the Input and Selection hosts that own their children.
        services.dispose();
    }
};

void sync(Fixture& f) {
    require(f.services.layout_and_synchronize({1000, 2000}, {0, 0, 1000, 2000}), "collection layout failed");
}

bool descendant(Fixture& f, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto current = std::optional{child}; current; current = f.services.components().parent(*current)) {
        if (*current == parent) {
            return true;
        }
    }
    return false;
}

bool text_present(Fixture& f, runtime::ComponentId owner, StringView value, bool active = false) {
    for (const auto& text : f.services.text().mounted_texts()) {
        if (descendant(f, text.component, owner) &&
            (!active || f.services.components().branch_active(text.component)) &&
            std::as_const(f.scene).text_state(text.scene).content() == value) {
            return true;
        }
    }
    return false;
}

detail::MountedInputComponent input_in(Fixture& f, runtime::ComponentId owner, bool active = false) {
    for (const auto input : f.inputs.mounted_inputs()) {
        if (descendant(f, input.component, owner) &&
            (!active || f.services.components().branch_active(input.component))) {
            return input;
        }
    }
    throw std::runtime_error("retained collection Input missing");
}

StringView placeholder(Fixture& f, runtime::ComponentId owner) {
    const auto input = input_in(f, owner);
    return std::as_const(f.scene).text_state(f.inputs.text_layers(input.component).placeholder).content();
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto& n = f.nodes.require(f.services.components().root(id));
    const auto x = n.bounds.x + n.translation.x + n.bounds.width / 2;
    const auto y = n.bounds.y + n.translation.y + n.bounds.height / 2;
    for (const auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary, x, y});
    }
    sync(f);
}

struct Record final {
    String key;
    String value;
    friend bool operator==(const Record&, const Record&) = default;
};

TableProps<Record> table_props() {
    return TableProps<Record>{}
        .rowKey([](const Record& row) { return row.key; })
        .columns({TableColumn<Record>{u8"value", u8"No data", [](TableCell<Record> cell) {
                                          Text(TextProps{}.content(bind([cell] { return cell.value().value; })));
                                      }}});
}

std::vector<TransferItem> transfer_items(std::size_t count) {
    std::vector<TransferItem> result;
    for (std::size_t i = 0; i < count; ++i) {
        result.push_back({static_cast<double>(i), String{u8"No data"}});
    }
    return result;
}

struct Family final {
    runtime::ComponentId pagination;
    runtime::ComponentId table;
    runtime::ComponentId transfer;
    runtime::ComponentId descriptions;
};

void mount_defaults(Fixture& f, Family& ids) {
    Pagination(PaginationProps{}.total(120).showQuickJumper().showSizeChanger());
    ids.pagination = f.services.pagination().mounted().back();
    Table(table_props().loading(true));
    ids.table = f.services.table().mounted().back();
    Transfer(TransferProps{}.showSearch(true).pagination(TransferPagination{true, 10, true, false}));
    ids.transfer = f.services.transfer().mounted().back();
    Descriptions(DescriptionsProps{}.column(1),
                 {DescriptionsItem{u8"literal", DescriptionsLabel{[] { Text(u8"No data"); }},
                                   DescriptionsContent{[] { Text(u8"Loading…"); }}}});
    ids.descriptions = f.services.descriptions().mounted().back();
}

void expect_language(Fixture& f, const Family& ids, bool chinese) {
    require(placeholder(f, ids.pagination) == (chinese ? String{u8"跳至"} : String{u8"Go to"}).view(),
            "Pagination placeholder did not consume its live group");
    require(text_present(f, ids.pagination, (chinese ? String{u8"10 条/页"} : String{u8"10 / page"}).view()),
            "Pagination native Select did not localize its selected integer label");
    require(text_present(f, ids.table, (chinese ? String{u8"加载中…"} : String{u8"Loading…"}).view(), true),
            "Table live loading label missing");
    require(text_present(f, ids.table, (chinese ? String{u8"暂无数据"} : String{u8"No data"}).view()),
            "Table retained empty label missing");
    require(text_present(f, ids.transfer, (chinese ? String{u8"源列表"} : String{u8"Source"}).view(), true) &&
                placeholder(f, f.services.transfer().snapshot(ids.transfer).search[0]) ==
                    (chinese ? String{u8"请输入搜索内容"} : String{u8"Search here"}).view(),
            "Transfer default title/search did not follow live context");
    require(text_present(f, ids.transfer, (chinese ? String{u8"暂无数据"} : String{u8"No data"}).view(), true),
            "Transfer default empty label missing");
    const auto pager = f.services.transfer().snapshot(ids.transfer).pagination[0];
    require(placeholder(f, pager) == (chinese ? String{u8"跳至"} : String{u8"Go to"}).view(),
            "Transfer internal Pagination sealed inherited locale");
    require(text_present(f, ids.descriptions, (chinese ? String{u8"："} : String{u8":"}).view(), true) &&
                text_present(f, ids.descriptions, String{u8"No data"}.view(), true) &&
                text_present(f, ids.descriptions, String{u8"Loading…"}.view(), true),
            "Descriptions failed separator inheritance or translated business slots");
}

void default_nested_and_two_windows() {
    Fixture f;
    Fixture other;
    Signal<LocaleId> language{locales::zh_CN};
    Family outer;
    Family inner;
    Family independent;
    int factories = 0;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                    ++factories;
                                    mount_defaults(f, outer);
                                    ConfigProvider(ConfigProviderProps{}.locale(locales::en_US),
                                                   ConfigProviderContent{[&] { mount_defaults(f, inner); }});
                                }});
                       }});
    }});
    other.services.mount(
        Content{[&] { Flex(FlexProps{}.vertical(true), FlexContent{[&] { mount_defaults(other, independent); }}); }});
    sync(f);
    sync(other);
    expect_language(f, outer, true);
    expect_language(f, inner, false);
    expect_language(other, independent, false);
    const auto original = f.services.descriptions().snapshot(outer.descriptions);
    language.set(locales::en_US);
    sync(f);
    expect_language(f, outer, false);
    language.set(locales::zh_CN);
    sync(f);
    expect_language(f, outer, true);
    expect_language(f, inner, false);
    expect_language(other, independent, false);
    require(factories == 1 &&
                f.services.descriptions().snapshot(outer.descriptions).items[0].content == original.items[0].content,
            "language switch rebuilt a family/Descriptions slot");
    f.services.dispose();
    expect_language(other, independent, false);
}

void explicit_locales_empty_strings_and_slots() {
    for (const bool empty : {false, true}) {
        Fixture f;
        Signal<LocaleId> language{locales::zh_CN};
        PaginationLocale pagination;
        TableLocale table;
        TransferLocale transfer;
        DescriptionsLocale descriptions;
        if (empty) {
            pagination.items_per_page = {};
            pagination.page_separator = {};
            pagination.jump_placeholder = {};
            table.empty = {};
            table.loading = {};
            transfer.titles = {};
            transfer.empty = {};
            transfer.search_placeholder = {};
            transfer.select_invert = {};
            descriptions.separator = {};
        }
        int page_slots = 0;
        int title_slots = 0;
        int empty_slots = 0;
        int label_slots = 0;
        runtime::ComponentId page;
        runtime::ComponentId grid;
        runtime::ComponentId move;
        runtime::ComponentId details;
        f.services.mount(Content{[&] {
            ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                               Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                        Pagination(PaginationProps{}
                                                       .locale(pagination)
                                                       .total(20)
                                                       .showQuickJumper()
                                                       .itemRender([&](PaginationItemContext) {
                                                           ++page_slots;
                                                           Text(u8"No data");
                                                       }));
                                        page = f.services.pagination().mounted().back();
                                        Table(table_props().locale(table).loading(true), TableEmpty{[&] {
                                                  ++empty_slots;
                                                  Text(u8"No data");
                                              }});
                                        grid = f.services.table().mounted().back();
                                        Transfer(TransferProps{}
                                                     .locale(transfer)
                                                     .showSearch(TransferSearchOptions{String{}})
                                                     .pagination(TransferPagination{true, 10, true, false})
                                                     .notFoundContent(String{})
                                                     .operations(std::array{String{}, String{u8"No data"}})
                                                     .title(TransferDirection::Left, TransferTitle{[&] {
                                                                ++title_slots;
                                                                Text(u8"Source");
                                                            }}));
                                        move = f.services.transfer().mounted().back();
                                        Descriptions(
                                            DescriptionsProps{}.locale(descriptions).colon(false).column(1),
                                            {DescriptionsItem{u8"a", DescriptionsLabel{[&] {
                                                                  ++label_slots;
                                                                  Text(u8"No data");
                                                              }},
                                                              DescriptionsContent{[] { Text(u8"literal"); }}}});
                                        details = f.services.descriptions().mounted().back();
                                    }});
                           }});
        }});
        sync(f);
        const auto factories = std::array{page_slots, title_slots, empty_slots, label_slots};
        const auto verify = [&] {
            require(placeholder(f, page) == pagination.jump_placeholder.view() &&
                        text_present(f, grid, table.loading.view(), true) &&
                        text_present(f, grid, String{u8"No data"}.view()) &&
                        text_present(f, move, String{u8"Source"}.view(), true) &&
                        placeholder(f, f.services.transfer().snapshot(move).search[0]).empty(),
                    "explicit English/empty Locale or concrete slot lost priority");
            const auto state = f.services.descriptions().snapshot(details);
            require(!f.services.components().branch_active(state.items[0].colon) &&
                        text_present(f, state.items[0].colon, descriptions.separator.view()) &&
                        factories == std::array{page_slots, title_slots, empty_slots, label_slots},
                    "explicit false/empty separator or retained factory contract failed");
            const auto pager = f.services.transfer().snapshot(move).pagination[0];
            require(placeholder(f, pager) == transfer.pagination.jump_placeholder.view(),
                    "explicit Transfer Locale did not reach its internal Pagination");
        };
        verify();
        language.set(locales::en_US);
        sync(f);
        verify();
        language.set(locales::zh_CN);
        sync(f);
        verify();
    }
}

void exact_counts_and_business_callbacks() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    int totals = 0;
    int labels = 0;
    int renders = 0;
    int filters = 0;
    runtime::ComponentId page;
    runtime::ComponentId counted;
    runtime::ComponentId custom;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                    Pagination(PaginationProps{}
                                                   .total(std::numeric_limits<std::size_t>::max())
                                                   .defaultPageSize(1)
                                                   .showSizeChanger(false)
                                                   .simple());
                                    page = f.services.pagination().mounted().back();
                                    Pagination(PaginationProps{}.total(30).showTotal([&](auto, auto) {
                                        ++totals;
                                        return String{u8"No data"};
                                    }));
                                    Transfer(TransferProps{}.dataSource(transfer_items(2)).defaultSelectedKeys({0.0}));
                                    counted = f.services.transfer().mounted().back();
                                    Transfer(TransferProps{}
                                                 .dataSource(transfer_items(1))
                                                 .showSearch(TransferSearchOptions{String{u8"No data"},
                                                                                   {String{u8"No"}, String{}}})
                                                 .render([&](const auto& item) {
                                                     ++renders;
                                                     return item.title;
                                                 })
                                                 .filterOption([&](String, const auto&, auto) {
                                                     ++filters;
                                                     return true;
                                                 })
                                                 .selectAllLabels([&](auto) {
                                                     ++labels;
                                                     return String{u8"No data"};
                                                 }));
                                    custom = f.services.transfer().mounted().back();
                                }});
                       }});
    }});
    sync(f);
    const auto integer = std::to_string(std::numeric_limits<std::size_t>::max());
    require(text_present(f, page, String::from_utf8("/ " + integer).value().view()),
            "Pagination count lost exact size_t/UInt precision");
    require(f.services.transfer().snapshot(counted).summaries[0] == String{u8"Select all · 1/2 items"},
            "English collection count did not use exact typed parameters");
    const auto callbacks = std::array{totals, labels, renders, filters};
    language.set(locales::zh_CN);
    sync(f);
    require(
        f.services.transfer().snapshot(counted).summaries[0] == String{u8"全选 · 已选 1 项，共 2 项"} &&
            f.services.transfer().snapshot(custom).summaries[0] == String{u8"全选 · No data"} &&
            callbacks == std::array{totals, labels, renders, filters} &&
            f.inputs.editors().require(input_in(f, f.services.transfer().snapshot(custom).search[0]).editor).value() ==
                "No" &&
            text_present(f, custom, String{u8"No data"}.view(), true),
        "locale reran showTotal/label/render/filter or translated business String");
}

void real_page_selection_scroll_and_controlled_refusal() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    Signal<std::size_t> page{1};
    Signal<std::vector<String>> selected{std::vector<String>{}};
    bool accept = false;
    int selections = 0;
    runtime::ComponentId grid;
    runtime::ComponentId pager;
    runtime::ComponentId move;
    std::vector<Record> records;
    for (int i = 0; i < 24; ++i) {
        records.push_back({String::from_utf8(std::to_string(i)).value(), String{u8"No data"}});
    }
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                    auto props = table_props();
                                    props.dataSource(records)
                                        .columns({TableColumn<Record>{u8"wide", u8"literal",
                                                                      [](TableCell<Record>) { Text(u8"No data"); }}
                                                      .width(1200)})
                                        .current(page)
                                        .pageSize(10)
                                        .scrollY(80)
                                        .rowSelection({selected,
                                                       [&](std::vector<String> keys) {
                                                           ++selections;
                                                           if (accept) {
                                                               selected.set(std::move(keys));
                                                           }
                                                       },
                                                       {}});
                                    Table(std::move(props));
                                    grid = f.services.table().mounted().back();
                                    Pagination(PaginationProps{}.total(24).current(page).pageSize(10).onChange(
                                        [&](auto value, auto) { page.set(value); }));
                                    pager = f.services.pagination().mounted().back();
                                    Transfer(TransferProps{}
                                                 .dataSource(transfer_items(24))
                                                 .listHeight(60)
                                                 .pagination(TransferPagination{true, 10, false, false}));
                                    move = f.services.transfer().mounted().back();
                                }});
                       }});
    }});
    sync(f);
    click(f, f.services.pagination().snapshot(pager).items.back());
    require(page.get() == 2 && f.services.table().snapshot(grid).current == 2,
            "real Pagination button did not control the Table page");
    auto state = f.services.table().snapshot(grid);
    const auto row =
        std::ranges::find_if(state.rows, [&](const auto& item) { return item.key == state.visible_keys[0]; });
    require(row != state.rows.end(), "current Table row missing");
    click(f, row->checkbox);
    require(selections == 1 && selected.get().empty(), "controlled selection refusal changed authority");
    language.set(locales::zh_CN);
    require(selected.get().empty() && selections == 1, "language switch accepted a refused selection");
    accept = true;
    click(f, row->checkbox);
    state = f.services.table().snapshot(grid);
    require(f.services.dispatch_scroll({2, -2, state.body.x + 20, state.body.y + 20}), "Table wheel did not scroll");
    sync(f);
    auto transfer = f.services.transfer().snapshot(move);
    const auto native_pager = transfer.pagination[0];
    click(f, f.services.pagination().snapshot(native_pager).items.back());
    transfer = f.services.transfer().snapshot(move);
    const auto shown = std::ranges::find_if(transfer.rows, [](const auto& item) { return item.shown && !item.target; });
    require(shown != transfer.rows.end(), "Transfer page has no actual row");
    click(f, shown->checkbox);
    const auto viewport = transfer.viewports[0];
    require(f.services.dispatch_scroll({0, -2, viewport.x + viewport.width - 2, viewport.y + 20}),
            "Transfer wheel did not scroll");
    sync(f);
    const auto before = f.services.table().snapshot(grid);
    const auto before_transfer = f.services.transfer().snapshot(move);
    const auto work = f.services.table().work(grid);
    require(before.scroll_x > 0 && before.scroll_y > 0 && before_transfer.pages[0] == 2 &&
                before_transfer.scroll[0] > 0 && !before_transfer.selected_keys.empty(),
            "retention fixture did not establish real page/selection/scroll state");
    const auto selected_before = selected.get();
    language.set(locales::en_US);
    sync(f);
    const auto after = f.services.table().snapshot(grid);
    const auto after_transfer = f.services.transfer().snapshot(move);
    const auto after_work = f.services.table().work(grid);
    require(before.current == after.current && before.visible_keys == after.visible_keys &&
                before.scroll_x == after.scroll_x && before.scroll_y == after.scroll_y &&
                before.rows[0].component == after.rows[0].component && selected.get() == selected_before &&
                before_transfer.pages == after_transfer.pages && before_transfer.scroll == after_transfer.scroll &&
                before_transfer.selected_keys == after_transfer.selected_keys &&
                before_transfer.rows[0].component == after_transfer.rows[0].component &&
                work.copied_records == after_work.copied_records && work.sort_passes == after_work.sort_passes &&
                work.visited_rows == after_work.visited_rows,
            "locale rebuilt collection projection or changed page/selection/scroll");
}

void rich_editors_retain_selection_preedit_undo_and_factories() {
    for (int family = 0; family < 4; ++family) {
        Fixture f;
        Signal<LocaleId> language{locales::en_US};
        int factories = 0;
        int changes = 0;
        runtime::ComponentId owner;
        const auto editor_slot = [&] {
            ++factories;
            Input(InputProps{}.defaultValue(String{u8"initial"}).onChange([&](String) { ++changes; }));
        };
        f.services.mount(Content{[&] {
            ConfigProvider(
                ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                    if (family == 0) {
                        Pagination(PaginationProps{}.total(100).simple().itemRender([&](auto) {
                            ++factories;
                            Text(u8"literal page slot");
                        }));
                        owner = f.services.pagination().mounted().back();
                    } else if (family == 1) {
                        Table(table_props()
                                  .dataSource(std::vector<Record>{{u8"a", u8"No data"}})
                                  .columns({TableColumn<Record>{u8"rich", u8"No data", [&](auto) { editor_slot(); }}}));
                        owner = f.services.table().mounted().back();
                    } else if (family == 2) {
                        auto items = transfer_items(1);
                        items[0].content = TransferItemContent{editor_slot};
                        Transfer(TransferProps{}.dataSource(items));
                        owner = f.services.transfer().mounted().back();
                    } else {
                        Descriptions(DescriptionsProps{}.column(1),
                                     {DescriptionsItem{u8"a", DescriptionsLabel{[] { Text(u8"No data"); }},
                                                       DescriptionsContent{editor_slot}}});
                        owner = f.services.descriptions().mounted().back();
                    }
                }});
        }});
        sync(f);
        const auto input = input_in(f, owner, true);
        require(f.services.focus().request_focus(input.interaction, FocusModality::keyboard),
                "rich editor focus failed");
        auto& editor = f.inputs.editors().require(input.editor);
        require(bool(f.inputs.replace_range(input.component, {0, editor.value().size()}, String{u8"a中文z"}.view())),
                "real Input edit failed");
        require(bool(editor.select({7, 1})), "reverse selection setup failed");
        require(bool(editor.update_composition({String{u8"拼"}, {0, 1}, {}})), "preedit setup failed");
        const auto selection = editor.selection();
        const auto history = editor.history();
        const auto draft = std::string(editor.value());
        const auto counts = std::array{factories, changes, f.platform.starts, f.platform.stops, f.platform.cancels};
        language.set(locales::zh_CN);
        sync(f);
        require(input_in(f, owner, true).editor == input.editor && editor.value() == draft &&
                    editor.selection() == selection && editor.composition().active &&
                    editor.composition().text == String{u8"拼"}.bytes() &&
                    editor.history().undo_count == history.undo_count &&
                    editor.history().redo_count == history.redo_count && f.inputs.focused(input.component) &&
                    counts == std::array{factories, changes, f.platform.starts, f.platform.stops, f.platform.cancels},
                "locale lost rich Input identity/draft/reverse selection/preedit/undo/focus or reran factories");
        editor.cancel_composition();
        require(bool(editor.undo()) && editor.value() != draft && bool(editor.redo()) && editor.value() == draft,
                "retained undo/redo no longer works after locale projection");
        f.services.dispose();
        const auto calls = changes;
        language.set(locales::en_US);
        require(f.nodes.size() == 0 && f.services.interactions().size() == 0 && f.inputs.mounted_inputs().empty() &&
                    !f.inputs.sessions().active().valid() && !f.services.next_frame_deadline() && calls == changes,
                "disposed collection retained input blocks/session/deadline/observer work");
    }
}

void controlled_pagination_and_transfer_requests() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    Signal<std::size_t> page{1};
    Signal<TransferKeys> selected{TransferKeys{}};
    Signal<TransferKeys> target{TransferKeys{}};
    std::size_t requested_page = 0;
    TransferSelectInfo selection;
    TransferChangeInfo change;
    int requests = 0;
    runtime::ComponentId pager;
    runtime::ComponentId transfer;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                    Pagination(PaginationProps{}.total(30).current(page).onChange(
                                        [&](auto next, auto) { requested_page = next; }));
                                    pager = f.services.pagination().mounted().back();
                                    Transfer(TransferProps{}
                                                 .dataSource(transfer_items(2))
                                                 .selectedKeys(selected)
                                                 .targetKeys(target)
                                                 .onSelectChange([&](auto value) { selection = std::move(value); })
                                                 .onChange([&](auto value) {
                                                     ++requests;
                                                     change = std::move(value);
                                                 }));
                                    transfer = f.services.transfer().mounted().back();
                                }});
                       }});
    }});
    sync(f);
    click(f, f.services.pagination().snapshot(pager).items.back());
    require(requested_page == 2 && f.services.pagination().snapshot(pager).current == 1,
            "controlled Pagination rejected page was published");
    auto state = f.services.transfer().snapshot(transfer);
    click(f, state.rows[0].checkbox);
    require(selection.source_selected_keys == TransferKeys{0.0} &&
                f.services.transfer().snapshot(transfer).selected_keys.empty(),
            "controlled Transfer selection refusal changed authority");
    selected.set(selection.source_selected_keys);
    sync(f);
    click(f, state.move[1]);
    require(requests == 1 && change.target_keys == TransferKeys{0.0} &&
                f.services.transfer().snapshot(transfer).target_keys.empty(),
            "controlled Transfer move refusal changed authority");
    language.set(locales::zh_CN);
    sync(f);
    state = f.services.transfer().snapshot(transfer);
    require(f.services.pagination().snapshot(pager).current == 1 && state.target_keys.empty() &&
                state.selected_keys == TransferKeys{0.0} && requests == 1,
            "locale committed a refused request or emitted a new one");
}

LocaleCatalog collection_catalog(bool changed = false) {
    auto data = LocaleCatalog::builtin().pack(locales::zh_CN).data();
    if (changed) {
        data.image.preview = u8"Unused image label";
    }
    return LocaleCatalog({{LocaleId("xx-collect"), LocalePack(std::move(data)), locales::en_US}});
}

void equal_unused_group_and_idle() {
    Fixture f;
    Signal<LocaleCatalog> catalog{collection_catalog()};
    Family ids;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(LocaleId("xx-collect")).catalog(catalog),
                       ConfigProviderContent{
                           [&] { Flex(FlexProps{}.vertical(true), FlexContent{[&] { mount_defaults(f, ids); }}); }});
    }});
    sync(f);
    static_cast<void>(f.frames.consume_request());
    const auto requests = f.frames.counters().requests;
    const auto measure = f.nodes.require(f.services.components().root(ids.table)).measure_count;
    const auto work = f.services.table().work(ids.table);
    const auto theme = f.services.components().theme_scope(ids.table)->generation();
    for (int i = 0; i < 3; ++i) {
        catalog.set(collection_catalog());
    }
    catalog.set(collection_catalog(true));
    require(!f.frames.pending() && f.frames.counters().requests == requests &&
                f.nodes.require(f.services.components().root(ids.table)).measure_count == measure &&
                f.services.components().theme_scope(ids.table)->generation() == theme &&
                f.services.table().work(ids.table).visited_rows == work.visited_rows &&
                !f.services.next_frame_deadline(),
            "equal/unused pack group caused collection work, theme resolution, frame or deadline");
    f.services.dispose();
    static_cast<void>(f.frames.consume_request());
    const auto disposed_requests = f.frames.counters().requests;
    catalog.set(collection_catalog());
    require(!f.frames.pending() && f.frames.counters().requests == disposed_requests,
            "disposed family retained catalog subscriptions");
}

void inherited_count_plural_rule_is_a_live_dependency() {
    Fixture f;
    const auto catalog_for = [](IntegerPluralRule rule) {
        auto data = LocaleCatalog::builtin().pack(locales::en_US).data();
        data.plural = rule;
        data.transfer.count_one = u8"{selected} singleton of {total}";
        data.transfer.count_other = u8"{selected} batch of {total}";
        return LocaleCatalog({{LocaleId("xx-count"), LocalePack(std::move(data)), locales::en_US}});
    };
    Signal<LocaleCatalog> catalog{catalog_for(IntegerPluralRule::EnglishOneOther)};
    int renders = 0;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(LocaleId("xx-count")).catalog(catalog), ConfigProviderContent{[&] {
                           Transfer(TransferProps{}.dataSource(transfer_items(1)).render([&](const auto& item) {
                               ++renders;
                               return item.title;
                           }));
                       }});
    }});
    sync(f);
    const auto id = f.services.transfer().mounted().front();
    require(f.services.transfer().snapshot(id).summaries[0] == String{u8"Select all · 0 singleton of 1"},
            "inherited UInt count did not select the one branch");
    const auto reads = renders;
    catalog.set(catalog_for(IntegerPluralRule::OtherOnly));
    sync(f);
    require(f.services.transfer().snapshot(id).summaries[0] == String{u8"Select all · 0 batch of 1"} &&
                renders == reads,
            "a plural-only pack change missed the count or reran business rendering");
}

void reentrant_label_observers_retire_owner() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    Scope observations;
    runtime::ComponentId owner;
    std::optional<TransferListContext> retained;
    bool reenter = false;
    bool destroy = false;
    int labels = 0;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Transfer(
                               TransferProps{}
                                   .loading(true)
                                   .renderList([&](TransferListContext context) {
                                       retained = context;
                                       return TransferListContent{[] { Text(u8"business body"); }};
                                   })
                                   .selectAllLabel(TransferDirection::Left, [&](TransferSelectAllContext context) {
                                       return TransferSelectAllLabel{[&, context] {
                                           ++labels;
                                           Text(TextProps{}.content(context.label));
                                           static_cast<void>(
                                               detail::connect_prop(observations, context.label, [&](String value) {
                                                   if (value.bytes().find("全选") == std::string_view::npos) {
                                                       return;
                                                   }
                                                   if (destroy) {
                                                       require(f.services.destroy(owner), "observer retirement failed");
                                                   } else if (reenter) {
                                                       reenter = false;
                                                       language.set(locales::en_US);
                                                   }
                                               }));
                                       }};
                                   }));
                           owner = f.services.transfer().mounted().back();
                       }});
    }});
    sync(f);
    require(text_present(f, owner, String{u8"Loading…"}.view(), true), "Transfer loading label was bypassed");
    language.set(locales::zh_CN);
    sync(f);
    require(text_present(f, owner, String{u8"加载中…"}.view(), true), "live Transfer loading label did not translate");
    language.set(locales::en_US);
    sync(f);
    reenter = true;
    language.set(locales::zh_CN);
    sync(f);
    require(f.services.transfer().snapshot(owner).summaries[0] == String{u8"Select all · 0/0 items"} && labels == 1,
            "older locale publication won after a label observer reentered");
    destroy = true;
    language.set(locales::zh_CN);
    require(!f.services.components().contains(owner) && f.services.transfer().mounted().empty() &&
                f.services.pagination().mounted().empty() && f.services.text().mounted_texts().empty() &&
                f.inputs.mounted_inputs().empty() && !f.services.next_frame_deadline(),
            "label observer destruction leaked a collection subtree/deadline");
    observations.dispose();
    retained->on_item_select(0.0, true);
    language.set(locales::en_US);
    f.services.dispose();
    require(f.nodes.size() == 0 && f.services.interactions().size() == 0 && !f.inputs.sessions().active().valid(),
            "retired callback resurrected collection/input ownership");
}

void locale_getters_can_destroy_each_collection() {
    for (int family = 0; family < 4; ++family) {
        Fixture f;
        Signal<int> revision{0};
        runtime::ComponentId owner;
        bool retire = false;
        int getters = 0;
        const auto read = [&] {
            static_cast<void>(revision.get());
            ++getters;
            if (retire) {
                require(f.services.destroy(owner), "Locale getter could not retire its owner");
            }
        };
        f.services.mount(Content{[&] {
            ConfigProvider(ConfigProviderProps{}.locale(locales::zh_CN), ConfigProviderContent{[&] {
                               if (family == 0) {
                                   Pagination(PaginationProps{}.total(30).showQuickJumper().locale(bind([&] {
                                       read();
                                       return PaginationLocale{};
                                   })));
                                   owner = f.services.pagination().mounted().back();
                               } else if (family == 1) {
                                   Table(table_props().loading(true).locale(bind([&] {
                                       read();
                                       return TableLocale{};
                                   })));
                                   owner = f.services.table().mounted().back();
                               } else if (family == 2) {
                                   Transfer(TransferProps{}.loading(true).locale(bind([&] {
                                       read();
                                       return TransferLocale{};
                                   })));
                                   owner = f.services.transfer().mounted().back();
                               } else {
                                   Descriptions(DescriptionsProps{}.locale(bind([&] {
                                       read();
                                       return DescriptionsLocale{};
                                   })),
                                                {DescriptionsItem{u8"a", DescriptionsLabel{[] { Text(u8"Label"); }},
                                                                  DescriptionsContent{[] { Text(u8"literal"); }}}});
                                   owner = f.services.descriptions().mounted().back();
                               }
                           }});
        }});
        sync(f);
        retire = true;
        revision.set(1);
        const auto calls = getters;
        revision.set(2);
        require(!f.services.components().contains(owner) && f.inputs.mounted_inputs().empty() &&
                    f.services.text().mounted_texts().empty() && f.services.interactions().size() == 0 &&
                    !f.inputs.sessions().active().valid() && !f.services.next_frame_deadline() && getters == calls,
                "destroying Locale getter left a stale write, observer, input owner or deadline");
        f.services.dispose();
    }
}

void virtual_table_locale_keeps_projection_and_draft() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    Signal<bool> loading{false};
    std::vector<Record> records;
    for (int i = 0; i < 1000; ++i) {
        records.push_back({String::from_utf8(std::to_string(i)).value(), u8"literal draft"});
    }
    int factories = 0;
    f.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                Table(TableProps<Record>{}
                          .dataSource(records)
                          .rowKey([](const Record& row) { return row.key; })
                          .columns({TableColumn<Record>{u8"edit", u8"Literal header",
                                                        [&](TableCell<Record> cell) {
                                                            ++factories;
                                                            Input(InputProps{}.defaultValue(cell.value().value));
                                                        }}
                                        .width(240)
                                        .fixed(TableFixed::Start),
                                    TableColumn<Record>{u8"value", u8"Literal header",
                                                        [](TableCell<Record> cell) {
                                                            Text(TextProps{}.content(
                                                                bind([cell] { return cell.key(); })));
                                                        }}
                                        .width(1200)})
                          .virtualRows({60, 100, 2})
                          .scrollY(180)
                          .loading(loading));
            }});
    }});
    sync(f);
    const auto id = f.services.table().mounted().front();
    const auto initial = f.services.table().snapshot(id);
    require(initial.rows.size() < 10 && factories < 10, "localized virtual Table eagerly mounted all rows");
    const auto input = input_in(f, initial.rows.front().component, true);
    require(f.services.focus().request_focus(input.interaction, FocusModality::keyboard),
            "virtual Table editor did not focus");
    auto& editor = f.inputs.editors().require(input.editor);
    require(
        bool(f.inputs.replace_range(input.component, {0, editor.value().size()}, String{u8"edited draft"}.view())) &&
            bool(editor.select({8, 1})),
        "virtual Table draft/selection setup failed");
    sync(f);
    const auto work = f.services.table().work(id);
    const auto count = factories;
    const auto selection = editor.selection();
    language.set(locales::zh_CN);
    sync(f);
    const auto after = f.services.table().snapshot(id);
    const auto after_work = f.services.table().work(id);
    require(after.rows.front().component == initial.rows.front().component && factories == count &&
                editor.value() == "edited draft" && editor.selection() == selection &&
                text_present(f, id, String{u8"暂无数据"}.view()) && work.copied_records == after_work.copied_records &&
                work.sort_passes == after_work.sort_passes && work.visited_rows == after_work.visited_rows,
            "virtual Table locale reran projection/factories or discarded retained input state");
    loading.set(true);
    sync(f);
    require(text_present(f, id, String{u8"加载中…"}.view(), true), "virtual Table restored hard-coded loading text");
    f.services.dispose();
}

} // namespace

int main() {
    ryn_test::i18n::suppress_dialogs();
    try {
        const auto run = [](const char* name, auto test) {
            std::cerr << "i18n collections: " << name << '\n';
            test();
        };
        run("default_nested_and_two_windows", default_nested_and_two_windows);
        run("explicit_locales_empty_strings_and_slots", explicit_locales_empty_strings_and_slots);
        run("exact_counts_and_business_callbacks", exact_counts_and_business_callbacks);
        run("real_page_selection_scroll_and_controlled_refusal", real_page_selection_scroll_and_controlled_refusal);
        run("controlled_pagination_and_transfer_requests", controlled_pagination_and_transfer_requests);
        run("rich_editors_retain_selection_preedit_undo_and_factories",
            rich_editors_retain_selection_preedit_undo_and_factories);
        run("equal_unused_group_and_idle", equal_unused_group_and_idle);
        run("inherited_count_plural_rule_is_a_live_dependency", inherited_count_plural_rule_is_a_live_dependency);
        run("reentrant_label_observers_retire_owner", reentrant_label_observers_retire_owner);
        run("locale_getters_can_destroy_each_collection", locale_getters_can_destroy_each_collection);
        run("virtual_table_locale_keeps_projection_and_draft", virtual_table_locale_keeps_projection_and_draft);
        std::cout << "i18n collections: 11 cases passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "i18n collections: " << error.what() << '\n';
        return 1;
    }
}
