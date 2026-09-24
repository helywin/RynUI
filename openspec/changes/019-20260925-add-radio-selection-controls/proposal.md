# Proposal

## Why

Switch 和 Checkbox 已验证窗口服务、按压与 retained surface 的复用，但单选控件还需要不同的激活规则：选中后再次激活不能取消，同组值必须互斥。Radio 可检验这些通用机制是否允许新组件只增加自身语义、Token 映射和视觉几何。

## What Changes

- 增加 `ryn::Radio` 的 typed Props 和 typed label slot，支持 controlled/uncontrolled `checked`、`disabled`、`onChange` 和外部 `LayoutStyle`。
- 增加以选项数据声明的 `ryn::RadioGroup`，支持受控或默认值、组级禁用、横向/纵向排列和单一变更回调。
- 在现有 Selection 宿主内复用窗口资源、`PressableBehavior`、焦点与 retained surface，增加 Radio 的圆环、圆点、主题颜色和单选激活策略。
- 加入平台通用行为、几何、局部失效与公开 API 回归，并在 Windows 正式 preset 验证真实窗口；Linux 专属验收依用户安排暂缓。

## Capabilities

### New Capabilities

- `radio-selection-controls`: Radio 与数据驱动 RadioGroup 的状态、输入、主题和生命周期合同。

### Modified Capabilities

无。

## Impact

新增 `include/ryn/radio.hpp`，扩展 `src/component/selection_component.*`、公开聚合头、测试与 Gallery。复用既有窗口服务和渲染管线，不增加第三方依赖。初次范围不包括 `Radio.Button`、自定义语义样式和动态选项集合；风险集中于受控回写、组内互斥、callback 自毁及颜色更新的最小失效范围。
