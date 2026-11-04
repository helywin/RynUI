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

static_assert(requires(Signal<std::vector<TabItem>> items, Signal<bool> flag, Signal<TabsIndicator> indicator) {
    Tabs(TabsProps{}
             .type(TabsType::EditableCard)
             .placement(TabsPlacement::Left)
             .rtl(flag)
             .lazy(flag)
             .destroyOnHidden(flag)
             .hideAdd(flag)
             .animated(flag)
             .indicator(indicator)
             .extraStart(TabsExtra{[] {}})
             .extraEnd(TabsExtra{[] {}})
             .onEdit([](String, TabsEditAction) {}),
         items);
    TabItem{u8"a", u8"A", TabContent{[] {}}}
        .richLabel(TabLabel{[] {}})
        .closable(flag)
        .forceRender()
        .destroyOnHidden(true);
});
static_assert(!std::is_convertible_v<TabsExtra, TabLabel>);

int main() {
    return 0;
}
