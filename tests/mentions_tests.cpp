#include "support/mentions_fixture.hpp"
#include "component/mentions_model.hpp"
#include "component/option_list_support.hpp"
#include "component/popover_component.hpp"
#include "runtime/component_host.hpp"
#include "theme/theme_runtime.hpp"
#include "theme/input_tokens.hpp"
#include <ryn/flex.hpp>
#include <ryn/popover.hpp>
#include <thread>
#include <mutex>

namespace {
using namespace mentions_test;

runtime::ComponentId owner(Fixture& f) {
    return f.services.mentions().mounted().front();
}

auto& editor(Fixture& f) {
    return f.inputs.editors().require(f.inputs.mounted_inputs().front().editor);
}

void commit(Fixture& f, String value) {
    check(bool(f.inputs.dispatch(TextCommitted{std::move(value), f.inputs.sessions().active()})),
          "native committed edit");
    sync(f);
}

void query(Fixture& f) {
    edit(f, u8"@");
}

void parsing() {
    const MentionsConfig config{{u8"@", u8"@@", u8"提及", u8"🙂"}, u8" "};
    auto text = String{u8"中🙂👩‍👩‍👧‍👦\n@@é"};
    const auto q = detail::mention_query(text.view(), text.size_bytes(), text.size_bytes(), config);
    check(q && q->prefix == String{u8"@@"} && q->query == String{u8"é"} &&
              q->begin == String{u8"中🙂👩‍👩‍👧‍👦\n"}.size_bytes(),
          "overlapping prefix/grapheme byte range");
    check(!detail::mention_query(text.view(), text.size_bytes() - 1, text.size_bytes() - 1, config),
          "midcodepoint caret admitted");
    check(!detail::mention_query(text.view(), 0, text.size_bytes(), config), "selection admitted");
    check(getMentions(u8"@light #竹 cat @", {{u8"@", u8"#"}, u8" "}) ==
              std::vector<MentionEntity>{{u8"@", u8"light"}, {u8"#", u8"竹"}},
          "pure locked parsing");
    check(getMentions(u8"@@中文|🙂אב", {{u8"@", u8"@@", u8"🙂"}, u8"|"}) ==
              std::vector<MentionEntity>{{u8"@@", u8"中文"}, {u8"🙂", u8"אב"}},
          "pure Unicode prefixes/separator");
    check(getMentions(u8"@x", {{u8"@"}, u8""}).empty(), "empty split must remain finite and grapheme safe");
    detail::MentionsModel model{{{u8"a", u8"École", u8"School"}, {u8"b", u8"else", u8"Other", true}}};
    model.search(u8"éC");
    check(model.filtered().size() == 1 && model.active() == 0, "Unicode normalization/case fold filtering");
    model.configure(MentionsFilter{false}, true);
    model.move(1);
    check(model.active() == 0, "disabled skip/wrap");
    bool rejected{};
    try {
        (void)detail::mention_query(text.view(), 0, 0, {{u8""}, u8" "});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "empty prefix accepted");
}

void editing() {
    Fixture f;
    MentionsRef ref;
    std::vector<String> events;
    f.services.mount(Content{[&] {
        Mentions(MentionsProps{}
                     .ref(ref)
                     .options({{u8"a", u8"阿明", u8"阿明"}})
                     .prefix({u8"@", u8"提及"})
                     .rows(std::size_t{3})
                     .onChange([&](String) { events.push_back(u8"change"); })
                     .onSearch([&](String text, String prefix) {
                         check(text == String{u8"阿"} || text.empty(), "query event includes document");
                         check(prefix == String{u8"提及"}, "prefix event");
                         events.push_back(u8"search");
                     })
                     .onSelect([&](MentionsOption option, String prefix) {
                         check(option.value == String{u8"阿明"} && prefix == String{u8"提及"}, "select payload");
                         events.push_back(u8"select");
                     })
                     .layout(LayoutStyle{}.width(dp(260))));
    }});
    sync(f);
    check(ref.focus(), "focus");
    edit(f, u8"第一行\n你好 提及阿 保留👩‍👩‍👧‍👦");
    const auto caret = String{u8"第一行\n你好 提及阿"}.size_bytes();
    check(ref.select(caret, caret), "grapheme selection");
    sync(f);
    auto s = snapshot(f, owner(f));
    check(s.visible && s.query == String{u8"阿"} && s.prefix == String{u8"提及"}, "actual caret query");
    events.clear();
    key(f, Key::enter);
    check(snapshot(f, owner(f)).value == String{u8"第一行\n你好 提及阿明 保留👩‍👩‍👧‍👦"},
          "choice replaced unrelated text");
    const auto expected = String{u8"第一行\n你好 提及阿明 "}.size_bytes();
    check(editor(f).selection() == TextSelection{expected, expected}, "inserted selection not at split end");
    check(events == std::vector<String>{u8"change", u8"select"} && !snapshot(f, owner(f)).visible,
          "selection ordering/close");
    key(f, Key::z, KeyModifier::control);
    check(snapshot(f, owner(f)).value == String{u8"第一行\n你好 提及阿 保留👩‍👩‍👧‍👦"},
          "selection did not participate in native undo");
    clean(f);
    check(!ref.bound() && !ref.select(0, 0), "stale reference");
    Fixture bidi;
    MentionsRef bidi_ref;
    bidi.services.mount(Content{[&] {
        Mentions(MentionsProps{}
                     .ref(bidi_ref)
                     .prefix(u8"💬")
                     .options({{u8"echo", u8"écho", u8"Unicode suggestion"}})
                     .direction(TextDirection::RightToLeft)
                     .rows(std::size_t{3})
                     .layout(LayoutStyle{}.width(dp(180))));
    }});
    sync(bidi);
    check(bidi_ref.focus(), "bidi focus");
    const String document{u8"אבג👩‍👩‍👧‍👦\n💬é ז"};
    edit(bidi, document);
    const auto position = String{u8"אבג👩‍👩‍👧‍👦\n💬é"}.size_bytes();
    check(bidi_ref.select(position, position) && !bidi_ref.select(position - 1, position - 1),
          "bidi grapheme ref range");
    sync(bidi);
    const auto model = snapshot(bidi, owner(bidi));
    const auto& map = bidi.inputs.caret_map(model.input);
    check(model.visible && model.query == String{u8"é"} && map.at(position, map.revision()).has_value(),
          "bidi native caret map/query");
    key(bidi, Key::enter);
    const auto selected = String{u8"אבג👩‍👩‍👧‍👦\n💬écho "}.size_bytes();
    check(snapshot(bidi, owner(bidi)).value == String{u8"אבג👩‍👩‍👧‍👦\n💬écho ז"} &&
              editor(bidi).selection() == TextSelection{selected, selected},
          "bidi exact replacement/selection");
    clean(bidi);
}

void caret_navigation() {
    Fixture f;
    MentionsRef ref;
    int changes{};
    int searches{};
    f.services.mount(Content{[&] {
        Mentions(MentionsProps{}
                     .ref(ref)
                     .options(options())
                     .defaultValue(u8"hello @al\n@ga tail")
                     .rows(std::size_t{3})
                     .onChange([&](String) { ++changes; })
                     .onSearch([&](String, String) { ++searches; }));
    }});
    sync(f);
    check(ref.focus(), "focus navigation");
    auto id = owner(f);
    const auto at = String{u8"hello @al"}.size_bytes();
    check(ref.select(at, at), "select first line");
    sync(f);
    check(snapshot(f, id).query == String{u8"al"} && snapshot(f, id).active == 0, "caret first query");
    key(f, Key::left);
    check(snapshot(f, id).query == String{u8"a"}, "left caret not queried");
    key(f, Key::right);
    check(snapshot(f, id).query == String{u8"al"}, "right caret not queried");
    key(f, Key::home);
    check(!snapshot(f, id).visible, "home kept stale query");
    key(f, Key::end);
    check(snapshot(f, id).visible, "end lost prefix");
    key(f, Key::left, KeyModifier::shift);
    check(!snapshot(f, id).visible, "selection kept suggestion");
    check(ref.select(at, at), "restore selection");
    sync(f);
    key(f, Key::escape);
    sync(f);
    sync(f);
    check(!snapshot(f, id).visible, "Escape reopened without edit");
    key(f, Key::backspace);
    check(snapshot(f, id).query == String{u8"a"}, "deletion query");
    key(f, Key::backspace);
    check(snapshot(f, id).query.empty() && snapshot(f, id).visible, "empty query");
    key(f, Key::backspace);
    check(!snapshot(f, id).visible && changes == 3 && searches >= 4, "prefix deletion");
    check(!ref.select(1, 10000), "invalid ref range");
    clean(f);
}

void controlled() {
    Fixture f;
    MentionsRef ref;
    Signal<String> value{String{u8"@al"}};
    bool accept{};
    bool reenter{};
    std::vector<String> events;
    f.services.mount(Content{[&] {
        Mentions(MentionsProps{}
                     .ref(ref)
                     .value(value)
                     .options(options())
                     .onChange([&](String next) {
                         events.push_back(u8"change");
                         if (accept) {
                             value.set(next);
                         }
                         if (reenter) {
                             value.set(u8"application replacement");
                         }
                     })
                     .onSearch([&](String, String) { events.push_back(u8"search"); })
                     .onSelect([&](MentionsOption, String) { events.push_back(u8"select"); }));
    }});
    sync(f);
    check(ref.focus(), "controlled focus");
    check(ref.select(3, 3), "controlled caret");
    sync(f);
    events.clear();
    key(f, Key::enter);
    check(snapshot(f, owner(f)).value == String{u8"@al"} && editor(f).value() == "@al" &&
              events == std::vector<String>{u8"change", u8"select"},
          "controlled refusal overwritten");
    accept = true;
    check(ref.select(2, 2), "caret");
    sync(f);
    check(ref.select(3, 3), "caret");
    sync(f);
    key(f, Key::enter);
    check(snapshot(f, owner(f)).value == String{u8"@alpha "}, "controlled echo missing");
    reenter = true;
    edit(f, u8"@ga");
    check(snapshot(f, owner(f)).value == String{u8"application replacement"} && !snapshot(f, owner(f)).visible,
          "reentrant authority overwritten");
    clean(f);
}

void max_length_and_clipboard() {
    Fixture f;
    MentionsRef ref;
    int changes{};
    f.services.mount(Content{[&] {
        Mentions(MentionsProps{}
                     .ref(ref)
                     .maxLength(std::size_t{5})
                     .options({{u8"a", u8"👩‍👩‍👧‍👦long", u8"Family"}})
                     .onChange([&](String) { ++changes; }));
    }});
    sync(f);
    check(ref.focus(), "limit focus");
    query(f);
    key(f, Key::enter);
    check(editor(f).boundaries().scalar_count() <= 5 && editor(f).boundaries().is_boundary(editor(f).selection().caret),
          "selection exceeded maxLength or cut grapheme");
    check(snapshot(f, owner(f)).value == String::from_utf8(editor(f).value()).value(), "limited value/editor diverged");
    f.platform.clipboard = String{u8"中\n@ga"};
    key(f, Key::a, KeyModifier::control);
    key(f, Key::v, KeyModifier::control);
    check(editor(f).value() == "中\n@ga" && snapshot(f, owner(f)).query == String{u8"ga"},
          "native multiline paste path");
    key(f, Key::a, KeyModifier::control);
    key(f, Key::c, KeyModifier::control);
    check(f.platform.clipboard == String{u8"中\n@ga"}, "native copy");
    key(f, Key::x, KeyModifier::control);
    check(editor(f).value().empty() && !snapshot(f, owner(f)).visible && changes >= 3, "native cut");
    clean(f);
}

void composition() {
    Fixture f;
    MentionsRef ref;
    int selects{};
    f.services.mount(Content{[&] {
        Mentions(MentionsProps{}.ref(ref).options(options()).onSelect([&](MentionsOption, String) { ++selects; }));
    }});
    sync(f);
    check(ref.focus(), "IME focus");
    query(f);
    auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"阿"}, {}, stamp})), "Core preedit");
    sync(f);
    const auto revision = editor(f).revision();
    key(f, Key::enter);
    check(editor(f).composition().active && editor(f).revision() == revision && selects == 0 &&
              !snapshot(f, owner(f)).visible,
          "composition Enter chose suggestion");
    commit(f, u8"ga");
    check(!editor(f).composition().active && snapshot(f, owner(f)).query == String{u8"ga"}, "composition commit query");
    key(f, Key::enter);
    check(snapshot(f, owner(f)).value == String{u8"@gamma "} && selects == 1, "postcomposition selection");
    clean(f);
}

void geometry() {
    Fixture f;
    MentionsRef ref;
    Signal<LogicalLength> width{dp(140)};
    Signal<MentionsPlacement> placement{MentionsPlacement::Bottom};
    f.services.mount(Content{[&] {
        Mentions(MentionsProps{}
                     .ref(ref)
                     .options(options())
                     .rows(std::size_t{3})
                     .placement(placement)
                     .popupWidth(160.0F)
                     .layout(LayoutStyle{}.width(width)));
    }});
    sync(f);
    check(ref.focus(), "geometry focus");
    edit(f, u8"one two three four five six seven eight nine\n@ga");
    auto id = owner(f);
    auto s = snapshot(f, id);
    auto g = f.inputs.layout_snapshot(s.input);
    check(f.inputs.caret_map(s.input).line_count() >= 4 && g.vertical_scroll > 0 && s.visible,
          "multiline/wrapped retained caret");
    check(std::abs(s.bounds.y - (g.caret.y + g.caret.height +
                                 f.services.components().theme_scope(id)->snapshot().mentions().metrics.gap)) < 1,
          "popup not at scrolled caret");
    const auto old_scroll = g.vertical_scroll;
    const auto old_height = g.text_height;
    width.set(dp(260));
    sync(f);
    s = snapshot(f, id);
    g = f.inputs.layout_snapshot(s.input);
    check(s.visible && std::abs(s.bounds.x - g.caret.x) < 1 &&
              (g.vertical_scroll < old_scroll || g.text_height < old_height),
          "resize did not follow caret");
    placement.set(MentionsPlacement::Top);
    sync(f);
    s = snapshot(f, id);
    g = f.inputs.layout_snapshot(s.input);
    check(s.bounds.y + s.bounds.height <= g.caret.y + 1 || s.bounds.y >= g.caret.y + g.caret.height,
          "top placement overflow");
    const auto before_translation = g;
    runtime::NodePropertyWriter writer(f.nodes, f.dirty);
    const auto translate = [&](const auto& self, runtime::NodeId node) -> void {
        const auto& item = f.nodes.require(node);
        if (item.clip_boundary) {
            return;
        }
        const auto children = item.children;
        (void)writer.set_translation(node, {20, 30});
        for (const auto child : children) {
            self(self, child);
        }
    };
    translate(translate, f.services.components().root(id));
    sync(f);
    s = snapshot(f, id);
    g = f.inputs.layout_snapshot(s.input);
    check(s.visible && std::abs(g.caret.x - before_translation.caret.x - 20) < 1 &&
              std::abs(g.caret.y - before_translation.caret.y - 30) < 1 && std::abs(s.bounds.x - g.caret.x) < 1,
          "flat ancestor translation did not follow actual caret");
    width.set(dp(140));
    sync(f);
    g = f.inputs.layout_snapshot(s.input);
    check(f.inputs.dispatch(
              ScrollInputEvent{0, -20, g.viewport.x + g.viewport.width - 2, g.viewport.y + g.viewport.height - 2}),
          "TextArea scroll path");
    sync(f);
    s = snapshot(f, id);
    g = f.inputs.layout_snapshot(s.input);
    check(s.visible && g.vertical_scroll > 0 && std::abs(s.bounds.x - g.caret.x) < 1,
          "scrolled caret popup did not follow the editor");
    clean(f);
}

void async() {
    Fixture f;
    MentionsRef ref;
    Signal<bool> disabled{false};
    std::vector<MentionsSource::Complete> requests;
    std::vector<std::function<void()>> posted;
    std::vector<std::shared_ptr<const std::atomic_bool>> cancelled;
    int cancels{};
    int errors{};
    std::mutex mutex;
    MentionsSource source{[&](AsyncOptionsRequest r, auto done) {
                              requests.push_back(std::move(done));
                              cancelled.push_back(r.cancelled);
                              return [&] {
                                  ++cancels;
                              };
                          },
                          [&](auto task) {
                              std::scoped_lock lock(mutex);
                              posted.push_back(std::move(task));
                          }};
    f.services.mount(Content{[&] {
        Mentions(MentionsProps{}.ref(ref).source(source).disabled(disabled).onLoadError([&](String) { ++errors; }));
    }});
    sync(f);
    check(ref.focus(), "async focus");
    edit(f, u8"@old");
    edit(f, u8"@new");
    check(requests.size() == 2 && cancelled[0]->load() && cancels == 1 && snapshot(f, owner(f)).pending,
          "query cancellation");
    std::thread worker([&] {
        requests[0]({{{u8"old", u8"old", u8"Old"}}, {}});
        requests[1]({{{u8"new", u8"new", u8"New"}}, {}});
    });
    worker.join();
    check(posted.size() == 1 && snapshot(f, owner(f)).rows.empty(), "worker applied or stale posted");
    auto task = posted.back();
    posted.clear();
    std::thread misuse(task);
    misuse.join();
    check(snapshot(f, owner(f)).pending, "nonowner task mutated Core");
    task();
    task();
    sync(f);
    check(!snapshot(f, owner(f)).pending && snapshot(f, owner(f)).total_rows == 1, "owner completion/duplicate guard");
    edit(f, u8"@late");
    requests.back()({{{u8"late", u8"late", u8"Late"}}, {}});
    key(f, Key::escape);
    for (auto& t : posted) {
        t();
    }
    posted.clear();
    sync(f);
    check(cancelled.back()->load() && !snapshot(f, owner(f)).visible, "close accepted stale result");
    edit(f, u8"@disabled");
    disabled.set(true);
    requests.back()({{}, u8"stale"});
    check(posted.empty() && cancelled.back()->load() && errors == 0, "disabled accepted async result");
    disabled.set(false);
    check(ref.focus(), "refocus");
    edit(f, u8"@error");
    requests.back()({{}, u8"synthetic failure"});
    for (auto& t : posted) {
        t();
    }
    posted.clear();
    check(errors == 1 && !snapshot(f, owner(f)).pending, "async error callback");
    edit(f, u8"@unmount");
    requests.back()({{}, u8"queued"});
    clean(f);
    for (auto& t : posted) {
        t();
    }
    check(cancelled.back()->load() && !ref.bound(), "window destroy accepted task");
    auto session = std::make_unique<detail::AsyncOptionsSession<MentionsOption>>();
    int providers{};
    MentionsSource latest{[&](AsyncOptionsRequest, auto) {
                              ++providers;
                              return AsyncOptionsCancel{};
                          },
                          [](auto t) { t(); }};
    MentionsSource reentrant{[&](AsyncOptionsRequest, auto) {
                                 return [&] {
                                     session->start(latest, u8"new", [](auto) {});
                                 };
                             },
                             [](auto t) { t(); }};
    session->start(reentrant, u8"old", [](auto) {});
    session->start(latest, u8"superseded", [](auto) {});
    check(providers == 1, "common cancellation reentry regressed");
}

void form_reset() {
    Fixture f;
    MentionsRef ref;
    FormRef form;
    FormField<String> field{String{u8"initial"}};
    int changes{};
    std::vector<MentionsSource::Complete> requests;
    std::vector<std::shared_ptr<const std::atomic_bool>> cancelled;
    MentionsSource source{[&](AsyncOptionsRequest r, auto done) {
                              requests.push_back(done);
                              cancelled.push_back(r.cancelled);
                              return AsyncOptionsCancel{};
                          },
                          [](auto t) { t(); }};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form), FormContent{[&] {
                 FormItem(
                     FormItemProps{}.field(u8"mentions", field).control(ref), FormItemContent{[&] {
                         Mentions(
                             MentionsProps{}.ref(ref).value(field.value()).source(source).onChange([&](String value) {
                                 ++changes;
                                 field.onChange()(value);
                             }));
                     }});
             }});
    }});
    sync(f);
    check(ref.focus(), "form focus");
    edit(f, u8"@old");
    auto id = owner(f);
    const auto input = snapshot(f, id).input;
    auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, stamp})), "form preedit");
    const auto old_changes = changes;
    check(form.reset(), "reset");
    sync(f);
    check(snapshot(f, id).value == String{u8"initial"} && snapshot(f, id).query.empty() && !snapshot(f, id).visible &&
              !snapshot(f, id).pending && snapshot(f, id).input == input && cancelled.back()->load() &&
              changes == old_changes && !editor(f).composition().active,
          "Form reset did not clear transient state");
    requests.back()({{{u8"late", u8"late", u8"Late"}}, {}});
    check(snapshot(f, id).total_rows == 0, "reset accepted stale completion");
    stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"same"}, {}, stamp})), "same value preedit");
    check(form.reset(), "same value reset");
    sync(f);
    check(!editor(f).composition().active && changes == old_changes && !snapshot(f, id).visible,
          "same-value reset lost native reset hook");
    clean(f);
}

void retained_window() {
    Fixture f;
    MentionsRef ref;
    InputRef draft;
    int mounts{};
    MentionsRichLabel rich{MentionsOptionLabel{[&] {
        ++mounts;
        Input(InputProps{}.ref(draft).defaultValue(u8"draft"));
    }}};
    std::vector<MentionsOption> data{{u8"rich", u8"rich", u8"Rich", false, rich}};
    for (int i = 0; i < 10000; ++i) {
        const auto key = String::from_utf8(std::to_string(i)).value();
        data.push_back({key, key, key});
    }
    Signal<std::vector<MentionsOption>> options_signal{data};
    f.services.mount(Content{
        [&] { Mentions(MentionsProps{}.ref(ref).options(options_signal).listHeight(64.0F).filterOption(false)); }});
    sync(f);
    check(ref.focus(), "window focus");
    query(f);
    auto id = owner(f);
    auto s = snapshot(f, id);
    const auto row = s.rows[0];
    const auto input = s.input;
    check(s.rows.size() <= 6 && s.total_rows == 10001 && mounts == 1, "virtual list mounted every row");
    check(draft.focus(), "rich focus");
    edit(f, u8"saved");
    const auto rich_owner = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, rich_owner})), "rich composition");
    check(f.inputs.dispatch(ScrollInputEvent{0, -40, s.clip.x + 2, s.clip.y + 2}), "list wheel");
    sync(f);
    s = snapshot(f, id);
    check(!editor(f).composition().active && !f.inputs.editors().require(rich_owner.owner).composition().active &&
              !f.services.components().branch_active(row),
          "parked rich row kept focus/preedit");
    check(f.inputs.dispatch(ScrollInputEvent{0, 40, s.clip.x + 2, s.clip.y + 2}), "list return");
    sync(f);
    s = snapshot(f, id);
    check(s.rows[0] == row && mounts == 1 && draft.bound() &&
              f.inputs.display_snapshot(f.inputs.mounted_inputs()[1].component).text == "saved",
          "stable rich draft remounted");
    check(ref.focus(), "main focus");
    for (int i = 0; i < 50; ++i) {
        key(f, Key::down);
    }
    s = snapshot(f, id);
    check(s.active && *s.active >= 49 && s.scroll > 0 && s.rows.size() <= 6, "keyboard active autoreveal");
    std::swap(data[1], data[2]);
    options_signal.set(data);
    sync(f);
    check(snapshot(f, id).input == input && mounts == 1, "options rebuilt editor/rich draft");
    s = snapshot(f, id);
    check(f.inputs.dispatch(ScrollInputEvent{0, -100000, s.clip.x + 2, s.clip.y + 2}), "wheel end");
    sync(f);
    s = snapshot(f, id);
    const auto scroll = s.scroll;
    check(f.inputs.dispatch(ScrollInputEvent{0, -1, s.clip.x + 2, s.clip.y + 2}), "wheel boundary leaked to ancestor");
    sync(f);
    check(snapshot(f, id).scroll == scroll, "boundary overscroll");
    clean(f);
}

void focus_and_boundaries() {
    Fixture f;
    MentionsRef ref;
    InputRef footer;
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    Signal<bool> visible{true};
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 Button({}, ButtonContent{[] { Text(u8"Before"); }});
                 Mentions(MentionsProps{}
                              .ref(ref)
                              .options(options())
                              .disabled(disabled)
                              .readOnly(read_only)
                              .visible(visible)
                              .popupRender(MentionsPopupRenderer{[&](auto origin) {
                                  Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                           origin();
                                           Input(InputProps{}.ref(footer).defaultValue(u8"popup draft"));
                                       }});
                              }}));
                 Button({}, ButtonContent{[] { Text(u8"After"); }});
             }});
    }});
    sync(f);
    check(ref.focus(), "focus main");
    query(f);
    auto id = owner(f);
    check(footer.focus(), "popup focus");
    sync(f);
    check(snapshot(f, id).visible, "child focus closed parent");
    key(f, Key::escape);
    check(!snapshot(f, id).visible && f.services.focus().state().focused == f.inputs.mounted_inputs()[0].interaction,
          "popup Escape focus restore");
    query(f);
    key(f, Key::tab);
    check(!snapshot(f, id).visible && f.services.focus().state().focused == f.buttons.mounted_buttons()[1].interaction,
          "Tab did not leave compound");
    check(ref.focus(), "refocus");
    query(f);
    key(f, Key::tab, KeyModifier::shift);
    check(f.services.focus().state().focused == f.buttons.mounted_buttons()[0].interaction,
          "ShiftTab did not leave compound");
    check(ref.focus(), "focus disabled");
    edit(f, u8"@ga");
    disabled.set(true);
    sync(f);
    check(!snapshot(f, id).visible && !ref.focus(), "disabled interactive");
    disabled.set(false);
    check(ref.focus(), "refocus");
    key(f, Key::down);
    check(snapshot(f, id).query == String{u8"ga"} && snapshot(f, id).total_rows == 1,
          "disabled recovery reopened a stale/unfiltered query");
    key(f, Key::escape);
    query(f);
    read_only.set(true);
    sync(f);
    check(!snapshot(f, id).visible, "readonly popup");
    read_only.set(false);
    check(ref.focus(), "refocus");
    query(f);
    visible.set(false);
    sync(f);
    check(!snapshot(f, id).visible && !ref.focus(), "hidden input active");
    visible.set(true);
    sync(f);
    check(ref.focus(), "visible refocus");
    query(f);
    check(f.services.set_input_blocked(id, true), "block root");
    sync(f);
    check(!snapshot(f, id).visible, "blocked popup");
    check(f.services.set_input_blocked(id, false), "unblock");
    check(ref.focus(), "unblock focus");
    query(f);
    f.inputs.set_window_active(false);
    sync(f);
    check(!snapshot(f, id).visible && !f.inputs.sessions().active().valid(), "inactive window retained session");
    f.inputs.set_window_active(true);
    clean(f);
}

void failures() {
    for (int mode = 0; mode < 7; ++mode) {
        Fixture f;
        MentionsRef ref;
        InputRef unrelated;
        bool armed{};
        try {
            f.services.mount(Content{[&] {
                Input(InputProps{}.ref(unrelated));
                MentionsProps props;
                props.ref(ref).options(options());
                if (mode == 0) {
                    props.popupRender(MentionsPopupRenderer{[](auto) { throw std::runtime_error("popup renderer"); }});
                }
                if (mode == 1) {
                    props.editor(
                        MentionsEditor{[](MentionsEditorContext) { throw std::runtime_error("editor factory"); }});
                }
                if (mode == 2) {
                    props.onSearch([&](String, String) {
                        if (armed) {
                            throw std::runtime_error("search callback");
                        }
                    });
                }
                if (mode == 3) {
                    props.onChange([&](String) {
                        if (armed) {
                            throw std::runtime_error("change callback");
                        }
                    });
                }
                if (mode == 4) {
                    props.validateSearch(MentionsSearchValidator{[&](StringView, const MentionsConfig&) {
                        if (armed) {
                            throw std::runtime_error("validator");
                        }
                        return true;
                    }});
                }
                if (mode == 5) {
                    props.editor(MentionsEditor{[&](MentionsEditorContext) {
                        const auto id = f.services.components().root_components().back();
                        check(f.services.destroy(id), "editor factory selfdestroy");
                    }});
                }
                if (mode == 6) {
                    props.popupRender(MentionsPopupRenderer{[&](auto origin) {
                        origin();
                        const auto id = f.services.components().root_components().back();
                        check(f.services.destroy(id), "popup factory selfdestroy");
                    }});
                }
                Mentions(props);
            }});
            sync(f);
            check(ref.focus(), "throw focus");
            armed = true;
            edit(f, u8"@");
        } catch (const std::runtime_error&) {
        }
        if (mode >= 2) {
            check(unrelated.bound() && unrelated.focus(), "failure damaged unrelated editor");
        }
        clean(f);
        check(!ref.bound(), "failure retained ref");
    }
    Fixture f;
    MentionsRef ref;
    InputRef unrelated;
    bool destroy{};
    f.services.mount(Content{[&] {
        Input(InputProps{}.ref(unrelated));
        Mentions(MentionsProps{}.ref(ref).options(options()).onChange([&](String) {
            if (destroy) {
                (void)f.services.destroy(owner(f));
            }
        }));
    }});
    sync(f);
    check(ref.focus(), "destroy focus");
    destroy = true;
    edit(f, u8"@");
    check(!ref.bound() && unrelated.bound() && unrelated.focus(), "selfdestroy failed");
    clean(f);
    Fixture g;
    MentionsRef stale;
    g.services.mount(Content{[&] {
        Mentions(MentionsProps{}.ref(stale).source(MentionsSource{
            [](AsyncOptionsRequest, auto) -> AsyncOptionsCancel { throw std::runtime_error("provider throw"); },
            [](auto t) { t(); }}));
    }});
    sync(g);
    check(stale.focus(), "provider focus");
    bool caught{};
    try {
        edit(g, u8"@");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    check(caught && !stale.bound(), "provider throw retained damaged owner");
    clean(g);
}

void configuration() {
    Fixture f;
    MentionsRef ref;
    InputRef empty_ref;
    Signal<String> prefix{String{u8"@"}};
    Signal<String> split{String{u8";"}};
    Signal<bool> loading{false};
    int changes{};
    int searches{};
    int empty_mounts{};
    std::vector<String> events;
    MentionsRichLabel empty{MentionsOptionLabel{[&] {
        ++empty_mounts;
        Input(InputProps{}.ref(empty_ref).defaultValue(u8"empty draft"));
    }}};
    f.services.mount(Content{[&] {
        Mentions(MentionsProps{}
                     .ref(ref)
                     .prefix(prefix)
                     .split(split)
                     .loading(loading)
                     .options(options())
                     .notFoundContent(empty)
                     .onChange([&](String) {
                         ++changes;
                         events.push_back(u8"change");
                     })
                     .onSearch([&](String, String) {
                         ++searches;
                         events.push_back(u8"search");
                     })
                     .validateSearch(MentionsSearchValidator{[](StringView text, const MentionsConfig& config) {
                         return text.bytes().find(config.split.bytes()) == std::string_view::npos &&
                                text.bytes().find('!') == std::string_view::npos;
                     }}));
    }});
    sync(f);
    check(ref.focus(), "configuration focus");
    edit(f, u8"@zz");
    const auto id = owner(f);
    check(events == std::vector<String>{u8"change", u8"search"} && empty_ref.focus() && empty_mounts == 1,
          "search order/rich empty content");
    loading.set(true);
    sync(f);
    check(snapshot(f, id).visible && !empty_ref.focus(), "loading kept empty editor interactive");
    loading.set(false);
    sync(f);
    check(empty_ref.focus() && empty_mounts == 1, "loading remounted empty content");
    check(ref.focus(), "main restore");
    edit(f, u8"@!");
    check(!snapshot(f, id).visible, "custom validator ignored");
    prefix.set(u8"提及");
    edit(f, u8"提及ga");
    check(snapshot(f, id).prefix == String{u8"提及"} && snapshot(f, id).query == String{u8"ga"},
          "reactive String prefix");
    key(f, Key::enter);
    check(snapshot(f, id).value == String{u8"提及gamma;"} && changes == 4 && searches >= 2,
          "reactive separator insertion");
    clean(f);
}

void zero_and_private_retirement() {
    Fixture f;
    MentionsRef ref;
    InputRef unrelated;
    Signal<LogicalLength> width{dp(220)};
    f.services.mount(Content{[&] {
        Input(InputProps{}.ref(unrelated));
        Mentions(MentionsProps{}.ref(ref).options(options()).layout(LayoutStyle{}.width(width)));
    }});
    sync(f);
    check(ref.focus(), "zero focus");
    query(f);
    const auto id = owner(f);
    const auto input = snapshot(f, id).input;
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, stamp})), "zero preedit");
    width.set(dp(0));
    sync(f);
    check(!snapshot(f, id).visible && !f.inputs.editors().require(stamp.owner).composition().active && !ref.focus(),
          "zero geometry retained native focus/preedit");
    width.set(dp(220));
    sync(f);
    check(ref.bound() && ref.focus() && snapshot(f, id).input == input, "zero restore remounted editor");
    bool thread_rejected{};
    std::thread worker([&] {
        try {
            (void)ref.select(0, 0);
        } catch (const std::logic_error&) {
            thread_rejected = true;
        }
    });
    worker.join();
    check(thread_rejected, "ref accepted nonowner access");
    check(f.services.destroy(input), "private editor removal");
    sync(f);
    check(!f.services.components().contains(id) && unrelated.bound() && unrelated.focus(),
          "damaged private tree kept owner/unrelated failed");
    clean(f);
}

void nested_popups() {
    Fixture f;
    MentionsRef ref;
    ButtonRef trigger;
    Signal<bool> child_open{false};
    MentionsRichLabel rich{MentionsOptionLabel{[&] {
        Popover(PopoverProps{}
                    .open(child_open)
                    .triggers(PopoverTriggers{false, false, true})
                    .content(u8"child popup")
                    .onOpenChange([&](bool value) { child_open.set(value); }),
                PopoverTrigger{[&] { Button(ButtonProps{}.ref(trigger), ButtonContent{[] { Text(u8"Child"); }}); }});
    }}};
    f.services.mount(
        Content{[&] { Mentions(MentionsProps{}.ref(ref).options({{u8"rich", u8"rich", u8"Rich", false, rich}})); }});
    sync(f);
    check(ref.focus(), "nested focus");
    query(f);
    const auto id = owner(f);
    check(trigger.focus(), "nested trigger");
    key(f, Key::enter);
    check(child_open.get() && snapshot(f, id).visible, "child activation closed mentions");
    const auto popup = f.services.popover().mounted().front();
    const auto rect = f.services.popover().snapshot(popup).geometry.bounds;
    const auto scroll = snapshot(f, id).scroll;
    check(f.services.dispatch_scroll({0, -1, rect.x + rect.width / 2, rect.y + rect.height / 2}), "child wheel");
    sync(f);
    check(snapshot(f, id).scroll == scroll, "child wheel moved parent");
    key(f, Key::escape);
    check(!child_open.get() && snapshot(f, id).visible, "Escape closed Mentions before child");
    key(f, Key::escape);
    check(!snapshot(f, id).visible, "parent Escape");
    clean(f);
}

void theme_and_idle() {
    Fixture f;
    MentionsRef ref;
    Signal<ThemeConfig> config{ThemeConfig{}};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config),
              ThemeContent{[&] { Mentions(MentionsProps{}.ref(ref).options(options()).motion()); }});
    }});
    sync(f);
    check(ref.focus(), "theme focus");
    query(f);
    const auto id = owner(f);
    const auto input = snapshot(f, id).input;
    const auto rows = snapshot(f, id).rows;
    auto changed = std::make_unique<ThemeConfig>();
    changed->mentions.algorithm = true;
    changed->mentions.seed.color_error = Color::rgba8(30, 90, 180);
    changed->mentions.seed.line_width = dp(2);
    changed->mentions.tokens.input.input_font_size = dp(19);
    changed->mentions.tokens.input.background = Color::rgba8(12, 23, 34);
    changed->mentions.tokens.suggestions.option_height = dp(40);
    changed->mentions.tokens.suggestions.option_color = Color::rgba8(77, 88, 99);
    config.set(*changed);
    sync(f);
    check(snapshot(f, id).input == input && snapshot(f, id).rows == rows, "theme remounted content");
    const auto& child_theme = f.services.components().theme_scope(input)->snapshot();
    check(detail::InputTokenAccess::get(child_theme).sizes[1].font_size == 19 &&
              detail::InputTokenAccess::get(child_theme).colors.background == Color::rgba8(12, 23, 34),
          "Mentions editor token not rendered");
    auto expected = std::make_unique<ThemeConfig>();
    expected->input.algorithm = true;
    expected->input.seed.color_error = Color::rgba8(30, 90, 180);
    expected->input.seed.line_width = dp(2);
    const auto reference = std::make_unique<ThemeSnapshot>(resolve_theme(*expected));
    check(detail::InputTokenAccess::get(child_theme).colors.error_border ==
                  detail::InputTokenAccess::get(*reference).colors.error_border &&
              detail::InputTokenAccess::get(child_theme).border_width == 2,
          "Mentions scoped seed did not reach status border/line width");
    key(f, Key::escape);
    check(ref.blur(), "idle blur");
    sync(f);
    const auto revision = f.services.frame_request_revision();
    for (int i = 0; i < 12; ++i) {
        sync(f);
        (void)f.services.tick_animations(animation::AnimationTime::microseconds(1000000 + i * 16000));
    }
    check(f.services.frame_request_revision() == revision && !f.services.next_frame_deadline(), "idle redraw loop");
    clean(f);
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string name = argc > 1 ? argv[1] : "";
        if (name == "configuration") {
            configuration();
        } else if (name == "zero") {
            zero_and_private_retirement();
        } else if (name == "nested") {
            nested_popups();
        } else if (name == "parsing") {
            parsing();
        } else if (name == "editing") {
            editing();
        } else if (name == "caret") {
            caret_navigation();
        } else if (name == "controlled") {
            controlled();
        } else if (name == "clipboard") {
            max_length_and_clipboard();
        } else if (name == "composition") {
            composition();
        } else if (name == "geometry") {
            geometry();
        } else if (name == "async") {
            async();
        } else if (name == "form") {
            form_reset();
        } else if (name == "window") {
            retained_window();
        } else if (name == "focus") {
            focus_and_boundaries();
        } else if (name == "failures") {
            failures();
        } else if (name == "idle") {
            theme_and_idle();
        } else {
            throw std::runtime_error("unknown Mentions case");
        }
        std::cout << "Mentions " << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
