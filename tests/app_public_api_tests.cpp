#include "../examples/app_notice/app_notice.hpp"
#include "support/input_fixture.hpp"
#include "support/i18n_test.hpp"

namespace {
using namespace ryn;
using ryn_test::i18n::require;

void application() {
    ryn_test::input_component::Fixture fixture;
    const auto model = std::make_shared<app_notice_example::Model>();
    fixture.services.mount(app_notice_example::content(model));
    fixture.synchronize();
    model->open();
    fixture.synchronize();
    require(model->app.bound() && model->editor.bound() && model->notice_mounts == 1,
            "public App example did not mount the real Notice/editor");
    const auto input = fixture.inputs.mounted_inputs().back();
    auto& editor = fixture.inputs.editors().require(input.editor);
    require(model->editor.focus() && model->editor.select(7, 1), "public App Input setup failed");
    const auto selection = editor.selection();
    model->language.set(locales::en_US);
    model->update();
    fixture.synchronize();
    require(fixture.inputs.mounted_inputs().back().editor == input.editor && editor.selection() == selection &&
                model->content_mounts == 1 && model->notice_mounts == 1,
            "public service language/status update rebuilt retained content");
    model->close();
    require(model->notice.closed().get() == NoticeCloseResult::Closed && !model->editor.bound(),
            "public example close did not settle/retire content");
    fixture.services.dispose();
    require(!model->app.bound(), "public example App ref survived window teardown");
}

void legacy_holders() {
    ryn_test::input_component::Fixture fixture;
    MessageRef messages;
    NotificationRef notifications;
    fixture.services.mount(Content{[&] {
        MessageHolder(MessageHolderProps{}.ref(messages));
        NotificationHolder(NotificationHolderProps{}.ref(notifications));
    }});
    const auto message = messages.openAsync(u8"old", MessageProps{}.content(u8"Legacy Message").duration(0));
    const auto notification =
        notifications.openAsync(u8"old", NotificationProps{}.title(u8"Legacy Notification").duration(0));
    require(message.update(MessageProps{}.status(NoticeStatus::Success)) &&
                notification.update(NotificationProps{}.description(u8"Still compatible")) && messages.clear() &&
                notifications.clear() && message.closed().get() == NoticeCloseResult::Closed &&
                notification.closed().get() == NoticeCloseResult::Closed,
            "App service extension broke existing holder/ref source compatibility");
}
} // namespace

int main() {
    ryn_test::i18n::suppress_dialogs();
    try {
        application();
        legacy_holders();
        std::cout << "Public App API example and existing holders completed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
