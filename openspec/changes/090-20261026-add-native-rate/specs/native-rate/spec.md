# Spec Delta

## Purpose

提供桌面原生 retained 评分组件，让应用展示并编辑整数或半级评分，在受控数据、隐藏布局、输入边界与主题更新期间维持清晰的数据权威和稳定资源身份，使用现有 logical scene 完成呈现。

## ADDED Requirements

### Requirement: 评分值与事件

Rate SHALL 支持 count、value/defaultValue、allowHalf/allowClear、disabled/readOnly、keyboard、size、visible、typed character/icon 和逐项 Tooltip。count 为 0..1000 的整数；有限值裁剪至 0..count 并按半级或整级四舍五入，非法值拒绝且保留上次合法状态。受控候选仅通知 onChange，不修改权威值；count/allowHalf 变化重新规范化权威输入而不伪造用户事件。

#### Scenario: 半选和受控拒绝

- **WHEN** 指针进入项左半并点击，应用不回写受控 value
- **THEN** hover 预览为半级并通知 onHoverChange，onChange 收到候选，提交后显示仍以 value 为准

#### Scenario: 缩减与非法输入恢复

- **WHEN** count 缩减、恢复，或 value 暂时变成 NaN 后恢复有限值
- **THEN** 每次合法变更规范化显示，非法更新抛出错误且不发布部分状态，恢复后继续交互

### Requirement: 原生交互与生命周期

Rate SHALL 使用真实鼠标 hover/click、原生 capture、单一 Tab 入口与 Left/Right、Home/End 键盘编辑；Enter/Space 在当前位置提交并可清空。disabled/readOnly 禁止自身编辑，readOnly 不建立后代输入封锁。隐藏、窗口失活、Modal/Spin 输入边界及卸载 SHALL 取消 hover/capture/focus 并清理 owned blocks；同步回调卸载不得访问失效代际。

#### Scenario: 隐藏和输入阻断

- **WHEN** 预览或按压期间组件隐藏或进入外部输入封锁
- **THEN** 不再提交，hover 回到空状态，capture/focus 取消，隐藏不保留 flow/gap，恢复后资源身份仍稳定

### Requirement: 逻辑场景与主题

Rate SHALL 用现有 logical glyph/quad 与内容裁剪呈现真实半字符，有限宽、短高和零尺寸不得泄漏绘制或 hit，并能恢复。Rate Theme SHALL 覆盖 config/equality/inheritance/identity/hash/serialize/dirty，发布前验证独立 seed、继承和祖先更新后的最终字体至少 .5 logical dp，拒绝非有限或不可渲染的尺寸。旧 typed snapshot 字段与 TokenIdentity 编号 MUST 保持兼容。

#### Scenario: 半字符与约束恢复

- **WHEN** value 为 2.5，布局从自然尺寸变为有限宽/短高/零尺寸再恢复
- **THEN** 第三项前景实际 glyph/quad 被裁剪为左半，所有实例与命中服从有效内容 clip，恢复后继续显示和交互

#### Scenario: 非法祖先主题更新

- **WHEN** 祖先更新导致后代 Rate 最终尺寸非法
- **THEN** 整个主题树拒绝发布，旧快照和订阅保留，后续合法更新可恢复且不重挂载组件
