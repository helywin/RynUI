#include "component/transfer_component.hpp"
#include "component/selection_component.hpp"
#include "support/timeline_fixture.hpp"
#include <thread>

namespace {
using namespace timeline_test;

struct TransferFixture final : Fixture {
    detail::SelectionComponentHost selections{services};

    ~TransferFixture() {
        services.dispose();
    }
};

std::vector<TransferItem> items() {
    return {{String{u8"a"}, u8"Alpha"}, {2.0, u8"杭州"}, {String{u8"off"}, u8"Disabled", {}, true}};
}

void controlled() {
    TransferFixture f;
    Signal<TransferKeys> target{TransferKeys{}};
    Signal<TransferKeys> selected{TransferKeys{}};
    TransferChangeInfo change;
    TransferSelectInfo selection;
    int changes = 0;
    f.services.mount(Content{[&] {
        Transfer(TransferProps{}
                     .dataSource(items())
                     .targetKeys(target)
                     .selectedKeys(selected)
                     .onChange([&](auto x) {
                         change = x;
                         ++changes;
                     })
                     .onSelectChange([&](auto x) { selection = x; })
                     .layout(LayoutStyle{}.width(dp(520))));
    }});
    sync(f);
    const auto id = f.services.transfer().mounted().front();
    auto s = f.services.transfer().snapshot(id);
    click(f, s.rows[0].checkbox);
    check(selection.source_selected_keys == TransferKeys{String{u8"a"}} &&
              f.services.transfer().snapshot(id).selected_keys.empty(),
          "controlled selection refusal failed");
    selected.set(selection.source_selected_keys);
    sync(f);
    click(f, s.move[1]);
    check(changes == 1 && change.direction == TransferDirection::Right &&
              change.moved_keys == TransferKeys{String{u8"a"}} &&
              f.services.transfer().snapshot(id).target_keys.empty(),
          "move event/control refusal failed");
    target.set(change.target_keys);
    sync(f);
    s = f.services.transfer().snapshot(id);
    check(s.rows[0].target, "target acceptance failed");
    click(f, s.move[0]);
    check(changes == 2 && change.direction == TransferDirection::Left, "reverse move missing");
    f.services.dispose();
    check(f.nodes.size() == 0 && f.services.interactions().size() == 0, "Transfer leaked");
}

void dynamic_rich() {
    TransferFixture f;
    int slots = 0;
    auto initial = items();
    initial[0].content = TransferItemContent{[&] {
        ++slots;
        Input(InputProps{}.defaultValue(String{u8"draft"}));
    }};
    Signal<std::vector<TransferItem>> data{initial};
    f.services.mount(Content{[&] {
        Transfer(
            TransferProps{}.dataSource(data).defaultSelectedKeys({String{u8"a"}}).layout(LayoutStyle{}.width(dp(520))));
    }});
    sync(f);
    const auto id = f.services.transfer().mounted().front();
    auto s = f.services.transfer().snapshot(id);
    const auto row = s.rows[0].component;
    const auto input = f.inputs.mounted_inputs().back();
    check(f.services.focus().request_focus(input.interaction, FocusModality::keyboard), "rich row Input cannot focus");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"草稿"}, f.inputs.sessions().active()})), "draft input failed");
    auto updated = initial;
    std::reverse(updated.begin(), updated.end());
    updated.back().title = u8"Renamed";
    data.set(updated);
    sync(f);
    s = f.services.transfer().snapshot(id);
    check(slots == 1 && s.rows.back().component == row && f.inputs.mounted_inputs().back().component == input.component,
          "reorder remounted rich input");
    click(f, s.move[1]);
    s = f.services.transfer().snapshot(id);
    check(s.rows.back().component == row && s.rows.back().target && slots == 1, "moving row lost draft identity");
    auto invalid = initial;
    invalid.push_back(initial[0]);
    bool threw = false;
    try {
        data.set(invalid);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw && f.services.transfer().snapshot(id).rows.back().component == row, "duplicate data did not roll back");
    f.services.dispose();
    check(!f.services.next_frame_deadline(), "Transfer idle requests frames");
}

void search_selection_wheel() {
    TransferFixture f;
    std::vector<TransferItem> data = items();
    for (int i = 3; i < 30; ++i) {
        auto label = String::from_utf8("row" + std::to_string(i)).value();
        data.push_back({static_cast<double>(i), label});
    }
    Signal<bool> disabled{false};
    int scrolled = 0;
    f.services.mount(Content{[&] {
        Transfer(TransferProps{}
                     .dataSource(data)
                     .disabled(disabled)
                     .showSearch(true)
                     .listHeight(100)
                     .onScroll([&](auto) { ++scrolled; })
                     .layout(LayoutStyle{}.width(dp(520))));
    }});
    sync(f);
    auto id = f.services.transfer().mounted().front();
    auto s = f.services.transfer().snapshot(id);
    f.services.transfer().select(id, TransferDirection::Left, TransferSelection::All);
    s = f.services.transfer().snapshot(id);
    check(s.selected_keys.size() == data.size() - 1 && !s.rows[2].selected, "select all crossed disabled isolation");
    f.services.transfer().select(id, TransferDirection::Left, TransferSelection::Invert);
    check(f.services.transfer().snapshot(id).selected_keys.empty(), "invert failed");
    const auto b = bounds(f, s.rows[0].checkbox);
    check(bool(f.inputs.dispatch(ScrollInputEvent{0, -0.125F, b.x + 2, b.y + 2})), "fractional wheel failed");
    sync(f);
    check(f.services.transfer().snapshot(id).scroll[0] == 12 && scrolled == 1, "scroll precision/event mismatch");
    const auto search = f.inputs.mounted_inputs().front();
    check(f.services.focus().request_focus(search.interaction, FocusModality::keyboard), "search focus failed");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"杭州"}, f.inputs.sessions().active()})),
          "Unicode search input failed");
    sync(f);
    s = f.services.transfer().snapshot(id);
    check(std::ranges::count_if(s.rows, [](const auto& r) { return r.shown; }) == 1 && s.rows[1].shown,
          "search did not filter lists");
    disabled.set(true);
    sync(f);
    f.services.transfer().select(id, TransferDirection::Left, TransferSelection::All);
    check(f.services.transfer().snapshot(id).selected_keys.empty(), "disabled modified selection");
    disabled.set(false);
    sync(f);
    click(f, s.rows[1].checkbox);
    check(f.services.transfer().snapshot(id).selected_keys == TransferKeys{2.0}, "reenabling failed");
    f.services.dispose();
}

void one_way_pagination_destroy() {
    TransferFixture f;
    auto data = items();
    Signal<bool> one_way{true};
    f.services.mount(Content{[&] {
        Transfer(TransferProps{}
                     .dataSource(data)
                     .oneWay(one_way)
                     .defaultTargetKeys({String{u8"a"}})
                     .pagination(TransferPagination{true, 1, true})
                     .onChange([&](auto) { f.services.dispose(); })
                     .layout(LayoutStyle{}.width(dp(520))));
    }});
    sync(f);
    auto id = f.services.transfer().mounted().front();
    auto s = f.services.transfer().snapshot(id);
    check(s.rows[0].target && s.rows[0].shown && !s.rows[2].shown, "pagination did not isolate current page");
    click(f, s.rows[0].remove);
    check(f.nodes.size() == 0 && f.services.interactions().size() == 0, "oneWay remove callback destruction failed");
}

void custom_and_reentrant() {
    TransferFixture f;
    std::vector<TransferListContext> contexts;
    Signal<TransferKeys> target{TransferKeys{}};
    Signal<TransferKeys> selected{TransferKeys{}};
    int lists = 0;
    int footers = 0;
    f.services.mount(Content{[&] {
        Transfer(TransferProps{}
                     .dataSource(items())
                     .targetKeys(target)
                     .selectedKeys(selected)
                     .renderList([&](TransferListContext context) {
                         contexts.push_back(context);
                         return TransferListContent{[&, context] {
                             ++lists;
                             Button(ButtonProps{}.onClick(
                                        [context] { context.on_item_select_all({String{u8"a"}, 2.0}, true); }),
                                    ButtonContent{[] { Text(u8"Select from custom list"); }});
                         }};
                     })
                     .footer(TransferDirection::Left, TransferFooter{[&] {
                                 ++footers;
                                 Input(InputProps{}.defaultValue(String{u8"footer draft"}));
                             }})
                     .onSelectChange([&](TransferSelectInfo info) {
                         info.source_selected_keys.insert(info.source_selected_keys.end(),
                                                          info.target_selected_keys.begin(),
                                                          info.target_selected_keys.end());
                         selected.set(info.source_selected_keys);
                     })
                     .onChange([&](TransferChangeInfo info) { target.set(info.target_keys); })
                     .layout(LayoutStyle{}.width(dp(520))));
    }});
    sync(f);
    const auto id = f.services.transfer().mounted().front();
    contexts[1].on_item_select(2.0, true);
    check(f.services.transfer().snapshot(id).selected_keys.empty(), "custom list crossed side isolation");
    std::thread worker{[&] { contexts[0].on_item_select(String{u8"a"}, true); }};
    worker.join();
    check(f.services.transfer().snapshot(id).selected_keys.empty(), "foreign custom callback changed UI");
    contexts[0].on_item_select(String{u8"a"}, true);
    sync(f);
    check(f.services.transfer().snapshot(id).selected_keys == TransferKeys{String{u8"a"}},
          "custom list reactive controlled acceptance failed");
    click(f, f.services.transfer().snapshot(id).move[1]);
    check(f.services.transfer().snapshot(id).target_keys == TransferKeys{String{u8"a"}} && lists == 2 && footers == 1,
          "custom list reentrant move remounted slots");
    f.services.dispose();
    contexts[0].on_item_select(String{u8"a"}, true);
    check(f.nodes.size() == 0, "retained custom callback survived teardown");
}

void retained_context_after_window() {
    std::optional<TransferListContext> retained;
    {
        TransferFixture f;
        f.services.mount(Content{[&] {
            Transfer(TransferProps{}.dataSource(items()).renderList([&](TransferListContext context) {
                retained = context;
                return TransferListContent{[] { Text(u8"custom"); }};
            }));
        }});
        sync(f);
    }
    retained->on_item_select(2.0, true);
    retained->on_item_select_all({String{u8"a"}}, true);
}

} // namespace

int main() {
    try {
        controlled();
        dynamic_rich();
        search_selection_wheel();
        one_way_pagination_destroy();
        custom_and_reentrant();
        retained_context_after_window();
        std::cout << "Transfer 6 scenarios passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
