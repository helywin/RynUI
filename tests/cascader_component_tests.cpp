#include "component/cascader_component.hpp"
#include "component/popover_component.hpp"
#include "support/timeline_fixture.hpp"
#include "theme/theme_runtime.hpp"
#include <thread>

namespace {
using namespace timeline_test;

std::vector<CascaderOption> options() {
    return {{u8"cn", String{u8"中国"}, u8"中国", {{u8"hz", 1.0, u8"杭州"}, {u8"nj", 2.0, u8"南京"}}},
            {u8"off", String{u8"off"}, u8"禁用", {}, true}};
}

void controlled() {
    Fixture f;
    Signal<CascaderPaths> value{CascaderPaths{}};
    Signal<std::vector<CascaderOption>> data{options()};
    CascaderPaths request;
    CascaderSelectedOptions selected;
    f.services.mount(Content{[&] {
        CascaderPanel(CascaderProps{}
                          .options(data)
                          .value(value)
                          .onChange([&](auto x, auto metadata) {
                              request = x;
                              selected = metadata;
                          })
                          .layout(LayoutStyle{}.width(dp(400))));
    }});
    sync(f);
    auto id = f.services.cascader().mounted().front();
    auto s = f.services.cascader().snapshot(id);
    click(f, s.rows[0].component);
    s = f.services.cascader().snapshot(id);
    check(s.rows[1].shown, "child column did not expand");
    const auto child = s.rows[1].component;
    click(f, child);
    check(request == CascaderPaths{{String{u8"中国"}, 1.0}} && f.services.cascader().snapshot(id).value.empty(),
          "controlled refusal changed value/path");
    check(selected.size() == 1 && selected[0].size() == 2 && selected[0][1].label == String{u8"杭州"},
          "native change event lost selected option ancestry");
    value.set(request);
    auto updated = options();
    std::reverse(updated[0].children.begin(), updated[0].children.end());
    data.set(updated);
    sync(f);
    s = f.services.cascader().snapshot(id);
    check(s.rows[2].component == child && s.value == request, "keyed reorder lost identity");
    auto invalid = options();
    invalid.push_back(invalid.front());
    bool failed = false;
    try {
        data.set(invalid);
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    check(failed && f.services.cascader().snapshot(id).rows[2].component == child,
          "invalid topology did not roll back");
    f.services.dispose();
    check(f.nodes.size() == 0 && f.services.interactions().size() == 0 && !f.services.next_frame_deadline(),
          "Cascader leaked");
}

void multiple_async() {
    Fixture f;
    std::vector<CascaderLoadRequest> loads;
    Signal<bool> disabled{false};
    auto data = options();
    data.push_back({u8"lazy", String{u8"lazy"}, u8"Lazy", {}, false, false, false});
    f.services.mount(Content{[&] {
        CascaderPanel(CascaderProps{}
                          .options(data)
                          .multiple(true)
                          .disabled(disabled)
                          .loadData([&](auto r) { loads.push_back(r); })
                          .layout(LayoutStyle{}.width(dp(400))));
    }});
    sync(f);
    auto id = f.services.cascader().mounted().front();
    auto s = f.services.cascader().snapshot(id);
    click(f, s.rows[0].component);
    check(f.services.cascader().snapshot(id).value == CascaderPaths{{String{u8"中国"}}}, "parent conduction failed");
    click(f, s.rows.back().component);
    check(loads.size() == 1, "lazy loader not called");
    bool foreign = true;
    std::thread worker{[&] { foreign = loads[0].complete({{{u8"x", 3.0, u8"异步"}}, {}}); }};
    worker.join();
    check(!foreign, "foreign completion touched UI");
    click(f, s.rows.back().component);
    check(loads.size() == 2 && loads[0].cancelled->load(), "new load did not cancel old");
    check(!loads[0].complete({}), "stale completion accepted");
    check(loads[1].complete({{{u8"x", 3.0, u8"异步"}}, {}}), "latest completion refused");
    sync(f);
    check(f.services.cascader().snapshot(id).rows.size() == 6, "loaded children absent");
    disabled.set(true);
    sync(f);
    auto before = f.services.cascader().snapshot(id).value;
    click(f, s.rows[0].component);
    check(f.services.cascader().snapshot(id).value == before, "disabled changed selection");
    f.services.dispose();
    check(!loads[1].complete({}), "completion survived unmount");
}

void input_lifecycle() {
    Fixture f;
    Signal<std::vector<CascaderOption>> data{options()};
    int slots = 0;
    auto rich = options();
    rich[0].children[0].content = CascaderOptionContent{[&] {
        ++slots;
        Input(InputProps{}.defaultValue(String{u8"draft"}));
    }};
    data.set(rich);
    f.services.mount(Content{
        [&] { CascaderPanel(CascaderProps{}.options(data).showSearch(true).layout(LayoutStyle{}.width(dp(400)))); }});
    sync(f);
    const auto id = f.services.cascader().mounted().front();
    auto s = f.services.cascader().snapshot(id);
    click(f, s.rows[0].component);
    const auto rich_input = f.inputs.mounted_inputs()[1];
    check(f.services.focus().request_focus(rich_input.interaction, FocusModality::keyboard), "rich draft focus");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8" saved"}, f.inputs.sessions().active()})),
          "rich draft commit");
    const auto draft = f.inputs.editors().require(rich_input.editor).value();
    auto reordered = rich;
    std::reverse(reordered[0].children.begin(), reordered[0].children.end());
    data.set(reordered);
    sync(f);
    check(slots == 1 && f.inputs.editors().require(rich_input.editor).value() == draft,
          "rich slot reran or lost draft on reorder");
    const auto search = f.inputs.mounted_inputs().front();
    check(f.services.focus().request_focus(search.interaction, FocusModality::keyboard), "search cannot focus");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"杭州"}, f.inputs.sessions().active()})),
          "Unicode query not committed");
    sync(f);
    s = f.services.cascader().snapshot(id);
    check(std::ranges::count_if(s.rows, [](const auto& r) { return r.shown; }) == 1 && s.rows[2].shown,
          "Unicode path filter failed");
    f.services.dispose();
    check(f.services.interactions().size() == 0 && f.services.surfaces().size() == 0, "rich input teardown leaked");
    Fixture g;
    g.services.mount(Content{
        [&] { CascaderPanel(CascaderProps{}.options(options()).onChange([&](auto) { g.services.dispose(); })); }});
    sync(g);
    auto owner = g.services.cascader().mounted().front();
    auto snap = g.services.cascader().snapshot(owner);
    click(g, snap.rows[0].component);
    snap = g.services.cascader().snapshot(owner);
    click(g, snap.rows[1].component);
    check(g.nodes.size() == 0, "callback destruction failed");
}

void wheel_keyboard() {
    Fixture f;
    std::vector<CascaderOption> data;
    for (int i = 0; i < 20; ++i) {
        auto label = String::from_utf8("Item " + std::to_string(i)).value();
        data.push_back({label, static_cast<double>(i), label});
    }
    f.services.mount(Content{
        [&] { CascaderPanel(CascaderProps{}.options(data).listHeight(120).layout(LayoutStyle{}.width(dp(200)))); }});
    sync(f);
    auto id = f.services.cascader().mounted().front();
    auto snap = f.services.cascader().snapshot(id);
    const auto before = bounds(f, snap.rows[0].component);
    check(bool(f.inputs.dispatch(ScrollInputEvent{0, -0.125F, before.x + 10, before.y + 10})),
          "fractional wheel not consumed");
    sync(f);
    const auto after = bounds(f, snap.rows[0].component);
    check(std::abs(after.y - before.y + 12) < 0.01F, "fractional wheel quantized");
    click(f, snap.rows[1].component);
    f.services.focus().dispatch({Key::down, KeyAction::down});
    f.services.focus().dispatch({Key::down, KeyAction::up});
    f.services.focus().dispatch({Key::enter, KeyAction::down});
    f.services.focus().dispatch({Key::enter, KeyAction::up});
    sync(f);
    auto selected = f.services.cascader().snapshot(id).value;
    check(selected == CascaderPaths{{2.0}}, "keyboard Down/Enter did not select next row");
    const auto row = bounds(f, snap.rows[2].component);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, row.x + 10, row.y + 10});
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::cancel, PointerButton::none, row.x + 10, row.y + 10});
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, row.x + 10, row.y + 10});
    check(f.services.cascader().snapshot(id).value == selected, "capture cancel activated a row");
    f.services.dispose();
    check(!f.services.next_frame_deadline(), "idle Cascader requests frames");
}

void popup() {
    Fixture f;
    int changed = 0;
    f.services.mount(Content{[&] {
        Cascader(CascaderProps{}
                     .options(options())
                     .showSearch(true)
                     .onChange([&](auto) { ++changed; })
                     .layout(LayoutStyle{}.width(dp(280))));
    }});
    sync(f);
    auto id = f.services.cascader().mounted().front();
    auto s = f.services.cascader().snapshot(id);
    check(!s.open && s.selector.valid(), "Cascader popup did not start closed");
    click(f, s.selector);
    s = f.services.cascader().snapshot(id);
    check(s.open, "native selector did not open popup");
    click(f, s.rows[0].component);
    s = f.services.cascader().snapshot(id);
    check(s.rows[1].shown, "popup second column absent");
    click(f, s.rows[1].component);
    s = f.services.cascader().snapshot(id);
    check(!s.open && changed == 1 && s.value == CascaderPaths{{String{u8"中国"}, 1.0}}, "popup selection/close failed");
    f.services.dispose();
    check(f.services.interactions().size() == 0 && !f.services.next_frame_deadline(), "popup resources leaked");
}

void key(Fixture& f, Key k, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({k, KeyAction::down, modifiers});
    f.services.focus().dispatch({k, KeyAction::up, modifiers});
    sync(f);
}

void tags_theme() {
    Fixture f;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<CascaderPaths> value{CascaderPaths{{String{u8"中国"}, 1.0}, {String{u8"中国"}, 2.0}}};
    Signal<CascaderMaxTagCount> limit{CascaderMaxTagCount{std::size_t{1}}};
    Signal<bool> readonly{false};
    CascaderRef ref;
    int changes = 0;
    int unrelated_factory = 0;
    CascaderPaths requested;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Cascader(CascaderProps{}
                               .options(options())
                               .multiple(true)
                               .value(value)
                               .maxTagCount(limit)
                               .maxTagTextLength(std::size_t{4})
                               .readOnly(readonly)
                               .ref(ref)
                               .onChange([&](auto x) {
                                   ++changes;
                                   requested = x;
                               })
                               .layout(LayoutStyle{}.width(dp(380))));
                  ++unrelated_factory;
                  Text(u8"unrelated");
              }});
    }});
    sync(f);
    const auto id = f.services.cascader().mounted().front();
    auto s = f.services.cascader().snapshot(id);
    check(s.tags.size() == 2 && s.tags[0].shown && !s.tags[1].shown && s.tags[0].remove.valid(),
          "real retained tags/maxTagCount");
    check(s.tags[0].label == String{u8"中国 /…"}, "grapheme tag truncation");
    const auto retained = s.tags[0].component;
    click(f, s.tags[0].remove);
    check(changes == 1 && requested.size() == 1 && f.services.cascader().snapshot(id).tags.size() == 2 &&
              !f.services.cascader().snapshot(id).open,
          "controlled tag refusal or removal opened popup");
    readonly.set(true);
    sync(f);
    click(f, s.tags[0].remove);
    check(changes == 1, "readonly tag removed");
    readonly.set(false);
    value.set(requested);
    sync(f);
    check(f.services.cascader().snapshot(id).tags.size() == 1, "accepted tag removal");
    auto c = theme.get();
    c.cascader.tokens.option_height = dp(47);
    c.cascader.tokens.column_width = dp(150);
    c.cascader.tokens.option_selected_background = Color::rgba8(12, 34, 56);
    c.cascader.tokens.tag_background = Color::rgba8(20, 80, 40);
    theme.set(c);
    sync(f);
    check(ref.bound() && ref.focus(), "native Cascader ref");
    key(f, Key::down);
    s = f.services.cascader().snapshot(id);
    check(s.open && bounds(f, s.rows[0].component).height >= 47, "component metrics not applied to row layout");
    const auto row_id = s.rows[0].component;
    c.cascader.tokens.option_color = Color::rgba8(3, 90, 50);
    theme.set(c);
    sync(f);
    check(f.services.cascader().snapshot(id).rows[0].component == row_id, "token change remounted row factory");
    check(unrelated_factory == 1 && f.buttons.snapshot(row_id).presentation_foreground == Color::rgba8(3, 90, 50),
          "Cascader palette did not reach actual scene or remounted unrelated factory");
    (void)retained;
    f.services.dispose();
    check(!ref.bound() && f.services.surfaces().size() == 0, "ref/scene retirement");
}

void retained_custom_tags() {
    Fixture f;
    Signal<CascaderPaths> value{CascaderPaths{{String{u8"中国"}, 1.0}}};
    Signal<std::size_t> max_text{20};
    std::function<void()> close;
    int factories = 0;
    f.services.mount(Content{[&] {
        Cascader(CascaderProps{}
                     .options(options())
                     .multiple(true)
                     .value(value)
                     .maxTagTextLength(max_text)
                     .tagRender([&](CascaderTagInfo info) {
                         ++factories;
                         close = info.onClose;
                         return CascaderTagContent{[info] {
                             Input(InputProps{}.defaultValue(String{u8"tag draft"}).disabled(info.disabled));
                             Text(TextProps{}.content(info.label));
                         }};
                     })
                     .onChange([&](auto v) { value.set(v); })
                     .layout(LayoutStyle{}.width(dp(420))));
    }});
    sync(f);
    const auto id = f.services.cascader().mounted().front();
    auto s = f.services.cascader().snapshot(id);
    const auto tag = s.tags[0].component;
    max_text.set(3);
    sync(f);
    check(factories == 1 && f.services.cascader().snapshot(id).tags[0].component == tag, "custom tag factory reran");
    close();
    sync(f);
    check(f.services.cascader().snapshot(id).tags.empty(), "custom close did not remove tag");
    f.services.dispose();
    close();
    check(f.nodes.size() == 0, "retained tag callback outlived owner");
}

void clipping_search_keyboard() {
    Fixture f;
    std::vector<CascaderOption> data;
    for (int i = 0; i < 30; ++i) {
        auto label = String::from_utf8("Straße " + std::to_string(i)).value();
        data.push_back({label, double(i), label});
    }
    f.services.mount(Content{[&] {
        CascaderPanel(CascaderProps{}.options(data).showSearch(true).listHeight(150).columnWidth(180).layout(
            LayoutStyle{}.width(dp(180))));
    }});
    sync(f);
    const auto id = f.services.cascader().mounted().front();
    auto s = f.services.cascader().snapshot(id);
    const auto search = f.inputs.mounted_inputs().front();
    check(f.services.focus().request_focus(search.interaction, FocusModality::keyboard), "search focus");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"STRASSE"}, f.inputs.sessions().active()})),
          "Unicode search commit");
    sync(f);
    s = f.services.cascader().snapshot(id);
    check(std::ranges::count_if(s.rows, [](auto& r) { return r.shown; }) == 30, "full Unicode case mapping");
    key(f, Key::down);
    key(f, Key::end);
    s = f.services.cascader().snapshot(id);
    check(s.scroll[0] > 0, "End did not reveal focused row");
    const auto last = bounds(f, s.rows.back().component);
    const auto header = bounds(f, s.search);
    check(last.y >= header.y + header.height &&
              last.y + last.height <= bounds(f, s.panel).y + bounds(f, s.panel).height + .1F,
          "focused last row not revealed below header");
    const auto hit = f.services.hit_test().hit_test({header.x + 10, header.y + 10});
    check(hit && f.services.interactions().require(*hit).component == s.search, "scrolled result hit header");
    const auto viewport = bounds(f, s.rows.back().viewport);
    const float before = s.scroll[0];
    check(!f.services.cascader().on_scroll_input({0, -.125F, viewport.x + 10, viewport.y + 10}),
          "wheel edge did not hand off");
    check(f.services.cascader().on_scroll_input({0, .125F, viewport.x + 10, viewport.y + 10}),
          "fractional reverse wheel");
    sync(f);
    check(std::abs(f.services.cascader().snapshot(id).scroll[0] - before + 12) < .01F, "fractional wheel quantized");
    key(f, Key::home);
    key(f, Key::page_down);
    check(f.services.cascader().snapshot(id).scroll[0] > 0, "PageDown reveal");
    f.services.dispose();
}

void composition_boundaries() {
    for (int mode = 0; mode < 7; ++mode) {
        Fixture f;
        Signal<bool> disabled{false};
        Signal<bool> readonly{false};
        Signal<bool> visible{true};
        Signal<LogicalLength> width{dp(260)};
        auto data = options();
        data[0].content = CascaderOptionContent{[] { Input(InputProps{}.defaultValue(String{u8"rich draft"})); }};
        f.services.mount(Content{[&] {
            CascaderPanel(CascaderProps{}
                              .options(data)
                              .showSearch(true)
                              .disabled(disabled)
                              .readOnly(readonly)
                              .visible(visible)
                              .layout(LayoutStyle{}.width(width)));
        }});
        sync(f);
        const auto id = f.services.cascader().mounted().front();
        const auto s = f.services.cascader().snapshot(id);
        const auto editor = f.inputs.mounted_inputs()[mode % 2 ? 1 : 0];
        check(f.services.focus().request_focus(editor.interaction, FocusModality::keyboard), "IME owner focus");
        const auto stamp = f.inputs.sessions().active();
        check(bool(f.inputs.dispatch(CompositionChanged{String{u8"预编辑"}, {}, stamp})), "Core preedit");
        if (mode == 0) {
            disabled.set(true);
        } else if (mode == 1) {
            readonly.set(true);
        } else if (mode == 2) {
            visible.set(false);
        } else if (mode == 3) {
            f.services.set_input_blocked(id, true);
        } else if (mode == 4) {
            width.set(dp(0));
        } else if (mode == 5) {
            f.services.set_window_active(false);
        } else {
            f.services.destroy(s.rows[0].viewport);
        }
        sync(f);
        check(!f.inputs.sessions().active().owner.valid() || f.inputs.sessions().active() != stamp,
              "stale IME session remained active");
        check(!bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stamp})), "stale commit accepted");
        f.services.dispose();
        check(f.services.interactions().size() == 0 && (!f.services.pointer().state(PointerIdentity::mouse()) ||
                                                        !f.services.pointer().state(PointerIdentity::mouse())->capture),
              "IME boundary leaked focus/capture");
    }
}

void loader_lifecycle() {
    for (int boundary = 0; boundary < 7; ++boundary) {
        auto f = std::make_unique<Fixture>();
        Signal<std::vector<CascaderOption>> data{
            std::vector<CascaderOption>{{u8"lazy", String{u8"lazy"}, u8"Lazy", {}, false, false, false}}};
        Signal<bool> visible{true};
        std::vector<CascaderLoadRequest> requests;
        f->services.mount(Content{[&] {
            CascaderPanel(CascaderProps{}.options(data).showSearch(true).visible(visible).loadData(
                [&](auto request) { requests.push_back(request); }));
        }});
        sync(*f);
        const auto id = f->services.cascader().mounted().front();
        auto s = f->services.cascader().snapshot(id);
        click(*f, s.rows[0].component);
        check(requests.size() == 1, "load start");
        if (boundary == 0) {
            const auto input = f->inputs.mounted_inputs().front();
            f->services.focus().request_focus(input.interaction, FocusModality::keyboard);
            (void)f->inputs.dispatch(TextCommitted{String{u8"search"}, f->inputs.sessions().active()});
        } else if (boundary == 1) {
            click(*f, s.rows[0].component);
        } else if (boundary == 2) {
            data.set(std::vector<CascaderOption>{{u8"new", 1., u8"New"}});
        } else if (boundary == 3) {
            visible.set(false);
        } else if (boundary == 4) {
            f->services.destroy(id);
        } else if (boundary == 5) {
            f->services.dispose();
        } else {
            f.reset();
        }
        check(requests[0].cancelled->load() && !requests[0].complete({}),
              "cancelled generation accepted old completion");
    }
    Fixture f;
    std::vector<CascaderLoadRequest> requests;
    f.services.mount(Content{[&] {
        CascaderPanel(CascaderProps{}
                          .options(std::vector<CascaderOption>{{u8"lazy", 1., u8"Lazy", {}, false, false, false}})
                          .loadData([&](auto r) { requests.push_back(r); }));
    }});
    sync(f);
    auto id = f.services.cascader().mounted().front();
    auto s = f.services.cascader().snapshot(id);
    click(f, s.rows[0].component);
    std::atomic<int> foreign{};
    std::vector<std::thread> workers;
    for (int i = 0; i < 8; ++i) {
        workers.emplace_back([&] {
            for (int j = 0; j < 50; ++j) {
                if (requests[0].complete({})) {
                    ++foreign;
                }
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    check(foreign == 0, "foreign completion raced UI");
    check(requests[0].complete({{}, String{u8"retry error"}}) && !requests[0].complete({}),
          "error/duplicate completion");
    sync(f);
    s = f.services.cascader().snapshot(id);
    const auto status_bounds = bounds(f, s.status);
    const auto options_viewport = bounds(f, s.rows[0].viewport);
    check(s.error == String{u8"retry error"} && status_bounds.height > 0 &&
              status_bounds.y + status_bounds.height <= options_viewport.y + .01F,
          "loader error status overlaps option viewport");
    const auto retry_bounds = bounds(f, s.retry);
    const auto retry_hit = f.services.hit_test().hit_test(
        {retry_bounds.x + retry_bounds.width / 2, retry_bounds.y + retry_bounds.height / 2});
    check(retry_hit && f.services.interactions().require(*retry_hit).component == s.retry,
          "Retry has no actual input target");
    click(f, s.retry);
    check(requests.size() == 2 && requests[1].complete({{{u8"child", 2., u8"Child"}}, {}}), "retry completion");
    sync(f);
    check(f.services.cascader().snapshot(id).rows.size() == 2 && f.services.cascader().snapshot(id).error.empty() &&
              !f.services.components().branch_active(f.services.cascader().snapshot(id).status) &&
              !f.services.next_frame_deadline(),
          "loaded tree, status cleanup or idle scheduling");
}

void exceptional_content() {
    Fixture f;
    Signal<std::vector<CascaderOption>> data{options()};
    InputRef unrelated;
    f.services.mount(Content{[&] {
        Input(InputProps{}.ref(unrelated).defaultValue(String{u8"healthy"}));
        CascaderPanel(CascaderProps{}.options(data));
    }});
    sync(f);
    auto next = options();
    next.push_back({u8"throws", 3., u8"throws", {}, false, false, true, CascaderOptionContent{[] {
                        Input(InputProps{});
                        throw std::runtime_error("slot failed");
                    }}});
    bool failed = false;
    try {
        data.set(next);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    sync(f);
    check(failed && f.services.cascader().mounted().empty() && unrelated.focus(),
          "failed append did not retire owner or damaged unrelated Input");
    f.services.dispose();
    check(f.nodes.size() == 0 && f.services.surfaces().size() == 0, "failed factory leaked resources");
    Fixture g;
    g.services.mount(Content{[&] {
        Input(InputProps{}.ref(unrelated));
        CascaderPanel(CascaderProps{}.options(options()).changeOnSelect(true).onChange(
            [](auto) { throw std::runtime_error("callback"); }));
    }});
    sync(g);
    auto id = g.services.cascader().mounted().front();
    auto s = g.services.cascader().snapshot(id);
    failed = false;
    try {
        click(g, s.rows[0].component);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    sync(g);
    check(failed && g.services.cascader().mounted().empty() && unrelated.focus(),
          "callback exception left half-live Cascader");
    g.services.dispose();
    Fixture h;
    std::shared_ptr<theme_runtime::ThemeScope> theme;
    Signal<CascaderPaths> value{CascaderPaths{}};
    h.services.mount(Content{[&] {
        Input(InputProps{}.ref(unrelated));
        Theme(ThemeProps{}, ThemeContent{[&] {
                  Cascader(CascaderProps{}.options(options()).multiple(true).value(value).displayRender([&](auto) {
                      if (theme) {
                          ThemeConfig changed;
                          changed.cascader.tokens.tag_background = Color::rgba8(20, 70, 40);
                          theme->update(changed);
                      }
                      return String{u8"tag"};
                  }));
              }});
    }});
    sync(h);
    theme = h.services.components().theme_scope(h.services.cascader().mounted().front());
    value.set({{String{u8"中国"}, 1.0}});
    sync(h);
    check(h.services.cascader().mounted().empty() && unrelated.focus(),
          "reentrant tag Theme callback kept a stale snapshot or damaged sibling");
    h.services.dispose();
}

void nested_popup_departure() {
    Fixture f;
    CascaderRef ref;
    InputRef outside;
    InputRef inner;
    f.services.mount(Content{[&] {
        Cascader(CascaderProps{}
                     .options(options())
                     .defaultOpen(true)
                     .ref(ref)
                     .showSearch(true)
                     .popupContent(CascaderPopupContent{[&] {
                         Popover(PopoverProps{}.defaultOpen(true),
                                 PopoverTrigger{[] { Button(ButtonProps{}, ButtonContent{[] { Text(u8"nested"); }}); }},
                                 PopoverSlots{{}, PopoverContent{[&] { Input(InputProps{}.ref(inner)); }}});
                     }})
                     .layout(LayoutStyle{}.width(dp(300))));
        Input(InputProps{}.ref(outside));
    }});
    sync(f);
    const auto id = f.services.cascader().mounted().front();
    check(inner.bound(), "nested popup editor bound");
    (void)inner.focus();
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"候选"}, {}, stamp})), "nested popup preedit");
    const auto panel = f.services.cascader().snapshot(id).panel;
    const auto child =
        *std::ranges::find_if(f.services.popover().mounted(), [&](auto popup) { return descendant(f, popup, panel); });
    key(f, Key::escape);
    check(f.services.cascader().snapshot(id).open && f.services.popover().snapshot(child).geometry.visible &&
              !f.inputs.editors().require(stamp.owner).composition().active,
          "IME Escape lost innermost precedence");
    key(f, Key::escape);
    check(f.services.cascader().snapshot(id).open, "child Escape closed Cascader");
    check(!f.services.popover().snapshot(child).geometry.visible, "child Escape did not dismiss nested popup");
    check(ref.focus(), "selector ref focus");
    key(f, Key::escape);
    check(!f.services.cascader().snapshot(id).open, "selector Escape did not close");
    check(ref.bound(), "selector refocus");
    (void)ref.focus();
    key(f, Key::down);
    check(f.services.cascader().snapshot(id).open, "keyboard reopen");
    check(outside.focus(), "outside focus");
    sync(f);
    check(!f.services.cascader().snapshot(id).open && !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stamp})),
          "focus departure left popup/session live");
    check(ref.bound(), "selector Tab focus");
    (void)ref.focus();
    key(f, Key::tab);
    key(f, Key::tab, KeyModifier::shift);
    f.services.dispose();
    check(!f.inputs.sessions().active().valid() && f.services.surfaces().size() == 0, "nested lifecycle leaked");
    Fixture rich;
    InputRef child_input;
    int expansions = 0;
    auto rich_options = options();
    rich_options[0].content = CascaderOptionContent{[&] {
        Popover(PopoverProps{}.defaultOpen(true),
                PopoverTrigger{[] { Button(ButtonProps{}, ButtonContent{[] { Text(u8"popup"); }}); }},
                PopoverSlots{{}, PopoverContent{[&] { Input(InputProps{}.ref(child_input)); }}});
    }};
    rich.services.mount(Content{[&] {
        CascaderPanel(CascaderProps{}
                          .options(rich_options)
                          .expandTrigger(CascaderExpandTrigger::Hover)
                          .onExpand([&](auto) { ++expansions; })
                          .layout(LayoutStyle{}.width(dp(300))));
    }});
    sync(rich);
    check(child_input.focus(), "rich child popup input focus");
    const auto child_editor = *std::ranges::find_if(rich.inputs.mounted_inputs(), [&](const auto& editor) {
        return editor.editor == rich.inputs.sessions().active().owner;
    });
    const auto child_bounds = bounds(rich, child_editor.component);
    rich.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::move, PointerButton::none, child_bounds.x + 4, child_bounds.y + 4});
    check(expansions == 0, "child popup hover expanded its parent Cascader row");

    Fixture controlled;
    Signal<bool> opened{true};
    InputRef controlled_inner;
    InputRef controlled_outside;
    int close_requests = 0;
    controlled.services.mount(Content{[&] {
        Cascader(CascaderProps{}
                     .options(options())
                     .open(opened)
                     .popupContent(CascaderPopupContent{[&] {
                         Popover(PopoverProps{}.open(true).onOpenChange([&](bool value) {
                             if (!value) {
                                 ++close_requests;
                             }
                         }),
                                 PopoverTrigger{[] { Text(u8"controlled child"); }},
                                 PopoverSlots{{}, PopoverContent{[&] { Input(InputProps{}.ref(controlled_inner)); }}});
                     }})
                     .layout(LayoutStyle{}.width(dp(300))));
        Input(InputProps{}.ref(controlled_outside));
        Popover(PopoverProps{}.open(true), PopoverTrigger{[] { Text(u8"unrelated popup"); }},
                PopoverSlots{{}, PopoverContent{[] { Text(u8"sibling"); }}});
    }});
    sync(controlled);
    const auto controlled_id = controlled.services.cascader().mounted().front();
    const auto controlled_panel = controlled.services.cascader().snapshot(controlled_id).panel;
    const auto owned_child = *std::ranges::find_if(controlled.services.popover().mounted(), [&](auto popup) {
        return descendant(controlled, popup, controlled_panel);
    });
    const auto sibling = controlled.services.popover().mounted().back();
    check(controlled_inner.focus(), "controlled popup editor focus");
    const auto controlled_stamp = controlled.inputs.sessions().active();
    check(controlled_outside.focus(), "controlled popup focus departure");
    sync(controlled);
    check(close_requests > 0 && controlled.services.cascader().snapshot(controlled_id).open &&
              controlled.services.popover().snapshot(owned_child).geometry.visible &&
              controlled.services.popover().snapshot(sibling).geometry.visible,
          "descendant dismissal overrode controlled authority or touched sibling");
    opened.set(false);
    sync(controlled);
    check(!controlled.services.popover().snapshot(owned_child).geometry.visible &&
              controlled.services.popover().snapshot(sibling).geometry.visible &&
              !bool(controlled.inputs.dispatch(TextCommitted{String{u8"late"}, controlled_stamp})),
          "closed Cascader failed to suppress controlled descendant independently");
}

void dynamic_factories_and_search() {
    Fixture f;
    int first = 0;
    int replacement = 0;
    auto data = options();
    data[0].content = CascaderOptionContent{[&] {
        ++first;
        Input(InputProps{}.defaultValue(String{u8"kept draft"}));
    }};
    Signal<std::vector<CascaderOption>> options_signal{data};
    Signal<String> query{String{u8"a"}};
    CascaderSearch search;
    search.limit = 1;
    search.filter = [](const String&, const auto& path) {
        return path.size() == 2;
    };
    search.sort = [](const auto& a, const auto& b, const String&) {
        return std::get<double>(a.back()) > std::get<double>(b.back());
    };
    search.render = [](const String&, const auto& path) {
        return path.back().label;
    };
    f.services.mount(Content{[&] {
        CascaderPanel(CascaderProps{}.options(options_signal).showSearch(true).searchValue(query).search(search));
    }});
    sync(f);
    const auto id = f.services.cascader().mounted().front();
    auto snap = f.services.cascader().snapshot(id);
    check(!snap.rows[1].shown && snap.rows[2].shown, "custom search filter/sort/render/limit");
    auto changed = data;
    changed[0].content = CascaderOptionContent{[&] {
        ++replacement;
        Input(InputProps{}.defaultValue(String{u8"new draft"}));
    }};
    options_signal.set(changed);
    sync(f);
    check(first == 1 && replacement == 0, "same revision replaced a retained factory");
    changed[0].content_revision = 1;
    options_signal.set(changed);
    sync(f);
    check(replacement == 1, "explicit content revision did not replace factory");
    query.set(String{});
    sync(f);
    check(f.services.cascader().snapshot(id).rows[0].shown, "controlled search authority not applied");
    const auto rich_editor = f.inputs.mounted_inputs().back();
    check(f.services.focus().request_focus(rich_editor.interaction, FocusModality::keyboard),
          "replacement rich editor focus");
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, f.inputs.sessions().active()})),
          "replacement rich preedit");
    options_signal.set(std::vector<CascaderOption>{options().back()});
    sync(f);
    check(f.services.cascader().mounted().size() == 1 && f.services.cascader().snapshot(id).rows.size() == 1 &&
              !f.inputs.editors().find(rich_editor.editor),
          "removing focused rich option retired healthy Cascader or retained old editor");
    f.services.dispose();
    Fixture g;
    Signal<std::vector<CascaderOption>> reentrant{options()};
    bool update = false;
    CascaderSearch invalid;
    invalid.filter = [&](const String&, const auto&) {
        if (update) {
            auto changed = options();
            changed.back().label = String{u8"reentrant change"};
            reentrant.set(changed);
        }
        return true;
    };
    g.services.mount(
        Content{[&] { CascaderPanel(CascaderProps{}.options(reentrant).showSearch(true).search(invalid)); }});
    sync(g);
    update = true;
    const auto input = g.inputs.mounted_inputs().front();
    g.services.focus().request_focus(input.interaction, FocusModality::keyboard);
    (void)g.inputs.dispatch(TextCommitted{String{u8"query"}, g.inputs.sessions().active()});
    sync(g);
    check(g.services.cascader().mounted().empty(), "reentrant search options left half-created component");
}

void responsive_and_rtl() {
    Fixture f;
    Signal<LogicalLength> width{dp(400)};
    auto data_options = options();
    data_options.push_back({u8"eu", String{u8"Europe"}, u8"Europe", {{u8"paris", String{u8"Paris"}, u8"Paris"}}});
    Signal<CascaderMaxTagCount> limit{CascaderMaxTagCount{CascaderTagCount::Responsive}};
    f.services.mount(Content{[&] {
        Cascader(CascaderProps{}
                     .options(data_options)
                     .multiple(true)
                     .defaultValue(
                         {{String{u8"中国"}, 1.0}, {String{u8"中国"}, 2.0}, {String{u8"Europe"}, String{u8"Paris"}}})
                     .maxTagCount(limit)
                     .maxTagPlaceholder([](const auto& omitted) {
                         return String::from_utf8("还有 " + std::to_string(omitted.size()) + " 项").value();
                     })
                     .direction(TextDirection::RightToLeft)
                     .layout(LayoutStyle{}.width(width)));
    }});
    sync(f);
    const auto id = f.services.cascader().mounted().front();
    auto s = f.services.cascader().snapshot(id);
    check(std::ranges::any_of(s.tags, [](const auto& t) { return t.shown; }), "wide responsive tags hid every item");
    const auto first = s.tags[0].component;
    width.set(dp(170));
    sync(f);
    s = f.services.cascader().snapshot(id);
    check(std::ranges::any_of(s.tags, [](const auto& t) { return !t.shown; }), "responsive tags did not overflow");
    width.set(dp(460));
    sync(f);
    check(f.services.cascader().snapshot(id).tags[0].component == first, "responsive width remounted tag");
    for (int settle = 0; settle < 4; ++settle) {
        sync(f);
    }
    f.dirty.clear();
    (void)f.frames.consume_request();
    sync(f);
    check(!f.frames.pending(), "responsive placeholder fitting requested frames forever");
    f.services.dispose();
    Fixture g;
    Signal<LogicalLength> panel_width{dp(280)};
    Signal<std::vector<CascaderOption>> data{options()};
    g.services.mount(Content{[&] {
        CascaderPanel(CascaderProps{}
                          .options(data)
                          .direction(TextDirection::RightToLeft)
                          .expandTrigger(CascaderExpandTrigger::Hover)
                          .changeOnSelect(true)
                          .listHeight(120)
                          .layout(LayoutStyle{}.width(panel_width)));
    }});
    sync(g);
    auto owner = g.services.cascader().mounted().front();
    auto snap = g.services.cascader().snapshot(owner);
    auto rect = bounds(g, snap.rows[0].component);
    g.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::move, PointerButton::none, rect.x + 10, rect.y + 10});
    sync(g);
    snap = g.services.cascader().snapshot(owner);
    check(snap.rows[1].shown && snap.value.empty() &&
              bounds(g, snap.rows[1].component).x < bounds(g, snap.rows[0].component).x,
          "hover/RTL columns");
    auto hit = std::ranges::find_if(g.buttons.mounted_buttons(),
                                    [&](const auto& b) { return b.component == snap.rows[0].component; });
    g.services.focus().request_focus(hit->interaction, FocusModality::keyboard);
    key(g, Key::left);
    check(g.services.focus().state().focused != hit->interaction, "RTL forward arrow did not move column");
    auto updated = options();
    updated[0].children.pop_back();
    data.set(updated);
    sync(g);
    check(g.services.cascader().snapshot(owner).rows.size() == 3, "dynamic content remeasure");
    panel_width.set(dp(70));
    sync(g);
    snap = g.services.cascader().snapshot(owner);
    const auto child = bounds(g, snap.rows[1].component);
    const auto panel = bounds(g, snap.panel);
    check(child.width > 0 && child.x >= panel.x - .01F && child.x + child.width <= panel.x + panel.width + .01F,
          "narrow RTL focused column was clipped away");
}

void loader_failure_reentry() {
    Fixture f;
    Signal<std::vector<CascaderOption>> data{
        std::vector<CascaderOption>{{u8"lazy", 1., u8"Lazy", {}, false, false, false}}};
    CascaderLoadRequest saved;
    f.services.mount(Content{[&] {
        CascaderPanel(CascaderProps{}.options(data).loadData([&](auto request) {
            saved = request;
            data.set(std::vector<CascaderOption>{{u8"new", 2., u8"New"}});
        }));
    }});
    sync(f);
    auto id = f.services.cascader().mounted().front();
    click(f, f.services.cascader().snapshot(id).rows[0].component);
    check(saved.cancelled->load() && !saved.complete({}) && f.services.cascader().snapshot(id).rows.size() == 1,
          "reentrant loader options update");
    f.services.dispose();
    Fixture g;
    CascaderLoadRequest failed;
    g.services.mount(Content{[&] {
        CascaderPanel(CascaderProps{}
                          .options(std::vector<CascaderOption>{{u8"lazy", 1., u8"Lazy", {}, false, false, false}})
                          .loadData([&](auto request) {
                              failed = request;
                              throw std::runtime_error("loader failed");
                          }));
    }});
    sync(g);
    id = g.services.cascader().mounted().front();
    bool threw = false;
    try {
        click(g, g.services.cascader().snapshot(id).rows[0].component);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    sync(g);
    check(threw && failed.cancelled->load() && !failed.complete({}) && !g.services.next_frame_deadline(),
          "throwing loader retained work");
}

void custom_trigger_and_empty() {
    Fixture f;
    CascaderRef ref;
    int trigger_factories = 0;
    int empty_factories = 0;
    int focus = 0;
    f.services.mount(Content{[&] {
        Cascader(CascaderProps{}
                     .ref(ref)
                     .triggerContent(CascaderTriggerContent{[&] {
                         ++trigger_factories;
                         Text(u8"Custom trigger");
                     }})
                     .emptyContent(CascaderEmptyContent{[&] {
                         ++empty_factories;
                         Text(u8"Custom empty");
                     }})
                     .onFocus([&] { ++focus; }));
    }});
    sync(f);
    const auto id = f.services.cascader().mounted().front();
    auto snap = f.services.cascader().snapshot(id);
    check(ref.bound() && ref.focus(), "custom trigger native ref");
    key(f, Key::enter);
    check(f.services.cascader().snapshot(id).open && focus == 1 && trigger_factories == 1 && empty_factories == 1,
          "custom trigger/empty retained slots");
    key(f, Key::escape);
    click(f, snap.selector);
    check(f.services.cascader().snapshot(id).open, "custom trigger pointer toggled twice");
    f.services.dispose();
    check(!ref.bound(), "custom trigger ref survived destruction");
}

void native_props_matrix() {
    Fixture f;
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<InputStatus> status{InputStatus::Default};
    Signal<InputVariant> variant{InputVariant::Outlined};
    Signal<PopoverPlacement> placement{PopoverPlacement::BottomLeft};
    Signal<bool> opened{true};
    CascaderRef ref;
    std::optional<bool> open_request;
    f.services.mount(Content{[&] {
        Cascader(CascaderProps{}
                     .options(options())
                     .defaultValue({{String{u8"中国"}, 1.0}})
                     .size(size)
                     .status(status)
                     .variant(variant)
                     .placement(placement)
                     .open(opened)
                     .onOpenChange([&](bool value) { open_request = value; })
                     .ref(ref)
                     .layout(LayoutStyle{}.width(dp(320))));
    }});
    sync(f);
    const auto id = f.services.cascader().mounted().front();
    const auto selector = f.services.cascader().snapshot(id).selector;
    check(f.services.cascader().snapshot(id).rows[1].shown, "initial selected path did not expand");
    for (auto v : {InputVariant::Outlined, InputVariant::Filled, InputVariant::Borderless, InputVariant::Underlined}) {
        variant.set(v);
        for (auto s : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
            size.set(s);
            status.set(s == ControlSize::Small   ? InputStatus::Error
                       : s == ControlSize::Large ? InputStatus::Warning
                                                 : InputStatus::Default);
            sync(f);
            check(f.inputs.variant(selector) == v && f.inputs.size(selector) == s &&
                      f.inputs.status(selector) == status.get(),
                  "Cascader selector did not forward native variant/size/status");
            check(bounds(f, selector).width > 0 && bounds(f, selector).height > 0,
                  "selector variant has no actual layout");
        }
    }
    for (auto p : {PopoverPlacement::BottomLeft, PopoverPlacement::BottomRight, PopoverPlacement::TopLeft,
                   PopoverPlacement::TopRight}) {
        placement.set(p);
        sync(f);
        const auto panel = f.services.cascader().snapshot(id).panel;
        const auto popup_owner = *std::ranges::find_if(f.services.popover().mounted(), [&](auto popup) {
            return descendant(f, panel, f.services.popover().snapshot(popup).popup);
        });
        const auto popup = f.services.popover().snapshot(popup_owner);
        check(popup.geometry.visible && popup.geometry.bounds.width > 0, "native popup placement has no geometry");
    }
    (void)ref.focus();
    key(f, Key::escape);
    const auto controlled_panel = f.services.cascader().snapshot(id).panel;
    const auto controlled_popup = *std::ranges::find_if(f.services.popover().mounted(), [&](auto popup) {
        return descendant(f, controlled_panel, f.services.popover().snapshot(popup).popup);
    });
    check(open_request == false && f.services.cascader().snapshot(id).open &&
              f.services.popover().snapshot(controlled_popup).geometry.visible,
          "controlled open refusal changed actual popup authority");
    opened.set(false);
    sync(f);
    check(!f.services.cascader().snapshot(id).open, "controlled close authority");
    f.services.dispose();
    check(!ref.bound(), "native matrix ref cleanup");
    Fixture tagged;
    Signal<ControlSize> tag_size{ControlSize::Middle};
    tagged.services.mount(Content{[&] {
        Cascader(CascaderProps{}
                     .options(options())
                     .multiple(true)
                     .size(tag_size)
                     .defaultValue({{String{u8"中国"}, 1.0}})
                     .layout(LayoutStyle{}.width(dp(400))));
    }});
    for (const auto& [size, height] :
         std::array{std::pair{ControlSize::Small, 24.F}, std::pair{ControlSize::Middle, 32.F},
                    std::pair{ControlSize::Large, 40.F}}) {
        tag_size.set(size);
        sync(tagged);
        const auto snap = tagged.services.cascader().snapshot(tagged.services.cascader().mounted().front());
        check(std::abs(bounds(tagged, snap.selector).height - height) < .1F &&
                  bounds(tagged, snap.tags[0].component).height <= height - 8.F + .1F,
              "multiple tag size did not follow the actual selector size");
    }
}

void disabled_conduction_and_overflow() {
    Fixture f;
    auto data = options();
    data[0].children[0].disabled = true;
    data[0].children[1].disable_checkbox = true;
    f.services.mount(Content{[&] { CascaderPanel(CascaderProps{}.options(data).multiple(true)); }});
    sync(f);
    const auto id = f.services.cascader().mounted().front();
    auto s = f.services.cascader().snapshot(id);
    click(f, s.rows[0].component);
    s = f.services.cascader().snapshot(id);
    check(s.value.empty() && s.rows[1].disabled && s.rows[2].shown,
          "parent selection crossed disabled checkbox boundary");
    f.services.dispose();
    Fixture g;
    InputRef outside;
    Signal<CascaderPaths> value{CascaderPaths{{String{u8"中国"}, 1.0}, {String{u8"中国"}, 2.0}}};
    g.services.mount(Content{[&] {
        Cascader(CascaderProps{}
                     .options(options())
                     .multiple(true)
                     .value(value)
                     .maxTagCount(std::size_t{0})
                     .onChange([&](auto paths) { value.set(paths); })
                     .layout(LayoutStyle{}.width(dp(300))));
        Input(InputProps{}.ref(outside));
    }});
    sync(g);
    const auto owner = g.services.cascader().mounted().front();
    auto tags = g.services.cascader().snapshot(owner);
    check(tags.tags.size() == 2 && std::ranges::none_of(tags.tags, [](auto& tag) { return tag.shown; }),
          "zero maxTagCount did not omit actual tags");
    click(g, tags.overflow);
    const auto overflow_it = std::ranges::find_if(g.services.popover().mounted(), [&](auto popup) {
        return g.services.popover().snapshot(popup).geometry.visible;
    });
    check(overflow_it != g.services.popover().mounted().end(), "overflow click did not open an owned popup");
    const auto popup = g.services.popover().snapshot(*overflow_it);
    check(popup.geometry.visible && !g.services.cascader().snapshot(owner).open, "tag overflow did not own its popup");
    const auto action = std::ranges::find_if(g.buttons.mounted_buttons(), [&](auto& b) {
        return descendant(g, b.component, popup.popup) && g.services.interactions().branch_active(b.interaction);
    });
    check(action != g.buttons.mounted_buttons().end(), "omitted values have no interactive removal");
    click(g, action->component);
    check(g.services.cascader().snapshot(owner).tags.size() == 1,
          "overflow removal did not update actual controlled tags");
    (void)outside.focus();
    sync(g);
    check(std::ranges::none_of(g.services.popover().mounted(),
                               [&](auto popup) { return g.services.popover().snapshot(popup).geometry.visible; }),
          "focus departure kept a Cascader-owned overflow popup open");
}

void typography_scene() {
    Fixture f;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    auto initial = theme.get();
    initial.cascader.tokens.selector_height = dp(48);
    initial.cascader.tokens.font_size = dp(16);
    initial.cascader.tokens.line_height = dp(31);
    theme.set(initial);
    int factories = 0;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  ++factories;
                  Cascader(CascaderProps{}.options(options()).showSearch(true).defaultSearchValue(String{u8"query"}));
              }});
    }});
    sync(f);
    const auto id = f.services.cascader().mounted().front();
    const auto selector = f.services.cascader().snapshot(id).selector;
    const auto scene = f.inputs.text_scene(selector);
    check(std::abs(f.scene.text_state(scene).measurement().height - 31.F) < .1F,
          "Cascader typography did not reach actual selector glyph scene");
    initial.cascader.tokens.line_height = dp(37);
    theme.set(initial);
    sync(f);
    check(factories == 1 && std::abs(f.scene.text_state(scene).measurement().height - 37.F) < .1F,
          "typography update remounted a factory or retained stale glyph layout");
}

void selector_composition() {
    Fixture f;
    Signal<IconSource> search_icon{IconSource{IconName::InfoCircleFilled}};
    CascaderRef ref;
    Signal<bool> readonly{false};
    f.services.mount(Content{[&] {
        Cascader(CascaderProps{}
                     .options(options())
                     .showSearch(true)
                     .searchIcon(search_icon)
                     .readOnly(readonly)
                     .defaultOpen(true)
                     .ref(ref)
                     .layout(LayoutStyle{}.width(dp(300))));
    }});
    sync(f);
    const auto id = f.services.cascader().mounted().front();
    auto s = f.services.cascader().snapshot(id);
    const auto editor = *std::ranges::find_if(f.inputs.mounted_inputs(),
                                              [&](const auto& input) { return input.component == s.selector; });
    check(f.services.text().icon_snapshot(s.arrow).source == IconSource{IconName::InfoCircleFilled},
          "search icon did not reach actual selector scene");
    search_icon.set(IconSource{IconName::QuestionCircleFilled});
    check(f.services.text().icon_snapshot(s.arrow).source == IconSource{IconName::QuestionCircleFilled},
          "reactive search icon did not update actual selector scene");
    check(ref.bound(), "selector IME ref");
    (void)ref.focus();
    auto stamp = f.inputs.sessions().active();
    check(stamp.valid() && stamp.owner == editor.editor, "editable selector has no owner session");
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"预编辑"}, {}, stamp})), "selector rejected Core preedit");
    key(f, Key::escape);
    check(!f.inputs.editors().require(editor.editor).composition().active && f.services.cascader().snapshot(id).open,
          "selector composition Escape closed popup");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"杭州"}, f.inputs.sessions().active()})),
          "selector query commit");
    sync(f);
    s = f.services.cascader().snapshot(id);
    check(std::ranges::count_if(s.rows, [](const auto& row) { return row.shown; }) == 1 && s.rows[1].shown,
          "selector query did not filter actual column");
    stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"候选"}, {}, stamp})), "second selector preedit");
    readonly.set(true);
    sync(f);
    check(!f.inputs.editors().require(editor.editor).composition().active &&
              !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stamp})),
          "readonly selector retained preedit/session");
    f.services.dispose();
    check(!ref.bound() && !f.inputs.sessions().active().valid(), "selector IME retirement");
    Fixture tagged;
    CascaderRef tagged_ref;
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    tagged.services.mount(Content{[&] {
        Cascader(CascaderProps{}
                     .options(options())
                     .multiple(true)
                     .showSearch(true)
                     .defaultValue({{String{u8"中国"}, 1.0}})
                     .ref(tagged_ref)
                     .direction(direction)
                     .layout(LayoutStyle{}.width(dp(340))));
    }});
    sync(tagged);
    (void)tagged_ref.focus();
    check(bool(tagged.inputs.dispatch(TextCommitted{String{u8"杭"}, tagged.inputs.sessions().active()})),
          "multiple selector query commit");
    for (auto d : {TextDirection::LeftToRight, TextDirection::RightToLeft}) {
        direction.set(d);
        sync(tagged);
        const auto snap = tagged.services.cascader().snapshot(tagged.services.cascader().mounted().front());
        const auto viewport = tagged.inputs.layout_snapshot(snap.selector).viewport;
        const auto tag = bounds(tagged, snap.tags[0].component);
        check(viewport.width > 0 &&
                  (tag.x + tag.width <= viewport.x + .1F || viewport.x + viewport.width <= tag.x + .1F),
              "multiple tag painted over editable selector query");
    }
    Fixture g;
    g.services.mount(Content{[] { Cascader(CascaderProps{}.options(options())); }});
    sync(g);
    const auto owner = g.services.cascader().mounted().front();
    const auto selector = g.services.cascader().snapshot(owner).selector;
    const auto view = *std::ranges::find_if(g.inputs.mounted_inputs(),
                                            [&](const auto& input) { return input.component == selector; });
    g.services.focus().request_focus(view.interaction, FocusModality::keyboard);
    check(!bool(g.inputs.dispatch(CompositionChanged{String{u8"blocked"}, {}, g.inputs.sessions().active()})),
          "non-search selector accepted editable preedit");
}

} // namespace

int main(int argc, char** argv) {
    const std::array cases{std::pair{"controlled", controlled},
                           std::pair{"multiple_async", multiple_async},
                           std::pair{"input_lifecycle", input_lifecycle},
                           std::pair{"wheel_keyboard", wheel_keyboard},
                           std::pair{"popup", popup},
                           std::pair{"tags_theme", tags_theme},
                           std::pair{"retained_custom_tags", retained_custom_tags},
                           std::pair{"clipping_search_keyboard", clipping_search_keyboard},
                           std::pair{"composition_boundaries", composition_boundaries},
                           std::pair{"loader_lifecycle", loader_lifecycle},
                           std::pair{"exceptional_content", exceptional_content},
                           std::pair{"nested_popup_departure", nested_popup_departure},
                           std::pair{"dynamic_factories_and_search", dynamic_factories_and_search},
                           std::pair{"responsive_and_rtl", responsive_and_rtl},
                           std::pair{"loader_failure_reentry", loader_failure_reentry},
                           std::pair{"custom_trigger_and_empty", custom_trigger_and_empty},
                           std::pair{"native_props_matrix", native_props_matrix},
                           std::pair{"disabled_conduction_and_overflow", disabled_conduction_and_overflow},
                           std::pair{"selector_composition", selector_composition},
                           std::pair{"typography_scene", typography_scene}};
    try {
        std::size_t count = 0;
        for (const auto& [name, run] : cases) {
            if (argc > 1 && std::string_view(argv[1]) != name) {
                continue;
            }
            std::cout << name << std::endl;
            run();
            ++count;
        }
        if (!count) {
            throw std::runtime_error("unknown Cascader scenario");
        }
        std::cout << "Cascader " << count << " scenario groups passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
