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
const ryn::MessageKey switch_format{u8"app.format.switch", ryn::String{u8"Switch format language"}};
const ryn::MessageKey switch_direction{u8"app.direction.switch", ryn::String{u8"Switch layout direction"}};
const ryn::MessageKey open_note{u8"app.note.open", ryn::String{u8"Open retained note"}};
const ryn::MessageKey format_status{u8"app.format.status",
                                    ryn::String{u8"Format language: {locale}; exact value: {value}"},
                                    {{u8"locale", ryn::MessageArgType::Text}, {u8"value", ryn::MessageArgType::Text}}};
const ryn::MessageKey amount{u8"app.amount",
                             ryn::String{u8"Order {id}: {amount}"},
                             {{u8"id", ryn::MessageArgType::UInt}, {u8"amount", ryn::MessageArgType::Decimal}}};

void toggle_language(const std::shared_ptr<Model>& model) {
    model->language.set(model->language.get() == ryn::locales::zh_CN ? ryn::locales::en_US : ryn::locales::zh_CN);
}

void language_button(const std::shared_ptr<Model>& model, const ryn::I18n& tr) {
    ryn::Button(ryn::ButtonProps{}.onClick([model] { toggle_language(model); }),
                ryn::ButtonContent{[tr] { ryn::Text(ryn::TextProps{}.content(tr.text(switch_language))); }});
}

void retained_note(const std::shared_ptr<Model>& model, const ryn::I18n& tr) {
    ryn::Modal(ryn::ModalProps{}
                   .open(model->popup_open)
                   .title(tr.text(open_note))
                   .onOk([model] { model->popup_open.set(false); })
                   .onCancel([model](ryn::ModalCancelReason) { model->popup_open.set(false); }),
               ryn::ModalContent{[model, tr] {
                   ++model->rich_mount_runs;
                   ryn::Flex(ryn::FlexProps{}.vertical(true).gap(ryn::dp(8)), ryn::FlexContent{[model, tr] {
                                 ryn::Input(ryn::InputProps{}
                                                .ref(model->popup_editor)
                                                .defaultValue(u8"会议备注 / meeting note · مرحبا")
                                                .placeholder(tr.text(hint)));
                                 ryn::Select(ryn::SelectProps{}.options(
                                     std::vector<ryn::SelectOption>{{u8"draft", u8"Draft"}, {u8"ready", u8"Ready"}}));
                                 language_button(model, tr);
                             }});
               }});
}

} // namespace

Model::Model()
    : messages(ryn::MessageCatalog({
          {ryn::locales::zh_CN, selected_message,
           ryn::PluralMessage{u8"count", std::nullopt, u8"已选择 {count} 个订单"}},
          {ryn::locales::zh_CN, hint, ryn::String{u8"填写备注"}},
          {ryn::locales::zh_CN, switch_language, ryn::String{u8"切换语言"}},
          {ryn::locales::zh_CN, add_order, ryn::String{u8"添加订单"}},
          {ryn::locales::zh_CN, switch_format, ryn::String{u8"切换格式语言"}},
          {ryn::locales::zh_CN, switch_direction, ryn::String{u8"切换布局方向"}},
          {ryn::locales::zh_CN, open_note, ryn::String{u8"打开保留备注"}},
          {ryn::locales::zh_CN, format_status, ryn::String{u8"格式语言：{locale}；精确数值：{value}"}},
          {ryn::locales::zh_CN, amount, ryn::String{u8"订单 {id}：{amount}"}},
      })) {}

ryn::Content content(std::shared_ptr<Model> model) {
    return ryn::Content{[model] {
        ryn::ConfigProvider(
            ryn::ConfigProviderProps{}
                .locale(model->language)
                .formatLocale(model->format_language)
                .direction(model->direction)
                .messages(model->messages),
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
                        language_button(model, tr);
                        ryn::Button(
                            ryn::ButtonProps{}.onClick([model] { model->selected.set(model->selected.get() + 1); }),
                            ryn::ButtonContent{[tr] { ryn::Text(ryn::TextProps{}.content(tr.text(add_order))); }});
                        ryn::Text(ryn::TextProps{}.content(
                            tr.text(amount, ryn::MessageArgs{{u8"id", std::uint64_t{18446744073709551615ULL}},
                                                             {u8"amount", ryn::Decimal{"9007199254740993.125"}}})));
                        ryn::Text(ryn::TextProps{}.content(tr.text(
                            format_status, ryn::bind([tr] {
                                return ryn::MessageArgs{
                                    {u8"locale", ryn::String::from_utf8(tr.formatLocale().effective.value()).value()},
                                    {u8"value", tr.formatNumber(ryn::Decimal{"9007199254740993.125"})}};
                            }))));
                        ryn::Button(
                            ryn::ButtonProps{}.onClick([model] {
                                model->format_language.set(model->format_language.get() == ryn::locales::zh_CN
                                                               ? ryn::locales::en_US
                                                               : ryn::locales::zh_CN);
                            }),
                            ryn::ButtonContent{[tr] { ryn::Text(ryn::TextProps{}.content(tr.text(switch_format))); }});
                        ryn::Button(ryn::ButtonProps{}.onClick([model] {
                            model->direction.set(model->direction.get() == ryn::TextDirection::LeftToRight
                                                     ? ryn::TextDirection::RightToLeft
                                                     : ryn::TextDirection::LeftToRight);
                        }),
                                    ryn::ButtonContent{
                                        [tr] { ryn::Text(ryn::TextProps{}.content(tr.text(switch_direction))); }});
                        ryn::Button(
                            ryn::ButtonProps{}.onClick([model] { model->popup_open.set(true); }),
                            ryn::ButtonContent{[tr] { ryn::Text(ryn::TextProps{}.content(tr.text(open_note))); }});
                        retained_note(model, tr);
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
