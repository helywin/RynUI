# Proposal

## Why

窗口服务已拥有独立声明、实现和资源，但 Button 宿主仍通过 `friend class ButtonComponentHost` 借用三个私有字段的引用。窗口服务现有 accessor 与 dirty 标记方法已足够表达这些操作，保留特权访问会使后续组件误以为也需要直接读取内部状态。

## What Changes

- Button 的动画时间和 motion preference 读取改用窗口服务的只读 accessor，scene dirty 改用显式 `mark_scene_structure_dirty()`。
- 删除 Button 宿主中的三个私有字段引用及窗口服务的 Button `friend` 声明；其他服务引用与 Button 便利入口保持现状。
- 运行 Button 动画、焦点、scene dirty、Input/Search/Selection 混合窗口与空闲回归，确认行为不变。

## Capabilities

### New Capabilities

无；纯内部访问边界整理。

### Modified Capabilities

无；公开行为和 API 不变，设置 `skip_specs: true`。

## Impact

- 涉及 `src/component/button_component.hpp/.cpp` 与 `window_component_services.hpp`，不改变公开头、对象资源所有权或渲染后端。
- 风险是漏改动画时间读取或 scene dirty 通知；以正式 Windows MSVC 构建、动画与场景回归验证。
- 本轮没有新增 OS/GPU 行为；Linux 验证依用户安排暂缓，原生 IME 待验保持原状态。
