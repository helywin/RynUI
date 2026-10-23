# Tabs

060正在实现原生Tabs。当前阶段提供typed API声明、选择模型和Tabs Token；实际标签栏、保留面板、Gallery与原生平台验收按tasks推进，API声明不代表可挂载组件已完成。

```cpp
ryn::Signal<ryn::String> active{ryn::String{u8"account"}};
ryn::Tabs(ryn::TabsProps{}.activeKey(active)
    .onChange([active](ryn::String key) mutable { active.set(std::move(key)); })
    .layout(ryn::LayoutStyle{}.width(ryn::dp(480))), {
        ryn::TabItem{u8"account", u8"账户", ryn::TabContent{[] {
            ryn::Input(ryn::InputProps{}.defaultValue(u8"草稿"));
        }}},
        ryn::TabItem{u8"settings", u8"设置", ryn::TabContent{[] {
            ryn::Text(u8"设置内容");
        }}}
    });
```

TabItem集合在挂载时固定，key为非空唯一String；`.label(Prop<String>)`与`.disabled(Prop<bool>)`可响应更新。label为空时显示key。所有TabContent首版一次预建，后续切换保留控件和编辑状态；动态增删/重排、lazy/destroyOnHidden后续扩展。

`activeKey`/`onChange`为受控请求语义；父级拒绝回写时仍显示旧面板。`defaultActiveKey`用于非受控；二者冲突拒绝。没有显式key时选择首个enabled，空集合/全禁用无初始面板。显式未知key保留但无面板，显式禁用key可显示对应面板；disabled只阻止用户导航激活，不偷偷改写已有值。重复激活不重复onChange，用户激活通知onTabClick。

首版范围是Line/Card、ControlSize三尺寸、Top/Bottom与centered，标签组单一Tab入口和横向overflow导航。vertical/RTL、富标签/额外栏、overflow popup、editable-card、动态结构、动画、自定义indicator与Web/DOM API尚未覆盖。

视觉使用`ThemeConfig::tabs`。颜色、17个布局尺寸、三尺寸字体和focus效果分四组捕获失效，支持继承/reset、独立component seed/algorithm和显式覆盖。默认line字号14/14/16、padding8/12/16，card高度32/40/48、gutter2，line gutter固定32；配方基于锁定Ant Design6.6.5。纯颜色不测量，非法负尺寸/非有限值、零card高度/字号或越界字重会原子拒绝。
