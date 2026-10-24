#include <ryn/descriptions.hpp>
#include <ryn/rynui.hpp>
#include <type_traits>

static_assert(!std::is_convertible_v<ryn::DescriptionsLabel, ryn::DescriptionsContent>);
static_assert(!std::is_convertible_v<ryn::DescriptionsTitle, ryn::DescriptionsExtra>);
static_assert(
    std::is_same_v<decltype(&ryn::Descriptions), void (*)(ryn::DescriptionsProps, std::vector<ryn::DescriptionsItem>)>);

int main() {
    ryn::Signal<int> columns{3};
    ryn::Signal<int> span{2};
    ryn::Signal<bool> visible{true};
    ryn::DescriptionsProps props;
    props.column(columns)
        .bordered()
        .colon(false)
        .size(ryn::DescriptionsSize::Small)
        .labelPlacement(ryn::DescriptionsLabelPlacement::Vertical)
        .title(ryn::DescriptionsTitle{[] { ryn::Text(u8"Account"); }})
        .extra(ryn::DescriptionsExtra{[] { ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"Save"); }); }})
        .layout(ryn::LayoutStyle{}.width(ryn::dp(500)));
    ryn::DescriptionsItem item{u8"name", ryn::DescriptionsLabel{[] { ryn::Text(u8"Name"); }},
                               ryn::DescriptionsContent{[] { ryn::Input(ryn::InputProps{}); }}};
    item.span(span).visible(visible);
    return 0;
}
