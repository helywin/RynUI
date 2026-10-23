# Modal

原生Modal提供保留式对话框、遮罩、受控取消/确认、模态输入边界与焦点恢复。Gallery和原生平台验收按061的tasks独立记录。

```cpp
ryn::Signal<bool> open{false};
ryn::Modal(ryn::ModalProps{}.open(open).title(u8"编辑账户")
    .onOk([open]() mutable { open.set(false); })
    .onCancel([open](ryn::ModalCancelReason) mutable { open.set(false); }),
    ryn::ModalContent{[] { ryn::Input(ryn::InputProps{}.defaultValue(u8"草稿")); }});
```

open默认false且由应用持有；OK/取消只发送请求，应用不回写false就保持显示。ModalCancelReason区分CancelButton、CloseButton、Mask和Escape。confirmLoading阻止重复OK，不阻断其余允许的取消入口。closable、keyboard和typed ModalMask分别控制关闭按钮、Escape与遮罩外部取消；无遮罩颜色仍保持模态交互。

width为正且有限的logical units，默认520；centered控制垂直居中。默认页脚OK/Cancel可改文字，footer(None)隐藏默认按钮，ModalSlots的typed title/footer支持自定义内容。restoreFocusAfterClose默认true。afterOpenChange在实际显示转换后通知，afterClose只在实际隐藏后通知，初始隐藏不产生关闭事件。

首次挂载一次预建并保留全部slots，包括初始关闭时的内容。隐藏时清理焦点、输入和子弹层，再开保留编辑器、草稿和组件身份；初始隐藏内容的autoFocus被阻止。显示转换通知在后续窗口同步的安全阶段发出，初始关闭不通知。回调可以重开或卸载；已卸载owner不再接收后续通知。

Modal脱离父级Box/Flex/Space和顶层正文流，不占空间或gap，保留逻辑owner和Theme。默认顶部距离100，宽度与高度按窗口安全边距夹紧；centered启用垂直居中。正文可滚动，标题与页脚固定；Tab焦点及超高TextArea的插入点会显露，滚动只更新像素对齐的平移和裁剪。正文内的TextArea、Select菜单和Tabs标签栏先处理自己的滚轮。

最新打开的Modal是唯一输入边界。Tab/Shift+Tab在其中环绕，无入口时停留在对话框；打开优先正文入口，其次默认OK和其他入口。关闭恢复仍有效、可见且未禁用的触发入口，嵌套关闭恢复父级；可用restoreFocusAfterClose关闭自动恢复。背景仍绘制并保留自身disabled值，无遮罩颜色也不会透传输入。Escape先处理编辑器组合事务，再处理Select/Tooltip，最后请求取消Modal。子浮层可越过正文和对话框边缘，仍夹紧在窗口内。

动态销毁/懒加载、动画、静态confirm/Promise自动关闭、拖动/resize、mask blur、RTL、响应断点宽度、平台可访问性桥接和Web/DOM API不在首版。实际状态以061的tasks与evidence为准。

视觉入口为`ThemeConfig::modal`，支持颜色、11个布局尺寸、正文/标题字体、focus/阴影及z-index，继承/reset、独立component seed/algorithm和显式覆盖。颜色/布局/字体/效果/层级分五组失效。默认elevated背景、黑色0.45遮罩、正文14/22、标题16/24及600字重、20垂直/24水平padding、header gap8/footer gap12、圆角8、close32；基于锁定Ant Design6.6.5。纯颜色变化不测量或重跑正文。活跃Modal组共享其中最高z-index作为全局层级base，打开顺序决定组间前后；子浮层在所属组内按自己的z-index排序，始终位于其对话框之上。
