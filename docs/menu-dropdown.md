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

Menu 只有一个 Tab stop。Up/Down 循环跳过禁用项，Home/End 到首末项，Enter/Space 执行活动项；Inline Right 展开，再次 Right 进入子项，Left 收起或返回祖先。受限高度内可 wheel 滚动，键盘活动行自动滚入；内容始终由内部 viewport 裁剪。

`ThemeConfig::menu.tokens` 提供 background/text/disabled/selected/selected_background/hover_background/danger/divider/group/focus、行高/间距/padding/indent/radius 与字体覆盖。Default/Dark/Compact 从已锁定的 Map/Alias 派生；颜色变化只重绘，metrics/typography 变化局部重新布局。`LayoutStyle` 只负责外部尺寸与布局。

首版范围：Vertical 平面菜单与 Group/Divider；子菜单只在 Inline 中展开（Vertical 子菜单配置直接拒绝）。没有 Horizontal、横向 overflow、vertical flyout、collapsed icon 导航、hover 延迟、虚拟化、DOM/CSS 字符串或通用 popup renderer。平台通用合同通过 MSVC headless 验证；不代表原生 GPU、Linux 或 IME 验收。
