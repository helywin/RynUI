# Tasks

## 1. 锁定参考与公开合同

- [x] 1.1 从仓库锁定的 Ant Design 6.6.5 source manifest 与 change 012 `evidence/source-diff.json` 核实 Switch/Checkbox 的 Props、状态、尺寸、Token 与四份关键 doc/style 文件 SHA256，写入最小 source contract；以版本、commit、source SHA、状态矩阵和非法尺寸测试验证，不访问网络
- [x] 1.2 确定 `SwitchProps`、`CheckboxProps` 的 controlled/defaultChecked、disabled/loading/indeterminate、onChange、typed label slot 与 LayoutStyle 边界；Switch 仅接受 Middle/Small，Checkbox 无 size Prop；以 public-only C++20 编译及非法 Props/视觉入口编译测试验证
- [x] 1.3 在一个受支持平台记录实际 OS/compiler/preset，运行本阶段 contract、`git diff --check`；以英文 `test: lock selection control contracts` 提交本阶段，不主动 push

## 2. Switch

- [x] 2.1 使用 013 的 `WindowComponentServices` 和 `PressableBehavior` 实现 `ryn::Switch` 的 typed Props、controlled/uncontrolled checked、disabled/loading、onChange 与 reactive 连接；以 public API、冲突/模式、Signal 回写、pointer/Space/repeat/cancel/focus tests 验证
- [x] 2.2 以共享 scene/effect/animation 服务实现 Switch 轨道、滑块、loading 与 token 状态，仅覆盖 6.6.5 来源的 Middle/Small 尺寸、Default/Dark/Compact 和 1.0/1.25/1.5/2.0 模拟 scale；以 geometry/color、retained identity、最小 dirty/upload、idle benchmark 验证
- [x] 2.3 在一个受支持正式 preset 运行 Switch、Button/Input 回归与 `git diff --check`，记录结果；与 Checkbox 共用同一实现宿主，以英文 `feat: add reusable selection controls` 合并提交，不主动 push

## 3. Checkbox

- [x] 3.1 使用 013 的窗口服务和 `PressableBehavior` 实现 `ryn::Checkbox` 的 typed Props、checked/defaultChecked、indeterminate、disabled、onChange 与 typed label slot；以 public API、模式冲突、label 生命周期、pointer/Space/repeat/cancel 与 callback 自毁测试验证
- [x] 3.2 以共享 scene/effect/Text 服务实现 box、check、居中 `fontSizeLG / 2` indeterminate 方块、label 与 focus-visible 的独立 token 状态，覆盖 Default/Dark/Compact 和模拟 scale；以 CJK/Latin、布局/clip/HitTest、retained identity、最小 dirty/upload 与 idle tests 验证
- [x] 3.3 在一个受支持正式 preset 运行 Checkbox、Switch、Button/Input 回归与 `git diff --check`，记录结果；与 Switch 共用 `feat: add reusable selection controls` 提交，不主动 push

## 4. 平台通用集成

- [x] 4.1 增加 Button/Input/Switch/Checkbox 同窗 headless journey，覆盖 Tab 顺序、pointer/keyboard 语义、Theme、controlled/uncontrolled 回写、destroy/reuse、scene ownership 和 idle；以完整 journey 与 diagnostics 验证无 sibling remount/shape/upload
- [x] 4.2 在一个受支持正式 preset 运行全部相关 unit/headless/contract/benchmark、public dependency、lock/license、无网络 runtime 与 Python cache 检查，记录实际 OS/compiler/preset/result；不要求另一平台重复通用合同
- [x] 4.3 运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive`、`git diff --check`；以英文 `test: validate shared selection controls` 提交平台通用 evidence，不主动 push

## 5. Windows 专属验收

- [x] 5.1 使用 `windows-msvc` 正式 preset 完成 Windows 受影响的 Debug/Release build 与 CTest，核对 Ninja Multi-Config、MSVC x64、Win32 输入、D3D12/DXIL 和系统字体，保存独立结果
- [x] 5.2 在 Windows 真实窗口以系统 display scale 与 1.0/1.25/1.5/2.0 acceptance scale 操作 Switch Middle/Small 与 Checkbox 固定尺寸的 checked、居中方块 indeterminate、disabled、loading、pointer/Space/Tab，并与 Button/Input 混排；人工核对视觉、CJK/Latin、clip、focus 与正常退出，保存截图、driver、font、scale、exit code 和 diagnostics
- [x] 5.3 运行 Windows passed evidence contract、受影响平台测试、shader/lock/license/cache 检查、OpenSpec strict validate 与 `git diff --check`；以英文 `test: validate Windows selection controls` 提交 Windows evidence，不修改 Linux 清单且不主动 push

## 6. Linux 专属验收

- [ ] 6.1 使用 `linux-gcc` 和 `linux-clang` 正式 preset 完成 Linux 受影响构建与 CTest，核对 Ninja Multi-Config、原生 Wayland、Vulkan/SPIR-V 与 Fontconfig 系统字体，保存独立结果
- [ ] 6.2 在原生 Linux Wayland 真实窗口以至少两档实际 display scale 操作 Switch/Checkbox 状态、pointer/Space/Tab 并与 Button/Input 混排；人工核对视觉、CJK/Latin、clip、focus 与正常退出，保存截图、window system、driver、font、scale、exit code 和 diagnostics，不以 XWayland 替代
- [ ] 6.3 运行 Linux passed evidence contract、受影响平台测试、shader/lock/license/cache 检查、OpenSpec strict validate 与 `git diff --check`；以英文 `test: validate Linux selection controls` 提交 Linux evidence，不修改 Windows 清单且不主动 push

## 7. Change 收口

- [ ] 7.1 仅在准备 archive 时核对平台通用、Windows、Linux 各自 checkbox 与 evidence，运行最终 OpenSpec doctor/strict validate、受影响 CTest、`git diff --check` 和 clean worktree 检查；本项不得替代任何真实平台验收，也不得自动 archive 或 push
