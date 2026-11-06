#include "../examples/i18n_app/i18n_app.hpp"
#include "support/i18n_test.hpp"
#include "support/input_fixture.hpp"

namespace {
using namespace ryn;
using namespace ryn_test::i18n;

void public_application_retains_real_text_and_editor() {
    ryn_test::input_component::Fixture fixture;
    auto model = std::make_shared<i18n_example::Model>();
    fixture.services.mount(i18n_example::content(model));
    fixture.synchronize();
    const auto input = fixture.inputs.mounted_inputs().front();
    const auto texts = fixture.services.text().mounted_texts();
    require(texts.size() >= 5, "public application did not mount real Text components");
    const auto translated = texts[0];
    const auto literal = texts[1];
    const auto translated_node = fixture.services.components().root(translated.component);
    const auto literal_content = fixture.scene.revisions(literal.scene).content;
    auto& editor = fixture.inputs.editors().require(input.editor);
    require(model->editor.focus(), "public InputRef failed to focus");
    require(static_cast<bool>(editor.replace_range({0, editor.value().size()}, "a中文z")), "draft setup failed");
    require(model->editor.select(7, 1), "public reverse selection setup failed");
    const auto selection = editor.selection();
    require(static_cast<bool>(editor.update_composition({String{u8"拼"}, {0, 1}, {}})), "composition setup failed");
    const auto draft = std::string(editor.value());
    const auto history = editor.history();
    const auto edits = model->edits;
    require(fixture.scene.text_state(translated.scene).content() == String{u8"已选择 2 个订单"}.view(),
            "public Text(binding) did not display translated parameters");
    model->language.set(locales::en_US);
    fixture.synchronize();
    require(fixture.scene.text_state(translated.scene).content() == String{u8"2 orders selected"}.view(),
            "public Text binding did not switch language");
    require(fixture.services.components().root(translated.component) == translated_node &&
                fixture.services.text().mounted_texts()[0].scene == translated.scene && model->mount_runs == 1 &&
                fixture.inputs.mounted_inputs().front().editor == input.editor && editor.value() == draft &&
                editor.selection() == selection && editor.composition().active &&
                editor.composition().text == String{u8"拼"}.bytes() &&
                editor.history().undo_count == history.undo_count && fixture.inputs.focused(input.component) &&
                model->edits == edits,
            "locale switch changed retained Text/editor identity, edit state, history, focus or callbacks");
    require(fixture.scene.revisions(literal.scene).content == literal_content,
            "ordinary business Text subscribed to i18n");
    model->selected.set(1);
    fixture.synchronize();
    require(fixture.scene.text_state(translated.scene).content() == String{u8"1 order selected"}.view(),
            "public count parameter did not select English one");
    model->selected.set(0);
    model->language.set(locales::zh_CN);
    fixture.synchronize();
    require(fixture.scene.text_state(translated.scene).content() == String{u8"已选择 0 个订单"}.view() &&
                model->inner.locale().effective == locales::en_US &&
                model->inner.formatLocale().effective == locales::zh_CN,
            "nested format/message locales or Chinese other branch failed");
    static_cast<void>(fixture.frames.consume_request());
    const auto requests = fixture.frames.counters().requests;
    const auto content_revision = fixture.scene.revisions(translated.scene).content;
    const auto measurement = fixture.scene.record_counters(translated.scene).measurement_synchronizations;
    const auto catalog = model->messages.get();
    model->messages.set(
        MessageCatalog(std::vector<MessageTranslation>(catalog.translations().begin(), catalog.translations().end())));
    require(!fixture.frames.pending() && fixture.frames.counters().requests == requests &&
                fixture.scene.revisions(translated.scene).content == content_revision &&
                fixture.scene.record_counters(translated.scene).measurement_synchronizations == measurement,
            "equal message catalog requested measurement or an extra frame");
    auto extended = std::vector<MessageTranslation>(catalog.translations().begin(), catalog.translations().end());
    extended.push_back({locales::zh_CN, MessageKey{u8"app.not_used", String{u8"Unused"}}, String{u8"未使用"}});
    model->messages.set(MessageCatalog(std::move(extended)));
    require(!fixture.frames.pending() && fixture.frames.counters().requests == requests &&
                fixture.scene.revisions(translated.scene).content == content_revision &&
                fixture.scene.record_counters(translated.scene).measurement_synchronizations == measurement &&
                fixture.scene.revisions(literal.scene).content == literal_content,
            "an unused key dirtied text, measurement or the idle frame state");
    ryn_test::input_component::Fixture second;
    auto second_model = std::make_shared<i18n_example::Model>();
    second_model->language.set(locales::en_US);
    second.services.mount(i18n_example::content(second_model));
    second.synchronize();
    require(model->outer.locale().effective == locales::zh_CN &&
                second_model->outer.locale().effective == locales::en_US,
            "public applications shared mutable language state across windows");
    fixture.services.dispose();
    require(!model->outer.active() && second_model->outer.active(), "window disposal leaked or crossed i18n ownership");
}

void reactive_argument_errors_keep_last_good_text() {
    ryn_test::input_component::Fixture fixture;
    Signal<int> revision{0};
    bool invalid = false;
    int evaluations = 0;
    const MessageKey key{u8"app.parameter", String{u8"Value: {value}"}, {{u8"value", MessageArgType::Text}}};
    I18n tr;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}, ConfigProviderContent{[&] {
                           tr = useI18n();
                           Text(TextProps{}.content(tr.text(key, bind([&] {
                                                                static_cast<void>(revision.get());
                                                                ++evaluations;
                                                                return invalid
                                                                           ? MessageArgs{}
                                                                           : MessageArgs{{u8"value", String{u8"有效"}}};
                                                            }))));
                       }});
    }});
    fixture.synchronize();
    const auto scene = fixture.services.text().mounted_texts().front().scene;
    const auto content = fixture.scene.revisions(scene).content;
    invalid = true;
    for (int i = 1; i <= 2; ++i) {
        const auto before = evaluations;
        rejects([&] { revision.set(i); }, I18nErrorCode::MissingArgument);
        require(evaluations == before + 1 && fixture.scene.revisions(scene).content == content &&
                    fixture.scene.text_state(scene).content() == String{u8"Value: 有效"}.view(),
                "invalid repeated args were swallowed or published half text");
    }
    invalid = false;
    revision.set(3);
    require(fixture.scene.text_state(scene).content() == String{u8"Value: 有效"}.view(),
            "valid argument recovery failed");
    const auto reads = evaluations;
    fixture.services.dispose();
    revision.set(4);
    require(reads == evaluations && !tr.active(), "disposed application retained message argument observers");
}

void captured_append_uses_live_callsite_context() {
    ryn_test::input_component::Fixture fixture;
    const MessageKey key{u8"app.append", String{u8"English"}};
    Signal<LocaleId> language{locales::zh_CN};
    runtime::CapturedComponentContext captured;
    fixture.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 ConfigProvider(ConfigProviderProps{}.locale(language).messages(
                                    MessageCatalog({{locales::zh_CN, key, String{u8"中文"}}})),
                                ConfigProviderContent{
                                    [&] { captured = runtime::require_component_build_context().capture_context(); }});
             }});
        Flex(FlexProps{}.vertical(true), FlexContent{[] {}});
    }});
    const auto holder = fixture.services.components().root_components()[0];
    const auto sibling = fixture.services.components().root_components()[1];
    fixture.services.append_slot(holder, Content{[&] {
                                     const auto tr = useI18n();
                                     Text(TextProps{}.content(tr.text(key)));
                                 }},
                                 captured);
    fixture.services.append_slot(holder, Content{[&] {
                                     const auto tr = useI18n();
                                     Text(TextProps{}.content(tr.text(key)));
                                 }});
    fixture.synchronize();
    const auto first = fixture.services.text().mounted_texts()[0];
    const auto second = fixture.services.text().mounted_texts()[1];
    require(fixture.scene.text_state(first.scene).content() == String{u8"中文"}.view() &&
                fixture.scene.text_state(second.scene).content() == String{u8"English"}.view(),
            "WindowServices/TextHost append lost the captured callsite or ordinary parent context");
    language.set(locales::en_US);
    require(fixture.scene.text_state(first.scene).content() == String{u8"English"}.view() &&
                fixture.services.text().mounted_texts()[0].component == first.component,
            "captured append used a configuration snapshot or remounted its child");
    require(fixture.services.destroy(holder), "captured append owner disposal failed");
    rejects([&] { fixture.services.append_slot(sibling, Content{[] {}}, captured); }, I18nErrorCode::StaleContext);
}

} // namespace

int main() {
    suppress_dialogs();
    try {
        public_application_retains_real_text_and_editor();
        reactive_argument_errors_keep_last_good_text();
        captured_append_uses_live_callsite_context();
        std::cout << "i18n public API: 3 contract groups passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
