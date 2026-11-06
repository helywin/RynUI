#include "app_notice.hpp"

#include <ryn/button.hpp>
#include <ryn/text.hpp>

namespace app_notice_example {
namespace {
const ryn::MessageKey title{u8"notice.title", ryn::String{u8"Saved draft"}};
const ryn::MessageKey description{u8"notice.description",
                                  ryn::String{u8"The editor stays mounted when language changes."}};
} // namespace

void Model::open() {
    if (editor.bound()) {
        update();
        return;
    }
    auto* self = this;
    notice =
        app.notification().openAsync(u8"draft", ryn::NotificationProps{}.title(i18n.text(title)).duration(0),
                                     {.description = ryn::NotificationDescription{[self] {
                                          ++self->notice_mounts;
                                          ryn::Text(ryn::TextProps{}.content(self->i18n.text(description)));
                                          ryn::Input(ryn::InputProps{}.ref(self->editor).defaultValue(u8"My draft"));
                                      }}});
}

void Model::update() {
    static_cast<void>(notice.update(ryn::NotificationProps{}.status(ryn::NoticeStatus::Success)));
}

void Model::close() {
    static_cast<void>(notice.close());
}

ryn::Content content(std::shared_ptr<Model> model) {
    return ryn::Content{[model = std::move(model)] {
        ryn::ConfigProvider(
            ryn::ConfigProviderProps{}
                .locale(model->language)
                .messages(ryn::MessageCatalog(
                    {{ryn::locales::zh_CN, title, ryn::String{u8"草稿已保存"}},
                     {ryn::locales::zh_CN, description, ryn::String{u8"切换语言时，编辑器保持挂载。"}}})),
            ryn::ConfigProviderContent{[model] {
                ryn::App(ryn::AppProps{}, ryn::AppContent{[model] {
                             ++model->content_mounts;
                             model->app = ryn::useApp();
                             model->i18n = ryn::useI18n();
                             ryn::Button(ryn::ButtonProps{}.onClick([model] { model->open(); }),
                                         [] { ryn::Text(u8"Open notice"); });
                             ryn::Button(ryn::ButtonProps{}.onClick([model] { model->update(); }),
                                         [] { ryn::Text(u8"Update status"); });
                             ryn::Button(ryn::ButtonProps{}.onClick([model] {
                                 model->language.set(model->language.get() == ryn::locales::zh_CN
                                                         ? ryn::locales::en_US
                                                         : ryn::locales::zh_CN);
                             }),
                                         [] { ryn::Text(u8"中文 / English"); });
                             ryn::Button(ryn::ButtonProps{}.onClick([model] { model->close(); }),
                                         [] { ryn::Text(u8"Close notice"); });
                         }});
            }});
    }};
}
} // namespace app_notice_example
