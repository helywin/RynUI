# Tasks

## 1. 规划

- [x] 1.1 核对 Password suffix 的交互、布局、scene 与生命周期路径，完成 proposal/design/tasks 并设置 `skip_specs: true`；运行 OpenSpec doctor、strict validate、`git diff --check` 后英文提交规划阶段

## 2. 内部 helper 提取

- [x] 2.1 提取 `InputAffixAction` 的状态、挂载、按压、键盘、禁用和清理；Password 改为传入标签与激活回调，公开 API 及视觉几何不变
- [x] 2.2 在正式 Windows MSVC Debug preset 构建，运行 Password、Input、Pointer、Focus、Gallery frame 与 Search 定向测试；运行 `git diff --check` 并英文提交实现与平台通用证据

## 3. Windows 专属回归

- [ ] 3.1 用真实 Win32 Gallery 在 1.5 倍 Default/Dark 检查 Password 指针/键盘切换、焦点、禁用、D3D12/DXIL、系统字体与退出；记录诊断和截图
- [ ] 3.2 正式 Release preset 构建并运行受影响 CTest；OpenSpec doctor、strict validate 和 `git diff --check`，英文提交 Windows 独立证据

## 4. Linux 专属回归

- [ ] 4.1 后续在真实 Linux 用正式 GCC/Clang preset 构建并运行受影响 CTest
- [ ] 4.2 后续在原生 Wayland 窗口检查附属动作、输入会话、主题与退出，独立记录 Vulkan/SPIR-V/Fontconfig 证据并英文提交

## 5. 收口

- [ ] 5.1 准备 archive 时再核对各平台证据与任务；不自动 push 或 archive
