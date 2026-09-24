# Tasks

## 1. 窗口服务翻译单元

- [x] 1.1 将全部 `WindowComponentServices::` 定义迁到独立 `window_component_services.cpp` 并加入 `rynui_components`；用源码搜索确认 Button 源文件不再定义窗口服务方法、无重复符号
- [x] 1.2 在正式 `windows-msvc-debug` Ninja Multi-Config/MSVC x64 构建，并运行窗口服务生命周期、Button/Input/Search/Selection/Gallery、scene 与 idle 定向 CTest、`git diff --check`；以英文提交源码阶段，不主动 push

## 2. 通用与 Windows 集成证据

- [x] 2.1 在一个正式 preset 运行全量 CTest（含公开头、依赖锁/license、shader、Python cache 与 benchmark），记录 OS/compiler/preset/结果；不要求 Linux 重复通用合同
- [x] 2.2 在 Windows 正式 Release preset 构建并运行受影响集成 CTest 与真实 Win32/D3D12/DXIL/系统字体 Gallery 回归，记录实际缩放、诊断与退出码；不代替 014 原生 IME 待验
- [x] 2.3 运行 OpenSpec doctor/strict validate、`git diff --check`，提交独立证据；不主动 push 或 archive

## 3. Linux 待验

- [ ] 3.1 用户安排后在原生 Wayland GCC/Clang 正式 preset 运行受影响构建/平台集成测试和真实 Vulkan/SPIR-V、Fontconfig、窗口验证；不以 WSLg/XWayland 代替
