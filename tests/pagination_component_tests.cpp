#include "component/pagination_component.hpp"
#include "support/input_fixture.hpp"
#include "support/bundled_icon_checks.hpp"
#include <iostream>
#include <limits>
#include <ryn/select.hpp>
#include "component/select_component.hpp"

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void click(Fixture& f, runtime::ComponentId wrapper) {
    const auto& node = f.nodes.require(f.services.components().root(wrapper));
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                       node.bounds.x + node.translation.x + node.bounds.width / 2,
                                       node.bounds.y + node.translation.y + node.bounds.height / 2});
    }
    f.synchronize(600, {0, 0, 600, 240});
}

void controlled() {
    Fixture f;
    Signal<std::size_t> total{200};
    Signal<std::size_t> page{5};
    Signal<std::size_t> size{10};
    Signal<bool> disabled{false};
    bool echo{};
    int runs{};
    std::vector<std::size_t> requests;
    f.services.mount(Content{[&] {
        ++runs;
        Pagination(PaginationProps{}.total(total).current(page).pageSize(size).disabled(disabled).onChange(
            [&](auto next, auto per_page) {
                check(per_page == size.get(), "callback pageSize");
                requests.push_back(next);
                if (echo) {
                    page.set(next);
                }
            }));
    }});
    f.synchronize(600, {0, 0, 600, 240});
    const auto id = f.services.pagination().mounted()[0];
    const auto original_count = f.services.components().component_count();
    auto s = f.services.pagination().snapshot(id);
    check(s.pages == 20 && s.current == 5 && s.items.size() <= 11, "initial pagination");
    ryn_test::require_bundled_icon(f.services, s.items.front(), IconName::LeftOutlined);
    ryn_test::require_bundled_icon(f.services, s.items.back(), IconName::RightOutlined);
    const auto jump_wrapper = s.items[2];
    const auto jump_icon = ryn_test::require_bundled_icon(f.services, jump_wrapper, IconName::EllipsisOutlined);
    click(f, s.items.back());
    check(requests == std::vector<std::size_t>{6} && f.services.pagination().snapshot(id).current == 5,
          "controlled refusal");
    echo = true;
    click(f, s.items.back());
    check(f.services.pagination().snapshot(id).current == 6, "controlled echo");
    disabled.set(true);
    click(f, s.items.front());
    check(requests.size() == 2, "disabled callback");
    disabled.set(false);
    total.set(12);
    f.synchronize(600, {0, 0, 600, 240});
    check(f.services.pagination().snapshot(id).current == 2 && requests.size() == 2, "shrink clamp callback");
    check(ryn_test::find_bundled_icon(f.services, jump_wrapper, IconName::EllipsisOutlined, false) == jump_icon,
          "Pagination did not hide the retained jump Icon when the slot became a page number");
    size.set(20);
    total.set(0);
    f.synchronize(600, {0, 0, 600, 240});
    s = f.services.pagination().snapshot(id);
    check(s.pages == 1 && s.current == 1 && s.items.size() == 3 && runs == 1 &&
              f.services.components().component_count() == original_count,
          "retained zero pagination");
    total.set(200);
    page.set(5);
    f.synchronize(600, {0, 0, 600, 240});
    check(ryn_test::require_bundled_icon(f.services, jump_wrapper, IconName::EllipsisOutlined) == jump_icon &&
              f.services.components().component_count() == original_count && requests.size() == 2,
          "Pagination jump/page transition remounted controls or emitted a callback");
    total.set(std::numeric_limits<std::size_t>::max());
    page.set(std::numeric_limits<std::size_t>::max());
    f.synchronize(600, {0, 0, 600, 240});
    s = f.services.pagination().snapshot(id);
    check(s.current == s.pages && s.items.size() <= 11, "overflow arithmetic");
    check(f.services.destroy(id), "destroy failed");
    f.synchronize();
    check(f.services.pagination().mounted().empty() && f.services.interactions().size() == 0 &&
              f.services.components().component_count() == 0 && f.scene.size() == 0,
          "Pagination leaked resources");
}

void keyboard_and_theme() {
    Fixture f;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<std::size_t> total{30};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme),
              ThemeContent{[&] { Pagination(PaginationProps{}.total(total).hideOnSinglePage()); }});
    }});
    f.synchronize(600, {0, 0, 600, 240});
    const auto id = f.services.pagination().mounted()[0];
    auto s = f.services.pagination().snapshot(id);
    click(f, s.items[2]);
    check(f.services.pagination().snapshot(id).current == 2, "uncontrolled click");
    for (auto action : {KeyAction::down, KeyAction::up}) {
        f.services.focus().dispatch({Key::enter, action});
    }
    f.synchronize(600, {0, 0, 600, 240});
    check(f.services.pagination().snapshot(id).current == 2, "same page keyboard changed page");
    s = f.services.pagination().snapshot(id);
    const auto next_wrapper = s.items.back();
    std::optional<InteractionId> next;
    for (auto hit : f.services.interactions().declaration_order()) {
        for (auto ancestor = std::optional{f.services.interactions().require(hit).component}; ancestor;
             ancestor = f.services.components().parent(*ancestor)) {
            if (*ancestor == next_wrapper) {
                next = hit;
                break;
            }
        }
    }
    check(next && f.services.focus().request_focus(*next, FocusModality::keyboard), "focus next page");
    for (auto action : {KeyAction::down, KeyAction::up}) {
        f.services.focus().dispatch({Key::space, action});
    }
    f.synchronize(600, {0, 0, 600, 240});
    check(f.services.pagination().snapshot(id).current == 3, "keyboard page navigation");
    ThemeConfig custom;
    custom.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
    custom.pagination.tokens.item_size = dp(28);
    custom.pagination.tokens.active = Color::rgba8(255, 0, 0);
    theme.set(custom);
    f.synchronize(600, {0, 0, 600, 240});
    check(f.nodes.require(f.services.components().root(s.items[0])).bounds.height == 28, "Pagination metrics override");
    check(resolve_theme(custom).pagination().colors.active == *custom.pagination.tokens.active,
          "Pagination color override");
    const auto inherited = resolve_theme({}, &f.services.components().theme_scope(id)->snapshot());
    check(inherited.pagination().metrics.item_size == 28, "nested Pagination inheritance");
    total.set(0);
    f.synchronize();
    check(f.services.pagination().snapshot(id).hidden, "hideOnSinglePage");
    f.services.dispose();
}

void token_contracts_and_callback_cleanup() {
    const auto token = resolve_theme();
    check(token.pagination().metrics.item_size == token.map().control_height &&
              token.pagination().metrics.item_size_small == token.map().control_height_small &&
              token.pagination().colors.active == token.map().color_primary,
          "locked Pagination token defaults");
    const auto scope = theme_runtime::ThemeScope::create_default();
    int colors{};
    int metrics{};
    auto color_subscription = scope->capture(
        [&](auto phase) {
            check(phase == theme_runtime::DirtyPhase::paint_material, "Pagination color phase");
            ++colors;
        },
        [&] { static_cast<void>(scope->pagination_colors()); });
    auto metric_subscription =
        scope->capture([&](auto) { ++metrics; }, [&] { static_cast<void>(scope->pagination_metrics()); });
    ThemeConfig config;
    config.pagination.tokens.active = Color::rgba8(2, 3, 4);
    check(scope->update(config) && colors == 1 && metrics == 0 && scope->snapshot().identity() != token.identity(),
          "Pagination token isolation/identity");
    check(scope->snapshot().diagnostic_json().find("\"pagination\"") != std::string::npos,
          "Pagination token serialization");
    const auto identity = scope->snapshot().identity();
    config.pagination.tokens.item_size = dp(-1);
    bool rejected{};
    try {
        scope->update(config);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && scope->snapshot().identity() == identity, "Pagination token validation rollback");
    Fixture f;
    runtime::ComponentId id;
    f.services.mount(Content{[&] {
        Pagination(PaginationProps{}.total(std::size_t{30}).onChange([&](auto, auto) { f.services.destroy(id); }));
    }});
    id = f.services.pagination().mounted()[0];
    f.synchronize(600, {0, 0, 600, 240});
    click(f, f.services.pagination().snapshot(id).items.back());
    check(f.services.pagination().mounted().empty() && f.services.interactions().size() == 0,
          "Pagination destruction callback leaked");
}

void hidden_pagination_flex_flow() {
    for (bool explicit_height : {false, true}) {
        Fixture f;
        Signal<std::size_t> total{10};
        Signal<bool> hide{true};
        f.services.mount(Content{[&] {
            Flex(FlexProps{}.vertical(true).gap(dp(12)), FlexContent{[&] {
                     Text(u8"Before");
                     auto props = PaginationProps{}.total(total).hideOnSinglePage(hide);
                     if (explicit_height) {
                         props.layout(LayoutStyle{}.height(dp(60)));
                     }
                     Pagination(props);
                     Text(u8"After");
                 }});
        }});
        f.synchronize(600, {0, 0, 600, 240});
        const auto flex = f.services.components().root_components().front();
        const auto children = f.services.components().children(flex);
        const auto before = f.services.components().root(children[0]);
        const auto pager = f.services.components().root(children[1]);
        const auto after = f.services.components().root(children[2]);
        const auto id = f.services.pagination().mounted().front();
        check(f.services.pagination().snapshot(id).hidden && f.nodes.require(pager).out_of_flow &&
                  std::abs(f.nodes.require(after).bounds.y -
                           (f.nodes.require(before).bounds.y + f.nodes.require(before).bounds.height) - 12) < 0.01F,
              "initially hidden Pagination consumed Flex space");
        total.set(30);
        f.synchronize(600, {0, 0, 600, 240});
        const auto original = f.services.pagination().snapshot(id);
        const auto count = f.services.components().component_count();
        const auto shown_height = f.nodes.require(pager).bounds.height;
        check(!original.hidden && (!explicit_height || shown_height == 60), "visible Pagination explicit height");
        const auto check_hidden = [&] {
            const auto& a = f.nodes.require(before).bounds;
            const auto& b = f.nodes.require(after).bounds;
            check(std::abs(b.y - (a.y + a.height) - 12) < 0.01F, "hidden Pagination introduced extra Flex gap/height");
            const auto& root = f.nodes.require(pager);
            check(root.out_of_flow && root.measured_size == runtime::Size{} && root.layout_size == runtime::Size{} &&
                      root.bounds.width == 0 && root.bounds.height == 0,
                  "hidden Pagination retained nonzero layout geometry");
        };
        const auto measures = f.nodes.require(f.services.components().root(flex)).measure_count;
        total.set(10);
        f.synchronize(600, {0, 0, 600, 240});
        check_hidden();
        check(f.nodes.require(f.services.components().root(flex)).measure_count > measures,
              "visibility change did not invalidate parent measurement");
        total.set(30);
        f.synchronize(600, {0, 0, 600, 240});
        const auto restored = f.services.pagination().snapshot(id);
        const auto& a = f.nodes.require(before).bounds;
        const auto& b = f.nodes.require(after).bounds;
        check(!restored.hidden && restored.items == original.items &&
                  f.services.components().component_count() == count && !f.nodes.require(pager).out_of_flow &&
                  f.nodes.require(pager).bounds.height == shown_height &&
                  std::abs(b.y - (a.y + a.height) - shown_height - 24) < 0.01F,
              "Pagination visibility restoration lost retained identity or flow geometry");
        click(f, restored.items[2]);
        check(f.services.pagination().snapshot(id).current == 2, "restored Pagination lost interaction");
        total.set(10);
        f.synchronize(600, {0, 0, 600, 240});
        check_hidden();
        hide.set(false);
        f.synchronize(600, {0, 0, 600, 240});
        check(!f.services.pagination().snapshot(id).hidden && !f.nodes.require(pager).out_of_flow &&
                  f.nodes.require(pager).bounds.height == shown_height,
              "hideOnSinglePage change did not restore flow");
    }
}

void localized_rtl_controlled_editor() {
    Fixture f;
    Signal<std::size_t> page{10};
    Signal<std::size_t> size{10};
    Signal<TextDirection> direction{TextDirection::RightToLeft};
    Signal<bool> simple{false};
    PaginationLocale translated;
    translated.items_per_page = u8" 项/页";
    translated.page_separator = u8"共 ";
    translated.jump_placeholder = u8"页码";
    Signal<PaginationLocale> locale{translated};
    bool accept = false;
    std::vector<std::pair<std::size_t, std::size_t>> requests;
    f.services.mount(Content{[&] {
        Pagination(PaginationProps{}
                       .total(1000)
                       .current(page)
                       .pageSize(size)
                       .pageSizeOptions(std::vector<std::size_t>{10, 20, 50})
                       .locale(locale)
                       .direction(direction)
                       .showLessItems(true)
                       .showSizeChanger()
                       .showQuickJumper()
                       .simple(simple)
                       .layout(LayoutStyle{}.width(dp(600)).height(dp(80)))
                       .onChange([&](auto next, auto count) {
                           requests.emplace_back(next, count);
                           if (accept) {
                               batch([&] {
                                   page.set(next);
                                   size.set(count);
                               });
                           }
                       }));
    }});
    const auto sync = [&] {
        f.synchronize(600, {0, 0, 600, 240});
    };
    sync();
    const auto id = f.services.pagination().mounted().front();
    const auto initial = f.services.pagination().snapshot(id);
    const auto rect = [&](runtime::ComponentId c) {
        return f.nodes.require(f.services.components().root(c)).bounds;
    };
    check(initial.items.size() == 9 && rect(initial.items.front()).x > rect(initial.items.back()).x,
          "localized less-items Pagination did not retain RTL navigation");
    const auto select = f.services.select().mounted().front();
    const auto choose = [&](std::size_t row) {
        click(f, f.services.select().snapshot(select).selector);
        const auto options = f.services.select().snapshot(select);
        check(options.visible, "localized Pagination Select did not open");
        check(options.total_rows == 3, "localized Pagination Select option count differs");
        const auto found = std::ranges::find(options.row_indices, row);
        check(found != options.row_indices.end(), "localized Pagination option is not in the retained window");
        click(f, options.rows.at(static_cast<std::size_t>(found - options.row_indices.begin())));
    };
    choose(1);
    check(requests.size() == 1 && requests.back().second == 20 && f.services.pagination().snapshot(id).page_size == 10,
          "localized RTL Pagination ignored controlled size refusal");
    accept = true;
    choose(2);
    check(requests.size() == 2 && f.services.pagination().snapshot(id).page_size == 50,
          "localized RTL Pagination ignored controlled size acceptance");
    const auto editor = f.inputs.mounted_inputs().front();
    const auto jump = [&](String value) {
        static_cast<void>(f.inputs.editors().require(editor.editor).set_value(value.bytes()));
        f.inputs.submit(editor.component);
        sync();
    };
    accept = false;
    jump(u8"3");
    check(f.services.pagination().snapshot(id).current == 10 && requests.back().first == 3,
          "localized RTL quick jumper ignored controlled refusal");
    accept = true;
    jump(u8"4");
    check(f.services.pagination().snapshot(id).current == 4, "localized RTL quick jumper failed acceptance");
    const auto count = requests.size();
    jump(u8"2.5");
    check(requests.size() == count, "localized quick jumper accepted a fraction");
    direction.set(TextDirection::LeftToRight);
    locale.set(PaginationLocale{});
    simple.set(true);
    sync();
    const auto changed = f.services.pagination().snapshot(id);
    check(changed.simple && changed.jumper == initial.jumper && changed.size_changer == initial.size_changer &&
              rect(changed.items.front()).x < rect(changed.items.back()).x,
          "dynamic locale/direction/simple update remounted or failed to reorder controls");
    f.services.dispose();
}

void extended_controls() {
    static_assert(requires(PaginationProps p, Signal<bool> flag, Signal<PaginationAlign> align,
                           Signal<std::vector<std::size_t>> sizes) {
        p.defaultPageSize(20)
            .showSizeChanger(flag)
            .pageSizeOptions(sizes)
            .showQuickJumper(flag)
            .simple(flag)
            .readOnly(flag)
            .responsive(flag)
            .align(align);
    });
    Fixture f;
    Signal<bool> simple{false};
    Signal<bool> responsive{false};
    Signal<std::size_t> total{201};
    std::vector<std::pair<std::size_t, std::size_t>> changes;
    int renders{};
    PaginationRange range;
    f.services.mount(Content{[&] {
        Pagination(PaginationProps{}
                       .total(total)
                       .defaultPageSize(20)
                       .showQuickJumper()
                       .showSizeChanger()
                       .simple(simple)
                       .responsive(responsive)
                       .layout(LayoutStyle{}.width(dp(520)))
                       .showTotal([&](auto, PaginationRange r) {
                           range = r;
                           return String{u8"Records"};
                       })
                       .itemRender([&](PaginationItemContext item) {
                           ++renders;
                           Text(TextProps{}.content(bind([type = item.type, label = item.label] {
                               switch (type.get()) {
                               case PaginationItemType::Previous:
                                   return String{u8"‹"};
                               case PaginationItemType::Next:
                                   return String{u8"›"};
                               case PaginationItemType::JumpPrevious:
                               case PaginationItemType::JumpNext:
                                   return String{u8"…"};
                               case PaginationItemType::Page:
                                   return label.get();
                               }
                               return String{};
                           })));
                       })
                       .onChange([&](auto page, auto size) { changes.emplace_back(page, size); }));
    }});
    f.synchronize(600, {0, 0, 600, 240});
    const auto id = f.services.pagination().mounted().front();
    check(renders == 11 && range.first == 1 && range.last == 20, "retained item renderer/total range");
    auto snap = f.services.pagination().snapshot(id);
    check(!ryn_test::find_bundled_icon(f.services, snap.items.front(), IconName::LeftOutlined) &&
              !ryn_test::find_bundled_icon(f.services, snap.items.back(), IconName::RightOutlined),
          "itemRender did not replace default navigation icons");
    const auto input = f.inputs.mounted_inputs().front();
    const auto enter = [&](String value) {
        auto& editor = f.inputs.editors().require(input.editor);
        static_cast<void>(editor.set_value(value.bytes()));
        f.inputs.submit(input.component);
        f.synchronize(600, {0, 0, 600, 240});
    };
    enter(u8" 999 ");
    check(f.services.pagination().snapshot(id).current == 11 && range.first == 201 && range.last == 201,
          "quick jump clamp/range");
    const auto count = changes.size();
    for (auto invalid : {u8"abc", u8"2.5", u8"-1", u8"184467440737095516160"}) {
        enter(String::from_utf8(std::string_view{reinterpret_cast<const char*>(invalid)}).value());
    }
    check(changes.size() == count, "invalid jump emitted request");
    enter(u8"0");
    check(f.services.pagination().snapshot(id).current == 1, "zero jump clamp");
    simple.set(true);
    f.synchronize(600, {0, 0, 600, 240});
    check(f.services.pagination().snapshot(id).items.size() == 2 && renders == 11, "simple retained buttons");
    enter(u8"3");
    check(f.services.pagination().snapshot(id).current == 3, "simple editor jump");
    const auto select = f.services.select().mounted().front();
    click(f, f.services.select().snapshot(select).selector);
    auto options = f.services.select().snapshot(select);
    check(options.visible && options.rows.size() == 5, "size changer popup/options");
    click(f, options.rows[2]);
    check(f.services.pagination().snapshot(id).page_size == 50 && changes.back().second == 50,
          "uncontrolled page size event");
    total.set(0);
    f.synchronize(600, {0, 0, 600, 240});
    check(range.first == 0 && range.last == 0, "empty total range");
    responsive.set(true);
    f.synchronize(520, {0, 0, 520, 240});
    check(f.services.pagination().snapshot(id).compact, "container responsive size");
    f.services.destroy(id);
    check(f.services.interactions().size() == 0 && f.inputs.mounted_inputs().empty(), "extended cleanup");
}

} // namespace

int main() {
    try {
        extended_controls();
        localized_rtl_controlled_editor();
        controlled();
        keyboard_and_theme();
        token_contracts_and_callback_cleanup();
        hidden_pagination_flex_flow();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
