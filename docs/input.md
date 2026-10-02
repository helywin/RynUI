# Input 原生桌面 API

Input、Password、Search 使用共用 typed Props 和长期编辑器。单行编辑已支持受控/非受控值、Unicode scalar 硬 maxLength、grapheme 选择/移动、剪贴板、undo/redo 与平台组合输入。视觉来自 Theme；LayoutStyle 只控制外部布局。

## 焦点引用与系统输入提示

```cpp
ryn::InputRef input;
ryn::Input(ryn::InputProps{}.ref(input)
               .autoFocus()
               .purpose(ryn::InputPurpose::Name)
               .capitalization(ryn::InputCapitalization::Words)
               .autocorrect(true)
               .onFocus([] {})
               .onBlur([] {}));
// 在挂载及首次布局之后使用。
input.focus({ryn::InputFocusCursor::All});
input.blur();
```

InputRef 可以复制共享同一个绑定，bound 在挂载前/卸载后为 false；focus/blur/select 无合法绑定时返回 false。一个引用不能同时绑定两个存活输入框；引用必须在创建线程使用。focus 的 Keep/Start/End/All 指定光标行为；select(anchor, caret) 使用 UTF-8 byte 偏移，必须均为完整 grapheme 边界，非法偏移返回 false。

autoFocus 只在首次布局请求一次，初始 disabled/不活跃分支不会在之后恢复时抢焦点。readOnly 仍允许焦点与选择。onFocus/onBlur 只通知实际焦点转换；可以在回调中请求另一引用焦点，转移在当前焦点事务结束后执行并检查身份/资格。同步卸载会解除绑定。

purpose 支持 Text、Name、Email、Username、Number；capitalization 支持 None、Sentences、Words、Letters，autocorrect 默认 true。这些是原生平台提示，不执行数值验证、大小写改写或 HTML 表单行为。Password 强制密码用途并关闭 autocorrect。reactive 提示变更在没有 IME preedit 时刷新会话，正在组合输入则延后至提交/取消；旧会话 stamp 不能继续提交。

系统输入端口是否提供对应键盘/纠正 UI 取决于实际平台；端口合同测试不能替代真实 OS 输入证据。

## 变体与主题

三个单行组件的 `.variant(Prop<InputVariant>)` 支持 Outlined、Filled、Borderless、Underlined，默认 Outlined。三个尺寸保持等高；Underlined 只画底边，Borderless 在键盘焦点时显示 outline。Filled 使用独立背景与边框，半透明背景不会重复混色。disabled 与显式 Error/Warning 状态通过同一 Theme 推导。

InputTokenOverride 可覆盖 Filled 普通/hover 背景、Error/Warning 背景和文字色以及 focus_width。颜色更新只修改 retained material；变体更新保持 editor、选择、IME 会话、scene slot 与内容挂载身份。Compact 在两侧都有边框时合并接缝，无边框变体不产生虚假接缝。

Password 透传变体。Search 使用 Compact 连接输入框与 action，非 Outlined 变体使用 Text action；Filled action 背景和 Underlined action 底边在操作阶段继续补齐。

## 收尾进度

052 正在补单行家族的变体、统计及操作配置，任务和验收记录位于对应 change。TextArea、OTP 和 RTL/混合文字视觉导航仍属于下一阶段原生收尾范围，整个 Input 家族尚未标为完成。DOM/CSS/React 和 HTML 自动填充 API 不移植。
