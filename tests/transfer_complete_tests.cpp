#include "component/transfer_component.hpp"
#include "component/selection_component.hpp"
#include "component/pagination_component.hpp"
#include "component/select_component.hpp"
#include "component/popover_component.hpp"
#include "component/modal_component.hpp"
#include "runtime/prop_connection.hpp"
#include "support/timeline_fixture.hpp"
#include <limits>
#include <thread>

namespace {
using namespace timeline_test;

struct Test final : Fixture {
    detail::SelectionComponentHost selections{services};
};

std::vector<TransferItem> data(std::size_t count = 8) {
    std::vector<TransferItem> out;
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back({static_cast<double>(i), String::from_utf8("Row " + std::to_string(i)).value(), {}, i == 7});
    }
    return out;
}

InteractionId interaction(Test& f, runtime::ComponentId id) {
    for (auto hit : f.services.interactions().declaration_order()) {
        const auto& record = f.services.interactions().require(hit);
        if (record.focusable && (record.component == id || descendant(f, record.component, id))) {
            return hit;
        }
    }
    throw std::runtime_error("missing native interaction");
}

void key(Test& f, Key k, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({k, KeyAction::down, modifiers});
    f.services.focus().dispatch({k, KeyAction::up, modifiers});
    sync(f);
}

bool text_present(Test& f, std::string_view text) {
    for (const auto& t : f.services.text().mounted_texts()) {
        if (f.services.components().branch_active(t.component) &&
            f.scene.text_state(t.scene).content().bytes().find(text) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

bool color_present(Test& f, Color color) {
    return std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                               [&](const auto& e) { return e.material.color == color; });
}

template <class F> void fails(F operation) {
    bool threw = false;
    try {
        operation();
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "invalid operation did not throw");
}

void theme_scene_and_retention() {
    Test f;
    ThemeConfig base;
    base.transfer.algorithm = true;
    base.transfer.seed.color_primary = Color::rgba8(170, 30, 20);
    base.transfer.tokens.background = Color::rgba8(30, 40, 50);
    base.transfer.tokens.header = Color::rgba8(40, 60, 80);
    base.transfer.tokens.footer = Color::rgba8(20, 80, 40);
    base.transfer.tokens.selected = Color::rgba8(60, 90, 120);
    base.transfer.tokens.error = Color::rgba8(200, 30, 50);
    base.transfer.tokens.item_height = dp(44);
    base.transfer.tokens.font_size = dp(16);
    base.transfer.tokens.line_height = dp(24);
    base.transfer.tokens.order = 3;
    Signal<ThemeConfig> theme{base};
    Signal<InputStatus> status{InputStatus::Default};
    int rich = 0;
    int unrelated = 0;
    auto rows = data();
    rows[0].content = TransferItemContent{[&] {
        ++rich;
        Input(InputProps{}.defaultValue(String{u8"draft"}));
    }};
    f.services.mount(Content{[&] {
        Button(ButtonProps{}, ButtonContent{[&] {
                   ++unrelated;
                   Text(u8"Unrelated");
               }});
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Transfer(TransferProps{}.dataSource(rows).defaultSelectedKeys({1.0}).status(status).footer(
                      TransferDirection::Left, TransferFooter{[] { Text(u8"Footer"); }}));
              }});
    }});
    sync(f);
    const auto id = f.services.transfer().mounted().front();
    const auto before = f.services.transfer().snapshot(id);
    check(color_present(f, *base.transfer.tokens.background) && color_present(f, *base.transfer.tokens.header) &&
              color_present(f, *base.transfer.tokens.footer) && color_present(f, *base.transfer.tokens.selected),
          "Transfer token palette not painted");
    check(bounds(f, before.rows[0].component).height == 44, "Transfer itemHeight not laid out");
    const auto resolved = f.services.components().theme_scope(id)->snapshot().transfer();
    check(f.buttons.snapshot(before.move[1]).presentation_background == resolved.colors.primary,
          "Transfer scoped primary palette did not reach actual native action");
    const auto input = f.inputs.mounted_inputs().back();
    check(f.services.components().theme_scope(input.component)->snapshot().text().font_size == 16,
          "Transfer typography not propagated");
    const auto node = f.nodes.require(f.services.components().root(id)).bounds;
    const auto measures = f.nodes.require(f.services.components().root(id)).measure_count;
    auto paint = base;
    paint.transfer.tokens.background = Color::rgba8(60, 30, 10);
    theme.set(paint);
    sync(f);
    check(color_present(f, *paint.transfer.tokens.background) &&
              f.nodes.require(f.services.components().root(id)).bounds == node &&
              f.nodes.require(f.services.components().root(id)).measure_count == measures,
          "Transfer paint update changed layout or stayed stale");
    paint.transfer.tokens.item_height = dp(52);
    theme.set(paint);
    sync(f);
    check(bounds(f, before.rows[0].component).height == 52 && rich == 1 && unrelated == 1 &&
              f.services.transfer().snapshot(id).rows[0].component == before.rows[0].component,
          "Transfer metric update remounted content");
    status.set(InputStatus::Error);
    sync(f);
    check(color_present(f, *base.transfer.tokens.error), "Transfer error border not painted");
    f.services.dispose();
    check(f.services.rounded_effects().live_count() == 0, "Transfer effect resources leaked");
}

void theme_scope_contract() {
    const auto baseline = resolve_theme();
    ThemeConfig c;
    c.transfer.tokens.list_width = dp(210);
    c.transfer.seed.color_primary = Color::rgba8(240, 30, 20);
    const auto inactive = resolve_theme(c);
    check(inactive.transfer().colors == baseline.transfer().colors && inactive.transfer().metrics.list_width == 210,
          "Transfer algorithm=false applied component seed");
    c.transfer.algorithm = true;
    const auto active = resolve_theme(c);
    check(active.transfer().colors.selected != baseline.transfer().colors.selected &&
              active.button() == baseline.button(),
          "Transfer component algorithm leaked or ignored seed");
    ThemeConfig nested;
    check(resolve_theme(nested, &active).transfer() == active.transfer(), "Transfer nested inheritance lost token");
    nested.inherit = false;
    check(resolve_theme(nested, &active).transfer() == baseline.transfer(), "Transfer inherit=false retained parent");
    for (auto size : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::max()}) {
        fails([&] {
            auto bad = c;
            bad.transfer.tokens.list_width = dp(size);
            static_cast<void>(resolve_theme(bad));
        });
    }
    auto scope = theme_runtime::ThemeScope::create_default();
    int color = 0;
    int metrics = 0;
    auto a = scope->capture(
        [&](auto phase) {
            check(phase == theme_runtime::DirtyPhase::paint_material, "Transfer color domain wrong");
            ++color;
        },
        [&] { static_cast<void>(scope->transfer_colors()); });
    auto b = scope->capture([&](auto) { ++metrics; }, [&] { static_cast<void>(scope->transfer_metrics()); });
    ThemeConfig paint;
    paint.transfer.tokens.background = Color::rgba8(1, 2, 3);
    check(scope->update(paint) && color == 1 && metrics == 0 && !scope->update(paint),
          "Transfer dirty identity/no-op contract wrong");
}

void geometry_and_paging() {
    Test f;
    Signal<float> width{240};
    Signal<float> height{92};
    Signal<float> spacing{2};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<TransferPagination> pagination{TransferPagination{true, 3, true, true, false, {2, 3, 6}}};
    Signal<std::vector<TransferItem>> rows{data(17)};
    int rich = 0;
    f.services.mount(Content{[&] {
        Transfer(TransferProps{}
                     .dataSource(rows)
                     .listWidth(width)
                     .listHeight(height)
                     .rowSpacing(spacing)
                     .size(size)
                     .pagination(pagination)
                     .footer(TransferDirection::Left, TransferFooter{[&] {
                                 ++rich;
                                 Input(InputProps{}.defaultValue(String{u8"footer draft"}));
                             }}));
    }});
    sync(f);
    const auto id = f.services.transfer().mounted().front();
    auto initial = f.services.transfer().snapshot(id);
    const auto pagination_id = initial.pagination[0];
    auto p = f.services.pagination().snapshot(pagination_id);
    click(f, p.items.back());
    check(f.services.transfer().snapshot(id).pages[0] == 2 &&
              std::ranges::count_if(f.services.transfer().snapshot(id).rows, [](const auto& r) { return r.shown; }) ==
                  3,
          "Transfer Pagination current callback incorrect");
    width.set(280);
    height.set(55);
    spacing.set(7);
    size.set(ControlSize::Large);
    sync(f);
    auto resized = f.services.transfer().snapshot(id);
    check(resized.sections[0].width > initial.sections[0].width && resized.viewports[0].height == 55 && rich == 1,
          "Transfer geometry did not relayout or remounted footer");
    check(bounds(f, resized.rows[4].component).y - bounds(f, resized.rows[3].component).y > 39,
          "Transfer row spacing/size had no geometry effect");
    auto page = pagination.get();
    page.page_size = 6;
    pagination.set(page);
    sync(f);
    check(f.services.pagination().snapshot(pagination_id).page_size == 6, "Transfer reactive page size ignored");
    rows.set(data(2));
    sync(f);
    check(f.services.transfer().snapshot(id).pages[0] == 1, "Transfer datasource shrink did not clamp page");
    const auto survivor = f.services.components().root_components().front();
    static_cast<void>(survivor);
    fails([&] { width.set(std::numeric_limits<float>::quiet_NaN()); });
    check(!f.services.components().contains(id) && f.services.transfer().mounted().empty() &&
              f.inputs.mounted_inputs().empty(),
          "invalid geometry left a partly published Transfer");
    f.services.dispose();
    for (int mode = 0; mode < 6; ++mode) {
        Test invalid;
        Signal<float> w{240};
        Signal<float> h{100};
        Signal<float> gap{0};
        Signal<TransferPagination> pages{TransferPagination{true, 3}};
        invalid.services.mount(Content{[&] {
            Transfer(TransferProps{}.dataSource(data()).listWidth(w).listHeight(h).rowSpacing(gap).pagination(pages));
        }});
        sync(invalid);
        const auto owner = invalid.services.transfer().mounted().front();
        fails([&] {
            if (mode == 0) {
                w.set(-1);
            }
            if (mode == 1) {
                h.set(std::numeric_limits<float>::infinity());
            }
            if (mode == 2) {
                gap.set(-1);
            }
            if (mode == 3 || mode == 4) {
                auto value = pages.get();
                if (mode == 3) {
                    value.page_size = 0;
                } else {
                    value.page_size_options = {0, 3};
                }
                pages.set(value);
            }
            if (mode == 5) {
                batch([&] {
                    w.set(std::numeric_limits<float>::max() / 8);
                    h.set(std::numeric_limits<float>::max() / 8);
                });
            }
        });
        check(!invalid.services.components().contains(owner) && invalid.nodes.size() == 0 &&
                  invalid.services.interactions().size() == 0,
              "invalid geometry combination left partial retained resources");
        invalid.services.dispose();
    }
}

void locale_slots_and_rtl() {
    Test f;
    TransferLocale zh;
    zh.titles = {String{u8"来源"}, String{u8"目标"}};
    zh.item_unit = u8"条";
    zh.items_unit = u8"条";
    zh.select_all = u8"全选";
    zh.deselect_all = u8"取消全选";
    zh.select_invert = u8"反选";
    zh.clear = u8"清空";
    zh.search_placeholder = u8"搜索";
    zh.empty = {String{u8"空来源"}, String{u8"空目标"}};
    zh.pagination.items_per_page = u8" 条/页";
    Signal<TransferLocale> locale{zh};
    Signal<TextDirection> direction{TextDirection::RightToLeft};
    Signal<bool> disabled{false};
    Signal<bool> show_all{true};
    Signal<std::array<String, 2>> operations{std::array{String{u8"Send"}, String{u8"Return selected items"}}};
    int labels = 0;
    int actions = 0;
    std::optional<TransferActionContext> retained;
    f.services.mount(Content{[&] {
        Transfer(TransferProps{}
                     .dataSource(data())
                     .locale(locale)
                     .direction(direction)
                     .disabled(disabled)
                     .showSelectAll(show_all)
                     .operations(operations)
                     .showSearch(true)
                     .selectAllLabel(TransferDirection::Left,
                                     [&](TransferSelectAllContext c) {
                                         return TransferSelectAllLabel{[&, c] {
                                             ++labels;
                                             Text(TextProps{}.content(c.label));
                                         }};
                                     })
                     .action(TransferDirection::Right, [&](TransferActionContext c) {
                         retained = c;
                         return TransferAction{[&, c] {
                             ++actions;
                             Button(ButtonProps{}.disabled(c.disabled).onClick(c.activate),
                                    ButtonContent{[] { Text(u8"移入"); }});
                         }};
                     }));
    }});
    sync(f);
    const auto id = f.services.transfer().mounted().front();
    auto s = f.services.transfer().snapshot(id);
    check(s.sections[0].x > s.sections[1].x && text_present(f, "来源") && text_present(f, "反选") &&
              text_present(f, "空目标") && s.summaries[0].bytes().find("条") != std::string_view::npos,
          "Transfer locale/RTL not presented");
    check(bounds(f, s.move[0]).width > 80, "Transfer custom operation text was clipped to icon width");
    const auto wide_action = bounds(f, s.move[0]).width;
    operations.set(std::array{String{u8"+"}, String{u8"-"}});
    sync(f);
    check(bounds(f, s.move[0]).width < wide_action, "reactive operations did not resize retained actions");
    const auto checkbox = s.rows[0].checkbox;
    const auto& children = f.services.components().children(checkbox);
    check(!children.empty(), "Checkbox label missing");
    const auto label = bounds(f, children.back());
    const auto allocation = bounds(f, checkbox);
    check(label.x <= allocation.x + 1, "inner Checkbox did not receive explicit RTL direction");
    click(f, checkbox);
    disabled.set(true);
    retained->activate();
    check(f.services.transfer().snapshot(id).target_keys.empty(), "custom action ignored current disabled gate");
    disabled.set(false);
    retained->activate();
    sync(f);
    check(f.services.transfer().snapshot(id).target_keys == TransferKeys{0.0},
          "retained custom action did not move row");
    show_all.set(false);
    sync(f);
    check(text_present(f, "0 / 7 条") && !f.services.components().branch_active(s.select_all[0]),
          "showSelectAll=false removed the localized count or left checkbox interactive");
    auto updated = zh;
    updated.titles[0] = u8"可用";
    updated.select_invert = u8"全部反选";
    locale.set(updated);
    direction.set(TextDirection::LeftToRight);
    sync(f);
    s = f.services.transfer().snapshot(id);
    check(s.sections[0].x < s.sections[1].x && text_present(f, "可用") && text_present(f, "全部反选") && labels == 1 &&
              actions == 1,
          "reactive locale/direction remounted slots or stayed stale");
    f.services.dispose();
    retained->activate();
    std::thread worker{[&] { retained->activate(); }};
    worker.join();
    check(f.nodes.size() == 0, "retained custom action survived unmount/thread gate");
}

void custom_contract_and_invalid_requests() {
    Test f;
    std::array<TransferListContext, 2>* unused = nullptr;
    static_cast<void>(unused);
    std::vector<TransferListContext> contexts;
    std::vector<TransferActionContext> actions;
    Signal<TransferKeys> target{TransferKeys{1.0}};
    Signal<std::vector<TransferItem>> rows{data()};
    Signal<bool> disabled{false};
    Signal<bool> one_way{false};
    int lists = 0;
    int footers = 0;
    f.services.mount(Content{[&] {
        Transfer(TransferProps{}
                     .dataSource(rows)
                     .targetKeys(target)
                     .disabled(disabled)
                     .oneWay(one_way)
                     .pagination(TransferPagination{true, 1, true})
                     .renderList([&](TransferListContext c) {
                         contexts.push_back(c);
                         return TransferListContent{[&, c] {
                             ++lists;
                             TextArea(TextAreaProps{}.defaultValue(String{u8"custom draft"}));
                         }};
                     })
                     .footer([&](TransferListContext) {
                         return TransferFooter{[&] {
                             ++footers;
                             Input(InputProps{}.defaultValue(String{u8"footer"}));
                         }};
                     })
                     .onChange([&](TransferChangeInfo info) { target.set(info.target_keys); }));
    }});
    sync(f);
    const auto id = f.services.transfer().mounted().front();
    check(!f.services.components().branch_active(f.services.transfer().snapshot(id).pagination[0]) &&
              detail::read_prop(contexts[0].filtered_items).size() == 7,
          "custom list incorrectly paged data");
    contexts[0].on_item_select_all({0.0, 1.0}, true);
    check(f.services.transfer().snapshot(id).selected_keys.empty(),
          "mixed cross-side request partially mutated selection");
    contexts[0].on_item_select(999.0, true);
    check(f.services.transfer().snapshot(id).selected_keys.empty(), "unknown key mutated selection");
    fails([&] { contexts[0].on_item_select_all({0.0, 0.0}, true); });
    check(f.services.transfer().snapshot(id).selected_keys.empty(), "duplicate callback request mutated selection");
    contexts[0].on_item_select(0.0, true);
    batch([&] {
        target.set({0.0, 1.0});
        contexts[0].on_item_select(0.0, false);
    });
    batch([&] {
        auto next = data();
        next[2].disabled = true;
        rows.set(next);
        contexts[0].on_item_select(2.0, true);
    });
    check(f.services.transfer().snapshot(id).selected_keys == TransferKeys{0.0},
          "batched target/data authority allowed stale custom selection");
    contexts[0].on_item_select(0.0, false);
    check(f.services.transfer().snapshot(id).selected_keys == TransferKeys{0.0},
          "retained list callback crossed dynamically changed side");
    disabled.set(true);
    contexts[1].on_item_select(0.0, false);
    check(f.services.transfer().snapshot(id).selected_keys == TransferKeys{0.0}, "custom list bypassed disabled gate");
    disabled.set(false);
    check(lists == 2 && footers == 2, "custom list/footer remounted on reactive update");
    one_way.set(true);
    contexts[0].on_item_remove({0.0});
    check(target.get() == TransferKeys({0.0, 1.0}), "source custom remove crossed side");
    contexts[1].on_item_remove({0.0});
    check(target.get() == TransferKeys{1.0}, "custom oneWay remove callback did not update target authority");
    one_way.set(false);
    contexts[1].on_item_remove({1.0});
    check(target.get() == TransferKeys{1.0}, "retained custom remove ignored oneWay state gate");
    f.services.dispose();
    contexts[0].on_item_select(2.0, true);
    contexts[1].on_item_remove({1.0});
}

void rich_input_lifecycle() {
    for (int gate = 0; gate < 8; ++gate) {
        Test f;
        auto rows = data(3);
        rows[0].content = TransferItemContent{[] { TextArea(TextAreaProps{}.defaultValue(String{u8"draft"})); }};
        Signal<bool> disabled{false};
        Signal<bool> read_only{false};
        Signal<bool> loading{false};
        Signal<bool> visible{true};
        Signal<bool> modal{false};
        Signal<float> width{520};
        Signal<TransferKeys> target{TransferKeys{}};
        Signal<std::vector<TransferItem>> items{rows};
        f.services.mount(Content{[&] {
            Transfer(TransferProps{}
                         .dataSource(items)
                         .disabled(disabled)
                         .readOnly(read_only)
                         .loading(loading)
                         .visible(visible)
                         .targetKeys(target)
                         .layout(LayoutStyle{}.width(bind([width] { return dp(width.get()); }))));
            Modal(ModalProps{}.open(modal).footer(ModalFooterMode::None), ModalContent{[] { Text(u8"Modal"); }});
        }});
        sync(f);
        const auto id = f.services.transfer().mounted().front();
        const auto original = f.services.transfer().snapshot(id).rows[0].component;
        const auto input = f.inputs.mounted_inputs().back();
        check(f.services.focus().request_focus(input.interaction, FocusModality::keyboard),
              "rich TextArea cannot focus");
        const auto stamp = f.inputs.sessions().active();
        check(bool(f.inputs.dispatch(CompositionChanged{String{u8"ni"}, {2, 0}, stamp})),
              "Transfer preedit did not start");
        key(f, Key::enter);
        check(f.inputs.editors().require(input.editor).composition().active &&
                  f.services.transfer().snapshot(id).target_keys.empty(),
              "Transfer consumed composing Enter");
        switch (gate) {
        case 0:
            disabled.set(true);
            break;
        case 1:
            read_only.set(true);
            break;
        case 2:
            loading.set(true);
            break;
        case 3:
            visible.set(false);
            break;
        case 4:
            modal.set(true);
            break;
        case 5:
            f.services.set_window_active(false);
            break;
        case 6:
            width.set(0);
            break;
        case 7:
            target.set({0.0});
            break;
        }
        sync(f);
        check(!f.inputs.editors().require(input.editor).composition().active && f.inputs.sessions().active() != stamp &&
                  f.services.focus().state().focused != input.interaction,
              "Transfer inactive/moved row retained preedit/focus/session");
        check(!bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stamp})), "stale row session committed");
        check(f.services.transfer().snapshot(id).rows[0].component == original &&
                  f.inputs.editors().require(input.editor).value() == "draft",
              "Transfer state gate discarded retained draft");
        f.services.dispose();
        check(f.nodes.size() == 0 && f.services.interactions().size() == 0 && !f.services.next_frame_deadline(),
              "Transfer lifecycle gate leaked resources/deadline");
    }
}

void filter_page_capture_and_reveal() {
    Test f;
    auto rows = data(20);
    rows[0].content = TransferItemContent{[] { Input(InputProps{}.defaultValue(String{u8"keep"})); }};
    Signal<TransferPagination> pagination{TransferPagination{false, 3, false}};
    f.services.mount(Content{
        [&] { Transfer(TransferProps{}.dataSource(rows).showSearch(true).pagination(pagination).listHeight(70)); }});
    sync(f);
    const auto id = f.services.transfer().mounted().front();
    auto s = f.services.transfer().snapshot(id);
    check(f.services.focus().request_focus(interaction(f, s.rows[5].checkbox), FocusModality::keyboard),
          "off-viewport row cannot focus for reveal");
    sync(f);
    check(f.services.transfer().snapshot(id).scroll[0] > 0, "focused row was not auto-revealed");
    const auto b = bounds(f, s.rows[5].checkbox);
    check(b.y >= s.viewports[0].y && b.y + b.height <= s.viewports[0].y + s.viewports[0].height + .01F,
          "focus reveal left row outside viewport");
    const auto snapshot = f.services.transfer().snapshot(id);
    const auto v = snapshot.viewports[0];
    check(f.services.dispatch_scroll({0, -10000, v.x + 5, v.y + 5}), "Transfer wheel not consumed");
    sync(f);
    check(!f.services.dispatch_scroll({0, -1, v.x + 5, v.y + 5}), "Transfer swallowed scroll at boundary");
    auto p = pagination.get();
    p.enabled = true;
    pagination.set(p);
    sync(f);
    s = f.services.transfer().snapshot(id);
    auto row_bounds = bounds(f, s.rows[0].checkbox);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, row_bounds.x + 4, row_bounds.y + 4});
    check(f.services.pointer().state(PointerIdentity::mouse())->capture.has_value(), "Transfer capture fixture failed");
    auto pager = f.services.pagination().snapshot(s.pagination[0]);
    // Controlled native pager request is activated without stealing the captured mouse pointer.
    f.services.focus().request_focus(interaction(f, pager.items.back()), FocusModality::keyboard);
    key(f, Key::enter);
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture &&
              !f.services.transfer().snapshot(id).rows[0].shown,
          "paging hid row without releasing capture");
    f.services.dispose();
}

void failure_and_reentry() {
    // User callbacks and reactive factories can fail after native controls were changed.
    for (int mode = 0; mode < 14; ++mode) {
        Test f;
        Signal<bool> trigger{false};
        Signal<std::vector<TransferItem>> rows{data()};
        Signal<TransferKeys> request_target{TransferKeys{}};
        Signal<TransferKeys> request_selected{TransferKeys{0.0}};
        runtime::ComponentId unrelated;
        f.services.mount(Content{[&] {
            Button(ButtonProps{}, ButtonContent{[] { Text(u8"healthy"); }});
            unrelated = f.buttons.mounted_buttons().back().component;
            auto props = TransferProps{}.dataSource(rows);
            if (mode == 0) {
                props.onSelectChange([](auto) { throw std::runtime_error("select failure"); });
            }
            if (mode == 1) {
                props.defaultSelectedKeys({0.0}).onChange([](auto) { throw std::runtime_error("move failure"); });
            }
            if (mode == 2) {
                props.render([&](const auto& item) {
                    if (trigger.get()) {
                        throw std::runtime_error("render failure");
                    }
                    return item.title;
                });
            }
            if (mode == 3) {
                props.showSearch(true).filterOption([&](auto, const auto&, auto) {
                    if (trigger.get()) {
                        throw std::runtime_error("filter failure");
                    }
                    return true;
                });
            }
            if (mode == 4) {
                props.listHeight(bind([trigger] {
                    if (trigger.get()) {
                        throw std::runtime_error("geometry binding failure");
                    }
                    return 100.0F;
                }));
            }
            if (mode == 5) {
                props.dataSource(bind([trigger] {
                    if (trigger.get()) {
                        throw std::runtime_error("data binding failure");
                    }
                    return data();
                }));
            }
            if (mode == 6) {
                props.rowKey([&](const auto& item) {
                    if (trigger.get()) {
                        throw std::runtime_error("row key failure");
                    }
                    return item.key;
                });
            }
            if (mode == 7) {
                props.selectAllLabels([&](TransferSelectAllInfo) {
                    if (trigger.get()) {
                        throw std::runtime_error("selection label failure");
                    }
                    return String{u8"Count"};
                });
            }
            if (mode == 8) {
                props.showSearch(true).onSearch([](auto, auto) { throw std::runtime_error("search failure"); });
            }
            if (mode == 9) {
                props.listHeight(40).onScroll([](auto) { throw std::runtime_error("scroll failure"); });
            }
            if (mode == 10) {
                props.pagination(TransferPagination{true, 2}).onPageChange([](auto) {
                    throw std::runtime_error("page failure");
                });
            }
            if (mode == 11 || mode == 13) {
                props.targetKeys(request_target);
            }
            if (mode == 12) {
                props.selectedKeys(request_selected);
            }
            Transfer(props);
        }});
        sync(f);
        const auto id = f.services.transfer().mounted().front();
        auto s = f.services.transfer().snapshot(id);
        if (mode == 3) {
            const auto search = f.inputs.mounted_inputs().front();
            f.services.focus().request_focus(search.interaction, FocusModality::keyboard);
            check(bool(f.inputs.dispatch(TextCommitted{String{u8"q"}, f.inputs.sessions().active()})),
                  "filter query fixture failed");
        }
        fails([&] {
            if (mode == 0) {
                click(f, s.rows[0].checkbox);
            } else if (mode == 1) {
                click(f, s.move[1]);
            } else if (mode == 8) {
                const auto search = f.inputs.mounted_inputs().front();
                f.services.focus().request_focus(search.interaction, FocusModality::keyboard);
                static_cast<void>(f.inputs.dispatch(TextCommitted{String{u8"q"}, f.inputs.sessions().active()}));
            } else if (mode == 9) {
                const auto v = s.viewports[0];
                static_cast<void>(f.services.dispatch_scroll({0, -.25F, v.x + 5, v.y + 5}));
            } else if (mode == 10) {
                click(f, f.services.pagination().snapshot(s.pagination[0]).items.back());
            } else if (mode >= 11) {
                batch([&] {
                    if (mode == 11) {
                        request_target.set({1.0, 1.0});
                    }
                    if (mode == 12) {
                        request_selected.set({99.0});
                    }
                    if (mode == 13) {
                        request_target.set({99.0});
                    }
                    click(f, s.rows[0].checkbox);
                });
            } else {
                trigger.set(true);
                if (mode == 2 || mode == 3 || mode == 6 || mode == 7) {
                    rows.set(data(9));
                }
            }
        });
        check(!f.services.components().contains(id) && f.services.components().contains(unrelated),
              "Transfer failure retired wrong generation or left scene live");
        sync(f);
        f.services.dispose();
        check(f.nodes.size() == 0 && f.services.rounded_effects().live_count() == 0,
              "Transfer failed transaction leaked resources");
    }
    Test f;
    Signal<TransferKeys> target{TransferKeys{}};
    Signal<TransferKeys> selected{TransferKeys{}};
    int callbacks = 0;
    f.services.mount(Content{[&] {
        Transfer(TransferProps{}
                     .dataSource(data())
                     .targetKeys(target)
                     .selectedKeys(selected)
                     .onSelectChange([&](TransferSelectInfo info) {
                         ++callbacks;
                         selected.set(info.source_selected_keys);
                     })
                     .onChange([&](TransferChangeInfo info) { target.set(info.target_keys); }));
    }});
    sync(f);
    const auto id = f.services.transfer().mounted().front();
    click(f, f.services.transfer().snapshot(id).rows[0].checkbox);
    click(f, f.services.transfer().snapshot(id).move[1]);
    check(target.get() == TransferKeys{0.0} && callbacks == 2, "controlled reentrant acceptance did not settle");
    f.services.dispose();
}

void nested_popup_and_tab_departure() {
    Test f;
    Signal<bool> visible{true};
    auto rows = data(3);
    rows[0].content = TransferItemContent{[] {
        Popover(PopoverProps{}.defaultOpen().trigger(PopoverTriggerMode::Click),
                PopoverTrigger{[] { Text(u8"Outer"); }},
                {.content = PopoverContent{[] {
                     Popover(PopoverProps{}.defaultOpen().trigger(PopoverTriggerMode::Click),
                             PopoverTrigger{[] { Text(u8"Inner"); }},
                             {.content =
                                  PopoverContent{[] { Input(InputProps{}.defaultValue(String{u8"popup draft"})); }}});
                 }}});
    }};
    f.services.mount(Content{[&] {
        Button(ButtonProps{}, ButtonContent{[] { Text(u8"Before Transfer"); }});
        Transfer(TransferProps{}.dataSource(rows).visible(visible));
        Button(ButtonProps{}, ButtonContent{[] { Text(u8"After Transfer"); }});
    }});
    sync(f);
    const auto id = f.services.transfer().mounted().front();
    const auto popups = f.services.popover().mounted();
    check(popups.size() == 2, "Transfer nested popup fixture missing");
    const auto inner = popups[0];
    const auto outer = popups[1];
    check(f.services.popover().snapshot(inner).geometry.visible &&
              f.services.popover().snapshot(outer).geometry.visible,
          "Transfer nested popup failed to open");
    auto editor = f.inputs.mounted_inputs().back();
    check(f.services.focus().request_focus(editor.interaction, FocusModality::keyboard), "popup editor cannot focus");
    key(f, Key::escape);
    check(!f.services.popover().snapshot(inner).geometry.visible &&
              f.services.popover().snapshot(outer).geometry.visible && f.services.components().contains(id),
          "first Escape did not close only innermost popup");
    key(f, Key::escape);
    check(!f.services.popover().snapshot(outer).geometry.visible, "second Escape did not close outer popup");
    // Traverse out of each end of the component through the real FocusManager.
    std::vector<InteractionId> order;
    for (auto hit : f.services.interactions().declaration_order()) {
        const auto& record = f.services.interactions().require(hit);
        if (record.focusable && record.tab_stop && record.eligible && f.services.interactions().branch_active(hit) &&
            descendant(f, record.component, id)) {
            order.push_back(hit);
        }
    }
    check(!order.empty(), "Transfer has no native traversal targets");
    f.services.focus().request_focus(order.back(), FocusModality::keyboard);
    key(f, Key::tab);
    const auto* after = f.services.interactions().find(*f.services.focus().state().focused);
    check(after && !descendant(f, after->component, id), "Tab was trapped inside Transfer");
    f.services.focus().request_focus(order.front(), FocusModality::keyboard);
    key(f, Key::tab, KeyModifier::shift);
    const auto* before = f.services.interactions().find(*f.services.focus().state().focused);
    check(before && !descendant(f, before->component, id), "Shift+Tab was trapped inside Transfer");
    visible.set(false);
    sync(f);
    check(!f.inputs.sessions().active().valid(), "hidden Transfer retained popup editor session");
    f.services.dispose();
}

void self_destroy_and_idle() {
    for (int mode = 0; mode < 9; ++mode) {
        Test f;
        Signal<std::vector<TransferItem>> rows{data(3)};
        runtime::ComponentId owner;
        runtime::ComponentId healthy;
        bool destroy = false;
        f.services.mount(Content{[&] {
            Button(ButtonProps{}, ButtonContent{[] { Text(u8"Survivor"); }});
            healthy = f.buttons.mounted_buttons().back().component;
            auto props = TransferProps{}
                             .dataSource(rows)
                             .render([&](const TransferItem& item) {
                                 if (destroy && mode == 0) {
                                     f.services.destroy(owner);
                                 }
                                 return item.title;
                             })
                             .onSelectChange([&](auto) {
                                 if (destroy && mode == 1) {
                                     f.services.destroy(owner);
                                 }
                             })
                             .onChange([&](auto) {
                                 if (destroy && mode == 2) {
                                     f.services.destroy(owner);
                                 }
                             });
            if (mode == 4) {
                props.dataSource(bind([&] {
                    auto value = rows.get();
                    if (destroy) {
                        f.services.destroy(owner);
                    }
                    return value;
                }));
            }
            if (mode == 5 || mode == 6) {
                auto keys = bind([&] {
                    static_cast<void>(rows.get());
                    if (destroy) {
                        f.services.destroy(owner);
                    }
                    return TransferKeys{};
                });
                if (mode == 5) {
                    props.targetKeys(keys);
                } else {
                    props.selectedKeys(keys);
                }
            }
            if (mode == 7) {
                props.searchDefaultValue(std::array{String{u8"q"}, String{}})
                    .filterOption([&](auto, const auto&, auto) {
                        if (destroy) {
                            f.services.destroy(owner);
                        }
                        return true;
                    });
            }
            if (mode == 8) {
                props.rowKey([&](const auto& item) {
                    if (destroy) {
                        f.services.destroy(owner);
                    }
                    return item.key;
                });
            }
            Transfer(props);
        }});
        sync(f);
        owner = f.services.transfer().mounted().front();
        auto original = f.services.transfer().snapshot(owner);
        static_cast<void>(f.frames.consume_request());
        f.services.transfer().select(owner, TransferDirection::Left, TransferSelection::Clear);
        check(!f.frames.pending(), "Transfer no-op selection requested another frame");
        destroy = true;
        if (mode == 0) {
            rows.set(data(4));
        }
        if (mode == 1) {
            click(f, original.rows[0].checkbox);
        }
        if (mode == 2) {
            click(f, original.rows[0].checkbox);
            click(f, original.move[1]);
        }
        if (mode >= 4) {
            rows.set(data(4));
        }
        if (mode == 3) {
            auto invalid = data(4);
            invalid.back().content = TransferItemContent{[] {
                Input(InputProps{});
                throw std::runtime_error("new row factory failure");
            }};
            fails([&] { rows.set(invalid); });
        }
        check(!f.services.components().contains(owner) && f.services.components().contains(healthy),
              "Transfer reentrant self-destruction corrupted another component");
        sync(f);
        f.services.dispose();
        check(f.nodes.size() == 0 && f.services.interactions().size() == 0, "self-destroy leaked resources");
    }
}

void pointer_and_keyboard_range() {
    Test f;
    f.services.mount(Content{[] { Transfer(TransferProps{}.dataSource(data()).listHeight(300)); }});
    sync(f);
    const auto id = f.services.transfer().mounted().front();
    auto s = f.services.transfer().snapshot(id);
    click(f, s.rows[0].checkbox);
    auto b = bounds(f, s.rows[3].checkbox);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, b.x + b.width / 2, b.y + b.height / 2, 0, true});
    }
    sync(f);
    check(f.services.transfer().snapshot(id).selected_keys == TransferKeys{0.0, 1.0, 2.0, 3.0},
          "Shift-click range did not select native keyed rows");
    f.services.focus().request_focus(interaction(f, s.rows[5].checkbox), FocusModality::keyboard);
    key(f, Key::space, KeyModifier::shift);
    check(f.services.transfer().snapshot(id).selected_keys == TransferKeys{0.0, 1.0, 2.0, 3.0, 4.0, 5.0},
          "Shift+Space range did not follow keyed anchor");
    f.services.dispose();
}

void controlled_page_size_requests() {
    Test f;
    TransferPagination initial{true, 3, true, true, false, {2, 3, 6}};
    initial.controlled_page_size = true;
    Signal<TransferPagination> config{initial};
    std::vector<TransferPageInfo> requests;
    bool accept = false;
    f.services.mount(Content{[&] {
        Transfer(TransferProps{}
                     .dataSource(data(17))
                     .direction(TextDirection::RightToLeft)
                     .listWidth(240)
                     .listHeight(100)
                     .pagination(config)
                     .onPageChange([&](TransferPageInfo info) {
                         requests.push_back(info);
                         if (accept) {
                             auto value = config.get();
                             value.page_size = info.page_size;
                             config.set(value);
                         }
                     }));
    }});
    sync(f);
    const auto id = f.services.transfer().mounted().front();
    const auto pager = f.services.transfer().snapshot(id).pagination[0];
    const auto size_owner = f.services.pagination().snapshot(pager).size_changer;
    const auto native_pager = f.services.pagination().snapshot(pager);
    check(bounds(f, native_pager.items.front()).x > bounds(f, native_pager.items.back()).x,
          "RTL Transfer native pager did not mirror action order");
    runtime::ComponentId select;
    for (auto candidate : f.services.select().mounted()) {
        if (descendant(f, candidate, size_owner)) {
            select = candidate;
        }
    }
    check(select.valid(), "Transfer native page-size Select missing");
    const auto choose = [&](std::size_t index) {
        auto snapshot = f.services.select().snapshot(select);
        check(inside(bounds(f, snapshot.selector), bounds(f, pager)),
              "Transfer size changer hit rect escaped list pager");
        click(f, snapshot.selector);
        snapshot = f.services.select().snapshot(select);
        check(snapshot.visible && snapshot.total_rows == 3, "Transfer configurable page-size options did not open");
        const auto row = std::ranges::find(snapshot.row_indices, index);
        check(row != snapshot.row_indices.end(), "Transfer page-size option missing");
        click(f, snapshot.rows.at(static_cast<std::size_t>(row - snapshot.row_indices.begin())));
    };
    choose(0);
    check(requests.size() == 1 && requests.back().page_size == 2 &&
              f.services.pagination().snapshot(pager).page_size == 3,
          "controlled Transfer page-size request overwrote authority");
    accept = true;
    choose(2);
    check(requests.size() == 2 && requests.back().page_size == 6 &&
              f.services.pagination().snapshot(pager).page_size == 6,
          "controlled Transfer page-size acceptance did not apply");
    f.services.dispose();
}

void retained_reorder_and_visibility() {
    Test f;
    auto initial = data(3);
    for (auto& item : initial) {
        item.title = u8"Same label";
    }
    Signal<std::vector<TransferItem>> rows{initial};
    Signal<bool> visible{true};
    runtime::ComponentId before;
    runtime::ComponentId after;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true).gap(dp(9)), [&] {
            Button(ButtonProps{}, ButtonContent{[] { Text(u8"Before"); }});
            before = f.buttons.mounted_buttons().back().component;
            Transfer(TransferProps{}.dataSource(rows).showSelectAll(false).visible(visible).selectAllLabel(
                TransferDirection::Left, [](auto) { return TransferSelectAllLabel{[] { Text(u8"Static label"); }}; }));
            Button(ButtonProps{}, ButtonContent{[] { Text(u8"After"); }});
            after = f.buttons.mounted_buttons().back().component;
        });
    }});
    sync(f);
    const auto id = f.services.transfer().mounted().front();
    const auto original = f.services.transfer().snapshot(id);
    const auto first = bounds(f, original.rows[0].component);
    const auto last = bounds(f, original.rows[2].component);
    std::reverse(initial.begin(), initial.end());
    rows.set(initial);
    sync(f);
    const auto changed = f.services.transfer().snapshot(id);
    check(changed.rows[0].component == original.rows[2].component &&
              bounds(f, changed.rows[0].component).y == first.y && bounds(f, changed.rows[2].component).y == last.y,
          "plain keyed reorder relied on text/count invalidation for layout");
    visible.set(false);
    sync(f);
    check(std::abs(bounds(f, after).y - (bounds(f, before).y + bounds(f, before).height + 9)) < .01F,
          "hidden Transfer kept parent layout space/gap");
    visible.set(true);
    sync(f);
    check(f.services.transfer().snapshot(id).rows[0].component == original.rows[2].component,
          "visibility restore remounted retained keyed row");
    f.services.dispose();
}

void initial_factory_context() {
    Test f;
    int lists = 0;
    int labels = 0;
    int actions = 0;
    TransferLocale locale;
    locale.item_unit = u8"条";
    locale.items_unit = u8"条";
    f.services.mount(Content{[&] {
        Transfer(TransferProps{}
                     .dataSource(data())
                     .defaultTargetKeys({1.0})
                     .defaultSelectedKeys({0.0})
                     .locale(locale)
                     .renderList([&](TransferListContext context) {
                         const bool right = context.direction == TransferDirection::Right;
                         check(detail::read_prop(context.filtered_items).size() == (right ? 1 : 7),
                               "custom factory received an empty provisional list");
                         check(detail::read_prop(context.selected_keys) == (right ? TransferKeys{} : TransferKeys{0.0}),
                               "custom factory received provisional selection");
                         ++lists;
                         return TransferListContent{[] { Text(u8"retained"); }};
                     })
                     .selectAllLabel(TransferDirection::Left,
                                     [&](TransferSelectAllContext context) {
                                         check(detail::read_prop(context.selected_count) == 1 &&
                                                   detail::read_prop(context.total_count) == 7,
                                               "selectAll factory received provisional counts");
                                         ++labels;
                                         return TransferSelectAllLabel{[] { Text(u8"Count"); }};
                                     })
                     .action(TransferDirection::Right, [&](TransferActionContext context) {
                         check(!detail::read_prop(context.disabled) && !detail::read_prop(context.label).empty(),
                               "action factory received provisional gate/locale");
                         ++actions;
                         return TransferAction{[context] {
                             Button(ButtonProps{}.disabled(context.disabled).onClick(context.activate),
                                    ButtonContent{[] { Text(u8"Move"); }});
                         }};
                     }));
    }});
    sync(f);
    check(lists == 2 && labels == 1 && actions == 1, "initial factories remounted");
    f.services.dispose();
}

void mount_factory_failures() {
    for (int mode = 0; mode < 5; ++mode) {
        Test f;
        fails([&] {
            f.services.mount(Content{[&] {
                auto props = TransferProps{}.dataSource(data());
                if (mode == 0) {
                    props.renderList([](auto) -> TransferListContent { throw std::runtime_error("list factory"); });
                }
                if (mode == 1) {
                    props.footer([](auto) -> TransferFooter { throw std::runtime_error("footer factory"); });
                }
                if (mode == 2) {
                    props.action(TransferDirection::Right,
                                 [](auto) -> TransferAction { throw std::runtime_error("action factory"); });
                }
                if (mode == 3) {
                    props.selectAllLabel(TransferDirection::Left, [](auto) -> TransferSelectAllLabel {
                        throw std::runtime_error("label factory");
                    });
                }
                if (mode == 4) {
                    auto rows = data();
                    rows[0].content = TransferItemContent{[] {
                        Input(InputProps{});
                        throw std::runtime_error("row factory");
                    }};
                    props.dataSource(rows);
                }
                Transfer(props);
            }});
        });
        check(f.nodes.size() == 0 && f.services.interactions().size() == 0 &&
                  f.services.rounded_effects().live_count() == 0,
              "Transfer mount failure leaked nested resources");
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::array<std::pair<std::string_view, void (*)()>, 15> contracts{
            {{"theme_scope", theme_scope_contract},
             {"theme_scene", theme_scene_and_retention},
             {"geometry", geometry_and_paging},
             {"locale_rtl", locale_slots_and_rtl},
             {"custom", custom_contract_and_invalid_requests},
             {"rich_lifecycle", rich_input_lifecycle},
             {"focus_scroll_capture", filter_page_capture_and_reveal},
             {"failure_reentry", failure_and_reentry},
             {"nested_popup", nested_popup_and_tab_departure},
             {"self_destroy_idle", self_destroy_and_idle},
             {"factory_failure", mount_factory_failures},
             {"range_selection", pointer_and_keyboard_range},
             {"page_size_requests", controlled_page_size_requests},
             {"reorder_visibility", retained_reorder_and_visibility},
             {"initial_context", initial_factory_context}}};
        std::size_t count = 0;
        for (const auto& [name, run] : contracts) {
            if (argc > 1 && name != argv[1]) {
                continue;
            }
            std::cout << name << '\n' << std::flush;
            run();
            ++count;
        }
        check(count != 0, "unknown Transfer contract");
        std::cout << "Transfer complete: " << count << " contracts passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
