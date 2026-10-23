# Popconfirm

原生 Popconfirm 提供靠近操作入口的二次确认，使用保留式标题/说明和真实小尺寸 Button。默认关闭、点击触发、顶部定位；也支持 Enter/Space 触发与 Escape 关闭。

```cpp
ryn::Popconfirm(
    ryn::PopconfirmProps{}.title(u8"删除这份草稿？")
        .description(u8"此操作无法撤销。")
        .okText(u8"删除").cancelText(u8"取消")
        .onConfirm([] { delete_draft(); }),
    ryn::PopconfirmTrigger{[] {
        ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"删除"); });
    }});
```

`open(Prop<bool>)` 为受控显示；`defaultOpen(bool)` 为非受控初值，两者不能并用。`onOpenChange(bool)` 表达用户请求；受控应用不回写 open 时，普通关闭请求保持显示。禁用、窗口失活、隐藏 owner 或移出 Modal 输入边界会强制隐藏并抑制陈旧 open，直到显式 open 更新或再次触发；去重不会跳过强制清理。

`disabled` 阻止打开并关闭已显示弹层。`showCancel` 默认 true；`confirmLoading` 阻止重复确认，保留取消能力。`onConfirm` 执行后默认请求关闭；如回调同步设置 `confirmLoading=true`，则保留弹层。异步工作推荐受控 open，由应用设置 loading 并在完成后关闭。`onCancel` 只由取消按钮调用，Escape/外部按下只发送关闭请求。

`PopconfirmSlots` 提供 typed `PopconfirmTitle` 和 `PopconfirmDescription`。同一内容不能同时通过文字 Prop 与 slot 声明。全部 slots 首次挂载一次，关闭时保留组件身份和编辑状态；初始关闭的内容不自动获得焦点。普通属性和 Theme 变化不重新执行 slots。使用一个明确的操作入口作为 trigger；已有 Button 等入口沿用自身焦点，无交互内容则由 wrapper 提供键盘入口。

打开后焦点移至确认按钮，关闭恢复有效触发入口；外部点击保持外部入口获得的焦点。弹层保持非模态，Tab 沿窗口或所属 Modal 的正常顺序移动。说明内可以放置 Select、Input 或 Tooltip；子浮层的逻辑归属保留，点击子菜单不关闭父层，Escape 优先由编辑器/子 Select/Tooltip 处理。关闭父层会清理子弹层输入。Modal 外部弹层不能越过其输入边界。

`PopconfirmPlacement` 使用 Tooltip 的12方位枚举；`arrow` 默认 true，`pointAtCenter` 默认 false，`autoAdjustOverflow` 默认 true。定位复用原生 Tooltip 算法，按窗口翻转和夹紧，弹层脱离正文布局并允许越过父级内容裁剪。`LayoutStyle` 只控制触发区域的外部布局。

视觉入口为 `ThemeConfig::popconfirm`，支持继承/reset、独立 seed/algorithm、颜色/尺寸/字体/阴影/顺序五组失效。默认 elevated 背景、警告图标、padding12、消息与按钮 gap8、说明 gap4、圆角8、14/22 字体和 z-index1060。按钮继续使用 Button Theme Token。`max_width=360` 是原生富内容的测量上限；窗口更窄时继续夹紧，超高内容裁剪，首版不提供内部滚动。

设计核对锁定 [Ant Design 6.6.5 Popconfirm](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/popconfirm/index.tsx)、[Popconfirm style](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/popconfirm/style/index.ts) 和 [Popover style](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/popover/style/index.ts)。本次仅点击/键盘触发；Promise、静态服务、动画、RTL、hover/contextMenu 触发、任意 ButtonProps 透传、平台可访问性桥接不在首版。实际验收见 [064 tasks](../openspec/changes/064-20261023-add-native-popconfirm/tasks.md)，平台原生项不能由 fixture 结果代替。
