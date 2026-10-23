#include <ryn/tabs.hpp>
#include <type_traits>

using namespace ryn;

static_assert(requires(Signal<String> key, Signal<String> label, Signal<bool> disabled, Signal<TabsType> type,
                       Signal<TabsPlacement> placement, Signal<ControlSize> size, TabContent body) {
    Tabs(TabsProps{}
             .defaultActiveKey(u8"one")
             .type(TabsType::Card)
             .placement(TabsPlacement::Bottom)
             .size(ControlSize::Large)
             .centered(),
         {TabItem{u8"one", u8"One", body}});
    Tabs(TabsProps{}
             .activeKey(key)
             .type(type)
             .placement(placement)
             .size(size)
             .centered(disabled)
             .onChange([](String) {})
             .onTabClick([](String) {})
             .layout(LayoutStyle{}.width(dp(320))),
         {TabItem{u8"one", u8"One", body}.label(label).disabled(disabled), TabItem{u8"two", u8"Two", body}.disabled()});
    Tabs(TabsProps{}.activeKey(u8"one"), {});
});
static_assert(!std::is_convertible_v<Content, TabContent>);

int main() {
    return 0;
}
