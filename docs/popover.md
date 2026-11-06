# Popover

原生 Popover 在操作入口旁显示可交互气泡卡片。title/content 可用字符串或 typed slots；内容中的 Input、Button、Select、DatePicker、TimePicker 和 Table 沿用自身的输入与键盘行为。

可编译并运行验证的例子见 [native_popover.hpp](../examples/native_popover.hpp)：

```cpp
ryn::Popover(
    ryn::PopoverProps{}.title(u8"编辑草稿")
        .trigger(ryn::PopoverTriggerMode::Click),
    ryn::PopoverTrigger{[] {
        ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"打开"); });
    }},
    {.content = ryn::PopoverContent{[] {
        ryn::Input(ryn::InputProps{}.defaultValue(u8"保留的草稿"));
    }}});
```

`PopoverSlots` 提供 `PopoverTitle` 与 `PopoverContent`。`PopoverProps::title/content(Prop<String>)` 是文字便捷入口，同一字段不能同时声明文字和 slot。所有 slots 首次挂载一次；普通开闭、文字、placement 和 Theme 更新保留子组件、编辑草稿和 refs，隐藏内容不获得输入焦点。

`open(Prop<bool>)` 为受控显示，`defaultOpen(bool)` 为非受控初值，二者不能并用。`onOpenChange(bool)` 发送用户请求，受控应用拒绝普通关闭时气泡保持显示。`disabled`、隐藏 owner、窗口失活、Spin 或 Modal 输入边界强制抑制陈旧 open；清理独立于请求通知去重，恢复边界不会自行复活旧弹层。显式 open 更新或新触发可恢复。

Manual 模式下，拒绝一次外部点击关闭后，下一次外部点击仍会交付新的关闭请求；无需先改写 open。关闭回调抛出异常后，原异常保留，后续关闭仍可重试。普通 hover/focus/click 组合触发的去重规则保持。

默认 `trigger(Hover)`，`mouseEnterDelay` 与 `mouseLeaveDelay` 均为100ms。`PopoverTriggerMode` 还提供 Manual、Focus、HoverFocus、Click、ContextMenu；`triggers(PopoverTriggers{hover, focus, click, context_menu})` 可以任意组合，全 false 表示 manual，不能与显式 `trigger` 同用。ContextMenu 要求在同一入口完成 secondary down/up，以该次指针位置作为锚点。延时须非负、有限且不超过一天。指针进入气泡或逻辑子浮层会取消离开；焦点进入富内容时编辑不会因指针离开而中断。Escape 关闭后同一次 hover/focus 不会立即重开。

使用一个原生入口作为 trigger。真实 Button 保留 onClick 和 Enter/Space 激活，不会重复执行；原生禁用/加载 gate 继续生效。非交互 trigger 由 wrapper 提供键盘入口。点击与 hover 打开不抢内容焦点，键盘激活打开后延迟转移到首个可焦点内容。关闭只在实际需要时恢复有效入口；外部点击保留外部控件的焦点和操作。Tab 沿窗口或所属 Modal 的正常顺序移动。编辑器 IME/draft Escape 和子浮层优先于父 Popover。

`PopoverPlacement` 为 Tooltip 的12方位。默认 Top、arrow=true、pointAtCenter=false、autoAdjustOverflow=true。定位复用现有翻转/夹紧与箭头字形；关闭自动调整后允许越界，实际绘制和命中仍裁剪到视口。`direction(Prop<TextDirection>)` 默认 LeftToRight，RightToLeft 镜像左右与角对齐，并传给内置 title/content 文本；富 slot 可使用自身方向 API。

`contentScroll(Prop<bool>)` 默认 true，标题固定，内容使用保留的裁剪视口，按有限宽测量自然高度。子 Table/选择器先处理自己的 wheel；未消费的 wheel 滚动最近 Popover 内容，到边界仍阻止 sibling 穿透。PageUp/PageDown/Home/End 可从非文本编辑器的内容焦点滚动；文本编辑键保持原合同。关闭、重开和 Theme 更新保留草稿、内容身份和合法 scroll offset。禁用内置滚动时沿用有限高度裁剪。外扩焦点装饰遵守有效外部 clip。`LayoutStyle` 只控制 trigger 的外部布局。

`animated(Prop<bool>)` 默认 false 以保留既有调用的即时显示；开启后以 Theme fast duration 做有限淡入淡出。逻辑关闭立即清理内容输入/焦点，退场仅保留绘制。窗口失活、隐藏/离页、禁用及 reduced motion 立即收敛，不留下动画 deadline。

视觉入口为 `ThemeConfig::popover`，与 Tooltip/Popconfirm 独立。支持继承/reset、独立 seed/algorithm，以及颜色、尺寸、字体、阴影、顺序五组失效。普通模式默认 elevated 背景、正文色、strong 标题、title_min_width177、padding12、title_gap8、radiusLG、14/22 字体和 z-index base+30。`max_width=480` 是原生富内容测量上限，可通过 Theme 修改；实际视口更窄时继续约束。其他 token 包括 arrow_size/gap、font_family/font_weight/title_font_weight 和 shadow。内嵌原生组件继续使用自身 Theme。

最终字号须至少0.5，且可安全舍入为原生字体的整数尺寸；不合法的 Theme 会在发布前连同后代 preflight 一起拒绝。Popover 与 Date/Time footer 可双向嵌套，移动或视口变化在同一帧按逻辑祖先顺序定位；焦点留在已打开子 Popover 的 trigger 时，Escape 仍先交给子层。待入/待出的延时热更新会保留方向并从更新时间重算，focus 会立即完成待入。

wireframe seed 派生独立 title/content padding、标题底边与颜色；相应 Theme token 为 `title_padding_inline/top/bottom`、`title_border_width/title_border` 和 `content_padding_inline/block`，全部参与 metrics/colors 失效与诊断。

参考锁定 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 `components/popover/index.tsx`、`PurePanel.tsx` 与 `style/index.ts`。Web 属性透传、DOM/CSS aliases 和 OS 可访问性桥接不由这些原生实现替代。历史结果见 [084 tasks](../openspec/changes/084-20261025-add-native-popover/tasks.md)；107 增补的正式 MSVC Debug/Release 与 D3D12 结果见[Windows 阶段验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/feedback-i18n-windows.md)，目录继续列出尚缺范围。

## Gallery

组件目录的 Popover 直接进入“项目备注”。点击入口编辑并保存，或用 Tab/Enter 完成保存；外部按钮切换四个主方位、箭头和禁用状态，“重新打开”恢复可编辑内容。普通开合、布局和 Theme 更新保留草稿及 Input/按钮身份。091 的 `--overlays-timeline-only` 入口检查真实目录、输入、窗口层裁剪和恢复；实际进展与证据见 [091](../openspec/changes/091-20261026-add-overlay-and-timeline-gallery/tasks.md)，不代替组件专属平台验收。

独立目录页增加“滚动清单 / 点击或右键”、RTL 与线框切换。滚动到末尾可编辑保留草稿并实际保存，入口 identities 为 `gallery.popover.context-trigger`、`gallery.popover.scroll-draft/scroll-save`、`gallery.popover.rtl/wireframe`。
