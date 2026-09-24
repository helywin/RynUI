# Tasks

## 1. 规划

- [x] 1.1 完成 proposal、spec、design 与 tasks；核对 Input/Password 现有行为，运行 OpenSpec doctor、strict validate、`git diff --check` 并英文提交规划阶段

## 2. 平台通用实现与测试

- [x] 2.1 为 `InputProps` 添加 reactive `allowClear`，扩展 `InputAffixAction` 的响应式折叠并组合自定义 suffix；用正式 Windows MSVC Debug preset 构建验证
- [x] 2.2 清空复用 editor/session/`onChange`，补充受控、组合输入、禁用/只读、焦点、键盘、布局和销毁回归；运行定向 CTest 与 `git diff --check`，英文提交实现和通用证据

## 3. Windows 专属验收

- [x] 3.1 在真实 Win32 Gallery 展示空值、内容、禁用与主题状态，运行 Default/Dark 1.5 倍自动指针/键盘流程，保存截图和 D3D12/DXIL/系统字体/退出诊断
- [x] 3.2 用正式 Release preset 构建并运行受影响 CTest；运行 OpenSpec doctor、strict validate、`git diff --check`，英文提交 Windows 独立证据

## 4. Linux 专属验收

- [ ] 4.1 后续在真实 Linux 的正式 GCC/Clang preset 构建，验证含平台分支的受影响测试
- [ ] 4.2 后续在原生 Wayland 窗口验证清空交互、字体、Vulkan/SPIR-V 与退出，独立记录并英文提交

## 5. 收口

- [ ] 5.1 准备 archive 时核对各平台证据；不自动 push 或 archive
