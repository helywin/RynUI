# Tasks

## 1. 锁定参考与公开合同

- [ ] 1.1 从仓库锁定的 Ant Design 6.5.0 source manifest 确认 Switch/Checkbox 的 Props、状态、尺寸、Token 与 source path，写入最小 source contract；以版本、source SHA、状态矩阵和非法尺寸测试验证，不访问网络
- [ ] 1.2 确定 `SwitchProps`、`CheckboxProps` 的 controlled/defaultChecked、disabled/loading/indeterminate、onChange、typed label slot 与 LayoutStyle 边界；以 public-only C++20 编译及非法 Props/视觉入口编译测试验证
- [ ] 1.3 在一个受支持平台记录实际 OS/compiler/preset，运行本阶段 contract、`git diff --check`；以英文 `test: lock selection control contracts` 提交本阶段，不主动 push

## 2. 共享窗口级组件服务

- [ ] 2.1 将 Button 宿主持有的公共 Text、interaction、HitTest、scene、focus、pointer、animation 与 frame synchronization 渐进迁移到 internal window-owned 服务，Button/Input 使用受限引用；以混合挂载、声明顺序、稳定 identity 和公共头隔离测试验证
- [ ] 2.2 保持一个窗口唯一 text-input session 与 Input 编辑所有权，完成 Button/Input 适配；以现有 Button pointer/focus/motion、Input editing/IME/clipboard、Theme/scene 和生命周期测试验证行为不变
- [ ] 2.3 在一个受支持正式 preset 运行服务阶段受影响的 unit/headless/benchmark、dependency leak 与 `git diff --check`，记录结果；以英文 `refactor: share window component services` 提交，不主动 push

## 3. 可组合按压行为

- [ ] 3.1 提取 internal pointer Pressable 行为，覆盖 primary pointer、capture、bounds 内 release、cancel、window blur、disabled、销毁、generation 复用和回调自毁，保持 keyboard policy 由控件决定；以独立行为测试验证一次性 activation intent 与释放顺序
- [ ] 3.2 让 Button 使用该行为而保持原 Enter/Space、loading 与视觉合同；以 Button pointer/keyboard/state、steady-state allocation、scene dirty range 和 `git diff --check` 验证；以英文 `refactor: share press gesture behavior` 提交，不主动 push

## 4. Switch

- [ ] 4.1 实现 `ryn::Switch` 的 typed Props、controlled/uncontrolled checked、disabled/loading、onChange 与 reactive 连接；以 public API、冲突/模式、Signal 回写、pointer/Space/repeat/cancel/focus tests 验证
- [ ] 4.2 以共享 scene/effect/animation 服务实现 Switch 轨道、滑块、loading 与 token 状态，覆盖允许尺寸、Default/Dark/Compact 和 1.0/1.25/1.5/2.0 模拟 scale；以 geometry/color、retained identity、最小 dirty/upload、idle benchmark 验证
- [ ] 4.3 在一个受支持正式 preset 运行 Switch、Button/Input 回归与 `git diff --check`，记录结果；以英文 `feat: add reusable switch control` 提交，不主动 push

## 5. Checkbox

- [ ] 5.1 实现 `ryn::Checkbox` 的 typed Props、checked/defaultChecked、indeterminate、disabled、onChange 与 typed label slot；以 public API、模式冲突、label 生命周期、pointer/Space/repeat/cancel 与 callback 自毁测试验证
- [ ] 5.2 以共享 scene/effect/Text 服务实现 box、check、indeterminate、label 与 focus-visible 的独立 token 状态，覆盖 Default/Dark/Compact 和模拟 scale；以 CJK/Latin、布局/clip/HitTest、retained identity、最小 dirty/upload 与 idle tests 验证
- [ ] 5.3 在一个受支持正式 preset 运行 Checkbox、Switch、Button/Input 回归与 `git diff --check`，记录结果；以英文 `feat: add reusable checkbox control` 提交，不主动 push

## 6. 平台通用集成

- [ ] 6.1 增加 Button/Input/Switch/Checkbox 同窗 headless journey，覆盖 Tab 顺序、pointer/keyboard 语义、Theme、controlled/uncontrolled 回写、destroy/reuse、scene ownership 和 idle；以完整 journey 与 diagnostics 验证无 sibling remount/shape/upload
- [ ] 6.2 在一个受支持正式 preset 运行全部相关 unit/headless/contract/benchmark、public dependency、lock/license、无网络 runtime 与 Python cache 检查，记录实际 OS/compiler/preset/result；不要求另一平台重复通用合同
- [ ] 6.3 运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive`、`git diff --check`；以英文 `test: validate shared selection controls` 提交平台通用 evidence，不主动 push

## 7. Windows 专属验收

- [ ] 7.1 使用 `windows-msvc` 正式 preset 完成 Windows 受影响的 Debug/Release build 与 CTest，核对 Ninja Multi-Config、MSVC x64、Win32 输入、D3D12/DXIL 和系统字体，保存独立结果
- [ ] 7.2 在 Windows 真实窗口以系统 display scale 与 1.0/1.25/1.5/2.0 acceptance scale 操作 Switch/Checkbox checked、indeterminate、disabled、loading、pointer/Space/Tab，并与 Button/Input 混排；人工核对视觉、CJK/Latin、clip、focus 与正常退出，保存截图、driver、font、scale、exit code 和 diagnostics
- [ ] 7.3 运行 Windows passed evidence contract、受影响平台测试、shader/lock/license/cache 检查、OpenSpec strict validate 与 `git diff --check`；以英文 `test: validate Windows selection controls` 提交 Windows evidence，不修改 Linux 清单且不主动 push

## 8. Linux 专属验收

- [ ] 8.1 使用 `linux-gcc` 和 `linux-clang` 正式 preset 完成 Linux 受影响构建与 CTest，核对 Ninja Multi-Config、原生 Wayland、Vulkan/SPIR-V 与 Fontconfig 系统字体，保存独立结果
- [ ] 8.2 在原生 Linux Wayland 真实窗口以至少两档实际 display scale 操作 Switch/Checkbox 状态、pointer/Space/Tab 并与 Button/Input 混排；人工核对视觉、CJK/Latin、clip、focus 与正常退出，保存截图、window system、driver、font、scale、exit code 和 diagnostics，不以 XWayland 替代
- [ ] 8.3 运行 Linux passed evidence contract、受影响平台测试、shader/lock/license/cache 检查、OpenSpec strict validate 与 `git diff --check`；以英文 `test: validate Linux selection controls` 提交 Linux evidence，不修改 Windows 清单且不主动 push

## 9. Change 收口

- [ ] 9.1 仅在准备 archive 时核对平台通用、Windows、Linux 各自 checkbox 与 evidence，运行最终 OpenSpec doctor/strict validate、受影响 CTest、`git diff --check` 和 clean worktree 检查；本项不得替代任何真实平台验收，也不得自动 archive 或 push
