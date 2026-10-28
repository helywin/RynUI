#pragma once
#include <ryn/rynui.hpp>

inline void AutoCompleteForm(ryn::FormRef form, ryn::FormField<ryn::String> name, ryn::AutoCompleteRef input) {
    using namespace ryn;
    Form(FormProps{}.ref(form), FormContent{[=] {
             FormItem(FormItemProps{}.field(u8"name", name).label(String{u8"名称"}).control(input),
                      FormItemContent{[=] {
                          AutoComplete(AutoCompleteProps{}
                                           .value(name.value())
                                           .onChange(name.onChange())
                                           .options({{u8"alice", u8"Alice", u8"Alice"}, {u8"bob", u8"Bob", u8"Bob"}})
                                           .placeholder(u8"输入任意名称，或选择建议")
                                           .allowClear()
                                           .status(name.status())
                                           .disabled(name.disabled())
                                           .readOnly(name.readOnly())
                                           .ref(input)
                                           .layout(LayoutStyle{}.width(dp(260))));
                      }});
             Button(ButtonProps{}.onClick([=] { static_cast<void>(form.reset()); }), [] { Text(u8"重置"); });
         }});
}
