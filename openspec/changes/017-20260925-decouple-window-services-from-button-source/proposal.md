# Proposal

## Why

`WindowComponentServices` 已拥有窗口级组件状态和同步阶段，但约 170 行实现仍编译在 `button_component.cpp`。这使独立窗口服务在源码所有权和增量构建依赖上继续挂靠 Button，不利于后续对等组件接入。

## What Changes

- 将 `WindowComponentServices` 的构造、参与者挂载、文本编辑绑定、销毁、动画、布局与 scene 同步实现迁入独立 `window_component_services.cpp`，由 `rynui_components` 显式编译。
- `button_component.cpp` 只保留 Button 行为与其兼容宿主转发，不改动 `WindowComponentServices` 的对象所有权、同步顺序或异常语义。
- 用现有多组件同窗、Button/Input/Search/Selection、scene、idle 和真实 Windows Gallery 回归验证行为等价。

## Capabilities

### New Capabilities

无；这是纯内部源码边界调整。

### Modified Capabilities

无；不改变公开 API 或既有行为要求，故设置 `skip_specs: true`。

## Impact

- 涉及 `src/component/button_component.cpp`、新增 `src/component/window_component_services.cpp` 和 `src/CMakeLists.txt`；`include/ryn/` 不变。
- 风险集中于迁移时遗漏实现或 CMake 源文件、翻译单元新增 include 依赖，以及窗口服务与 Button 的析构顺序；以正式 MSVC 构建和生命周期/同窗测试检验。
- 平台通用逻辑只在一个正式 preset 运行完整 CTest；Windows 实窗回归单列，Linux 原生 Wayland/GPU 验收依用户安排暂缓，不将其标为通过。
