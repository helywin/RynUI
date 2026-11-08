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

108 默认文案已接入：默认页脚读取最近活 ConfigProvider 的
`common.confirm/cancel`，无 Provider 仍为 OK/Cancel。`locale(Prop<CommonLocale>)` 是完整组件覆盖；
`okText/cancelText` 的显式英文或空串高于完整 Locale，完整 Locale 的英文/空字段高于作用域包。
typed footer/title 与业务 body String 保留原意。语言切换只更新默认按钮标签与必要测量，保留打开状态、
富 Input 身份、草稿、反向选区、preedit、undo 与焦点，不发送 onOk/onCancel/afterOpenChange。
CPU host 入口为 `rynui.i18n_components_defaults` / `rynui.i18n_components_overrides`；
实际 Debug/Release 结果见 [108 组件阶段](../openspec/changes/108-20261107-add-native-i18n/evidence/components-windows.md)，不替代原生平台验收。

首次挂载一次预建并保留全部slots，包括初始关闭时的内容。隐藏时清理焦点、输入和子弹层，再开保留编辑器、草稿和组件身份；初始隐藏内容的autoFocus被阻止。显示转换通知在后续窗口同步的安全阶段发出，初始关闭不通知。回调可以重开或卸载；已卸载owner不再接收后续通知。

Modal脱离父级Box/Flex/Space和顶层正文流，不占空间或gap，保留逻辑owner和Theme。默认顶部距离100，宽度与高度按窗口安全边距夹紧；centered启用垂直居中。正文可滚动，标题与页脚固定；Tab焦点及超高TextArea的插入点会显露，滚动只更新像素对齐的平移和裁剪。正文内的TextArea、Select菜单和Tabs标签栏先处理自己的滚轮。

默认最新打开的Modal是唯一输入边界。`focusTrap(false)` 可释放本层的焦点限制，但不会解除仍打开的其他 Modal 所需的输入限制；嵌套层继承底层边界，无共同模态祖先的兄弟层收口到顶层。动态切换和销毁后重新解析存活栈。Tab/Shift+Tab在其中环绕，无入口时停留在对话框；打开优先正文入口，其次默认OK和其他入口。关闭恢复仍有效、可见且未禁用的触发入口，嵌套关闭恢复父级；可用restoreFocusAfterClose关闭自动恢复。背景仍绘制并保留自身disabled值，无遮罩颜色也不会透传输入。Escape先处理编辑器组合事务，再处理Select/Tooltip，最后请求取消Modal。子浮层可越过正文和对话框边缘，仍夹紧在窗口内。

`onOkAsync(ConfirmAction)`、`onActionError` 与 `showCancel` 提供异步确认。异步 Close 只调用原 onOk 请求；受控方拒绝回写时保留打开状态和草稿。KeepOpen/错误恢复 loading，可重试；pending 阻止普通取消和重复确认。关闭/隐藏/owner 退役使动作失效。`AppRef::modal()` 的真实动态服务、结果与调度合同及实际 Debug/Release 证据见 [App 服务](app.md)。完整 lazy/motion、拖动/resize、RTL、响应断点宽度、平台可访问性桥接和 Web/DOM API 继续独立跟踪。

视觉入口为`ThemeConfig::modal`，支持颜色、11个布局尺寸、正文/标题字体、focus/阴影及z-index，继承/reset、独立component seed/algorithm和显式覆盖。颜色/布局/字体/效果/层级分五组失效。默认elevated背景、黑色0.45遮罩、正文14/22、标题16/24及600字重、20垂直/24水平padding、header gap8/footer gap12、圆角8、close32；基于锁定Ant Design6.6.5。纯颜色变化不测量或重跑正文。活跃Modal组共享其中最高z-index作为全局层级base，打开顺序决定组间前后；子浮层在所属组内按自己的z-index排序，始终位于其对话框之上。

## 背景模糊

`ModalMask{enabled, closable, blur}` 在原两字段后追加默认 false 的 blur。enabled 且 blur 开启时，对当前窗口的真实前序 framebuffer 进行两遍离散 Gaussian；不包含当前 dialog 或后层。服务入口消费同一 ModalProps。关闭 blur 保留旧 tint 路径，关闭 mask 不创建滤波请求；输入、关闭 future 与 GPU frame lease 保持各自职责。

`ModalEffectToken::mask_blur_sigma` 是 native 派生 Theme 字段，默认 4 logical units，override 为有限非负 LogicalLength。它不是冻结上游 catalog 中的 token。RGBA8 PM 中间结果逐 pass 量化；源与目标分离，窗口外采样 clamp-to-edge。最终关闭硬件 blending，使用 `(1-m)*B+m*(T over F)` replacement；不把 F 再 source-over 到 B，透明背景的 alpha 不会因此从 .5 变为 .75。

新场景在共同上传 begin 前检查版本、能力、kernel 和实际容量预算；失败失效整个附件，并可在没有新 dirty 时重试。所有实际提交帧重新采样前序内容。Windows hidden D3D12 与 Linux private Wayland/Vulkan 的 GPU 结果分别验收；source closure 和 Recording 不代表实际 GPU 通过。

背景模糊已通过正式 MSVC HEADLESS Debug/Release 和 Windows D3D12/DXIL Debug/Release 的独立 RGBA 回读，包含真实 App 服务、透明背景、halo 和多层前序内容；见 [共同合成证据](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/composition-native.md)。
