#include "component/calendar_component.hpp"
#include "component/form_component.hpp"
#include "component/select_component.hpp"
#include "component/popover_component.hpp"
#include "runtime/prop_connection.hpp"
#include "support/input_fixture.hpp"
#include "../examples/calendar_example.hpp"
#include <iostream>
#include <map>
#include <thread>
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool v, const char* text) {
    if (!v) {
        throw std::runtime_error(text);
    }
}

void sync(Fixture& f, float width = 760, float height = 1100) {
    check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}), "Calendar layout");
}

void dates() {
    Fixture f;
    CalendarRef ref;
    Signal<CivilDate> value{CivilDate{2024, 2, 29}};
    Signal<CalendarMode> mode{CalendarMode::Month};
    PickerLocale locale;
    locale.first_day = 1;
    locale.first_week_contains = 4;
    f.services.mount(
        Content{[&] { Calendar(CalendarProps{}.ref(ref).value(value).mode(mode).locale(locale).showWeek()); }});
    sync(f);
    const auto id = f.services.calendar().mounted().back();
    auto s = f.services.calendar().snapshot(id);
    check(s.cells.size() == 42 && s.cells.front().date == CivilDate{2024, 1, 29} &&
              s.cells.back().date == CivilDate{2024, 3, 10},
          "leap 42 grid");
    check(calendar_week({2021, 1, 1}, locale) == CalendarWeek{2020, 53}, "ISO week year");
    value.set({1, 1, 1});
    sync(f);
    s = f.services.calendar().snapshot(id);
    check(s.cells.size() == 42, "year 1 grid");
    value.set({9999, 12, 31});
    sync(f);
    s = f.services.calendar().snapshot(id);
    check(s.cells.back().disabled, "year 9999 invalid spill");
    value.set({2024, 2, 29});
    mode.set(CalendarMode::Year);
    sync(f);
    s = f.services.calendar().snapshot(id);
    check(s.cells.size() == 12 && s.cells[1].selected && s.cells[11].date == CivilDate{2024, 12, 1}, "12 month grid");
    check(ref.focus(), "Calendar native ref focus");
}

void controlled() {
    Fixture f;
    CalendarRef ref;
    std::vector<int> events;
    int selected{};
    Signal<CivilDate> value{CivilDate{2024, 2, 10}};
    Signal<std::optional<CalendarRange>> range{std::optional{CalendarRange{{2024, 2, 5}, {2024, 3, 20}}}};
    f.services.mount(Content{[&] {
        Calendar(CalendarProps{}
                     .ref(ref)
                     .value(value)
                     .validRange(range)
                     .onPanelChange([&](auto, auto) { events.push_back(1); })
                     .onChange([&](auto) { events.push_back(2); })
                     .onSelect([&](auto, CalendarSelectInfo i) {
                         check(i.source == CalendarSelectSource::Customize, "select metadata");
                         ++selected;
                         events.push_back(3);
                     }));
    }});
    sync(f);
    const auto id = f.services.calendar().mounted().back();
    check(ref.select({2024, 3, 1}), "controlled request rejected");
    sync(f);
    check(events == std::vector<int>{1, 2, 3} && f.services.calendar().snapshot(id).value == value.get(),
          "controlled refusal/event order");
    check(!ref.select({2024, 4, 1}) && selected == 1, "range request");
    range.set(CalendarRange{{2024, 2, 12}, {2024, 2, 20}});
    sync(f);
    check(!ref.select({2024, 2, 10}), "rules replacement");
    check(ref.changeMode(CalendarMode::Year), "mode request");
    sync(f);
    check(f.services.calendar().snapshot(id).mode == CalendarMode::Year, "uncontrolled mode");
    f.services.destroy(id);
    check(!ref.focus() && !ref.select({2024, 2, 15}), "stale ref");
    bool thread_rejected{};
    std::thread worker([&] {
        try {
            static_cast<void>(ref.focus());
        } catch (const std::logic_error&) {
            thread_rejected = true;
        }
    });
    worker.join();
    check(thread_rejected, "foreign thread ref");
}

void retained() {
    Fixture f;
    CalendarRef ref;
    Signal<CivilDate> value{CivilDate{2024, 2, 10}};
    Signal<bool> fullscreen{true};
    Signal<PickerLocale> locale{PickerLocale{}};
    std::map<std::string, InputRef> editors;
    std::map<std::string, int> mounts;
    CalendarCellRenderer rich{[&](Prop<CalendarCellInfo> meta) {
        const auto info = ryn::detail::read_prop(meta);
        const auto key = std::string{format_iso_date(info.date).bytes()};
        return CalendarCell{[&, key] {
            ++mounts[key];
            Input(InputProps{}.ref(editors[key]).defaultValue(u8"draft"));
        }};
    }};
    f.services.mount(Content{[&] {
        Calendar(CalendarProps{}.ref(ref).value(value).fullscreen(fullscreen).locale(locale).cellRender(rich));
    }});
    sync(f);
    const auto id = f.services.calendar().mounted().back();
    auto before = f.services.calendar().snapshot(id);
    auto editor = editors.at("2024-02-29");
    check(editor.focus(), "rich cell focus");
    const auto stamp = f.inputs.sessions().active();
    static_cast<void>(f.inputs.editors().require(stamp.owner).select_all());
    check(bool(f.inputs.dispatch(input::TextCommitted{String{u8"保留草稿"}, stamp})), "rich cell edit");
    sync(f);
    value.set({2024, 3, 10});
    sync(f);
    auto after = f.services.calendar().snapshot(id);
    const auto find = [](const auto& s, CivilDate d) {
        for (std::size_t i = 0; i < s.cells.size(); ++i) {
            if (s.cells[i].date == d) {
                return s.components[i];
            }
        }
        return runtime::ComponentId{};
    };
    check(find(before, {2024, 2, 29}) == find(after, {2024, 2, 29}) && mounts["2024-02-29"] == 1, "overlap remounted");
    check(f.inputs.editors().require(stamp.owner).value() == String{u8"保留草稿"}.bytes(),
          "overlap lost real editor draft");
    fullscreen.set(false);
    auto loc = locale.get();
    loc.first_day = 1;
    locale.set(loc);
    sync(f);
    check(mounts["2024-02-29"] == 1 && editor.focus(), "locale/fullscreen remounted draft");
    value.set({2025, 9, 1});
    sync(f);
    check(!editor.focus(), "retired cell ref survived");
}

void failure() {
    Fixture f;
    CalendarRef ref;
    Signal<CalendarCellRenderer> renderer{CalendarCellRenderer{}};
    f.services.mount(Content{[&] {
        Calendar(CalendarProps{}.ref(ref).defaultValue({2024, 2, 1}).cellRender(renderer));
        Button(ButtonProps{}, [] { Text(u8"survivor"); });
    }});
    sync(f);
    const auto id = f.services.calendar().mounted().back();
    const auto before = f.services.calendar().snapshot(id).components;
    renderer.set(CalendarCellRenderer{[](auto) -> CalendarCell {
        return CalendarCell{[] {
            Input(InputProps{});
            throw std::runtime_error("factory");
        }};
    }});
    bool caught{};
    try {
        sync(f);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    check(caught && f.services.calendar().snapshot(id).components == before, "factory rollback");
    renderer.set(CalendarCellRenderer{});
    sync(f);
    f.services.destroy(id);
    check(!ref.focus(), "unmount ref");
    Fixture g;
    runtime::ComponentId owner;
    g.services.mount(Content{[&] {
        Calendar(
            CalendarProps{}.ref(ref).defaultValue({2024, 2, 1}).onChange([&](auto) { g.services.destroy(owner); }));
    }});
    sync(g);
    owner = g.services.calendar().mounted().back();
    check(ref.select({2024, 2, 2}) && !ref.focus(), "self destroy");
    Fixture h;
    h.services.mount(Content{[&] {
        Calendar(CalendarProps{}.ref(ref).defaultValue({2024, 2, 1}).onSelect([](auto, auto) {
            throw std::runtime_error("select");
        }));
        Button(ButtonProps{}, [] { Text(u8"survivor"); });
    }});
    sync(h);
    const auto throwing = h.services.calendar().mounted().back();
    caught = false;
    try {
        static_cast<void>(ref.select({2024, 2, 2}));
    } catch (const std::runtime_error&) {
        caught = true;
    }
    check(caught && !h.services.components().contains(throwing) && !ref.focus() && h.nodes.size() > 0,
          "throw retired unrelated content or leaked Calendar");
}

void geometry() {
    Fixture f;
    CalendarRef ref;
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    Signal<bool> visible{true};
    Signal<LogicalLength> extent{dp(700)};
    f.services.mount(Content{[&] {
        Calendar(CalendarProps{}
                     .ref(ref)
                     .defaultValue({2024, 2, 10})
                     .direction(direction)
                     .visible(visible)
                     .layout(LayoutStyle{}.width(extent).height(extent)));
    }});
    sync(f);
    const auto id = f.services.calendar().mounted().back();
    const auto s = f.services.calendar().snapshot(id);
    const auto a = f.nodes.require(f.services.components().root(s.components[0])).bounds;
    direction.set(TextDirection::RightToLeft);
    sync(f);
    const auto b = f.nodes.require(f.services.components().root(s.components[0])).bounds;
    check(b.x > a.x, "RTL physical geometry");
    check(ref.focus(), "focus before hide");
    visible.set(false);
    sync(f);
    check(!ref.focus() && !f.services.focus().state().focused, "hidden focus cleanup");
    visible.set(true);
    extent.set(dp(0));
    sync(f);
    check(!ref.focus(), "zero area ref");
}

void nested() {
    Fixture f;
    CalendarRef ref;
    InputRef editor;
    Signal<bool> open{false};
    Signal<CivilDate> value{CivilDate{2024, 2, 29}};
    f.services.mount(Content{[&] {
        Calendar(
            CalendarProps{}.ref(ref).value(value).cellRender(CalendarCellRenderer{[&](Binding<CalendarCellInfo> meta) {
                return CalendarCell{[&, meta] {
                    if (meta.get().date != CivilDate{2024, 2, 29}) {
                        return;
                    }
                    Input(InputProps{}.ref(editor).defaultValue(u8"draft"));
                    Popover(
                        PopoverProps{}.open(open).onOpenChange([&](bool v) { open.set(v); }),
                        PopoverTrigger{[] { Button(ButtonProps{}, [] { Text(u8"popup"); }); }},
                        PopoverSlots{.content = PopoverContent{[] { Input(InputProps{}.defaultValue(u8"nested")); }}});
                }};
            }}));
    }});
    sync(f);
    check(editor.focus(), "nested editor focus");
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(input::CompositionChanged{String{u8"拼音"}, {0, 2, true}, stamp})),
          "cell Core preedit");
    f.services.focus().dispatch({input::Key::left, input::KeyAction::down});
    sync(f);
    check(f.services.calendar().snapshot(f.services.calendar().mounted().back()).value == CivilDate{2024, 2, 29},
          "calendar stole editor key");
    open.set(true);
    sync(f);
    value.set({2025, 2, 1});
    sync(f);
    check(!f.inputs.sessions().active().valid() && !editor.focus(), "retired rich cell retained IME/focus");
    Fixture g;
    int headers{};
    Signal<CalendarHeaderInfo> observed{CalendarHeaderInfo{}};
    g.services.mount(Content{[&] {
        Calendar(CalendarProps{}.ref(ref).defaultValue({2024, 2, 1}).headerRender([&](CalendarHeaderContext context) {
            return CalendarHeader{[&, context] {
                ++headers;
                Text(TextProps{}.content(bind([context] { return format_iso_date(context.info.get().value); })));
                Button(ButtonProps{}.onClick(
                           [context] { static_cast<void>(context.calendar.changeMode(CalendarMode::Year)); }),
                       [] { Text(u8"Year"); });
            }};
        }));
    }});
    sync(g);
    check(ref.select({2024, 3, 1}), "custom header ref");
    sync(g);
    check(headers == 1, "custom header remounted");
}

void header_replacement() {
    Fixture f;
    CalendarRef ref;
    int mounts{};
    Signal<CalendarHeaderRenderer> renderer{CalendarHeaderRenderer{}};
    f.services.mount(Content{[&] {
        Calendar(CalendarProps{}
                     .ref(ref)
                     .defaultValue({2024, 2, 29})
                     .headerRender(renderer)
                     .cellRender(CalendarCellRenderer{[&](Binding<CalendarCellInfo>) {
                         return CalendarCell{[&] {
                             ++mounts;
                             Input(InputProps{}.defaultValue(u8"draft"));
                         }};
                     }}));
    }});
    sync(f);
    const auto id = f.services.calendar().mounted().back();
    const auto before = f.services.calendar().snapshot(id);
    renderer.set(CalendarHeaderRenderer{[](CalendarHeaderContext c) {
        return CalendarHeader{
            [c] { Text(TextProps{}.content(bind([c] { return format_iso_date(c.info.get().value); }))); }};
    }});
    sync(f);
    const auto after = f.services.calendar().snapshot(id);
    check(after.header != before.header && after.components == before.components && mounts == 42,
          "header replacement remounted cells");
    bool caught{};
    try {
        renderer.set(CalendarHeaderRenderer{[](CalendarHeaderContext) {
            return CalendarHeader{[] {
                Input(InputProps{});
                throw std::runtime_error("header factory");
            }};
        }});
    } catch (const std::runtime_error&) {
        caught = true;
    }
    check(caught && f.services.calendar().snapshot(id).header == after.header, "header rollback");
    renderer.set(CalendarHeaderRenderer{});
    sync(f);
    check(ref.select({2024, 3, 1}), "header recovery");
}

void keyboard() {
    Fixture f;
    CalendarRef ref;
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    int selected{};
    f.services.mount(Content{[&] {
        Calendar(CalendarProps{}.ref(ref).defaultValue({2024, 2, 28}).direction(direction).onSelect([&](auto, auto) {
            ++selected;
        }));
    }});
    sync(f);
    const auto id = f.services.calendar().mounted().back();
    const auto key = [&](input::Key key) {
        f.services.focus().dispatch({key, input::KeyAction::down});
        f.services.focus().dispatch({key, input::KeyAction::up});
        sync(f);
    };
    check(ref.focus(), "keyboard focus");
    key(input::Key::right);
    key(input::Key::enter);
    check(f.services.calendar().snapshot(id).value == CivilDate{2024, 2, 29} && selected == 1,
          "leap keyboard selection");
    direction.set(TextDirection::RightToLeft);
    sync(f);
    key(input::Key::right);
    key(input::Key::enter);
    check(f.services.calendar().snapshot(id).value == CivilDate{2024, 2, 28} && selected == 2,
          "RTL keyboard selection");
    key(input::Key::page_down);
    key(input::Key::enter);
    check(f.services.calendar().snapshot(id).value == CivilDate{2024, 3, 28}, "keyboard panel navigation");
}

void theme() {
    const auto base = resolve_theme();
    ThemeConfig config;
    config.calendar.tokens.full_background = Color::rgba8(7, 11, 13);
    config.calendar.tokens.cell_height = dp(72);
    auto overridden = resolve_theme(config);
    check(overridden.calendar().metrics.cell_height == 72 &&
              overridden.calendar().colors.full_background == Color::rgba8(7, 11, 13) &&
              base.date_picker() == overridden.date_picker(),
          "independent calendar tokens");
    check(resolve_theme({}, &overridden).calendar() == overridden.calendar(), "calendar inherit");
    ThemeConfig isolated;
    isolated.inherit = false;
    check(resolve_theme(isolated, &overridden).calendar() == base.calendar(), "calendar isolated");
    config.calendar.algorithm = true;
    config.calendar.seed.color_primary = Color::rgba8(200, 40, 30);
    auto seeded = resolve_theme(config);
    check(seeded.calendar().colors.today != base.calendar().colors.today && seeded.button() == base.button(),
          "scoped algorithm");
    Fixture f;
    f.services.mount(Content{[] { CalendarExample(); }});
    sync(f);
    Fixture g;
    Signal<ThemeConfig> live_theme{ThemeConfig{}};
    int unrelated{};
    g.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(live_theme), ThemeContent{[&] {
                  Calendar(CalendarProps{}.defaultValue({2024, 2, 29}));
                  ++unrelated;
                  Button(ButtonProps{}, [] { Text(u8"stable"); });
              }});
    }});
    sync(g);
    const auto owner = g.services.calendar().mounted().back();
    const auto node = g.services.components().root(owner);
    const auto measured = g.nodes.require(node).measure_count;
    const auto cells = g.services.calendar().snapshot(owner).components;
    ThemeConfig colors_only;
    colors_only.calendar.tokens.text = Color::rgba8(12, 34, 56);
    live_theme.set(colors_only);
    sync(g);
    check(g.services.components().theme_scope(owner)->snapshot().calendar().colors.text ==
                  *colors_only.calendar.tokens.text &&
              g.nodes.require(node).measure_count == measured &&
              g.services.calendar().snapshot(owner).components == cells && unrelated == 1,
          "Calendar color Theme update expanded layout or remounted unrelated content");
}

int main(int argc, char** argv) {
    try {
        const std::string which = argc > 1 ? argv[1] : "dates";
        if (which == "dates") {
            dates();
        } else if (which == "controlled") {
            controlled();
        } else if (which == "retained") {
            retained();
        } else if (which == "failure") {
            failure();
        } else if (which == "geometry") {
            geometry();
        } else if (which == "nested") {
            nested();
        } else if (which == "header") {
            header_replacement();
        } else if (which == "keyboard") {
            keyboard();
        } else if (which == "theme") {
            theme();
        } else {
            throw std::invalid_argument("unknown calendar case");
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
