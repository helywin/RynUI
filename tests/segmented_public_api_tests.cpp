#include <ryn/segmented.hpp>
#include <ryn/theme.hpp>
#include <type_traits>

static_assert(!std::is_convertible_v<ryn::SegmentedIcon, ryn::SegmentedLabel>);
static_assert(!std::is_convertible_v<ryn::SegmentedLabel, ryn::SegmentedIcon>);
static_assert(std::is_copy_constructible_v<ryn::SegmentedProps>);
static_assert(std::is_copy_constructible_v<ryn::SegmentedOption>);

int main() {
    ryn::Signal<ryn::String> selected{ryn::String{u8"a"}};
    ryn::Signal<bool> disabled{false};
    auto option = ryn::SegmentedOption{u8"a", u8"Alpha"}
                      .label(selected)
                      .disabled(disabled)
                      .content(ryn::SegmentedLabel{[] {}})
                      .icon(ryn::SegmentedIcon{[] {}});
    auto props = ryn::SegmentedProps{}
                     .value(selected)
                     .disabled(disabled)
                     .vertical(disabled)
                     .block(disabled)
                     .size(ryn::ControlSize::Small)
                     .onChange([](ryn::String) {})
                     .layout(ryn::LayoutStyle{}.width(ryn::dp(240)));
    auto defaults = ryn::SegmentedProps{}.defaultValue(u8"a");
    static_cast<void>(option);
    static_cast<void>(props);
    static_cast<void>(defaults);
}
