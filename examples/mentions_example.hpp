#pragma once
#include <ryn/rynui.hpp>

inline void MentionsForm(ryn::FormRef form, ryn::FormField<ryn::String> text, ryn::MentionsRef editor) {
    using namespace ryn;
    Form(FormProps{}.ref(form), FormContent{[=] {
             FormItem(FormItemProps{}.field(u8"message", text).label(String{u8"消息"}).control(editor),
                      FormItemContent{[=] {
                          Mentions(MentionsProps{}
                                       .value(text.value())
                                       .onChange(text.onChange())
                                       .ref(editor)
                                       .prefix({u8"@", u8"#"})
                                       .options({{u8"ada", u8"Ada", u8"Ada"}, {u8"native", u8"native", u8"Native"}})
                                       .rows(std::size_t{3})
                                       .autoSize(TextAreaAutoSize{true, 3, 6})
                                       .maxLength(std::size_t{500})
                                       .placeholder(u8"输入 @Ada 或 #native")
                                       .allowClear()
                                       .layout(LayoutStyle{}.width(dp(320))));
                      }});
             Button(ButtonProps{}.onClick([=] { (void)form.reset(); }), ButtonContent{[] { Text(u8"重置"); }});
         }});
}
