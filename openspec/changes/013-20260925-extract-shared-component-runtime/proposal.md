# Proposal

## Why

Button 与 Input 已复用交互、文字、场景和动画设施，但窗口级所有权与帧同步仍放在 `ButtonComponentHost`，`InputComponentHost` 因而依赖 Button 宿主。新增控件前先提取这些现成服务和指针按压机制，可减少重复宿主代码并保持既有行为。

## What Changes

- 提取内部窗口级组件服务，统一拥有 Text、interaction、hit-test、scene、focus、pointer、animation 和帧同步设施；Button/Input 改为服务的对等消费者。
- 提取内部 `PressableBehavior`，只负责 primary pointer 的按下、捕获、取消、释放和一次性 activation intent；Button 的键盘和视觉语义保持专属。
- 保留窗口内唯一 TextInput session，明确编辑 store、IME 和剪贴板的共享接入边界；不在本轮扩展单行编辑算法。
- 通过现有 Button/Input 回归及新增混合挂载、生命周期、重入、场景失效和闲置帧合同验证行为等价。
- 本变更只调整内部实现，不新增公开组件或改变公开 API、Token、布局和用户可观察行为；因此声明 `skip_specs: true`。

## Capabilities

### New Capabilities

无；本变更是内部重构，不新增行为需求。

### Modified Capabilities

无；Button/Input 的既有行为合同不变。

## Impact

- 主要涉及 `src/component/`、相关窗口适配器及内部测试夹具，保留 `ryn` 公开头和 SDL3 隔离边界。
- 011 的 Switch/Checkbox 实现随后使用本变更的服务与按压行为；013 不勾选 011 的组件任务，也不以本轮结果代替 011 的平台验收。
- 不增加依赖、通用公开基类、视觉 `Modifier`、TextArea、Password、Search 或 InputNumber。
