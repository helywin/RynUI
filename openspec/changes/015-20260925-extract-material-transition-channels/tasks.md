# Tasks

## 1. Helper 合同与阶段提交

- [x] 1.1 以固定通道 typed target group 和共享有限过渡 retarget helper 定义内部 API；测试 color/scalar、注册异常回滚、同目标收敛、反向 retarget、dispose 后无 deadline 和稳态零分配
- [x] 1.2 在 `windows-msvc` Debug 正式 preset 跑 helper 与 AnimationRuntime/Material 相关定向 CTest、`git diff --check`；英文提交本阶段，不主动 push

## 2. Button/Input 迁移

- [x] 2.1 InputMaterialTransition 改用 helper，保持颜色/shadow opacity、same-spec skip、dirty 回调和销毁语义；运行 Input 动画、编辑、scene 与 allocation 回归
- [x] 2.2 Button 的 target 注册/释放及有限材质过渡改用 helper，spinner 循环、Button Token、focus/hover/press 保持组件专属；运行 Button pointer/keyboard、场景、spinner、reduced motion、同窗 Search/Selection 回归
- [x] 2.3 正式 Windows MSVC Debug build 与受影响 CTest、`git diff --check`；英文提交 consumer 迁移，不主动 push

## 3. 集成验证与证据

- [x] 3.1 在一个正式 preset 运行完整 CTest、public dependency、shader、lock/license、Python cache 与 idle benchmark，记录 OS/compiler/preset/结果；不在第二平台重复通用合同
- [x] 3.2 使用 Windows MSVC x64 Debug/Release 验证构建和受影响平台集成测试，复用已有 Button/Input/Search 实窗诊断，记录真实 Win32、D3D12/DXIL、字体和退出码；不将 014 原生 IME 待验改成通过
- [x] 3.3 运行 OpenSpec doctor/strict validate、`git diff --check`，核对纯内部重构和 013/014 证据边界；英文提交 evidence，不主动 push 或 archive

## 4. Linux 待验边界

- [ ] 4.1 用户安排 Linux 验证后，再在原生 Wayland GCC/Clang 正式 preset 运行受影响构建/平台集成测试并记录 Vulkan/SPIR-V、Fontconfig 和真实窗口结果；不以 WSLg/XWayland 代替
