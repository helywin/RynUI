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

默认 `trigger(Hover)`，`mouseEnterDelay` 与 `mouseLeaveDelay` 均为100ms。`PopoverTriggerMode` 还提供 Manual、Focus、HoverFocus、Click；`triggers(PopoverTriggers{hover, focus, click})` 可以任意组合，三个值均 false 表示 manual，不能与显式 `trigger` 同用。延时须非负、有限且不超过一天。指针进入气泡或逻辑子浮层会取消离开；焦点进入富内容时编辑不会因指针离开而中断。Escape 关闭后同一次 hover/focus 不会立即重开。

使用一个原生入口作为 trigger。真实 Button 保留 onClick 和 Enter/Space 激活，不会重复执行；原生禁用/加载 gate 继续生效。非交互 trigger 由 wrapper 提供键盘入口。点击与 hover 打开不抢内容焦点，键盘激活打开后延迟转移到首个可焦点内容。关闭只在实际需要时恢复有效入口；外部点击保留外部控件的焦点和操作。Tab 沿窗口或所属 Modal 的正常顺序移动。编辑器 IME/draft Escape 和子浮层优先于父 Popover。

`PopoverPlacement` 为 Tooltip 的12方位。默认 Top、arrow=true、pointAtCenter=false、autoAdjustOverflow=true。定位复用现有翻转/夹紧与箭头字形；关闭自动调整后允许越界，实际绘制和命中仍裁剪到视口。内容超过有限视口时裁剪，首版不提供内部滚动容器；内容 Table/选择器的滚动继续有效，未处理 wheel 在最近 popup 边界截止，覆盖的 sibling 不接收穿透输入。外扩焦点装饰遵守有效外部 clip。`LayoutStyle` 只控制 trigger 的外部布局。

视觉入口为 `ThemeConfig::popover`，与 Tooltip/Popconfirm 独立。支持继承/reset、独立 seed/algorithm，以及颜色、尺寸、字体、阴影、顺序五组失效。普通模式默认 elevated 背景、正文色、strong 标题、title_min_width177、padding12、title_gap8、radiusLG、14/22 字体和 z-index base+30。`max_width=480` 是原生富内容测量上限，可通过 Theme 修改；实际视口更窄时继续约束。其他 token 包括 arrow_size/gap、font_family/font_weight/title_font_weight 和 shadow。内嵌原生组件继续使用自身 Theme。

参考本地锁定 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 `components/popover/index.tsx`、`PurePanel.tsx` 与 `style/index.ts`。首版排除 Web 属性透传、contextMenu、动画、RTL、wireframe 特例和 OS 可访问性桥接。验收见 [084 tasks](../openspec/changes/084-20261025-add-native-popover/tasks.md)；headless 证据不代表 native/GPU/Linux/system IME/AT 验收。
