# Spec Delta

## Purpose

为原生设置和详情界面提供固定面板集合的折叠容器，允许用户按需显示正文，同时保留编辑草稿、独立子控件和主题响应，并在隐藏时释放空间及交互资源，形成可验证的首版公共合同。

## ADDED Requirements

### Requirement: 固定面板和受控状态

Collapse SHALL 提供 typed Props 与 title/body/extra slots，拒绝空或重复面板 key 及同时设置 activeKeys/defaultActiveKeys。默认全部关闭；多开模式保留指定已知 key，accordion 按传入顺序只显示第一个已知 key；未知及重复 active key 被忽略。受控模式只发送建议值并等待 owner 更新；disabled 只禁止用户切换，不覆盖显式 active key 或 extra 子控件行为。

#### Scenario: 受控拒绝及 accordion
- **WHEN** 用户请求展开一个面板而受控 owner 不更新值
- **THEN** 展开状态保持原值，回调收到建议 key 集合；accordion 接受新值时至多显示一个正文且可再次关闭

### Requirement: 保留式正文和布局

Collapse MUST 在首次挂载时只执行一次所有 slots，普通 props/Theme/开合变化不得重建它们。隐藏正文 SHALL 不占布局空间，不参与命中、Tab 或文本输入；隐藏时清理焦点、会话、捕获和子浮层，重开保留真实 Input 草稿。

#### Scenario: 草稿与相邻布局
- **WHEN** 编辑正文内 Input 后关闭再展开面板
- **THEN** editor identity 与草稿保持，隐藏期间下方 sibling 上移，输入会话和浮层退出

### Requirement: 原生交互和逻辑边界

可用 header SHALL 支持 pointer 与 Enter/Space，独立 Tab stop 和 focus-visible；Up/Down/Home/End 移动可用 header 焦点而不切换面板。交互 SHALL 遵守 Modal、Spin 和祖先输入门控，extra 的原生子控件操作不得切换面板，回调销毁不得留下资源。

#### Scenario: 阻塞与 extra
- **WHEN** Collapse 被逻辑输入边界阻塞或用户激活 extra 内 Button
- **THEN** 阻塞的 header 不改变状态，extra Button 执行自身动作且不改变展开状态

### Requirement: 主题和首版边界

Collapse SHALL 使用锁定 Ant Design 6.6.5 的尺寸、箭头、标题/正文背景、边框及焦点层级，经组件 Theme tokens 适配 Default/Dark/Compact、继承和覆盖。纯颜色变化 SHALL 仅失效 material，不重新测量或重挂载。公共文档 MUST 明确未实现 motion、动态拓扑、drag、服务、RTL/辅助功能桥接和完整 Web aliases，且不把 headless 结果描述为 GPU/Linux/人工 IME 验收。

#### Scenario: 独立主题更新
- **WHEN** 只覆盖 Collapse 标题背景
- **THEN** scene 原地更新，稳定 slot 与其他组件 Theme 合同保持
