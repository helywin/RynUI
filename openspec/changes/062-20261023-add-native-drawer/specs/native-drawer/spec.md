# Spec Delta

## Purpose

为原生桌面详情和编辑流程提供从窗口四边停靠的受控 Drawer，保留正文状态并隔离背景输入，使嵌套表单与对话框能够可靠地恢复焦点和释放输入会话。

## ADDED Requirements

### Requirement: Controlled retained panel
Drawer SHALL 提供 typed Props 和 title/content/footer slots，响应式 open、title、placement、width、height、closable、keyboard、mask 和 restoreFocusAfterClose。关闭请求 SHALL 由应用决定是否回写 open；隐藏及再次打开 MUST 保留内容身份和草稿。

#### Scenario: Refused close and reopen
- **WHEN** 应用收到 CloseButton、Mask 或 Escape 取消请求而未关闭，随后显式关闭再打开
- **THEN** 面板在拒绝时保持显示，再打开后正文不重新执行，草稿与组件身份保持

#### Scenario: Lifecycle callbacks
- **WHEN** 面板实际显示或隐藏，或回调重开及销毁 owner
- **THEN** afterOpenChange 通知实际转换，afterClose 只通知实际隐藏，初始关闭不通知，销毁后不调用迟到回调

### Requirement: Edge geometry and fixed regions
Drawer SHALL 支持 Top、Bottom、Left、Right，Left/Right 使用 width 且填满窗口高度，Top/Bottom 使用 height 且填满宽度；默认尺寸为 378 logical units。尺寸 MUST 正且有限，面板 SHALL 夹紧在窗口，header/footer 固定，超高 body 可滚动且裁剪。

#### Scenario: Resize and scroll
- **WHEN** 四种 placement 的窗口缩小或滚动超高正文
- **THEN** 面板停靠指定边缘且不越界，只有正文平移，header/footer 保持原位

### Requirement: Shared modal input and focus
Drawer 和 Modal SHALL 共享按实际打开顺序选择的唯一输入边界，阻止背景 pointer、focus、输入与滚轮；Tab 在当前层内循环。关闭或销毁顶层 SHALL 恢复仍有效的触发入口，嵌套子浮层保持所属层级。

#### Scenario: Mixed nested overlays
- **WHEN** Drawer 内打开 Modal，或 Modal 内打开 Drawer，再关闭及销毁顶层
- **THEN** 只有最新层可接受输入，关闭恢复父层，父层关闭恢复外部入口，旧输入会话不能再提交

### Requirement: Theme and precise updates
Drawer SHALL 使用独立 Component Token，继承 Default/Dark/Compact 及嵌套 Theme，颜色、metrics、typography、effects、order 分组失效。普通属性及主题更新 MUST 不重新执行 slots；纯颜色更新 MUST 不触发测量，不改变 CPU scene v3/GPU ABI v2。

#### Scenario: Local color update
- **WHEN** 只改变 Drawer 背景颜色
- **THEN** 只更新 Drawer 材质，Modal token 与无关组件保持，布局代际和内容执行数保持
