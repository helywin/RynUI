# Menu 与 Dropdown

设计参考固定为 Ant Design 6.6.5；组件使用 `ryn` typed Props、`Prop<T>` 与 retained logical scene，不依赖 React/DOM。

```cpp
auto selected = ryn::state(ryn::MenuKeys{ryn::String{u8"home"}});
auto label = ryn::state(ryn::String{u8"Home"});
ryn::Menu(ryn::MenuProps{}
    .mode(ryn::MenuMode::Inline)
    .items({
        {u8"home", label},
        {u8"tools", u8"Tools", false, false, ryn::MenuItemKind::SubMenu,
            {{u8"logs", u8"Logs"}, {u8"reset", u8"Reset", false, true}}}
    })
    .selectedKeys(selected)
    .onSelect([=](ryn::MenuSelectInfo intent) { selected.set(intent.selected_keys); })
    .onClick([](ryn::MenuClickInfo command) { /* dispatch command.key */ })
    .layout(ryn::LayoutStyle{}.width(ryn::dp(240))));
```

`MenuItem` 的 key、kind、children 与可选 typed `MenuIcon` slot 在挂载时固定；label、disabled、danger 可使用静态值、Signal 或 Binding。组/分隔线同样需要唯一非空 key。拓扑变化通过现有结构 scope 重挂载，不暗中重跑整个页面。禁用组或子菜单会禁用其后代。

`selectedKeys` / `defaultSelectedKeys`、`openKeys` / `defaultOpenKeys` 各自互斥。选择和展开回调都是意图；受控调用方不回写时，实际值保持不变。`onClick` 每次合法命令都执行；`onSelect` 仅选择集合变化时执行，`multiple` 允许切换取消。`MenuClickInfo::key_path` 为 leaf 到祖先 submenu 的顺序。组与分隔线不可激活。

Menu 的行共享一个 Tab stop；slot 内显式嵌入的原生交互控件保留自己的焦点语义。Up/Down 循环跳过禁用项，Home/End 到首末项，Enter/Space 执行活动项；Inline Right 展开，再次 Right 进入子项，Left 收起或返回祖先。受限高度内可 wheel 滚动，键盘活动行自动滚入；内容始终由内部 viewport 裁剪。

`ThemeConfig::menu.tokens` 提供 background/text/disabled/selected/selected_background/hover_background/danger/divider/group/focus、行高/间距/padding/indent/radius 与字体覆盖。Default/Dark/Compact 从已锁定的 Map/Alias 派生；颜色变化只重绘，metrics/typography 变化局部重新布局。`LayoutStyle` 只负责外部尺寸与布局。

首版范围：Vertical 平面菜单与 Group/Divider；子菜单只在 Inline 中展开（Vertical 子菜单配置直接拒绝）。没有 Horizontal、横向 overflow、vertical flyout、collapsed icon 导航、hover 延迟、虚拟化、DOM/CSS 字符串或通用 popup renderer。平台通用合同通过 MSVC headless 验证；不代表原生 GPU、Linux 或 IME 验收。

## Dropdown

```cpp
ryn::Dropdown(
    ryn::DropdownProps{}
        .menu(ryn::MenuProps{}.items({
            {u8"copy", u8"Copy"},
            {u8"delete", u8"Delete", false, true}
        }).onClick([](ryn::MenuClickInfo command) { /* dispatch command.key */ }))
        .trigger(ryn::DropdownTriggerMode::ClickContextMenu)
        .placement(ryn::DropdownPlacement::BottomLeft),
    ryn::DropdownTrigger{[] {
        ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"Actions"); });
    }});
```

默认 click 打开；可选 ContextMenu、ClickContextMenu 或 Manual。右键使用 logical pointer 坐标；Up/Down 从 trigger 打开并聚焦首/末项。原生 Button 的 Enter/Space 与 pointer `onClick` 保留且只执行一次。静态 Text trigger 则由 wrapper 提供焦点和键盘激活。运行时增加的 trigger 结构应通过 Dropdown 所属结构 scope 重挂载，以重新建立原生 activation 装饰。

内嵌原生 trigger 的 disabled/loading activation gate 同时约束 pointer、右键与方向键；禁用 Button 不会通过 wrapper 重新获得 Tab stop 或绕过禁用打开菜单。Dropdown 自身 `.disabled()` 只控制菜单触发，不改写子控件的业务回调。

`open(Prop<bool>)` 与 `defaultOpen(bool)` 互斥。`onOpenChange(DropdownOpenInfo)` 携带 open 与 Trigger/Menu/Escape/Outside/Environment 来源；正常用户请求被拒绝时不私自提交。默认命令菜单不选择项目，可显式 `.menu(MenuProps{}.selectable(true)...)` 开启选择；命令后默认请求关闭，`.closeOnSelect(false)` 可保持打开。子菜单需要 `.menu(MenuProps{}.mode(MenuMode::Inline)...)`，在同一 viewport 内展开。

默认使用窗口 clip 做十二方向 placement、翻转和钳制；popup 不参与 trigger 的外部尺寸，不继承 trigger/document clip。内部 Menu 独立滚动、裁剪并保留节点。`ThemeConfig::dropdown.tokens` 控制背景、padding、命令行高、最小宽度、最大高度、gap、radius、shadow 与 popup priority；Menu Token 继续控制行的颜色/字体。没有任意视觉 style 参数。

Escape 从更深子浮层开始关闭；命令/Escape 关闭后恢复 trigger，外部点击保留外部控件焦点。Tab 关闭并继续到 trigger 之外的下一个有效 Tab stop（Shift+Tab 反向），遵守当前 Modal 输入边界。嵌套 Select 与 Dropdown 保持逻辑 owner，因此操作子 popup 不被视为外部点击。

环境关闭与普通受控拒绝不同：禁用、窗口失活或曾展示后失去父层/Modal 边界会压制旧 open，防止恢复时复活；新外部 open 变化或明确 trigger 打开可解除压制。初次挂载于隐藏 Modal 时尚未展示，不作 dismissed，边界激活后重新计算可见性。

首版不提供 hover trigger/delay、split Dropdown.Button、箭头、弹层自定义 renderer、任意 item DOM、跨窗口/native context menu 或独立子菜单 flyout。nested popup 的不同 priority 完整排序沿用 Popconfirm 分支共同 child-layer 修复，父集成须包含该修复；本 change 不复制它。
