#include "component/select_component.hpp"
#include "component/tree_select_component.hpp"
#include "component/tree_component.hpp"
#include "component/mentions_component.hpp"
#include "component/cascader_component.hpp"
#include "runtime/prop_connection.hpp"
#include "support/i18n_test.hpp"
#include "support/input_fixture.hpp"

#include <ryn/config_provider.hpp>

#include <algorithm>

namespace {
using namespace ryn;
using namespace ryn::input;
using namespace ryn_test::i18n;
using Fixture = ryn_test::input_component::Fixture;
using runtime::ComponentId;

bool descendant(Fixture& f, ComponentId child, ComponentId parent) {
    for (auto current = std::optional{child}; current; current = f.services.components().parent(*current)) {
        if (*current == parent) {
            return true;
        }
    }
    return false;
}

detail::MountedTextComponent text_in(Fixture& f, ComponentId parent) {
    const auto texts = f.services.text().mounted_texts();
    const auto found =
        std::ranges::find_if(texts, [&](const auto& text) { return descendant(f, text.component, parent); });
    require(found != texts.end(), "expected a retained real Text descendant");
    return *found;
}

String text(Fixture& f, ComponentId parent) {
    return String::from_utf8(std::as_const(f.scene).text_state(text_in(f, parent).scene).content().bytes()).value();
}

runtime::Rect bounds(Fixture& f, ComponentId id) {
    const auto& node = f.nodes.require(f.services.components().root(id));
    return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
            node.bounds.height};
}

void key(Fixture& f, Key value, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({value, KeyAction::down, modifiers});
    f.services.focus().dispatch({value, KeyAction::up, modifiers});
    f.synchronize();
}

void click(Fixture& f, ComponentId id) {
    const auto rect = bounds(f, id);
    const auto x = rect.x + rect.width / 2;
    const auto y = rect.y + rect.height / 2;
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, x, y});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, x, y});
    f.synchronize();
}

void select_sources_and_windows() {
    Fixture f;
    Fixture other;
    Signal<LocaleId> language{locales::zh_CN};
    int mounts = 0;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           ++mounts;
                           Select(SelectProps{});
                           Select(SelectProps{}.notFoundContent(u8"No data"));
                           Select(SelectProps{}.notFoundContent(u8""));
                           ConfigProvider(ConfigProviderProps{}.locale(locales::en_US),
                                          ConfigProviderContent{[] { Select(SelectProps{}); }});
                       }});
    }});
    other.services.mount(Content{[] { Select(SelectProps{}); }});
    f.synchronize();
    other.synchronize();
    const auto ids =
        std::vector<ComponentId>{f.services.select().mounted().begin(), f.services.select().mounted().end()};
    const auto row = [&](std::size_t i) {
        return f.services.select().snapshot(ids[i]).rows.front();
    };
    const auto first = text_in(f, row(0));
    require(text(f, row(0)) == String{u8"暂无数据"} && text(f, row(1)) == String{u8"No data"} &&
                text(f, row(2)).empty() && text(f, row(3)) == String{u8"No data"},
            "Select lost inherited, explicit English/empty, or nested sources");
    const auto other_id = other.services.select().mounted().front();
    const auto other_row = other.services.select().snapshot(other_id).rows.front();
    require(text(other, other_row) == String{u8"No data"}, "Select changed no-provider English defaults");
    language.set(locales::en_US);
    f.synchronize();
    require(text(f, row(0)) == String{u8"No data"} && text(f, row(1)) == String{u8"No data"} &&
                text(f, row(2)).empty() && text_in(f, row(0)).scene == first.scene && mounts == 1 &&
                text(other, other_row) == String{u8"No data"},
            "Select locale update remounted Text, overwrote explicit text, or crossed windows");
    auto data = LocaleCatalog::builtin().pack(locales::en_US).data();
    data.common.empty = u8"Custom empty";
    const LocaleId custom{"en-XA"};
    const LocaleCatalog catalog({{custom, LocalePack(std::move(data))}});
    Fixture custom_fixture;
    custom_fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.catalog(catalog).locale(custom),
                       ConfigProviderContent{[] { Select(SelectProps{}); }});
    }});
    const auto custom_id = custom_fixture.services.select().mounted().front();
    require(text(custom_fixture, custom_fixture.services.select().snapshot(custom_id).rows.front()) ==
                String{u8"Custom empty"},
            "Select ignored the custom effective pack");
}

void select_pending_editor_and_retirement() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    Signal<bool> disabled{false};
    std::vector<SelectSource::Complete> requests;
    std::vector<std::function<void()>> posted;
    int cancellations = 0;
    int searches = 0;
    int changes = 0;
    SelectRef reference;
    const SelectSource source{[&](AsyncOptionsRequest, auto complete) {
                                  requests.push_back(std::move(complete));
                                  return [&] {
                                      ++cancellations;
                                  };
                              },
                              [&](auto task) { posted.push_back(std::move(task)); }};
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Select(SelectProps{}
                                      .showSearch()
                                      .disabled(disabled)
                                      .source(source)
                                      .ref(reference)
                                      .onSearch([&](String) { ++searches; })
                                      .onChange([&](SelectSelection) { ++changes; }));
                       }});
    }});
    f.synchronize();
    const auto id = f.services.select().mounted().front();
    click(f, f.services.select().snapshot(id).selector);
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"a中文z"}, stamp})), "Select real text input failed");
    f.synchronize();
    require(requests.size() == 1 && f.services.select().snapshot(id).pending, "Select did not start one async request");
    require(reference.select(7, 1), "Select reverse selection failed");
    auto& editor = f.inputs.editors().require(stamp.owner);
    const auto selection = editor.selection();
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"拼"}, {0, 1}, stamp})), "Select preedit failed");
    const auto history = editor.history();
    const auto before = f.services.select().snapshot(id);
    const auto starts = f.platform.starts;
    const auto search_count = searches;
    language.set(locales::zh_CN);
    f.synchronize();
    const auto after = f.services.select().snapshot(id);
    require(after.visible && after.pending && after.selector == before.selector && after.rows == before.rows &&
                after.query == before.query && after.value == before.value && after.scroll == before.scroll &&
                requests.size() == 1 && cancellations == 0 && searches == search_count && changes == 0 &&
                f.inputs.sessions().active() == stamp && f.platform.starts == starts &&
                editor.value() == String{u8"a中文z"}.bytes() && editor.selection() == selection &&
                editor.composition().active && editor.composition().text == String{u8"拼"}.bytes() &&
                editor.history().undo_count == history.undo_count &&
                text(f, after.rows.front()) == String{u8"暂无数据"},
            "Select locale switch changed pending query/editor/selection/composition or invoked business work");
    key(f, Key::escape);
    require(!editor.composition().active && f.services.select().snapshot(id).visible,
            "Select locale update broke IME Escape priority");
    requests.front()({{{1, u8"a中文z result"}}, {}});
    require(posted.size() == 1, "Select valid completion was cancelled by locale update");
    posted.front()();
    posted.clear();
    f.synchronize();
    require(!f.services.select().snapshot(id).pending && f.services.select().snapshot(id).total_rows == 1,
            "Select lost valid async completion after language change");
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"x"}, f.inputs.sessions().active()})),
            "Select second query failed");
    disabled.set(true);
    requests.back()({{{2, u8"late disabled"}}, {}});
    for (auto& task : posted) {
        task();
    }
    posted.clear();
    require(!f.services.select().snapshot(id).visible && changes == 0, "disabled Select accepted late selection work");
    require(f.services.destroy(id), "Select retirement failed");
    requests.front()({{{3, u8"retired"}}, {}});
    language.set(locales::en_US);
    f.synchronize();
    require(!reference.bound() && f.services.select().mounted().empty() && !f.services.next_frame_deadline(),
            "retired Select retained locale observers, ref, or deadline");
}

void select_rich_rows_and_idle() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    Signal<LocaleCatalog> catalog{LocaleCatalog{}};
    SelectRef selector;
    InputRef rich_editor;
    int row_runs = 0;
    int popup_runs = 0;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language).catalog(catalog), ConfigProviderContent{[&] {
                           Select(SelectProps{}
                                      .options({{1, u8"Business label"}})
                                      .showSearch()
                                      .ref(selector)
                                      .optionRender(SelectOptionRenderer{[&](SelectRenderInfo info) {
                                          ++row_runs;
                                          Text(TextProps{}.content(info.label));
                                          if (info.value == SelectValue{1}) {
                                              Input(InputProps{}.defaultValue(u8"draft").ref(rich_editor));
                                          }
                                      }})
                                      .popupRender(SelectPopupRenderer{[&](std::function<void()> origin) {
                                          ++popup_runs;
                                          origin();
                                      }}));
                       }});
    }});
    f.synchronize();
    const auto id = f.services.select().mounted().front();
    require(selector.focus(), "Select rich selector focus failed");
    key(f, Key::down);
    require(rich_editor.focus(), "Select rich editor focus failed");
    const auto stamp = f.inputs.sessions().active();
    auto& editor = f.inputs.editors().require(stamp.owner);
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"中文"}, stamp})), "Select rich real typing failed");
    require(rich_editor.select(editor.value().size(), 0), "Select rich reverse selection failed");
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"拼"}, {0, 1}, stamp})), "Select rich preedit failed");
    const auto draft = std::string(editor.value());
    const auto selection = editor.selection();
    const auto before = f.services.select().snapshot(id);
    const auto runs = row_runs;
    language.set(locales::zh_CN);
    f.synchronize();
    require(f.services.select().snapshot(id).rows == before.rows && f.services.select().snapshot(id).visible &&
                row_runs == runs && popup_runs == 1 && f.inputs.sessions().active() == stamp &&
                editor.value() == draft && editor.selection() == selection && editor.composition().active,
            "Select locale update reran rich renderer or replaced its active editor");
    const auto count = f.services.components().mount_runs();
    const auto geometry = f.services.surfaces().diagnostics().geometry_updates;
    const auto material = f.services.surfaces().diagnostics().material_updates;
    auto unused = LocaleCatalog::builtin().pack(locales::en_US).data();
    unused.image.loading = u8"Unused image group";
    catalog.set(LocaleCatalog({{LocaleId{"en-XA"}, LocalePack(std::move(unused))}}));
    f.synchronize();
    catalog.set(LocaleCatalog({{LocaleId{"en-XA"}, catalog.get().pack(LocaleId{"en-XA"})}}));
    f.synchronize();
    require(f.services.components().mount_runs() == count && row_runs == runs && popup_runs == 1 &&
                f.services.surfaces().diagnostics().geometry_updates == geometry &&
                f.services.surfaces().diagnostics().material_updates == material,
            "unused/equal locale group update dirtied Select");
    key(f, Key::escape);
    key(f, Key::end);
    key(f, Key::z, KeyModifier::control);
    require(editor.history().redo_count > 0, "Select language update discarded rich Input undo history");
    require(f.services.destroy(id), "Select rich owner retirement failed");
    language.set(locales::en_US);
    f.synchronize();
    require(!rich_editor.bound() && !f.inputs.sessions().active().valid() && !f.services.next_frame_deadline(),
            "Select rich retirement leaked editors or locale scheduling");
}

void select_reentrant_locale_and_destroy() {
    Fixture f;
    Scope observations;
    Signal<LocaleId> language{locales::en_US};
    bool reenter = false;
    bool retire = false;
    ComponentId owner;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Select(SelectProps{}.optionRender(SelectOptionRenderer{[&](SelectRenderInfo info) {
                               Text(TextProps{}.content(info.label));
                               detail::connect_prop(observations, info.label, [&](String value) {
                                   if (value == String{u8"暂无数据"}) {
                                       if (retire) {
                                           f.services.destroy(owner);
                                       } else if (reenter) {
                                           reenter = false;
                                           language.set(locales::en_US);
                                       }
                                   }
                               });
                           }}));
                           Select(SelectProps{});
                       }});
    }});
    f.synchronize();
    owner = f.services.select().mounted()[0];
    const auto inherited = f.services.select().mounted()[1];
    reenter = true;
    language.set(locales::zh_CN);
    f.synchronize();
    require(text(f, f.services.select().snapshot(owner).rows.front()) == String{u8"No data"} &&
                text(f, f.services.select().snapshot(inherited).rows.front()) == String{u8"No data"},
            "Select reentrant label observer overwrote a newer locale projection");
    retire = true;
    language.set(locales::zh_CN);
    f.synchronize();
    require(!f.services.components().contains(owner) &&
                text(f, f.services.select().snapshot(inherited).rows.front()) == String{u8"暂无数据"},
            "Select callback retirement affected a sibling or wrote retired state");
}

void tree_select_sources() {
    Fixture f;
    Fixture other;
    Signal<LocaleId> language{locales::zh_CN};
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           TreeSelect(TreeSelectProps{});
                           TreeSelect(TreeSelectProps{}.notFoundContent(u8"No data"));
                           TreeSelect(TreeSelectProps{}.notFoundContent(u8""));
                           ConfigProvider(ConfigProviderProps{}.locale(locales::en_US),
                                          ConfigProviderContent{[] { TreeSelect(TreeSelectProps{}); }});
                       }});
    }});
    other.services.mount(Content{[] { TreeSelect(TreeSelectProps{}); }});
    f.synchronize();
    other.synchronize();
    const auto owners = f.services.tree_select().mounted();
    const auto inherited = f.services.tree_select().snapshot(owners[0]);
    const auto label = text_in(f, inherited.empty);
    require(text(f, inherited.empty) == String{u8"暂无数据"} &&
                text(f, f.services.tree_select().snapshot(owners[1]).empty) == String{u8"No data"} &&
                text(f, f.services.tree_select().snapshot(owners[2]).empty).empty() &&
                text(f, f.services.tree_select().snapshot(owners[3]).empty) == String{u8"No data"},
            "TreeSelect default/explicit/nested text sources were mixed");
    language.set(locales::en_US);
    f.synchronize();
    const auto after = f.services.tree_select().snapshot(owners[0]);
    require(text(f, after.empty) == String{u8"No data"} && after.tree == inherited.tree &&
                after.selector == inherited.selector && after.empty == inherited.empty &&
                text_in(f, after.empty).scene == label.scene &&
                text(f, f.services.tree_select().snapshot(owners[2]).empty).empty() &&
                text(other, other.services.tree_select().snapshot(other.services.tree_select().mounted()[0]).empty) ==
                    String{u8"No data"},
            "TreeSelect language update replaced retained children or crossed a window/source boundary");
}

void tree_select_pending_rich_and_keys() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    Signal<bool> disabled{false};
    std::vector<TreeLoader::Complete> requests;
    int loads = 0;
    int changes = 0;
    int searches = 0;
    int title_runs = 0;
    int tag_runs = 0;
    int popup_runs = 0;
    TreeSelectRef selector;
    InputRef rich_editor;
    const TreeLoader loader{[&](AsyncOptionsRequest, auto complete) {
                                requests.push_back(std::move(complete));
                                return [] {
                                };
                            },
                            [](auto task) { task(); }};
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           TreeSelect(TreeSelectProps{}
                                          .treeCheckable()
                                          .defaultValue(TreeKeys{u8"lazy"})
                                          .disabled(disabled)
                                          .defaultOpen()
                                          .ref(selector)
                                          .loadData(loader)
                                          .onTreeLoad([&](TreeLoadInfo) { ++loads; })
                                          .onValuesChange([&](TreeKeys) { ++changes; })
                                          .onSearch([&](String) { ++searches; })
                                          .tagRender([&](TreeSelectTagInfo info) {
                                              ++tag_runs;
                                              return TreeSelectTag{[info] { Text(TextProps{}.content(info.label)); }};
                                          })
                                          .popupRender([&](std::function<void()> origin) {
                                              ++popup_runs;
                                              return TreeSelectPopup{[origin] { origin(); }};
                                          })
                                          .layout(LayoutStyle{}.width(dp(280))),
                                      {TreeSelectItem{u8"lazy", u8"Business lazy"}.richTitle(TreeTitle{[&] {
                                          ++title_runs;
                                          Input(InputProps{}.defaultValue(u8"a中文z").ref(rich_editor));
                                      }})});
                       }});
    }});
    f.synchronize();
    const auto id = f.services.tree_select().mounted()[0];
    const auto before = f.services.tree_select().snapshot(id);
    click(f, f.services.tree().snapshot(before.tree).rows.front().switcher);
    require(requests.size() == 1, "TreeSelect did not start its real child Tree loader");
    require(rich_editor.focus(), "TreeSelect rich title editor focus failed");
    const auto stamp = f.inputs.sessions().active();
    auto& editor = f.inputs.editors().require(stamp.owner);
    require(rich_editor.select(7, 1), "TreeSelect reverse selection failed");
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"拼"}, {0, 1}, stamp})), "TreeSelect preedit failed");
    f.synchronize();
    const auto old_tree = f.services.tree().snapshot(before.tree);
    const auto history = editor.history();
    const auto selection = editor.selection();
    const auto old_tags = f.services.tree_select().snapshot(id).tags;
    const auto factories = std::array{title_runs, tag_runs, popup_runs};
    language.set(locales::zh_CN);
    f.synchronize();
    const auto after = f.services.tree_select().snapshot(id);
    const auto new_tree = f.services.tree().snapshot(after.tree);
    require(after.open && after.selector == before.selector && after.tree == before.tree && after.tags == old_tags &&
                new_tree.rows.front().component == old_tree.rows.front().component && new_tree.rows.front().loading &&
                new_tree.expanded == old_tree.expanded && new_tree.checked == old_tree.checked &&
                new_tree.selected == old_tree.selected && new_tree.scroll == old_tree.scroll &&
                after.values == before.values && loads == 0 && changes == 0 && searches == 0 && requests.size() == 1 &&
                factories == std::array{title_runs, tag_runs, popup_runs} && f.inputs.sessions().active() == stamp &&
                editor.value() == String{u8"a中文z"}.bytes() && editor.selection() == selection &&
                editor.composition().active && editor.history().undo_count == history.undo_count,
            "TreeSelect language update restarted loader, reset keys/scroll, remounted slots or lost rich editing");
    requests.front()({{TreeItem{u8"child", u8"Business child"}.isLeaf()}, {}});
    f.synchronize();
    require(loads == 1 && f.services.tree_select().snapshot(id).tree == before.tree &&
                f.services.tree().snapshot(before.tree).rows.size() == 2,
            "TreeSelect cancelled valid completion on a locale update");
    disabled.set(true);
    f.synchronize();
    require(!f.services.tree_select().snapshot(id).open && !rich_editor.focus(), "disabled TreeSelect retained input");
    require(f.services.destroy(id), "TreeSelect source owner retirement failed");
    requests.front()({{TreeItem{u8"late", u8"Late"}}, {}});
    language.set(locales::en_US);
    f.synchronize();
    require(!selector.bound() && !rich_editor.bound() && !f.inputs.sessions().active().valid() &&
                !f.services.next_frame_deadline(),
            "TreeSelect retirement retained locale/loader/input lifetime");
}

void tree_select_search_editor() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    TreeSelectRef selector;
    int searches = 0;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           TreeSelect(TreeSelectProps{}.showSearch().defaultOpen().ref(selector).onSearch(
                                          [&](String) { ++searches; }),
                                      {TreeSelectItem{u8"a", u8"a中文z"}});
                       }});
    }});
    f.synchronize();
    const auto id = f.services.tree_select().mounted()[0];
    require(selector.focus(), "TreeSelect search focus failed");
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"a中文z"}, stamp})), "TreeSelect search typing failed");
    require(selector.select(7, 1), "TreeSelect search reverse selection failed");
    auto& editor = f.inputs.editors().require(stamp.owner);
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"拼"}, {0, 1}, stamp})),
            "TreeSelect search preedit failed");
    const auto before = f.services.tree_select().snapshot(id);
    const auto selection = editor.selection();
    const auto count = searches;
    language.set(locales::zh_CN);
    f.synchronize();
    require(f.services.tree_select().snapshot(id).search == before.search &&
                f.services.tree_select().snapshot(id).open && editor.selection() == selection &&
                editor.composition().active && f.inputs.sessions().active() == stamp && searches == count,
            "TreeSelect locale update changed native search draft/selection/preedit");
    key(f, Key::escape);
    key(f, Key::end);
    key(f, Key::z, KeyModifier::control);
    require(editor.history().redo_count > 0, "TreeSelect language update discarded native undo history");
    f.services.dispose();
}

void mentions_sources_and_rich_empty() {
    Fixture f;
    Signal<LocaleId> language{locales::zh_CN};
    MentionsRef reference;
    InputRef rich;
    int empty_runs = 0;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Mentions(MentionsProps{});
                           Mentions(MentionsProps{}.notFoundContent(u8"No data"));
                           Mentions(MentionsProps{}.notFoundContent(u8""));
                           ConfigProvider(ConfigProviderProps{}.locale(locales::en_US),
                                          ConfigProviderContent{[] { Mentions(MentionsProps{}); }});
                           Mentions(MentionsProps{}
                                        .defaultValue(u8"@none")
                                        .ref(reference)
                                        .notFoundContent(u8"Literal")
                                        .notFoundContent(MentionsRichLabel{MentionsOptionLabel{[&] {
                                            ++empty_runs;
                                            Input(InputProps{}.defaultValue(u8"Rich business draft").ref(rich));
                                        }}}));
                       }});
    }});
    f.synchronize();
    const auto owners =
        std::vector<ComponentId>{f.services.mentions().mounted().begin(), f.services.mentions().mounted().end()};
    require(text(f, f.services.mentions().snapshot(owners[0]).empty) == String{u8"暂无数据"} &&
                text(f, f.services.mentions().snapshot(owners[1]).empty) == String{u8"No data"} &&
                text(f, f.services.mentions().snapshot(owners[2]).empty).empty() &&
                text(f, f.services.mentions().snapshot(owners[3]).empty) == String{u8"No data"},
            "Mentions default/explicit/nested sources were confused");
    require(reference.focus({InputFocusCursor::End}), "Mentions rich empty query focus failed");
    f.synchronize();
    require(f.services.mentions().snapshot(owners[4]).visible && rich.focus(),
            "Mentions typed rich empty did not override the default label");
    const auto stamp = f.inputs.sessions().active();
    auto& editor = f.inputs.editors().require(stamp.owner);
    require(rich.select(editor.value().size(), 0), "Mentions rich empty reverse selection failed");
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"拼"}, {0, 1}, stamp})),
            "Mentions rich empty preedit failed");
    const auto selection = editor.selection();
    const auto history = editor.history();
    language.set(locales::en_US);
    f.synchronize();
    require(empty_runs == 1 && f.services.mentions().snapshot(owners[4]).visible &&
                f.inputs.sessions().active() == stamp && editor.selection() == selection &&
                editor.composition().active && editor.history().undo_count == history.undo_count &&
                text(f, f.services.mentions().snapshot(owners[0]).empty) == String{u8"No data"} &&
                text(f, f.services.mentions().snapshot(owners[2]).empty).empty(),
            "Mentions locale update reran typed empty content or changed explicit sources/editor state");
    f.services.dispose();
}

void mentions_pending_and_rich_row() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    Signal<bool> disabled{false};
    MentionsRef reference;
    InputRef rich;
    std::vector<MentionsSource::Complete> requests;
    int cancellations = 0;
    int searches = 0;
    int changes = 0;
    int row_runs = 0;
    int editor_runs = 0;
    int popup_runs = 0;
    const MentionsSource source{[&](AsyncOptionsRequest, auto complete) {
                                    requests.push_back(std::move(complete));
                                    return [&] {
                                        ++cancellations;
                                    };
                                },
                                [](auto task) { task(); }};
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Mentions(MentionsProps{}
                                        .ref(reference)
                                        .source(source)
                                        .disabled(disabled)
                                        .onChange([&](String) { ++changes; })
                                        .onSearch([&](String, String) { ++searches; })
                                        .editor(MentionsEditor{[&](MentionsEditorContext context) {
                                            ++editor_runs;
                                            TextArea(std::move(context.text_area));
                                        }})
                                        .popupRender(MentionsPopupRenderer{[&](std::function<void()> origin) {
                                            ++popup_runs;
                                            origin();
                                        }}));
                       }});
    }});
    f.synchronize();
    const auto id = f.services.mentions().mounted()[0];
    require(reference.focus(), "Mentions pending focus failed");
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"@a"}, stamp})), "Mentions real query typing failed");
    f.synchronize();
    const auto before = f.services.mentions().snapshot(id);
    const auto empty_scene = text_in(f, before.empty).scene;
    const auto search_count = searches;
    const auto change_count = changes;
    require(before.pending && before.visible && requests.size() == 1 && text(f, before.empty) == String{u8"Loading…"},
            "Mentions pending fixture did not show loading");
    language.set(locales::zh_CN);
    f.synchronize();
    auto after = f.services.mentions().snapshot(id);
    require(after.visible && after.pending && after.input == before.input && after.query == before.query &&
                after.value == before.value && after.scroll == before.scroll && requests.size() == 1 &&
                cancellations == 0 && searches == search_count && changes == change_count &&
                text(f, after.empty) == String{u8"加载中…"} && text_in(f, after.empty).scene == empty_scene,
            "Mentions language switch restarted async search or replaced its loading/editor state");
    const MentionsRichLabel rich_label{MentionsOptionLabel{[&] {
        ++row_runs;
        Input(InputProps{}.defaultValue(u8"a中文z").ref(rich));
    }}};
    requests.front()({{{u8"alpha", u8"alpha", u8"Business Alpha", false, rich_label}}, {}});
    f.synchronize();
    after = f.services.mentions().snapshot(id);
    require(!after.pending && after.rows.size() == 1 && rich.focus(),
            "Mentions valid completion did not retain rich editable row");
    const auto rich_stamp = f.inputs.sessions().active();
    auto& editor = f.inputs.editors().require(rich_stamp.owner);
    require(rich.select(7, 1), "Mentions rich row reverse selection failed");
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"拼"}, {0, 1}, rich_stamp})),
            "Mentions rich row preedit failed");
    f.synchronize();
    const auto rows = after.rows;
    const auto selection = editor.selection();
    const auto source_count = requests.size();
    language.set(locales::en_US);
    f.synchronize();
    require(f.services.mentions().snapshot(id).rows == rows && f.services.mentions().snapshot(id).visible &&
                row_runs == 1 && editor_runs == 1 && popup_runs == 1 && requests.size() == source_count &&
                searches == search_count && changes == change_count && f.inputs.sessions().active() == rich_stamp &&
                editor.selection() == selection && editor.composition().active,
            "Mentions locale update reran factories or discarded rich row editing");
    require(reference.focus({InputFocusCursor::End}), "Mentions next pending query focus failed");
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"x"}, f.inputs.sessions().active()})),
            "Mentions second query failed");
    f.synchronize();
    disabled.set(true);
    f.synchronize();
    requests.back()({{{u8"late", u8"late", u8"Late disabled"}}, {}});
    require(!f.services.mentions().snapshot(id).visible && !reference.focus(), "disabled Mentions accepted late input");
    require(f.services.destroy(id), "Mentions owner retirement failed");
    requests.front()({{{u8"retired", u8"retired", u8"Retired"}}, {}});
    language.set(locales::zh_CN);
    f.synchronize();
    require(!reference.bound() && !rich.bound() && !f.inputs.sessions().active().valid() &&
                !f.services.next_frame_deadline(),
            "Mentions retirement retained locale observers, loader or editors");
}

void mentions_composition_and_undo() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    MentionsRef reference;
    int changes = 0;
    int searches = 0;
    f.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                Mentions(
                    MentionsProps{}.ref(reference).onChange([&](String) { ++changes; }).onSearch([&](String, String) {
                        ++searches;
                    }));
            }});
    }});
    f.synchronize();
    require(reference.focus(), "Mentions composition focus failed");
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"a中文z @query"}, stamp})),
            "Mentions composition typing failed");
    require(reference.select(7, 1), "Mentions reverse selection failed");
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"拼"}, {0, 1}, stamp})),
            "Mentions composition setup failed");
    f.synchronize();
    const auto id = f.services.mentions().mounted()[0];
    const auto before = f.services.mentions().snapshot(id);
    auto& editor = f.inputs.editors().require(stamp.owner);
    const auto selection = editor.selection();
    const auto history = editor.history();
    const auto counts = std::pair{changes, searches};
    language.set(locales::zh_CN);
    f.synchronize();
    const auto after = f.services.mentions().snapshot(id);
    require(after.input == before.input && after.value == before.value && after.visible == before.visible &&
                editor.selection() == selection && editor.composition().active &&
                editor.history().undo_count == history.undo_count && f.inputs.sessions().active() == stamp &&
                counts == std::pair{changes, searches},
            "Mentions language update changed composing native editor or emitted business events");
    key(f, Key::escape);
    key(f, Key::end);
    key(f, Key::z, KeyModifier::control);
    require(editor.history().redo_count > 0, "Mentions language update destroyed undo history");
    f.services.dispose();
}

String placeholder(Fixture& f, ComponentId input) {
    return String::from_utf8(
               std::as_const(f.scene).text_state(f.inputs.text_layers(input).placeholder).content().bytes())
        .value();
}

void cascader_sources_and_rich_empty() {
    Fixture f;
    Signal<LocaleId> language{LocaleId{"en-XA"}};
    auto data = LocaleCatalog::builtin().pack(locales::en_US).data();
    data.common.empty = u8"Wrong common group";
    data.cascader.empty = u8"Custom cascader empty";
    data.cascader.search_paths = u8"Custom search";
    const LocaleCatalog catalog({{LocaleId{"en-XA"}, LocalePack(std::move(data))}});
    const CascaderLocale silent{String{}, String{}, String{}, String{}, String{}};
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language).catalog(catalog), ConfigProviderContent{[&] {
                           Cascader(CascaderProps{});
                           Cascader(CascaderProps{}.notFoundContent(String{u8"No data"}));
                           Cascader(CascaderProps{}.notFoundContent(String{}));
                           Cascader(CascaderProps{}.locale(CascaderLocale{}));
                           Cascader(CascaderProps{}.locale(silent));
                           ConfigProvider(ConfigProviderProps{}.locale(locales::en_US),
                                          ConfigProviderContent{[] { Cascader(CascaderProps{}); }});
                       }});
    }});
    f.synchronize();
    const auto owners =
        std::vector<ComponentId>{f.services.cascader().mounted().begin(), f.services.cascader().mounted().end()};
    const auto first = f.services.cascader().snapshot(owners[0]);
    const auto empty_scene = text_in(f, first.empty).scene;
    require(text(f, first.empty) == String{u8"Custom cascader empty"} &&
                placeholder(f, first.search) == String{u8"Custom search"} && placeholder(f, first.selector).empty() &&
                text(f, f.services.cascader().snapshot(owners[1]).empty) == String{u8"No data"} &&
                text(f, f.services.cascader().snapshot(owners[2]).empty).empty() &&
                text(f, f.services.cascader().snapshot(owners[3]).empty) == String{u8"No data"} &&
                text(f, f.services.cascader().snapshot(owners[4]).empty).empty() &&
                placeholder(f, f.services.cascader().snapshot(owners[4]).search).empty() &&
                text(f, f.services.cascader().snapshot(owners[5]).empty) == String{u8"No data"},
            "Cascader custom group, explicit String/complete Locale or empty placeholder source was lost");
    language.set(locales::zh_CN);
    f.synchronize();
    const auto after = f.services.cascader().snapshot(owners[0]);
    require(text(f, after.empty) == String{u8"暂无数据"} && placeholder(f, after.search) == String{u8"搜索路径"} &&
                after.selector == first.selector && after.search == first.search && after.empty == first.empty &&
                text_in(f, after.empty).scene == empty_scene &&
                text(f, f.services.cascader().snapshot(owners[1]).empty) == String{u8"No data"} &&
                text(f, f.services.cascader().snapshot(owners[2]).empty).empty() &&
                text(f, f.services.cascader().snapshot(owners[3]).empty) == String{u8"No data"} &&
                text(f, f.services.cascader().snapshot(owners[4]).empty).empty(),
            "Cascader locale projection remounted children or overwrote explicit English/empty content");
    Fixture rich_fixture;
    InputRef editor_ref;
    int empty_runs = 0;
    rich_fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Cascader(CascaderProps{}
                                        .defaultOpen(true)
                                        .notFoundContent(String{u8"Ignored"})
                                        .emptyContent(CascaderEmptyContent{[&] {
                                            ++empty_runs;
                                            Input(InputProps{}.defaultValue(u8"a中文z").ref(editor_ref));
                                        }}));
                       }});
    }});
    rich_fixture.synchronize();
    const auto rich_owner = rich_fixture.services.cascader().mounted()[0];
    require(editor_ref.focus(), "Cascader typed empty Input focus failed");
    const auto stamp = rich_fixture.inputs.sessions().active();
    auto& editor = rich_fixture.inputs.editors().require(stamp.owner);
    require(editor_ref.select(7, 1), "Cascader typed empty reverse selection failed");
    require(bool(rich_fixture.inputs.dispatch(CompositionChanged{String{u8"拼"}, {0, 1}, stamp})),
            "Cascader typed empty preedit failed");
    const auto selection = editor.selection();
    language.set(locales::en_US);
    rich_fixture.synchronize();
    require(empty_runs == 1 && rich_fixture.services.cascader().snapshot(rich_owner).open &&
                rich_fixture.inputs.sessions().active() == stamp && editor.selection() == selection &&
                editor.composition().active,
            "Cascader language update reran typed empty or replaced its active rich Input");
    rich_fixture.services.dispose();
}

void cascader_pending_literal_builtin_and_retirement() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    Signal<bool> disabled{false};
    std::vector<CascaderLoadRequest> requests;
    CascaderRef reference;
    InputRef rich;
    bool throw_loader = false;
    int row_runs = 0;
    int displays = 0;
    int changes = 0;
    auto lazy = CascaderOption{u8"lazy", String{u8"lazy"}, u8"Business lazy", {}, false, false, false};
    lazy.content = CascaderOptionContent{[&] {
        ++row_runs;
        Input(InputProps{}.defaultValue(u8"a中文z").ref(rich));
    }};
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Cascader(CascaderProps{}
                                        .options(std::vector{lazy})
                                        .defaultOpen(true)
                                        .showSearch(true)
                                        .listHeight(180.0F)
                                        .ref(reference)
                                        .disabled(disabled)
                                        .displayRender([&](const CascaderPaths&) {
                                            ++displays;
                                            return String{};
                                        })
                                        .onChange([&](CascaderPaths) { ++changes; })
                                        .loadData([&](auto request) {
                                            requests.push_back(std::move(request));
                                            if (throw_loader) {
                                                throw std::runtime_error("business loader failure");
                                            }
                                        }));
                       }});
    }});
    f.synchronize();
    const auto id = f.services.cascader().mounted()[0];
    require(reference.focus(), "Cascader lazy selector focus failed");
    key(f, Key::down);
    key(f, Key::right);
    require(requests.size() == 1 && f.services.cascader().snapshot(id).loading, "Cascader real loader did not start");
    require(rich.focus(), "Cascader pending rich row Input focus failed");
    const auto stamp = f.inputs.sessions().active();
    auto& editor = f.inputs.editors().require(stamp.owner);
    require(rich.select(7, 1), "Cascader pending reverse selection failed");
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"拼"}, {0, 1}, stamp})),
            "Cascader pending preedit failed");
    f.synchronize();
    const auto before = f.services.cascader().snapshot(id);
    const auto selection = editor.selection();
    const auto history = editor.history();
    const auto display_count = displays;
    language.set(locales::zh_CN);
    f.synchronize();
    auto after = f.services.cascader().snapshot(id);
    require(after.open && after.loading && after.selector == before.selector && after.search == before.search &&
                after.rows[0].component == before.rows[0].component && after.value == before.value &&
                after.expanded == before.expanded && after.scroll == before.scroll && row_runs == 1 &&
                displays == display_count && changes == 0 && requests.size() == 1 && !requests[0].cancelled->load() &&
                f.inputs.sessions().active() == stamp && editor.selection() == selection &&
                editor.composition().active && editor.history().undo_count == history.undo_count &&
                text(f, after.status) == String{u8"加载中…"} && text(f, after.retry) == String{u8"重试"},
            "Cascader language update cancelled loader, reran renderer or lost retained editor/keys/scroll");
    require(requests[0].complete({{}, u8"Loading failed"}), "Cascader valid literal failure completion was cancelled");
    f.synchronize();
    language.set(locales::en_US);
    language.set(locales::zh_CN);
    f.synchronize();
    after = f.services.cascader().snapshot(id);
    require(after.error == String{u8"Loading failed"} && text(f, after.status) == String{u8"Loading failed"},
            "Cascader guessed literal loader error source from default English text");
    throw_loader = true;
    bool threw = false;
    try {
        click(f, after.retry);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    f.synchronize();
    after = f.services.cascader().snapshot(id);
    require(threw && after.error == String{u8"加载失败"} && text(f, after.status) == String{u8"加载失败"} &&
                !requests.back().complete({}),
            "Cascader builtin failure did not retain translated source or reject old completion");
    language.set(locales::en_US);
    f.synchronize();
    after = f.services.cascader().snapshot(id);
    require(after.error == String{u8"Loading failed"} && text(f, after.status) == String{u8"Loading failed"},
            "Cascader builtin error was frozen to an earlier language");
    throw_loader = false;
    click(f, after.retry);
    require(f.services.cascader().snapshot(id).loading, "Cascader retry did not start a new request");
    disabled.set(true);
    f.synchronize();
    require(!requests.back().complete({{{u8"late", String{u8"late"}, u8"Late"}}, {}}),
            "disabled Cascader accepted late async completion");
    require(f.services.destroy(id), "Cascader retirement failed");
    require(!requests[0].complete({}), "retired Cascader accepted an old completion");
    language.set(locales::zh_CN);
    f.synchronize();
    require(!reference.bound() && !rich.bound() && !f.inputs.sessions().active().valid() &&
                !f.services.next_frame_deadline(),
            "Cascader retirement retained locale observers, loader or editor");
}

void cascader_search_tags_and_undo() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    CascaderRef reference;
    InputRef tag_editor;
    int tag_runs = 0;
    int renders = 0;
    int filters = 0;
    int searches = 0;
    int changes = 0;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Cascader(CascaderProps{}
                                        .options(std::vector<CascaderOption>{{u8"a", String{u8"a"}, u8"Alpha"},
                                                                             {u8"b", String{u8"b"}, u8"Beta"}})
                                        .multiple(true)
                                        .showSearch(true)
                                        .defaultOpen(true)
                                        .defaultValue({{String{u8"a"}}, {String{u8"b"}}})
                                        .defaultSearchValue(u8"Alpha")
                                        .ref(reference)
                                        .maxTagCount(std::size_t{1})
                                        .tagRender([&](CascaderTagInfo info) {
                                            ++tag_runs;
                                            return CascaderTagContent{[&, info] {
                                                if (info.value == CascaderPath{String{u8"a"}}) {
                                                    Input(InputProps{}.defaultValue(u8"a中文z").ref(tag_editor));
                                                } else {
                                                    Text(TextProps{}.content(info.label));
                                                }
                                            }};
                                        })
                                        .search(CascaderSearch{.filter =
                                                                   [&](const String&, const auto&) {
                                                                       ++filters;
                                                                       return true;
                                                                   },
                                                               .render =
                                                                   [&](const String&, const auto&) {
                                                                       ++renders;
                                                                       return String{u8"Business search result"};
                                                                   }})
                                        .onSearch([&](String) { ++searches; })
                                        .onChange([&](CascaderPaths) { ++changes; })
                                        .layout(LayoutStyle{}.width(dp(300))));
                       }});
    }});
    f.synchronize();
    const auto id = f.services.cascader().mounted()[0];
    require(tag_editor.focus(), "Cascader retained tag Input focus failed");
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"中"}, stamp})), "Cascader tag real typing failed");
    auto& editor = f.inputs.editors().require(stamp.owner);
    require(tag_editor.select(editor.value().size(), 0), "Cascader tag reverse selection failed");
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"拼"}, {0, 1}, stamp})), "Cascader tag preedit failed");
    f.synchronize();
    const auto before = f.services.cascader().snapshot(id);
    const auto selection = editor.selection();
    const auto history = editor.history();
    const auto counts = std::array{tag_runs, renders, filters, searches, changes};
    language.set(locales::zh_CN);
    f.synchronize();
    const auto after = f.services.cascader().snapshot(id);
    require(after.open && after.selector == before.selector && after.panel == before.panel &&
                after.value == before.value && after.expanded == before.expanded &&
                after.tags.size() == before.tags.size() && after.tags[0].component == before.tags[0].component &&
                after.rows[0].component == before.rows[0].component && f.inputs.sessions().active() == stamp &&
                editor.selection() == selection && editor.composition().active &&
                editor.history().undo_count == history.undo_count &&
                counts == std::array{tag_runs, renders, filters, searches, changes},
            "Cascader locale update reran search/tag renderer or reset retained tag editing/business state");
    key(f, Key::escape);
    key(f, Key::end);
    key(f, Key::z, KeyModifier::control);
    require(editor.history().redo_count > 0, "Cascader locale update discarded undo history");
    f.services.dispose();
}

void select_scroll_clip_after_language() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    SelectRef reference;
    std::vector<SelectOption> options;
    for (int i = 0; i < 100; ++i) {
        options.push_back({i, String::from_utf8("Business " + std::to_string(i)).value()});
    }
    int changes = 0;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Select(SelectProps{}.options(options).ref(reference).listHeight(64.0F).onChange(
                               [&](SelectSelection) { ++changes; }));
                       }});
    }});
    f.synchronize();
    const auto id = f.services.select().mounted()[0];
    require(reference.focus(), "Select scroll locale focus failed");
    key(f, Key::down);
    auto snapshot = f.services.select().snapshot(id);
    require(f.inputs.dispatch(ScrollInputEvent{0, -10, snapshot.popup.x + 10, snapshot.popup.y + 10}),
            "Select scroll locale real wheel failed");
    f.synchronize();
    const auto before = f.services.select().snapshot(id);
    require(before.scroll > 0 && before.rows.size() <= 6, "Select scroll fixture did not use a virtual window");
    language.set(locales::zh_CN);
    f.synchronize();
    snapshot = f.services.select().snapshot(id);
    require(snapshot.visible && snapshot.scroll == before.scroll && snapshot.rows == before.rows &&
                snapshot.active == before.active && changes == 0,
            "Select locale update reset virtual rows, scroll or active selection");
    bool selected = false;
    for (std::size_t i = 0; i < snapshot.rows.size(); ++i) {
        const auto rect = bounds(f, snapshot.rows[i]);
        const auto y = rect.y + rect.height / 2;
        if (y > snapshot.popup.y + 4 && y < snapshot.popup.y + snapshot.popup.height - 4) {
            const auto hit = f.services.hit_test().hit_test({rect.x + rect.width / 2, y});
            require(hit && descendant(f, f.services.interactions().require(*hit).component, snapshot.rows[i]),
                    "Select popup clip/hit ownership changed after locale layout");
            click(f, snapshot.rows[i]);
            require(f.services.select().snapshot(id).value ==
                        SelectSelection{static_cast<int>(snapshot.row_indices[i])},
                    "Select pointer hit selected the wrong virtual row after language switch");
            selected = true;
            break;
        }
    }
    require(selected && changes == 1, "Select locale popup had no working pointer target");
    f.services.dispose();
}

void unused_groups_equal_packs_and_owner_retirement() {
    Fixture f;
    const LocaleId custom{"en-XA"};
    const auto make_catalog = [&](String image_loading) {
        auto data = LocaleCatalog::builtin().pack(locales::en_US).data();
        data.image.loading = std::move(image_loading);
        return LocaleCatalog({{custom, LocalePack(std::move(data))}});
    };
    Signal<LocaleCatalog> catalog{make_catalog(u8"Unused image A")};
    Signal<LocaleId> language{custom};
    int mounts = 0;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.catalog(catalog).locale(language), ConfigProviderContent{[&] {
                           ++mounts;
                           Select(SelectProps{});
                           TreeSelect(TreeSelectProps{});
                           Mentions(MentionsProps{});
                           Cascader(CascaderProps{});
                       }});
    }});
    f.synchronize();
    f.synchronize();
    const auto requests = f.frames.counters().requests;
    const auto count = f.services.components().mount_runs();
    const auto geometry = f.services.surfaces().diagnostics().geometry_updates;
    const auto material = f.services.surfaces().diagnostics().material_updates;
    catalog.set(make_catalog(u8"Unused image B"));
    require(f.frames.counters().requests == requests && f.dirty.layout_roots().empty() &&
                f.dirty.geometry_nodes().empty(),
            "an unused effective locale group scheduled selection layout/frame work");
    catalog.set(make_catalog(u8"Unused image B"));
    f.synchronize();
    require(f.frames.counters().requests == requests && mounts == 1 && f.services.components().mount_runs() == count &&
                f.services.surfaces().diagnostics().geometry_updates == geometry &&
                f.services.surfaces().diagnostics().material_updates == material && !f.services.next_frame_deadline(),
            "equal/new-pointer locale data prevented idle or remounted the selection family");
    language.set(locales::zh_CN);
    f.synchronize();
    f.synchronize();
    require(!f.services.next_frame_deadline(), "selection language change introduced a persistent deadline");
    f.services.dispose();
    const auto retired_requests = f.frames.counters().requests;
    catalog.set(make_catalog(u8"Unused image C"));
    language.set(custom);
    require(f.nodes.size() == 0 && f.services.interactions().size() == 0 &&
                f.frames.counters().requests == retired_requests && !f.services.next_frame_deadline(),
            "retired selection family retained locale observers or frame work");
}

void retained_appends_during_another_provider_mount() {
    Fixture f;
    const MessageKey label{u8"selection.context", String{u8"English context"}};
    const MessageCatalog dictionary({{locales::zh_CN, label, String{u8"中文上下文"}}});
    Signal<std::vector<SelectOption>> options{std::vector<SelectOption>{}};
    Signal<SelectValues> values{SelectValues{}};
    Signal<std::vector<MentionsOption>> mentions{std::vector<MentionsOption>{}};
    I18n select_row;
    I18n select_tag;
    I18n mention_row;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(locales::zh_CN).messages(dictionary), ConfigProviderContent{[&] {
                           Select(SelectProps{}
                                      .mode(SelectMode::Multiple)
                                      .options(options)
                                      .values(values)
                                      .optionRender(SelectOptionRenderer{[&](SelectRenderInfo info) {
                                          if (info.value == SelectValue{1}) {
                                              select_row = useI18n();
                                              Text(TextProps{}.content(select_row.text(label)));
                                          } else {
                                              Text(TextProps{}.content(info.label));
                                          }
                                      }})
                                      .tagRender(SelectTagRenderer{[&](SelectRenderInfo) {
                                          select_tag = useI18n();
                                          Text(TextProps{}.content(select_tag.text(label)));
                                      }}));
                           Mentions(MentionsProps{}.options(mentions));
                       }});
        ConfigProvider(ConfigProviderProps{}.locale(locales::en_US), ConfigProviderContent{[&] {
                           options.set(std::vector<SelectOption>{{1, u8"Business"}});
                           values.set(SelectValues{SelectValue{1}});
                           mentions.set(std::vector<MentionsOption>{
                               {u8"key", u8"value", u8"Business", false, MentionsRichLabel{MentionsOptionLabel{[&] {
                                    mention_row = useI18n();
                                    Text(TextProps{}.content(mention_row.text(label)));
                                }}}}});
                       }});
    }});
    f.synchronize();
    require(select_row.active() && select_tag.active() && mention_row.active() &&
                select_row.locale().effective == locales::zh_CN && select_tag.locale().effective == locales::zh_CN &&
                mention_row.locale().effective == locales::zh_CN &&
                select_row.translate(label) == String{u8"中文上下文"} &&
                select_tag.translate(label) == String{u8"中文上下文"} &&
                mention_row.translate(label) == String{u8"中文上下文"},
            "a retained selection append captured another Provider's mount context");
    f.services.dispose();
}

void retirement_during_locale_prop_evaluation() {
    for (int family = 0; family < 4; ++family) {
        Fixture f;
        Signal<int> revision{0};
        Signal<LocaleId> language{locales::zh_CN};
        ComponentId owner;
        bool retire = false;
        int reads = 0;
        const auto evaluate = [&] {
            static_cast<void>(revision.get());
            ++reads;
            if (retire) {
                f.services.destroy(owner);
            }
        };
        f.services.mount(Content{[&] {
            ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                               const auto explicit_empty = bind([&] {
                                   evaluate();
                                   return String{u8"No data"};
                               });
                               if (family == 0) {
                                   Select(SelectProps{}.notFoundContent(explicit_empty));
                                   owner = f.services.select().mounted()[0];
                               } else if (family == 1) {
                                   TreeSelect(TreeSelectProps{}.notFoundContent(explicit_empty));
                                   owner = f.services.tree_select().mounted()[0];
                               } else if (family == 2) {
                                   Mentions(MentionsProps{}.notFoundContent(explicit_empty));
                                   owner = f.services.mentions().mounted()[0];
                               } else {
                                   Cascader(CascaderProps{}.locale(bind([&] {
                                       evaluate();
                                       return CascaderLocale{};
                                   })));
                                   owner = f.services.cascader().mounted()[0];
                               }
                           }});
        }});
        f.synchronize();
        retire = true;
        revision.set(1);
        f.synchronize();
        require(!f.services.components().contains(owner) && f.services.interactions().size() == 0 &&
                    !f.inputs.sessions().active().valid(),
                "locale prop evaluation retirement left a live selection owner or input");
        const auto retired_reads = reads;
        revision.set(2);
        language.set(locales::en_US);
        require(reads == retired_reads && !f.services.next_frame_deadline(),
                "retired locale prop observer still evaluated or scheduled work");
        f.services.dispose();
    }
}
} // namespace

int main() {
    suppress_dialogs();
    try {
        const auto run = [](const char* name, auto test) {
            std::cerr << "i18n selection: " << name << '\n';
            test();
        };
        run("select_sources_and_windows", select_sources_and_windows);
        run("select_pending_editor_and_retirement", select_pending_editor_and_retirement);
        run("select_rich_rows_and_idle", select_rich_rows_and_idle);
        run("select_reentrant_locale_and_destroy", select_reentrant_locale_and_destroy);
        run("tree_select_sources", tree_select_sources);
        run("tree_select_pending_rich_and_keys", tree_select_pending_rich_and_keys);
        run("tree_select_search_editor", tree_select_search_editor);
        run("mentions_sources_and_rich_empty", mentions_sources_and_rich_empty);
        run("mentions_pending_and_rich_row", mentions_pending_and_rich_row);
        run("mentions_composition_and_undo", mentions_composition_and_undo);
        run("cascader_sources_and_rich_empty", cascader_sources_and_rich_empty);
        run("cascader_pending_literal_builtin_and_retirement", cascader_pending_literal_builtin_and_retirement);
        run("cascader_search_tags_and_undo", cascader_search_tags_and_undo);
        run("select_scroll_clip_after_language", select_scroll_clip_after_language);
        run("unused_groups_equal_packs_and_owner_retirement", unused_groups_equal_packs_and_owner_retirement);
        run("retained_appends_during_another_provider_mount", retained_appends_during_another_provider_mount);
        run("retirement_during_locale_prop_evaluation", retirement_during_locale_prop_evaluation);
        std::cout << "i18n selection: 17 contract groups passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
