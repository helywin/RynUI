# Tabs

060已实现可挂载的原生Tabs、选择模型和Tabs Token，并接入Gallery。平台通用和Windows原生验收已完成，Linux原生验收按tasks单独推进；证据见060的evidence目录。

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

TabItem集合支持按 key 动态更新，key为非空唯一String；`.label(Prop<String>)`与`.disabled(Prop<bool>)`可响应更新。label为空时显示key。默认一次预建 TabContent，后续切换保留控件和编辑状态；lazy/destroyOnHidden 可显式选择。

`activeKey`/`onChange`为受控请求语义；父级拒绝回写时仍显示旧面板。`defaultActiveKey`用于非受控；二者冲突拒绝。没有显式key时选择首个enabled，空集合/全禁用无初始面板。显式未知key保留但无面板，显式禁用key可显示对应面板；disabled只阻止用户导航激活，不偷偷改写已有值。重复激活不重复onChange，用户激活通知onTabClick。

Line/Card/EditableCard、三尺寸、四方向、RTL 和 centered 均为原生合同；动态内容和扩展见下节。Web DOM/CSS/React API 不移植。

标签栏只有一个Tab入口，Left/Right环绕并跳过禁用项，Home/End定位首尾，Enter/Space激活。Tab进入当前面板，Shift+Tab返回标签入口。切换先隐藏旧面板，清理其中的焦点、输入会话与Select弹层；再次显示时保留草稿与组件身份，不自动抢回焦点。未显示面板的autoFocus不会启动输入。

标签溢出时提供非Tab入口的左右箭头，滚轮仅在标签栏内横向滚动；手动滚动保留位置，键盘导航、选择变化和尺寸变化会显露目标。超长标签单行省略，命中区域与字形受视口裁剪。滚动距离对齐物理像素，已有字形通过平移和裁剪更新，不重新排版或重建几何。centered只在没有溢出时生效。

视觉使用`ThemeConfig::tabs`。颜色、17个布局尺寸、三尺寸字体和focus效果分四组捕获失效，支持继承/reset、独立component seed/algorithm和显式覆盖。默认line字号14/14/16、padding8/12/16，card高度32/40/48、gutter2，line gutter固定32；配方基于锁定Ant Design6.6.5。纯颜色不测量，非法负尺寸/非有限值、零card高度/字号或越界字重会原子拒绝。


## Change 107 扩展

`Tabs(props, Prop<std::vector<TabItem>>)` 支持稳定 key 的新增、删除和重排；`TabItem::key()` 供应用维护集合。未变 key 的 label、panel wrapper、Input 草稿和 typed richLabel 保留。相同 key 的内容工厂是构造合同；要替换富结构请使用新 key。动态描述符可更新 label/disabled/closable。非法 key 在改变树之前拒绝，新增工厂异常恢复旧结构。

`TabsType::EditableCard` 提供新增按钮、每项关闭按钮和 header Delete 操作，发送 `onEdit(key,Add|Remove)` 请求；应用通过更新 items 接受请求，`hideAdd`/`closable(false)` 控制入口。删除非受控活动项回退到同位置下一个启用项，再回退前项；受控 activeKey 不被内部覆盖。

保留兼容默认的预挂载策略。`lazy(true)` 只在首次激活时挂载；`forceRender` 可逐项预挂载并保留，显式优先于该项的 `destroyOnHidden`，避免两种策略同时开启时反复重挂载。全局或每项 `destroyOnHidden` 销毁隐藏内容，后续激活重新挂载；panel wrapper 保留，隐藏输入的焦点、捕获、IME 与 popup 由共同运行时清理。

`Top|Bottom|Left|Right` 四方向配合 `rtl` 提供实际布局和方向键；Left/Right 按逻辑 start/end 在 RTL 下互换，Top/Bottom 保持方向并镜像栏内顺序；`extraStart`、`extraEnd` 是保留 typed slots。富 label 可包含原生子控件；聚焦原生标签子控件或关闭按钮会显露对应 header，并保留当前活动 panel。溢出箭头/滚轮保留，并增加键盘可达的 Dropdown/Menu 入口（因此溢出时比旧版多一个 Tab stop），菜单只列未完全可见项。菜单拓扑更新在布局前提交，不影响主面板草稿。

`indicator(TabsIndicator{size,align})` 设置逻辑尺寸和 Start/Center/End 对齐；默认使用 label 尺寸。`animated` 控制有限激活伸展动画，遵守 Theme motion/reduced motion，失活和销毁结束动画。Gallery 增加 editable-card 动态编辑、lazy/destroy、extra slots、左右和 RTL 实例。

依据：[Ant Design 固定 Tabs API](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/tabs/index.en-US.md)。Linux HEADLESS 验证与原生 GPU/Windows 平台验收分开记录。
