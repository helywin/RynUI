#include <ryn/rynui.hpp>
#include <type_traits>

static_assert(!std::is_convertible_v<ryn::TimelineTitle, ryn::TimelineContent>);
static_assert(!std::is_convertible_v<ryn::TimelineIcon, ryn::TimelineTitle>);
static_assert(std::is_same_v<decltype(&ryn::Timeline), void (*)(ryn::TimelineProps, std::vector<ryn::TimelineItem>)>);
static_assert(!std::is_assignable_v<decltype(ryn::TimelineTokenOverride{}.dot_size)&, float>);

int main() {
    using namespace ryn;
    Signal<String> text{String{u8"Event"}};
    Signal<bool> visible{true};
    Signal<TimelineMode> mode{TimelineMode::Alternate};
    Signal<std::optional<TimelinePlacement>> placement{std::nullopt};
    TimelineProps props;
    props.mode(mode).reverse().visible(visible).layout(LayoutStyle{}.width(dp(400)));
    TimelineItem item{u8"event", text};
    item.title(text)
        .content(u8"Text")
        .placement(placement)
        .tone(TimelineTone::Green)
        .loading()
        .visible(visible)
        .richTitle(TimelineTitle{[] { Text(u8"Title"); }})
        .richContent(TimelineContent{[] { Input(InputProps{}); }})
        .icon(TimelineIcon{[] { Button(ButtonProps{}, [] { Text(u8"Open"); }); }});
    return 0;
}
