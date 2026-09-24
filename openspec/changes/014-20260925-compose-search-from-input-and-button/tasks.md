# Tasks

## 1. 来源与公开合同

- [x] 1.1 锁定本地 Ant Design 6.6.5 `Search.tsx`、`style/search.ts`、英文文档与两个 demo 的 commit/SHA256，列出首批支持和未支持的 Props、Enter/按钮/composition/loading 语义；以离线 source contract 测试验证
- [x] 1.2 定义 `ryn::SearchProps`、`SearchSource`、可选 typed `SearchButtonContent` 与 `LayoutStyle` 边界；以 public-only C++20 编译和非法 value/defaultValue、非法尺寸测试验证
- [x] 1.3 在正式 Windows MSVC Debug preset 验证阶段 1、运行 `git diff --check`；公开函数与行为测试需要同一实现目标，合并到阶段 2 的英文提交，不主动 push

## 2. Search 组合与提交

- [x] 2.1 以现有 Input、Button、Flex 实现 Search 的内部 value bridge，验证受控 Signal 回写、无回写、程序化更新、非受控连续编辑及子树销毁后无悬挂订阅
- [x] 2.2 实现 Enter/按钮单次 onSearch、`SearchSource::Input`、loading/disabled/readOnly/composition 门禁及 typed 按钮内容；以 repeat、pointer cancel、多 Search、回调自毁和同窗 Input/Selection 回归验证
- [x] 2.3 在正式 Windows MSVC Debug preset 运行 Search、Button/Input、Switch/Checkbox 定向测试及 `git diff --check`；以英文 `feat: compose Search from existing controls` 提交，不主动 push

## 3. 布局、Theme 与 Gallery

- [x] 3.1 使 Search 的 Input flex grow/shrink、Button 固定并在三档尺寸、窄宽、Default/Dark/Compact 及 1.0/1.25/1.5/2.0 模拟 scale 下验证 clip、hit-test、focus、Token、retained identity 与最小 dirty/upload
- [x] 3.2 在 Gallery 加入真实 Search 样例及准确 `partial` 能力说明，更新目录生成与文档合同；以同窗 headless journey 和 idle benchmark 验证无第二编辑会话、无 sibling remount 或持续提交
- [x] 3.3 在正式 Windows MSVC Debug preset 运行相关测试、OpenSpec doctor/strict validate 与 `git diff --check`；以英文 `feat: integrate Search composition in Gallery` 提交，不主动 push

## 4. 平台通用验证

- [x] 4.1 在一个受支持正式 preset 运行完整 CTest、public dependency、来源、lock/license、shader、Python cache 与空闲 benchmark，记录 OS/compiler/preset/结果；不要求另一平台重复平台通用合同
- [x] 4.2 验证平台通用 passed evidence、OpenSpec doctor/strict validate 和 `git diff --check`；以英文 `test: validate Search composition contracts` 提交独立 evidence，不主动 push

## 5. Windows 专属验收

- [ ] 5.1 使用 `windows-msvc` Ninja Multi-Config/MSVC x64 完成受影响 Debug/Release build 与 CTest；记录 Win32、D3D12/DXIL、系统字体、实际 display scale 与运行结果
- [ ] 5.2 在真实 Windows 窗口操作 Search 的 Enter、Button、loading/disabled、三档尺寸、CJK/Latin、Tab/focus 与 IME，至少覆盖 1.0/1.25/1.5/2.0 模拟 scale 并人工核对布局/clip；保存截图、driver/font/scale、diagnostics 与 exit code
- [ ] 5.3 运行 Windows passed evidence contract、shader/lock/license/cache、OpenSpec strict validate 与 `git diff --check`；以英文 `test: validate Windows Search composition` 提交独立 evidence，不主动 push

## 6. Linux 专属验收

- [ ] 6.1 使用 `linux-gcc` 与 `linux-clang` 正式 preset 完成受影响构建/CTest，记录 Ninja Multi-Config、原生 Wayland、Vulkan/SPIR-V 与 Fontconfig 系统字体
- [ ] 6.2 在原生 Linux Wayland 真实窗口与至少两档实际 display scale 操作 Search Enter/按钮、CJK/Latin/IME、loading/disabled、Tab/focus；人工核对布局/clip 并保存截图、driver/font/scale、diagnostics 和 exit code，不以 XWayland/WSLg 替代
- [ ] 6.3 运行 Linux passed evidence contract、平台测试、shader/lock/license/cache、OpenSpec strict validate 与 `git diff --check`；以英文 `test: validate Linux Search composition` 提交独立 evidence，不修改 Windows 清单且不主动 push

## 7. Change 收口

- [ ] 7.1 仅在准备 archive 时核对平台通用、Windows、Linux 各自 checkbox/evidence，运行最终 doctor/strict validate、受影响 CTest 与 clean worktree 检查；不自动 archive 或 push
