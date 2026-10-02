# Spec Delta

## Purpose

为 RynUI 原生单行输入家族定义统一的外观、统计、焦点和操作行为，在保留编辑状态和 typed API 的同时覆盖 Ant Design 6 可在原生桌面上成立的交互合同，并为后续多行输入提供共用基础。

## ADDED Requirements

### Requirement: Reactive native Input variants

Input、Password、Search SHALL 支持 Outlined、Borderless、Filled、Underlined 变体，默认 Outlined。视觉 SHALL 来自 Theme/Component Token；状态、disabled、hover、focus-visible、Compact 接缝及尺寸切换 SHALL 保持一致，变体切换 MUST 保留 editor、选择、组合输入和组件身份。

#### Scenario: 保留编辑会话切换外观
- **WHEN** 已选择文本或存在 IME preedit 的输入框切换变体和 Theme
- **THEN** 输入值、选择和组件身份保持，Borderless 没有容器边框且键盘焦点可见，Underlined 只绘制底边，Filled 使用对应背景及状态色

### Requirement: Native count and soft-limit editing

输入家族 SHALL 支持 reactive 显示统计、软 max、Unicode scalar/grapheme 计数及自定义策略/显示 formatter。默认统计不泄漏 Password 明文到视觉输出；软 max 超出 SHALL 提示 error 状态且不自动截断，显式 status 的 Error/Warning 优先。配置 exceedFormatter 时 SHALL 在用户编辑提交前完成裁剪并纳入一个 undo 事务；IME preedit、受控回写、undo/redo MUST 不被重复裁剪。

#### Scenario: 超限粘贴与组合输入
- **WHEN** 用户粘贴或提交 IME 文本超过软上限且配置裁剪函数
- **THEN** 仅发布最终候选值，onChange 一次，undo 一次恢复前值；preedit 完整显示，自定义策略以完整 Unicode 文本为参数

#### Scenario: formatter 失败或卸载
- **WHEN** formatter 抛错、重入修改当前值或卸载输入框
- **THEN** 未完成候选不发布，不使用已失效对象，统计更新不触发无关组件重新执行

### Requirement: Public Input focus and native properties

InputRef SHALL 提供 bound、focus、blur 和基于 UTF-8 grapheme 边界的 select；focus SHALL 支持 Keep、Start、End、All 光标选项。autoFocus、onFocus/onBlur SHALL 遵循窗口焦点与 disabled/branch 生命周期。typed 输入用途、大小写和 autocorrect SHALL 通过平台输入端口传递；Password SHALL 强制密码用途及关闭 autocorrect。属性变更 SHALL 安全刷新会话并拒绝旧事件。

#### Scenario: 安全的公开引用
- **WHEN** ref 在挂载前、挂载后、卸载后或被第二个存活组件重复绑定
- **THEN** 无绑定方法返回 false，存活重复绑定报错；focus/select 保留合法边界，卸载清除绑定且不能作用到复用的新组件

### Requirement: Configurable clear and Password actions

输入家族 SHALL 支持独立 clearDisabled、自定义 IconSource 和 onClear，清空只在有值且编辑允许时可操作， SHALL 发布空值并调用 onClear 一次。Password SHALL 转发所有共用属性、prefix/suffix，支持 reactive visibilityToggle、可配置键盘停靠、自定义 visible→IconSource 和 Click/Hover 操作；受控显隐 SHALL 仅提出候选，组合输入期间显隐的系统会话刷新 SHALL 延后至提交或取消。

#### Scenario: 密码组合输入和操作配置
- **WHEN** 密码框正在组合输入时切换显隐，随后隐藏操作或禁用 clear
- **THEN** 不丢失 preedit；隐藏或禁用动作取消捕获/焦点，清空不会发生，显隐候选只通知一次，输入会话属性在合适时机刷新

### Requirement: Complete native Search forwarding

Search SHALL 转发共用输入属性和 prefix/suffix，支持自定义搜索 IconSource、typed 按钮内容、loading 和变体连接外观。onSearch SHALL 对按钮/Enter 使用 Input 来源，对成功清空使用 Clear 来源及空候选值；disabled/readOnly/loading SHALL 阻止不允许的搜索。回调中的同步回写或卸载 MUST 不读失效的编辑器。

#### Scenario: 清空来源与受控回写
- **WHEN** Search 的清空操作成功且父级同步接受或拒绝候选值
- **THEN** onChange 与 onClear 各一次，onSearch 收到空候选和 Clear 来源，编辑器不重建；按钮/Enter 读取最新接受的值

### Requirement: Verifiable native completion boundary

单行家族的 common 合同 SHALL 在一个支持平台实际测试，窗口、GPU、DPI、系统字体和输入属性 SHALL 分 Windows/Linux 独立记录。Gallery SHALL 展示上述能力且保持原样本稳定 ID；整个 Input SHALL 在 TextArea、OTP 和双向视觉编辑完成前保持 partial。

#### Scenario: 本机证据不替代另一平台
- **WHEN** Windows 完成 headless 与原生窗口验收而 Linux 未运行
- **THEN** Windows 项可完成，Linux 项独立保持未完成，文档明确本 change 及全家族剩余范围
