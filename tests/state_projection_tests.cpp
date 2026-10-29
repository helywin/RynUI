#include "support/state_projection_fixture.hpp"
#include "support/allocation_probe.hpp"
#include <cstdio>
#include <exception>
#include <string_view>

namespace {
using namespace projection_test;

void tree_local() {
    for (const auto count : {128U, 512U, 1024U}) {
        Fixture f;
        const auto id = mount_tree(f, count);
        const auto initial = f.services.tree().snapshot(id);
        hover(f, initial.rows[0]);
        sync(f);
        auto before = tree_counts(f.services.tree(), id);
        auto rebuilds = f.services.scene_composer().diagnostics().rebuilds;
        hover(f, initial.rows[1]);
        sync(f);
        const auto after_hover = tree_counts(f.services.tree(), id);
        check(f.services.scene_composer().diagnostics().rebuilds == rebuilds,
              "moving Tree hover rebuilt scene structure");
        check(!before.available ||
                  (after_hover.projected == before.projected && after_hover.presented - before.presented == 2),
              "Tree hover visited unrelated projection/presentation rows");
        check(f.services.focus().request_focus(initial.rows[0].interaction, FocusModality::keyboard),
              "Tree initial keyboard focus failed");
        sync(f);
        before = tree_counts(f.services.tree(), id);
        rebuilds = f.services.scene_composer().diagnostics().rebuilds;
        f.services.focus().dispatch({Key::down, KeyAction::down});
        f.services.focus().dispatch({Key::down, KeyAction::up});
        sync(f);
        const auto after_focus = tree_counts(f.services.tree(), id);
        check(f.services.focus().state().focused == initial.rows[1].interaction &&
                  !f.services.interactions().require(initial.rows[0].interaction).tab_stop &&
                  f.services.interactions().require(initial.rows[1].interaction).tab_stop,
              "Tree local keyboard move lost focus or roving entry");
        check(!before.available ||
                  (after_focus.projected == before.projected && after_focus.presented - before.presented == 2),
              "Tree keyboard move reprojected or presented unrelated rows");
        check(f.services.scene_composer().diagnostics().rebuilds == rebuilds,
              "Tree keyboard move without scrolling rebuilt scene structure");
        const auto after = f.services.tree().snapshot(id);
        for (std::size_t i = 0; i < initial.rows.size(); ++i) {
            check(after.rows[i].component == initial.rows[i].component && after.rows[i].title == initial.rows[i].title,
                  "local Tree update changed a retained generation");
        }
    }
}

void tree_invalidation() {
    Fixture f;
    Signal<TreeKeys> expanded{TreeKeys{u8"parent"}};
    Signal<TreeKeys> checked{TreeKeys{u8"parent"}};
    Signal<bool> disabled{false};
    Signal<bool> visible{true};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Tree(
                      TreeProps{}.expandedKeys(expanded).checkedKeys(checked).checkable(),
                      {TreeItem{u8"parent",
                                u8"Parent",
                                {TreeItem{u8"a", u8"A"}.disabled(disabled), TreeItem{u8"b", u8"B"}.visible(visible)}}});
              }});
    }});
    sync(f);
    const auto id = f.services.tree().mounted().back();
    const auto initial = f.services.tree().snapshot(id);
    hover(f, initial.rows[1]);
    sync(f);
    disabled.set(true);
    sync(f);
    auto view = f.services.tree().snapshot(id);
    check(!view.rows[1].checked && view.rows[2].checked &&
              !f.services.interactions().require(view.rows[1].interaction).focusable,
          "hover cache hid a disabled conduction change");
    checked.set(TreeKeys{u8"b"});
    disabled.set(false);
    sync(f);
    view = f.services.tree().snapshot(id);
    check(view.rows[0].indeterminate && !view.rows[0].checked && view.rows[2].checked,
          "hover cache hid a checked/half-checked change");
    expanded.set(TreeKeys{});
    sync(f);
    check(!f.services.tree().snapshot(id).rows[1].shown, "hover cache hid collapsed children");
    expanded.set(TreeKeys{u8"parent"});
    visible.set(false);
    auto config = ThemeConfig{};
    config.tree.tokens.font_size = dp(21);
    config.tree.tokens.font_family = SystemFontFamily::ui_monospace;
    theme.set(config);
    sync(f);
    view = f.services.tree().snapshot(id);
    check(view.rows[1].shown && !view.rows[2].shown &&
              f.services.components().theme_scope(id)->snapshot().tree().metrics.font_size == 21 &&
              view.rows[1].title == initial.rows[1].title,
          "Tree model/Theme invalidation lost visibility, font or retained title");
    bool actual_font = false;
    for (const auto& text : f.services.text().mounted_texts()) {
        if (descendant(f, text.component, view.rows[1].title)) {
            actual_font |= f.scene.text_state(text.scene).content().bytes() == "A" &&
                           f.nodes.require(f.services.components().root(text.component)).measured_size.height > 20;
        }
    }
    check(actual_font && f.chains.contains(21) &&
              f.services.components().theme_scope(id)->snapshot().tree().metrics.font_family ==
                  SystemFontFamily::ui_monospace,
          "Tree Theme font did not reach actual retained text layout");
}

void tree_retry() {
    std::size_t failures = 0;
    for (std::size_t failure = 0; failure < 16; ++failure) {
        Fixture f;
        const auto id = mount_tree(f, 32);
        const auto initial = f.services.tree().snapshot(id);
        hover(f, initial.rows[0]);
        sync(f);
        const runtime::Size viewport{640, 600};
        const runtime::Rect full{0, 0, 640, 600};
        const runtime::Rect clipped{0, 40, 640, 560};
        auto& tree = f.services.tree();
        auto& effects = f.services.rounded_effects();
        tree.synchronize_auxiliary_geometry(viewport, clipped);
        static_cast<void>(effects.compact(full));
        const std::vector expected(effects.packed_instances().begin(), effects.packed_instances().end());
        tree.synchronize_auxiliary_geometry(viewport, full);
        static_cast<void>(effects.compact(full));
        // MSVC's noexcept empty-vector constructor allocates a small iterator proxy.
        // Inject data-buffer failures rather than failures that must terminate by contract.
        ryn_test::allocation::begin(failure, 64);
        try {
            tree.synchronize_auxiliary_geometry(viewport, clipped);
        } catch (const std::bad_alloc&) {
            ++failures;
        }
        ryn_test::allocation::end();
        tree.synchronize_auxiliary_geometry(viewport, clipped);
        static_cast<void>(effects.compact(full));
        check(std::ranges::equal(effects.packed_instances(), expected),
              "Tree retry lost clipped effect updates after allocation failure");
        check(f.services.tree().snapshot(id).rows[0].component == initial.rows[0].component,
              "Tree allocation retry changed retained identity");
    }
    check(failures > 0, "Tree allocation retry probe did not throw");
    std::cout << "tree_retry allocation failures=" << failures << " minimum_request=64\n";
}

void table_cache() {
    for (const auto count : {1000U, 5000U, 10000U}) {
        std::cout << "table_cache rows=" << count << " begin\n" << std::flush;
        TableFixture fixture(count);
        fixture.mount();
        auto& f = fixture.f;
        const auto id = fixture.id;
        const auto original = f.services.table().snapshot(id);
        const auto before = table_counts(f.services.table(), id);
        const auto comparisons = fixture.comparisons;
        fixture.selected.set({key(count - 1)});
        sync(f);
        check(fixture.comparisons == comparisons && f.selections.snapshot(original.rows.back().checkbox).checked,
              "Table selection repeated sorting or lost actual checkbox feedback");
        fixture.bordered.set(true);
        sync(f);
        check(fixture.comparisons == comparisons, "Table bordered repeated sorting");
        fixture.page.set(2);
        sync(f);
        auto view = f.services.table().snapshot(id);
        check(fixture.comparisons == comparisons && view.visible_keys.front() == key(count - 11),
              "Table page change did not reuse sorted order");
        fixture.page_size.set(7);
        sync(f);
        view = f.services.table().snapshot(id);
        check(fixture.comparisons == comparisons && view.visible_keys.size() == 7 &&
                  view.visible_keys.front() == key(count - 8),
              "Table page-size change repeated sorting or used a stale slice");
        const auto after = table_counts(f.services.table(), id);
        check(!before.available || (after.sorts == before.sorts && after.copied == before.copied),
              "Table presentation/page update copied source records");
        fixture.page.set(1);
        auto next = fixture.source.get();
        next[0].rank = -1;
        next[0].label = u8"Same-key replacement";
        fixture.source.set(next);
        sync(f);
        view = f.services.table().snapshot(id);
        check(fixture.comparisons > comparisons && view.visible_keys.front() == key(0) &&
                  view.rows[0].component == original.rows[0].component && view.rows[0].cells == original.rows[0].cells,
              "same-key record replacement did not invalidate sorting or retained cells");
        bool replacement_text = false;
        for (const auto& text : f.services.text().mounted_texts()) {
            replacement_text |= f.scene.text_state(text.scene).content().bytes() == "Same-key replacement";
        }
        check(replacement_text, "same-key replacement did not update actual cell text");
        next.erase(next.begin());
        next.push_back({u8"added", -2});
        fixture.source.set(next);
        sync(f);
        view = f.services.table().snapshot(id);
        check(view.visible_keys.front() == String{u8"added"} &&
                  !f.services.components().contains(original.rows[0].component) &&
                  view.rows[0].component == original.rows[1].component,
              "Table add/remove retained a stale record projection or generation");
        fixture.sort.set({u8"value", TableSortOrder::Descend});
        sync(f);
        check(f.services.table().snapshot(id).visible_keys.front() == key(1),
              "Table sort authority did not invalidate cache");
        std::cout << "table_cache rows=" << count << " passed\n" << std::flush;
    }
}

void table_reentry() {
    TableFixture fixture(32);
    fixture.mount();
    auto& f = fixture.f;
    const auto id = fixture.id;
    fixture.compare_callback = [&] {
        auto next = fixture.source.get();
        next[0].rank = 1000;
        next[0].label = u8"Reentrant replacement";
        fixture.source.set(next);
        fixture.sort.set({u8"value", TableSortOrder::Ascend});
        fixture.page.set(2);
    };
    fixture.sort.set({u8"value", TableSortOrder::Descend});
    sync(f);
    const auto view = f.services.table().snapshot(id);
    check(view.sort.order == TableSortOrder::Ascend && view.current == 2 && view.visible_keys.front() == key(21),
          "Table committed an obsolete projection after comparator reentry");
    const auto old = view.rows[0].component;
    const auto parent = *f.services.components().parent(id);
    bool replacement_mounted = false;
    fixture.compare_callback = [&] {
        check(f.services.destroy(id), "comparator could not destroy old Table");
        f.services.append_slot(parent, Content{[&] {
                                   Table(
                                       TableProps<Record>{}
                                           .dataSource(table_records(2))
                                           .rowKey([](const Record& row) { return row.key; })
                                           .columns({TableColumn<Record>{u8"replacement", u8"Replacement",
                                                                         [](TableCell<Record>) { Text(u8"Alive"); }}}));
                               }});
        replacement_mounted = true;
    };
    fixture.sort.set({u8"value", TableSortOrder::Descend});
    sync(f);
    check(replacement_mounted && !f.services.components().contains(id) && !f.services.components().contains(old) &&
              f.services.table().mounted().size() == 1 && f.services.table().mounted()[0] != id &&
              f.services.table().mounted()[0].index == id.index &&
              f.services.table().snapshot(f.services.table().mounted()[0]).visible_keys ==
                  std::vector<String>{key(0), key(1)},
          "obsolete Table refresh accessed or changed its replacement generation");
}

void table_cell_retirement() {
    for (bool during_mount : {false, true}) {
        TableFixture fixture(4);
        bool retire = false;
        fixture.cell_callback = [&](TableCell<Record> cell) {
            if (during_mount && retire && cell.key() == String{u8"added"}) {
                retire = false;
                fixture.f.services.destroy(fixture.id);
                return;
            }
            Text(TextProps{}.content(bind([&, cell] {
                const auto value = cell.value();
                if (!during_mount && retire && cell.key() == key(0)) {
                    retire = false;
                    fixture.f.services.destroy(fixture.id);
                }
                return value.label;
            })));
        };
        fixture.mount();
        const auto original = fixture.f.services.table().snapshot(fixture.id);
        auto next = fixture.source.get();
        retire = true;
        if (during_mount) {
            next.push_back({u8"added", -1});
        } else {
            next[0].label = u8"Retire from data binding";
        }
        fixture.source.set(next);
        sync(fixture.f);
        check(!retire && !fixture.f.services.components().contains(fixture.id) &&
                  fixture.f.services.table().mounted().empty(),
              "data/cell callback left the old Table generation alive");
        for (const auto& row : original.rows) {
            check(!fixture.f.services.components().contains(row.component), "cell retirement leaked a retained row");
        }
    }
}

void table_retry() {
    std::size_t failures = 0;
    for (std::size_t failure = 0; failure < 64; ++failure) {
        TableFixture fixture(12);
        auto records = fixture.source.get();
        for (auto& record : records) {
            record.key = String::from_utf8(std::string(512, 'k') + std::string{record.key.bytes()}).value();
        }
        fixture.source.set(records);
        fixture.page_size.set(records.size());
        fixture.mount();
        const auto original = fixture.f.services.table().snapshot(fixture.id);
        fixture.compare_callback = [failure] {
            ryn_test::allocation::begin(failure, 512);
        };
        try {
            fixture.sort.set({u8"value", TableSortOrder::Descend});
        } catch (const std::bad_alloc&) {
            ++failures;
        }
        ryn_test::allocation::end();
        fixture.bordered.set(true);
        sync(fixture.f);
        const auto view = fixture.f.services.table().snapshot(fixture.id);
        check(view.visible_keys.size() == records.size(), "Table retry retained an incomplete sorted order");
        for (std::size_t i = 0; i < records.size(); ++i) {
            check(view.visible_keys[i] == records[i].key && view.rows[i].component == original.rows[i].component,
                  "Table retry lost sorted rows or retained identity");
            if (i > 0) {
                check(bounds(fixture.f, view.rows[i - 1].component).y < bounds(fixture.f, view.rows[i].component).y,
                      "Table retry did not lay out the recovered sorted order");
            }
        }
    }
    check(failures > 0, "Table allocation retry probe did not throw");
    std::cout << "table_retry allocation failures=" << failures << " minimum_request=512\n";
    TableFixture fixture(12);
    fixture.mount();

    struct ComparatorFailure final {};

    fixture.compare_callback = [] {
        throw ComparatorFailure{};
    };
    bool rejected = false;
    try {
        fixture.sort.set({u8"value", TableSortOrder::Descend});
    } catch (const ComparatorFailure&) {
        rejected = true;
    }
    check(rejected, "Table did not preserve the comparator exception");
    fixture.bordered.set(true);
    sync(fixture.f);
    const auto view = fixture.f.services.table().snapshot(fixture.id);
    check(view.visible_keys.front() == key(0) &&
              bounds(fixture.f, view.rows[0].component).y < bounds(fixture.f, view.rows[1].component).y,
          "Table did not recover sorting/layout after a comparator exception");
}

void table_retained() {
    Fixture f;
    Signal<std::vector<Record>> source{table_records(4)};
    Signal<TableSort> sort{TableSort{u8"value", TableSortOrder::Ascend}};
    Signal<std::size_t> page{1};
    Signal<bool> bordered{false};
    std::uint64_t comparisons = 0;
    int mounts = 0;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 Input(InputProps{}.defaultValue(u8"Before"));
                 Table(TableProps<Record>{}
                           .dataSource(source)
                           .rowKey([](const Record& row) { return row.key; })
                           .columns({TableColumn<Record>{
                               u8"value", u8"Value",
                               [&](TableCell<Record> cell) {
                                   ++mounts;
                                   Input(InputProps{}.defaultValue(cell.key()));
                               }}.sorter([&](const Record& a, const Record& b) {
                               ++comparisons;
                               return a.rank < b.rank;
                           })})
                           .sort(sort)
                           .current(page)
                           .pageSize(std::size_t{2})
                           .bordered(bordered)
                           .showHeader(false));
                 Input(InputProps{}.defaultValue(u8"After"));
             }});
    }});
    sync(f);
    const auto id = f.services.table().mounted().back();
    const auto initial = f.services.table().snapshot(id);
    const std::vector initial_order(f.services.interactions().declaration_order().begin(),
                                    f.services.interactions().declaration_order().end());
    const auto before = f.inputs.mounted_inputs()[0].interaction;
    const auto after = f.inputs.mounted_inputs()[5].interaction;
    const auto row0 = f.inputs.mounted_inputs()[1];
    const auto row1 = f.inputs.mounted_inputs()[2];
    const auto row2 = f.inputs.mounted_inputs()[3];
    const auto row3 = f.inputs.mounted_inputs()[4];
    const auto expect_tabs = [&](std::vector<InteractionId> expected) {
        static_cast<void>(f.services.focus().request_focus(before, FocusModality::keyboard));
        check(f.services.focus().state().focused == before, "preceding Input focus failed");
        for (const auto interaction : expected) {
            f.services.focus().dispatch({Key::tab, KeyAction::down});
            f.services.focus().dispatch({Key::tab, KeyAction::up});
            sync(f);
            check(f.services.focus().state().focused == interaction,
                  "cached Table projection corrupted actual Tab order");
        }
        expected.pop_back();
        std::ranges::reverse(expected);
        expected.push_back(before);
        for (const auto interaction : expected) {
            f.services.focus().dispatch({Key::tab, KeyAction::down, KeyModifier::shift});
            f.services.focus().dispatch({Key::tab, KeyAction::up, KeyModifier::shift});
            sync(f);
            check(f.services.focus().state().focused == interaction,
                  "cached Table projection corrupted actual reverse Tab order");
        }
    };
    expect_tabs({row3.interaction, row2.interaction, after});
    check(f.services.focus().request_focus(row3.interaction, FocusModality::keyboard), "retained Input focus failed");
    check(bool(f.inputs.editors()
                   .require(row3.editor)
                   .select({0, f.inputs.editors().require(row3.editor).value().size()})),
          "retained Input selection failed");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"Retained draft"}, f.inputs.sessions().active()})),
          "retained Input text commit failed");
    const std::string draft{f.inputs.editors().require(row3.editor).value()};
    const auto session = f.inputs.sessions().active();
    const auto compared = comparisons;
    bordered.set(true);
    sync(f);
    check(comparisons == compared && f.services.focus().state().focused == row3.interaction &&
              f.inputs.sessions().active() == session && f.inputs.editors().require(row3.editor).value() == draft,
          "border refresh sorted, remounted or disturbed retained editor draft/session");
    page.set(2);
    sync(f);
    check(std::ranges::equal(f.services.interactions().declaration_order(), initial_order),
          "Table paging unnecessarily reordered retained interactions");
    check(f.services.focus().state().focused != row3.interaction && f.inputs.sessions().active() != session,
          "paging retained the hidden row editor focus/session");
    expect_tabs({row1.interaction, row0.interaction, after});
    page.set(1);
    sync(f);
    check(comparisons == compared && mounts == 4 && f.inputs.editors().require(row3.editor).value() == draft,
          "paging lost retained editor draft or repeated comparison");
    auto next = source.get();
    next[3].rank = 10;
    source.set(next);
    sync(f);
    expect_tabs({row2.interaction, row1.interaction, after});
    sort.set({u8"value", TableSortOrder::Descend});
    sync(f);
    expect_tabs({row3.interaction, row0.interaction, after});
    next.erase(next.begin() + 1);
    next.push_back({u8"added", 11});
    source.set(next);
    sync(f);
    const auto added = f.inputs.mounted_inputs().back().interaction;
    expect_tabs({added, row3.interaction, after});
    const auto final = f.services.table().snapshot(id);
    check(final.rows[0].component == initial.rows[0].component &&
              final.rows[2].component == initial.rows[3].component && mounts == 5 &&
              f.inputs.editors().require(row3.editor).value() == draft,
          "Table data/sort/page changes replaced retained cells or drafts");
}
} // namespace

int main(int argc, char** argv) {
    std::set_terminate([] {
        ryn_test::allocation::end();
        std::fputs("Unexpected termination in state projection test\n", stderr);
        std::_Exit(3);
    });
    try {
        const std::string_view test = argc > 1 ? argv[1] : "all";
        check(test == "all" || test == "tree_local" || test == "tree_invalidation" || test == "tree_retry" ||
                  test == "table_cache" || test == "table_reentry" || test == "table_cell_retirement" ||
                  test == "table_retained" || test == "table_retry",
              "unknown state projection test");
        if (test == "tree_local" || test == "all") {
            tree_local();
        }
        if (test == "tree_invalidation" || test == "all") {
            tree_invalidation();
        }
        if (test == "tree_retry" || test == "all") {
            tree_retry();
        }
        if (test == "table_cache" || test == "all") {
            table_cache();
        }
        if (test == "table_reentry" || test == "all") {
            table_reentry();
        }
        if (test == "table_cell_retirement" || test == "all") {
            table_cell_retirement();
        }
        if (test == "table_retained" || test == "all") {
            table_retained();
        }
        if (test == "table_retry" || test == "all") {
            table_retry();
        }
    } catch (const std::exception& error) {
        ryn_test::allocation::end();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
