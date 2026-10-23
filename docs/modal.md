# Modal

061正在实现原生Modal。当前提供typed API声明、受控生命周期/取消模型与Modal Token；模态输入边界、实际对话框、Gallery和平台验收尚在实施。API声明不代表可挂载组件已完成。

```cpp
ryn::Signal<bool> open{false};
ryn::Modal(ryn::ModalProps{}.open(open).title(u8"编辑账户")
    .onOk([open]() mutable { open.set(false); })
    .onCancel([open](ryn::ModalCancelReason) mutable { open.set(false); }),
    ryn::ModalContent{[] { ryn::Input(ryn::InputProps{}.defaultValue(u8"草稿")); }});
```

open默认false且由应用持有；OK/取消只发送请求，应用不回写false就保持显示。ModalCancelReason区分CancelButton、CloseButton、Mask和Escape。confirmLoading阻止重复OK，不阻断其余允许的取消入口。closable、keyboard和typed ModalMask分别控制关闭按钮、Escape与遮罩外部取消；无遮罩颜色仍保持模态交互。

width为正且有限的logical units，默认520；centered控制垂直居中。默认页脚OK/Cancel可改文字，footer(None)隐藏默认按钮，ModalSlots的typed title/footer支持自定义内容。restoreFocusAfterClose默认true。afterOpenChange在实际显示转换后通知，afterClose只在实际隐藏后通知，初始隐藏不产生关闭事件。

规划的首版一次预建并保留全部slots，隐藏时清理焦点、输入和弹层，再开保留草稿。动态销毁/懒加载、动画、静态confirm/Promise自动关闭、拖动/resize、mask blur、RTL、响应断点宽度、平台可访问性桥接和Web/DOM API不在首版。实际状态以061的tasks与evidence为准。

视觉入口为`ThemeConfig::modal`，支持颜色、11个布局尺寸、正文/标题字体、focus/阴影及z-index，继承/reset、独立component seed/algorithm和显式覆盖。颜色/布局/字体/效果/层级分五组失效。默认elevated背景、黑色0.45遮罩、正文14/22、标题16/24及600字重、20垂直/24水平padding、header gap8/footer gap12、圆角8、close32；基于锁定Ant Design6.6.5。纯颜色域只通知材料更新，真实对话框的布局局部性由后续组件验收确认。
