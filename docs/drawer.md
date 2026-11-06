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

typed title/content/footer 默认首次挂载一次，包括初始关闭内容；隐藏停止输入会话、清理子浮层并保留草稿，重开不重跑 slots。没有默认确认/取消按钮，footer slot 可放应用按钮。关闭按钮位于标题起始侧。

107 增补（[Windows 阶段验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/feedback-i18n-windows.md)）：`DrawerSlots::extra` 提供独立保留的 header action；`loading(Prop<bool>)` 用实际 Skeleton 替换正文呈现并取消正文编辑会话，恢复时保留正文。`lazy()` 将首次正文挂载延迟到 open；`destroyOnHidden()` 关闭时销毁正文后代，重开重新执行正文 factory，title/extra/footer 保留。`rtl()` 镜像 header 的 close/title/extra，物理 placement 不变。

`resizable(DrawerResize{true, minimum, maximum, keyboard_step})` 在面板内侧边缘提供 pointer capture 的拖动入口与方向键入口；`onResize(float)` 只请求新的 logical size，应用须写回 width/height，拒绝请求不会改变尺寸。pointer up 发 `onResizeEnd`；取消、隐藏、卸载和禁用立即清理 capture。该合同对应上游 resizable 的拖动，不把停靠 Drawer 改成任意浮动窗口。嵌套 Drawer 默认 `push(DrawerPush{true, 180})`，仅移动其逻辑祖先 Drawer，关闭恢复；可以显式关闭或调整距离。

Drawer 与 Modal 共用同一模态栈，最新打开层是唯一输入边界。Tab/Shift+Tab 在该层循环；打开优先正文，空面板仍有焦点入口。`restoreFocusAfterClose` 默认true，关闭/销毁恢复仍有效触发入口。子 Select/Tooltip 维持所属层级；Escape 先给编辑器组合事务与子浮层。

视觉入口为 `ThemeConfig::drawer`。Default/Dark/Compact、嵌套继承/reset、独立 component seed/algorithm、颜色/metrics/typography/effects/order 五组订阅均独立于 Modal。默认 body24、header16/24、footer8/16、close24/icon16/gap8、分隔线1、标题16/24及600字重、黑色0.45 mask，以锁定 [Ant Design 6.6.5 Drawer 源码](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/drawer/style/index.ts) 为依据。107 将 Theme 的基准向下阴影随 placement 旋转到内侧边缘，blur/spread/color 保持；不复制 CSS。颜色更新不触发测量或内容重建。

尚缺完整开合动画、静态 service、mask blur、平台可访问性桥接及 Web/DOM/CSS 字段。完整退出动画需要与共享 Modal 的可见性、焦点约束和 afterOpenChange 队列共同设计；静态 service 需要有作用域、显式窗口归属及销毁取消的宿主合同，交父线程协调。上述 107 源码和 Gallery 不是验收通过；公开 API 无平台 SDK 类型，CPU scene v3/GPU ABI v2 保持。历史证据见 change062；107 未运行构建、CTest 或真实窗口。
