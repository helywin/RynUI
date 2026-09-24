# Tasks

## 1. 通用 surface core

- [x] 1.1 迁出通用 surface ID/effects/diagnostics/service 与 span create/update/destroy，实现和场景/effect/store/generation 行为等价；Button visuals 留在专属头
- [x] 1.2 用正式 Windows MSVC Debug preset 跑 surface/scene/effect/stale ID/dirty/upload 定向 CTest、`git diff --check`；英文提交 core 阶段，不主动 push

## 2. Consumer 迁移

- [ ] 2.1 WindowComponentServices 只持有通用 service；Button 的 typed array 走通用 span API，Button spinner/focus 外观仍专属
- [ ] 2.2 Input、Selection、Gallery reference surface 改用通用类型与服务 accessor；验证同窗唯一 store、mount/destroy、scene 顺序及颜色变化最小失效
- [ ] 2.3 正式 Windows MSVC Debug build 与受影响 Button/Input/Search/Selection/Gallery CTest、idle benchmark、`git diff --check`；英文提交 consumer 阶段，不主动 push

## 3. 集成证据

- [ ] 3.1 在一个正式 preset 运行完整 CTest、公开头/依赖锁/license/shader/Python cache，记录平台、编译器、preset 与结果；不在另一平台重复通用合同
- [ ] 3.2 Windows MSVC x64 Debug/Release 构建及受影响平台集成测试，复用实际 Win32/D3D12/DXIL/系统字体的 Gallery/Input/Search/Selection 实窗回归，记录退出码，不代替 014 原生 IME 待验
- [ ] 3.3 运行 OpenSpec doctor/strict validate、`git diff --check`，提交独立 evidence；不主动 push 或 archive

## 4. Linux 待验

- [ ] 4.1 用户安排后在原生 Wayland GCC/Clang 正式 preset 运行受影响构建/平台集成测试与真实 Vulkan/SPIR-V、Fontconfig、窗口验证；不以 WSLg/XWayland 代替
