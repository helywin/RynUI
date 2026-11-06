#include "support/i18n_components.hpp"

#include <array>

namespace {
using namespace ryn_test::i18n_components;

void explicit_literals_and_complete_locales() {
    Fixture fixture;
    Signal<LocaleId> language{locales::zh_CN};
    CommonLocale empty;
    empty.confirm = String{};
    empty.cancel = String{};
    empty.empty = String{};
    Signal<CommonLocale> explicit_locale{CommonLocale{}};
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Empty(EmptyProps{}.description(u8"No data"));
                           Modal(ModalProps{}.okText(u8"OK").cancelText(u8"Cancel"), ModalContent{[] {}});
                           Popconfirm(PopconfirmProps{}.okText(u8"OK").cancelText(u8"Cancel"),
                                      PopconfirmTrigger{[] { Text(u8"Literal trigger"); }});
                           Empty(EmptyProps{}.description(String{}));
                           Modal(ModalProps{}.okText(String{}).cancelText(String{}), ModalContent{[] {}});
                           Popconfirm(PopconfirmProps{}.okText(String{}).cancelText(String{}),
                                      PopconfirmTrigger{[] { Text(u8"Empty trigger"); }});
                           Empty(EmptyProps{}.locale(explicit_locale));
                           Modal(ModalProps{}.locale(explicit_locale), ModalContent{[] {}});
                           Popconfirm(PopconfirmProps{}.locale(explicit_locale),
                                      PopconfirmTrigger{[] { Text(u8"Locale trigger"); }});
                           Empty(EmptyProps{}.locale(empty));
                           Modal(ModalProps{}.locale(empty), ModalContent{[] {}});
                           Popconfirm(PopconfirmProps{}.locale(empty),
                                      PopconfirmTrigger{[] { Text(u8"Empty Locale trigger"); }});
                       }});
    }});
    synchronize(fixture);
    const auto verify = [&] {
        labels(fixture, 0, u8"No data", u8"OK", u8"Cancel");
        labels(fixture, 1, String{}, String{}, String{});
        labels(fixture, 2, u8"No data", u8"OK", u8"Cancel");
        labels(fixture, 3, String{}, String{}, String{});
    };
    verify();
    static_cast<void>(fixture.frames.consume_request());
    const auto explicit_requests = fixture.frames.counters().requests;
    language.set(locales::en_US);
    require(!fixture.frames.pending() && fixture.frames.counters().requests == explicit_requests,
            "explicit English/empty sources subscribed to inherited language");
    synchronize(fixture);
    verify();
    language.set(locales::zh_CN);
    synchronize(fixture);
    verify();
    auto changed = CommonLocale{};
    changed.confirm = u8"Locale confirm";
    changed.cancel = u8"Locale cancel";
    changed.empty = u8"Locale description";
    explicit_locale.set(changed);
    synchronize(fixture);
    labels(fixture, 2, changed.empty, changed.confirm, changed.cancel);
    static_cast<void>(fixture.frames.consume_request());
    const auto requests = fixture.frames.counters().requests;
    explicit_locale.set(changed);
    require(!fixture.frames.pending() && fixture.frames.counters().requests == requests,
            "equal explicit Locale requested another frame");
    changed.loading = u8"Unused loading label";
    explicit_locale.set(changed);
    require(!fixture.frames.pending() && fixture.frames.counters().requests == requests,
            "an unused field in a complete Locale dirtied default labels");
}

void concrete_text_precedes_complete_locale() {
    Fixture fixture;
    CommonLocale locale;
    locale.confirm = u8"Locale confirm";
    locale.cancel = u8"Locale cancel";
    locale.empty = u8"Locale description";
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(locales::zh_CN), ConfigProviderContent{[&] {
                           Empty(EmptyProps{}.locale(locale).description(u8"No data"));
                           Modal(ModalProps{}.locale(locale).okText(u8"OK"), ModalContent{[] {}});
                           Popconfirm(PopconfirmProps{}.locale(locale).cancelText(String{}),
                                      PopconfirmTrigger{[] { Text(u8"Trigger"); }});
                       }});
    }});
    synchronize(fixture);
    require(label_text(fixture,
                       fixture.services.empty_result().snapshot(fixture.services.empty_result().empties()[0]).label) ==
                String{u8"No data"},
            "complete Locale overrode explicit Empty description");
    const auto modal = fixture.services.modal().snapshot(fixture.services.modal().mounted()[0]);
    const auto popup = fixture.services.popconfirm().snapshot(fixture.services.popconfirm().mounted()[0]);
    require(label_text(fixture, modal.ok) == String{u8"OK"} && label_text(fixture, modal.cancel) == locale.cancel &&
                label_text(fixture, popup.confirm) == locale.confirm && label_text(fixture, popup.cancel).empty(),
            "specific English/empty button text did not take priority over complete Locale");
}

void active_layers_retain_rich_input_and_callbacks() {
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    Signal<bool> loading{false};
    std::array<InputRef, 3> refs;
    int modal_body = 0;
    int modal_title = 0;
    int empty_description = 0;
    int popup_title = 0;
    int popup_description = 0;
    int changes = 0;
    int open_events = 0;
    int confirms = 0;
    int cancels = 0;
    InputRef background;
    fixture.services.mount(Content{[&] {
        Input(InputProps{}.ref(background));
        ConfigProvider(
            ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                Modal(ModalProps{}
                          .open()
                          .onOk([&] { ++confirms; })
                          .onCancel([&](ModalCancelReason) { ++cancels; })
                          .afterOpenChange([&](bool) { ++open_events; }),
                      ModalContent{[&] {
                          ++modal_body;
                          Input(InputProps{}.ref(refs[0]).defaultValue(u8"Business value").onChange([&](String) {
                              ++changes;
                          }));
                          Empty(EmptyProps{},
                                {.description = EmptyDescription{[&] {
                                     ++empty_description;
                                     Input(InputProps{}.ref(refs[1]).defaultValue(u8"No data").onChange([&](String) {
                                         ++changes;
                                     }));
                                 }}});
                          Popconfirm(
                              PopconfirmProps{}
                                  .defaultOpen()
                                  .confirmLoading(loading)
                                  .onOpenChange([&](bool) { ++open_events; })
                                  .onConfirm([&] { ++confirms; })
                                  .onCancel([&] { ++cancels; }),
                              PopconfirmTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Business trigger"); }); }},
                              {.title = PopconfirmTitle{[&] {
                                   ++popup_title;
                                   Text(u8"Business question");
                               }},
                               .description = PopconfirmDescription{[&] {
                                   ++popup_description;
                                   Input(
                                       InputProps{}.ref(refs[2]).defaultValue(u8"Business draft").onChange([&](String) {
                                           ++changes;
                                       }));
                               }}});
                      }},
                      {.title = ModalTitle{[&] {
                           ++modal_title;
                           Text(u8"No data");
                       }}});
            }});
    }});
    synchronize(fixture);
    const auto modal_id = fixture.services.modal().mounted()[0];
    const auto popup_id = fixture.services.popconfirm().mounted()[0];
    const auto modal = fixture.services.modal().snapshot(modal_id);
    const auto popup = fixture.services.popconfirm().snapshot(popup_id);
    require(modal.visible && popup.geometry.visible && !background.focus(),
            "active Modal/Popconfirm or background input confinement setup failed");
    require(refs[2].focus(), "rich popup Input did not focus");
    const auto input = fixture.inputs.mounted_inputs().back();
    auto& editor = fixture.inputs.editors().require(input.editor);
    require(bool(editor.replace_range({0, editor.value().size()}, "a中文z")) && refs[2].select(7, 1),
            "rich Input draft/reverse selection setup failed");
    const auto selection = editor.selection();
    require(bool(editor.update_composition({String{u8"拼"}, {0, 1}, {}})), "rich Input preedit setup failed");
    const auto history = editor.history();
    require(history.undo_count > 0, "rich Input did not retain a real undo transaction");
    const auto draft = std::string(editor.value());
    const auto stamp = fixture.inputs.sessions().active();
    const auto focus = fixture.services.focus().state().focused;
    const auto count = fixture.services.components().component_count();
    const auto events = open_events;
    const auto starts = fixture.platform.starts;
    const auto cancelled = fixture.platform.cancels;
    const auto ok = text_component(fixture, modal.ok);
    const auto confirm = text_component(fixture, popup.confirm);
    loading.set(true);
    language.set(locales::zh_CN);
    synchronize(fixture);
    require(label_text(fixture, modal.ok) == String{u8"确定"} &&
                label_text(fixture, modal.cancel) == String{u8"取消"} &&
                label_text(fixture, popup.confirm) == String{u8"确定"} &&
                label_text(fixture, popup.cancel) == String{u8"取消"},
            "already-open Modal/Popconfirm default buttons did not switch language");
    require(fixture.services.modal().snapshot(modal_id).dialog == modal.dialog &&
                fixture.services.modal().snapshot(modal_id).visible &&
                fixture.services.popconfirm().snapshot(popup_id).popup == popup.popup &&
                fixture.services.popconfirm().snapshot(popup_id).geometry.visible &&
                text_component(fixture, modal.ok).scene == ok.scene &&
                text_component(fixture, popup.confirm).scene == confirm.scene &&
                fixture.services.components().component_count() == count && modal_body == 1 && modal_title == 1 &&
                empty_description == 1 && popup_title == 1 && popup_description == 1,
            "language switching remounted a layer, rich slot or label");
    require(fixture.inputs.mounted_inputs().back().editor == input.editor && editor.value() == draft &&
                editor.selection() == selection && editor.composition().active &&
                editor.composition().text == String{u8"拼"}.bytes() &&
                editor.history().undo_count == history.undo_count &&
                editor.history().redo_count == history.redo_count && fixture.inputs.sessions().active() == stamp &&
                fixture.services.focus().state().focused == focus && fixture.platform.starts == starts &&
                fixture.platform.cancels == cancelled && !changes && !confirms && !cancels && open_events == events &&
                label_text(fixture, modal.title) == String{u8"No data"},
            "language switching changed business/editor state, focus, preedit, history or callbacks");
    require(fixture.services.destroy(modal_id) && background.focus() && !refs[0].bound() && !refs[1].bound() &&
                !refs[2].bound() && fixture.services.popconfirm().mounted().empty(),
            "retirement retained Modal input blocks, rich Input refs or its popup");
}

void typed_footer_and_description_slots_win() {
    Fixture fixture;
    Signal<LocaleId> language{locales::zh_CN};
    int footer_mounts = 0;
    int description_mounts = 0;
    InputRef footer;
    InputRef description;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Empty(EmptyProps{}.description(u8"Ignored literal"),
                                 {.description = EmptyDescription{[&] {
                                      ++description_mounts;
                                      Input(InputProps{}.ref(description).defaultValue(u8"No data"));
                                  }}});
                           Modal(ModalProps{}.okText(u8"Ignored OK"), ModalContent{[] {}},
                                 {.footer = ModalFooter{[&] {
                                      ++footer_mounts;
                                      Input(InputProps{}.ref(footer).defaultValue(u8"OK"));
                                  }}});
                       }});
    }});
    synchronize(fixture);
    const auto inputs = fixture.inputs.mounted_inputs();
    require(inputs.size() == 2, "typed rich description/footer did not mount real Inputs");
    const auto first = inputs[0];
    const auto second = inputs[1];
    static_cast<void>(fixture.frames.consume_request());
    const auto requests = fixture.frames.counters().requests;
    language.set(locales::en_US);
    require(!fixture.frames.pending() && fixture.frames.counters().requests == requests,
            "unused builtin labels under rich slots requested a locale frame");
    synchronize(fixture);
    require(description_mounts == 1 && footer_mounts == 1 && footer.bound() && description.bound() &&
                fixture.inputs.mounted_inputs()[0].editor == first.editor &&
                fixture.inputs.mounted_inputs()[1].editor == second.editor &&
                fixture.inputs.editors().require(first.editor).value() == "No data" &&
                fixture.inputs.editors().require(second.editor).value() == "OK",
            "locale update replaced typed description/footer content or translated business values");
}

void label_observer_reentry_and_retirement() {
    for (int kind = 0; kind < 3; ++kind) {
        Fixture fixture;
        Signal<String> label{String{u8"OK"}};
        Signal<LocaleId> language{locales::en_US};
        int label_reads = 0;
        int locale_reads = 0;
        ComponentId owner;
        bool armed = false;
        Scope observer;
        const auto projection = bind([&] {
            ++label_reads;
            return label.get();
        });
        // Register before the component so a queued label delivery must survive
        // owner retirement and a nested locale publication in this observer.
        static_cast<void>(connect_binding(observer, projection, [&](const String&) {
            if (armed) {
                armed = false;
                require(fixture.services.destroy(owner), "label observer did not retire its owner");
                language.set(locales::zh_CN);
            }
        }));
        owner =
            mount_owned(fixture, Content{[&] {
                            ConfigProvider(ConfigProviderProps{}.locale(bind([&] {
                                ++locale_reads;
                                return language.get();
                            })),
                                           ConfigProviderContent{[&] {
                                               if (kind == 0) {
                                                   Empty(EmptyProps{}.description(projection));
                                               } else if (kind == 1) {
                                                   Modal(ModalProps{}.open().okText(projection), ModalContent{[] {}});
                                               } else {
                                                   Popconfirm(PopconfirmProps{}.defaultOpen().okText(projection),
                                                              PopconfirmTrigger{[] { Text(u8"Trigger"); }});
                                               }
                                           }});
                        }});
        synchronize(fixture);
        armed = true;
        label.set(String{u8"Proceed"});
        observer.dispose();
        synchronize(fixture);
        const auto reads = label_reads;
        const auto locales_read = locale_reads;
        label.set(String{u8"Late"});
        language.set(locales::en_US);
        require(label_reads == reads && locale_reads == locales_read &&
                    !fixture.services.components().contains(owner) &&
                    fixture.services.components().component_count() == 0 && fixture.inputs.editors().size() == 0 &&
                    !fixture.services.next_frame_deadline(),
                "label observer reentry left a retired binding, input owner or deadline");
    }
}

void dynamic_empty_description_uses_current_default() {
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    InputRef retained;
    int descriptions = 0;
    int extras = 0;
    const auto description = std::make_shared<const EmptyDescription>(EmptyDescription{[&] {
        ++descriptions;
        Text(u8"Business description");
    }});
    const auto extra = std::make_shared<const EmptyExtra>(EmptyExtra{[&] {
        ++extras;
        Input(InputProps{}.ref(retained).defaultValue(u8"Business draft"));
    }});
    Signal<EmptyDynamicContent> dynamic{EmptyDynamicContent{{}, extra, description}};
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language),
                       ConfigProviderContent{[&] { Empty(EmptyProps{}.dynamicContent(dynamic)); }});
    }});
    synchronize(fixture);
    const auto owner = fixture.services.empty_result().empties()[0];
    const auto input = fixture.inputs.mounted_inputs()[0];
    require(retained.focus() && retained.select(8, 0), "dynamic Empty retained Input setup failed");
    const auto selection = fixture.inputs.editors().require(input.editor).selection();
    language.set(locales::zh_CN);
    synchronize(fixture);
    auto snapshot = fixture.services.empty_result().snapshot(owner);
    require(label_text(fixture, snapshot.label) == String{u8"Business description"} && descriptions == 1 && extras == 1,
            "language switching translated or remounted a dynamic rich description");
    dynamic.set(EmptyDynamicContent{{}, extra, {}});
    synchronize(fixture);
    snapshot = fixture.services.empty_result().snapshot(owner);
    require(label_text(fixture, snapshot.label) == String{u8"暂无数据"} && extras == 1 &&
                fixture.inputs.mounted_inputs()[0].editor == input.editor &&
                fixture.inputs.editors().require(input.editor).selection() == selection && retained.bound(),
            "dynamic Empty fallback used stale English or replaced the retained extra Input");
    language.set(locales::en_US);
    synchronize(fixture);
    require(label_text(fixture, snapshot.label) == String{u8"No data"},
            "dynamic Empty fallback stopped following its live locale");
}
} // namespace

int main() {
    ryn_test::i18n::suppress_dialogs();
    try {
        explicit_literals_and_complete_locales();
        concrete_text_precedes_complete_locale();
        active_layers_retain_rich_input_and_callbacks();
        typed_footer_and_description_slots_win();
        label_observer_reentry_and_retirement();
        dynamic_empty_description_uses_current_default();
        std::cout << "i18n component overrides passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
