# Menu 与 Dropdown

Token Gallery 的 Menu 独立页提供三种模式、稳定 key 重排、RTL 和折叠导航；Dropdown 独立页提供点击/右键、hover 组合触发、分离按钮和保留编辑草稿的自定义 popup。既有 Table 联动命令与 frozen sample IDs 保持可用。

设计参考固定为 Ant Design 6.6.5；组件使用 `ryn` typed Props、`Prop<T>` 与 retained logical scene，不依赖 React/DOM。

```cpp
ryn::Signal<ryn::MenuKeys> selected{ryn::MenuKeys{ryn::String{u8"home"}}};
ryn::Signal<ryn::String> label{ryn::String{u8"Home"}};
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

`MenuItem` 的非空 key 全树唯一。旧 `items(vector)` 与所有构造函数保持可用；新增 `items(Prop<vector<MenuItem>>)` 可通过 `bind` 更新拓扑。新增、删除、重排与跨父项移动按 key 对齐，未变项保留行、图标和富内容控件身份。相同 key/kind 的 slot factory 保留首次挂载内容；其内部使用响应属性更新，改变 slot 结构或 kind 时显式更换 key。`content: MenuLabel` 与 `extra: MenuExtra` 提供保留富标签和辅助内容，`icon: MenuIcon` 保持兼容。删除活动项优先回退到相邻可用项；删除项的捕获、焦点、子浮层和绑定随组件释放。

`selectedKeys` / `defaultSelectedKeys`、`openKeys` / `defaultOpenKeys` 各自互斥。选择和展开回调都是请求；受控调用方不回写时实际值保持不变。`onClick` 每次合法命令都执行；`onSelect` 仅选择集合变化时执行，`MenuSelectInfo::selected` 区分多选取消。`MenuClickInfo::key_path` 为 leaf 到祖先 submenu 的顺序。组与分隔线不可激活；禁用祖先使其后代不可操作。

`mode(Prop<MenuMode>)` 支持 Vertical flyout、Horizontal 导航/溢出弹层和 Inline 层级。`inlineCollapsed` 在 Inline 模式将保留内容切换为居中图标导航，无图标项显示完整的首个 Unicode 字素；富标签输入在折叠期间停用，恢复后保留草稿。未显式指定外部宽度时使用 `menu.tokens.collapsed_width`（默认 80），`collapsed_icon_size` 控制折叠字号；显式 LayoutStyle 宽度仍由应用决定，子菜单使用 flyout。横向空间不足时尾部条目进入可键盘访问的溢出菜单，窗口缩放不重新执行条目 slot。`direction(TextDirection::RightToLeft)` 镜像行排列、水平键盘导航及 flyout 定位；`theme(MenuTheme::Inherit/Light/Dark)` 控制菜单配色；Light/Dark 使用 `ThemeSnapshot::menu().light_colors/dark_colors`，保留 scoped seed、alias 和 Menu 颜色覆盖，尺寸与字体仍跟随当前 Theme。`triggerSubMenuAction` 可用 Hover 或 Click，`subMenuOpenDelay` / `subMenuCloseDelay` 为有限非负秒数（默认 0 / 0.1）；离页、窗口失活、禁用及移除取消延迟工作。

Menu 行共享一个 Tab stop；富 slot 中的原生控件保留自身焦点语义。Up/Down 在当前弹层循环并跳过禁用项，Home/End 到首末项；Horizontal 根菜单以方向相关的 Left/Right 移动，Up/Down 打开子菜单；Enter/Space 执行。Inline 的方向相关前进键展开/进入，后退键收起/返回祖先；flyout 的 Escape 先关闭最深子菜单。根 viewport 与每级 popup 独立裁剪和 wheel 滚动，键盘活动行自动滚入，popup 在窗口边界翻转和钳制。

`ThemeConfig::menu.tokens` 提供 background/text/disabled/selected/selected_background/hover_background/danger/divider/group/focus、行高/间距/padding/indent/radius 与字体覆盖。Default/Dark/Compact 从锁定 Map/Alias 派生；颜色变化只重绘，metrics/typography 变化局部重新布局。`LayoutStyle` 仅控制外部布局。浮层使用既有 logical scene、window layer 和 Theme shadow；不引入 renderer/SDL API。Web DOM/CSS/React 与系统辅助技术桥接不属于本 API，原生 GPU/系统字体/输入平台证据单独记录。

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

为保持兼容，默认 Click 打开；可选 ContextMenu、ClickContextMenu、Manual、Hover、HoverClick、HoverContextMenu 或 HoverClickContextMenu。右键使用 logical pointer 坐标；Up/Down 从 trigger 打开并聚焦首/末项。原生 Button 的 Enter/Space 与 pointer `onClick` 保留且只执行一次。静态 Text trigger 则由 wrapper 提供焦点和键盘激活。运行时增加的 trigger 结构应通过 Dropdown 所属结构 scope 重挂载，以重新建立原生 activation 装饰。

内嵌原生 trigger 的 disabled/loading activation gate 同时约束 pointer、右键与方向键；禁用 Button 不会通过 wrapper 重新获得 Tab stop 或绕过禁用打开菜单。Dropdown 自身 `.disabled()` 只控制菜单触发，不改写子控件的业务回调。

`open(Prop<bool>)` 与 `defaultOpen(bool)` 互斥。`onOpenChange(DropdownOpenInfo)` 携带 open 与 Trigger/Menu/Escape/Outside/Environment 来源；正常用户请求被拒绝时不私自提交。默认命令菜单不选择项目，可显式 `.menu(MenuProps{}.selectable(true)...)` 开启选择；命令后默认请求关闭，`.closeOnSelect(false)` 可保持打开。子菜单可用 Inline 在同一 viewport 展开，也可用 Vertical 的独立 flyout。

默认使用窗口 clip 做十二方向 placement、翻转和钳制；popup 不参与 trigger 的外部尺寸，不继承 trigger/document clip。内部 Menu 独立滚动、裁剪并保留节点。`ThemeConfig::dropdown.tokens` 控制背景、padding、命令行高、最小宽度、最大高度、gap、radius、shadow 与 popup priority；Menu Token 继续控制行的颜色/字体。没有任意视觉 style 参数。

Escape 从更深子浮层开始关闭；命令/Escape 关闭后恢复 trigger，外部点击保留外部控件焦点。Tab 关闭并继续到 trigger 之外的下一个有效 Tab stop（Shift+Tab 反向），遵守当前 Modal 输入边界。嵌套 Select 与 Dropdown 保持逻辑 owner，因此操作子 popup 不被视为外部点击。

环境关闭与普通受控拒绝不同：禁用、窗口失活或曾展示后失去父层/Modal 边界会压制旧 open，防止恢复时复活；新外部 open 变化或明确 trigger 打开可解除压制。初次挂载于隐藏 Modal 时尚未展示，不作 dismissed，边界激活后重新计算可见性。

`mouseEnterDelay` / `mouseLeaveDelay` 是 0..3600 的有限秒数，默认 0.15 / 0.1。Hover 只响应 mouse，不抢走当前焦点；从 trigger 跨入 popup 或其子浮层取消延迟关闭。组合 Click 会保留显式打开状态；禁用、窗口失活、离页或捕获取消清除待执行请求。Trigger 请求的 `DropdownOpenInfo::cause` 区分 Pointer/Keyboard/Hover/ContextMenu，`source` 的既有枚举与回调签名保持兼容。旧的两字段 aggregate 初始化仍可用；对该 struct 做结构化绑定的代码需要接收第三字段或改为具名成员访问。受控拒绝不修改应用状态，也不在 idle 反复重试。

`.arrow()` 或 `.arrow(DropdownArrow{true, true})` 显示箭头，第二个字段让箭头指向 trigger 中心。十二方向 placement 继续自动翻转/钳制；箭头和 popup 主体通过已有 logical TooltipArrowPath 组成一次填充，不叠加透明背景或改变 GPU ABI。尺寸使用独立的 `ThemeConfig::dropdown.tokens.arrow_size`；其他 Dropdown Token、继承与完整 Theme diagnostic/golden shape 保持有效。

`popupRender(DropdownPopup)` 用保留 typed slot 替换默认内容，适合编辑器、按钮或任意原生组合。需要保留内置 Menu 时，用下面的 renderer overload；`DropdownMenuContent` 只能在该 popup 的构建上下文挂载一次。默认隐藏保留内容及草稿，重新打开不运行 renderer；销毁 owner 释放整个子树。

```cpp
ryn::Dropdown(
    ryn::DropdownProps{}.menu(ryn::MenuProps{}.items({{u8"apply", u8"Apply"}}))
        .popupRender([](ryn::DropdownMenuContent menu) {
            ryn::Flex(ryn::FlexProps{}.vertical(true), ryn::FlexContent{[menu] {
                ryn::Text(u8"Custom popup");
                menu(); // 保留默认 Menu 的 closeOnSelect 和事件来源
                ryn::Input(ryn::InputProps{}.defaultValue(u8"Retained draft"));
            }});
        }),
    ryn::DropdownTrigger{[] { ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"Open"); }); }});
```

`DropdownButton(DropdownButtonProps, DropdownButtonSlots)` 以原生 Button + SpaceCompact 组合分离操作；`.button` 和 `.trigger_button` 分别接受完整 ButtonProps，`.content` 与 `.icon` 是 typed slots。主按钮只执行业务动作，右侧按钮保持自己的禁用/loading gate 和回调，再触发 Dropdown。锁定上游已弃用 `Dropdown.Button` Web 组合；此原生 helper 保留对应功能，也可直接组合公开的 Button、SpaceCompact 和 Dropdown。

自定义 popup 内的 Input、Select 和后代 popup 沿用共同 owner/generation、输入边界、捕获、焦点与 IME 生命周期；关闭 owner 后子内容不能继续消费输入，命令/Escape 恢复 trigger，Outside 保留外部控件焦点。Web DOM/CSS/React、跨窗口 OS context menu、系统辅助技术桥接不属于此公开 API；GPU、系统字体与 Windows/Linux 平台验收分别记录。
