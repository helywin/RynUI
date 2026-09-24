# Proposal

## Why

Password 的可见性操作已证明，输入框内的附属动作需要复用按压、焦点策略、键盘激活、禁用和资源清理。当前这些机制以约百行代码留在 Password 私有实现中，下一类带操作区的输入组件会重复同一套交互装配。

## What Changes

- 从 Password 中提取内部 `InputAffixAction`，统一附属动作的可点击区域、`PressableBehavior`、指针焦点策略、键盘激活、禁用与清理。
- Password 只提供可见性状态、标签和回调；保持公开 Props、编辑、IME、剪贴板、主题外观与场景行为不变。
- 用现有 Password、Input、交互与 Gallery 回归验证提取前后的行为和资源身份。

## Capabilities

### New Capabilities

无；纯内部复用提取。

### Modified Capabilities

无；公开行为不变，设置 `skip_specs: true`。

## Impact

涉及 `src/component/input_component.cpp`、新内部 helper 和 `src/CMakeLists.txt`，不新增公开 API 或第三方依赖。风险是交互父子关系、失焦和回调自毁时的生命周期退化；Windows MSVC 定向与真实窗口回归验证。Linux 原生验收依用户安排暂缓。
