#include "gallery_source_panel.hpp"

#include <ryn/rynui.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace rynui::example {

void gallery_source_panel(const GallerySampleSource& source, ryn::Signal<ryn::LogicalLength> width,
                          std::function<void(const ryn::String&)> copy) {
    using namespace ryn;
    const auto inner = bind([width] { return dp(std::max(1.0F, width.get().value() - 32)); });
    Collapse(
        CollapseProps{}.layout(LayoutStyle{}.width(width)),
        {CollapsePanel{
            String{u8"cpp"}, CollapseTitle{[] { Text(u8"C++ · 查看源码"); }},
            CollapseBody{[source, inner, copy = std::move(copy)] {
                const auto code = String::from_utf8(source.cpp).value();
                const auto location = std::string{source.source_path} + ':' + std::to_string(source.source_line) +
                                      " · " + std::string{source.source_function};
                Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)).layout(LayoutStyle{}.width(inner)),
                     FlexContent{[source, inner, copy, code, location] {
                         Text(TextProps{}
                                  .content(String::from_utf8(location).value())
                                  .tone(TextTone::Secondary)
                                  .layout(LayoutStyle{}.width(inner)));
                         Text(TextProps{}
                                  .content(u8"实际样例源码；页面状态与辅助函数见对应文件。")
                                  .tone(TextTone::Secondary)
                                  .layout(LayoutStyle{}.width(inner)));
                         auto config = std::make_unique<ThemeConfig>();
                         config->text.tokens.font_family = SystemFontFamily::ui_monospace;
                         config->seed.font_size = dp(12);
                         auto theme = std::make_unique<ThemeProps>();
                         theme->config(std::move(*config));
                         Theme(std::move(*theme), ThemeContent{[inner, code] {
                                   TextArea(TextAreaProps{}
                                                .defaultValue(code)
                                                .readOnly(true)
                                                .wrap(false)
                                                .rows(12)
                                                .resize(TextAreaResize::None)
                                                .layout(LayoutStyle{}.width(inner)));
                               }});
                         if (copy) {
                             Button(ButtonProps{}.size(ControlSize::Small).onClick([copy, code] { copy(code); }),
                                    [] { Text(u8"复制代码"); });
                         }
                     }});
            }}}});
}

} // namespace rynui::example
