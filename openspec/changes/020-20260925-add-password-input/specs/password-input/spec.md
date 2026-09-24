# Spec Delta

## Purpose

为 RynUI 提供可复用现有单行编辑服务的密码输入控件，使应用能够用类型化属性和主题化外观收集密码，同时保持遮罩、光标、输入法、可见性与剪贴板行为一致。

## ADDED Requirements

### Requirement: 密码值与遮罩
Password SHALL 支持受控 `value` 或 `defaultValue`，二者同时提供时 MUST 在挂载前拒绝。默认隐藏时，展示文本 SHALL 对每个 Unicode 字素显示一个遮罩符号，包含组合输入；文本场景不得含原文，编辑与 `onChange` SHALL 使用原文。

#### Scenario: 字素映射
- **WHEN** 密码包含多字节字符、组合字素或正在输入法预编辑，并发生选区、点击定位或编辑
- **THEN** 光标与选区映射到原文的正确字素边界，隐藏场景只显示相应数量的遮罩符号

#### Scenario: 非法状态声明
- **WHEN** 同时声明 `value` 与 `defaultValue`
- **THEN** 系统在分配组件、编辑器和交互资源前拒绝挂载

### Requirement: 可见性与焦点
Password SHALL 默认隐藏，支持受控 `visible` 或 `defaultVisible`、`visibilityToggle` 和 `onVisibleChange`。启用切换时，指针或键盘激活 SHALL 请求或改变可见性；指针切换 MUST 保留编辑焦点、光标与有效组合输入，受控模式 SHALL 等待外部回写。禁用状态 SHALL 阻止切换，关闭切换时 SHALL 不挂载切换交互。

#### Scenario: 指针切换
- **WHEN** 已聚焦 Password 的可见性按钮被指针激活
- **THEN** 展示状态改变或发出受控请求，编辑焦点和光标保持，输入法组合输入不因指针按下被取消

#### Scenario: 受控回写与禁用
- **WHEN** 受控切换发出请求但未回写，或 Password 已禁用
- **THEN** 前者维持当前展示状态，后者不切换也不发出回调

### Requirement: 文本输入与剪贴板
Password SHALL 复用同窗文本编辑与输入法会话。隐藏状态 SHALL 使用平台密码输入类型，并阻止复制及剪切原文；粘贴、撤销、提交及可见状态的普通单行编辑 SHALL 遵守 Input 的现有合同。可见性变化 SHALL 保持当前编辑内容。

#### Scenario: 隐藏复制和粘贴
- **WHEN** 隐藏 Password 中选中文本并请求复制、剪切或粘贴
- **THEN** 复制和剪切不改变剪贴板或原文，粘贴按现有编辑规则写入原文

### Requirement: 主题与生命周期
Password SHALL 使用 Input 的主题尺寸、状态及外部布局规则，切换控件使用主题语义颜色。颜色变化 MUST 不重建稳定 scene topology 或重新挂载编辑器；销毁 SHALL 释放交互与场景资源而不影响同窗 Input。

#### Scenario: 颜色更新和销毁
- **WHEN** Password 主题颜色变化后被销毁
- **THEN** 颜色更新保持编辑器和稳定 scene 身份，销毁后不再接收输入且同窗编辑器可继续使用
