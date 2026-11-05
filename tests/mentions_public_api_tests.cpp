#include <ryn/mentions.hpp>
#include <ryn/rynui.hpp>
#include "../examples/mentions_example.hpp"
#include <type_traits>

static_assert(std::is_same_v<ryn::MentionsRef, ryn::TextAreaRef>);
static_assert(std::is_copy_constructible_v<ryn::MentionsRichLabel>);
static_assert(std::is_copy_constructible_v<ryn::MentionsProps>);

int main() {
    using namespace ryn;
    Signal<String> prefix{String{u8"@"}};
    Signal<String> split{String{u8" "}};
    Signal<std::vector<MentionsOption>> options{std::vector<MentionsOption>{}};
    MentionsProps props;
    props.prefix(prefix)
        .split(split)
        .options(options)
        .filterOption(false)
        .validateSearch(
            MentionsSearchValidator{[](StringView text, const MentionsConfig&) { return text.size_bytes() < 50; }})
        .visible(true)
        .loading(false)
        .autoSize()
        .rows(std::size_t{2})
        .maxLength(std::size_t{100})
        .placement(MentionsPlacement::Top)
        .size(ControlSize::Small)
        .status(InputStatus::Warning)
        .variant(InputVariant::Underlined)
        .direction(TextDirection::RightToLeft)
        .clearDisabled(false)
        .onSearch([](String, String) {})
        .onSelect([](MentionsOption, String) {})
        .notFoundContent(MentionsRichLabel{MentionsOptionLabel{[] { Text(u8"No matching person"); }}})
        .popupRender(MentionsPopupRenderer{[](auto origin) { origin(); }});
    return getMentions(u8"@Ada #native", {{u8"@", u8"#"}, u8" "}).size() == 2 ? 0 : 1;
}
