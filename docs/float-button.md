# FloatButton

`<ryn/float_button.hpp>` 提供 `FloatButton`、`FloatButtonGroup` 和 `FloatButtonBackTop`。设计基线为锁定的 Ant Design 6.6.5；全部输入、布局与绘制使用 RynUI retained logical scene，无 SDL 或 renderer 专属入口。

`FloatButtonProps` 支持 reactive `type`、`shape`、`disabled`、`tooltip`（String 或 typed TooltipProps）、`anchor`、`draggable`、`floating`，以及 `badge`、`onClick` 和外部 `layout`。`FloatButtonSlots` 提供 typed `icon`、`content`，图标与内容纵向组合。默认图标为 AppstoreOutlined。禁用状态沿用 Button 的键盘/指针门禁与主题颜色。圆形使用直径，方形使用组件圆角 token。

默认 `floating(true)` 将组件放入窗口层，不占正文布局空间；四角 `anchor` 使用窗口逻辑尺寸及组件 inset，窗口变小时夹取到可用范围。`draggable(true)` 从操作按钮开始拖动，保留 Button 的 PointerRouter 捕获，超过 4 logical px 后抑制此次点击。松开、取消、禁用、关闭窗口输入、隐藏祖先或销毁会释放捕获。`floating(false)` 可将同一组件用于示例区或自定义原生容器。

`FloatButtonGroupProps` 支持 `None` 常驻、`Click`、`Hover` 模式和四向 `placement`。Group 的 `shape`、`disabled` 传递给成员，`type` 控制触发器。`icon`/`closeIcon` 定制触发器；成员通过 `FloatButtonGroupContent` 挂载。`open` 与 `defaultOpen` 互斥；受控模式只发出 `onOpenChange` 请求，由调用方更新 `open`。外部按下和 Escape 请求关闭，按钮保留 Tab/Enter/Space 操作。隐藏菜单仍保留内容身份，但不接收输入；`onClick` 处理菜单触发器激活。

BackTop 的 `scroll` 是 `Prop<FloatButtonScrollState>`：`offset`、`extent`、`viewport` 均为非负有限逻辑长度。`onScrollTo(float)` 是必需的显式原生目标回调；调用方执行实际滚动，并把目标状态写回 `scroll`。`visibilityHeight` 默认 400，`duration` 默认 450 ms，`showProgress` 默认 false；`button` 和可选 `FloatButtonSlots` 配置共同按钮能力。进度为 `clamp(offset / (extent - viewport), 0, 1)`，无可滚动区时为零。返回动画使用宿主时钟，穿过显示阈值后仍继续到零；reduced motion 或零时长直接发送零。禁用、窗口失活、祖先隐藏/销毁取消后续回调，完成后不持续请求帧。

视觉通过 `ThemeConfig.float_button.tokens`：`size`、`icon_size`、`content_font_size`、`border_radius`、`gap`、`inset_inline`、`inset_block`、`background`、`primary_background`、`color`、`primary_color`、`shadow`。尺寸、间距、颜色与阴影默认从当前 Theme 派生（包含 Compact），局部 Theme 可覆盖并反应更新。`LayoutStyle` 只表达外部布局。

Gallery canonical 页 `ant.component.float-button` 包含实际窗口浮动入口、四向/受控/悬停分组，以及滑块与 BackTop 共用状态的真实裁剪内容视口。DOM、CSS class/style、React、href/target 和 HTML 表单行为不属于原生 API。平台验收边界见 change106 的精简证据。

官方实现参照：[6.6.5 FloatButton tokens](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/float-button/style/index.ts)、[按钮样式](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/float-button/style/button.ts)、[分组样式](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/float-button/style/group.ts)。
