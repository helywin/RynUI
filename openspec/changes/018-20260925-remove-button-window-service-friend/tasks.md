# Tasks

## 1. Button 访问边界

- [ ] 1.1 用 `WindowComponentServices` 现有 accessor/dirty 方法替换 Button 的三个私有引用并删除 `friend`；用源码搜索确认无旧字段、无私有字段直接访问
- [ ] 1.2 在正式 `windows-msvc-debug` Ninja Multi-Config/MSVC x64 构建，运行 Button 动画/scene、Input/Search/Selection 混合窗口、Gallery frame 与 idle 定向 CTest、`git diff --check`；英文提交源码阶段，不主动 push

## 2. 集成验证与证据

- [ ] 2.1 在正式 `windows-msvc-release` 构建并运行受影响 CTest 与真实 Win32/D3D12/DXIL/系统字体 Gallery 动画回归；记录编译器、preset、诊断、缩放和退出码
- [ ] 2.2 运行 OpenSpec doctor/strict validate、`git diff --check`，核对纯内部边界和 014 原生 IME 待验，英文提交独立证据；不主动 push 或 archive
