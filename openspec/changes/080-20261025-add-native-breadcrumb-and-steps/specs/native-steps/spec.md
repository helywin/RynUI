# Spec Delta

## Purpose

为原生业务录入和设置流程展示有序阶段、当前进度及错误状态，让应用通过受控或默认状态推进流程。步骤内容保持稳定身份，并提供真实的指针和键盘导航、横纵布局与一致主题，支持组合现有业务组件。

## ADDED Requirements

### Requirement: 步骤状态与排列

Steps SHALL 提供固定稳定 key 的步骤项、标题/副标题/说明/图标 typed slots，支持默认/Small 两尺寸及横纵排列。未显式覆盖的状态 SHALL 根据 current 推导 Wait/Process/Finish，当前状态可为 Error，单项状态覆盖优先。

#### Scenario: 推进与错误
- **WHEN** 应用推进 current、将当前项标记 Error，或显式覆盖某项状态
- **THEN** 实际编号/状态图标、文字和连接线正确更新，显式覆盖优先，普通更新保持 item/slot 身份

### Requirement: 受控切换和键盘

Steps SHALL 支持受控/default current、可选 onChange 以及项禁用。启用交互时指针或 Enter/Space 激活有效项发出准确请求；方向键/Home/End 移动可见启用项焦点，不提交新 current。受控拒绝不得伪造当前项，隐藏/禁用项不可激活。

#### Scenario: 拒绝与重新启用
- **WHEN** 应用拒绝切换请求，再禁用并重新启用目标项
- **THEN** 实际 current 保持父级值，后续显式激活可重新发送请求，重新启用后真实 pointer 命中恢复，重复键与 keyup 不重复激活

### Requirement: 有限几何和生命周期

Steps SHALL 在窄/短/零约束、富内容及横纵切换时裁剪真实 glyph/表面/命中；隐藏项及全空组件移出普通布局。Modal/Spin/隐藏边界及回调销毁 SHALL 清理失效交互，恢复保留身份，不持续请求空闲帧。

#### Scenario: 裁剪与关闭
- **WHEN** 富说明超出分配区域，或包含步骤的边界失活、组件在切换回调内销毁
- **THEN** 不画入/命中相邻步骤或页面内容，不访问失效状态，边界恢复后仅有效内容可交互

### Requirement: 独立 Theme

Steps SHALL 支持完整的 Theme 继承、组件算法、typed override 与 dirty 分离；最终派生值发布前校验，关闭算法的局部 seed 不影响继承，既有组件 token 值不因新增 Steps 改变。

#### Scenario: 主题局部性
- **WHEN** 更新 Steps 颜色/度量，或尝试发布非法派生值
- **THEN** 仅相关阶段失效，保留内容身份，非法主题原子拒绝，其他组件既有 token 不变
