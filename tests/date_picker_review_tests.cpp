#include "component/date_picker_component.hpp"
#include "component/dropdown_component.hpp"
#include "component/menu_component.hpp"
#include "component/select_component.hpp"
#include "component/table_component.hpp"
#include "theme/date_picker_theme.hpp"
#include "theme/theme_runtime.hpp"
#include "support/input_fixture.hpp"
#include <cmath>
#include <iostream>
#include <limits>

namespace {
using namespace ryn;
using namespace ryn::input;
using namespace ryn::theme_runtime;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <typename F> void rejects(F action, const char* message) {
    bool rejected = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, message);
}

void sync(Fixture& f) {
    check(f.services.layout_and_synchronize({1000, 1000}, {0, 0, 1000, 1000}), "review layout failed");
}

std::vector<int> records() {
    std::vector<int> values;
    for (int i = 0; i < 24; ++i) {
        values.push_back(i);
    }
    return values;
}

String label(int value) {
    return String::from_utf8(std::to_string(value)).value();
}

std::vector<TableColumn<int>> plain_columns() {
    return {TableColumn<int>{
        u8"number", u8"Number", [](TableCell<int> cell) {
            Text(TextProps{}.content(label(cell.value())));
        }}.width(420)};
}

void wheel(Fixture& f, runtime::Rect rect, float dy = -1, float dx = 0) {
    check(f.inputs.dispatch(ScrollInputEvent{dx, dy, rect.x + rect.width / 2, rect.y + rect.height / 2}),
          "review wheel unhandled");
    sync(f);
}

void footer_table_wheel() {
    Signal<bool> dropdown_open{false};
    SelectRef select_ref;
    DatePickerRef date_ref;
    ThemeConfig config;
    config.dropdown.tokens.max_height = dp(80);
    Fixture f;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  Table(TableProps<int>{}
                            .dataSource(records())
                            .rowKey([](int i) { return label(i); })
                            .columns({TableColumn<int>{
                                u8"owner", u8"Owner",
                                [&](TableCell<int> cell) {
                                    if (cell.value() != 0) {
                                        Text(TextProps{}.content(label(cell.value())));
                                        return;
                                    }
                                    DatePicker(
                                        DatePickerProps{}.defaultPanelDate({2024, 2, 1}).ref(date_ref),
                                        DatePickerFooter{[&] {
                                            std::vector<SelectOption> options;
                                            std::vector<MenuItem> items;
                                            for (int i = 0; i < 20; ++i) {
                                                options.push_back({i, label(i)});
                                                items.push_back({label(i), label(i)});
                                            }
                                            Table(
                                                TableProps<int>{}
                                                    .dataSource(records())
                                                    .rowKey([](int i) { return label(i); })
                                                    .columns({TableColumn<int>{
                                                                  u8"select", u8"Select",
                                                                  [&, options](TableCell<int> row) {
                                                                      if (row.value() == 0) {
                                                                          Select(SelectProps{}
                                                                                     .options(options)
                                                                                     .listHeight(64)
                                                                                     .ref(select_ref)
                                                                                     .layout(
                                                                                         LayoutStyle{}.width(dp(140))));
                                                                      } else {
                                                                          Text(TextProps{}.content(label(row.value())));
                                                                      }
                                                                  }}
                                                                  .width(160),
                                                              TableColumn<int>{
                                                                  u8"menu", u8"Menu",
                                                                  [&, items](TableCell<int> row) {
                                                                      if (row.value() == 0) {
                                                                          Dropdown(DropdownProps{}
                                                                                       .open(dropdown_open)
                                                                                       .menu(MenuProps{}.items(items)),
                                                                                   DropdownTrigger{[] {
                                                                                       Button(ButtonProps{},
                                                                                              [] { Text(u8"Menu"); });
                                                                                   }});
                                                                      } else {
                                                                          Text(TextProps{}.content(label(row.value())));
                                                                      }
                                                                  }}
                                                                  .width(260)})
                                                    .showHeader(false)
                                                    .scrollY(96));
                                        }});
                                }}.width(600)})
                            .scrollY(88)
                            .layout(LayoutStyle{}.width(dp(600))));
                  Table(TableProps<int>{}
                            .dataSource(records())
                            .rowKey([](int i) { return label(i); })
                            .columns(plain_columns())
                            .scrollY(900)
                            .layout(LayoutStyle{}.width(dp(600))));
              }});
    }});
    sync(f);
    check(date_ref.focus(), "DatePicker in ancestor Table could not focus");
    f.services.focus().dispatch({Key::down, KeyAction::down});
    f.services.focus().dispatch({Key::down, KeyAction::up});
    sync(f);
    sync(f);
    // Child mounting registers the retained footer Table before its containing Table.
    const auto tables = f.services.table().mounted();
    check(tables.size() == 3, "review Table fixture missing");
    const auto date = f.services.date_picker().mounted()[0];
    const auto popup = f.services.date_picker().snapshot(date);
    check(popup.visible, "DatePicker in ancestor Table did not open");
    const auto footer_table = tables[0];
    const auto ancestor_table = tables[1];
    const auto sibling_table = tables[2];
    const auto before = f.services.table().snapshot(footer_table);
    check(before.maximum_y > 0 && before.maximum_x > 0 && before.body.height == 96,
          "footer Table was not finite/scrollable");
    wheel(f, before.body);
    auto after = f.services.table().snapshot(footer_table);
    check(after.scroll_y > before.scroll_y, "R1: DatePicker boundary swallowed footer Table wheel");
    check(f.services.table().snapshot(ancestor_table).scroll_y == 0 &&
              f.services.table().snapshot(sibling_table).scroll_y == 0,
          "footer wheel escaped to ancestor/covered sibling");
    wheel(f, after.body, 0, 1);
    check(f.services.table().snapshot(footer_table).scroll_x > 0, "footer Table horizontal wheel swallowed");
    wheel(f, after.body, 10000, -10000);
    after = f.services.table().snapshot(footer_table);
    check(after.scroll_x == 0 && after.scroll_y == 0, "footer Table scroll reset");
    check(select_ref.focus(), "footer Select focus");
    f.services.focus().dispatch({Key::down, KeyAction::down});
    f.services.focus().dispatch({Key::down, KeyAction::up});
    sync(f);
    sync(f);
    const auto select = f.services.select().mounted()[0];
    const auto menu = f.services.select().snapshot(select);
    check(menu.visible, "child Select did not open");
    wheel(f, menu.popup);
    check(f.services.select().snapshot(select).scroll > 0 && f.services.table().snapshot(footer_table).scroll_y == 0,
          "calendar/Table stole child Select wheel");
    f.services.focus().dispatch({Key::escape, KeyAction::down});
    f.services.focus().dispatch({Key::escape, KeyAction::up});
    sync(f);
    dropdown_open.set(true);
    sync(f);
    sync(f);
    const auto dropdown = f.services.dropdown().mounted()[0];
    const auto drop = f.services.dropdown().snapshot(dropdown);
    check(drop.geometry.visible, "child Dropdown did not open");
    const auto menu_before = f.services.menu().snapshot(drop.menu);
    const auto footer_before_dropdown = f.services.table().snapshot(footer_table).scroll_y;
    const auto ancestor_before_dropdown = f.services.table().snapshot(ancestor_table).scroll_y;
    wheel(f, drop.geometry.bounds);
    check(f.services.menu().snapshot(drop.menu).scroll > menu_before.scroll &&
              f.services.table().snapshot(footer_table).scroll_y == footer_before_dropdown &&
              f.services.table().snapshot(ancestor_table).scroll_y == ancestor_before_dropdown,
          "calendar/Table stole child Dropdown wheel");
    dropdown_open.set(false);
    sync(f);
    sync(f);
    after = f.services.table().snapshot(footer_table);
    const auto ancestor_before_limits = f.services.table().snapshot(ancestor_table).scroll_y;
    wheel(f, after.body, -10000);
    after = f.services.table().snapshot(footer_table);
    check(after.scroll_y == after.maximum_y, "footer Table limit not reached");
    wheel(f, after.body);
    check(f.services.table().snapshot(ancestor_table).scroll_y == ancestor_before_limits &&
              f.services.table().snapshot(sibling_table).scroll_y == 0,
          "footer limit escaped popup shielding");
    const auto current_popup = f.services.date_picker().snapshot(date);
    wheel(f, {current_popup.bounds.x + 1, current_popup.bounds.y + 1, 1, 1});
    check(f.services.table().snapshot(ancestor_table).scroll_y == ancestor_before_limits &&
              f.services.table().snapshot(sibling_table).scroll_y == 0,
          "calendar padding escaped popup shielding");
    const auto sibling = f.services.table().snapshot(sibling_table);
    wheel(f, {sibling.body.x + sibling.body.width - 12, sibling.body.y + 20, 4, 4});
    check(f.services.table().snapshot(sibling_table).scroll_y > 0, "uncovered sibling Table lost wheel");
    const auto ancestor = f.services.table().snapshot(ancestor_table);
    wheel(f, {ancestor.body.x + ancestor.body.width - 12, ancestor.body.y + 10, 4, 4});
    check(f.services.table().snapshot(ancestor_table).scroll_y > ancestor_before_limits,
          "inline ancestor content lost wheel outside popup");
}

ThemeConfig compact_height(float height) {
    ThemeConfig config;
    config.algorithms = {ThemeAlgorithm::Compact};
    config.date_picker.algorithm = true;
    config.date_picker.seed.control_height = dp(height);
    return config;
}

void final_metrics() {
    rejects([] { (void)resolve_theme(compact_height(1)); }, "R1: negative derived DatePicker metrics published");
    const auto base = resolve_theme().date_picker();
    const std::array fields{
        &DatePickerMetricToken::panel_width, &DatePickerMetricToken::padding,     &DatePickerMetricToken::header_height,
        &DatePickerMetricToken::row_height,  &DatePickerMetricToken::cell_height, &DatePickerMetricToken::radius,
        &DatePickerMetricToken::gap,         &DatePickerMetricToken::font_size,   &DatePickerMetricToken::line_height};
    for (auto field : fields) {
        auto token = base;
        token.metrics.*field = -1;
        rejects([&] { detail::apply_date_picker_override(token, {}); }, "unvalidated final fallback metric");
        token = base;
        token.metrics.*field = std::numeric_limits<float>::infinity();
        rejects([&] { detail::apply_date_picker_override(token, {}); }, "nonfinite final fallback metric");
    }
    const float threshold = 4.0F + 4.0F / 3.0F;
    for (auto height : {1.0F, 4.0F, 5.0F, std::nextafter(threshold, 0.0F)}) {
        rejects([&] { (void)resolve_theme(compact_height(height)); }, "subminimum final derived metric published");
    }
    const auto accepted = resolve_theme(compact_height(threshold));
    check(accepted.date_picker().metrics.cell_height >= 1, "threshold metric was not valid");
    auto repaired = compact_height(1);
    repaired.date_picker.tokens.header_height = dp(8);
    repaired.date_picker.tokens.cell_height = dp(2);
    repaired.date_picker.tokens.panel_width = dp(200);
    check(resolve_theme(repaired).date_picker().metrics.cell_height == 2,
          "valid final overrides failed to repair derived metrics");
    auto root = ThemeScope::create_default();
    const auto identity = root->snapshot().identity();
    const auto generation = root->generation();
    rejects([&] { root->update(compact_height(1)); }, "reactive negative metrics accepted");
    check(root->snapshot().identity() == identity && root->generation() == generation,
          "invalid metrics partially published");
    rejects([&] { (void)ThemeScope::create(root, compact_height(1)); }, "initial child accepted negative metrics");
    root->update(compact_height(threshold));
    check(root->snapshot().date_picker().metrics.cell_height >= 1, "reactive recovery failed");
    Signal<ThemeConfig> config{compact_height(8)};
    Fixture f;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config),
              ThemeContent{[] { DatePicker(DatePickerProps{}.defaultPanelDate({2024, 2, 1}).defaultOpen()); }});
    }});
    sync(f);
    const auto date = f.services.date_picker().mounted()[0];
    const auto before = f.services.date_picker().snapshot(date);
    const auto scope = f.services.components().theme_scope(date);
    const auto old_identity = scope->snapshot().identity();
    rejects([&] { config.set(compact_height(1)); }, "opened calendar accepted invalid metrics");
    check(scope->snapshot().identity() == old_identity, "opened calendar published invalid Theme");
    sync(f);
    check(f.services.date_picker().snapshot(date).bounds == before.bounds, "rejected metrics altered opened calendar");
    config.set(compact_height(threshold));
    sync(f);
    const auto after = f.services.date_picker().snapshot(date);
    check(after.visible && after.input == before.input && after.cells == before.cells &&
              std::isfinite(after.bounds.height),
          "threshold calendar failed to remain retained/open");
    {
        Fixture initial;
        rejects(
            [&] {
                initial.services.mount(Content{[] {
                    Theme(ThemeProps{}.config(compact_height(1)), ThemeContent{[] {
                              DatePicker(DatePickerProps{}.defaultPanelDate({2024, 2, 1}).defaultOpen());
                          }});
                }});
            },
            "invalid initial opened calendar accepted");
        check(initial.services.date_picker().mounted().empty() && initial.services.interactions().size() == 0,
              "invalid initial calendar leaked");
    }
}

void inactive_algorithm_inheritance() {
    ThemeConfig parent_config;
    const auto purple = Color::rgba8(100, 20, 140);
    const auto green = Color::rgba8(20, 110, 60);
    parent_config.date_picker.tokens.selected = purple;
    const auto parent = resolve_theme(parent_config);
    ThemeConfig child_config;
    child_config.date_picker.seed.color_primary = green;
    child_config.date_picker.seed.control_height = dp(1);
    check(resolve_theme(child_config, &parent).date_picker() == parent.date_picker(),
          "R1: disabled component algorithm discarded inherited tokens");
    child_config.date_picker.tokens.text = Color::rgba8(30, 50, 70);
    check(resolve_theme(child_config, &parent).date_picker().colors.selected == purple,
          "inactive seed broke partial token inheritance");
    child_config.date_picker.seed.control_height = dp(32);
    child_config.date_picker.algorithm = true;
    check(resolve_theme(child_config, &parent).date_picker().colors.selected == green,
          "enabled local algorithm failed");
    auto root = ThemeScope::create_default();
    root->update(parent_config);
    child_config.date_picker.algorithm = false;
    auto child = ThemeScope::create(root, child_config);
    check(child->snapshot().date_picker().colors.selected == purple, "initial inherited scope lost parent token");
    child_config.date_picker.algorithm = true;
    child->update(child_config);
    check(child->snapshot().date_picker().colors.selected == green, "reactive algorithm enable failed");
    const auto blue = Color::rgba8(30, 70, 150);
    parent_config.date_picker.tokens.selected = blue;
    root->update(parent_config);
    check(child->snapshot().date_picker().colors.selected == green, "enabled child algorithm lost independence");
    child_config.date_picker.algorithm = false;
    child->update(child_config);
    check(child->snapshot().date_picker().colors.selected == blue,
          "reactive disabled algorithm failed to recover inheritance");
    Signal<ThemeConfig> outside{parent_config};
    Signal<ThemeConfig> inside{child_config};
    Fixture f;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(outside), ThemeContent{[&] {
                  Theme(ThemeProps{}.config(inside), ThemeContent{[] {
                            DatePicker(DatePickerProps{}
                                           .defaultValue(CivilDate{2024, 2, 29})
                                           .defaultPanelDate({2024, 2, 1})
                                           .defaultOpen());
                        }});
              }});
    }});
    sync(f);
    const auto date = f.services.date_picker().mounted()[0];
    const auto before = f.services.date_picker().snapshot(date);
    const auto scope = f.services.components().theme_scope(date);
    const auto selected_color = [&](Color expected) {
        check(scope->snapshot().date_picker().colors.selected == expected,
              "opened calendar Theme inheritance mismatch");
        bool found = false;
        for (auto surface : before.surfaces) {
            const auto range = f.services.surfaces().visual_range(surface);
            for (std::uint32_t i = 0; i < range.count; ++i) {
                const auto& q = f.services.surfaces().instances().at(range.first + i);
                if (q.opacity > 0 &&
                    q.color == std::array{expected.red(), expected.green(), expected.blue(), expected.alpha()}) {
                    found = true;
                }
            }
        }
        check(found, "opened calendar selected surface missed inherited token");
    };
    selected_color(blue);
    child_config.date_picker.algorithm = true;
    inside.set(child_config);
    sync(f);
    selected_color(green);
    child_config.date_picker.algorithm = false;
    inside.set(child_config);
    sync(f);
    selected_color(blue);
    check(f.services.date_picker().snapshot(date).cells == before.cells, "algorithm recovery remounted calendar");
}
} // namespace

int main(int argc, char** argv) {
    try {
        check(argc == 2, "review test requires a case");
        const std::string_view test = argv[1];
        if (test == "wheel") {
            footer_table_wheel();
        } else if (test == "metrics") {
            final_metrics();
        } else if (test == "inheritance") {
            inactive_algorithm_inheritance();
        } else {
            throw std::runtime_error("unknown review case");
        }
        std::cout << "DatePicker R1 " << test << " passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
