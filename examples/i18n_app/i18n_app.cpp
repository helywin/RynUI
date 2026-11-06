#include "i18n_app.hpp"

namespace i18n_example {
namespace {

const ryn::MessageKey selected_message{
    u8"app.orders.selected",
    ryn::PluralMessage{u8"count", ryn::String{u8"{count} order selected"}, u8"{count} orders selected"},
    {{u8"count", ryn::MessageArgType::UInt}}};
const ryn::MessageKey hint{u8"app.note.hint", ryn::String{u8"Write a note"}};
const ryn::MessageKey switch_language{u8"app.language.switch", ryn::String{u8"Switch language"}};
const ryn::MessageKey add_order{u8"app.orders.add", ryn::String{u8"Add order"}};

} // namespace

Model::Model()
    : messages(ryn::MessageCatalog({
          {ryn::locales::zh_CN, selected_message,
           ryn::PluralMessage{u8"count", std::nullopt, u8"已选择 {count} 个订单"}},
          {ryn::locales::zh_CN, hint, ryn::String{u8"填写备注"}},
          {ryn::locales::zh_CN, switch_language, ryn::String{u8"切换语言"}},
          {ryn::locales::zh_CN, add_order, ryn::String{u8"添加订单"}},
      })) {}

ryn::Content content(std::shared_ptr<Model> model) {
    return ryn::Content{[model] {
        ryn::ConfigProvider(
            ryn::ConfigProviderProps{}.locale(model->language).messages(model->messages),
            ryn::ConfigProviderContent{[model] {
                ++model->mount_runs;
                const auto tr = ryn::useI18n();
                model->outer = tr;
                ryn::Flex(
                    ryn::FlexProps{}.vertical(true), ryn::FlexContent{[model, tr] {
                        ryn::Text(ryn::TextProps{}.content(tr.text(selected_message, ryn::bind([model] {
                                                                       return ryn::MessageArgs{
                                                                           {u8"count", model->selected.get()}};
                                                                   }))));
                        ryn::Text(ryn::TextProps{}.content(ryn::String{u8"Literal business text"}));
                        ryn::Input(ryn::InputProps{}
                                       .ref(model->editor)
                                       .defaultValue(u8"业务草稿")
                                       .placeholder(tr.text(hint))
                                       .onChange([model](ryn::String) { ++model->edits; }));
                        ryn::Button(ryn::ButtonProps{}.onClick([model] {
                            model->language.set(model->language.get() == ryn::locales::zh_CN ? ryn::locales::en_US
                                                                                             : ryn::locales::zh_CN);
                        }),
                                    ryn::ButtonContent{
                                        [tr] { ryn::Text(ryn::TextProps{}.content(tr.text(switch_language))); }});
                        ryn::Button(
                            ryn::ButtonProps{}.onClick([model] { model->selected.set(model->selected.get() + 1); }),
                            ryn::ButtonContent{[tr] { ryn::Text(ryn::TextProps{}.content(tr.text(add_order))); }});
                        ryn::ConfigProvider(ryn::ConfigProviderProps{}
                                                .locale(ryn::locales::en_US)
                                                .formatLocale(ryn::locales::zh_CN)
                                                .direction(ryn::TextDirection::Auto)
                                                .disabled(false),
                                            ryn::ConfigProviderContent{[model] {
                                                const auto nested = ryn::useI18n();
                                                model->inner = nested;
                                                ryn::Text(ryn::TextProps{}.content(nested.text(hint)));
                                            }});
                    }});
            }});
    }};
}

} // namespace i18n_example
