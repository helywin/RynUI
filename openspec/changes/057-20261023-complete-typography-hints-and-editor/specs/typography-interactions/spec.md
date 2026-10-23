# Spec Delta

## Purpose

为原生桌面 Typography 提供可发现的操作提示、与实际截断状态一致的全文提示和保留文本/字体/焦点的多行编辑体验，使复制、阅读与编辑在响应式更新及生命周期变化后仍具有一致行为。

## ADDED Requirements

### Requirement: 操作提示

Typography SHALL 在可用的复制与编辑入口上提供 hover 和键盘焦点提示，复制成功后显示成功文案，失败不显示成功；提示文案 MUST 支持响应式更新和空文案关闭。

#### Scenario: 复制反馈和配置变化
- **WHEN** 用户聚焦复制入口并成功复制，随后反馈到期或修改提示文案
- **THEN** 提示显示当前复制状态对应的文案，反馈到期恢复普通文案，空文案关闭提示且不阻止复制

#### Scenario: 失效与清理
- **WHEN** 入口隐藏、禁用、进入编辑、窗口失活或组件销毁
- **THEN** 对应提示关闭并清理等待状态，既有操作、焦点顺序和兄弟组件保持可用

### Requirement: 截断提示

Typography SHALL 只在实际截断、未展开、非编辑态且配置了非空省略提示时显示文本提示；宽度、内容、展开与配置更新 MUST 使提示状态与当前截断结果一致。

#### Scenario: 宽度和展开变化
- **WHEN** 已截断文本的提示打开后增加可用宽度或展开全文
- **THEN** 提示关闭；再次收窄或收起后可重新通过 hover 打开，操作入口不误触发文本提示

### Requirement: 保留多行编辑

Typography SHALL 使用保留的多行编辑子树，支持换行内容、自动行高及 typed 最小/最大行数；Enter 提交，Shift+Enter 换行，Escape 优先取消 IME 组合再取消草稿，blur 保持既有提交行为。受控值未接受回写 MUST 保留草稿。

#### Scenario: 多行编辑提交与取消
- **WHEN** 用户编辑含换行的段落、使用 Shift+Enter 换行后按 Enter 提交，或按 Escape 取消
- **THEN** 提交保留完整换行文本，取消恢复原文，结束后回到编辑入口且组件 identity 不变

#### Scenario: 配置和输入生命周期
- **WHEN** 编辑中更新 autoSize、maxLength、主题、方向或禁用状态
- **THEN** 编辑器复用既有 Unicode、IME、剪贴板和字体服务，按配置更新且不重新挂载，禁用和销毁释放活动会话
