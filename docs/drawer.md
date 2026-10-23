# Drawer

原生 Drawer 提供四边停靠面板、受控取消、保留内容和真实模态输入边界。首版静态呈现；Token Gallery 的 LiveSamples 末尾可打开四边示例、编辑保留草稿并在抽屉内使用 Popconfirm。Gallery Recording 与真实平台验收独立记录。

```cpp
ryn::Signal<bool> open{false};
ryn::Drawer(ryn::DrawerProps{}.open(open).title(u8"编辑账户")
    .placement(ryn::DrawerPlacement::Right).width(420.0F)
    .onCancel([open](ryn::DrawerCancelReason) mutable { open.set(false); }),
    ryn::DrawerContent{[] { ryn::Input(ryn::InputProps{}.defaultValue(u8"草稿")); }},
    ryn::DrawerSlots{.footer = ryn::DrawerFooter{[open]() mutable {
        ryn::Button(ryn::ButtonProps{}.onClick([open]() mutable { open.set(false); }),
                    ryn::ButtonContent{[] { ryn::Text(u8"完成"); }});
    }}});
```

`open` 默认 false，由应用持有；`onCancel` 区分 CloseButton、Mask、Escape，只发请求。`closable`、`keyboard` 和 `DrawerMask{enabled, closable}` 分别控制入口；透明 mask 仍保持模态边界。`afterOpenChange` 在实际转换后的安全同步阶段通知，`afterClose` 只在隐藏时通知，初始关闭不通知；回调允许重开和销毁 owner。

`placement` 默认 Right，Left/Right 使用 `width` 且填满窗口高度，Top/Bottom 使用 `height` 且填满宽度。width/height 均默认378，必须正且有限，按窗口可用区夹紧。面板脱离父布局，不占据 Flex/Space gap；保留逻辑 owner、Theme 和生命周期。header/footer 固定，body 填满剩余空间并裁剪、滚动，键盘焦点自动显露。

typed title/content/footer 首次挂载一次，包括初始关闭内容；隐藏停止输入会话、清理子浮层并保留草稿，重开不重跑 slots。没有默认确认/取消按钮，footer slot 可放应用按钮。关闭按钮位于标题起始侧。

Drawer 与 Modal 共用同一模态栈，最新打开层是唯一输入边界。Tab/Shift+Tab 在该层循环；打开优先正文，空面板仍有焦点入口。`restoreFocusAfterClose` 默认true，关闭/销毁恢复仍有效触发入口。子 Select/Tooltip 维持所属层级；Escape 先给编辑器组合事务与子浮层。

视觉入口为 `ThemeConfig::drawer`。Default/Dark/Compact、嵌套继承/reset、独立 component seed/algorithm、颜色/metrics/typography/effects/order 五组订阅均独立于 Modal。默认 body24、header16/24、footer8/16、close24/icon16/gap8、分隔线1、标题16/24及600字重、黑色0.45 mask，以锁定 [Ant Design 6.6.5 Drawer 源码](https://github.com/ant-design/ant-design/blob/6.6.5/components/drawer/style/index.ts) 为依据。阴影首版复用全局 elevated `box_shadow`，可显式覆盖；尚未映射上游四向专用阴影。颜色更新不触发测量或内容重建。

首版不包含 loading/Skeleton、动画、嵌套 push 位移、静态 service、extra slot、拖动/resize、动态销毁/懒加载、mask blur、RTL、平台可访问性桥接或 DOM/CSS 字段。公开 API 无平台 SDK 类型；CPU scene v3/GPU ABI v2 保持不变。实际证据见 change062；未宣称原生 Linux/Wayland、人工系统 IME 或真实窗口视觉验收。
