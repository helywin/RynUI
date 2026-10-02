# Tooltip

`Tooltip` 以 typed `TooltipTrigger` 包装原有组件。提示挂载一次，绘制在窗口浮层，测量只包含触发项；显示提示不改变父布局尺寸，也不夺走键盘焦点。

```cpp
ryn::Tooltip(ryn::TooltipProps{}
    .title(ryn::String{u8"保存当前文档"})
    .placement(ryn::TooltipPlacement::Bottom),
    ryn::TooltipTrigger{[] {
        ryn::Button(ryn::ButtonProps{},
            ryn::ButtonContent{[] { ryn::Text(u8"保存"); }});
    }});
```

默认 `HoverFocus` 在鼠标悬停或后代获得键盘焦点时显示。`Hover`、`Focus` 可单独启用，`Manual` 只使用 open 属性。鼠标进入/离开默认延迟均为 100 ms，通过 `mouseEnterDelay` / `mouseLeaveDelay` 设置 `Duration`，最大一天；键盘焦点立即显示。Escape 关闭并保持触发项焦点；离开全部触发状态后才允许自动再次显示。失活窗口取消等待和自动显示。禁用子按钮仍可通过外层 Tooltip 接收悬停，`TooltipProps::disabled` 则禁用提示本身。

`open(Prop<bool>)` 控制显示，`onOpenChange` 报告请求，等待调用者回写；非受控可用 `defaultOpen(bool)`。二者同时设置会抛出 `std::invalid_argument`。受控 Escape/失焦只请求关闭，调用者须响应回调。空 title、提示 disabled 或窗口失活会隐藏浮层并清除 deadline；恢复后重新评估触发状态。动态 title 和 props 更新不重跑插槽内容。

十二种 `TooltipPlacement` 包含 Top/Bottom/Left/Right 及各自的两种边缘对齐。`autoAdjustOverflow` 默认为 true：先比较主轴溢出并翻转，再限制到窗口边界。`arrow(false)` 隐藏箭头；箭头随锚点位置调整。resize、滚动 translation 与文字变化在同一帧的布局后、文字同步前重新定位；即使触发项位于文档 clip 中，提示使用窗口 clip。

视觉只使用 `ThemeConfig::tooltip`：background、text、max_width、padding_inline/block、min_height、border_radius、arrow_size、gap、shadow、z_index_popup。默认 max_width=250、zIndexPopupBase+70；支持 Default/Dark/Compact、继承、组件 seed/algorithm 和显式 token。纯颜色只更新 material，指标或字号改变才重新测量提示；极小窗口限制 padding、圆角和尺寸。

`Click` 在 trigger 同一后代完成 primary click 后切换提示，保留 Button 激活、focus 和 capture；拖到外部释放不切换。`ContextMenu` 在 trigger 内 secondary release 打开，锚点固定为该次指针位置。窗口外部控件或空白处 primary/secondary down 关闭动作提示；popup 内部不按外部处理。受控连续点击在未回写时仍可请求 true/false。Escape、失焦和 reactive trigger 配置改变清除动作锁存，保持原有子控件交互。

`triggers(Prop<TooltipTriggers>)` 组合 `hover`、`focus`、`click`、`context_menu`（默认 struct 字段均 false，空组合手动），与显式 `trigger()` 互斥。组合中点击关闭后，仍在 hover/focus 时保持关闭，离开全部自动触发状态后再允许自动打开。

富标题使用 `Tooltip(props, TooltipTrigger{...}, TooltipTitle{...})`，与显式 String `title()` 互斥。标题可组合 Text/Icon/Flex，继承提示 foreground/typography，挂载一次；内部 reactive 变化只调整自身浮层尺寸。`titleAvailable(Prop<bool>)` 默认 true，表示富标题是否有可显示内容；为 false 时取消显示和 deadline。关闭保留标题 state，销毁/挂载异常完整清理。

```cpp
ryn::Tooltip(ryn::TooltipProps{}.trigger(ryn::TooltipTriggerMode::Click),
    ryn::TooltipTrigger{[] { ryn::Text(u8"查看说明"); }},
    ryn::TooltipTitle{[] {
        ryn::Flex(ryn::FlexProps{}.vertical(true), ryn::FlexContent{[] {
            ryn::Text(u8"支持组合的原生标题");
            ryn::Icon(ryn::IconProps{}.name(ryn::IconName::CheckOutlined));
        }});
    }});
```

`pointAtCenter(Prop<bool>)` 默认 false：中心 placement 指向 trigger 中心，角 placement 保留固定边缘内距；启用后移动 popup，使固定角箭头对准 trigger 中心。溢出 flip/shift 仍受窗口约束。箭头使用四向私有向量 glyph，经共用 R8 atlas 呈现连续三角形，颜色跟随 Tooltip background；不产生细条 quad。DOM、CSS、portal container 等 Web 专用 API 不移植。基线来自锁定 [Ant Design 6.6.5 Tooltip API](https://github.com/ant-design/ant-design/blob/6.6.5/components/tooltip/shared/sharedProps.en-US.md) 和 [style](https://github.com/ant-design/ant-design/blob/6.6.5/components/tooltip/style/index.ts)。实际阶段与平台验收见 [040 清单](../openspec/changes/040-20261002-add-tooltip-overlay-foundation/tasks.md)、[043 清单](../openspec/changes/043-20261002-complete-native-tooltip-interactions/tasks.md)，已有组件的原生收尾范围见 [组件收尾](component-completion.md)。
