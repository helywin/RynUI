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

本组件是基础文字提示。富内容 title slot、click/context-menu trigger、箭头 pointAtCenter 等扩展尚未提供；DOM、CSS、portal container 等 Web 专用 API 不移植。基线来自锁定 [Ant Design 6.6.5 Tooltip API](https://github.com/ant-design/ant-design/blob/6.6.5/components/tooltip/shared/sharedProps.en-US.md) 和 [style](https://github.com/ant-design/ant-design/blob/6.6.5/components/tooltip/style/index.ts)。实施及各平台实际验收见 [040 清单](../openspec/changes/040-20261002-add-tooltip-overlay-foundation/tasks.md)，现有组件的原生收尾范围见 [组件收尾](component-completion.md)。
