# Spec Delta

## Purpose

为桌面危险操作提供靠近触发入口的二次确认，使用保留式原生组件表达标题、说明和真实确认按钮，并使响应式状态、键盘、焦点、模态边界和窗口定位具有可验证的一致行为。

## ADDED Requirements

### Requirement: 原生声明与显示状态

Popconfirm SHALL 提供 typed Props 和 trigger/title/description slots，默认关闭且点击触发；open 受控或 defaultOpen 非受控二选一。disabled SHALL 阻止打开，showCancel SHALL 控制取消按钮。普通属性更新 MUST 不重跑 slots。

#### Scenario: 受控请求
- **WHEN** 用户打开或关闭受控 Popconfirm 而应用不回写 open
- **THEN** onOpenChange 收到请求且外部值不被修改，普通关闭请求保持外部可见状态

### Requirement: 确认与取消

Popconfirm SHALL 使用真实 Button，支持 okText/cancelText、onConfirm/onCancel、confirmLoading。确认后请求关闭；确认回调同步设置 loading 时保留显示。loading SHALL 阻止重复确认但保留取消。

#### Scenario: 加载确认
- **WHEN** 确认回调设置 confirmLoading=true
- **THEN** 弹层继续显示且重复确认无效，取消仍可关闭

### Requirement: 输入与生命周期

Popconfirm SHALL 支持 Enter/Space 触发、Escape 和外部按下关闭；内部和逻辑后代 popup 点击不关闭父层。打开 SHALL 转移焦点至确认按钮，关闭 SHALL 恢复有效触发入口；外部点击允许焦点移往外部入口。Modal 边界、隐藏 owner、失活和销毁 MUST 清理子层输入并抑制陈旧受控弹层复现。

#### Scenario: 模态边界切换
- **WHEN** 外部受控弹层打开时出现 Modal，随后 Modal 关闭
- **THEN** Popconfirm 发送一次关闭请求且持续隐藏，直到显式 open 更新或重新触发，不自动复现

#### Scenario: 子弹层
- **WHEN** Popconfirm 说明内打开 Select 并按 Escape
- **THEN** 先关闭 Select，父 Popconfirm 及 Modal 继续显示

### Requirement: 定位与主题

Popconfirm SHALL 支持 12 placements、箭头、中心指向、自动翻转和窗口夹紧，子层可越过正文裁剪且保持 Modal 层级。视觉 MUST 通过 Theme/Component Token 控制，LayoutStyle 仅控制触发区域外部布局；不得修改 scene ABI。

#### Scenario: 窗口边缘与主题更新
- **WHEN** 触发入口靠近窗口边缘或颜色 Token 更新
- **THEN** 弹层按可用空间调整，颜色更新不重新测量或执行 slots
