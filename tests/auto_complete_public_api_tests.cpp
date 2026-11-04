#include <ryn/auto_complete.hpp>
#include <ryn/flex.hpp>
#include <concepts>

static_assert(std::same_as<ryn::AutoCompleteRef, ryn::InputRef>);
static_assert(std::same_as<decltype(ryn::AutoCompleteProps{}.value(ryn::String{})), ryn::AutoCompleteProps&>);
static_assert(std::same_as<decltype(ryn::AutoCompleteProps{}.groups(std::vector<ryn::AutoCompleteOptionGroup>{})),
                           ryn::AutoCompleteProps&>);

int main() {
    ryn::AutoCompleteSource source{[](ryn::AsyncOptionsRequest request, ryn::AutoCompleteSource::Complete complete) {
                                       complete({{{u8"a", request.query, u8"Label"}}, {}});
                                       return ryn::AsyncOptionsCancel{};
                                   },
                                   [](std::function<void()> apply) { apply(); }};
    const auto copy = source;
    auto props = ryn::AutoCompleteProps{}
                     .source(source)
                     .virtualList()
                     .direction(ryn::TextDirection::RightToLeft)
                     .popupRender(ryn::AutoCompletePopupRenderer{[](std::function<void()> origin) { origin(); }})
                     .editor(ryn::AutoCompleteEditor{[](ryn::AutoCompleteEditorContext context) {
                         ryn::TextArea(context.text_area.rows(std::size_t{2}));
                     }})
                     .clearIcon(ryn::IconSource{ryn::IconName::CloseOutlined})
                     .notFoundContent(ryn::String{u8"Empty"})
                     .motion()
                     .popupWidth(320)
                     .popupMatchSelectWidth(false)
                     .readOnly(false);
    (void)props;
    return source && copy == source ? 0 : 1;
}
