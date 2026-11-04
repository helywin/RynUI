#include "support/auto_complete_fixture.hpp"
#include "component/select_component.hpp"
#include "component/select_model.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/flex.hpp>
#include <thread>

namespace {
using namespace auto_complete_test;

auto selection(Fixture& f, runtime::ComponentId id) {
    return f.services.select().snapshot(id);
}

void multiple_tags() {
    Fixture custom_tags;
    SelectRef custom_ref;
    SelectRef sibling_ref;
    Signal<String> sibling_feedback{String{}};
    custom_tags.services.mount(Content{[&] {
        Select(SelectProps{}
                   .mode(SelectMode::Tags)
                   .ref(custom_ref)
                   .tokenSeparators({u8",", u8"；"})
                   .responsiveMaxTagCount()
                   .tagRender(SelectTagRenderer{[](SelectRenderInfo info) {
                       Button(ButtonProps{}.size(ControlSize::Small).disabled(info.disabled).onClick(info.remove),
                              ButtonContent{[label = info.label] { Text(TextProps{}.content(label)); }});
                   }})
                   .layout(LayoutStyle{}.width(dp(340))));
        Text(TextProps{}.content(sibling_feedback));
        Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}).ref(sibling_ref).showSearch(), SelectPrefix{[&] {
                   Button(ButtonProps{}.onClick([&] { sibling_feedback.set(u8"interactive prefix"); }),
                          ButtonContent{[] { Text(u8"Prefix"); }});
               }});
    }});
    sync(custom_tags);
    const auto custom_id = custom_tags.services.select().mounted()[0];
    check(custom_ref.focus(), "responsive custom Tags focus");
    edit(custom_tags, u8"中文；英文,尾部");
    key(custom_tags, Key::enter);
    check(selection(custom_tags, custom_id).values.size() == 3, "responsive custom Tags commit");
    click(custom_tags, selection(custom_tags, custom_id).tags.front());
    check(selection(custom_tags, custom_id).values.size() == 2, "responsive custom Tags removal");
    click(custom_tags, custom_tags.buttons.mounted_buttons().front().component);
    check(sibling_ref.focus(), "sibling Select focus");
    key(custom_tags, Key::down);
    sync(custom_tags);
    clean(custom_tags);
    Fixture f;
    SelectRef ref;
    Signal<SelectValues> values{SelectValues{SelectValue{1}}};
    bool accept = true;
    std::vector<SelectValues> intents;
    std::vector<String> events;
    f.services.mount(Content{[&] {
        Select(SelectProps{}
                   .mode(SelectMode::Tags)
                   .values(values)
                   .ref(ref)
                   .options({{1, u8"One"}, {2, u8"Two"}, {3, u8"Disabled", true}, {4, u8"Four"}})
                   .tokenSeparators({u8",", u8"；"})
                   .maxTagCount(std::size_t{1})
                   .maxTagTextLength(std::size_t{2})
                   .onValuesChange([&](SelectValues intent) {
                       events.push_back(u8"change");
                       intents.push_back(intent);
                       if (accept) {
                           values.set(std::move(intent));
                       }
                   })
                   .onSelect([&](auto, auto) { events.push_back(u8"select"); })
                   .onDeselect([&](auto, auto) { events.push_back(u8"deselect"); })
                   .layout(LayoutStyle{}.width(dp(320))));
    }});
    sync(f);
    const auto id = f.services.select().mounted()[0];
    check(ref.focus(), "multiple focus");
    key(f, Key::down);
    edit(f, u8"Two,中文；尾部");
    auto s = selection(f, id);
    check(s.values == SelectValues{SelectValue{1}, SelectValue{2}, SelectValue{u8"中文"}} &&
              s.query == String{u8"尾部"} && s.visible,
          "tags did not tokenize real editor input");
    check(!s.overflow.empty() && s.tags.size() == 4 &&
              events == std::vector<String>{u8"change", u8"select", u8"select"},
          "tag overflow/events");
    check(f.inputs.layout_snapshot(s.selector).viewport.width > 20, "omitted tags consumed editable layout width");
    key(f, Key::enter);
    check(selection(f, id).values.size() == 4 && selection(f, id).query.empty() && selection(f, id).visible,
          "tags Enter did not commit free value");
    key(f, Key::backspace);
    check(selection(f, id).values.size() == 3 && events.back() == String{u8"deselect"},
          "empty Backspace did not remove last tag");
    accept = false;
    edit(f, u8"rejected");
    key(f, Key::enter);
    check(selection(f, id).values.size() == 3 && intents.back().size() == 4 && selection(f, id).query.empty(),
          "controlled multi refusal changed authority");
    accept = true;
    edit(f, u8"pointer-created");
    const auto created = selection(f, id);
    check(created.active.has_value(), "Tags query did not create a selectable candidate");
    const auto candidate = std::ranges::find(created.row_indices, *created.active);
    check(candidate != created.row_indices.end(), "Tags candidate was not realized");
    click(f, created.rows[static_cast<std::size_t>(candidate - created.row_indices.begin())]);
    check(selection(f, id).values.back() == SelectValue{u8"pointer-created"}, "Tags pointer did not create value");
    edit(f, u8"Fo");
    key(f, Key::down);
    key(f, Key::enter);
    check(selection(f, id).values.back() == SelectValue{4}, "Tags Enter ignored keyboard-active existing option");
    clean(f);

    Fixture g;
    g.services.mount(Content{[&] {
        Select(SelectProps{}
                   .mode(SelectMode::Multiple)
                   .options({{1, u8"One"}, {2, u8"Two"}})
                   .ref(ref)
                   .maxCount(std::size_t{1}),
               SelectPrefix{[] { Button(ButtonProps{}, ButtonContent{[] { Text(u8"Prefix"); }}); }});
    }});
    sync(g);
    const auto mid = g.services.select().mounted()[0];
    check(ref.focus(), "multiple refocus");
    key(g, Key::down);
    key(g, Key::enter);
    check(selection(g, mid).values == SelectValues{SelectValue{1}} && selection(g, mid).visible,
          "multiple keyboard did not keep popup open");
    const auto tag_bounds = bounds(g, selection(g, mid).tags[0]);
    const auto prefix_bounds = bounds(g, g.buttons.mounted_buttons()[0].component);
    check(tag_bounds.x >= prefix_bounds.x + prefix_bounds.width &&
              g.inputs.layout_snapshot(selection(g, mid).selector).viewport.width > 0,
          "multiple tags overlapped interactive prefix");
    key(g, Key::down);
    key(g, Key::enter);
    check(selection(g, mid).values.size() == 1, "maxCount allowed extra selection");
    edit(g, u8"free,never,");
    check(selection(g, mid).values == SelectValues{SelectValue{1}}, "multiple admitted a free tag");
    clean(g);
    Fixture h;
    h.services.mount(Content{[&] {
        Select(SelectProps{}.mode(SelectMode::Tags).ref(ref).tokenizer([](StringView text) {
            std::vector<String> values;
            const auto comma = text.bytes().find(',');
            if (comma == std::string_view::npos) {
                values.push_back(String::from_utf8(text.bytes()).value());
            } else {
                values.push_back(String::from_utf8(text.bytes().substr(0, comma)).value());
                values.push_back(String::from_utf8(text.bytes().substr(comma + 1)).value());
            }
            return values;
        }));
    }});
    sync(h);
    const auto tokenizer = h.services.select().mounted()[0];
    check(ref.focus(), "tokenizer focus");
    edit(h, u8"a");
    check(selection(h, tokenizer).values.empty() && selection(h, tokenizer).query == String{u8"a"},
          "custom tokenizer consumed partial ordinary typing");
    edit(h, u8"a,b");
    check(selection(h, tokenizer).values == SelectValues{SelectValue{u8"a"}, SelectValue{u8"b"}},
          "custom tokenizer did not submit parsed pasted tokens");
    edit(h, u8"c");
    key(h, Key::enter);
    check(selection(h, tokenizer).values.back() == SelectValue{u8"c"}, "custom tokenizer Enter");
    clean(h);
}

void windowed_groups() {
    std::vector<SelectOption> options;
    for (int i = 0; i < 10000; ++i) {
        options.push_back({i, String::from_utf8(std::to_string(i)).value()});
    }
    Fixture f;
    SelectRef ref;
    f.services.mount(Content{[&] { Select(SelectProps{}.options(options).listHeight(64.0F).ref(ref)); }});
    sync(f);
    const auto id = f.services.select().mounted()[0];
    check(ref.focus(), "Select window focus");
    key(f, Key::down);
    key(f, Key::end);
    auto s = selection(f, id);
    check(s.active == 9999 && s.rows.size() <= 6 && s.scroll > 300000, "Select window/End bounds");
    const auto count = f.services.components().component_count();
    for (int i = 0; i < 30; ++i) {
        const auto before = selection(f, id);
        check(f.inputs.dispatch(ScrollInputEvent{0, 1, before.popup.x + 8, before.popup.y + 8}),
              "Select virtual wheel");
        sync(f);
        check(selection(f, id).rows.size() <= 6 && f.services.components().component_count() <= count + 32,
              "Select window leaked visited ordinary rows");
    }
    key(f, Key::home);
    check(selection(f, id).scroll == 0, "Select Home reveal");
    clean(f);
    Fixture footer;
    footer.services.mount(Content{[&] {
        Select(SelectProps{}.options(options).listHeight(64.0F).ref(ref).popupRender(
            SelectPopupRenderer{[](std::function<void()> origin) {
                Flex(FlexProps{}.vertical(true), FlexContent{[origin] {
                         origin();
                         Input(InputProps{}.placeholder(u8"Retained footer"));
                     }});
            }}));
    }});
    sync(footer);
    const auto footer_id = footer.services.select().mounted()[0];
    check(ref.focus(), "footer window focus");
    key(footer, Key::down);
    key(footer, Key::end);
    const auto at_end = selection(footer, footer_id);
    const auto last = bounds(footer, at_end.rows.back());
    check(footer.inputs.dispatch(ScrollInputEvent{0, -1, last.x + last.width / 2, last.y + last.height / 2}),
          "custom footer bottom wheel was not consumed");
    sync(footer);
    check(selection(footer, footer_id).scroll == at_end.scroll && selection(footer, footer_id).rows.size() <= 6,
          "custom footer height made bottom wheel jump backwards");
    clean(footer);
    Fixture g;
    g.services.mount(Content{[&] {
        Select(SelectProps{}
                   .groups(std::vector<SelectOptionGroup>{{u8"g", u8"Group", {{1, u8"Alpha"}, {2, u8"Beta"}}},
                                                          {u8"h", u8"Other", {{3, u8"Gamma"}}}})
                   .showSearch()
                   .ref(ref)
                   .filterSort(SelectSort{
                       [](const auto& a, const auto& b, StringView) { return a.label.bytes() > b.label.bytes(); }}));
    }});
    sync(g);
    const auto gid = g.services.select().mounted()[0];
    check(ref.focus(), "group refocus");
    key(g, Key::down);
    key(g, Key::home);
    check(selection(g, gid).active == 2, "group sort/header activity");
    edit(g, u8"Alpha");
    check(selection(g, gid).total_rows == 2 && selection(g, gid).active == 1, "Select group filter/header");
    clean(g);
}

void retained_rich_and_affixes() {
    Fixture f;
    SelectRef ref;
    InputRef child;
    int runs = 0;
    int affix_clicks = 0;
    SelectRichContent rich{SelectOptionContent{[&] {
        ++runs;
        Input(InputProps{}.ref(child).defaultValue(u8"draft"));
    }}};
    Signal<std::vector<SelectOption>> options{std::vector<SelectOption>{{1, u8"One", false, rich}, {2, u8"Two"}}};
    Signal<SelectOptionRenderer> renderer{SelectOptionRenderer{}};
    Signal<String> search{String{}};
    f.services.mount(Content{[&] {
        Select(SelectProps{}
                   .options(options)
                   .showSearch()
                   .searchValue(search)
                   .onSearch([search](String value) { search.set(std::move(value)); })
                   .ref(ref)
                   .optionRender(renderer)
                   .layout(LayoutStyle{}.width(dp(320))),
               SelectPrefix{[&] {
                   Button(ButtonProps{}.onClick([&] { ++affix_clicks; }), ButtonContent{[] { Text(u8"Action"); }});
               }});
    }});
    sync(f);
    const auto id = f.services.select().mounted()[0];
    check(ref.focus(), "rich focus");
    key(f, Key::down);
    const auto row = selection(f, id).rows[0];
    check(child.focus(), "rich child focus");
    edit(f, u8"retained draft");
    options.set({{2, u8"Two"}, {1, u8"Renamed", false, rich}});
    sync(f);
    check(selection(f, id).rows[1] == row && runs == 1 &&
              f.inputs.display_snapshot(f.inputs.mounted_inputs()[1].component).text == "retained draft",
          "Select key reorder rebuilt rich editor");
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, stamp})), "rich composition");
    search.set(u8"Two");
    sync(f);
    check(!f.inputs.editors().require(stamp.owner).composition().active, "filtered rich row retained IME");
    const auto input = selection(f, id).selector;
    renderer.set(SelectOptionRenderer{[](SelectRenderInfo info) { Text(TextProps{}.content(info.label)); }});
    sync(f);
    check(!child.bound() && selection(f, id).selector == input, "option renderer replaced unrelated editor");
    click(f, f.buttons.mounted_buttons()[0].component);
    check(affix_clicks == 1, "interactive Select affix was rejected or inert");
    clean(f);
    Fixture g;
    Signal<SelectLabeledSelection> labeled{SelectLabeledSelection{{1, u8"External label"}}};
    Signal<SelectLabelRenderer> label_renderer{SelectLabelRenderer{}};
    Prop<String> retained_label{String{}};
    InputRef label_input;
    int label_runs = 0;
    auto render = [&](SelectRenderInfo info) {
        ++label_runs;
        retained_label = info.label;
        Input(InputProps{}.ref(label_input).defaultValue(u8"label draft"));
    };
    label_renderer.set(SelectLabelRenderer{render});
    std::vector<SelectLabeledValues> labeled_requests;
    g.services.mount(Content{[&] {
        Select(
            SelectProps{}
                .labeledValue(labeled)
                .labelRender(label_renderer)
                .ref(ref)
                .options({{1, u8"One"}, {2, u8"Two"}})
                .onLabeledChange([&](SelectLabeledValues request) { labeled_requests.push_back(std::move(request)); }));
    }});
    sync(g);
    const auto gid = g.services.select().mounted()[0];
    check(label_runs == 1 && detail::read_prop(retained_label) == String{u8"External label"},
          "labeled display authority");
    labeled.set(SelectLabeledSelection{{1, u8"Renamed external"}});
    sync(g);
    check(label_runs == 1 && detail::read_prop(retained_label) == String{u8"Renamed external"} && label_input.bound(),
          "labeled text update replaced retained label owner");
    check(ref.focus(), "labeled focus");
    key(g, Key::down);
    key(g, Key::end);
    key(g, Key::enter);
    check(selection(g, gid).value == SelectSelection{1} && labeled_requests.size() == 1 &&
              labeled_requests[0] == SelectLabeledValues{{2, u8"Two"}},
          "labeled controlled refusal or typed event");
    label_renderer.set(SelectLabelRenderer{[](SelectRenderInfo info) { Text(TextProps{}.content(info.label)); }});
    sync(g);
    check(!label_input.bound() && selection(g, gid).selector.valid(), "label renderer retained obsolete editor");
    clean(g);
    Fixture window;
    InputRef virtual_input;
    int virtual_runs = 0;
    SelectRichContent virtual_label{SelectOptionContent{[&] {
        ++virtual_runs;
        Input(InputProps{}.ref(virtual_input).defaultValue(u8"window draft"));
    }}};
    std::vector<SelectOption> virtual_options{{0, u8"Rich", false, virtual_label}};
    for (int i = 1; i < 100; ++i) {
        virtual_options.push_back({i, u8"Ordinary"});
    }
    window.services.mount(Content{[&] { Select(SelectProps{}.options(virtual_options).ref(ref).listHeight(64.0F)); }});
    sync(window);
    const auto wid = window.services.select().mounted()[0];
    check(ref.focus(), "retained virtual focus");
    key(window, Key::down);
    const auto rich_row = selection(window, wid).rows[0];
    check(virtual_input.focus(), "virtual rich Input focus");
    edit(window, u8"saved virtual draft");
    const auto virtual_stamp = window.inputs.sessions().active();
    check(bool(window.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, virtual_stamp})), "virtual preedit");
    const auto popup = selection(window, wid).popup;
    check(window.inputs.dispatch(ScrollInputEvent{0, -20, popup.x + 4, popup.y + 4}), "virtual rich scroll");
    sync(window);
    check(!window.inputs.editors().require(virtual_stamp.owner).composition().active &&
              !window.services.components().branch_active(rich_row) && selection(window, wid).visible,
          "offscreen retained Select row kept IME or activity");
    check(ref.focus(), "virtual return focus");
    key(window, Key::home);
    check(selection(window, wid).rows[0] == rich_row && virtual_runs == 1 && virtual_input.bound() &&
              window.inputs.display_snapshot(window.inputs.mounted_inputs()[1].component).text == "saved virtual draft",
          "Select virtual return replaced keyed rich draft");
    clean(window);
}

void async_lifetime() {
    Fixture f;
    SelectRef ref;
    std::vector<SelectSource::Complete> requests;
    std::vector<std::function<void()>> posted;
    int cancelled = 0;
    SelectSource source{[&](AsyncOptionsRequest, auto complete) {
                            requests.push_back(std::move(complete));
                            return [&] {
                                ++cancelled;
                            };
                        },
                        [&](auto task) { posted.push_back(std::move(task)); }};
    f.services.mount(Content{[&] {
        Select(SelectProps{}.source(source).showSearch().ref(ref).popupRender(
            SelectPopupRenderer{[&](std::function<void()> origin) {
                Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                         origin();
                         Button(ButtonProps{}.onClick([&] {
                             requests.back()({{{7, u8"button"}}, {}});
                             auto tasks = std::move(posted);
                             posted.clear();
                             for (auto& task : tasks) {
                                 task();
                             }
                         }),
                                ButtonContent{[] { Text(u8"Deliver synthetic options"); }});
                     }});
            }}));
    }});
    sync(f);
    const auto id = f.services.select().mounted()[0];
    check(ref.focus(), "async Select focus");
    key(f, Key::down);
    edit(f, u8"old");
    edit(f, u8"new");
    requests[1]({{{2, u8"new"}}, {}});
    requests[0]({{{1, u8"old"}}, {}});
    check(posted.size() == 1 && cancelled == 1, "Select stale completion was not cancelled");
    posted[0]();
    posted.clear();
    sync(f);
    check(selection(f, id).total_rows == 1 && !selection(f, id).pending, "Select async result missing");
    edit(f, u8"button");
    click(f, f.buttons.mounted_buttons()[0].component);
    check(!selection(f, id).pending && selection(f, id).visible && cancelled == 2,
          "Select popup delivery cancelled current query");
    click(f, selection(f, id).rows[0]);
    check(selection(f, id).value == SelectSelection{7} &&
              f.services.focus().state().focused == f.inputs.mounted_inputs()[0].interaction,
          "Select popup choice did not restore editor focus");
    edit(f, u8"late");
    requests.back()({{{3, u8"late"}}, {}});
    clean(f);
    posted[0]();
    check(!ref.bound() && static_cast<std::size_t>(cancelled) == requests.size(),
          "Select async teardown retained transport or editor");
}

void lifetime_motion() {
    for (bool throwing : {false, true}) {
        Fixture f;
        SelectRef ref;
        runtime::ComponentId id;
        f.services.mount(Content{[&] {
            Select(SelectProps{}.options({{1, u8"One"}}).ref(ref).onChange([&](SelectSelection) {
                if (throwing) {
                    throw std::runtime_error("intent failure");
                }
                f.services.destroy(id);
            }));
        }});
        sync(f);
        id = f.services.select().mounted()[0];
        check(ref.focus(), "retiring Select focus");
        key(f, Key::down);
        bool caught = false;
        try {
            key(f, Key::enter);
        } catch (const std::runtime_error&) {
            caught = true;
        }
        check(caught == throwing && !f.services.components().contains(id) && !ref.bound(),
              "Select callback did not retire exact owner");
        clean(f);
    }
    {
        Fixture active;
        SelectRef ref;
        active.services.mount(Content{[&] {
            Select(SelectProps{}.options({{1, u8"One"}}).ref(ref).onActive([](SelectValue) {
                throw std::runtime_error("active failure");
            }));
        }});
        sync(active);
        const auto id = active.services.select().mounted()[0];
        check(ref.focus(), "active callback focus");
        bool caught = false;
        try {
            key(active, Key::down);
        } catch (const std::runtime_error&) {
            caught = true;
        }
        check(caught && !active.services.components().contains(id) && !ref.bound(),
              "active callback failure retained owner");
        clean(active);
    }
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    SelectRef ref;
    Signal<bool> read_only{false};
    Signal<LogicalLength> width{dp(240)};
    f.services.mount(Content{[&] {
        Select(SelectProps{}
                   .options({{1, u8"One"}})
                   .showSearch()
                   .ref(ref)
                   .motion()
                   .readOnly(read_only)
                   .layout(LayoutStyle{}.width(width)));
    }});
    sync(f);
    const auto id = f.services.select().mounted()[0];
    check(ref.focus(), "motion Select focus");
    key(f, Key::down);
    check(static_cast<detail::WindowComponentParticipant&>(f.services.select()).next_auxiliary_deadline().has_value(),
          "Select motion missing");
    (void)f.services.tick_animations(animation::AnimationTime::microseconds(1000000));
    sync(f);
    check(!static_cast<detail::WindowComponentParticipant&>(f.services.select()).next_auxiliary_deadline(),
          "Select motion did not finish");
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, stamp})), "Select preedit");
    read_only.set(true);
    sync(f);
    check(!selection(f, id).visible && !f.inputs.editors().require(stamp.owner).composition().active,
          "Select readOnly retained preedit/popup");
    read_only.set(false);
    width.set(dp(0));
    sync(f);
    check(!ref.focus() && !selection(f, id).visible, "zero Select retained focus/popup");
    clean(f);
    Fixture g;
    Signal<SelectMode> mode{SelectMode::Multiple};
    g.services.mount(Content{[&] {
        Select(SelectProps{}.mode(mode).defaultValues({SelectValue{1}}).options({{1, u8"One"}, {2, u8"Two"}}));
    }});
    sync(g);
    const auto changed = g.services.select().mounted()[0];
    mode.set(SelectMode::Single);
    sync(g);
    check(selection(g, changed).value == SelectSelection{1}, "mode change lost first selected value");
    mode.set(SelectMode::Tags);
    sync(g);
    check(selection(g, changed).values == SelectValues{SelectValue{1}},
          "mode change did not restore typed multi values");
    clean(g);
    Fixture responsive;
    Signal<LogicalLength> responsive_width{dp(100)};
    responsive.services.mount(Content{[&] {
        Select(SelectProps{}
                   .mode(SelectMode::Multiple)
                   .defaultValues({SelectValue{1}, SelectValue{2}, SelectValue{3}})
                   .options({{1, u8"A long first tag"}, {2, u8"更长的第二个标签"}, {3, u8"A third tag"}})
                   .responsiveMaxTagCount()
                   .layout(LayoutStyle{}.width(responsive_width)));
    }});
    sync(responsive);
    const auto rid = responsive.services.select().mounted()[0];
    auto visible_tags = [&] {
        std::size_t count = 0;
        for (auto tag : selection(responsive, rid).tags) {
            count += responsive.services.components().branch_active(tag);
        }
        return count;
    };
    check(visible_tags() == 1 &&
              responsive.inputs.layout_snapshot(selection(responsive, rid).selector).viewport.width >= 10,
          "responsive overflow did not preserve editing space at narrow width");
    responsive_width.set(dp(500));
    sync(responsive);
    check(visible_tags() > 1, "responsive tags did not expand on container resize");
    sync(responsive);
    const auto selector = selection(responsive, rid).selector;
    const auto measured = responsive.nodes.require(responsive.services.components().root(selector)).measure_count;
    sync(responsive);
    sync(responsive);
    check(responsive.nodes.require(responsive.services.components().root(selector)).measure_count == measured,
          "responsive tags scheduled idle layout work");
    clean(responsive);
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string test = argc > 1 ? argv[1] : "";
        if (test == "multiple") {
            multiple_tags();
        } else if (test == "window") {
            windowed_groups();
        } else if (test == "retained") {
            retained_rich_and_affixes();
        } else if (test == "async") {
            async_lifetime();
        } else if (test == "lifetime") {
            lifetime_motion();
        } else {
            throw std::runtime_error("unknown test");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
