#pragma once
#include <ryn/rynui.hpp>

namespace ryn_examples {
inline void timeline(ryn::Signal<bool> reverse, ryn::Signal<ryn::String> title, ryn::InputRef draft) {
    using namespace ryn;
    Timeline(TimelineProps{}.mode(TimelineMode::Alternate).reverse(reverse).layout(LayoutStyle{}.width(dp(560))),
             {TimelineItem{u8"created", u8"Request created"}.title(title).tone(TimelineTone::Green),
              TimelineItem{u8"editing", u8""}.title(u8"Review").richContent(TimelineContent{[draft] {
                  Input(InputProps{}.defaultValue(u8"A retained review draft").ref(draft));
              }}),
              TimelineItem{u8"waiting", u8"Waiting for approval"}.title(u8"Next").loading(true)});
}
} // namespace ryn_examples
